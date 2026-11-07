#include "music.h"
#include "dos_compat.h"
#include "sound.h"
#include <stdint.h>
#include <string.h>

/* AdLib / Sound Blaster Hardware OPL2 (YM3812) I/O Ports */
#define OPL_ADDR 0x388
#define OPL_DATA 0x389

/* Note Semitone Offsets (Scientific Pitch: NT(octave, semitone)) */
#define N_C   0
#define N_Cs  1
#define N_D   2
#define N_Ds  3
#define N_E   4
#define N_F   5
#define N_Fs  6
#define N_G   7
#define N_Gs  8
#define N_A   9
#define N_As 10
#define N_B  11

#define NT(oct, semi) ((uint8_t)((oct) * 12 + (semi)))
#define N_HOLD 0
#define N_OFF  255

/* Percussion Trigger Codes (Channels 6=Kick, 7=Snare, 8=Hi-Hat) */
#define D_NONE   0
#define D_KICK   1
#define D_TOM    2
#define D_SNARE  1
#define D_CLANK  2
#define D_HAT_C  1
#define D_HAT_O  2

typedef struct {
    uint8_t m_avekm; /* 0x20: AM(7) VIB(6) EGT(5) KSR(4) MULT(3..0) */
    uint8_t m_ksltl; /* 0x40: KSL(7..6) TL(5..0) */
    uint8_t m_ardr;  /* 0x60: AR(7..4) DR(3..0) */
    uint8_t m_slrr;  /* 0x80: SL(7..4) RR(3..0) */
    uint8_t m_wave;  /* 0xE0: Waveform (0..3) */

    uint8_t c_avekm;
    uint8_t c_ksltl;
    uint8_t c_ardr;
    uint8_t c_slrr;
    uint8_t c_wave;

    uint8_t fb_alg;  /* 0xC0: Feedback(3..1) | Algorithm(0: 0=FM, 1=Additive) */
    int8_t  detune;  /* Fine F-Number detune for rich double-tracked thickness */
    uint8_t gate;    /* Auto Key-Off after `gate` frames (0 = sustain until N_OFF) */
} OplPatch;

/* YM3812 Operator Register Offsets for Channels 0..8 */
static const uint8_t mod_off[9] = { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 };
static const uint8_t car_off[9] = { 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15 };

/* Accurate 49,716 Hz OPL2 F-Numbers for C..B (Block = Scientific Octave) */
static const uint16_t note_fnum[12] = {
    345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651
};

/* Volume attenuation table (in 0.75 dB OPL2 TL steps): OFF, LOW, MED, HIGH */
static const uint8_t vol_atten[4] = { 63, 14, 5, 0 };

static int      opl_active     = 0;
static int      cur_track      = MUSIC_TRACK_NONE;
static int      cur_vol        = 2; /* Default MEDIUM */
static int      frame_in_step  = 0;
static int      cur_step       = 0;
static uint32_t drm_rng        = 0x94C009E5U;

static OplPatch ch_patch[9];
static uint8_t  ch_reg_b0[9];
static uint8_t  ch_gate_rem[9];

/* Active per-frame drum synthesis state (Ch 6=Kick/Tom, Ch 7=Snare, Ch 8=Hat) */
static int kick_age  = -1;
static int kick_type = 0;
static int snare_age = -1;
static int snare_type= 0;
static int hat_age   = -1;
static int hat_limit = 0;

static void opl_write(uint8_t reg, uint8_t val) {
    int i;
    outportb(OPL_ADDR, reg);
    for (i = 0; i < 6; i++) (void)inportb(OPL_ADDR);
    outportb(OPL_DATA, val);
    for (i = 0; i < 35; i++) (void)inportb(OPL_ADDR);
}

static uint8_t scale_tl(uint8_t ksltl, int atten) {
    uint8_t ksl = ksltl & 0xC0U;
    int tl = (int)(ksltl & 0x3FU) + atten;
    if (tl > 63) tl = 63;
    return (uint8_t)(ksl | (uint8_t)tl);
}

static void apply_channel_volume(int ch) {
    const OplPatch *p = &ch_patch[ch];
    int atten = (cur_vol <= 0) ? 63 : (int)vol_atten[cur_vol & 3];
    if (p->fb_alg & 1) {
        opl_write((uint8_t)(0x40 + mod_off[ch]), scale_tl(p->m_ksltl, atten));
    } else {
        opl_write((uint8_t)(0x40 + mod_off[ch]), p->m_ksltl);
    }
    opl_write((uint8_t)(0x40 + car_off[ch]), scale_tl(p->c_ksltl, atten));
}

static void opl_load_patch(int ch, const OplPatch *p) {
    uint8_t m = mod_off[ch];
    uint8_t c = car_off[ch];
    ch_patch[ch] = *p;

    opl_write((uint8_t)(0x20 + m), p->m_avekm);
    opl_write((uint8_t)(0x60 + m), p->m_ardr);
    opl_write((uint8_t)(0x80 + m), p->m_slrr);
    opl_write((uint8_t)(0xE0 + m), p->m_wave);

    opl_write((uint8_t)(0x20 + c), p->c_avekm);
    opl_write((uint8_t)(0x60 + c), p->c_ardr);
    opl_write((uint8_t)(0x80 + c), p->c_slrr);
    opl_write((uint8_t)(0xE0 + c), p->c_wave);

    opl_write((uint8_t)(0xC0 + ch), p->fb_alg);
    apply_channel_volume(ch);
}

static void opl_key_off(int ch) {
    if (ch_reg_b0[ch] & 0x20U) {
        ch_reg_b0[ch] &= (uint8_t)~0x20U;
        opl_write((uint8_t)(0xB0 + ch), ch_reg_b0[ch]);
    }
    ch_gate_rem[ch] = 0;
}

static void opl_note_on(int ch, uint8_t note) {
    int oct  = (int)(note / 12);
    int semi = (int)(note % 12);
    int fnum;
    uint8_t b0;

    if (oct < 1) oct = 1;
    if (oct > 7) oct = 7;

    fnum = (int)note_fnum[semi] + (int)ch_patch[ch].detune;
    if (fnum < 1) fnum = 1;
    if (fnum > 1023) fnum = 1023;

    /* Retrigger envelope cleanly by clearing Key-On first */
    opl_key_off(ch);

    b0 = (uint8_t)(0x20U | ((oct & 7) << 2) | ((fnum >> 8) & 3));
    opl_write((uint8_t)(0xA0 + ch), (uint8_t)(fnum & 0xFFU));
    opl_write((uint8_t)(0xB0 + ch), b0);
    ch_reg_b0[ch]   = b0;
    ch_gate_rem[ch] = ch_patch[ch].gate;
}

static void opl_set_raw_freq(int ch, int block, int fnum, int key_on) {
    uint8_t b0;
    if (block < 0) block = 0;
    if (block > 7) block = 7;
    if (fnum < 0) fnum = 0;
    if (fnum > 1023) fnum = 1023;
    b0 = (uint8_t)((key_on ? 0x20U : 0x00U) | ((block & 7) << 2) | ((fnum >> 8) & 3));
    opl_write((uint8_t)(0xA0 + ch), (uint8_t)(fnum & 0xFFU));
    opl_write((uint8_t)(0xB0 + ch), b0);
    ch_reg_b0[ch] = b0;
}

static void silence_all_channels(void) {
    int ch;
    for (ch = 0; ch < 9; ch++) {
        opl_write((uint8_t)(0xB0 + ch), 0x00);
        opl_write((uint8_t)(0x40 + mod_off[ch]), 0x3F);
        opl_write((uint8_t)(0x40 + car_off[ch]), 0x3F);
        ch_reg_b0[ch]   = 0;
        ch_gate_rem[ch] = 0;
    }
    kick_age  = -1;
    snare_age = -1;
    hat_age   = -1;
}

/* =========================================================================
 * TRACK 1: TITLE SCREEN — "COOPERSBURG INDUSTRIAL PROTOCOL"
 * 90s Industrial Moody Metal (Nine Inch Nails vein) — 105 BPM (10 frames/16th)
 * ========================================================================= */
static const OplPatch title_patches[9] = {
    /* Ch 0: Snarling Overdriven NIN FM Synth Bass (Abs-Sine -> Pulse-Sine, FB=6) */
    { 0x01, 0x0D, 0xF5, 0x37, 0x02,  0x01, 0x06, 0xF4, 0x26, 0x03,  0x0C,  0, 8 },
    /* Ch 1: Heavy Drop-D Distorted Guitar Left (Pulse-Sine FB=7 -> Half-Sine) */
    { 0x21, 0x09, 0xF2, 0x16, 0x03,  0x21, 0x0A, 0xF3, 0x26, 0x01,  0x0E,  0, 0 },
    /* Ch 2: Heavy Distorted Guitar Double/Fifth (Detuned +3 for wide wall-of-sound) */
    { 0x22, 0x0B, 0xF2, 0x16, 0x03,  0x21, 0x0B, 0xF3, 0x26, 0x02,  0x0E,  3, 0 },
    /* Ch 3: Haunting Reznor Detuned Vibrato Synth Lead */
    { 0x62, 0x12, 0xB4, 0x25, 0x01,  0x61, 0x09, 0xC3, 0x15, 0x02,  0x0A,  2, 0 },
    /* Ch 4: Dark Industrial Sub-Drone */
    { 0x21, 0x18, 0x52, 0x24, 0x00,  0x21, 0x0E, 0x52, 0x14, 0x01,  0x08, -1, 0 },
    /* Ch 5: 16th-Note Mechanical Electro-Sequencer Pulse */
    { 0x04, 0x14, 0xF9, 0x69, 0x03,  0x02, 0x10, 0xF8, 0x58, 0x01,  0x0A,  0, 4 },
    /* Ch 6: Punchy Industrial Kick Drum (Pitch-swept in music_tick) */
    { 0x01, 0x0E, 0xF8, 0x89, 0x02,  0x01, 0x04, 0xF7, 0x48, 0x00,  0x0A,  0, 0 },
    /* Ch 7: Gated Metallic Industrial Snare (Noise-modulated in music_tick) */
    { 0x03, 0x06, 0xF8, 0x48, 0x03,  0x01, 0x07, 0xF7, 0x37, 0x03,  0x0E,  0, 0 },
    /* Ch 8: Crisp Industrial 16th Hi-Hat */
    { 0x07, 0x05, 0xFA, 0x6A, 0x03,  0x05, 0x0E, 0xFB, 0x6B, 0x03,  0x0E,  0, 0 }
};

/* 128-Step (8-Bar) Title Pattern: D Phrygian / Tritone G# Industrial Riff + Synth Hook */
static const uint8_t title_bass[128] = {
    /* Bar 1: Syncopated D2 / F2 / Tritone G#2 industrial bass groove */
    NT(2,N_D),  NT(2,N_D),  N_OFF,      NT(2,N_D),  NT(2,N_F),  N_OFF,      NT(2,N_D),  NT(2,N_D),
    NT(2,N_Gs), N_HOLD,     NT(2,N_G),  N_OFF,      NT(2,N_F),  N_OFF,      NT(2,N_Ds), NT(2,N_D),
    /* Bar 2 */
    NT(2,N_D),  NT(2,N_D),  N_OFF,      NT(2,N_D),  NT(2,N_F),  N_OFF,      NT(2,N_G),  N_OFF,
    NT(2,N_Gs), N_HOLD,     NT(2,N_G),  N_HOLD,     NT(2,N_F),  NT(2,N_E),  NT(2,N_Ds), NT(2,N_Cs),
    /* Bar 3: Heavy Riff Entry */
    NT(2,N_D),  NT(2,N_D),  N_OFF,      NT(2,N_D),  NT(2,N_F),  N_OFF,      NT(2,N_D),  NT(2,N_D),
    NT(2,N_Gs), N_HOLD,     NT(2,N_G),  N_OFF,      NT(2,N_F),  N_OFF,      NT(2,N_Ds), NT(2,N_D),
    /* Bar 4 */
    NT(1,N_As), N_HOLD,     NT(1,N_As), N_OFF,      NT(1,N_A),  N_HOLD,     NT(1,N_A),  N_OFF,
    NT(2,N_Gs), N_HOLD,     NT(2,N_G),  N_HOLD,     NT(2,N_F),  N_OFF,      NT(2,N_E),  NT(2,N_Ds),
    /* Bar 5: Driving 16th Octave Synth-Bass Breakdown */
    NT(2,N_D),  NT(3,N_D),  NT(2,N_D),  NT(2,N_D),  NT(2,N_F),  NT(3,N_F),  NT(2,N_D),  NT(2,N_D),
    NT(2,N_Gs), NT(3,N_Gs), NT(2,N_G),  NT(2,N_G),  NT(2,N_F),  NT(2,N_F),  NT(2,N_Ds), NT(2,N_E),
    /* Bar 6 */
    NT(2,N_D),  NT(3,N_D),  NT(2,N_D),  NT(2,N_D),  NT(2,N_C),  NT(3,N_C),  NT(2,N_D),  NT(2,N_F),
    NT(2,N_Gs), N_HOLD,     NT(2,N_G),  N_HOLD,     NT(2,N_F),  NT(2,N_E),  NT(2,N_Ds), NT(2,N_Cs),
    /* Bar 7: Full Climax Chromatic Descent */
    NT(1,N_As), NT(1,N_As), N_HOLD,     NT(1,N_As), NT(1,N_A),  NT(1,N_A),  N_HOLD,     NT(1,N_A),
    NT(2,N_Gs), NT(2,N_Gs), N_HOLD,     NT(2,N_G),  N_HOLD,     NT(2,N_F),  N_HOLD,     NT(2,N_E),
    /* Bar 8: Turnaround Slam */
    NT(2,N_Ds), NT(2,N_Ds), N_OFF,      NT(2,N_Ds), NT(2,N_D),  NT(2,N_D),  N_OFF,      NT(2,N_C),
    NT(2,N_Cs), N_HOLD,     NT(2,N_Cs), N_OFF,      NT(2,N_D),  NT(2,N_D),  NT(1,N_A),  NT(2,N_Cs)
};

static const uint8_t title_gtr1[128] = {
    /* Bar 1: Tacet while industrial bass & drums establish the groove */
    N_OFF,  0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
    /* Bar 2: Muted build-up chugs on beat 3-4 */
    0,      0, 0, 0,  0, 0, 0, 0,  NT(2,N_Gs), 0, NT(2,N_G), 0,  NT(2,N_F), N_OFF, NT(2,N_Ds), N_OFF,
    /* Bar 3: Crushing Drop-D Power Chords */
    NT(2,N_D),  0, N_OFF, NT(2,N_D),  NT(2,N_F), N_OFF, NT(2,N_D), N_OFF,
    NT(2,N_Gs), 0, NT(2,N_G), N_OFF,  NT(2,N_F), N_OFF, NT(2,N_Ds), N_OFF,
    /* Bar 4 */
    NT(1,N_As), 0, 0, N_OFF,  NT(1,N_A), 0, 0, N_OFF,
    NT(2,N_Gs), 0, NT(2,N_G), 0,  NT(2,N_F), N_OFF, NT(2,N_E), NT(2,N_Ds),
    /* Bar 5: Staccato Industrial Off-Beat Stabs */
    NT(2,N_D), N_OFF, 0, 0,  0, 0, NT(2,N_F), N_OFF,
    NT(2,N_Gs), 0, N_OFF, 0,  NT(2,N_G), N_OFF, NT(2,N_Ds), N_OFF,
    /* Bar 6 */
    NT(2,N_D), N_OFF, 0, 0,  0, 0, NT(2,N_G), N_OFF,
    NT(2,N_Gs), 0, NT(2,N_G), 0,  NT(2,N_F), N_OFF, NT(2,N_Ds), NT(2,N_Cs),
    /* Bar 7: Full Heavy Wall of Sound */
    NT(1,N_As), 0, 0, NT(1,N_As),  NT(1,N_A), 0, 0, NT(1,N_A),
    NT(2,N_Gs), 0, 0, NT(2,N_G),   0, NT(2,N_F), 0, NT(2,N_E),
    /* Bar 8 */
    NT(2,N_Ds), 0, N_OFF, NT(2,N_Ds),  NT(2,N_D), 0, N_OFF, NT(2,N_C),
    NT(2,N_Cs), 0, 0, N_OFF,  NT(2,N_D), N_OFF, NT(2,N_D), N_OFF
};

static const uint8_t title_gtr2[128] = {
    /* Bar 1 */
    N_OFF,  0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
    /* Bar 2: Power-chord 5ths */
    0,      0, 0, 0,  0, 0, 0, 0,  NT(3,N_Ds), 0, NT(3,N_D), 0,  NT(3,N_C), N_OFF, NT(2,N_As), N_OFF,
    /* Bar 3 */
    NT(2,N_A),  0, N_OFF, NT(2,N_A),  NT(3,N_C), N_OFF, NT(2,N_A), N_OFF,
    NT(3,N_Ds), 0, NT(3,N_D), N_OFF,  NT(3,N_C), N_OFF, NT(2,N_As), N_OFF,
    /* Bar 4 */
    NT(2,N_F),  0, 0, N_OFF,  NT(2,N_E), 0, 0, N_OFF,
    NT(3,N_Ds), 0, NT(3,N_D), 0,  NT(3,N_C), N_OFF, NT(2,N_B), NT(2,N_As),
    /* Bar 5 */
    NT(2,N_A), N_OFF, 0, 0,  0, 0, NT(3,N_C), N_OFF,
    NT(3,N_Ds), 0, N_OFF, 0,  NT(3,N_D), N_OFF, NT(2,N_As), N_OFF,
    /* Bar 6 */
    NT(2,N_A), N_OFF, 0, 0,  0, 0, NT(3,N_D), N_OFF,
    NT(3,N_Ds), 0, NT(3,N_D), 0,  NT(3,N_C), N_OFF, NT(2,N_As), NT(2,N_Gs),
    /* Bar 7 */
    NT(2,N_F),  0, 0, NT(2,N_F),  NT(2,N_E), 0, 0, NT(2,N_E),
    NT(3,N_Ds), 0, 0, NT(3,N_D),  0, NT(3,N_C), 0, NT(2,N_B),
    /* Bar 8 */
    NT(2,N_As), 0, N_OFF, NT(2,N_As),  NT(2,N_A), 0, N_OFF, NT(2,N_G),
    NT(2,N_Gs), 0, 0, N_OFF,  NT(2,N_A), N_OFF, NT(2,N_A), N_OFF
};

static const uint8_t title_lead[128] = {
    /* Bars 1-2: Eerie sparse industrial whistle over intro */
    N_OFF, 0, 0, 0,  0, 0, 0, 0,  NT(4,N_A), 0, 0, 0,  NT(4,N_Gs), 0, 0, N_OFF,
    0,     0, 0, 0,  0, 0, 0, 0,  0,         0, 0, 0,  0,          0, 0, 0,
    /* Bars 3-4: Aching Nine Inch Nails Chromatic Melody */
    NT(4,N_A),  0, 0, 0,  0, 0, NT(4,N_As), 0,  NT(4,N_A), 0, 0, 0,  NT(4,N_F), 0, 0, 0,
    NT(4,N_F),  0, 0, 0,  NT(4,N_E), 0, 0, 0,   NT(4,N_Gs),0, 0, 0,  NT(4,N_G), 0, NT(4,N_F), N_OFF,
    /* Bars 5-6: High Octave Tension Climb */
    NT(5,N_D),  0, 0, 0,  0, 0, NT(5,N_F),  0,  NT(5,N_E), 0, 0, 0,  NT(5,N_Cs),0, 0, 0,
    NT(5,N_D),  0, 0, 0,  NT(4,N_As),0, 0, 0,   NT(4,N_Gs),0, NT(4,N_G), 0, NT(4,N_F), 0, N_OFF, 0,
    /* Bars 7-8: Climax Cry & Chromatic Resolution */
    NT(5,N_F),  0, 0, 0,  NT(5,N_E), 0, 0, 0,   NT(5,N_Ds),0, 0, NT(5,N_D), 0, NT(5,N_C), 0, NT(4,N_B),
    NT(4,N_As), 0, 0, 0,  NT(4,N_A), 0, 0, 0,   NT(4,N_Gs),0, 0, 0,  NT(4,N_A), 0, 0, N_OFF
};

static const uint8_t title_drone[128] = {
    NT(1,N_D),  0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,          0,0,0, 0,0,0,0, NT(1,N_Gs),0,0,0, NT(1,N_G),0,0,0,
    NT(1,N_D),  0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    NT(1,N_As), 0,0,0, NT(1,N_A),0,0,0, NT(1,N_Gs),0,0,0, NT(1,N_G),0,0,0,
    NT(1,N_D),  0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,          0,0,0, 0,0,0,0, NT(1,N_Gs),0,0,0, NT(1,N_A),0,0,0,
    NT(1,N_As), 0,0,0, NT(1,N_A),0,0,0, NT(1,N_Gs),0,0,NT(1,N_G), 0,NT(1,N_F),0,0,
    NT(1,N_Ds), 0,0,0, NT(1,N_D),0,0,0, NT(1,N_Cs),0,0,0, NT(1,N_D),0,0,N_OFF
};

/* Repeating 16-step Mechanical Sequencer Arpeggio & Industrial Drum Groove */
static const uint8_t title_seq16[16] = {
    NT(3,N_D), NT(3,N_A), NT(4,N_D), NT(3,N_A),
    NT(3,N_F), NT(3,N_A), NT(4,N_Ds),NT(3,N_A),
    NT(3,N_Gs),NT(3,N_D), NT(4,N_D), NT(3,N_G),
    NT(3,N_F), NT(3,N_A), NT(3,N_Ds),NT(3,N_Cs)
};

static const uint8_t title_kick16[16] = {
    D_KICK, 0, 0, D_KICK,  0, 0, D_KICK, 0,
    0,      0, D_KICK, 0,  0, 0, 0,      D_KICK
};

static const uint8_t title_snare16[16] = {
    0, 0, 0, 0,  D_SNARE, 0, 0, 0,
    0, 0, 0, 0,  D_SNARE, 0, 0, D_CLANK
};

static const uint8_t title_hat16[16] = {
    D_HAT_C, D_HAT_C, D_HAT_O, D_HAT_C,  D_HAT_C, D_HAT_C, D_HAT_O, D_HAT_C,
    D_HAT_C, D_HAT_C, D_HAT_O, D_HAT_C,  D_HAT_C, D_HAT_C, D_HAT_O, D_HAT_C
};

/* =========================================================================
 * TRACK 2: GAMEPLAY — "LEHIGH VALLEY NIGHTFALL"
 * Slow Moody Atmospheric Dark Sci-Fi (~66 BPM = 16 frames/step, 128 steps)
 * Unhurried, brooding, zero hi-hat clutter so footsteps & gunfire shine!
 * ========================================================================= */
static const OplPatch game_patches[9] = {
    /* Ch 0: Deep Warm Sub-Bass Drone */
    { 0x21, 0x1B, 0x74, 0x35, 0x00,  0x21, 0x0B, 0x73, 0x25, 0x00,  0x06,  0, 0 },
    /* Ch 1: Lush Dark Minor String Pad (Low Voice, slow AR=3 swell + vibrato) */
    { 0x61, 0x17, 0x32, 0x23, 0x01,  0x61, 0x0F, 0x32, 0x13, 0x00,  0x06, -2, 0 },
    /* Ch 2: Lush Dark Minor String Pad (Upper Voice, detuned +2 for warm chorus) */
    { 0x62, 0x19, 0x32, 0x23, 0x01,  0x61, 0x10, 0x32, 0x13, 0x02,  0x08,  2, 0 },
    /* Ch 3: Sparse Glassy X-COM Sonar / Crystal Motif */
    { 0x03, 0x17, 0xD6, 0x55, 0x00,  0x01, 0x0D, 0xC5, 0x44, 0x00,  0x04,  0, 11 },
    /* Ch 4: Mournful Dark Synth Cello Swell */
    { 0x61, 0x14, 0x43, 0x24, 0x01,  0x61, 0x0E, 0x42, 0x14, 0x01,  0x0A,  1, 0 },
    /* Ch 5: Quiet Tape-Delay Echo & Subtle Mystery Pulse */
    { 0x02, 0x1B, 0xB7, 0x66, 0x00,  0x01, 0x14, 0xB6, 0x55, 0x00,  0x04,  3, 9 },
    /* Ch 6: Distant Soft Heartbeat Tom */
    { 0x01, 0x16, 0xD9, 0x99, 0x00,  0x01, 0x0D, 0xC8, 0x68, 0x00,  0x04,  0, 0 },
    /* Ch 7: Unused / Silent in Gameplay */
    { 0x01, 0x3F, 0xF8, 0x88, 0x00,  0x01, 0x3F, 0xF8, 0x88, 0x00,  0x00,  0, 0 },
    /* Ch 8: Faint Distant UFO Shimmer (triggered once every 32 steps) */
    { 0x47, 0x16, 0x66, 0x46, 0x02,  0x43, 0x1A, 0x65, 0x45, 0x00,  0x0C,  0, 0 }
};

/* 8 Slow Harmonic Bars (16 steps each = 128 steps):
 * Dm9 -> BbMaj7#11 -> Gm6 -> A7b9 -> Dm/F -> Ebmaj7/G -> Edim7 -> A7sus4->A7 */
static const uint8_t game_bass[128] = {
    NT(2,N_D),  0,0,0, 0,0,0,0,  0,0,NT(2,N_D), 0,0,0,0,0,
    NT(1,N_As), 0,0,0, 0,0,0,0,  0,0,NT(1,N_As),0,0,0,0,0,
    NT(1,N_G),  0,0,0, 0,0,0,0,  0,0,NT(1,N_G), 0,0,0,0,0,
    NT(1,N_A),  0,0,0, 0,0,0,0,  NT(2,N_Cs),0,0,0, 0,0,0,0,

    NT(2,N_D),  0,0,0, 0,0,0,0,  NT(1,N_F), 0,0,0, 0,0,0,0,
    NT(1,N_G),  0,0,0, 0,0,0,0,  NT(1,N_Gs),0,0,0, 0,0,0,0,
    NT(1,N_As), 0,0,0, 0,0,0,0,  NT(1,N_E), 0,0,0, 0,0,0,0,
    NT(1,N_A),  0,0,0, 0,0,0,0,  0,0,0,0,       0,0,0,N_OFF
};

static const uint8_t game_pad1[128] = {
    NT(3,N_D),  0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    NT(3,N_D),  0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    NT(2,N_As), 0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    NT(3,N_Cs), 0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

    NT(3,N_D),  0,0,0, 0,0,0,0, NT(3,N_C), 0,0,0, 0,0,0,0,
    NT(2,N_As), 0,0,0, 0,0,0,0, NT(3,N_C), 0,0,0, 0,0,0,0,
    NT(3,N_D),  0,0,0, 0,0,0,0, NT(2,N_As),0,0,0, 0,0,0,0,
    NT(3,N_D),  0,0,0, 0,0,0,0, NT(3,N_Cs),0,0,0, 0,0,0,N_OFF
};

static const uint8_t game_pad2[128] = {
    NT(3,N_F),  0,0,0, 0,0,0,0, NT(3,N_A), 0,0,0, 0,0,0,0,
    NT(3,N_F),  0,0,0, 0,0,0,0, NT(3,N_E), 0,0,0, 0,0,0,0,
    NT(3,N_D),  0,0,0, 0,0,0,0, NT(3,N_E), 0,0,0, 0,0,0,0,
    NT(3,N_G),  0,0,0, 0,0,0,0, NT(3,N_E), 0,0,0, 0,0,0,0,

    NT(3,N_A),  0,0,0, 0,0,0,0, NT(3,N_F), 0,0,0, 0,0,0,0,
    NT(3,N_D),  0,0,0, 0,0,0,0, NT(3,N_Ds),0,0,0, 0,0,0,0,
    NT(3,N_F),  0,0,0, 0,0,0,0, NT(3,N_G), 0,0,0, 0,0,0,0,
    NT(3,N_E),  0,0,0, 0,0,0,0, NT(3,N_E), 0,0,0, 0,0,0,N_OFF
};

/* Sparse, unhurried glassy motif with generous breathing room */
static const uint8_t game_motif[128] = {
    0, 0, NT(4,N_A), 0,  0, 0, NT(5,N_E), 0,  0, 0, NT(5,N_D), 0,  0, 0, 0, 0,
    0, 0, 0,         0,  NT(5,N_C), 0, 0, 0,  NT(4,N_A), 0, 0, 0,  0, 0, 0, 0,
    0, 0, NT(4,N_As),0,  0, 0, NT(4,N_A), 0,  0, 0, NT(4,N_G), 0,  0, 0, 0, 0,
    0, 0, 0,         0,  NT(4,N_Gs),0, 0, 0,  NT(4,N_A), 0, 0, 0,  0, 0, 0, 0,

    0, 0, NT(5,N_F), 0,  0, 0, NT(5,N_E), 0,  0, 0, NT(5,N_D), 0,  NT(4,N_A), 0, 0, 0,
    0, 0, 0,         0,  NT(5,N_C), 0, 0, 0,  NT(4,N_As),0, 0, 0,  0, 0, 0, 0,
    0, 0, NT(4,N_G), 0,  0, 0, NT(4,N_As),0,  0, 0, NT(4,N_A), 0,  0, 0, 0, 0,
    0, 0, NT(4,N_E), 0,  0, 0, 0,         0,  0, 0, 0,         0,  0, 0, 0, 0
};

/* Mournful Synth Cello Counter-Melody in Second Half (Steps 64..127) */
static const uint8_t game_cello[128] = {
    N_OFF,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,    0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,    0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,    0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

    NT(2,N_A),  0,0,0, 0,0,0,0,  NT(3,N_C), 0,0,0, 0,0,0,0,
    NT(2,N_As), 0,0,0, 0,0,0,0,  NT(2,N_Gs),0,0,0, 0,0,0,0,
    NT(2,N_G),  0,0,0, 0,0,0,0,  NT(2,N_F), 0,0,0, 0,0,0,0,
    NT(2,N_E),  0,0,0, 0,0,0,0,  NT(2,N_A), 0,0,0, 0,0,0,N_OFF
};

static void handle_melodic_event(int ch, uint8_t ev) {
    if (ev == N_OFF) {
        opl_key_off(ch);
    } else if (ev >= 12) {
        opl_note_on(ch, ev);
    }
}

void music_init(void) {
    uint8_t st1, st2;
    int r;

    opl_active = 0;
    cur_track  = MUSIC_TRACK_NONE;
    memset(ch_reg_b0, 0, sizeof(ch_reg_b0));
    memset(ch_gate_rem, 0, sizeof(ch_gate_rem));

    /* Standard YM3812 Hardware Timer 1 Detection */
    opl_write(0x04, 0x60);
    opl_write(0x04, 0x80);
    st1 = inportb(OPL_ADDR);
    opl_write(0x02, 0xFF);
    opl_write(0x04, 0x21);
    delay(2);
    st2 = inportb(OPL_ADDR);
    opl_write(0x04, 0x60);
    opl_write(0x04, 0x80);

    if (((st1 & 0xE0U) == 0x00U && (st2 & 0xE0U) == 0xC0U) || sound_is_sb_active()) {
        opl_active = 1;
    }

    if (!opl_active) return;

    for (r = 0x01; r <= 0xF5; r++) {
        opl_write((uint8_t)r, 0x00);
    }
    /* Enable YM3812 Extended Waveforms (bit 5 of 0x01) + Deep Vibrato/Tremolo (0xBD) */
    opl_write(0x01, 0x20);
    opl_write(0xBD, 0xC0);
    silence_all_channels();
}

void music_shutdown(void) {
    int r;
    if (opl_active) {
        silence_all_channels();
        for (r = 0x01; r <= 0xF5; r++) {
            opl_write((uint8_t)r, 0x00);
        }
        opl_active = 0;
    }
    cur_track = MUSIC_TRACK_NONE;
}

int music_is_opl_active(void) {
    return opl_active;
}

void music_set_volume(int vol_level) {
    int ch;
    if (vol_level < 0) vol_level = 0;
    if (vol_level > 3) vol_level = 3;
    cur_vol = vol_level;

    if (!opl_active) return;
    if (cur_vol == 0) {
        silence_all_channels();
        return;
    }
    if (cur_track != MUSIC_TRACK_NONE) {
        for (ch = 0; ch < 9; ch++) {
            apply_channel_volume(ch);
        }
    }
}

void music_play_track(int track_id) {
    int ch;
    if (track_id == cur_track) return;

    cur_track     = track_id;
    cur_step      = 0;
    frame_in_step = 0;

    if (!opl_active) return;
    silence_all_channels();

    if (track_id == MUSIC_TRACK_TITLE) {
        for (ch = 0; ch < 9; ch++) opl_load_patch(ch, &title_patches[ch]);
    } else if (track_id == MUSIC_TRACK_GAME) {
        for (ch = 0; ch < 9; ch++) opl_load_patch(ch, &game_patches[ch]);
    }
}

void music_tick(void) {
    int step_frames;
    int ch;

    if (!opl_active || cur_track == MUSIC_TRACK_NONE || cur_vol <= 0) return;

    step_frames = (cur_track == MUSIC_TRACK_TITLE) ? 10 : 16;

    if (frame_in_step == 0) {
        int s = cur_step & 127;

        if (cur_track == MUSIC_TRACK_TITLE) {
            uint8_t k_ev = title_kick16[s & 15];
            uint8_t s_ev = title_snare16[s & 15];
            uint8_t h_ev = title_hat16[s & 15];

            handle_melodic_event(0, title_bass[s]);
            handle_melodic_event(1, title_gtr1[s]);
            handle_melodic_event(2, title_gtr2[s]);
            handle_melodic_event(3, title_lead[s]);
            handle_melodic_event(4, title_drone[s]);
            handle_melodic_event(5, title_seq16[s & 15]);

            if (s == 62 || s == 126) k_ev = D_KICK;
            if ((s & 63) == 63)      s_ev = D_SNARE;

            if (k_ev) { kick_type = k_ev; kick_age = 0; }
            if (s_ev) { snare_type = s_ev; snare_age = 0; }
            if (h_ev) { hat_limit = (h_ev == D_HAT_O) ? 7 : 3; hat_age = 0; }
        } else if (cur_track == MUSIC_TRACK_GAME) {
            uint8_t echo_ev = game_motif[(s + 128 - 3) & 127];

            handle_melodic_event(0, game_bass[s]);
            handle_melodic_event(1, game_pad1[s]);
            handle_melodic_event(2, game_pad2[s]);
            handle_melodic_event(3, game_motif[s]);
            handle_melodic_event(4, game_cello[s]);

            if (echo_ev >= 12) {
                handle_melodic_event(5, echo_ev);
            } else if ((s & 7) == 4) {
                handle_melodic_event(5, ((s & 32) == 0) ? NT(3, N_A) : NT(3, N_E));
            }

            if ((s & 15) == 0 || (s & 15) == 10) {
                kick_type = D_TOM;
                kick_age  = 0;
            }
            if ((s & 31) == 16) {
                opl_note_on(8, NT(5, N_A));
            } else if ((s & 31) == 24) {
                opl_key_off(8);
            }
        }
    } else {
        /* Process automatic gate note-offs on melodic channels 0..5 */
        for (ch = 0; ch < 6; ch++) {
            if (ch_gate_rem[ch] > 0) {
                ch_gate_rem[ch]--;
                if (ch_gate_rem[ch] == 0) {
                    opl_key_off(ch);
                }
            }
        }
    }

    /* 70Hz Per-Frame Drum Synthesis: Analog Pitch-Drop Kick & Metallic FM Noise Snare/Hat */
    if (kick_age >= 0) {
        if (kick_type == D_KICK) {
            static const int k_blk[5]  = { 3,   2,   2,   2,   1   };
            static const int k_fnum[5] = { 410, 520, 370, 280, 450 };
            if (kick_age == 0) opl_key_off(6);
            if (kick_age < 5) {
                opl_set_raw_freq(6, k_blk[kick_age], k_fnum[kick_age], 1);
                kick_age++;
            } else {
                opl_key_off(6);
                kick_age = -1;
            }
        } else {
            static const int t_blk[4]  = { 2,   2,   1,   1   };
            static const int t_fnum[4] = { 440, 340, 540, 440 };
            if (kick_age == 0) opl_key_off(6);
            if (kick_age < 4) {
                opl_set_raw_freq(6, t_blk[kick_age], t_fnum[kick_age], 1);
                kick_age++;
            } else {
                opl_key_off(6);
                kick_age = -1;
            }
        }
    }

    if (snare_age >= 0) {
        drm_rng = drm_rng * 1664525U + 1013904223U;
        if (snare_age == 0) {
            opl_key_off(7);
            opl_set_raw_freq(7, 3, (snare_type == D_CLANK) ? 620 : 500, 1);
            snare_age++;
        } else if (snare_age < 7) {
            int blk  = (snare_type == D_CLANK) ? 5 : (5 + (int)((drm_rng >> 24) & 1U));
            int fnum = 380 + (int)((drm_rng >> 15) & 0x1FFU);
            opl_set_raw_freq(7, blk, fnum, 1);
            snare_age++;
        } else {
            opl_key_off(7);
            snare_age = -1;
        }
    }

    if (hat_age >= 0 && cur_track == MUSIC_TRACK_TITLE) {
        drm_rng = drm_rng * 1664525U + 1013904223U;
        if (hat_age == 0) opl_key_off(8);
        if (hat_age < hat_limit) {
            int fnum = 460 + (int)((drm_rng >> 14) & 0x1FFU);
            opl_set_raw_freq(8, 6, fnum, 1);
            hat_age++;
        } else {
            opl_key_off(8);
            hat_age = -1;
        }
    }

    frame_in_step++;
    if (frame_in_step >= step_frames) {
        frame_in_step = 0;
        cur_step = (cur_step + 1) & 127;
    }
}
