#include "sound.h"
#include "settings.h"
#include "dos_compat.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *pcm;
    int      len;
} SfxSample;

static SfxSample g_sfx[SFX_COUNT];
static int       sb_active   = 0;
static uint16_t  sb_base     = 0x220;
static int       sb_dma      = 1;
static int       dma_sel     = -1;
static uint32_t  dma_phys    = 0;
static int       step_toggle = 0;

/* ISA 8237 8-bit DMA channel I/O ports (channels 0..3) */
static const uint8_t dma_addr_port[4]  = { 0x00, 0x02, 0x04, 0x06 };
static const uint8_t dma_count_port[4] = { 0x01, 0x03, 0x05, 0x07 };
static const uint8_t dma_page_port[4]  = { 0x87, 0x83, 0x81, 0x82 };

static void sb_dsp_write(uint8_t val) {
    int timeout = 12000;
    while ((inportb((uint16_t)(sb_base + 0x0C)) & 0x80) != 0 && --timeout > 0) {
    }
    outportb((uint16_t)(sb_base + 0x0C), val);
}

static int sb_dsp_reset(uint16_t base) {
    int i;
    outportb((uint16_t)(base + 0x06), 1);
    delay(3);
    outportb((uint16_t)(base + 0x06), 0);

    for (i = 0; i < 2000; i++) {
        if ((inportb((uint16_t)(base + 0x0E)) & 0x80) != 0) {
            if (inportb((uint16_t)(base + 0x0A)) == 0xAA) {
                return 1;
            }
        }
    }
    return 0;
}

static void parse_blaster_env(void) {
    const char *env = getenv("BLASTER");
    const char *p;
    if (!env) return;

    for (p = env; *p; p++) {
        char c = (char)toupper((unsigned char)*p);
        if (c == 'A') {
            unsigned int port = 0;
            if (sscanf(p + 1, "%x", &port) == 1 && port >= 0x210 && port <= 0x280) {
                sb_base = (uint16_t)port;
            }
        } else if (c == 'D') {
            int d = p[1] - '0';
            if (d >= 0 && d <= 3) {
                sb_dma = d;
            }
        }
    }
}

static void sb_play_dma(const uint8_t *pcm, int len) {
    uint16_t offset, count;
    uint8_t  page;

    if (!sb_active || !pcm || len < 4) return;
    if (len > SFX_DMA_MAX) len = SFX_DMA_MAX;

    /* 1. Mask DMA channel, pause any active 8-bit DSP transfer, and ack pending DSP IRQ */
    outportb(0x0A, (uint8_t)(0x04 | sb_dma));
    sb_dsp_write(0xD0);
    (void)inportb((uint16_t)(sb_base + 0x0E));

    /* 2. Copy unsigned 8-bit PCM into 64KB-aligned DOS conventional memory */
    dosmemput(pcm, (size_t)len, dma_phys);

    offset = (uint16_t)(dma_phys & 0xFFFFU);
    page   = (uint8_t)((dma_phys >> 16) & 0xFFU);
    count  = (uint16_t)(len - 1);

    /* 3. Program ISA 8237 DMA controller for Single-Cycle Playback (0x48 | chan) */
    outportb(0x0C, 0x00);
    outportb(0x0B, (uint8_t)(0x48 | sb_dma));
    outportb(dma_addr_port[sb_dma],  (uint8_t)(offset & 0xFFU));
    outportb(dma_addr_port[sb_dma],  (uint8_t)((offset >> 8) & 0xFFU));
    outportb(dma_count_port[sb_dma], (uint8_t)(count & 0xFFU));
    outportb(dma_count_port[sb_dma], (uint8_t)((count >> 8) & 0xFFU));
    outportb(dma_page_port[sb_dma],  page);
    outportb(0x0A, (uint8_t)sb_dma);

    /* 4. Set Sound Blaster Time Constant (~11,025 Hz -> TC = 166) & start 8-bit DMA (0x14) */
    sb_dsp_write(0x40);
    sb_dsp_write((uint8_t)(256 - (1000000 / SFX_SAMPLE_RATE)));
    sb_dsp_write(0x14);
    sb_dsp_write((uint8_t)(count & 0xFFU));
    sb_dsp_write((uint8_t)((count >> 8) & 0xFFU));
}

/* Open external WAV asset from assets/ or current directory */
static FILE *open_sfx_file(const char *name) {
    char path[128];
    FILE *fp = fopen(name, "rb");
    if (fp) return fp;
    snprintf(path, sizeof(path), "assets/%s", name);
    fp = fopen(path, "rb");
    if (fp) return fp;
    snprintf(path, sizeof(path), "../assets/%s", name);
    return fopen(path, "rb");
}

/* Load any uncompressed 8/16-bit Mono/Stereo RIFF .WAV and convert to 11,025 Hz 8-bit unsigned PCM */
static uint8_t *wav_load_11k_u8(const char *name, int *out_len) {
    FILE *fp = open_sfx_file(name);
    uint8_t riff[12];
    uint16_t audio_fmt = 0, channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    int got_fmt = 0;

    if (!fp) return NULL;
    if (fread(riff, 1, 12, fp) != 12 ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        fclose(fp);
        return NULL;
    }

    for (;;) {
        uint8_t chdr[8];
        uint32_t csize;
        if (fread(chdr, 1, 8, fp) != 8) break;
        memcpy(&csize, chdr + 4, 4);

        if (memcmp(chdr, "fmt ", 4) == 0 && csize >= 16) {
            uint8_t fmtbuf[16];
            if (fread(fmtbuf, 1, 16, fp) != 16) break;
            memcpy(&audio_fmt,       fmtbuf + 0,  2);
            memcpy(&channels,        fmtbuf + 2,  2);
            memcpy(&sample_rate,     fmtbuf + 4,  4);
            memcpy(&bits_per_sample, fmtbuf + 14, 2);
            if (csize > 16) {
                fseek(fp, (long)((csize - 16) + (csize & 1U)), SEEK_CUR);
            }
            got_fmt = 1;
        } else if (memcmp(chdr, "data", 4) == 0 && got_fmt) {
            uint32_t bytes_per_frame, in_frames, max_in_frames, raw_bytes;
            int out_samples, i;
            uint8_t *raw = NULL;
            uint8_t *dst = NULL;

            if (audio_fmt != 1 || (channels != 1 && channels != 2) ||
                (bits_per_sample != 8 && bits_per_sample != 16) ||
                sample_rate < 4000 || sample_rate > 96000) {
                break;
            }

            bytes_per_frame = (uint32_t)((bits_per_sample / 8) * channels);
            in_frames = csize / bytes_per_frame;
            if (in_frames < 4) break;

            out_samples = (int)(((uint64_t)in_frames * (uint64_t)SFX_SAMPLE_RATE) / sample_rate);
            if (out_samples > SFX_DMA_MAX) {
                out_samples = SFX_DMA_MAX;
                max_in_frames = (uint32_t)(((uint64_t)out_samples * sample_rate) / SFX_SAMPLE_RATE) + 2;
                if (in_frames > max_in_frames) in_frames = max_in_frames;
            }

            raw_bytes = in_frames * bytes_per_frame;
            raw = (uint8_t *)malloc((size_t)raw_bytes);
            dst = (uint8_t *)malloc((size_t)out_samples);
            if (!raw || !dst) {
                free(raw);
                free(dst);
                break;
            }

            raw_bytes = (uint32_t)fread(raw, 1, (size_t)raw_bytes, fp);
            in_frames = raw_bytes / bytes_per_frame;
            if (in_frames < 4) {
                free(raw);
                free(dst);
                break;
            }

            for (i = 0; i < out_samples; i++) {
                uint32_t src_idx = (uint32_t)(((uint64_t)i * sample_rate) / SFX_SAMPLE_RATE);
                if (src_idx >= in_frames) src_idx = in_frames - 1;

                if (bits_per_sample == 8) {
                    const uint8_t *p = raw + src_idx * bytes_per_frame;
                    int v = (channels == 2) ? (((int)p[0] + (int)p[1]) >> 1) : (int)p[0];
                    dst[i] = (uint8_t)v;
                } else {
                    const uint8_t *p = raw + src_idx * bytes_per_frame;
                    int16_t s0, s1;
                    int v;
                    memcpy(&s0, p, 2);
                    if (channels == 2) {
                        memcpy(&s1, p + 2, 2);
                        v = (((int)s0 + (int)s1) >> 9) + 128;
                    } else {
                        v = ((int)s0 >> 8) + 128;
                    }
                    if (v < 0) v = 0;
                    if (v > 255) v = 255;
                    dst[i] = (uint8_t)v;
                }
            }

            free(raw);
            fclose(fp);
            if (out_len) *out_len = out_samples;
            return dst;
        } else {
            fseek(fp, (long)(csize + (csize & 1U)), SEEK_CUR);
        }
    }

    fclose(fp);
    return NULL;
}

/* Fast deterministic LCG noise helper (-127..+127) */
static uint32_t sfx_rng = 0x1994DF01U;
static int noise_s8(void) {
    sfx_rng = sfx_rng * 1664525U + 1013904223U;
    return (int)((sfx_rng >> 24) & 0xFFU) - 128;
}

static void alloc_proc_sfx(int id, int len) {
    if (len > SFX_DMA_MAX) len = SFX_DMA_MAX;
    g_sfx[id].pcm = (uint8_t *)malloc((size_t)len);
    g_sfx[id].len = g_sfx[id].pcm ? len : 0;
    if (g_sfx[id].pcm) memset(g_sfx[id].pcm, 128, (size_t)len);
}

static uint8_t clamp_u8(int s) {
    int v = 128 + s;
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

/* Procedural 8-bit PCM fallback synthesizer (overridden by assets/*.WAV when present) */
static void build_procedural_fallbacks(void) {
    int i, n;
    double ph = 0.0;

    /* 1. CLICK */
    n = 260; alloc_proc_sfx(SFX_ID_CLICK, n);
    for (i = 0; i < n && g_sfx[SFX_ID_CLICK].pcm; i++) {
        int env = ((n - i) * 90) / n;
        int sq  = ((i % 9) < 4) ? env : -env;
        g_sfx[SFX_ID_CLICK].pcm[i] = clamp_u8(sq);
    }

    /* 2 & 3. STEP_A & STEP_B (Left / Right boot thud + gravel crunch) */
    for (int st = 0; st < 2; st++) {
        int id = (st == 0) ? SFX_ID_STEP_A : SFX_ID_STEP_B;
        int lp = 0;
        n = 920; alloc_proc_sfx(id, n);
        ph = 0.0;
        for (i = 0; i < n && g_sfx[id].pcm; i++) {
            double t = (double)i / SFX_SAMPLE_RATE;
            double env = exp(-t * 34.0);
            double f = (st == 0 ? 135.0 : 152.0) * exp(-t * 16.0);
            int raw_n = noise_s8();
            ph += 6.2831853 * f / SFX_SAMPLE_RATE;
            lp = (lp * 3 + raw_n) / 4;
            g_sfx[id].pcm[i] = clamp_u8((int)((sin(ph) * 75.0 + lp * 0.7) * env));
        }
    }

    /* 4. DOOR (Hydraulic slide + latch clunk) */
    n = 2600; alloc_proc_sfx(SFX_ID_DOOR, n);
    {
        int lp = 0;
        for (i = 0; i < n && g_sfx[SFX_ID_DOOR].pcm; i++) {
            double env = (i < 2100) ? sin((3.14159 * i) / 2100.0) : exp(-(i - 2100) / 110.0);
            int clunk = (i >= 2100) ? (int)(sin(i * 0.14) * 70.0 * env) : 0;
            lp = (lp * 5 + noise_s8()) / 6;
            g_sfx[SFX_ID_DOOR].pcm[i] = clamp_u8((int)(lp * 0.85 * env) + clunk);
        }
    }

    /* 5. ASSAULT RIFLE (Sharp gunshot crack + low-end punch + chamber echo) */
    n = 2800; alloc_proc_sfx(SFX_ID_RIFLE, n);
    {
        int lp = 0;
        ph = 0.0;
        for (i = 0; i < n && g_sfx[SFX_ID_RIFLE].pcm; i++) {
            double t = (double)i / SFX_SAMPLE_RATE;
            double crack = exp(-t * 38.0);
            double tail  = exp(-t * 11.0);
            ph += 6.2831853 * (190.0 * exp(-t * 22.0) + 48.0) / SFX_SAMPLE_RATE;
            lp = (lp * 2 + noise_s8()) / 3;
            g_sfx[SFX_ID_RIFLE].pcm[i] = clamp_u8((int)(sin(ph) * 95.0 * crack + lp * (0.95 * crack + 0.35 * tail)));
        }
    }

    /* 6. LASER RIFLE (Capacitor chirp sweep + harmonic) */
    n = 2600; alloc_proc_sfx(SFX_ID_LASER, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_LASER].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = 2100.0 * exp(-t * 9.5) + 210.0;
        double env = exp(-t * 7.5);
        ph += 6.2831853 * f / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_LASER].pcm[i] = clamp_u8((int)((sin(ph) * 80.0 + sin(ph * 2.0) * 35.0) * env));
    }

    /* 7. HEAVY PLASMA (Deep X-COM FM plasma discharge + sizzle) */
    n = 3600; alloc_proc_sfx(SFX_ID_PLASMA, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_PLASMA].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = 680.0 * exp(-t * 5.2) + 95.0 + 45.0 * sin(t * 95.0);
        double env = exp(-t * 5.5);
        ph += 6.2831853 * f / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_PLASMA].pcm[i] = clamp_u8((int)((sin(ph + sin(ph * 1.5)) * 82.0 + noise_s8() * 0.32) * env));
    }

    /* 8. EXPLOSION (Deep concussion rumble + debris roar) */
    n = 8200; alloc_proc_sfx(SFX_ID_EXPLODE, n);
    {
        int lp = 0;
        ph = 0.0;
        for (i = 0; i < n && g_sfx[SFX_ID_EXPLODE].pcm; i++) {
            double t = (double)i / SFX_SAMPLE_RATE;
            double env = exp(-t * 3.4);
            ph += 6.2831853 * (95.0 * exp(-t * 4.0) + 32.0) / SFX_SAMPLE_RATE;
            lp = (lp * 6 + noise_s8()) / 7;
            g_sfx[SFX_ID_EXPLODE].pcm[i] = clamp_u8((int)((lp * 1.35 + sin(ph) * 65.0) * env));
        }
    }

    /* 9. GLASS SHATTER */
    n = 2600; alloc_proc_sfx(SFX_ID_GLASS, n);
    for (i = 0; i < n && g_sfx[SFX_ID_GLASS].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double env = exp(-t * 10.0);
        double chime = sin(t * 6.28318 * 2450.0) + sin(t * 6.28318 * 3380.0) + sin(t * 6.28318 * 4190.0);
        g_sfx[SFX_ID_GLASS].pcm[i] = clamp_u8((int)((chime * 24.0 + noise_s8() * 0.55) * env));
    }

    /* 10. IMPACT HIT */
    n = 1500; alloc_proc_sfx(SFX_ID_HIT, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_HIT].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double env = exp(-t * 18.0);
        ph += 6.2831853 * (220.0 * exp(-t * 15.0) + 55.0) / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_HIT].pcm[i] = clamp_u8((int)((sin(ph) * 85.0 + noise_s8() * 0.45) * env));
    }

    /* 11. SECTOID DEATH SCREAM */
    n = 6800; alloc_proc_sfx(SFX_ID_ALIEN_DIE, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_ALIEN_DIE].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = (1280.0 * exp(-t * 2.4) + 210.0) + 75.0 * sin(t * 95.0);
        double env = sin(3.14159 * (double)i / n) * exp(-t * 1.1);
        ph += 6.2831853 * f / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_ALIEN_DIE].pcm[i] = clamp_u8((int)((sin(ph) * 68.0 + sin(ph * 2.41) * 38.0 + noise_s8() * 0.24) * env));
    }

    /* 12. FLOATER CYBERNETIC DEATH SHRIEK */
    n = 7400; alloc_proc_sfx(SFX_ID_FLOATER_DIE, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_FLOATER_DIE].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = (960.0 * exp(-t * 2.1) + 165.0) + 48.0 * sin(t * 130.0);
        double env = sin(3.14159 * (double)i / n);
        ph += 6.2831853 * f / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_FLOATER_DIE].pcm[i] = clamp_u8((int)((sin(ph + 1.4 * sin(ph * 1.5)) * 72.0 + noise_s8() * 0.35) * env));
    }

    /* 13. MUTON GUTTURAL DEATH ROAR */
    n = 8100; alloc_proc_sfx(SFX_ID_MUTON_DIE, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_MUTON_DIE].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = 148.0 * exp(-t * 1.1) + 58.0 + 14.0 * sin(t * 210.0);
        double env = sin(3.14159 * (double)i / n);
        ph += 6.2831853 * f / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_MUTON_DIE].pcm[i] = clamp_u8((int)((sin(ph) * 64.0 + sin(ph * 2.0) * 42.0 + sin(ph * 3.0) * 28.0 + noise_s8() * 0.25) * env));
    }

    /* 14. MALE OPERATIVE DEATH SCREAM ("AAARRGH!") */
    n = 7400; alloc_proc_sfx(SFX_ID_MAN_DIE, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_MAN_DIE].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = (t < 0.09) ? (220.0 + t * 380.0) : (254.0 * exp(-(t - 0.09) * 1.35) + 92.0);
        double env = sin(3.14159 * (double)i / n);
        ph += 6.2831853 * (f + 6.0 * sin(t * 42.0)) / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_MAN_DIE].pcm[i] = clamp_u8((int)((sin(ph) * 42.0 + sin(ph * 3.0) * 48.0 + sin(ph * 5.0) * 28.0 + noise_s8() * 0.22) * env));
    }

    /* 15. FEMALE OPERATIVE DEATH SCREAM ("AAAHHH!") */
    n = 7100; alloc_proc_sfx(SFX_ID_FEM_DIE, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_FEM_DIE].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = (t < 0.08) ? (410.0 + t * 850.0) : (478.0 * exp(-(t - 0.08) * 1.25) + 190.0);
        double env = sin(3.14159 * (double)i / n);
        ph += 6.2831853 * (f + 11.0 * sin(t * 48.0)) / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_FEM_DIE].pcm[i] = clamp_u8((int)((sin(ph) * 52.0 + sin(ph * 2.0) * 44.0 + sin(ph * 3.0) * 24.0 + noise_s8() * 0.18) * env));
    }

    /* 16. TACTICAL ALERT (Two-tone X-COM motion/spotted chime) */
    n = 2200; alloc_proc_sfx(SFX_ID_ALERT, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_ALERT].pcm; i++) {
        double f = (i < n / 2) ? 784.0 : 1046.5;
        ph += 6.2831853 * f / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_ALERT].pcm[i] = clamp_u8((int)(sin(ph) * 75.0));
    }

    /* 17. MALE CIVILIAN PANIC SCREAM ("YAAAH!") */
    n = 5600; alloc_proc_sfx(SFX_ID_CIV_SCREAM_M, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_CIV_SCREAM_M].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = (t < 0.14) ? (290.0 + t * 1100.0) : (444.0 * exp(-(t - 0.14) * 1.65) + 145.0);
        double env = sin(3.14159 * (double)i / n);
        ph += 6.2831853 * (f + 14.0 * sin(t * 58.0)) / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_CIV_SCREAM_M].pcm[i] = clamp_u8((int)((sin(ph) * 50.0 + sin(ph * 2.0) * 44.0 + sin(ph * 3.0) * 26.0 + noise_s8() * 0.20) * env));
    }

    /* 18. FEMALE CIVILIAN PANIC SHRIEK ("EEEEAAAH!") */
    n = 5800; alloc_proc_sfx(SFX_ID_CIV_SCREAM_F, n);
    ph = 0.0;
    for (i = 0; i < n && g_sfx[SFX_ID_CIV_SCREAM_F].pcm; i++) {
        double t = (double)i / SFX_SAMPLE_RATE;
        double f = (t < 0.12) ? (580.0 + t * 2100.0) : (832.0 * exp(-(t - 0.12) * 1.45) + 310.0);
        double env = sin(3.14159 * (double)i / n);
        ph += 6.2831853 * (f + 22.0 * sin(t * 68.0)) / SFX_SAMPLE_RATE;
        g_sfx[SFX_ID_CIV_SCREAM_F].pcm[i] = clamp_u8((int)((sin(ph) * 58.0 + sin(ph * 2.0) * 42.0 + sin(ph * 3.0) * 20.0 + noise_s8() * 0.16) * env));
    }
}

static void load_wav_overrides(void) {
    static const char *wav_files[SFX_COUNT] = {
        "CLICK.WAV",
        "STEPA.WAV",
        "STEPB.WAV",
        "DOOR.WAV",
        "RIFLE.WAV",
        "LASER.WAV",
        "PLASMA.WAV",
        "EXPLODE.WAV",
        "GLASS.WAV",
        "HIT.WAV",
        "ALIENDIE.WAV",
        "FLOATDIE.WAV",
        "MUTONDIE.WAV",
        "MANDIE.WAV",
        "FEMDIE.WAV",
        "ALERT.WAV",
        "CIVSCRM.WAV",
        "CIVSCRF.WAV"
    };
    int i;
    for (i = 0; i < SFX_COUNT; i++) {
        int len = 0;
        uint8_t *buf = wav_load_11k_u8(wav_files[i], &len);
        if (buf && len > 0) {
            free(g_sfx[i].pcm);
            g_sfx[i].pcm = buf;
            g_sfx[i].len = len;
        }
    }
}

void sound_init(void) {
    static const uint16_t probe_ports[4] = { 0x220, 0x240, 0x210, 0x230 };
    int i, seg;

    memset(g_sfx, 0, sizeof(g_sfx));
    sb_active = 0;
    sb_base   = 0x220;
    sb_dma    = 1;

    parse_blaster_env();

    /* Probe configured BLASTER port first, then standard fallback ports */
    if (sb_dsp_reset(sb_base)) {
        sb_active = 1;
    } else {
        for (i = 0; i < 4; i++) {
            if (sb_dsp_reset(probe_ports[i])) {
                sb_base   = probe_ports[i];
                sb_active = 1;
                break;
            }
        }
    }

    if (sb_active) {
        /* Allocate 2 * SFX_DMA_MAX (48 KB = 3072 paragraphs) so half is guaranteed 64KB-page safe */
        seg = __dpmi_allocate_dos_memory((SFX_DMA_MAX * 2) / 16, &dma_sel);
        if (seg >= 0) {
            uint32_t phys = (uint32_t)seg << 4;
            if ((phys & 0xFFFF0000U) != ((phys + SFX_DMA_MAX - 1) & 0xFFFF0000U)) {
                dma_phys = phys + SFX_DMA_MAX;
            } else {
                dma_phys = phys;
            }
            /* Enable Sound Blaster DAC Speaker Output (0xD1) */
            sb_dsp_write(0xD1);
        } else {
            sb_active = 0;
        }
    }

    build_procedural_fallbacks();
    load_wav_overrides();
}

void sound_shutdown(void) {
    int i;
    if (sb_active) {
        outportb(0x0A, (uint8_t)(0x04 | sb_dma));
        sb_dsp_write(0xD0);
        sb_dsp_write(0xD3); /* Turn off Sound Blaster speaker */
        (void)inportb((uint16_t)(sb_base + 0x0E));
        if (dma_sel >= 0) {
            __dpmi_free_dos_memory(dma_sel);
            dma_sel = -1;
        }
        sb_active = 0;
    }
    nosound();
    for (i = 0; i < SFX_COUNT; i++) {
        free(g_sfx[i].pcm);
        g_sfx[i].pcm = NULL;
        g_sfx[i].len = 0;
    }
}

int sound_is_sb_active(void) {
    return sb_active;
}

void sound_play(int sfx_id) {
    if (!g_settings.sfx_enabled) return;
    if (sfx_id < 0 || sfx_id >= SFX_COUNT) return;

    if (sb_active && g_sfx[sfx_id].pcm && g_sfx[sfx_id].len > 0) {
        sb_play_dma(g_sfx[sfx_id].pcm, g_sfx[sfx_id].len);
        return;
    }

    /* Fallback to PC Speaker if Sound Blaster is disabled */
    switch (sfx_id) {
        case SFX_ID_CLICK:     sfx_beep(1100, 6); break;
        case SFX_ID_STEP_A:    sfx_beep(160,  8); break;
        case SFX_ID_STEP_B:    sfx_beep(195,  8); break;
        case SFX_ID_DOOR:      sfx_beep(440, 18); break;
        case SFX_ID_RIFLE:     sfx_beep(320, 22); break;
        case SFX_ID_LASER:     sfx_beep(1250, 24); break;
        case SFX_ID_PLASMA:    sfx_beep(540, 30); break;
        case SFX_ID_EXPLODE:   sfx_beep(110, 55); break;
        case SFX_ID_GLASS:     sfx_beep(1900, 20); break;
        case SFX_ID_HIT:         sfx_beep(240, 18); break;
        case SFX_ID_ALIEN_DIE:   sfx_beep(720, 65); break;
        case SFX_ID_FLOATER_DIE: sfx_beep(580, 65); break;
        case SFX_ID_MUTON_DIE:   sfx_beep(190, 75); break;
        case SFX_ID_MAN_DIE:     sfx_beep(260, 65); break;
        case SFX_ID_FEM_DIE:     sfx_beep(460, 65); break;
        case SFX_ID_ALERT:       sfx_beep(880, 30); break;
        case SFX_ID_CIV_SCREAM_M:sfx_beep(520, 55); break;
        case SFX_ID_CIV_SCREAM_F:sfx_beep(940, 55); break;
    }
}

void sound_play_weapon(int weapon_id) {
    if (weapon_id == 0)      sound_play(SFX_ID_RIFLE);
    else if (weapon_id == 1) sound_play(SFX_ID_LASER);
    else                     sound_play(SFX_ID_PLASMA);
}

void sound_play_death(int faction, int unit_type, int gender) {
    if (faction == 0 || faction == 2) {
        sound_play(gender ? SFX_ID_FEM_DIE : SFX_ID_MAN_DIE);
    } else if (unit_type == 2) {
        sound_play(SFX_ID_FLOATER_DIE);
    } else if (unit_type == 3) {
        sound_play(SFX_ID_MUTON_DIE);
    } else {
        sound_play(SFX_ID_ALIEN_DIE);
    }
}

void sound_play_civ_scream(int gender) {
    sound_play(gender ? SFX_ID_CIV_SCREAM_F : SFX_ID_CIV_SCREAM_M);
}

void sfx_beep(int freq_hz, int duration_ms) {
    if (!g_settings.sfx_enabled) return;
    if (sb_active) {
        sound_play(freq_hz >= 650 ? SFX_ID_ALERT : SFX_ID_HIT);
        return;
    }
    if (freq_hz > 0) {
        sound((unsigned)freq_hz);
        delay((unsigned)duration_ms);
        nosound();
    }
}

void sfx_click(void)     { sound_play(SFX_ID_CLICK); }
void sfx_step(void)      { step_toggle ^= 1; sound_play(step_toggle ? SFX_ID_STEP_A : SFX_ID_STEP_B); }
void sfx_door(void)      { sound_play(SFX_ID_DOOR); }
void sfx_rifle(void)     { sound_play(SFX_ID_RIFLE); }
void sfx_laser(void)     { sound_play(SFX_ID_LASER); }
void sfx_plasma(void)    { sound_play(SFX_ID_PLASMA); }
void sfx_explosion(void) { sound_play(SFX_ID_EXPLODE); }
void sfx_glass(void)     { sound_play(SFX_ID_GLASS); }
void sfx_hit(void)       { sound_play(SFX_ID_HIT); }
void sfx_alien_die(void) { sound_play(SFX_ID_ALIEN_DIE); }
void sfx_man_die(void)   { sound_play(SFX_ID_MAN_DIE); }
void sfx_fem_die(void)   { sound_play(SFX_ID_FEM_DIE); }
void sfx_alert(void)     { sound_play(SFX_ID_ALERT); }
