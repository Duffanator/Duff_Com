#include "settings.h"
#include "vga.h"
#include "input.h"
#include "sound.h"
#include "music.h"
#include <stdio.h>
#include <string.h>

GameSettings g_settings;
static int opt_sel_row = 0; /* Keyboard focus row (0..4) */
static int opt_sel_btn = 0; /* Bottom row focus: 0=RESUME, 1=ABORT/QUIT */

void settings_init(void) {
    FILE *fp;
    char line[64];

    /* Calibrated defaults: MEDIUM animation speed out-of-the-box! */
    g_settings.anim_speed   = ANIM_MEDIUM;
    g_settings.music_vol    = 2;
    g_settings.sfx_enabled  = 1;
    g_settings.scroll_speed = 1;
    g_settings.menu_open    = 0;

    fp = fopen("SETTINGS.CFG", "r");
    if (!fp) fp = fopen("assets/SETTINGS.CFG", "r");
    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        int val = 0;
        if (sscanf(line, "ANIM_SPEED=%d", &val) == 1) {
            if (val >= 0 && val <= 2) g_settings.anim_speed = val;
        } else if (sscanf(line, "MUSIC_VOL=%d", &val) == 1) {
            if (val >= 0 && val <= 3) g_settings.music_vol = val;
        } else if (sscanf(line, "SFX=%d", &val) == 1) {
            g_settings.sfx_enabled = val ? 1 : 0;
        } else if (sscanf(line, "SCROLL_SPEED=%d", &val) == 1) {
            if (val >= 0 && val <= 2) g_settings.scroll_speed = val;
        }
    }
    fclose(fp);
}

void settings_save(void) {
    FILE *fp = fopen("SETTINGS.CFG", "w");
    if (!fp) return;
    fprintf(fp, "# DUFF-COM User Preferences\n");
    fprintf(fp, "ANIM_SPEED=%d\n",   g_settings.anim_speed);
    fprintf(fp, "MUSIC_VOL=%d\n",    g_settings.music_vol);
    fprintf(fp, "SFX=%d\n",          g_settings.sfx_enabled);
    fprintf(fp, "SCROLL_SPEED=%d\n", g_settings.scroll_speed);
    fclose(fp);
}

void game_wait_frames(int frames) {
    int i;
    if (frames < 1) frames = 1;
    for (i = 0; i < frames; i++) {
        vga_vsync();
        music_tick();
    }
}

int settings_walk_frames(void) {
    static const int tbl[3] = { 22, 13, 6 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_bolt_frames(void) {
    static const int tbl[3] = { 6, 3, 1 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_muzzle_frames(void) {
    static const int tbl[3] = { 9, 5, 2 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_impact_step_frames(void) {
    static const int tbl[3] = { 5, 3, 2 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_burst_gap_frames(void) {
    static const int tbl[3] = { 14, 8, 4 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_grenade_step_frames(void) {
    static const int tbl[3] = { 6, 3, 2 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_explode_step_frames(void) {
    static const int tbl[3] = { 9, 5, 3 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_death_step_frames(void) {
    static const int tbl[3] = { 16, 12, 8 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_alien_step_frames(void) {
    static const int tbl[3] = { 22, 13, 6 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_alien_post_shot_frames(void) {
    static const int tbl[3] = { 36, 20, 10 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_hidden_banner_frames(void) {
    static const int tbl[3] = { 70, 50, 32 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_turn_splash_frames(void) {
    static const int tbl[3] = { 210, 168, 120 };
    return tbl[g_settings.anim_speed % 3];
}

int settings_scroll_px(void) {
    static const int tbl[3] = { 2, 4, 7 };
    return tbl[g_settings.scroll_speed % 3];
}

static void draw_opt_pill(int x, int y, int w, int h, const char *txt, int selected, int danger) {
    int hov = (g_input.mx >= x && g_input.mx < x + w &&
               g_input.my >= y && g_input.my < y + h);
    uint8_t bg  = selected ? (danger ? 35 : 65) : (hov ? (danger ? 37 : 68) : (danger ? 41 : 75));
    uint8_t hi  = (selected || hov) ? 96 : (danger ? 34 : 163);
    uint8_t fg  = selected ? 96 : (hov ? 0 : 2);
    int tw = (int)strlen(txt) * 6;
    int tx = x + (w - tw) / 2;
    int ty = y + (h - 7) / 2;

    vga_rect_bevel(x, y, w, h, bg, hi, 79);
    vga_print_shadow(tx, ty, txt, fg, 15);
}

void settings_render_modal(int in_battlescape) {
    /* Centered 1994 X-COM Tactical Options Modal (264x172) */
    vga_rect_bevel(28, 14, 264, 172, 74, 96, 79);
    vga_rect_fill(32, 18, 256, 164, 14);

    /* Title Bar & Audio Hardware Indicator */
    vga_print_shadow(38, 23, "DUFF-COM GAME OPTIONS", 96, 15);
    vga_print(218, 23,
              (music_is_opl_active() && sound_is_sb_active()) ? "OPL2+SB OK" :
              music_is_opl_active() ? "OPL2 FM OK" : "AUDIO STD",
              18);
    vga_hline(36, 284, 34, 66);

    /* Row 0: ANIMATION SPEED (SLOW / MEDIUM / FAST) */
    vga_print_shadow(38, 46, "ANIM SPEED:", (opt_sel_row == 0) ? 96 : 80, 15);
    draw_opt_pill(110, 40, 54, 20, "SLOW",   g_settings.anim_speed == ANIM_SLOW,   0);
    draw_opt_pill(168, 40, 58, 20, "MEDIUM", g_settings.anim_speed == ANIM_MEDIUM, 0);
    draw_opt_pill(230, 40, 52, 20, "FAST",   g_settings.anim_speed == ANIM_FAST,   0);

    /* Row 1: MUSIC VOLUME (OFF / LOW / MED / HIGH) */
    vga_print_shadow(38, 71, "MUSIC VOL:", (opt_sel_row == 1) ? 96 : 80, 15);
    draw_opt_pill(110, 65, 40, 20, "OFF",  g_settings.music_vol == 0, 0);
    draw_opt_pill(154, 65, 40, 20, "LOW",  g_settings.music_vol == 1, 0);
    draw_opt_pill(198, 65, 40, 20, "MED",  g_settings.music_vol == 2, 0);
    draw_opt_pill(242, 65, 40, 20, "HIGH", g_settings.music_vol == 3, 0);

    /* Row 2: DIGITAL SOUND FX (OFF / ON) */
    vga_print_shadow(38, 96, "SOUND FX:", (opt_sel_row == 2) ? 96 : 80, 15);
    draw_opt_pill(110, 90, 84, 20, "OFF", g_settings.sfx_enabled == 0, 0);
    draw_opt_pill(198, 90, 84, 20, "ON",  g_settings.sfx_enabled == 1, 0);

    /* Row 3: TRACKBALL EDGE SCROLL SPEED (SLOW / NORM / FAST) */
    vga_print_shadow(38, 121, "MAP SCROLL:", (opt_sel_row == 3) ? 96 : 80, 15);
    draw_opt_pill(110, 115, 54, 20, "SLOW", g_settings.scroll_speed == 0, 0);
    draw_opt_pill(168, 115, 58, 20, "NORM", g_settings.scroll_speed == 1, 0);
    draw_opt_pill(230, 115, 52, 20, "FAST", g_settings.scroll_speed == 2, 0);

    vga_hline(36, 284, 141, 66);

    /* Row 4: RESUME GAME & ABORT MISSION / QUIT TO DOS */
    draw_opt_pill(38,  147, 118, 24, "RESUME GAME",
                  (opt_sel_row == 4 && opt_sel_btn == 0), 0);
    draw_opt_pill(164, 147, 118, 24,
                  in_battlescape ? "ABORT TO HQ" : "QUIT TO DOS",
                  (opt_sel_row == 4 && opt_sel_btn == 1), 1);
}

int settings_handle_input(int in_battlescape) {
    int mx = g_input.mx;
    int my = g_input.my;
    int key = g_input.last_key;

    /* Right-click or ESC / O closes Options and resumes immediately */
    if (g_input.right_clicked || key == KEY_ESC || key == 'o' || key == 'O') {
        g_settings.menu_open = 0;
        sfx_click();
        return 0;
    }

    /* Keyboard navigation for Arcade Cabinet buttons / Arrow keys */
    if (key == KEY_UP) {
        opt_sel_row = (opt_sel_row + 4) % 5;
        sfx_click();
    } else if (key == KEY_DOWN || key == KEY_TAB) {
        opt_sel_row = (opt_sel_row + 1) % 5;
        sfx_click();
    } else if (key == KEY_LEFT || key == KEY_RIGHT) {
        int dir = (key == KEY_RIGHT) ? 1 : -1;
        if (opt_sel_row == 0) {
            g_settings.anim_speed = (g_settings.anim_speed + 3 + dir) % 3;
            settings_save();
        } else if (opt_sel_row == 1) {
            g_settings.music_vol = (g_settings.music_vol + 4 + dir) % 4;
            music_set_volume(g_settings.music_vol);
            settings_save();
        } else if (opt_sel_row == 2) {
            g_settings.sfx_enabled ^= 1;
            settings_save();
        } else if (opt_sel_row == 3) {
            g_settings.scroll_speed = (g_settings.scroll_speed + 3 + dir) % 3;
            settings_save();
        } else if (opt_sel_row == 4) {
            opt_sel_btn ^= 1;
        }
        sfx_click();
    } else if (key == KEY_ENTER || key == ' ') {
        if (opt_sel_row == 0) {
            g_settings.anim_speed = (g_settings.anim_speed + 1) % 3;
            settings_save();
            sfx_click();
        } else if (opt_sel_row == 1) {
            g_settings.music_vol = (g_settings.music_vol + 1) % 4;
            music_set_volume(g_settings.music_vol);
            settings_save();
            sfx_click();
        } else if (opt_sel_row == 2) {
            g_settings.sfx_enabled ^= 1;
            settings_save();
            sfx_click();
        } else if (opt_sel_row == 3) {
            g_settings.scroll_speed = (g_settings.scroll_speed + 1) % 3;
            settings_save();
            sfx_click();
        } else if (opt_sel_row == 4) {
            g_settings.menu_open = 0;
            sfx_click();
            if (opt_sel_btn == 1) {
                return in_battlescape ? 1 : 2;
            }
            return 0;
        }
    }

    if (!g_input.left_clicked) return 0;

    /* Clicking outside the modal closes and resumes */
    if (mx < 28 || mx >= 292 || my < 14 || my >= 186) {
        g_settings.menu_open = 0;
        sfx_click();
        return 0;
    }

    /* Row 0: ANIM SPEED */
    if (my >= 40 && my < 60) {
        opt_sel_row = 0;
        if (mx >= 110 && mx < 164)      g_settings.anim_speed = ANIM_SLOW;
        else if (mx >= 168 && mx < 226) g_settings.anim_speed = ANIM_MEDIUM;
        else if (mx >= 230 && mx < 282) g_settings.anim_speed = ANIM_FAST;
        else return 0;
        settings_save();
        sfx_click();
        return 0;
    }

    /* Row 1: MUSIC VOL */
    if (my >= 65 && my < 85) {
        opt_sel_row = 1;
        if (mx >= 110 && mx < 150)      g_settings.music_vol = 0;
        else if (mx >= 154 && mx < 194) g_settings.music_vol = 1;
        else if (mx >= 198 && mx < 238) g_settings.music_vol = 2;
        else if (mx >= 242 && mx < 282) g_settings.music_vol = 3;
        else return 0;
        music_set_volume(g_settings.music_vol);
        settings_save();
        sfx_click();
        return 0;
    }

    /* Row 2: SOUND FX */
    if (my >= 90 && my < 110) {
        opt_sel_row = 2;
        if (mx >= 110 && mx < 194)      g_settings.sfx_enabled = 0;
        else if (mx >= 198 && mx < 282) g_settings.sfx_enabled = 1;
        else return 0;
        settings_save();
        sfx_click();
        return 0;
    }

    /* Row 3: MAP SCROLL */
    if (my >= 115 && my < 135) {
        opt_sel_row = 3;
        if (mx >= 110 && mx < 164)      g_settings.scroll_speed = 0;
        else if (mx >= 168 && mx < 226) g_settings.scroll_speed = 1;
        else if (mx >= 230 && mx < 282) g_settings.scroll_speed = 2;
        else return 0;
        settings_save();
        sfx_click();
        return 0;
    }

    /* Row 4: RESUME / ABORT OR QUIT */
    if (my >= 147 && my < 171) {
        opt_sel_row = 4;
        if (mx >= 38 && mx < 156) {
            opt_sel_btn = 0;
            g_settings.menu_open = 0;
            sfx_click();
            return 0;
        }
        if (mx >= 164 && mx < 282) {
            opt_sel_btn = 1;
            g_settings.menu_open = 0;
            sfx_click();
            return in_battlescape ? 1 : 2;
        }
    }

    return 0;
}
