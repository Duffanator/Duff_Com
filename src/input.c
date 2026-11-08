#include "input.h"
#include "vga.h"
#include "gfx_assets.h"
#include "dos_compat.h"

InputState g_input;

void input_init(void) {
    __dpmi_regs r;
    memset(&g_input, 0, sizeof(g_input));
    g_input.mx = SCREEN_W / 2;
    g_input.my = VIEWPORT_H / 2;

    /* Reset DOS Mouse Driver (INT 33h, AX=0000h) */
    memset(&r, 0, sizeof(r));
    r.x.ax = 0x0000;
    __dpmi_int(0x33, &r);
    g_input.mouse_present = (r.x.ax == 0xFFFF);

    if (g_input.mouse_present) {
        /* Set Horizontal Range (AX=0007h): 0..639 (DOS mouse driver uses 640 virtual width in Mode 13h) */
        memset(&r, 0, sizeof(r));
        r.x.ax = 0x0007;
        r.x.cx = 0;
        r.x.dx = (SCREEN_W * 2) - 1;
        __dpmi_int(0x33, &r);

        /* Set Vertical Range (AX=0008h): 0..199 */
        memset(&r, 0, sizeof(r));
        r.x.ax = 0x0008;
        r.x.cx = 0;
        r.x.dx = SCREEN_H - 1;
        __dpmi_int(0x33, &r);

        /* Center initial trackball position (AX=0004h) */
        memset(&r, 0, sizeof(r));
        r.x.ax = 0x0004;
        r.x.cx = SCREEN_W; /* 320 in 640-space = 160 */
        r.x.dx = VIEWPORT_H / 2;
        __dpmi_int(0x33, &r);
    }
}

void input_poll(void) {
    __dpmi_regs r;
    g_input.prev_mx = g_input.mx;
    g_input.prev_my = g_input.my;
    g_input.prev_buttons = g_input.buttons;
    g_input.last_key = 0;

    if (g_input.mouse_present) {
        memset(&r, 0, sizeof(r));
        r.x.ax = 0x0003;
        __dpmi_int(0x33, &r);
        g_input.buttons = r.x.bx & 0x07;
        g_input.mx = r.x.cx / 2; /* Convert 0..639 to 0..319 */
        g_input.my = r.x.dx;
        if (g_input.mx < 0) g_input.mx = 0;
        if (g_input.mx >= SCREEN_W) g_input.mx = SCREEN_W - 1;
        if (g_input.my < 0) g_input.my = 0;
        if (g_input.my >= SCREEN_H) g_input.my = SCREEN_H - 1;
    }

    g_input.left_held     = (g_input.buttons & 1) != 0;
    g_input.right_held    = (g_input.buttons & 2) != 0;
    g_input.left_clicked  = ((g_input.buttons & 1) && !(g_input.prev_buttons & 1));
    g_input.right_clicked = ((g_input.buttons & 2) && !(g_input.prev_buttons & 2));

#ifdef __DJGPP__
    if (kbhit()) {
        int ch = getch();
        if (ch == 0 || ch == 0xE0) {
            ch = 0x100 | getch();
        }
        g_input.last_key = ch;
    }
#endif
}

void input_flush(void) {
#ifdef __DJGPP__
    while (kbhit()) {
        int ch = getch();
        if (ch == 0 || ch == 0xE0) {
            if (kbhit()) (void)getch();
        }
    }
#endif
    input_poll();
    g_input.left_clicked  = 0;
    g_input.right_clicked = 0;
    g_input.last_key      = 0;
    g_input.prev_buttons  = g_input.buttons;
}

/* Classic X-COM angled tactical pointer with high-contrast outline */
static const uint8_t cursor_bmp[9 * 9] = {
    96, 96, 15,255,255,255,255,255,255,
    96,  0, 96, 15,255,255,255,255,255,
    96,  0,  0, 96, 15,255,255,255,255,
    96,  0,  0,  0, 96, 15,255,255,255,
    96,  0,  0,  0,  0, 96, 15,255,255,
    96,  0,  0, 96, 96, 15,255,255,255,
    96, 96, 15, 96,  0, 96, 15,255,255,
    15,255,255, 15, 96,  0, 96, 15,255,
   255,255,255,255, 15, 96, 15,255,255
};

void input_draw_cursor_ex(int cursor_mode) {
    if (cursor_mode == CURSOR_RETICLE_CLEAR) {
        vga_blit_ui(g_input.mx - (FX_W / 2), g_input.my - (FX_H / 2),
                    FX_W, FX_H, spr_reticle[0]);
    } else if (cursor_mode == CURSOR_RETICLE_BLOCKED) {
        vga_blit_ui(g_input.mx - (FX_W / 2), g_input.my - (FX_H / 2),
                    FX_W, FX_H, spr_reticle[1]);
    } else {
        vga_blit_ui(g_input.mx, g_input.my, 9, 9, cursor_bmp);
    }
}

void input_draw_cursor(void) {
    input_draw_cursor_ex(CURSOR_ARROW);
}
