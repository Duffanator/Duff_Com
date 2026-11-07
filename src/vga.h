#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 200
#define VIEWPORT_H 144
#define HUD_Y 144
#define HUD_H 56

/* 16 Color Ramps x 16 Shades (0 = brightest, 15 = darkest) */
#define RAMP_GRAY    0   /* 0..15:    Metals, concrete, asphalt, steel UI */
#define RAMP_GREEN   1   /* 16..31:   PA grass, turf, foliage, TU bar */
#define RAMP_BRICK   2   /* 32..47:   Coopersburg brick, barn red, health bar */
#define RAMP_WOOD    3   /* 48..63:   Barn wood, dirt, diner booths, skin tones */
#define RAMP_BLUE    4   /* 64..79:   DUFF-COM tactical armor, sky, water */
#define RAMP_CYAN    5   /* 80..95:   Diner neon, arcade screens, laser/plasma */
#define RAMP_GOLD    6   /* 96..111:  Cornstalks, highway lines, cursor box, brass */
#define RAMP_ORANGE  7   /* 112..127: Explosions, fire, stamina bar, streetlights */
#define RAMP_PURPLE  8   /* 128..143: Alien alloys, arcade carpet, morale bar */
#define RAMP_ALIEN   9   /* 144..159: Sectoid skin, Elerium glow, bio-slime */

#define COL_TRANS    255 /* Transparent color index for sprites */
#define COL_BLACK    15
#define COL_WHITE    0
#define COL_YELLOW   96
#define COL_RED      33
#define COL_GREEN    17
#define COL_CYAN     81

extern uint8_t vga_backbuf[SCREEN_W * SCREEN_H];
extern uint8_t vga_palette[256][3]; /* 6-bit RGB (0..63) */
extern uint8_t vga_shade_table[16][256]; /* Light level 0(bright)..15(pitch black) */

void vga_init(void);
void vga_shutdown(void);
void vga_build_palette(void);
void vga_apply_palette(void);
void vga_vsync(void);
void vga_flip(void);

void vga_clear(uint8_t col);
void vga_pset(int x, int y, uint8_t col);
void vga_pset_clip(int x, int y, uint8_t col, int max_y);
void vga_hline(int x1, int x2, int y, uint8_t col);
void vga_vline(int x, int y1, int y2, uint8_t col);
void vga_line(int x0, int y0, int x1, int y1, uint8_t col);
void vga_line_clip(int x0, int y0, int x1, int y1, uint8_t col, int max_y);
void vga_rect_fill(int x, int y, int w, int h, uint8_t col);
void vga_rect_bevel(int x, int y, int w, int h, uint8_t fill_col, uint8_t hi_col, uint8_t lo_col);

/* Blit sprite with transparency (COL_TRANS) and light-level shading (0=brightest, 15=darkest) */
void vga_blit_shaded(int dx, int dy, int w, int h, const uint8_t *src, int shade, int max_y);
void vga_blit_ui(int dx, int dy, int w, int h, const uint8_t *src);

/* Compact 5x7 & 6x8 bitmap font rendering */
void vga_print(int x, int y, const char *str, uint8_t col);
void vga_print_shadow(int x, int y, const char *str, uint8_t col, uint8_t shadow_col);
int  vga_text_width(const char *str);

/* Save current backbuffer to a 24-bit BMP file for verification or screenshots */
int  vga_save_bmp(const char *filename);

#endif /* VGA_H */
