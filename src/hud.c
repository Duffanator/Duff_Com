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
#include <stdlib.h>
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

#define MAX_TRAY_ITEMS 48

enum {
    SLOT_NONE      = -1,
    SLOT_RHAND     = 0,
    SLOT_LHAND     = 1,
    SLOT_BELT_S0   = 2,
    SLOT_BELT_S1   = 3,
    SLOT_BELT_S2   = 4,
    SLOT_BELT_S3   = 5,
    SLOT_BELT_L0   = 6,
    SLOT_BELT_L1   = 7,
    SLOT_BACKPACK0 = 8,
    SLOT_BACKPACK1 = 9,
    SLOT_BACKPACK2 = 10,
    SLOT_BACKPACK3 = 11,
    SLOT_BACKPACK4 = 12,
    SLOT_BACKPACK5 = 13,
    SLOT_OPERATIVE_COUNT = 14,
    SLOT_TRAY_BASE = 100
};

typedef struct {
    int slot_id;
    int x, y, w, h;
} InvSlotRect;

static const InvSlotRect inv_slot_rects[SLOT_OPERATIVE_COUNT] = {
    { SLOT_RHAND,       7,  31, 36, 24 },
    { SLOT_LHAND,     164,  31, 36, 24 },
    { SLOT_BELT_S0,   163,  86, 22, 20 },
    { SLOT_BELT_S1,   186,  86, 22, 20 },
    { SLOT_BELT_S2,   209,  86, 22, 20 },
    { SLOT_BELT_S3,   232,  86, 22, 20 },
    { SLOT_BELT_L0,   257,  85, 55, 18 },
    { SLOT_BELT_L1,   257, 104, 55, 18 },
    { SLOT_BACKPACK0, 208,  31, 34, 19 },
    { SLOT_BACKPACK1, 243,  31, 34, 19 },
    { SLOT_BACKPACK2, 278,  31, 34, 19 },
    { SLOT_BACKPACK3, 208,  51, 34, 19 },
    { SLOT_BACKPACK4, 243,  51, 34, 19 },
    { SLOT_BACKPACK5, 278,  51, 34, 19 }
};

#define TRAY_COLS 6
#define TRAY_ROWS 2
#define TRAY_VIS  (TRAY_COLS * TRAY_ROWS)

static int g_inv_tray_scroll   = 0; /* Scroll row offset (each row = 6 items) */
static int g_inv_last_unit     = -1;
static int g_drag_active       = 0;
static int g_drag_src_slot     = SLOT_NONE;
static int g_drag_item_id      = ITEM_NONE;
static int g_drag_item_ammo    = 0;
static int g_drag_item_prime   = -1;
static int g_drag_tray_src     = -1;
static int g_drag_start_x      = 0;
static int g_drag_start_y      = 0;
static int g_prime_target_slot = -1; /* Operative slot 0..13 currently selecting fuse 0..6 (-1 = none) */

static int build_bus_or_tile_items(const Unit *u, int in_bus, int *out_ids, int *out_src, int *out_qty) {
    int i, n = 0;
    int is_base_def = (g_battle.biome_id == BIOME_BASE_DEFENSE);
    if (in_bus) {
        for (i = 0; i < ITEM_COUNT && n < MAX_TRAY_ITEMS; i++) {
            int qty = g_camp.arv_cargo[i] + (is_base_def ? g_camp.stores[i] : 0);
            if (qty > 0 && campaign_is_item_researched(i) &&
                item_get_def(i)->category <= ICAT_UTILITY) {
                out_ids[n] = i;
                out_src[n] = -1; /* -1 = from ARV Bus / Base Stores */
                out_qty[n] = qty;
                n++;
            }
        }
    } else {
        /* 1. Persistent ground items on current tile (u->x, u->y, u->z) */
        for (i = 0; i < MAX_GND_ITEMS && n < MAX_TRAY_ITEMS; i++) {
            int gid = g_map[u->z][u->y][u->x].gnd_item[i];
            if (gid >= 0 && gid < ITEM_COUNT) {
                out_ids[n] = gid;
                out_src[n] = i; /* 0..MAX_GND_ITEMS-1 = ground slot */
                out_qty[n] = 1;
                n++;
            }
        }
        /* 2. Inspect any corpse on the same tile for remaining equipment */
        for (i = 0; i < MAX_UNITS && n < MAX_TRAY_ITEMS; i++) {
            const Unit *cu = &g_units[i];
            if (cu->is_corpse && cu->x == u->x && cu->y == u->y && cu->z == u->z) {
                if (item_is_weapon(cu->weapon_id)) {
                    out_ids[n] = cu->weapon_id;
                    out_src[n] = 100 + i; /* 100+i = corpse weapon */
                    out_qty[n] = 1;
                    n++;
                }
            }
        }
    }
    return n;
}

static int get_unit_slot_item(const Unit *u, int slot_id, int *out_ammo) {
    if (out_ammo) *out_ammo = 0;
    if (!u) return ITEM_NONE;
    if (slot_id == SLOT_RHAND) {
        if (out_ammo) *out_ammo = u->rhand_ammo;
        return u->rhand_item;
    }
    if (slot_id == SLOT_LHAND) {
        if (out_ammo) *out_ammo = u->lhand_ammo;
        return u->lhand_item;
    }
    if (slot_id >= SLOT_BELT_S0 && slot_id <= SLOT_BELT_S3) {
        return u->belt_small[slot_id - SLOT_BELT_S0];
    }
    if (slot_id >= SLOT_BELT_L0 && slot_id <= SLOT_BELT_L1) {
        return u->belt_large[slot_id - SLOT_BELT_L0];
    }
    if (slot_id >= SLOT_BACKPACK0 && slot_id <= SLOT_BACKPACK5) {
        return u->backpack[slot_id - SLOT_BACKPACK0];
    }
    return ITEM_NONE;
}

static void set_unit_slot_item_primed(Unit *u, int slot_id, int item_id, int ammo, int prime_turns) {
    if (!u) return;
    if (slot_id == SLOT_RHAND) {
        u->rhand_item = item_id;
        u->rhand_ammo = (item_id >= 0 && item_is_weapon(item_id)) ? ammo : 0;
    } else if (slot_id == SLOT_LHAND) {
        u->lhand_item = item_id;
        u->lhand_ammo = (item_id >= 0 && item_is_weapon(item_id)) ? ammo : 0;
    } else if (slot_id >= SLOT_BELT_S0 && slot_id <= SLOT_BELT_S3) {
        u->belt_small[slot_id - SLOT_BELT_S0] = item_id;
    } else if (slot_id >= SLOT_BELT_L0 && slot_id <= SLOT_BELT_L1) {
        u->belt_large[slot_id - SLOT_BELT_L0] = item_id;
    } else if (slot_id >= SLOT_BACKPACK0 && slot_id <= SLOT_BACKPACK5) {
        u->backpack[slot_id - SLOT_BACKPACK0] = item_id;
    }
    if (slot_id >= 0 && slot_id < SLOT_OPERATIVE_COUNT) {
        u->slot_prime[slot_id] = (item_id >= 0 && item_is_grenade(item_id) && prime_turns >= 0)
                                 ? (int8_t)prime_turns : -1;
    }
    isomap_sync_unit_inventory(u);
}

static void set_unit_slot_item(Unit *u, int slot_id, int item_id, int ammo) {
    set_unit_slot_item_primed(u, slot_id, item_id, ammo, -1);
}

static int inv_slot_accepts_item(int slot_id, int item_id) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return 0;
    if (slot_id == SLOT_RHAND || slot_id == SLOT_LHAND) {
        return item_fits_hand(item_id) && campaign_is_item_researched(item_id);
    }
    if (slot_id >= SLOT_BELT_S0 && slot_id <= SLOT_BELT_S3) {
        return item_fits_small_belt(item_id) && campaign_is_item_researched(item_id);
    }
    if (slot_id >= SLOT_BELT_L0 && slot_id <= SLOT_BELT_L1) {
        return item_fits_large_belt(item_id) && campaign_is_item_researched(item_id);
    }
    if (slot_id >= SLOT_BACKPACK0 && slot_id <= SLOT_BACKPACK5) {
        return item_fits_backpack(item_id);
    }
    return 0;
}

static void consume_tray_source(Unit *u, int id, int src) {
    if (src == -1) {
        if (g_camp.arv_cargo[id] > 0) {
            g_camp.arv_cargo[id]--;
        } else if (g_battle.biome_id == BIOME_BASE_DEFENSE && g_camp.stores[id] > 0) {
            g_camp.stores[id]--;
        }
    } else if (src >= 0 && src < MAX_GND_ITEMS) {
        g_map[u->z][u->y][u->x].gnd_item[src]  = -1;
        g_map[u->z][u->y][u->x].gnd_prime[src] = -1;
    } else if (src >= 100 && (src - 100) < MAX_UNITS) {
        g_units[src - 100].weapon_id = ITEM_NONE;
    }
}

static void return_item_to_tray_primed(Unit *u, int in_bus, int item_id, int prime_turns) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return;
    if (in_bus) {
        g_camp.arv_cargo[item_id]++;
    } else {
        isomap_drop_ground_item_primed(u->x, u->y, u->z, item_id, prime_turns);
    }
}

static void draw_slot_prime_badge(int x, int y, int prime_turns) {
    char ps[8];
    if (prime_turns < 0) return;
    snprintf(ps, sizeof(ps), "P%d", prime_turns);
    vga_rect_fill(x, y, 13, 7, 36);
    vga_print(x + 1, y + 1, ps, 96);
}

static int hit_test_inv_slot(int mx, int my, int n_tray, int allow_panel_snap) {
    int i;
    for (i = 0; i < SLOT_OPERATIVE_COUNT; i++) {
        const InvSlotRect *r = &inv_slot_rects[i];
        if (mx >= r->x && mx < r->x + r->w && my >= r->y && my < r->y + r->h) {
            return r->slot_id;
        }
    }
    if (allow_panel_snap) {
        if (mx >= 6 && mx < 91 && my >= 21 && my < 59) return SLOT_RHAND;
        if (mx >= 160 && mx < 203 && my >= 21 && my < 72) return SLOT_LHAND;
    }
    if (mx >= 8 && mx < 8 + TRAY_COLS * 47 && my >= 139 && my < 139 + TRAY_ROWS * 27) {
        int col = (mx - 8) / 47;
        int row = (my - 139) / 27;
        int rx  = (mx - 8) % 47;
        int ry  = (my - 139) % 27;
        if (col >= 0 && col < TRAY_COLS && row >= 0 && row < TRAY_ROWS && rx < 45 && ry < 25) {
            int idx = g_inv_tray_scroll * TRAY_COLS + row * TRAY_COLS + col;
            if (idx >= 0 && idx < n_tray) {
                return SLOT_TRAY_BASE + idx;
            }
        }
    }
    return SLOT_NONE;
}

/* Blit the central 20x16 portion of a 32x16 item icon so compact belt pouches show full-detail clips/grenades/flares */
static void blit_compact_pouch_icon(int dx, int dy, int item_id) {
    int x, y;
    const uint8_t *src;
    if (item_id < 0 || item_id >= ITEM_COUNT) return;
    src = spr_item_icons[item_id];
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 20; x++) {
            uint8_t c = src[y * ITEM_ICON_W + (x + 6)];
            if (c != COL_TRANS) {
                vga_pset(dx + x, dy + y, c);
            }
        }
    }
}

static void draw_inv_mini_bar(int x, int y, const char *lbl, int val, int max_v, uint8_t col) {
    char s[12];
    int bw = 38;
    int fw = (max_v > 0) ? ((val * bw) / max_v) : 0;
    if (fw < 0) fw = 0;
    if (fw > bw) fw = bw;
    vga_print(x, y, lbl, 3);
    snprintf(s, sizeof(s), "%2d", val);
    vga_print(x + 20, y, s, 0);
    vga_rect_bevel(x + 34, y, bw + 2, 6, 13, 10, 15);
    if (fw > 0) {
        vga_rect_fill(x + 35, y + 1, fw, 4, col);
    }
}

void hud_inv_scroll(int delta_rows) {
    Unit *u = &g_units[g_battle.selected_unit];
    int in_bus = g_battle.pre_mission_equip || isomap_unit_in_bus(u);
    int tray_ids[MAX_TRAY_ITEMS], tray_src[MAX_TRAY_ITEMS], tray_qty[MAX_TRAY_ITEMS];
    int n_tray = build_bus_or_tile_items(u, in_bus, tray_ids, tray_src, tray_qty);
    int total_rows = (n_tray + TRAY_COLS - 1) / TRAY_COLS;
    int max_scroll = (total_rows > TRAY_ROWS) ? (total_rows - TRAY_ROWS) : 0;

    g_inv_tray_scroll += delta_rows;
    if (g_inv_tray_scroll < 0) g_inv_tray_scroll = 0;
    if (g_inv_tray_scroll > max_scroll) g_inv_tray_scroll = max_scroll;
}

static void render_inv_modal(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    int in_bus = g_battle.pre_mission_equip || isomap_unit_in_bus(u);
    int tray_ids[MAX_TRAY_ITEMS], tray_src[MAX_TRAY_ITEMS], tray_qty[MAX_TRAY_ITEMS];
    int n_tray = build_bus_or_tile_items(u, in_bus, tray_ids, tray_src, tray_qty);
    int total_rows = (n_tray + TRAY_COLS - 1) / TRAY_COLS;
    int max_scroll = (total_rows > TRAY_ROWS) ? (total_rows - TRAY_ROWS) : 0;
    int mx = g_input.mx, my = g_input.my;
    int hov_slot = hit_test_inv_slot(mx, my, n_tray, g_drag_active);
    int inspect_id = ITEM_NONE;
    int i, r, c;
    char buf[64];

    if (g_inv_last_unit != g_battle.selected_unit) {
        g_inv_last_unit     = g_battle.selected_unit;
        g_drag_active       = 0;
        g_prime_target_slot = -1;
    }
    if (g_prime_target_slot >= 0 && g_prime_target_slot < SLOT_OPERATIVE_COUNT) {
        int pid = get_unit_slot_item(u, g_prime_target_slot, NULL);
        if (!item_is_grenade(pid)) {
            g_prime_target_slot = -1;
        }
    }
    if (g_inv_tray_scroll > max_scroll) g_inv_tray_scroll = max_scroll;
    if (g_inv_tray_scroll < 0)          g_inv_tray_scroll = 0;

    /* 1. Full-Screen 320x200 Metallic X-COM Inventory Backdrop */
    vga_rect_bevel(0, 0, SCREEN_W, SCREEN_H, 76, 96, 79);
    vga_rect_fill(2, 2, SCREEN_W - 4, SCREEN_H - 4, 14);

    /* 2. Top Navigation & Operative Header Bar (y=3..17) */
    vga_rect_bevel(4, 3, 312, 15, 74, 68, 79);
    {
        int hov_prev = (mx >= 6  && mx < 24 && my >= 4 && my < 17);
        int hov_next = (mx >= 25 && mx < 43 && my >= 4 && my < 17);
        int hov_done = (mx >= 234 && mx < 314 && my >= 4 && my < 17);
        vga_rect_bevel(6,  4, 18, 13, hov_prev ? 65 : 68, hov_prev ? 96 : 162, 79);
        vga_print_shadow(12, 7, "<", hov_prev ? 96 : 1, 15);
        vga_rect_bevel(25, 4, 18, 13, hov_next ? 65 : 68, hov_next ? 96 : 162, 79);
        vga_print_shadow(31, 7, ">", hov_next ? 96 : 1, 15);

        operative_draw_rank_badge(46, 7, u->rank);
        snprintf(buf, sizeof(buf), "%s (#%d)", u->name, (g_battle.selected_unit % MAX_SQUAD_SIZE) + 1);
        vga_print_shadow(57, 7, buf, 96, 15);

        vga_print_shadow(158, 7,
                         g_battle.pre_mission_equip ? "ARV LOADOUT"
                         : (in_bus ? "ARV BAY EQUIP" : "FIELD EQUIP"),
                         80, 15);

        vga_rect_bevel(234, 4, 80, 13,
                       hov_done ? 34 : (g_battle.pre_mission_equip ? 36 : 67),
                       96, 45);
        vga_print_shadow(240, 7,
                         g_battle.pre_mission_equip ? "START MSN" : "DONE [ESC]",
                         96, 15);
    }

    /* 3. Left Column: RIGHT HAND Slot (y=19..59) + Operative Vital Stats (y=61..124) */
    vga_rect_bevel(4, 19, 89, 41, 74, (u->active_hand == 0) ? 96 : 68, 79);
    vga_print_shadow(8, 22, "R.HAND", 96, 15);
    {
        int hov_rh = (!g_drag_active && mx >= 49 && mx < 90 && my >= 21 && my < 30);
        vga_rect_bevel(49, 21, 41, 9, (u->active_hand == 0) ? 67 : (hov_rh ? 65 : 68),
                       (u->active_hand == 0 || hov_rh) ? 96 : 162, 79);
        vga_print(52, 23, (u->active_hand == 0) ? "ACTIVE" : "SELECT",
                  (u->active_hand == 0) ? 18 : 80);
    }
    {
        const InvSlotRect *sr = &inv_slot_rects[SLOT_RHAND];
        int item_id = u->rhand_item;
        int can_drop = g_drag_active && ( inv_slot_accepts_item(SLOT_RHAND, g_drag_item_id) ||
                       (item_is_ammo(g_drag_item_id) && item_is_weapon(item_id) &&
                        item_get_def(item_id)->ammo_item_id == g_drag_item_id) );
        uint8_t bdr = (g_prime_target_slot == SLOT_RHAND) ? 33
                    : (can_drop ? ((hov_slot == SLOT_RHAND) ? 96 : 18)
                                : ((hov_slot == SLOT_RHAND) ? 80 : 68));
        vga_rect_bevel(sr->x, sr->y, sr->w, sr->h, 13, bdr, 15);
        if (item_id >= 0 && !(g_drag_active && g_drag_src_slot == SLOT_RHAND)) {
            vga_blit_ui(sr->x + 2, sr->y + 4, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[item_id]);
            if (item_is_grenade(item_id) && u->slot_prime[SLOT_RHAND] >= 0) {
                draw_slot_prime_badge(sr->x + sr->w - 14, sr->y + 1, u->slot_prime[SLOT_RHAND]);
            }
        } else if (item_id < 0) {
            vga_print(sr->x + 4, sr->y + 9, "EMPTY", 10);
        }
        if (item_id >= 0) {
            const ItemDef *d = item_get_def(item_id);
            vga_print(45, 32, d->short_name, 80);
            if (item_is_weapon(item_id)) {
                snprintf(buf, sizeof(buf), "AM:%d", u->rhand_ammo);
                vga_print(45, 40, buf, u->rhand_ammo > 0 ? 18 : 33);
            } else if (item_is_grenade(item_id)) {
                if (u->slot_prime[SLOT_RHAND] >= 0) {
                    snprintf(buf, sizeof(buf), "PRM:%dT", u->slot_prime[SLOT_RHAND]);
                    vga_print(45, 40, buf, 33);
                } else {
                    vga_print(45, 40, "UNPRMD", 113);
                }
            } else {
                snprintf(buf, sizeof(buf), "WT:%d", d->weight);
                vga_print(45, 40, buf, 144);
            }
            if (!g_battle.pre_mission_equip) {
                if (item_is_grenade(item_id)) {
                    int hov_prm = (!g_drag_active && mx >= 45 && mx < 67 && my >= 47 && my < 58);
                    int hov_thr = (!g_drag_active && mx >= 68 && mx < 90 && my >= 47 && my < 58);
                    int is_prmd = (u->slot_prime[SLOT_RHAND] >= 0);
                    vga_rect_bevel(45, 47, 22, 11,
                                   (g_prime_target_slot == SLOT_RHAND || hov_prm) ? 34 : 68,
                                   96, 45);
                    vga_print(47, 50, "PRM", 96);
                    vga_rect_bevel(68, 47, 22, 11,
                                   is_prmd ? (hov_thr ? 65 : 67) : 13,
                                   is_prmd ? 96 : 10, 79);
                    vga_print(70, 50, "THR", is_prmd ? 96 : 8);
                } else {
                    int hov_thr = (!g_drag_active && mx >= 45 && mx < 89 && my >= 47 && my < 58);
                    vga_rect_bevel(45, 47, 44, 11, hov_thr ? 65 : 68, hov_thr ? 96 : 162, 79);
                    vga_print(58, 50, "THR", 96);
                }
            } else if (item_is_grenade(item_id)) {
                int hov_prm = (!g_drag_active && mx >= 45 && mx < 89 && my >= 47 && my < 58);
                vga_rect_bevel(45, 47, 44, 11,
                               (g_prime_target_slot == SLOT_RHAND || hov_prm) ? 34 : 68,
                               96, 45);
                vga_print(52, 50, "PRIME", 96);
            }
        } else {
            vga_print(45, 35, "PRIMARY", 8);
            vga_print(45, 44, "WEAPON", 8);
        }
    }

    /* Operative Vital & Combat Stats + Weight/Encumbrance Panel (x=4..92, y=61..124) */
    vga_rect_bevel(4, 61, 89, 63, 74, 68, 79);
    {
        int two_hand_pct = isomap_unit_two_hand_penalty_pct(u);
        int eff_aim      = (u->firing_acc * two_hand_pct) / 100;
        if (two_hand_pct < 100) {
            snprintf(buf, sizeof(buf), "STATS (-%d%%2H)", 100 - two_hand_pct);
            vga_print_shadow(6, 63, buf, 113, 15);
        } else {
            vga_print_shadow(8, 63, "AGENT STATS", 80, 15);
        }
        draw_inv_mini_bar(7,  71, "TU",  u->tu,        u->tu_max,     18);
        draw_inv_mini_bar(7,  78, "STM", u->energy,    u->energy_max, 113);
        draw_inv_mini_bar(7,  85, "HP",  u->hp,        u->hp_max,     33);
        draw_inv_mini_bar(7,  92, "AIM", eff_aim,      100,           (two_hand_pct < 100) ? 113 : 96);
        draw_inv_mini_bar(7,  99, "THR", u->throw_acc, 100,           80);
        draw_inv_mini_bar(7, 106, "RCN", u->reactions, 100,           144);
    }
    {
        int wt  = isomap_unit_carried_weight(u);
        int pct = isomap_unit_move_tu_pct(u);
        uint8_t wcol = (pct <= 108) ? 18 : ((pct <= 132) ? 96 : ((pct <= 160) ? 113 : 33));
        snprintf(buf, sizeof(buf), "WT:%2d MV:%d%%", wt, pct);
        vga_print(7, 115, buf, wcol);
    }

    /* 4. Center Column: Full-Body Paper-Doll Chamber (x=95..158, y=19..124) */
    vga_rect_bevel(95, 19, 63, 105, 13, 66, 15);
    /* Subtle tactical scanner grid lines behind operative */
    for (r = 28; r < 120; r += 12) {
        vga_hline(97, 155, r, 78);
    }
    vga_vline(126, 21, 121, 78);

    /* Leader lines connecting full-body anatomy to slot panels (kept strictly off text) */
    vga_hline(95, 110, 44, 67);  /* Right Hand leader */
    vga_hline(142, 160, 44, 67); /* Left Hand leader */
    vga_hline(136, 160, 95, 67); /* Tactical Belt leader */
    vga_hline(136, 205, 28, 67); /* Backpack shoulder leader */

    /* Full-Body Operative Sprite (48x96) */
    vga_blit_ui(102, 24, FULLBODY_W, FULLBODY_H, spr_fullbody[u->portrait_id % NUM_PORTRAITS]);

    /* Held-item visual overlay on the paper-doll's hands */
    if (u->rhand_item >= 0 && u->rhand_item < ITEM_COUNT &&
        !(g_drag_active && g_drag_src_slot == SLOT_RHAND)) {
        vga_blit_ui(102, 64, GROUND_ITEM_W, GROUND_ITEM_H, spr_ground_items[u->rhand_item]);
    }
    if (u->lhand_item >= 0 && u->lhand_item < ITEM_COUNT &&
        !(g_drag_active && g_drag_src_slot == SLOT_LHAND)) {
        vga_blit_ui(134, 64, GROUND_ITEM_W, GROUND_ITEM_H, spr_ground_items[u->lhand_item]);
    }

    /* 5. Right Top-Left: LEFT HAND Slot (x=160..203, y=19..73) */
    vga_rect_bevel(160, 19, 44, 54, 74, (u->active_hand == 1) ? 96 : 68, 79);
    vga_print_shadow(164, 22, (u->active_hand == 1) ? "L.HND*" : "L.HAND",
                     (u->active_hand == 1) ? 18 : 96, 15);
    {
        const InvSlotRect *sr = &inv_slot_rects[SLOT_LHAND];
        int item_id = u->lhand_item;
        int can_drop = g_drag_active && ( inv_slot_accepts_item(SLOT_LHAND, g_drag_item_id) ||
                       (item_is_ammo(g_drag_item_id) && item_is_weapon(item_id) &&
                        item_get_def(item_id)->ammo_item_id == g_drag_item_id) );
        uint8_t bdr = (g_prime_target_slot == SLOT_LHAND) ? 33
                    : (can_drop ? ((hov_slot == SLOT_LHAND) ? 96 : 18)
                                : ((hov_slot == SLOT_LHAND) ? 80 : 68));
        vga_rect_bevel(sr->x, sr->y, sr->w, sr->h, 13, bdr, 15);
        if (item_id >= 0 && !(g_drag_active && g_drag_src_slot == SLOT_LHAND)) {
            vga_blit_ui(sr->x + 2, sr->y + 4, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[item_id]);
            if (item_is_grenade(item_id) && u->slot_prime[SLOT_LHAND] >= 0) {
                draw_slot_prime_badge(sr->x + sr->w - 14, sr->y + 1, u->slot_prime[SLOT_LHAND]);
            }
        } else if (item_id < 0) {
            vga_print(sr->x + 3, sr->y + 9, "EMPTY", 10);
        }
        if (item_id >= 0) {
            if (item_is_weapon(item_id)) {
                snprintf(buf, sizeof(buf), "AM:%d", u->lhand_ammo);
                vga_print(164, 56, buf, u->lhand_ammo > 0 ? 18 : 33);
            } else if (item_is_grenade(item_id)) {
                if (u->slot_prime[SLOT_LHAND] >= 0) {
                    snprintf(buf, sizeof(buf), "PRM:%dT", u->slot_prime[SLOT_LHAND]);
                    vga_print(164, 56, buf, 33);
                } else {
                    vga_print(164, 56, "UNPRMD", 113);
                }
            } else {
                snprintf(buf, sizeof(buf), "%.6s", item_get_def(item_id)->short_name);
                vga_print(164, 56, buf, 80);
            }
            if (!g_battle.pre_mission_equip) {
                if (item_is_grenade(item_id)) {
                    int hov_prm = (!g_drag_active && mx >= 163 && mx < 181 && my >= 62 && my < 72);
                    int hov_thr = (!g_drag_active && mx >= 182 && mx < 201 && my >= 62 && my < 72);
                    int is_prmd = (u->slot_prime[SLOT_LHAND] >= 0);
                    vga_rect_bevel(163, 62, 18, 10,
                                   (g_prime_target_slot == SLOT_LHAND || hov_prm) ? 34 : 68,
                                   96, 45);
                    vga_print(166, 64, "PR", 96);
                    vga_rect_bevel(182, 62, 19, 10,
                                   is_prmd ? (hov_thr ? 65 : 67) : 13,
                                   is_prmd ? 96 : 10, 79);
                    vga_print(186, 64, "TH", is_prmd ? 96 : 8);
                } else {
                    int hov_thr = (!g_drag_active && mx >= 164 && mx < 200 && my >= 62 && my < 72);
                    vga_rect_bevel(164, 62, 36, 10, hov_thr ? 65 : 68, hov_thr ? 96 : 162, 79);
                    vga_print(173, 64, "THR", 96);
                }
            } else if (item_is_grenade(item_id)) {
                int hov_prm = (!g_drag_active && mx >= 164 && mx < 200 && my >= 62 && my < 72);
                vga_rect_bevel(164, 62, 36, 10,
                               (g_prime_target_slot == SLOT_LHAND || hov_prm) ? 34 : 68,
                               96, 45);
                vga_print(167, 64, "PRIME", 96);
            }
        } else {
            vga_print(164, 59, "2ND HD", 8);
        }
    }

    /* 6. Right Top-Right: BACKPACK 6-Slot Area (x=205..315, y=19..73) */
    vga_rect_bevel(205, 19, 111, 54, 74, 68, 79);
    vga_print_shadow(209, 22, "BACKPACK (3X2)", 96, 15);
    for (i = 0; i < INV_BACKPACK_SZ; i++) {
        int sid = SLOT_BACKPACK0 + i;
        const InvSlotRect *sr = &inv_slot_rects[sid];
        int item_id = u->backpack[i];
        int can_drop = g_drag_active && inv_slot_accepts_item(sid, g_drag_item_id);
        uint8_t bdr = (g_prime_target_slot == sid) ? 33
                    : (can_drop ? ((hov_slot == sid) ? 96 : 18)
                                : ((hov_slot == sid) ? 80 : 68));
        vga_rect_bevel(sr->x, sr->y, sr->w, sr->h, 13, bdr, 15);
        if (item_id >= 0 && !(g_drag_active && g_drag_src_slot == sid)) {
            vga_blit_ui(sr->x + 1, sr->y + 1, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[item_id]);
            if (item_is_grenade(item_id) && u->slot_prime[sid] >= 0) {
                draw_slot_prime_badge(sr->x + sr->w - 14, sr->y + 1, u->slot_prime[sid]);
            }
        }
    }

    /* 7. Right Middle: TACTICAL BELT with 4 Small Slots + 2 Large Slots (x=160..315, y=75..124) */
    vga_rect_bevel(160, 75, 156, 49, 74, 68, 79);
    vga_print_shadow(164, 78, "BELT: 4 SMALL", 96, 15);
    vga_print_shadow(257, 78, "2 LARGE", 80, 15);

    /* 4 Compact Small Belt Pouches (22x20 each: clips, frag/alien grenades, flares) */
    for (i = 0; i < INV_BELT_SMALL; i++) {
        int sid = SLOT_BELT_S0 + i;
        const InvSlotRect *sr = &inv_slot_rects[sid];
        int item_id = u->belt_small[i];
        int can_drop = g_drag_active && inv_slot_accepts_item(sid, g_drag_item_id);
        uint8_t bdr = (g_prime_target_slot == sid) ? 33
                    : (can_drop ? ((hov_slot == sid) ? 96 : 18)
                                : (g_drag_active ? 37 : ((hov_slot == sid) ? 80 : 68)));
        vga_rect_bevel(sr->x, sr->y, sr->w, sr->h, 13, bdr, 15);
        if (item_id >= 0 && !(g_drag_active && g_drag_src_slot == sid)) {
            blit_compact_pouch_icon(sr->x + 1, sr->y + 2, item_id);
            if (item_is_grenade(item_id) && u->slot_prime[sid] >= 0) {
                draw_slot_prime_badge(sr->x + sr->w - 14, sr->y + 1, u->slot_prime[sid]);
            }
        } else if (item_id < 0) {
            snprintf(buf, sizeof(buf), "S%d", i + 1);
            vga_print(sr->x + 5, sr->y + 7, buf, 10);
        }
    }

    /* Under-belt strip below the 4 small belt pouches (x=164..254, max 15 chars = 90px) */
    if (!g_battle.pre_mission_equip) {
        int hov_tg = (!g_drag_active && mx >= 163 && mx < 207 && my >= 108 && my < 121);
        int hov_tf = (!g_drag_active && mx >= 209 && mx < 254 && my >= 108 && my < 121);
        vga_rect_bevel(163, 108, 44, 13, hov_tg ? 65 : 68, hov_tg ? 96 : 162, 79);
        vga_print(167, 112, "PRIME", u->grenades > 0 ? 96 : 8);

        vga_rect_bevel(209, 108, 45, 13, hov_tf ? 65 : 68, hov_tf ? 96 : 162, 79);
        snprintf(buf, sizeof(buf), "FLR:%d", u->flares);
        vga_print(213, 112, buf, u->flares > 0 ? 80 : 8);
    } else {
        vga_print(164, 108, "CLICK GREN=PRM", 80);
        snprintf(buf, sizeof(buf), "CL:%d GR:%d FL:%d", u->extra_clips, u->grenades, u->flares);
        vga_print(164, 116, buf, 18);
    }

    /* 2 Wide Large Belt Holsters/Pouches (55x18 each: drums, heavy packs, satchels, pistols) */
    for (i = 0; i < INV_BELT_LARGE; i++) {
        int sid = SLOT_BELT_L0 + i;
        const InvSlotRect *sr = &inv_slot_rects[sid];
        int item_id = u->belt_large[i];
        int can_drop = g_drag_active && inv_slot_accepts_item(sid, g_drag_item_id);
        uint8_t bdr = (g_prime_target_slot == sid) ? 33
                    : (can_drop ? ((hov_slot == sid) ? 96 : 18)
                                : (g_drag_active ? 37 : ((hov_slot == sid) ? 80 : 68)));
        vga_rect_bevel(sr->x, sr->y, sr->w, sr->h, 13, bdr, 15);
        if (item_id >= 0 && !(g_drag_active && g_drag_src_slot == sid)) {
            vga_blit_ui(sr->x + 2, sr->y + 1, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[item_id]);
            if (item_is_grenade(item_id) && u->slot_prime[sid] >= 0) {
                draw_slot_prime_badge(sr->x + 35, sr->y + 5, u->slot_prime[sid]);
            } else {
                vga_print(sr->x + 35, sr->y + 6, "LRG", 80);
            }
        } else if (item_id < 0) {
            snprintf(buf, sizeof(buf), "LRG #%d", i + 1);
            vga_print(sr->x + 10, sr->y + 6, buf, 10);
        }
    }

    /* Determine hovered or actively dragged item for the info bar */
    if (g_drag_active && g_drag_item_id >= 0) {
        inspect_id = g_drag_item_id;
    } else if (hov_slot >= 0 && hov_slot < SLOT_OPERATIVE_COUNT) {
        inspect_id = get_unit_slot_item(u, hov_slot, NULL);
    } else if (hov_slot >= SLOT_TRAY_BASE && (hov_slot - SLOT_TRAY_BASE) < n_tray) {
        inspect_id = tray_ids[hov_slot - SLOT_TRAY_BASE];
    }

    /* 8. Bottom Scrollable Equipment Tray (x=4..315, y=126..196) */
    {
        int drop_to_tray = (g_drag_active && g_drag_src_slot >= 0 &&
                            g_drag_src_slot < SLOT_OPERATIVE_COUNT && my >= 126);
        vga_rect_bevel(4, 126, 312, 71, 74, drop_to_tray ? 96 : 68, 79);
    }

    if (g_prime_target_slot >= 0 && g_prime_target_slot < SLOT_OPERATIVE_COUNT) {
        /* Interactive Grenade Fuse Selector Bar (0..6 turns + OFF) */
        int cur_p = u->slot_prime[g_prime_target_slot];
        int t;
        vga_rect_bevel(6, 127, 308, 11, 36, 96, 45);
        vga_print_shadow(9, 129, "FUSE TURNS:", 96, 15);
        for (t = 0; t <= 6; t++) {
            int bx = 80 + t * 28;
            int hov_t = (mx >= bx && mx < bx + 25 && my >= 127 && my < 138);
            char ts[8];
            snprintf(ts, sizeof(ts), "%dT", t);
            vga_rect_bevel(bx, 128, 25, 9,
                           (cur_p == t) ? 67 : (hov_t ? 65 : 13),
                           (cur_p == t || hov_t) ? 96 : 68, 15);
            vga_print(bx + 7, 129, ts, (cur_p == t) ? 18 : 96);
        }
        {
            int hov_off = (mx >= 278 && mx < 310 && my >= 127 && my < 138);
            vga_rect_bevel(278, 128, 32, 9, hov_off ? 34 : 68, 96, 79);
            vga_print(285, 129, "OFF", 96);
        }
    } else if (inspect_id >= 0 && inspect_id < ITEM_COUNT) {
        const ItemDef *id_def = item_get_def(inspect_id);
        if (item_is_weapon(inspect_id)) {
            snprintf(buf, sizeof(buf), "%s DMG:%d CLP:%d WT:%d",
                     id_def->full_name, id_def->dmg_base, id_def->clip_size, id_def->weight);
        } else if (item_is_ammo(inspect_id)) {
            snprintf(buf, sizeof(buf), "%s (%d RDS) WT:%d%s",
                     id_def->full_name, id_def->clip_size, id_def->weight,
                     item_fits_small_belt(inspect_id) ? "" : " [LRG]");
        } else if (item_is_grenade(inspect_id)) {
            int pr = (hov_slot >= 0 && hov_slot < SLOT_OPERATIVE_COUNT) ? u->slot_prime[hov_slot] : -1;
            if (pr >= 0) {
                snprintf(buf, sizeof(buf), "%s [PRIMED: %d TRN] DMG:%d WT:%d",
                         id_def->full_name, pr, id_def->dmg_base, id_def->weight);
            } else {
                snprintf(buf, sizeof(buf), "%s DMG:%d WT:%d (CLICK TO PRIME 0-6)",
                         id_def->full_name, id_def->dmg_base, id_def->weight);
            }
        } else {
            snprintf(buf, sizeof(buf), "%s (WEIGHT:%d)", id_def->full_name, id_def->weight);
        }
        vga_print_shadow(8, 129, buf, 18, 15);
    } else {
        vga_print_shadow(8, 129,
                         in_bus ? "ARV EQUIP BAY:" : "GROUND ITEMS:",
                         96, 15);
        vga_print(96, 129, "DRAG TO SLOTS / R-CLICK UNEQUIP", 80);
    }

    /* 2 Rows x 6 Columns of Scrollable Equipment Cells */
    for (r = 0; r < TRAY_ROWS; r++) {
        for (c = 0; c < TRAY_COLS; c++) {
            int cx = 8 + c * 47;
            int cy = 139 + r * 27;
            int idx = (g_inv_tray_scroll + r) * TRAY_COLS + c;
            int is_hov = (hov_slot == SLOT_TRAY_BASE + idx);

            if (idx < n_tray) {
                int id = tray_ids[idx];
                int qty = tray_qty[idx];
                int res = campaign_is_item_researched(id);
                const ItemDef *d = item_get_def(id);
                vga_rect_bevel(cx, cy, 45, 25, is_hov ? 75 : 13, is_hov ? 96 : 68, 15);
                vga_blit_ui(cx + 1, cy + 1, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[id]);
                if (in_bus) {
                    int qx;
                    snprintf(buf, sizeof(buf), "x%d", qty);
                    qx = cx + 44 - (int)strlen(buf) * 6;
                    vga_rect_fill(qx - 1, cy + 1, (int)strlen(buf) * 6 + 1, 8, is_hov ? 75 : 13);
                    vga_print(qx, cy + 2, buf, 18);
                } else if (tray_src[idx] >= 0 && tray_src[idx] < MAX_GND_ITEMS &&
                           g_map[u->z][u->y][u->x].gnd_prime[tray_src[idx]] >= 0) {
                    draw_slot_prime_badge(cx + 31, cy + 1,
                                          g_map[u->z][u->y][u->x].gnd_prime[tray_src[idx]]);
                } else if (tray_src[idx] >= 100) {
                    vga_rect_fill(cx + 25, cy + 1, 19, 8, is_hov ? 75 : 13);
                    vga_print(cx + 26, cy + 2, "CRP", 33);
                }
                vga_print(cx + 2, cy + 17, d->short_name, res ? 96 : 8);
            } else {
                vga_rect_bevel(cx, cy, 45, 25, 13, 12, 15);
            }
        }
    }
    if (n_tray == 0) {
        vga_print_shadow(64, 162,
                         in_bus ? "NO MORE EQUIPMENT IN ARV BAY" : "NO ITEMS ON GROUND AT THIS TILE",
                         8, 15);
    }

    /* Scroll Buttons & Scrollbar Track on Right Edge of Bottom Tray (x=291..312, y=139..193) */
    {
        int hov_up = (mx >= 291 && mx < 312 && my >= 139 && my < 154);
        int hov_dn = (mx >= 291 && mx < 312 && my >= 178 && my < 193);
        int thumb_h = 8;
        int thumb_y = 157;
        if (max_scroll > 0) {
            thumb_y = 157 + ((20 - thumb_h) * g_inv_tray_scroll) / max_scroll;
        }
        vga_rect_bevel(291, 139, 21, 15, hov_up ? 65 : 68, hov_up ? 96 : 162, 79);
        vga_print_shadow(296, 144, "UP", (g_inv_tray_scroll > 0) ? 96 : 8, 15);

        vga_rect_bevel(294, 156, 15, 20, 13, 68, 15);
        vga_rect_bevel(295, thumb_y, 13, thumb_h, 66, 96, 79);

        vga_rect_bevel(291, 178, 21, 15, hov_dn ? 65 : 68, hov_dn ? 96 : 162, 79);
        vga_print_shadow(296, 183, "DN", (g_inv_tray_scroll < max_scroll) ? 96 : 8, 15);
    }

    /* 9. Floating Dragged Item Icon under Mouse Cursor */
    if (g_drag_active && g_drag_item_id >= 0 && g_drag_item_id < ITEM_COUNT) {
        int fx = mx - (ITEM_ICON_W / 2);
        int fy = my - (ITEM_ICON_H / 2);
        vga_rect_bevel(fx - 2, fy - 2, ITEM_ICON_W + 4, ITEM_ICON_H + 4, 75, 96, 15);
        vga_blit_ui(fx, fy, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[g_drag_item_id]);
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

static void execute_inv_drop(Unit *u, int in_bus, int dst_slot, int mx, int my) {
    int src_slot   = g_drag_src_slot;
    int item_id    = g_drag_item_id;
    int item_am    = g_drag_item_ammo;
    int item_prime = g_drag_item_prime;
    int tray_src   = g_drag_tray_src;
    int moved_px   = abs(mx - g_drag_start_x) + abs(my - g_drag_start_y);

    if (item_id < 0 || item_id >= ITEM_COUNT) return;

    /* Single-click on an Operative Slot (without dragging):
     * - If it holds a grenade, open/toggle the Fuse Selector (0..6 turns)!
     * - If it is RHAND or LHAND, also select that hand as the active hand! */
    if (src_slot >= 0 && src_slot < SLOT_OPERATIVE_COUNT && dst_slot == src_slot && moved_px < 5) {
        if (src_slot == SLOT_RHAND || src_slot == SLOT_LHAND) {
            u->active_hand = (src_slot == SLOT_LHAND) ? 1 : 0;
            isomap_sync_unit_inventory(u);
        }
        if (item_is_grenade(item_id)) {
            g_prime_target_slot = (g_prime_target_slot == src_slot) ? -1 : src_slot;
        }
        sfx_click();
        return;
    }

    /* Quick-click on a bottom tray item (without dragging to a slot) auto-equips to best free slot! */
    if (src_slot >= SLOT_TRAY_BASE && dst_slot == src_slot && moved_px < 5) {
        int s_before[SLOT_OPERATIVE_COUNT], s;
        for (s = 0; s < SLOT_OPERATIVE_COUNT; s++) {
            s_before[s] = get_unit_slot_item(u, s, NULL);
        }
        if (!campaign_is_item_researched(item_id) && item_get_def(item_id)->category <= ICAT_UTILITY) {
            /* Unresearched alien gear goes to backpack if possible */
            int bi;
            for (bi = 0; bi < INV_BACKPACK_SZ; bi++) {
                if (u->backpack[bi] < 0) {
                    consume_tray_source(u, item_id, tray_src);
                    u->backpack[bi] = item_id;
                    u->slot_prime[SLOT_BACKPACK0 + bi] = (item_is_grenade(item_id) && item_prime >= 0)
                                                         ? (int8_t)item_prime : -1;
                    isomap_sync_unit_inventory(u);
                    sfx_click();
                    return;
                }
            }
            sfx_beep(200, 50);
            return;
        }
        if (isomap_give_unit_item(u, item_id)) {
            consume_tray_source(u, item_id, tray_src);
            if (item_is_grenade(item_id) && item_prime >= 0) {
                for (s = 0; s < SLOT_OPERATIVE_COUNT; s++) {
                    if (s_before[s] < 0 && get_unit_slot_item(u, s, NULL) == item_id) {
                        u->slot_prime[s] = (int8_t)item_prime;
                        break;
                    }
                }
            }
            sfx_click();
        } else {
            sfx_beep(200, 50);
        }
        return;
    }

    /* Dropping onto the Full-Body Paper-Doll Chamber (x=95..158, y=19..124) auto-equips to best free slot */
    if (dst_slot == SLOT_NONE && mx >= 95 && mx < 158 && my >= 19 && my < 124) {
        if (src_slot >= SLOT_TRAY_BASE) {
            int s_before[SLOT_OPERATIVE_COUNT], s;
            for (s = 0; s < SLOT_OPERATIVE_COUNT; s++) {
                s_before[s] = get_unit_slot_item(u, s, NULL);
            }
            if (isomap_give_unit_item(u, item_id)) {
                consume_tray_source(u, item_id, tray_src);
                if (item_is_grenade(item_id) && item_prime >= 0) {
                    for (s = 0; s < SLOT_OPERATIVE_COUNT; s++) {
                        if (s_before[s] < 0 && get_unit_slot_item(u, s, NULL) == item_id) {
                            u->slot_prime[s] = (int8_t)item_prime;
                            break;
                        }
                    }
                }
                sfx_click();
            } else {
                sfx_beep(200, 50);
            }
        }
        return;
    }

    /* Case A: Dropped onto an Operative Slot (SLOT_RHAND .. SLOT_BACKPACK5) */
    if (dst_slot >= 0 && dst_slot < SLOT_OPERATIVE_COUNT) {
        int dst_ammo  = 0;
        int dst_item  = get_unit_slot_item(u, dst_slot, &dst_ammo);
        int dst_prime = u->slot_prime[dst_slot];

        if (dst_slot == src_slot) return;

        /* Special Case: Dragging an Ammo Clip directly onto a matching Weapon in RHAND or LHAND reloads it! */
        if ((dst_slot == SLOT_RHAND || dst_slot == SLOT_LHAND) &&
            item_is_ammo(item_id) && item_is_weapon(dst_item) &&
            item_get_def(dst_item)->ammo_item_id == item_id) {
            int max_clip = item_get_def(dst_item)->clip_size;
            if (dst_ammo < max_clip) {
                if (src_slot >= SLOT_TRAY_BASE) {
                    consume_tray_source(u, item_id, tray_src);
                } else {
                    set_unit_slot_item(u, src_slot, ITEM_NONE, 0);
                }
                set_unit_slot_item(u, dst_slot, dst_item, max_clip);
                sfx_click();
                return;
            }
        }

        if (!inv_slot_accepts_item(dst_slot, item_id)) {
            sfx_beep(200, 50);
            return;
        }

        if (src_slot >= SLOT_TRAY_BASE) {
            /* Dragged from Bottom Tray (ARV Bay or Ground Tile) to Operative Slot */
            int new_ammo = item_is_weapon(item_id) ? item_get_def(item_id)->clip_size : 0;
            consume_tray_source(u, item_id, tray_src);
            if (dst_item >= 0) {
                return_item_to_tray_primed(u, in_bus, dst_item, dst_prime);
            }
            if ((dst_slot == SLOT_RHAND || dst_slot == SLOT_LHAND) &&
                (item_is_weapon(item_id) || item_is_grenade(item_id))) {
                u->active_hand = (dst_slot == SLOT_LHAND) ? 1 : 0;
            }
            set_unit_slot_item_primed(u, dst_slot, item_id, new_ammo, item_prime);
            sfx_click();
            return;
        } else if (src_slot >= 0 && src_slot < SLOT_OPERATIVE_COUNT) {
            /* Dragged from one Operative Slot to another Operative Slot */
            int new_ammo = item_is_weapon(item_id)
                           ? ((src_slot == SLOT_RHAND || src_slot == SLOT_LHAND) ? item_am : item_get_def(item_id)->clip_size)
                           : 0;
            if (dst_item < 0) {
                set_unit_slot_item_primed(u, src_slot, ITEM_NONE, 0, -1);
                if ((dst_slot == SLOT_RHAND || dst_slot == SLOT_LHAND) &&
                    (item_is_weapon(item_id) || item_is_grenade(item_id))) {
                    u->active_hand = (dst_slot == SLOT_LHAND) ? 1 : 0;
                }
                set_unit_slot_item_primed(u, dst_slot, item_id, new_ammo, item_prime);
                if (g_prime_target_slot == src_slot) g_prime_target_slot = dst_slot;
                sfx_click();
                return;
            } else if (inv_slot_accepts_item(src_slot, dst_item)) {
                int swap_ammo = item_is_weapon(dst_item)
                                ? ((dst_slot == SLOT_RHAND || dst_slot == SLOT_LHAND) ? dst_ammo : item_get_def(dst_item)->clip_size)
                                : 0;
                set_unit_slot_item_primed(u, src_slot, dst_item, swap_ammo, dst_prime);
                set_unit_slot_item_primed(u, dst_slot, item_id, new_ammo, item_prime);
                if (g_prime_target_slot == src_slot)      g_prime_target_slot = dst_slot;
                else if (g_prime_target_slot == dst_slot) g_prime_target_slot = src_slot;
                sfx_click();
                return;
            } else {
                sfx_beep(200, 50);
                return;
            }
        }
    }

    /* Case B: Dragged from an Operative Slot and dropped onto the Bottom Tray (y >= 126) -> Unequip / Drop! */
    if (src_slot >= 0 && src_slot < SLOT_OPERATIVE_COUNT && my >= 126) {
        if (g_prime_target_slot == src_slot) g_prime_target_slot = -1;
        set_unit_slot_item_primed(u, src_slot, ITEM_NONE, 0, -1);
        return_item_to_tray_primed(u, in_bus, item_id, item_prime);
        sfx_click();
    }
}

void hud_update_inventory(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    int in_bus = g_battle.pre_mission_equip || isomap_unit_in_bus(u);
    int tray_ids[MAX_TRAY_ITEMS], tray_src[MAX_TRAY_ITEMS], tray_qty[MAX_TRAY_ITEMS];
    int n_tray = build_bus_or_tile_items(u, in_bus, tray_ids, tray_src, tray_qty);
    int mx = g_input.mx, my = g_input.my;

    if (!g_battle.inv_modal_open) {
        g_drag_active       = 0;
        g_prime_target_slot = -1;
        return;
    }

    /* 0. Keyboard shortcut 0..6 when priming a grenade (or hovering a grenade slot) */
    if (g_input.last_key >= '0' && g_input.last_key <= '6') {
        int tslot = g_prime_target_slot;
        if (tslot < 0) {
            int hslot = hit_test_inv_slot(mx, my, n_tray, 0);
            if (hslot >= 0 && hslot < SLOT_OPERATIVE_COUNT &&
                item_is_grenade(get_unit_slot_item(u, hslot, NULL))) {
                tslot = hslot;
            }
        }
        if (tslot >= 0 && tslot < SLOT_OPERATIVE_COUNT &&
            item_is_grenade(get_unit_slot_item(u, tslot, NULL))) {
            u->slot_prime[tslot] = (int8_t)(g_input.last_key - '0');
            g_prime_target_slot  = -1;
            g_input.last_key     = 0;
            sfx_click();
            return;
        }
    }

    /* 1. Handle active drag release when left mouse button is released */
    if (g_drag_active) {
        if (!g_input.left_held) {
            int dst_slot = hit_test_inv_slot(mx, my, n_tray, 1);
            execute_inv_drop(u, in_bus, dst_slot, mx, my);
            g_drag_active     = 0;
            g_drag_src_slot   = SLOT_NONE;
            g_drag_item_id    = ITEM_NONE;
            g_drag_item_prime = -1;
        }
        return;
    }

    /* 2. Right-Click: Quick-unequip an operative slot to ARV/Ground, or close inventory if outside slots */
    if (g_input.right_clicked) {
        int rslot = hit_test_inv_slot(mx, my, n_tray, 0);
        if (rslot >= 0 && rslot < SLOT_OPERATIVE_COUNT) {
            int r_item  = get_unit_slot_item(u, rslot, NULL);
            int r_prime = u->slot_prime[rslot];
            if (r_item >= 0) {
                if (g_prime_target_slot == rslot) g_prime_target_slot = -1;
                set_unit_slot_item_primed(u, rslot, ITEM_NONE, 0, -1);
                return_item_to_tray_primed(u, in_bus, r_item, r_prime);
                sfx_click();
                return;
            }
        }
        if (g_prime_target_slot >= 0) {
            g_prime_target_slot = -1;
            sfx_click();
            return;
        }
        if (!g_battle.pre_mission_equip) {
            g_battle.inv_modal_open = 0;
            sfx_click();
        }
        return;
    }

    if (!g_input.left_clicked) return;

    /* 2b. Click on the interactive Fuse Selector Bar (y=127..138) when priming a grenade */
    if (g_prime_target_slot >= 0 && g_prime_target_slot < SLOT_OPERATIVE_COUNT &&
        my >= 127 && my < 138) {
        int t;
        for (t = 0; t <= 6; t++) {
            int bx = 80 + t * 28;
            if (mx >= bx && mx < bx + 25) {
                u->slot_prime[g_prime_target_slot] = (int8_t)t;
                g_prime_target_slot = -1;
                sfx_click();
                return;
            }
        }
        if (mx >= 278 && mx < 310) {
            u->slot_prime[g_prime_target_slot] = -1;
            g_prime_target_slot = -1;
            sfx_click();
            return;
        }
    }

    /* 3. Top Header Bar Buttons: [<], [>], and [START MSN] / [DONE] */
    if (my >= 3 && my < 18) {
        if (mx >= 6 && mx < 24) {
            isomap_select_prev_soldier();
            return;
        }
        if (mx >= 25 && mx < 43) {
            isomap_select_next_soldier();
            return;
        }
        if (mx >= 234 && mx < 314) {
            g_battle.pre_mission_equip = 0;
            g_battle.inv_modal_open    = 0;
            g_prime_target_slot        = -1;
            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                     in_bus ? "EQUIPMENT READY - DEPLOY FROM ARV!" : "INVENTORY CLOSED");
            sfx_click();
            return;
        }
    }

    /* 3b. Active Hand Selector Headers (R.HAND vs L.HAND) */
    if (my >= 20 && my < 30) {
        if (mx >= 6 && mx < 91) {
            u->active_hand = 0;
            isomap_sync_unit_inventory(u);
            sfx_click();
            return;
        }
        if (mx >= 160 && mx < 203) {
            u->active_hand = 1;
            isomap_sync_unit_inventory(u);
            sfx_click();
            return;
        }
    }

    /* 4. Bottom Tray Scroll Buttons: [UP] and [DN] */
    if (mx >= 291 && mx < 312) {
        if (my >= 139 && my < 154) {
            hud_inv_scroll(-1);
            sfx_click();
            return;
        }
        if (my >= 178 && my < 193) {
            hud_inv_scroll(1);
            sfx_click();
            return;
        }
    }

    /* 5. Hand & Belt Action Buttons: [PRM]/[THR] on RHAND, [PR]/[TH] on LHAND, [PRIME], [FLR] */
    if (g_battle.pre_mission_equip) {
        if (mx >= 45 && mx < 89 && my >= 47 && my < 58 && item_is_grenade(u->rhand_item)) {
            g_prime_target_slot = (g_prime_target_slot == SLOT_RHAND) ? -1 : SLOT_RHAND;
            sfx_click();
            return;
        }
        if (mx >= 164 && mx < 200 && my >= 62 && my < 72 && item_is_grenade(u->lhand_item)) {
            g_prime_target_slot = (g_prime_target_slot == SLOT_LHAND) ? -1 : SLOT_LHAND;
            sfx_click();
            return;
        }
    } else {
        /* RHAND buttons (x=45..90, y=47..58) */
        if (mx >= 45 && mx < 90 && my >= 47 && my < 58 && u->rhand_item >= 0) {
            int id = u->rhand_item;
            if (item_is_grenade(id)) {
                if (mx < 67) {
                    /* [PRM] button: open fuse selector 0..6 */
                    g_prime_target_slot = (g_prime_target_slot == SLOT_RHAND) ? -1 : SLOT_RHAND;
                    sfx_click();
                } else if (u->slot_prime[SLOT_RHAND] >= 0) {
                    /* [THR] button when primed: throw grenade! */
                    u->active_hand           = 0;
                    u->grenade_id            = id;
                    g_battle.throw_hand_slot = 0;
                    g_battle.fire_mode       = 4;
                    g_battle.inv_modal_open  = 0;
                    g_prime_target_slot      = -1;
                    isomap_sync_unit_inventory(u);
                    snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW PRIMED GRENADE: L-CLICK TARGET!");
                    sfx_click();
                } else {
                    g_prime_target_slot = SLOT_RHAND;
                    sfx_beep(200, 50);
                }
            } else if (id == ITEM_FLARE) {
                g_battle.fire_mode      = 5;
                g_battle.inv_modal_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW FLARE: L-CLICK TARGET!");
                sfx_click();
            } else {
                set_unit_slot_item(u, SLOT_RHAND, ITEM_NONE, 0);
                g_battle.throw_item_id  = id;
                g_battle.throw_prime    = -1;
                g_battle.fire_mode      = 6;
                g_battle.inv_modal_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW ITEM: L-CLICK TARGET TILE!");
                sfx_click();
            }
            return;
        }
        /* LHAND buttons (x=163..201, y=62..72) */
        if (mx >= 163 && mx < 201 && my >= 62 && my < 72 && u->lhand_item >= 0) {
            int id = u->lhand_item;
            if (item_is_grenade(id)) {
                if (mx < 181) {
                    /* [PR] button: open fuse selector 0..6 */
                    g_prime_target_slot = (g_prime_target_slot == SLOT_LHAND) ? -1 : SLOT_LHAND;
                    sfx_click();
                } else if (u->slot_prime[SLOT_LHAND] >= 0) {
                    /* [TH] button when primed: throw grenade! */
                    u->active_hand           = 1;
                    u->grenade_id            = id;
                    g_battle.throw_hand_slot = 1;
                    g_battle.fire_mode       = 4;
                    g_battle.inv_modal_open  = 0;
                    g_prime_target_slot      = -1;
                    isomap_sync_unit_inventory(u);
                    snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW PRIMED GRENADE: L-CLICK TARGET!");
                    sfx_click();
                } else {
                    g_prime_target_slot = SLOT_LHAND;
                    sfx_beep(200, 50);
                }
            } else if (id == ITEM_FLARE) {
                g_battle.fire_mode      = 5;
                g_battle.inv_modal_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW FLARE: L-CLICK TARGET!");
                sfx_click();
            } else {
                set_unit_slot_item(u, SLOT_LHAND, ITEM_NONE, 0);
                g_battle.throw_item_id  = id;
                g_battle.throw_prime    = -1;
                g_battle.fire_mode      = 6;
                g_battle.inv_modal_open = 0;
                snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW ITEM: L-CLICK TARGET TILE!");
                sfx_click();
            }
            return;
        }
        /* Belt [PRIME] button (x=163..207, y=108..121): select first carried grenade to prime */
        if (mx >= 163 && mx < 207 && my >= 108 && my < 121 && u->grenades > 0) {
            int s;
            for (s = 0; s < SLOT_OPERATIVE_COUNT; s++) {
                if (item_is_grenade(get_unit_slot_item(u, s, NULL))) {
                    g_prime_target_slot = s;
                    break;
                }
            }
            sfx_click();
            return;
        }
        /* Belt [FLR] quick-throw button (x=209..254, y=108..121) */
        if (mx >= 209 && mx < 254 && my >= 108 && my < 121 && u->flares > 0) {
            g_battle.fire_mode      = 5;
            g_battle.inv_modal_open = 0;
            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg), "THROW FLARE: L-CLICK TARGET!");
            sfx_click();
            return;
        }
    }

    /* 6. Start Dragging from any Occupied Operative Slot or Bottom Tray Item */
    {
        int slot = hit_test_inv_slot(mx, my, n_tray, 0);
        if (slot >= 0 && slot < SLOT_OPERATIVE_COUNT) {
            int am = 0;
            int id = get_unit_slot_item(u, slot, &am);
            if (id >= 0) {
                g_drag_active     = 1;
                g_drag_src_slot   = slot;
                g_drag_item_id    = id;
                g_drag_item_ammo  = am;
                g_drag_item_prime = u->slot_prime[slot];
                g_drag_tray_src   = -1;
                g_drag_start_x    = mx;
                g_drag_start_y    = my;
                sfx_click();
            }
        } else if (slot >= SLOT_TRAY_BASE && (slot - SLOT_TRAY_BASE) < n_tray) {
            int idx = slot - SLOT_TRAY_BASE;
            int src = tray_src[idx];
            g_drag_active     = 1;
            g_drag_src_slot   = slot;
            g_drag_item_id    = tray_ids[idx];
            g_drag_item_ammo  = item_get_def(tray_ids[idx])->clip_size;
            g_drag_item_prime = (!in_bus && src >= 0 && src < MAX_GND_ITEMS)
                                ? g_map[u->z][u->y][u->x].gnd_prime[src] : -1;
            g_drag_tray_src   = src;
            g_drag_start_x    = mx;
            g_drag_start_y    = my;
            sfx_click();
        }
    }
}

/* Popup Firing & Hand Action Menu geometry (renders right above the Left Weapon Slot) */
#define POPUP_X  4
#define POPUP_Y  44
#define POPUP_W  148
#define POPUP_H  98

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

    /* When Inventory / Pre-Mission Equip is open, render the Full-Screen 320x200 Paper-Doll UI! */
    if (g_battle.inv_modal_open) {
        render_inv_modal();
        return;
    }

    /* Main X-COM Metallic Beveled Control Panel (y=144..199) */
    vga_rect_bevel(0, HUD_Y, SCREEN_W, HUD_H, 166, 162, 171);

    /* Top Status Ticker Strip (y=145..153) */
    vga_rect_fill(3, 146, 279, 8, 14);
    vga_print(6, 147, g_battle.status_msg, 96);
    snprintf(buf, sizeof(buf), "TRN:%d", g_battle.turn_number);
    vga_print(244, 147, buf, 80);

    /* 1. Left Weapon / Hand Slot (x=4, y=156, w=44, h=38) */
    {
        int wep_hov = (g_input.mx >= 4 && g_input.mx < 48 && g_input.my >= 156 && g_input.my < 194);
        uint8_t bg = g_battle.fire_mode ? 38 : (wep_hov ? 66 : 168);
        int hand_slot = (u->active_hand == 1) ? SLOT_LHAND : SLOT_RHAND;
        int hand_item = (hand_slot == SLOT_LHAND) ? u->lhand_item : u->rhand_item;
        int hand_ammo = (hand_slot == SLOT_LHAND) ? u->lhand_ammo : u->rhand_ammo;
        int icon_item = (g_battle.fire_mode == 4) ? ((u->grenade_id >= 0) ? u->grenade_id : ITEM_FRAG_GRENADE)
                      : (g_battle.fire_mode == 5) ? ITEM_FLARE
                      : (g_battle.fire_mode == 6) ? g_battle.throw_item_id
                      : hand_item;
        vga_rect_bevel(4, 156, 44, 38, bg, (wep_hov || g_battle.fire_mode) ? 96 : 163, 172);
        if (icon_item >= 0 && icon_item < ITEM_COUNT)
            vga_blit_ui(10, 167, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[icon_item]);

        if (g_battle.fire_mode == 0) {
            if (hand_item >= 0 && hand_item < ITEM_COUNT) {
                const ItemDef *hd = item_get_def(hand_item);
                snprintf(buf, sizeof(buf), "%.5s", hd->short_name);
                vga_print(6, 158, buf, 96);
                vga_print(39, 158, (hand_slot == SLOT_LHAND) ? "L" : "R", 80);
                if (item_is_weapon(hand_item)) {
                    snprintf(buf, sizeof(buf), "%02d+%d", hand_ammo, u->extra_clips);
                    vga_print(8, 185, buf, hand_ammo > 0 ? 2 : 33);
                } else if (item_is_grenade(hand_item)) {
                    if (u->slot_prime[hand_slot] >= 0) {
                        snprintf(buf, sizeof(buf), "PRM:%d", u->slot_prime[hand_slot]);
                        vga_print(8, 185, buf, 33);
                    } else {
                        vga_print(8, 185, "UNPRM", 113);
                    }
                } else {
                    vga_print(8, 185, "READY", 80);
                }
            } else {
                vga_print(6, 158, "EMPTY", 14);
                vga_print(39, 158, (hand_slot == SLOT_LHAND) ? "L" : "R", 80);
                vga_print(8, 185, "HAND", 14);
            }
        } else {
            static const char *f_names[7] = {"MOVE", "SNAP", "AIMED", "AUTO", "GREN", "FLARE", "THROW"};
            vga_print(8, 158, f_names[g_battle.fire_mode % 7], 96);
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

    /* 5. Classic X-COM Popup Hand & Firing Menu when Weapon Slot is Clicked! */
    if (g_battle.weapon_menu_open) {
        int hand_slot = (u->active_hand == 1) ? SLOT_LHAND : SLOT_RHAND;
        int hand_item = (hand_slot == SLOT_LHAND) ? u->lhand_item : u->rhand_item;
        int row;
        char opt_buf[36];
        vga_rect_bevel(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, 74, 80, 79);

        /* Top Row: Right Hand [R:...] and Left Hand [L:...] Selector Tabs */
        {
            int hov_r = (g_input.mx >= POPUP_X + 3  && g_input.mx < POPUP_X + 73 &&
                         g_input.my >= POPUP_Y + 3  && g_input.my < POPUP_Y + 17);
            int hov_l = (g_input.mx >= POPUP_X + 75 && g_input.mx < POPUP_X + 145 &&
                         g_input.my >= POPUP_Y + 3  && g_input.my < POPUP_Y + 17);
            const char *rn = (u->rhand_item >= 0) ? item_get_def(u->rhand_item)->short_name : "EMPTY";
            const char *ln = (u->lhand_item >= 0) ? item_get_def(u->lhand_item)->short_name : "EMPTY";

            vga_rect_bevel(POPUP_X + 3, POPUP_Y + 3, 70, 14,
                           (u->active_hand == 0) ? 67 : (hov_r ? 65 : 13),
                           (u->active_hand == 0 || hov_r) ? 96 : 68, 15);
            snprintf(opt_buf, sizeof(opt_buf), "R:%.7s", rn);
            vga_print_shadow(POPUP_X + 7, POPUP_Y + 7, opt_buf,
                             (u->active_hand == 0) ? 96 : 80, 15);

            vga_rect_bevel(POPUP_X + 75, POPUP_Y + 3, 70, 14,
                           (u->active_hand == 1) ? 67 : (hov_l ? 65 : 13),
                           (u->active_hand == 1 || hov_l) ? 96 : 68, 15);
            snprintf(opt_buf, sizeof(opt_buf), "L:%.7s", ln);
            vga_print_shadow(POPUP_X + 79, POPUP_Y + 7, opt_buf,
                             (u->active_hand == 1) ? 96 : 80, 15);
        }

        if (item_is_grenade(hand_item)) {
            /* Active hand holds a Grenade: Fuse Selector [0..6] + Throw + Drop */
            int pr = u->slot_prime[hand_slot];
            int t;
            int ry0 = POPUP_Y + 20;
            vga_rect_bevel(POPUP_X + 3, ry0, POPUP_W - 6, 14, 76, 68, 79);
            vga_print_shadow(POPUP_X + 5, ry0 + 4, "FUSE:", 96, 15);
            for (t = 0; t <= 6; t++) {
                int bx = POPUP_X + 37 + t * 15;
                int hov_t = (g_input.mx >= bx && g_input.mx < bx + 14 &&
                             g_input.my >= ry0 + 1 && g_input.my < ry0 + 13);
                char ts[4];
                snprintf(ts, sizeof(ts), "%d", t);
                vga_rect_bevel(bx, ry0 + 1, 14, 12,
                               (pr == t) ? 67 : (hov_t ? 65 : 13),
                               (pr == t || hov_t) ? 96 : 68, 15);
                vga_print(bx + 4, ry0 + 4, ts, (pr == t) ? 18 : 96);
            }
            for (row = 1; row < 5; row++) {
                int ry = POPUP_Y + 20 + row * 15;
                int hov = (g_input.mx >= POPUP_X + 3 && g_input.mx < POPUP_X + POPUP_W - 3 &&
                           g_input.my >= ry && g_input.my < ry + 14);
                uint8_t tc = hov ? 96 : 1;
                if (row == 1) {
                    if (pr < 0) {
                        snprintf(opt_buf, sizeof(opt_buf), "UNPRIMED (PICK 0-6)");
                        tc = 113;
                    } else if (pr == 0) {
                        snprintf(opt_buf, sizeof(opt_buf), "PRIMED: 0T (IMPACT!)");
                        tc = 33;
                    } else {
                        snprintf(opt_buf, sizeof(opt_buf), "PRIMED: %d TURN DELAY", pr);
                        tc = 18;
                    }
                } else if (row == 2) {
                    snprintf(opt_buf, sizeof(opt_buf), "THROW GREN 15TU(%2d%%)", u->throw_acc);
                    if (pr < 0) tc = 8;
                } else if (row == 3) {
                    snprintf(opt_buf, sizeof(opt_buf), "DROP ON TILE   (2TU)");
                    if (pr < 0) tc = 8;
                } else {
                    snprintf(opt_buf, sizeof(opt_buf), "OPEN INVENTORY   [I]");
                }
                vga_rect_bevel(POPUP_X + 3, ry, POPUP_W - 6, 14,
                               hov ? 65 : 76, hov ? 96 : 68, 79);
                vga_print_shadow(POPUP_X + 6, ry + 4, opt_buf, tc, 15);
            }
        } else if (item_is_weapon(hand_item)) {
            /* Active hand holds a Weapon: Snap, Aimed, Auto, Throw Weapon, 2H Grip status */
            int pen_pct = isomap_unit_two_hand_penalty_pct(u);
            for (row = 0; row < 5; row++) {
                int ry = POPUP_Y + 20 + row * 15;
                int hov = (g_input.mx >= POPUP_X + 3 && g_input.mx < POPUP_X + POPUP_W - 3 &&
                           g_input.my >= ry && g_input.my < ry + 14);
                uint8_t tc = hov ? 96 : 1;
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
                    snprintf(opt_buf, sizeof(opt_buf), "4.THROW WEP  (10TU)");
                } else {
                    if (pen_pct < 100) {
                        snprintf(opt_buf, sizeof(opt_buf), "2H PENALTY: -%d%% AIM", 100 - pen_pct);
                        tc = 113;
                    } else {
                        snprintf(opt_buf, sizeof(opt_buf), "2H GRIP: FULL ACC");
                        tc = 18;
                    }
                }
                vga_rect_bevel(POPUP_X + 3, ry, POPUP_W - 6, 14,
                               hov ? 65 : 76, hov ? 96 : 68, 79);
                vga_print_shadow(POPUP_X + 6, ry + 4, opt_buf, tc, 15);
            }
        } else {
            /* Active hand holds a Flare, Clip, or is Empty */
            for (row = 0; row < 5; row++) {
                int ry = POPUP_Y + 20 + row * 15;
                int hov = (g_input.mx >= POPUP_X + 3 && g_input.mx < POPUP_X + POPUP_W - 3 &&
                           g_input.my >= ry && g_input.my < ry + 14);
                uint8_t tc = hov ? 96 : 1;
                if (row == 0) {
                    if (hand_item == ITEM_FLARE) {
                        snprintf(opt_buf, sizeof(opt_buf), "THROW E-FLARE  (8TU)");
                    } else if (hand_item >= 0) {
                        snprintf(opt_buf, sizeof(opt_buf), "THROW ITEM    (10TU)");
                    } else {
                        snprintf(opt_buf, sizeof(opt_buf), "HAND IS EMPTY");
                        tc = hov ? 96 : 7;
                    }
                } else if (row == 1) {
                    snprintf(opt_buf, sizeof(opt_buf), "SWITCH R/L HAND TAB");
                    tc = 80;
                } else if (row == 2) {
                    snprintf(opt_buf, sizeof(opt_buf), "OPEN INVENTORY   [I]");
                } else if (row == 3) {
                    snprintf(opt_buf, sizeof(opt_buf), "FLARES CARRIED:  %2d", u->flares);
                    tc = u->flares > 0 ? 18 : 8;
                } else {
                    snprintf(opt_buf, sizeof(opt_buf), "GRENS CARRIED:   %2d", u->grenades);
                    tc = u->grenades > 0 ? 96 : 8;
                }
                vga_rect_bevel(POPUP_X + 3, ry, POPUP_W - 6, 14,
                               hov ? 65 : 76, hov ? 96 : 68, 79);
                vga_print_shadow(POPUP_X + 6, ry + 4, opt_buf, tc, 15);
            }
        }
    }

    /* 6. Bottom-Right Spotted Alien Head Indicators (1..8) */
    render_alien_head_indicators();
}

int hud_handle_click(int mx, int my, int right_click) {
    unsigned i;

    if (g_battle.inv_modal_open) {
        hud_update_inventory();
        return 1;
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
        Unit *u = &g_units[g_battle.selected_unit];
        int hand_slot = (u->active_hand == 1) ? SLOT_LHAND : SLOT_RHAND;
        int hand_item = (hand_slot == SLOT_LHAND) ? u->lhand_item : u->rhand_item;

        if (mx >= POPUP_X && mx < POPUP_X + POPUP_W &&
            my >= POPUP_Y && my < POPUP_Y + POPUP_H) {
            /* 1. Top Hand Selector Tabs (R.HAND vs L.HAND) */
            if (my >= POPUP_Y + 3 && my < POPUP_Y + 17) {
                if (mx < POPUP_X + 74) {
                    u->active_hand = 0;
                    isomap_sync_unit_inventory(u);
                    sfx_click();
                    return 1;
                } else {
                    u->active_hand = 1;
                    isomap_sync_unit_inventory(u);
                    sfx_click();
                    return 1;
                }
            }

            /* 2. Action Rows (row 0..4) */
            if (my >= POPUP_Y + 20) {
                int row = (my - (POPUP_Y + 20)) / 15;
                if (item_is_grenade(hand_item)) {
                    if (row == 0) {
                        int t = (mx - (POPUP_X + 37)) / 15;
                        if (t >= 0 && t <= 6) {
                            u->slot_prime[hand_slot] = (int8_t)t;
                            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                     t == 0 ? "GRENADE PRIMED: 0 TURNS (IMPACT/DEATH)!"
                                            : "GRENADE PRIMED: %d TURN FUSE!", t);
                            sfx_click();
                        }
                        return 1;
                    } else if (row == 2) {
                        if (u->slot_prime[hand_slot] >= 0) {
                            u->grenade_id            = hand_item;
                            g_battle.throw_hand_slot = hand_slot;
                            g_battle.fire_mode       = 4;
                            g_battle.weapon_menu_open = 0;
                            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                     "THROW PRIMED GRENADE (L-CLICK TARGET)");
                            sfx_click();
                        } else {
                            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                     "SELECT FUSE TURNS (0-6) ABOVE FIRST!");
                            sfx_beep(200, 50);
                        }
                        return 1;
                    } else if (row == 3) {
                        if (u->slot_prime[hand_slot] >= 0) {
                            int pr = u->slot_prime[hand_slot];
                            if (u->tu >= 2) u->tu -= 2;
                            set_unit_slot_item_primed(u, hand_slot, ITEM_NONE, 0, -1);
                            isomap_drop_ground_item_primed(u->x, u->y, u->z, hand_item, pr);
                            g_battle.weapon_menu_open = 0;
                            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                     "DROPPED PRIMED GRENADE (%d TRN) ON TILE!", pr);
                            sfx_click();
                        } else {
                            snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                     "SELECT FUSE TURNS (0-6) ABOVE FIRST!");
                            sfx_beep(200, 50);
                        }
                        return 1;
                    } else if (row == 4) {
                        g_battle.weapon_menu_open = 0;
                        g_battle.inv_modal_open   = 1;
                        sfx_click();
                        return 1;
                    }
                    return 1;
                } else if (item_is_weapon(hand_item)) {
                    if (row >= 0 && row <= 2) {
                        g_battle.fire_mode        = row + 1;
                        g_battle.weapon_menu_open = 0;
                        snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                 row == 0 ? "AIMING: SNAP SHOT (L-CLICK TARGET)" :
                                 row == 1 ? "AIMING: AIMED SHOT (L-CLICK TARGET)" :
                                            "AIMING: AUTO BURST (L-CLICK TARGET)");
                        sfx_click();
                        return 1;
                    } else if (row == 3) {
                        set_unit_slot_item_primed(u, hand_slot, ITEM_NONE, 0, -1);
                        g_battle.throw_item_id    = hand_item;
                        g_battle.throw_prime      = -1;
                        g_battle.fire_mode        = 6;
                        g_battle.weapon_menu_open = 0;
                        snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                 "THROW WEAPON: L-CLICK TARGET TILE!");
                        sfx_click();
                        return 1;
                    } else if (row == 4) {
                        g_battle.weapon_menu_open = 0;
                        g_battle.inv_modal_open   = 1;
                        sfx_click();
                        return 1;
                    }
                } else {
                    if (row == 0 && hand_item == ITEM_FLARE) {
                        g_battle.fire_mode        = 5;
                        g_battle.weapon_menu_open = 0;
                        snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                 "THROW ELECTRO-FLARE (L-CLICK TILE)");
                        sfx_click();
                        return 1;
                    } else if (row == 0 && hand_item >= 0) {
                        set_unit_slot_item_primed(u, hand_slot, ITEM_NONE, 0, -1);
                        g_battle.throw_item_id    = hand_item;
                        g_battle.throw_prime      = -1;
                        g_battle.fire_mode        = 6;
                        g_battle.weapon_menu_open = 0;
                        snprintf(g_battle.status_msg, sizeof(g_battle.status_msg),
                                 "THROW ITEM: L-CLICK TARGET TILE!");
                        sfx_click();
                        return 1;
                    } else if (row == 1) {
                        u->active_hand = !u->active_hand;
                        isomap_sync_unit_inventory(u);
                        sfx_click();
                        return 1;
                    } else if (row >= 2) {
                        g_battle.weapon_menu_open = 0;
                        g_battle.inv_modal_open   = 1;
                        sfx_click();
                        return 1;
                    }
                }
            }
            return 1;
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
