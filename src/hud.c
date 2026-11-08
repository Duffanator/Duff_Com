#include "hud.h"
#include "vga.h"
#include "gfx_assets.h"
#include "isomap.h"
#include "operative.h"
#include "input.h"
#include "sound.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int x, y, w, h;
    const char *label;
    int action_id;
} HudButton;

enum {
    ACT_WEAPON = 1,
    ACT_NEXT_UNIT,
    ACT_KNEEL,
    ACT_LEV_UP,
    ACT_LEV_DN,
    ACT_ROOF,
    ACT_NIGHT,
    ACT_RESERVE,
    ACT_END_TURN,
    ACT_OPTIONS
};

static const HudButton hud_buttons[] = {
    {   4, 156, 44, 38, "",     ACT_WEAPON    }, /* Left Weapon Slot */
    { 204, 156, 26, 18, "NXT",  ACT_NEXT_UNIT },
    { 232, 156, 26, 18, "KNL",  ACT_KNEEL     },
    { 260, 156, 26, 18, "UP",   ACT_LEV_UP    },
    { 288, 156, 28, 18, "END",  ACT_END_TURN  },
    { 204, 176, 26, 18, "RSV",  ACT_RESERVE   },
    { 232, 176, 26, 18, "ROF",  ACT_ROOF      },
    { 260, 176, 26, 18, "DN",   ACT_LEV_DN    },
    { 288, 176, 28, 18, "NGT",  ACT_NIGHT     },
    { 284, 145, 32, 10, "OPT",  ACT_OPTIONS   }  /* In-Game Options Menu */
};

#define NUM_HUD_BTNS (sizeof(hud_buttons) / sizeof(hud_buttons[0]))

/* Popup Firing Menu geometry (renders right above the Left Weapon Slot) */
#define POPUP_X  4
#define POPUP_Y  59
#define POPUP_W  142
#define POPUP_H  83

static void draw_stat_bar(int x, int y, const char *lbl, int val, int max_val, uint8_t bar_col) {
    char num_str[12];
    int bar_w = 54;
    int fill_w = (max_val > 0) ? ((val * bar_w) / max_val) : 0;
    if (fill_w < 0) fill_w = 0;
    if (fill_w > bar_w) fill_w = bar_w;

    vga_print(x, y, lbl, 3);
    snprintf(num_str, sizeof(num_str), "%2d", val);
    vga_print(x + 18, y, num_str, 0);

    vga_rect_bevel(x + 34, y, bar_w + 2, 7, 13, 10, 5);
    if (fill_w > 0) {
        vga_rect_fill(x + 35, y + 1, fill_w, 5, bar_col);
    }
}

void hud_render(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    unsigned i;
    char buf[48];

    /* Main X-COM Metallic Beveled Control Panel (y=144..199) */
    vga_rect_bevel(0, HUD_Y, SCREEN_W, HUD_H, 166, 162, 171);

    /* Top Status Ticker Strip (y=145..153) */
    vga_rect_fill(3, 146, 279, 8, 14);
    vga_print(6, 147, g_battle.status_msg, 96);
    snprintf(buf, sizeof(buf), "TRN:%d", g_battle.turn_number);
    vga_print(244, 147, buf, 80);

    /* 1. Left Weapon & Firing Mode Slot (x=4, y=156, w=44, h=38) */
    {
        int wep_hov = (g_input.mx >= 4 && g_input.mx < 48 && g_input.my >= 156 && g_input.my < 194);
        uint8_t bg = g_battle.fire_mode ? 38 : (wep_hov ? 66 : 168);
        int icon_id = (g_battle.fire_mode == 4) ? 3 : (u->weapon_id & 3);
        vga_rect_bevel(4, 156, 44, 38, bg, (wep_hov || g_battle.fire_mode) ? 96 : 163, 172);
        vga_blit_ui(6, 160, 40, 24, spr_weapons[icon_id]);

        if (g_battle.fire_mode == 0) {
            static const char *w_names[3] = {"RIFLE", "LASER", "PLASMA"};
            vga_print(8, 158, w_names[u->weapon_id % 3], 96);
            snprintf(buf, sizeof(buf), "AM:%02d", u->ammo);
            vga_print(8, 185, buf, 2);
        } else {
            static const char *f_names[6] = {"MOVE", "SNAP", "AIMED", "AUTO", "GREN", "FLARE"};
            vga_print(8, 158, f_names[g_battle.fire_mode % 6], 96);
            vga_print(8, 185, "AIM!", 0);
        }
    }

    /* 2. Soldier Portrait Box (x=51, y=156, w=36, h=38) */
    vga_rect_bevel(51, 156, 36, 38, 14, 163, 172);
    vga_blit_ui(53, 157, 32, 36, spr_portraits[u->portrait_id % NUM_PORTRAITS]);

    /* 3. Center Soldier Name & Stat Bars (TU, HP, EN) */
    vga_rect_bevel(90, 156, 111, 38, 169, 172, 163);
    operative_draw_rank_badge(93, 158, u->rank);
    vga_print_shadow(104, 158, u->name, 96, 15);

    draw_stat_bar(93, 167, "TU", u->tu,     u->tu_max,     18);  /* Bright Green TU */
    draw_stat_bar(93, 175, "HP", u->hp,     u->hp_max,     33);  /* Bright Red Health */
    draw_stat_bar(93, 183, "EN", u->energy, u->energy_max, 113); /* Orange Energy */

    /* 4. Right Tactical Arcade Button Grid */
    for (i = 1; i < NUM_HUD_BTNS; i++) {
        const HudButton *b = &hud_buttons[i];
        int active = 0;
        int hovered = (g_input.mx >= b->x && g_input.mx < b->x + b->w &&
                       g_input.my >= b->y && g_input.my < b->y + b->h);
        uint8_t fill = 166;
        if (b->action_id == ACT_KNEEL   && u->stance) active = 1;
        if (b->action_id == ACT_ROOF    && g_battle.roof_cutaway) active = 1;
        if (b->action_id == ACT_NIGHT   && g_battle.night_mode) active = 1;
        if (b->action_id == ACT_RESERVE && g_battle.reserve_mode > 0) active = 1;
        if (b->action_id == ACT_OPTIONS && g_settings.menu_open) active = 1;

        if (active) fill = 67;
        if (b->action_id == ACT_END_TURN) fill = 38;
        if (b->action_id == ACT_OPTIONS)  fill = 68;
        if (hovered) fill = (b->action_id == ACT_END_TURN) ? 34 : 65;

        vga_rect_bevel(b->x, b->y, b->w, b->h, fill, (active || hovered) ? 96 : 162, 172);
        vga_print_shadow(b->x + (b->w - (int)strlen(b->label) * 6) / 2,
                         b->y + (b->h - 6) / 2,
                         b->label, (active || hovered) ? 96 : 1, 15);
    }

    /* 5. Classic X-COM Popup Firing Menu when Weapon Slot is Clicked */
    if (g_battle.weapon_menu_open) {
        static const char *opt_labels[5] = {
            "1.SNAP SHOT   (12TU)",
            "2.AIMED SHOT  (20TU)",
            "3.AUTO SHOT   (18TU)",
            "4.HE GRENADE  (15TU)",
            "5.ELEC-FLARE   (8TU)"
        };
        int row;
        vga_rect_bevel(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, 74, 80, 79);
        for (row = 0; row < 5; row++) {
            int ry = POPUP_Y + 4 + row * 15;
            int hov = (g_input.mx >= POPUP_X + 3 && g_input.mx < POPUP_X + POPUP_W - 3 &&
                       g_input.my >= ry && g_input.my < ry + 14);
            vga_rect_bevel(POPUP_X + 3, ry, POPUP_W - 6, 14,
                           hov ? 65 : 76, hov ? 96 : 68, 79);
            vga_print_shadow(POPUP_X + 8, ry + 3, opt_labels[row], hov ? 96 : 1, 15);
        }
    }
}

int hud_handle_click(int mx, int my, int right_click) {
    unsigned i;

    /* Check if clicking inside the open Firing Mode Popup Menu first */
    if (g_battle.weapon_menu_open) {
        if (mx >= POPUP_X && mx < POPUP_X + POPUP_W &&
            my >= POPUP_Y && my < POPUP_Y + POPUP_H) {
            int row = (my - (POPUP_Y + 4)) / 15;
            if (row >= 0 && row < 5) {
                g_battle.fire_mode = row + 1;
                g_battle.weapon_menu_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                         row == 0 ? "AIMING: SNAP SHOT (L-CLICK TARGET)" :
                         row == 1 ? "AIMING: AIMED SHOT (L-CLICK TARGET)" :
                         row == 2 ? "AIMING: AUTO BURST (L-CLICK TARGET)" :
                         row == 3 ? "THROW HE GRENADE (L-CLICK TARGET)" :
                                    "THROW ELECTRO-FLARE (L-CLICK TILE)");
                sfx_click();
                return 1;
            }
        }
        /* Clicking anywhere outside the popup closes it */
        g_battle.weapon_menu_open = 0;
        if (my < HUD_Y) {
            sfx_click();
            return 1;
        }
    }

    if (my < HUD_Y) return 0;

    for (i = 0; i < NUM_HUD_BTNS; i++) {
        const HudButton *b = &hud_buttons[i];
        if (mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h) {
            switch (b->action_id) {
                case ACT_WEAPON:
                    if (right_click) {
                        g_battle.fire_mode = 0;
                        g_battle.weapon_menu_open = 0;
                        snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "MOVEMENT MODE");
                    } else {
                        g_battle.weapon_menu_open = !g_battle.weapon_menu_open;
                    }
                    sfx_click();
                    break;

                case ACT_NEXT_UNIT:
                    isomap_select_next_soldier();
                    break;

                case ACT_KNEEL:
                    isomap_toggle_kneel();
                    break;

                case ACT_LEV_UP:
                    if (g_battle.view_level < MAP_LEVS - 1) {
                        g_battle.view_level++;
                        sfx_click();
                    }
                    break;

                case ACT_LEV_DN:
                    if (g_battle.view_level > 0) {
                        g_battle.view_level--;
                        sfx_click();
                    }
                    break;

                case ACT_ROOF:
                    g_battle.roof_cutaway = !g_battle.roof_cutaway;
                    sfx_click();
                    break;

                case ACT_NIGHT:
                    g_battle.night_mode = !g_battle.night_mode;
                    isomap_recalc_lighting_and_fov();
                    sfx_click();
                    break;

                case ACT_RESERVE:
                    g_battle.reserve_mode = (g_battle.reserve_mode + 1) % 3;
                    snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                             g_battle.reserve_mode == 0 ? "RESERVE: FREE MOVE" :
                             g_battle.reserve_mode == 1 ? "RESERVE: SNAP SHOT (12 TU)" :
                                                          "RESERVE: AIMED SHOT (20 TU)");
                    sfx_click();
                    break;

                case ACT_END_TURN:
                    isomap_end_turn();
                    break;

                case ACT_OPTIONS:
                    g_battle.weapon_menu_open = 0;
                    g_settings.menu_open = !g_settings.menu_open;
                    sfx_click();
                    break;
            }
            return 1;
        }
    }
    return 1;
}
