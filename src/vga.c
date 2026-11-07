#include "vga.h"
#include "dos_compat.h"

uint8_t vga_backbuf[SCREEN_W * SCREEN_H];
uint8_t vga_palette[256][3];
uint8_t vga_shade_table[16][256];

/* Built-in 5x7 uppercase/lowercase/digits/symbols pixel font (ASCII 32..95) */
static const uint8_t font5x7[64][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x14,0x08,0x3E,0x08,0x14}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* backslash */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}  /* '_' */
};

static void make_ramp(int ramp_idx, int r0, int g0, int b0, int r1, int g1, int b1) {
    int i;
    for (i = 0; i < 16; i++) {
        int idx = ramp_idx * 16 + i;
        vga_palette[idx][0] = (uint8_t)(r0 + ((r1 - r0) * i) / 15);
        vga_palette[idx][1] = (uint8_t)(g0 + ((g1 - g0) * i) / 15);
        vga_palette[idx][2] = (uint8_t)(b0 + ((b1 - b0) * i) / 15);
    }
}

void vga_build_palette(void) {
    int s, c, ramp, off, new_off;
    memset(vga_palette, 0, sizeof(vga_palette));

    /* 10 primary 16-shade ramps (0 = brightest highlight, 15 = darkest shadow) */
    make_ramp(RAMP_GRAY,   63, 63, 63,   0,  0,  0); /* 0..15:   White -> Steel -> Pitch Black */
    make_ramp(RAMP_GREEN,  30, 62, 26,   3, 12,  3); /* 16..31:  Bright Turf -> Deep Forest */
    make_ramp(RAMP_BRICK,  63, 18, 16,  12,  2,  2); /* 32..47:  Bright Red -> Coopersburg Brick */
    make_ramp(RAMP_WOOD,   58, 44, 28,  12,  7,  3); /* 48..63:  Tan/Skin -> Barn Wood -> Dark Mud */
    make_ramp(RAMP_BLUE,   28, 44, 63,   3,  6, 15); /* 64..79:  DUFF-COM Blue -> Midnight Blue */
    make_ramp(RAMP_CYAN,   24, 63, 63,   2, 14, 16); /* 80..95:  Neon Cyan -> Deep Teal */
    make_ramp(RAMP_GOLD,   63, 58, 14,  15, 11,  2); /* 96..111: Bright Gold -> Dark Ochre */
    make_ramp(RAMP_ORANGE, 63, 38,  8,  16,  5,  1); /* 112..127:Flame Orange -> Burnt Ember */
    make_ramp(RAMP_PURPLE, 52, 26, 62,   9,  3, 14); /* 128..143:Arcade Neon Purple -> Dark Indigo */
    make_ramp(RAMP_ALIEN,  42, 63, 12,   6, 14,  2); /* 144..159:Elerium Lime -> Dark Slime */

    /* Fill remaining ramps 10..15 with useful metallic/olive/slate gradients */
    make_ramp(10, 50, 54, 60,  8, 10, 14); /* Cold Steel UI ramp */
    make_ramp(11, 46, 50, 34,  8, 10,  6); /* Olive Drab military ramp */
    make_ramp(12, 60, 52, 42, 12, 10,  8); /* Limestone / Concrete sidewalk ramp */
    make_ramp(13, 63, 24, 52, 14,  3, 10); /* Diner Neon Magenta */
    make_ramp(14, 38, 38, 46,  5,  5,  8); /* Route 309 Asphalt ramp */
    make_ramp(15, 63, 49, 40, 14,  7,  5); /* Warm Portrait Flesh & Skin-Tone ramp */

    /* Build 16-level light shading lookup table for instant fog-of-war & night lighting */
    for (s = 0; s < 16; s++) {
        for (c = 0; c < 256; c++) {
            if (c == COL_TRANS) {
                vga_shade_table[s][c] = COL_TRANS;
                continue;
            }
            ramp = c >> 4;
            off  = c & 15;
            new_off = off + s;
            if (new_off >= 15) {
                /* Fade completely into near-black at the bottom of the ramp or pitch black */
                vga_shade_table[s][c] = (s >= 14) ? COL_BLACK : (uint8_t)((ramp << 4) | 15);
            } else {
                vga_shade_table[s][c] = (uint8_t)((ramp << 4) | new_off);
            }
        }
    }
}

void vga_apply_palette(void) {
    int i;
    outportb(0x3C8, 0);
    for (i = 0; i < 256; i++) {
        outportb(0x3C9, vga_palette[i][0]);
        outportb(0x3C9, vga_palette[i][1]);
        outportb(0x3C9, vga_palette[i][2]);
    }
}

void vga_init(void) {
    __dpmi_regs r;
    memset(&r, 0, sizeof(r));
    r.x.ax = 0x0013; /* VGA 320x200x256 Mode 13h */
    __dpmi_int(0x10, &r);

    vga_build_palette();
    vga_apply_palette();
    memset(vga_backbuf, COL_BLACK, sizeof(vga_backbuf));
}

void vga_shutdown(void) {
    __dpmi_regs r;
    memset(&r, 0, sizeof(r));
    r.x.ax = 0x0003; /* Standard 80x25 Text Mode */
    __dpmi_int(0x10, &r);
}

void vga_vsync(void) {
    while (inportb(0x3DA) & 8) {}
    while (!(inportb(0x3DA) & 8)) {}
}

void vga_flip(void) {
    dosmemput(vga_backbuf, SCREEN_W * SCREEN_H, 0xA0000);
}

void vga_clear(uint8_t col) {
    memset(vga_backbuf, col, sizeof(vga_backbuf));
}

void vga_pset(int x, int y, uint8_t col) {
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) {
        vga_backbuf[y * SCREEN_W + x] = col;
    }
}

void vga_pset_clip(int x, int y, uint8_t col, int max_y) {
    if ((unsigned)x < SCREEN_W && y >= 0 && y < max_y) {
        vga_backbuf[y * SCREEN_W + x] = col;
    }
}

void vga_hline(int x1, int x2, int y, uint8_t col) {
    if (y < 0 || y >= SCREEN_H) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x2 < 0 || x1 >= SCREEN_W) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= SCREEN_W) x2 = SCREEN_W - 1;
    memset(&vga_backbuf[y * SCREEN_W + x1], col, (size_t)(x2 - x1 + 1));
}

void vga_vline(int x, int y1, int y2, uint8_t col) {
    int y;
    if (x < 0 || x >= SCREEN_W) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (y2 < 0 || y1 >= SCREEN_H) return;
    if (y1 < 0) y1 = 0;
    if (y2 >= SCREEN_H) y2 = SCREEN_H - 1;
    for (y = y1; y <= y2; y++) {
        vga_backbuf[y * SCREEN_W + x] = col;
    }
}

void vga_line_clip(int x0, int y0, int x1, int y1, uint8_t col, int max_y) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        if ((unsigned)x0 < SCREEN_W && y0 >= 0 && y0 < max_y) {
            vga_backbuf[y0 * SCREEN_W + x0] = col;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void vga_line(int x0, int y0, int x1, int y1, uint8_t col) {
    vga_line_clip(x0, y0, x1, y1, col, SCREEN_H);
}

void vga_rect_fill(int x, int y, int w, int h, uint8_t col) {
    int ry;
    for (ry = y; ry < y + h; ry++) {
        vga_hline(x, x + w - 1, ry, col);
    }
}

void vga_rect_bevel(int x, int y, int w, int h, uint8_t fill_col, uint8_t hi_col, uint8_t lo_col) {
    vga_rect_fill(x, y, w, h, fill_col);
    vga_hline(x, x + w - 1, y, hi_col);
    vga_vline(x, y, y + h - 1, hi_col);
    vga_hline(x, x + w - 1, y + h - 1, lo_col);
    vga_vline(x + w - 1, y, y + h - 1, lo_col);
}

void vga_blit_shaded(int dx, int dy, int w, int h, const uint8_t *src, int shade, int max_y) {
    int x, y;
    const uint8_t *lut;
    if (shade < 0) shade = 0;
    if (shade > 15) shade = 15;
    lut = vga_shade_table[shade];

    for (y = 0; y < h; y++) {
        int sy = dy + y;
        const uint8_t *row_src;
        uint8_t *row_dst;
        if (sy < 0) continue;
        if (sy >= max_y) break;
        row_src = src + y * w;
        row_dst = vga_backbuf + sy * SCREEN_W;
        for (x = 0; x < w; x++) {
            int sx = dx + x;
            uint8_t c;
            if ((unsigned)sx >= SCREEN_W) continue;
            c = row_src[x];
            if (c != COL_TRANS) {
                row_dst[sx] = lut[c];
            }
        }
    }
}

void vga_blit_ui(int dx, int dy, int w, int h, const uint8_t *src) {
    vga_blit_shaded(dx, dy, w, h, src, 0, SCREEN_H);
}

void vga_print(int x, int y, const char *str, uint8_t col) {
    int cx = x;
    while (*str) {
        char ch = *str++;
        int idx, col_i, row_i;
        if (ch >= 'a' && ch <= 'z') ch -= 32; /* Uppercase for crisp 5x7 tactical look */
        if (ch < 32 || ch > 95) ch = '?';
        idx = ch - 32;
        for (col_i = 0; col_i < 5; col_i++) {
            uint8_t bits = font5x7[idx][col_i];
            for (row_i = 0; row_i < 7; row_i++) {
                if (bits & (1 << row_i)) {
                    vga_pset(cx + col_i, y + row_i, col);
                }
            }
        }
        cx += 6;
    }
}

void vga_print_shadow(int x, int y, const char *str, uint8_t col, uint8_t shadow_col) {
    vga_print(x + 1, y + 1, str, shadow_col);
    vga_print(x, y, str, col);
}

int vga_text_width(const char *str) {
    return (int)strlen(str) * 6;
}

int vga_save_bmp(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    uint8_t hdr[54];
    uint32_t file_size = 54 + (SCREEN_W * SCREEN_H * 3);
    uint32_t data_offset = 54;
    uint32_t dib_size = 40;
    int32_t width = SCREEN_W;
    int32_t height = SCREEN_H;
    uint16_t planes = 1;
    uint16_t bpp = 24;
    uint32_t img_size = SCREEN_W * SCREEN_H * 3;
    int x, y;

    if (!fp) return -1;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(&hdr[2],  &file_size, 4);
    memcpy(&hdr[10], &data_offset, 4);
    memcpy(&hdr[14], &dib_size, 4);
    memcpy(&hdr[18], &width, 4);
    memcpy(&hdr[22], &height, 4);
    memcpy(&hdr[26], &planes, 2);
    memcpy(&hdr[28], &bpp, 2);
    memcpy(&hdr[34], &img_size, 4);
    fwrite(hdr, 1, 54, fp);

    /* BMP rows are stored bottom-to-top, BGR 8-bit per channel */
    for (y = SCREEN_H - 1; y >= 0; y--) {
        for (x = 0; x < SCREEN_W; x++) {
            uint8_t idx = vga_backbuf[y * SCREEN_W + x];
            uint8_t bgr[3];
            bgr[0] = (uint8_t)((vga_palette[idx][2] * 255) / 63);
            bgr[1] = (uint8_t)((vga_palette[idx][1] * 255) / 63);
            bgr[2] = (uint8_t)((vga_palette[idx][0] * 255) / 63);
            fwrite(bgr, 1, 3, fp);
        }
    }
    fclose(fp);
    return 0;
}
