#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include "sound.h"

/* Extended key codes from BIOS INT 16h / getch */
#define KEY_ESC    27
#define KEY_ENTER  13
#define KEY_TAB    9
#define KEY_SPACE  32
#define KEY_UP     0x148
#define KEY_DOWN   0x150
#define KEY_LEFT   0x14B
#define KEY_RIGHT  0x14D
#define KEY_PGUP   0x149
#define KEY_PGDN   0x151

typedef struct {
    int mouse_present;
    int mx, my;          /* Trackball / mouse screen coordinates (0..319, 0..199) */
    int prev_mx, prev_my;
    int buttons;         /* Current button bitmask: 1=Left, 2=Right, 4=Middle */
    int prev_buttons;
    int left_clicked;    /* Rising edge this frame */
    int right_clicked;   /* Rising edge this frame */
    int left_held;
    int right_held;
    int last_key;        /* Last keyboard/arcade button keycode pressed this frame (0 if none) */
} InputState;

extern InputState g_input;

enum {
    CURSOR_ARROW           = 0,
    CURSOR_RETICLE_CLEAR   = 1,
    CURSOR_RETICLE_BLOCKED = 2
};

void input_init(void);
void input_poll(void);
void input_flush(void);
void input_draw_cursor(void);
void input_draw_cursor_ex(int cursor_mode);

#endif /* INPUT_H */
