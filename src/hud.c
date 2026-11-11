#include "hud.h"
#include "vga.h"
#include "gfx_assets.h"
#include "items.h"
#include "campaign.h"
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
    ACT_INV,
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
    { 288, 176, 28, 18, "INV",  ACT_INV       }, /* Field Inventory / Ground / ARV Bus */
    { 284, 145, 32, 10, "OPT",  ACT_OPTIONS   }  /* In-Game Options Menu */
};

#define NUM_HUD_BTNS (sizeof(hud_buttons) / sizeof(hud_buttons[0]))

/* Build list of living aliens currently visible in LOS of the selected operative */
static int build_spotted_aliens(int *out_indices) {
    const Unit *u = &g_units[g_battle.selected_unit];
    int a, n = 0;
    if (!u->active || u->faction != 0) return 0;
    for (a = 0; a < MAX_UNITS; a++) {
        const Unit *al = &g_units[a];
        if (!al->active || al->faction != 1) continue;
        if (g_map[al->z][al->y][al->x].fog == FOG_VISIBLE &&
            isomap_has_los(u->x, u->y, u->z, al->x, al->y, al->z)) {
            out_indices[n++] = a;
        }
    }
    return n;
}

static int build_bus_or_tile_items(const Unit *u, int in_bus, int *out_ids, int *out_src) {
    int i, n = 0;
    int is_base_def = (g_battle.biome_id == BIOME_BASE_DEFENSE);
    if (in_bus) {
        for (i = 0; i < ITEM_COUNT && n < 16; i++) {
            int qty = g_camp.arv_cargo[i] + (is_base_def ? g_camp.stores[i] : 0);
            if (qty > 0 && campaign_is_item_researched(i) &&
                item_get_def(i)->category <= ICAT_UTILITY) {
                out_ids[n] = i;
                out_src[n] = -1; /* -1 = from ARV Bus / Base Stores */
                n++;
            }
        }
    } else {

        /* 1. Ground items on current tile (u->x, u->y, u->z) */
        for (i = 0; i < 4 && n < 16; i++) {
            int gid = g_map[u->z][u->y][u->x].gnd_item[i];
            if (gid >= 0 && gid < ITEM_COUNT) {
                out_ids[n] = gid;
                out_src[n] = i; /* 0..3 = ground slot */
                n++;
            }
        }
        /* 2. Inspect any corpse on the same tile for remaining equipment! */
        for (i = 0; i < MAX_UNITS && n < 16; i++) {
            const Unit *cu = &g_units[i];
            if (cu->is_corpse && cu->x == u->x && cu->y == u->y && cu->z == u->z) {
                if (item_is_weapon(cu->weapon_id)) {
                    out_ids[n] = cu->weapon_id;
                    out_src[n] = 10 + i; /* 10+i = corpse weapon */
                    n++;
                }
                break;
            }
        }
    }
    return n;
}

static void render_inv_modal(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    int in_bus = g_battle.pre_mission_equip || isomap_unit_in_bus(u);
    const ItemDef *wdef = item_get_def(u->weapon_id);
    int ammo_id = (wdef->ammo_item_id >= 0) ? wdef->ammo_item_id : ITEM_RIFLE_MAG;
    int gren_id = (u->grenade_id >= 0) ? u->grenade_id : ITEM_FRAG_GRENADE;
    int rhs_ids[16], rhs_src[16];
    int n_rhs = build_bus_or_tile_items(u, in_bus, rhs_ids, rhs_src);
    int i, rows = (n_rhs < 4) ? n_rhs : 4;
    char buf[64];

    vga_rect_bevel(8, 6, 304, 136, 76, 96, 79);
    vga_rect_fill(11, 9, 298, 130, 14);

    /* Top Header Bar */
    vga_rect_bevel(12, 10, 22, 13, 68, 96, 79); vga_print_shadow(19, 13, "<", 96, 15);
    vga_rect_bevel(36, 10, 22, 13, 68, 96, 79); vga_print_shadow(43, 13, ">", 96, 15);
    vga_print_shadow(62, 13,
                     g_battle.pre_mission_equip ? "PRE-MISSION ARV EQUIP"
                     : (in_bus ? "OPERATIVE & ARV BUS" : "INVENTORY & TILE LOOT"),
                     96, 15);

    vga_rect_bevel(216, 10, 90, 13, 36, 96, 45);
    vga_print_shadow(222, 13, g_battle.pre_mission_equip ? "START MISSION" : "CLOSE [ESC]", 96, 15);

    /* Left Panel: Selected Operative's Current Equipment (x=12..158, y=25..137) */
    vga_rect_bevel(12, 25, 146, 112, 74, 68, 79);
    vga_print_shadow(16, 28, u->name, 80, 15);

    /* Row 0: Primary Weapon */
    vga_rect_bevel(15, 38, 140, 23, 13, 68, 15);
    vga_blit_ui(17, 41, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[u->weapon_id]);
    vga_print_shadow(51, 41, wdef->short_name, 96, 15);
    vga_rect_bevel(104, 41, 24, 16, 68, 162, 79); vga_print(107, 45, "DRP", 18);
    vga_rect_bevel(129, 41, 24, 16, 68, 96,  79); vga_print(132, 45, "THR", 96);

    /* Row 1: Ammo Magazines */
    vga_rect_bevel(15, 62, 140, 23, 13, 68, 15);
    vga_blit_ui(17, 65, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[ammo_id]);
    snprintf(buf, sizeof(buf), "AM:%d+%d", u->ammo, u->extra_clips);
    vga_print_shadow(51, 68, buf, 144, 15);
    vga_rect_bevel(104, 65, 24, 16, 68, 162, 79); vga_print(107, 69, "DRP", u->extra_clips > 0 ? 18 : 8);
    vga_rect_bevel(129, 65, 24, 16, 68, 96,  79); vga_print(132, 69, "THR", u->extra_clips > 0 ? 96 : 8);

    /* Row 2: Grenades */
    vga_rect_bevel(15, 86, 140, 23, 13, 68, 15);
    vga_blit_ui(17, 89, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[gren_id]);
    snprintf(buf, sizeof(buf), "GRN:%d", u->grenades);
    vga_print_shadow(51, 92, buf, 96, 15);
    vga_rect_bevel(104, 89, 24, 16, 68, 162, 79); vga_print(107, 93, "DRP", u->grenades > 0 ? 18 : 8);
    vga_rect_bevel(129, 89, 24, 16, 68, 96,  79); vga_print(132, 93, "THR", u->grenades > 0 ? 96 : 8);

    /* Row 3: Flares */
    vga_rect_bevel(15, 110, 140, 23, 13, 68, 15);
    vga_blit_ui(17, 113, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[ITEM_FLARE]);
    snprintf(buf, sizeof(buf), "FLR:%d", u->flares);
    vga_print_shadow(51, 116, buf, 80, 15);
    vga_rect_bevel(104, 113, 24, 16, 68, 162, 79); vga_print(107, 117, "DRP", u->flares > 0 ? 18 : 8);

    /* Right Panel: ARV Bus Cargo OR Ground / Corpse Items on Current Tile (x=160..306, y=25..137) */
    vga_rect_bevel(160, 25, 146, 112, 74, 68, 79);
    vga_print_shadow(164, 28, in_bus ? "ARV BUS GEAR RACK" : "GROUND & CORPSE ITEMS", 96, 15);

    if (n_rhs == 0) {
        vga_print(168, 76, in_bus ? "ARV CARGO RACK EMPTY" : "NO ITEMS ON THIS TILE", 8);
    }
    for (i = 0; i < rows; i++) {
        int id = rhs_ids[i];
        const ItemDef *d = item_get_def(id);
        int ry = 38 + i * 24;
        int researched = campaign_is_item_researched(id);
        vga_rect_bevel(163, ry, 140, 22, 13, 68, 15);
        vga_blit_ui(165, ry + 3, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[id]);
        vga_print_shadow(199, ry + 3, d->short_name, researched ? 96 : 8, 15);
        if (in_bus) {
            int qty = g_camp.arv_cargo[id] + (g_battle.biome_id == BIOME_BASE_DEFENSE ? g_camp.stores[id] : 0);
            snprintf(buf, sizeof(buf), "QTY:%d", qty);
            vga_print(199, ry + 12, buf, 18);
        } else {

            vga_print(199, ry + 12, rhs_src[i] >= 10 ? "CORPSE" : "GROUND", 80);
        }
        vga_rect_bevel(262, ry + 3, 38, 16, researched ? 36 : 68, 96, 79);
        vga_print(265, ry + 7, researched ? "TAKE" : "LOCK", researched ? 96 : 8);
    }
}

static void render_alien_head_indicators(void) {
    int spotted[MAX_UNITS];
    int n = build_spotted_aliens(spotted);
    int k;
    char num_s[4];
    for (k = 0; k < n && k < 8; k++) {
        int bx = SCREEN_W - 18 - k * 17;
        int by = HUD_Y - 15;
        int hov = (!g_battle.inv_modal_open && !g_battle.weapon_menu_open &&
                   g_input.mx >= bx && g_input.mx < bx + 15 &&
                   g_input.my >= by && g_input.my < by + 14);
        vga_rect_bevel(bx, by, 15, 14, hov ? 34 : 36, 96, 45);
        vga_rect_fill(bx + 4, by + 2, 7, 5, 7);
        vga_rect_fill(bx + 5, by + 7, 5, 3, 6);
        vga_pset(bx + 5, by + 5, 15); vga_pset(bx + 6, by + 5, 15);
        vga_pset(bx + 8, by + 5, 15); vga_pset(bx + 9, by + 5, 15);
        snprintf(num_s, sizeof(num_s), "%d", k + 1);
        vga_print(bx + 9, by + 7, num_s, 96);
    }
}

static int handle_inv_modal_click(int mx, int my) {
    Unit *u = &g_units[g_battle.selected_unit];
    int in_bus = g_battle.pre_mission_equip || isomap_unit_in_bus(u);
    const ItemDef *wdef = item_get_def(u->weapon_id);
    int ammo_id = (wdef->ammo_item_id >= 0) ? wdef->ammo_item_id : ITEM_RIFLE_MAG;
    int gren_id = (u->grenade_id >= 0) ? u->grenade_id : ITEM_FRAG_GRENADE;
    int rhs_ids[16], rhs_src[16];
    int n_rhs = build_bus_or_tile_items(u, in_bus, rhs_ids, rhs_src);
    int rows  = (n_rhs < 4) ? n_rhs : 4;

    if (my >= 10 && my < 23) {
        if (mx >= 12 && mx < 58) {
            isomap_select_next_soldier();
            return 1;
        }
        if (mx >= 216 && mx < 306) {
            g_battle.pre_mission_equip = 0;
            g_battle.inv_modal_open    = 0;
            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "MISSION ACTIVE - DEPLOY FROM BUS!");
            sfx_click();
            return 1;
        }
    }

    /* Left Panel: [DRP] (x=104..128) and [THR] (x=129..153) on Operative's 4 Slots */
    if (mx >= 104 && mx < 154) {
        int drop = (mx < 129);
        if (my >= 38 && my < 61) {
            if (drop) {
                isomap_drop_ground_item(u->x, u->y, u->z, u->weapon_id);
                u->weapon_id = ITEM_PISTOL;
                u->ammo      = item_get_def(ITEM_PISTOL)->clip_size;
                sfx_click();
            } else {
                g_battle.throw_item_id  = u->weapon_id;
                g_battle.fire_mode      = 6;
                g_battle.inv_modal_open = 0;
                u->weapon_id            = ITEM_PISTOL;
                u->ammo                 = item_get_def(ITEM_PISTOL)->clip_size;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW WEAPON: L-CLICK TARGET TILE!");
                sfx_click();
            }
            return 1;
        } else if (my >= 62 && my < 85 && u->extra_clips > 0) {
            u->extra_clips--;
            if (drop) {
                isomap_drop_ground_item(u->x, u->y, u->z, ammo_id);
            } else {
                g_battle.throw_item_id  = ammo_id;
                g_battle.fire_mode      = 6;
                g_battle.inv_modal_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW AMMO CLIP: L-CLICK TARGET!");
            }
            sfx_click();
            return 1;
        } else if (my >= 86 && my < 109 && u->grenades > 0) {
            if (drop) {
                u->grenades--;
                isomap_drop_ground_item(u->x, u->y, u->z, gren_id);
            } else {
                g_battle.fire_mode      = 4;
                g_battle.inv_modal_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW GRENADE: L-CLICK TARGET!");
            }
            sfx_click();
            return 1;
        } else if (my >= 110 && my < 133 && u->flares > 0 && drop) {
            u->flares--;
            isomap_drop_ground_item(u->x, u->y, u->z, ITEM_FLARE);
            sfx_click();
            return 1;
        }
    }

    /* Right Panel: [TAKE] from ARV Bus Cargo OR Ground / Corpse on Current Tile */
    if (n_rhs > 0 && mx >= 163 && mx < 303 && my >= 38 && my < 38 + rows * 24) {
        int row = (my - 38) / 24;
        int id  = rhs_ids[row];
        int src = rhs_src[row];
        if (!campaign_is_item_researched(id)) {
            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                     "ALIEN TECH UNRESEARCHED - RECOVER AT VICTORY!");
            sfx_beep(200, 50);
            return 1;
        }
        if (item_is_weapon(id)) {
            if (in_bus) g_camp.arv_cargo[u->weapon_id]++;
            else        isomap_drop_ground_item(u->x, u->y, u->z, u->weapon_id);
            u->weapon_id = id;
            u->ammo      = item_get_def(id)->clip_size;
        } else if (item_is_ammo(id)) {
            u->extra_clips++;
        } else if (item_is_grenade(id)) {
            u->grenade_id = id;
            u->grenades++;
        } else if (id == ITEM_FLARE) {
            u->flares++;
        }
        if (src == -1) {
            if (g_camp.arv_cargo[id] > 0) g_camp.arv_cargo[id]--;
            else if (g_battle.biome_id == BIOME_BASE_DEFENSE && g_camp.stores[id] > 0) g_camp.stores[id]--;
        } else if (src >= 0 && src < 4) {

            g_map[u->z][u->y][u->x].gnd_item[src] = -1;
        } else if (src >= 10 && src - 10 < MAX_UNITS) {
            g_units[src - 10].weapon_id = ITEM_NONE;
        }
        sfx_click();
        return 1;
    }

    return 1;
}




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
        /* Show the actual equipped item's inventory icon (grenade/flare while in those modes) */
        int icon_item = (g_battle.fire_mode == 4) ? ((u->grenade_id >= 0) ? u->grenade_id : ITEM_FRAG_GRENADE)
                      : (g_battle.fire_mode == 5) ? ITEM_FLARE
                      : u->weapon_id;
        const ItemDef *wd = item_get_def(u->weapon_id);
        vga_rect_bevel(4, 156, 44, 38, bg, (wep_hov || g_battle.fire_mode) ? 96 : 163, 172);
        if (icon_item >= 0 && icon_item < ITEM_COUNT)
            vga_blit_ui(10, 167, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[icon_item]);

        if (g_battle.fire_mode == 0) {
            vga_print(6, 158, wd->short_name, 96);
            snprintf(buf, sizeof(buf), "%02d+%d", u->ammo, u->extra_clips);
            vga_print(8, 185, buf, u->ammo > 0 ? 2 : 33);
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
        if (b->action_id == ACT_INV     && g_battle.inv_modal_open) active = 1;
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

    /* 5. Classic X-COM Popup Firing Menu when Weapon Slot is Clicked (shows TU & Hit %!) */
    if (g_battle.weapon_menu_open) {
        int row;
        char opt_buf[32];
        vga_rect_bevel(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, 74, 80, 79);
        for (row = 0; row < 5; row++) {
            int ry = POPUP_Y + 4 + row * 15;
            int hov = (g_input.mx >= POPUP_X + 3 && g_input.mx < POPUP_X + POPUP_W - 3 &&
                       g_input.my >= ry && g_input.my < ry + 14);
            if (row == 0) {
                snprintf(opt_buf, sizeof(opt_buf), "1.SNAP  12TU (%2d%%)",
                         isomap_calc_shot_acc(u, 1, 6));
            } else if (row == 1) {
                snprintf(opt_buf, sizeof(opt_buf), "2.AIMED 20TU (%2d%%)",
                         isomap_calc_shot_acc(u, 2, 6));
            } else if (row == 2) {
                snprintf(opt_buf, sizeof(opt_buf), "3.AUTO  18TU (%2d%%)",
                         isomap_calc_shot_acc(u, 3, 6));
            } else if (row == 3) {
                snprintf(opt_buf, sizeof(opt_buf), "4.GREN  15TU (%2d%%)", u->throw_acc);
            } else {
                snprintf(opt_buf, sizeof(opt_buf), "5.ELEC-FLARE  (8TU)");
            }
            vga_rect_bevel(POPUP_X + 3, ry, POPUP_W - 6, 14,
                           hov ? 65 : 76, hov ? 96 : 68, 79);
            vga_print_shadow(POPUP_X + 6, ry + 3, opt_buf, hov ? 96 : 1, 15);
        }
    }


    /* 6. Bottom-Right Spotted Alien Head Indicators (1..8) */
    render_alien_head_indicators();

    /* 7. Pre-Mission ARV Equip / Field Inventory / Corpse Loot Modal */
    if (g_battle.inv_modal_open) {
        render_inv_modal();
    }
}

int hud_handle_click(int mx, int my, int right_click) {
    unsigned i;

    if (g_battle.inv_modal_open) {
        if (right_click && !g_battle.pre_mission_equip) {
            g_battle.inv_modal_open = 0;
            sfx_click();
            return 1;
        }
        return handle_inv_modal_click(mx, my);
    }

    /* Check if clicking any Bottom-Right Spotted Alien Head Indicator (centers camera on alien!) */
    if (!g_battle.weapon_menu_open && my >= HUD_Y - 15 && my < HUD_Y - 1) {
        int spotted[MAX_UNITS];
        int n = build_spotted_aliens(spotted);
        int k;
        for (k = 0; k < n && k < 8; k++) {
            int bx = SCREEN_W - 18 - k * 17;
            if (mx >= bx && mx < bx + 15) {
                Unit *al = &g_units[spotted[k]];
                g_battle.view_level = al->z;
                isomap_center_camera_on(al->x, al->y, al->z);
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                         "CENTERED ON SPOTTED %s!", al->name);
                sfx_click();
                return 1;
            }
        }
    }


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

                case ACT_INV:
                    g_battle.weapon_menu_open = 0;
                    g_battle.inv_modal_open = !g_battle.inv_modal_open;
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
