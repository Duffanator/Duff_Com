#include "base_ui.h"
#include "campaign.h"
#include "items.h"
#include "gfx_assets.h"
#include "vga.h"

#include "input.h"
#include "sound.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    const char *short_name;
    const char *full_name;
    const char *hover_tip;
    int cash_cost;
    int alloy_cost;
} FacilityDef;

static const FacilityDef fac_defs[FAC_COUNT] = {
    { FAC_EMPTY_LOT,    "YARD LOT", "GRASS YARD LOT",     "EMPTY YARD: CLICK IF CONNECTED TO BUILD",      0,    0 },
    { FAC_MAIN_HOUSE,   "HOUSE HQ", "COLONIAL HOUSE HQ",  "MAIN COLONIAL HOUSE: HQ COMMAND (+6 MAX TU)",  0,    0 },
    { FAC_GARAGE,       "ARV.GARG", "VEHICLE GARAGE",     "ARMORED BUS GARAGE: CLICK TO EQUIP VEHICLE",   0,    0 },
    { FAC_RADAR,        "RADAR",    "HOMEMADE RADAR",     "HOMEMADE RADAR DISH: TRACKS VALLEY UFOS",      0,    0 },
    { FAC_LAB,          "HOME LAB", "HOMEMADE LAB/WKSHP", "STARTER LAB & WORKSHOP: CLICK FOR RESEARCH",   0,    0 },
    { FAC_STORES,       "STORES",   "EQUIPMENT STORES",   "EQUIPMENT STORES: +40 BASE STORAGE SPACE",  1500,    4 },
    { FAC_QUARTERS,     "QUARTERS", "LIVING QUARTERS",    "LIVING QUARTERS: +6 STAFF CAP & +15 MAX HP",2000,    5 },
    { FAC_CONTAINMENT,  "CONTAIN",  "ALIEN CONTAINMENT",  "ALIEN CONTAINMENT: HOLDS LIVE CAPTURED ALIENS",2200, 6 },
    { FAC_UPG_LAB,      "UPG.LAB",  "UPGRADED LAB",       "UPGRADED LAB: ADDS +2 SCIENTISTS TO HQ",    3000,    8 },
    { FAC_UPG_WORKSHOP, "UPG.WORK", "UPGRADED WORKSHOP",  "UPGRADED WORKSHOP: +2 ENG & -25% MFG COST", 2800,    8 },
    { FAC_MISSILE_DEF,  "SAM.DEF",  "MISSILE DEFENSE",    "SAM BATTERY: SHOOT DOWN FLYING UFOS!",      3200,   10 },
    { FAC_OFFENSE_SYS,  "PLSM.AA",  "PLASMA OFFENSE SYS", "HEAVY UFO OFFENSE: 2X INTERCEPT DAMAGE!",   4800,   15 },
    { FAC_PSI_LAB,      "PSI-LAB",  "PSIONICS LAB",       "PSIONICS LAB: TRAIN OPERATIVES IN PSIONICS",7500,   12 }
};

static int base_has_facility(int fid) {
    int r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            if (g_camp.bunker_grid[r][c] == fid) return 1;
        }
    }
    return 0;
}

int base_ui_is_lot_connected(int r, int c) {
    static const int dr[4] = { -1, 1, 0, 0 };
    static const int dc[4] = {  0, 0,-1, 1 };
    int d;
    if (r < 0 || r >= 3 || c < 0 || c >= 4) return 0;
    for (d = 0; d < 4; d++) {
        int nr = r + dr[d], nc = c + dc[d];
        if (nr >= 0 && nr < 3 && nc >= 0 && nc < 4) {
            if (g_camp.bunker_grid[nr][nc] != FAC_EMPTY_LOT) return 1;
        }
    }
    return 0;
}

static void draw_facility_tile(int r, int c, int bx, int by, int fid, int hov) {
    int conn = (fid == FAC_EMPTY_LOT) ? base_ui_is_lot_connected(r, c) : 1;
    uint8_t bdr = hov ? 96 : (fid == FAC_EMPTY_LOT ? (conn ? 20 : 26) : 163);

    /* Lush Coopersburg Grass Lawn Base (58x41) */
    vga_rect_bevel(bx, by, 58, 41, 23, bdr, 27);
    if (has_base_bmp) {
        vga_blit_ui(bx + 2, by + 2, BASE_TILE_W, BASE_TILE_H, spr_base_tiles[fid % FAC_COUNT]);
        if (fid != FAC_EMPTY_LOT) {
            if (r > 0 && g_camp.bunker_grid[r - 1][c] != FAC_EMPTY_LOT)
                vga_rect_fill(bx + 25, by, 8, 5, 7);
            if (r < 2 && g_camp.bunker_grid[r + 1][c] != FAC_EMPTY_LOT)
                vga_rect_fill(bx + 25, by + 35, 8, 6, 7);
            if (c > 0 && g_camp.bunker_grid[r][c - 1] != FAC_EMPTY_LOT)
                vga_rect_fill(bx, by + 17, 6, 6, 7);
            if (c < 3 && g_camp.bunker_grid[r][c + 1] != FAC_EMPTY_LOT)
                vga_rect_fill(bx + 52, by + 17, 6, 6, 7);
            vga_rect_fill(bx + 2, by + 31, 54, 8, 15);
            vga_print(bx + 4, by + 32, fac_defs[fid % FAC_COUNT].short_name, hov ? 96 : 80);
        } else if (conn) {
            vga_print_shadow(bx + 7, by + 16, hov ? "[+BUILD]" : "+BUILD+", hov ? 96 : 18, 15);
        } else {
            vga_print(bx + 10, by + 16, "YARD LOT", 26);
        }
        return;
    }
    vga_rect_fill(bx + 2, by + 2, 54, 37, (r + c) & 1 ? 23 : 24);


    /* Gravel Walkways connecting orthogonally adjacent built structures */
    if (fid != FAC_EMPTY_LOT) {
        if (r > 0 && g_camp.bunker_grid[r - 1][c] != FAC_EMPTY_LOT)
            vga_rect_fill(bx + 25, by, 8, 6, 7);
        if (r < 2 && g_camp.bunker_grid[r + 1][c] != FAC_EMPTY_LOT)
            vga_rect_fill(bx + 25, by + 35, 8, 6, 7);
        if (c > 0 && g_camp.bunker_grid[r][c - 1] != FAC_EMPTY_LOT)
            vga_rect_fill(bx, by + 17, 8, 6, 7);
        if (c < 3 && g_camp.bunker_grid[r][c + 1] != FAC_EMPTY_LOT)
            vga_rect_fill(bx + 50, by + 17, 8, 6, 7);
    }

    switch (fid) {
        case FAC_EMPTY_LOT:
            vga_rect_fill(bx + 8,  by + 10, 6, 5, 20);
            vga_rect_fill(bx + 42, by + 22, 7, 5, 20);
            if (conn) {
                vga_print_shadow(bx + 7, by + 16, hov ? "[+BUILD]" : "+BUILD+", hov ? 96 : 18, 15);
            } else {
                vga_print(bx + 10, by + 16, "YARD LOT", 26);
            }
            return;

        case FAC_MAIN_HOUSE:
            vga_rect_fill(bx + 42, by + 3, 5, 10, 36);
            vga_rect_fill(bx + 10, by + 5, 38, 6, 37);
            vga_rect_fill(bx + 7,  by + 10,44, 5, 35);
            vga_rect_fill(bx + 10, by + 15,38, 16, 1);
            vga_rect_fill(bx + 13, by + 17, 5, 5, 68);
            vga_rect_fill(bx + 26, by + 17, 5, 5, 96);
            vga_rect_fill(bx + 39, by + 17, 5, 5, 68);
            vga_rect_fill(bx + 26, by + 24, 6, 7, 54);
            vga_hline(bx + 8, bx + 49, by + 31, 3);
            break;

        case FAC_GARAGE:
            vga_rect_fill(bx + 8,  by + 7, 42, 6, 54);
            vga_rect_fill(bx + 10, by + 13,38, 18, 7);
            vga_rect_fill(bx + 15, by + 16,28, 15, 14);
            vga_rect_fill(bx + 18, by + 18,22, 11, 21);
            vga_rect_fill(bx + 20, by + 20,18, 4,  80);
            vga_pset(bx + 20, by + 26, 96); vga_pset(bx + 37, by + 26, 96);
            break;

        case FAC_RADAR:
            vga_rect_fill(bx + 18, by + 20, 22, 10, 8);
            vga_line(bx + 29, by + 20, bx + 24, by + 11, 5);
            vga_line(bx + 29, by + 20, bx + 34, by + 11, 5);
            vga_rect_fill(bx + 20, by + 6, 18, 6, 2);
            vga_hline(bx + 22, bx + 35, by + 5, 80);
            vga_pset(bx + 29, by + 3, (g_camp.radar_tick & 8) ? 96 : 32);
            break;

        case FAC_LAB:
        case FAC_UPG_LAB:
            vga_rect_fill(bx + 9,  by + 8,  40, 5, (fid == FAC_UPG_LAB) ? 66 : 55);
            vga_rect_fill(bx + 11, by + 13, 36, 17, (fid == FAC_UPG_LAB) ? 2 : 6);
            vga_rect_fill(bx + 15, by + 16, 11, 8, 80);
            vga_rect_fill(bx + 31, by + 16, 11, 8, 144);
            break;

        case FAC_STORES:
            vga_rect_fill(bx + 9,  by + 10, 40, 20, 9);
            vga_hline(bx + 11, bx + 46, by + 9, 7);
            vga_rect_fill(bx + 14, by + 18, 9, 9, 56);
            vga_rect_fill(bx + 25, by + 18, 9, 9, 58);
            vga_rect_fill(bx + 36, by + 18, 8, 9, 98);
            break;

        case FAC_QUARTERS:
            vga_rect_fill(bx + 9,  by + 7,  40, 5, 36);
            vga_rect_fill(bx + 11, by + 12, 36, 18, 2);
            vga_rect_fill(bx + 26, by + 15, 6, 2, 32);
            vga_rect_fill(bx + 28, by + 13, 2, 6, 32);
            vga_rect_fill(bx + 15, by + 20, 7, 5, 96);
            vga_rect_fill(bx + 35, by + 20, 7, 5, 96);
            break;

        case FAC_CONTAINMENT:
            vga_rect_fill(bx + 12, by + 10, 34, 20, 12);
            vga_rect_fill(bx + 18, by + 8,  22, 18, 18);
            vga_rect_fill(bx + 22, by + 10, 14, 14, 144);
            vga_rect_fill(bx + 27, by + 13, 4,  6,  14);
            break;

        case FAC_UPG_WORKSHOP:
            vga_rect_fill(bx + 40, by + 3,  5, 12, 10);
            vga_rect_fill(bx + 9,  by + 11, 40, 19, 8);
            vga_rect_fill(bx + 16, by + 17, 14, 11, 113);
            vga_rect_fill(bx + 34, by + 18, 10, 10, 80);
            break;

        case FAC_MISSILE_DEF:
            vga_rect_fill(bx + 10, by + 22, 38, 8, 56);
            vga_line(bx + 18, by + 24, bx + 28, by + 7, 0);
            vga_line(bx + 19, by + 24, bx + 29, by + 7, 2);
            vga_line(bx + 30, by + 24, bx + 40, by + 7, 0);
            vga_line(bx + 31, by + 24, bx + 41, by + 7, 2);
            vga_pset(bx + 28, by + 7, 32); vga_pset(bx + 40, by + 7, 32);
            break;

        case FAC_OFFENSE_SYS:
            vga_rect_fill(bx + 16, by + 16, 26, 14, 9);
            vga_rect_fill(bx + 20, by + 10, 18, 8,  20);
            vga_line(bx + 25, by + 10, bx + 25, by + 3, 144);
            vga_line(bx + 33, by + 10, bx + 33, by + 3, 144);
            vga_rect_fill(bx + 24, by + 2, 11, 3, 96);
            break;

        case FAC_PSI_LAB:
            vga_rect_fill(bx + 10, by + 14, 38, 16, 134);
            vga_rect_fill(bx + 14, by + 8,  30, 12, 131);
            vga_rect_fill(bx + 20, by + 6,  18, 14, 128);
            vga_rect_fill(bx + 24, by + 10, 10, 8,  80);
            vga_pset(bx + 29, by + 13, 96);
            break;
    }

    vga_rect_fill(bx + 2, by + 31, 54, 8, 15);
    vga_print(bx + 4, by + 32, fac_defs[fid % FAC_COUNT].short_name, hov ? 96 : 80);
}

static void draw_close_btn(int mx, int my) {
    int hov = (mx >= 216 && mx < 238 && my >= 7 && my < 19);
    vga_rect_bevel(216, 7, 22, 12, hov ? 34 : 36, 96, 45);
    vga_print_shadow(222, 9, "X", 96, 15);
}

static void render_construct_modal(int mx, int my) {
    int i;
    char buf[64];
    vga_rect_bevel(6, 5, 236, 158, 76, 96, 79);
    vga_rect_fill(9, 8, 230, 152, 14);
    vga_print_shadow(12, 10, "BUILD BASE ADDON (LOT CONNECTED)", 96, 15);
    draw_close_btn(mx, my);

    for (i = 0; i < 8; i++) {
        int fid = FAC_STORES + i;
        const FacilityDef *fd = &fac_defs[fid];
        int ry = 21 + i * 17;
        int unlocked = (fid != FAC_PSI_LAB) || g_camp.tech_done[TECH_PSI_LAB];
        int can_afford = unlocked && (g_camp.money >= fd->cash_cost && g_camp.alloys >= fd->alloy_cost);
        int hov = (mx >= 11 && mx < 235 && my >= ry && my < ry + 16);

        vga_rect_bevel(11, ry, 224, 16, hov ? 65 : 74, (hov && can_afford) ? 96 : 68, 79);
        vga_print_shadow(15, ry + 1, fd->full_name, can_afford ? 96 : 8, 15);
        if (unlocked) {
            snprintf(buf, sizeof(buf), "$%d + %dA", fd->cash_cost, fd->alloy_cost);
            vga_print(152, ry + 1, buf, can_afford ? 144 : 33);
        } else {
            vga_print_sm(152, ry + 2, "[NEED PSI TECH]", 33);
        }
        vga_print_sm(15, ry + 9, fd->hover_tip + 18, 80);
    }
}


static void draw_scroll_btns(int mx, int my) {
    int hov_up = (mx >= 170 && mx < 191 && my >= 7 && my < 19);
    int hov_dn = (mx >= 193 && mx < 214 && my >= 7 && my < 19);
    vga_rect_bevel(170, 7, 21, 12, hov_up ? 65 : 68, hov_up ? 96 : 162, 79);
    vga_print_shadow(175, 9, "UP", hov_up ? 96 : 1, 15);
    vga_rect_bevel(193, 7, 21, 12, hov_dn ? 65 : 68, hov_dn ? 96 : 162, 79);
    vga_print_shadow(198, 9, "DN", hov_dn ? 96 : 1, 15);
}

static int build_discovered_items(int *out_list) {
    int i, n = 0;
    for (i = 0; i < ITEM_COUNT; i++) {
        if (campaign_is_item_discovered(i)) {
            out_list[n++] = i;
        }
    }
    return n;
}

static int build_visible_techs(int *out_list) {
    int t, n = 0;
    for (t = 0; t < TECH_COUNT; t++) {
        /* Only show uncompleted topics that are currently eligible to be researched! */
        if (campaign_can_research_tech(t)) {
            out_list[n++] = t;
        }
    }
    return n;
}

static int build_researched_mfg_items(int *out_list) {
    int i, n = 0;
    for (i = 0; i < ITEM_COUNT; i++) {
        const ItemDef *d = item_get_def(i);
        if (d->mfg_hours > 0 && campaign_is_item_researched(i)) {
            out_list[n++] = i;
        }
    }
    return n;
}

static void render_stores_arv_modal(int mx, int my) {
    int vis[ITEM_COUNT];
    int n_vis = build_discovered_items(vis);
    int i, rows = (n_vis < 6) ? n_vis : 6;
    int s_used = campaign_store_used_space(), s_max = campaign_store_max_space();
    int a_used = campaign_arv_used_space(),   a_max = campaign_arv_max_space();
    int base_idx = (n_vis > 0) ? (g_camp.store_scroll % n_vis) : 0;
    char buf[64];

    vga_rect_bevel(6, 5, 236, 158, 76, 96, 79);
    vga_rect_fill(9, 8, 230, 152, 14);
    vga_print_shadow(12, 10, "STORES & ARV BUS CARGO", 96, 15);
    draw_scroll_btns(mx, my);
    draw_close_btn(mx, my);

    snprintf(buf, sizeof(buf), "BASE STORES:%d/%d  ARV BUS:%d/%d",
             s_used, s_max, a_used, a_max);
    vga_print(12, 22, buf, 80);

    for (i = 0; i < rows; i++) {
        int id = vis[(base_idx + i) % n_vis];
        const ItemDef *d = item_get_def(id);
        int ry = 32 + i * 21;
        int can_use = campaign_is_item_researched(id) && (d->category <= ICAT_UTILITY);
        int hov_to_bus = (mx >= 166 && mx < 199 && my >= ry + 2 && my < ry + 18);
        int hov_to_str = (mx >= 201 && mx < 234 && my >= ry + 2 && my < ry + 18);

        vga_rect_bevel(11, ry, 226, 20, 74, 68, 79);
        vga_blit_ui(13, ry + 2, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[id]);
        vga_print_shadow(47, ry + 2, d->short_name, can_use ? 96 : 80, 15);
        snprintf(buf, sizeof(buf), "STORES:%d  ARV BUS:%d", g_camp.stores[id], g_camp.arv_cargo[id]);
        vga_print_sm(47, ry + 12, buf, 18);

        if (can_use) {
            vga_rect_bevel(166, ry + 2, 33, 16, hov_to_bus ? 65 : 68, hov_to_bus ? 96 : 162, 79);
            vga_print(169, ry + 6, ">BUS", g_camp.stores[id] > 0 ? 96 : 8);

            vga_rect_bevel(201, ry + 2, 33, 16, hov_to_str ? 65 : 68, hov_to_str ? 96 : 162, 79);
            vga_print(204, ry + 6, "<STR", g_camp.arv_cargo[id] > 0 ? 80 : 8);
        } else {
            vga_print(168, ry + 6, d->category >= ICAT_ARTIFACT ? "VAULT" : "NEED RES", 8);
        }
    }
}

static void render_store_buysell_modal(int mx, int my) {
    int vis[ITEM_COUNT];
    int n_vis = build_discovered_items(vis);
    int i, rows = (n_vis < 5) ? n_vis : 5;
    int s_used = campaign_store_used_space(), s_max = campaign_store_max_space();
    int p_used = campaign_total_personnel(),  p_max = campaign_max_personnel();
    int base_idx = (n_vis > 0) ? (g_camp.store_scroll % n_vis) : 0;
    int hov_op  = (mx >= 11  && mx < 85  && my >= 141 && my < 157);
    int hov_sci = (mx >= 86  && mx < 160 && my >= 141 && my < 157);
    int hov_eng = (mx >= 161 && mx < 235 && my >= 141 && my < 157);
    char buf[64];

    vga_rect_bevel(6, 5, 236, 158, 76, 96, 79);
    vga_rect_fill(9, 8, 230, 152, 14);
    vga_print_shadow(12, 10, "SUPPLY STORE & RECRUITMENT", 96, 15);
    draw_scroll_btns(mx, my);
    draw_close_btn(mx, my);

    snprintf(buf, sizeof(buf), "$%d SPC:%d/%d QTR:%d/%d S:%d E:%d",
             g_camp.money, s_used, s_max, p_used, p_max, g_camp.scientists, g_camp.engineers);
    vga_print(11, 22, buf, 144);

    for (i = 0; i < rows; i++) {
        int id = vis[(base_idx + i) % n_vis];
        const ItemDef *d = item_get_def(id);
        int ry = 32 + i * 21;
        int can_buy = (d->buy_price > 0) && campaign_is_item_researched(id);
        int hov_buy = (mx >= 152 && mx < 192 && my >= ry + 2 && my < ry + 18);
        int hov_sel = (mx >= 194 && mx < 234 && my >= ry + 2 && my < ry + 18);

        vga_rect_bevel(11, ry, 226, 20, 74, 68, 79);
        vga_blit_ui(13, ry + 2, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[id]);
        vga_print_shadow(47, ry + 2, d->short_name, can_buy ? 96 : 80, 15);
        if (g_camp.store_transit_qty[id] > 0) {
            snprintf(buf, sizeof(buf), "OWN:%d  TRANSIT:%d (%dH)",
                     g_camp.stores[id], g_camp.store_transit_qty[id], g_camp.store_transit_hrs[id]);
            vga_print_sm(47, ry + 12, buf, 96);
        } else {
            snprintf(buf, sizeof(buf), "IN STORES:%d", g_camp.stores[id]);
            vga_print_sm(47, ry + 12, buf, 18);
        }

        if (can_buy) {
            vga_rect_bevel(152, ry + 2, 40, 16, hov_buy ? 34 : 36, hov_buy ? 96 : 45, 79);
            snprintf(buf, sizeof(buf), "+$%d", d->buy_price);
            vga_print(154, ry + 6, buf, g_camp.money >= d->buy_price ? 96 : 8);
        } else {
            vga_print(150, ry + 6, "SELL ONLY", 8);
        }

        vga_rect_bevel(194, ry + 2, 40, 16, hov_sel ? 65 : 68, hov_sel ? 96 : 162, 79);
        snprintf(buf, sizeof(buf), "-$%d", d->sell_price);
        vga_print(196, ry + 6, buf, g_camp.stores[id] > 0 ? 144 : 8);
    }

    vga_rect_bevel(11,  141, 74, 16, hov_op  ? 65 : 68, hov_op  ? 96 : 162, 79);
    vga_print_shadow(14, 145, "+AGENT$1500",
                     (g_camp.money >= 1500 && p_used < p_max) ? 96 : 8, 15);

    vga_rect_bevel(86,  141, 74, 16, hov_sci ? 65 : 68, hov_sci ? 96 : 162, 79);
    vga_print_shadow(92, 145, "+SCI $2000",
                     (g_camp.money >= 2000 && p_used < p_max) ? 96 : 8, 15);

    vga_rect_bevel(161, 141, 74, 16, hov_eng ? 65 : 68, hov_eng ? 96 : 162, 79);
    vga_print_shadow(167, 145, "+ENG $1800",
                     (g_camp.money >= 1800 && p_used < p_max) ? 96 : 8, 15);
}


static void render_lab_modal(int mx, int my) {
    static const char *sub_lbl[3] = { "RESEARCH", "RES.LOG", "WORKSHOP" };
    int t, i;
    char buf[64];

    vga_rect_bevel(6, 5, 236, 158, 76, 96, 79);
    vga_rect_fill(9, 8, 230, 152, 14);

    for (t = 0; t < 3; t++) {
        int bx = 11 + t * 53;
        int act = (g_camp.lab_subtab == t);
        int hov = (mx >= bx && mx < bx + 51 && my >= 7 && my < 19);
        vga_rect_bevel(bx, 7, 51, 12, act ? 65 : (hov ? 68 : 167),
                       (act || hov) ? 96 : 162, 79);
        vga_print_shadow(bx + 4, 9, sub_lbl[t], (act || hov) ? 96 : 1, 15);
    }
    draw_scroll_btns(mx, my);
    draw_close_btn(mx, my);

    if (g_camp.lab_subtab == 0) {
        int vis_t[TECH_COUNT];
        int n_vis = build_visible_techs(vis_t);
        int rows  = (n_vis < 6) ? n_vis : 6;
        int base_t = (n_vis > 0) ? (g_camp.store_scroll % n_vis) : 0;
        int free_sci = campaign_free_scientists();

        snprintf(buf, sizeof(buf), "IDLE SCI:%d/%d", free_sci, g_camp.scientists);
        vga_print(11, 22, buf, free_sci > 0 ? 144 : 80);
        vga_print_sm(112, 23, "CLICK [-]/[+] TO ASSIGN", 80);

        if (n_vis == 0) {
            vga_print_shadow(16, 66, "NO ACTIVE RESEARCH PROJECTS AVAILABLE.", 96, 15);
            vga_print_sm(16, 82, "RECOVER NEW ALIEN ARTIFACTS, CORPSES,", 80);
            vga_print_sm(16, 92, "OR CAPTURE LIVE ALIENS ON MISSIONS!", 80);
        }

        for (i = 0; i < rows; i++) {
            int tid = vis_t[(base_t + i) % n_vis];
            const TechDef *td = tech_get_def(tid);
            int ry = 33 + i * 20;
            int sci_n = g_camp.tech_scientists[tid];
            int pct   = (td->research_cost > 0) ? ((g_camp.tech_prog[tid] * 100) / td->research_cost) : 0;
            int hov_dn = (mx >= 176 && mx < 194 && my >= ry + 2 && my < ry + 17);
            int hov_up = (mx >= 214 && mx < 233 && my >= ry + 2 && my < ry + 17);
            if (pct > 99) pct = 99;

            vga_rect_bevel(11, ry, 224, 19, (sci_n > 0) ? 65 : 74,
                           (sci_n > 0) ? 96 : 68, 79);
            vga_print_shadow(14, ry + 2, td->title, (sci_n > 0) ? 96 : 80, 15);
            snprintf(buf, sizeof(buf), "%2d%%", pct);
            vga_print_shadow(148, ry + 2, buf, (sci_n > 0) ? 144 : 18, 15);
            vga_print_sm(14, ry + 11, td->unlock_summary, (sci_n > 0) ? 2 : 80);

            /* Down / Minus Assignment Arrow Button */
            vga_rect_bevel(176, ry + 2, 18, 15, hov_dn ? 65 : 68, hov_dn ? 96 : 162, 79);
            vga_print_shadow(182, ry + 5, "-", sci_n > 0 ? 96 : 8, 15);

            /* Assigned Scientist Count */
            snprintf(buf, sizeof(buf), "%d", sci_n);
            vga_print_shadow(201, ry + 5, buf, sci_n > 0 ? 144 : 8, 15);

            /* Up / Plus Assignment Arrow Button */
            vga_rect_bevel(214, ry + 2, 19, 15, hov_up ? 34 : 36, hov_up ? 96 : 45, 79);
            vga_print_shadow(221, ry + 5, "+", free_sci > 0 ? 96 : 8, 15);
        }
    } else if (g_camp.lab_subtab == 1) {
        int done_t[TECH_COUNT];
        int n_done = 0;
        for (i = 0; i < TECH_COUNT; i++) {
            if (g_camp.tech_done[i]) done_t[n_done++] = i;
        }
        vga_rect_bevel(11, 34, 224, 122, 13, 80, 15);
        if (n_done > 0) {
            int idx = g_camp.log_sel % n_done;
            int tid = done_t[idx];
            const TechDef *td = tech_get_def(tid);
            snprintf(buf, sizeof(buf), "RESEARCH LOG #%02d/%02d (UP/DN TO CYCLE)",
                     idx + 1, n_done);
            vga_print_sm(11, 23, buf, 80);
            vga_print_shadow(16, 40, td->title, 96, 15);
            vga_print_sm(16, 52, "STATUS: RESEARCH COMPLETED", 144);
            vga_print_sm(16, 62, td->unlock_summary, 80);
            vga_hline(16, 228, 72, 68);
            vga_print_sm(16, 78, td->log_entry[0], 18);
            vga_print_sm(16, 88, td->log_entry[1], 18);
            vga_print_sm(16, 98, td->log_entry[2], 18);
        } else {
            vga_print(11, 22, "RESEARCH LOG (0 COMPLETED)", 80);
            vga_print(16, 64, "NO COMPLETED RESEARCH REPORTS YET.", 96);
            vga_print_sm(16, 84, "ASSIGN SCIENTISTS IN THE RESEARCH TAB", 2);
            vga_print_sm(16, 94, "AND ADVANCE TIME ON THE VALLEY RADAR.", 2);
        }
    } else {
        int mfg_list[ITEM_COUNT];
        int n_mfg = build_researched_mfg_items(mfg_list);
        int rows  = (n_mfg < 5) ? n_mfg : 5;
        int base_m = (n_mfg > 0) ? (g_camp.store_scroll % n_mfg) : 0;
        int hov_tur = (mx >= 11 && mx < 235 && my >= 138 && my < 156);
        int has_upg_wk = base_has_facility(FAC_UPG_WORKSHOP);

        snprintf(buf, sizeof(buf), "ENG:%d  $%d  ALLOY:%d  ELER:%d",
                 g_camp.engineers, g_camp.money, g_camp.alloys, g_camp.elerium);
        vga_print(11, 22, buf, 144);

        if (n_mfg == 0) {
            vga_print(16, 68, "NO MANUFACTURABLE TECH RESEARCHED YET.", 80);
            vga_print_sm(16, 82, "COMPLETE LASER OR PLASMA RESEARCH FIRST!", 2);
        }
        for (i = 0; i < rows; i++) {
            int id = mfg_list[(base_m + i) % n_mfg];
            const ItemDef *d = item_get_def(id);
            int ry = 32 + i * 21;
            int cash_cost = has_upg_wk ? ((d->mfg_cash * 3) / 4) : d->mfg_cash;
            int can_b = (g_camp.money >= cash_cost) &&
                        (g_camp.alloys >= d->mfg_alloys) && (g_camp.elerium >= d->mfg_elerium);
            int hov = (mx >= 176 && mx < 234 && my >= ry + 2 && my < ry + 18);

            vga_rect_bevel(11, ry, 226, 20, 74, 68, 79);
            vga_blit_ui(13, ry + 2, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[id]);
            vga_print_shadow(47, ry + 2, d->short_name, 96, 15);
            if (g_camp.mfg_queue[id] > 0) {
                int pct = (d->mfg_hours > 0) ? ((g_camp.mfg_prog[id] * 100) / d->mfg_hours) : 0;
                snprintf(buf, sizeof(buf), "Q:%d (%2d%%) $%d %dA %dE",
                         g_camp.mfg_queue[id], pct, cash_cost, d->mfg_alloys, d->mfg_elerium);
                vga_print_sm(47, ry + 12, buf, 144);
            } else {
                snprintf(buf, sizeof(buf), "COST: $%d  %dA  %dE", cash_cost, d->mfg_alloys, d->mfg_elerium);
                vga_print_sm(47, ry + 12, buf, can_b ? 18 : 33);
            }

            vga_rect_bevel(176, ry + 2, 58, 16, (hov && can_b) ? 34 : 68,
                           (hov && can_b) ? 96 : 162, 79);
            vga_print(182, ry + 6, "QUEUE+1", can_b ? 96 : 8);
        }

        if (g_camp.tech_done[TECH_ARV_TURRET] || g_camp.arv_turret_level > 0) {
            vga_rect_bevel(11, 138, 224, 18, hov_tur ? 65 : 68, hov_tur ? 96 : 162, 79);
            if (g_camp.arv_turret_level == 0) {
                int ok = g_camp.money >= 1500 && g_camp.alloys >= 6;
                vga_print_shadow(15, 143, "BUILD ARV ROOF TURRET MK1 ($1500+6A)", ok ? 96 : 8, 15);
            } else if (g_camp.arv_turret_level == 1 && g_camp.tech_done[TECH_ARV_TURRET_MK2]) {
                int ok = g_camp.money >= 2500 && g_camp.alloys >= 10;
                vga_print_shadow(15, 143, "UPGRADE ARV TURRET MK2 ($2500+10A)", ok ? 96 : 8, 15);
            } else {
                vga_print_shadow(15, 143,
                                 g_camp.arv_turret_level >= 2 ? "ARV ROOF TURRET MK2 INSTALLED!"
                                                              : "ARV ROOF TURRET MK1 INSTALLED!",
                                 144, 15);
            }
        }
    }

}

void base_ui_render(void) {
    static const char *top_btns[4] = { "STORES", "STORE $", "LAB/MFG", "ARV BUS" };
    int b, r, c;
    int mx = g_input.mx, my = g_input.my;
    const char *hover_msg = "CLICK A CONNECTED LOT TO BUILD OR FACILITY TO OPEN";

    vga_rect_bevel(4, 4, 240, 162, 22, 20, 26);

    /* Top Sub-Menu Action Bar (x=6..241, y=6..21) */
    for (b = 0; b < 4; b++) {
        int bx = 6 + b * 59;
        int hov = (g_camp.base_modal == 0 && mx >= bx && mx < bx + 58 && my >= 6 && my < 21);
        vga_rect_bevel(bx, 6, 58, 15, hov ? 65 : 68, hov ? 96 : 162, 172);
        vga_print_shadow(bx + 7, 10, top_btns[b], hov ? 96 : 1, 15);
        if (hov) {
            hover_msg = (b == 0) ? "VIEW BASE STORES & TRANSFER GEAR TO ARV BUS" :
                        (b == 1) ? "BUY RESEARCHED WEAPONS/AMMO OR SELL LOOT" :
                        (b == 2) ? "ASSIGN SCIENTISTS, VIEW RES LOG & WORKSHOP" :
                                   "LOAD WEAPONS & AMMO INTO ARMORED BUS";
        }
    }

    /* 3x4 Graphical Colonial House Compound Grid (x=6..241, y=23..149) */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            int bx = 6 + c * 59;
            int by = 23 + r * 42;
            int fid = g_camp.bunker_grid[r][c] % FAC_COUNT;
            int hov = (g_camp.base_modal == 0 &&
                       mx >= bx && mx < bx + 58 && my >= by && my < by + 41);
            draw_facility_tile(r, c, bx, by, fid, hov);
            if (hov) {
                if (fid == FAC_EMPTY_LOT) {
                    hover_msg = base_ui_is_lot_connected(r, c)
                                ? "CONNECTED YARD LOT: CLICK TO CONSTRUCT ADDON"
                                : "DISCONNECTED LOT: MUST CONNECT TO BASE TILE";
                } else {
                    hover_msg = fac_defs[fid].hover_tip;
                }
            }
        }
    }

    /* Bottom Mouse-Over Tooltip Strip (y=150..163) */
    if (g_camp.base_modal == 0) {
        vga_rect_fill(6, 150, 236, 13, 15);
        vga_hline(6, 241, 150, 25);
        vga_print_sm(9, 154, hover_msg, 96);
    }

    /* Render Active Base Sub-Modal if Open */
    if (g_camp.base_modal == 1)      render_construct_modal(mx, my);
    else if (g_camp.base_modal == 2) render_stores_arv_modal(mx, my);
    else if (g_camp.base_modal == 3) render_store_buysell_modal(mx, my);
    else if (g_camp.base_modal == 4) render_lab_modal(mx, my);
}

static void sync_resource_counters(void) {
    g_camp.elerium       = g_camp.stores[ITEM_ELERIUM];
    g_camp.alloys        = g_camp.stores[ITEM_ALLOYS];
    g_camp.nav_units     = g_camp.stores[ITEM_UFO_NAV];
    g_camp.cryo_pods     = g_camp.stores[ITEM_UFO_CRYO];
    g_camp.tech_pods     = g_camp.stores[ITEM_UFO_POD];
    g_camp.alien_corpses = g_camp.stores[ITEM_CORPSE_SECTOID] +
                           g_camp.stores[ITEM_CORPSE_FLOATER] +
                           g_camp.stores[ITEM_CORPSE_MUTON]   +
                           g_camp.stores[ITEM_CORPSE_CRYPTID];
}

void base_ui_handle_input(void) {
    int mx = g_input.mx, my = g_input.my;

    if (g_camp.base_modal != 0) {
        if (g_input.last_key == KEY_ESC || g_input.right_clicked) {
            g_camp.base_modal = 0;
            sfx_click();
            return;
        }
        if (g_input.last_key == KEY_UP) {
            g_camp.store_scroll = (g_camp.store_scroll + ITEM_COUNT - 1) % ITEM_COUNT;
            g_camp.log_sel      = (g_camp.log_sel + TECH_COUNT - 1) % TECH_COUNT;
            sfx_click();
            return;
        }
        if (g_input.last_key == KEY_DOWN) {
            g_camp.store_scroll = (g_camp.store_scroll + 1) % ITEM_COUNT;
            g_camp.log_sel      = (g_camp.log_sel + 1) % TECH_COUNT;
            sfx_click();
            return;
        }

        if (!g_input.left_clicked) return;

        if (mx >= 216 && mx < 238 && my >= 7 && my < 19) {
            g_camp.base_modal = 0;
            sfx_click();
            return;
        }
        if (g_camp.base_modal >= 2 && my >= 7 && my < 19) {
            if (mx >= 170 && mx < 191) {
                g_camp.store_scroll = (g_camp.store_scroll + ITEM_COUNT - 1) % ITEM_COUNT;
                g_camp.log_sel      = (g_camp.log_sel + TECH_COUNT - 1) % TECH_COUNT;
                sfx_click();
                return;
            }
            if (mx >= 193 && mx < 214) {
                g_camp.store_scroll = (g_camp.store_scroll + 1) % ITEM_COUNT;
                g_camp.log_sel      = (g_camp.log_sel + 1) % TECH_COUNT;
                sfx_click();
                return;
            }
        }

        if (g_camp.base_modal == 1) {
            if (mx >= 11 && mx < 235 && my >= 21 && my < 157) {
                int idx = (my - 21) / 17;
                if (idx >= 0 && idx < 8) {
                    int fid = FAC_STORES + idx;
                    const FacilityDef *fd = &fac_defs[fid];
                    if (fid == FAC_PSI_LAB && !g_camp.tech_done[TECH_PSI_LAB]) {
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "CAPTURE & INTERROGATE SECTOID COMMANDER FIRST!");
                        sfx_beep(200, 60);
                    } else if (g_camp.money >= fd->cash_cost && g_camp.alloys >= fd->alloy_cost) {
                        g_camp.money  -= fd->cash_cost;
                        g_camp.alloys -= fd->alloy_cost;
                        if (g_camp.stores[ITEM_ALLOYS] >= fd->alloy_cost)
                            g_camp.stores[ITEM_ALLOYS] -= fd->alloy_cost;
                        else
                            g_camp.stores[ITEM_ALLOYS] = g_camp.alloys;
                        g_camp.bunker_grid[g_camp.build_r][g_camp.build_c] = fid;
                        if (fid == FAC_UPG_LAB)      g_camp.scientists += 2;
                        if (fid == FAC_UPG_WORKSHOP) g_camp.engineers  += 2;
                        g_camp.base_modal = 0;
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "BUILT %s!", fd->full_name);
                        sfx_beep(700, 60);
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "NEED $%d + %d ALLOYS TO BUILD!", fd->cash_cost, fd->alloy_cost);
                        sfx_beep(200, 60);
                    }
                }
            }
        } else if (g_camp.base_modal == 2) {
            int vis[ITEM_COUNT];
            int n_vis = build_discovered_items(vis);
            int rows  = (n_vis < 6) ? n_vis : 6;
            if (n_vis > 0 && my >= 32 && my < 32 + rows * 21) {
                int row = (my - 32) / 21;
                int id  = vis[((g_camp.store_scroll % n_vis) + row) % n_vis];
                const ItemDef *d = item_get_def(id);
                if (campaign_is_item_researched(id) && d->category <= ICAT_UTILITY) {
                    if (mx >= 166 && mx < 199) {
                        if (g_camp.stores[id] > 0 &&
                            campaign_arv_used_space() + d->store_space <= campaign_arv_max_space()) {
                            g_camp.stores[id]--;
                            g_camp.arv_cargo[id]++;
                            if (id == ITEM_QUAD_DRONE || id == ITEM_UFO_DRONE) {
                                g_camp.arv_drone_item = id;
                            }
                            sfx_click();
                        } else {
                            snprintf(g_camp.notice, sizeof(g_camp.notice),
                                     g_camp.stores[id] <= 0 ? "NONE IN BASE STORES!" : "ARV BUS CARGO FULL!");
                            sfx_beep(200, 50);
                        }
                    } else if (mx >= 201 && mx < 234) {
                        if (g_camp.arv_cargo[id] > 0 &&
                            campaign_store_used_space() + d->store_space <= campaign_store_max_space()) {
                            g_camp.arv_cargo[id]--;
                            g_camp.stores[id]++;
                            if (g_camp.arv_drone_item == id && g_camp.arv_cargo[id] <= 0) {
                                g_camp.arv_drone_item = (g_camp.arv_cargo[ITEM_UFO_DRONE] > 0) ? ITEM_UFO_DRONE :
                                                        ((g_camp.arv_cargo[ITEM_QUAD_DRONE] > 0) ? ITEM_QUAD_DRONE : ITEM_NONE);
                            }
                            sfx_click();
                        } else {
                            sfx_beep(200, 50);
                        }
                    }
                }
            }
        } else if (g_camp.base_modal == 3) {
            int vis[ITEM_COUNT];
            int n_vis = build_discovered_items(vis);
            int rows  = (n_vis < 5) ? n_vis : 5;
            if (n_vis > 0 && my >= 32 && my < 32 + rows * 21) {
                int row = (my - 32) / 21;
                int id  = vis[((g_camp.store_scroll % n_vis) + row) % n_vis];
                const ItemDef *d = item_get_def(id);
                if (mx >= 152 && mx < 192 && d->buy_price > 0 && campaign_is_item_researched(id)) {
                    if (g_camp.money >= d->buy_price &&
                        campaign_store_used_space() + d->store_space <= campaign_store_max_space()) {
                        int d_hrs = (d->delivery_hours > 0) ? d->delivery_hours : 12;
                        g_camp.money -= d->buy_price;
                        g_camp.store_transit_qty[id]++;
                        if (g_camp.store_transit_hrs[id] <= 0) {
                            g_camp.store_transit_hrs[id] = d_hrs;
                        }
                        g_camp.item_seen[id] = 1;
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "ORDERED %s ($%d, %dH TRANSIT)", d->short_name, d->buy_price,
                                 g_camp.store_transit_hrs[id]);
                        sfx_click();
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 g_camp.money < d->buy_price ? "NOT ENOUGH FUNDS!" : "BASE STORES FULL! BUILD STORES");
                        sfx_beep(200, 50);
                    }
                } else if (mx >= 194 && mx < 234) {
                    if (g_camp.stores[id] > 0) {
                        g_camp.stores[id]--;
                        g_camp.money += d->sell_price;
                        sync_resource_counters();
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "SOLD %s (+$%d)", d->full_name, d->sell_price);
                        sfx_click();
                    } else {
                        sfx_beep(200, 50);
                    }
                }
            } else if (my >= 141 && my < 157) {
                int p_used = campaign_total_personnel();
                int p_max  = campaign_max_personnel();
                if (p_used >= p_max) {
                    snprintf(g_camp.notice, sizeof(g_camp.notice),
                             "QUARTERS FULL (%d/%d)! BUILD LIVING QUARTERS!", p_used, p_max);
                    sfx_beep(200, 50);
                } else if (mx >= 11 && mx < 85) {
                    /* Hire Rookie Operative ($1500) */
                    int s, slot = -1;
                    for (s = 0; s < MAX_ROSTER_SIZE; s++) {
                        if (!g_camp.squad[s].active) { slot = s; break; }
                    }
                    if (slot < 0) {
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "MAX HQ OPERATIVE ROSTER REACHED!");
                        sfx_beep(200, 50);
                    } else if (g_camp.money >= 1500) {
                        g_camp.money -= 1500;
                        operative_hire_replacement(&g_camp.squad[slot],
                                                   g_camp.recruit_pool, MAX_RECRUIT_POOL,
                                                   g_camp.squad, MAX_ROSTER_SIZE);
                        g_camp.squad[slot].assigned_arv = (campaign_count_arv_squad() < MAX_SQUAD_SIZE) ? 1 : 0;
                        campaign_sync_squad_units();
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "RECRUITED RKT. %s (%s)!",
                                 g_camp.squad[slot].name,
                                 g_camp.squad[slot].assigned_arv ? "ASSIGNED ARV" : "HQ RESERVE");
                        sfx_beep(660, 60);
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "NEED $1500 TO RECRUIT OPERATIVE!");
                        sfx_beep(200, 50);
                    }
                } else if (mx >= 86 && mx < 160) {
                    /* Hire Scientist ($2000) */
                    if (g_camp.money >= 2000) {
                        g_camp.money -= 2000;
                        g_camp.scientists++;
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "HIRED SCIENTIST! (TOTAL:%d)", g_camp.scientists);
                        sfx_beep(660, 60);
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "NEED $2000 TO HIRE SCIENTIST!");
                        sfx_beep(200, 50);
                    }
                } else if (mx >= 161 && mx < 235) {
                    /* Hire Engineer ($1800) */
                    if (g_camp.money >= 1800) {
                        g_camp.money -= 1800;
                        g_camp.engineers++;
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "HIRED ENGINEER! (TOTAL:%d)", g_camp.engineers);
                        sfx_beep(660, 60);
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "NEED $1800 TO HIRE ENGINEER!");
                        sfx_beep(200, 50);
                    }
                }
            }
        } else if (g_camp.base_modal == 4) {
            if (my >= 7 && my < 19 && mx >= 11 && mx < 168) {
                g_camp.lab_subtab = (mx - 11) / 53;
                sfx_click();
                return;
            }
            if (g_camp.lab_subtab == 0) {
                int vis_t[TECH_COUNT];
                int n_vis = build_visible_techs(vis_t);
                int rows  = (n_vis < 6) ? n_vis : 6;
                if (n_vis > 0 && my >= 33 && my < 33 + rows * 20) {
                    int row = (my - 33) / 20;
                    int tid = vis_t[((g_camp.store_scroll % n_vis) + row) % n_vis];
                    if (mx >= 176 && mx < 194) {
                        /* Unassign 1 scientist from this project */
                        if (g_camp.tech_scientists[tid] > 0) {
                            g_camp.tech_scientists[tid]--;
                            snprintf(g_camp.notice, sizeof(g_camp.notice),
                                     "%s: %d SCIENTISTS ASSIGNED",
                                     tech_get_def(tid)->title, g_camp.tech_scientists[tid]);
                            sfx_click();
                        } else {
                            sfx_beep(200, 40);
                        }
                    } else if ((mx >= 214 && mx < 233) || mx < 176) {
                        /* Assign 1 scientist to this project */
                        if (campaign_free_scientists() > 0) {
                            g_camp.tech_scientists[tid]++;
                            g_camp.active_tech = tid;
                            snprintf(g_camp.notice, sizeof(g_camp.notice),
                                     "%s: %d SCIENTISTS ASSIGNED",
                                     tech_get_def(tid)->title, g_camp.tech_scientists[tid]);
                            sfx_click();
                        } else {
                            snprintf(g_camp.notice, sizeof(g_camp.notice),
                                     "NO IDLE SCIENTISTS! UNASSIGN [-] OR HIRE MORE");
                            sfx_beep(200, 50);
                        }
                    }
                }
            } else if (g_camp.lab_subtab == 2) {
                int mfg_list[ITEM_COUNT];
                int n_mfg = build_researched_mfg_items(mfg_list);
                int rows  = (n_mfg < 5) ? n_mfg : 5;
                int has_upg_wk = base_has_facility(FAC_UPG_WORKSHOP);
                if (n_mfg > 0 && my >= 32 && my < 32 + rows * 21 && mx >= 176 && mx < 234) {
                    int row = (my - 32) / 21;
                    int id  = mfg_list[((g_camp.store_scroll % n_mfg) + row) % n_mfg];
                    const ItemDef *d = item_get_def(id);
                    int cash_cost = has_upg_wk ? ((d->mfg_cash * 3) / 4) : d->mfg_cash;
                    if (g_camp.money >= cash_cost &&
                        g_camp.alloys >= d->mfg_alloys &&
                        g_camp.elerium >= d->mfg_elerium) {
                        g_camp.money   -= cash_cost;
                        g_camp.alloys  -= d->mfg_alloys;
                        g_camp.elerium -= d->mfg_elerium;
                        g_camp.stores[ITEM_ALLOYS]  = g_camp.alloys;
                        g_camp.stores[ITEM_ELERIUM] = g_camp.elerium;
                        g_camp.mfg_queue[id]++;
                        g_camp.item_seen[id] = 1;
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "QUEUED %s IN WORKSHOP! (QUEUE:%d)",
                                 d->full_name, g_camp.mfg_queue[id]);
                        sfx_beep(720, 60);
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "NEED MORE FUNDS, ALLOYS, OR ELERIUM!");
                        sfx_beep(200, 50);
                    }
                } else if (mx >= 11 && mx < 235 && my >= 138 && my < 156) {

                    if (g_camp.arv_turret_level == 0 && g_camp.tech_done[TECH_ARV_TURRET] &&
                        g_camp.money >= 1500 && g_camp.alloys >= 6) {
                        g_camp.money -= 1500; g_camp.alloys -= 6;
                        g_camp.stores[ITEM_ALLOYS] = g_camp.alloys;
                        g_camp.arv_turret_level = 1;
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "INSTALLED ARV ROOF TURRET MK1!");
                        sfx_beep(780, 70);
                    } else if (g_camp.arv_turret_level == 1 && g_camp.tech_done[TECH_ARV_TURRET_MK2] &&
                               g_camp.money >= 2500 && g_camp.alloys >= 10) {
                        g_camp.money -= 2500; g_camp.alloys -= 10;
                        g_camp.stores[ITEM_ALLOYS] = g_camp.alloys;
                        g_camp.arv_turret_level = 2;
                        snprintf(g_camp.notice, sizeof(g_camp.notice), "UPGRADED ARV TURRET TO MK2!");
                        sfx_beep(880, 80);
                    } else {
                        sfx_beep(200, 50);
                    }
                }
            }
        }
        return;
    }

    if (!g_input.left_clicked) return;

    if (mx >= 6 && mx < 242 && my >= 6 && my < 21) {
        int b = (mx - 6) / 59;
        g_camp.store_scroll = 0;
        if (b == 0 || b == 3) g_camp.base_modal = 2;
        else if (b == 1)      g_camp.base_modal = 3;
        else if (b == 2)      g_camp.base_modal = 4;
        sfx_click();
        return;
    }

    if (mx >= 6 && mx < 242 && my >= 23 && my < 149) {
        int c = (mx - 6) / 59;
        int r = (my - 23) / 42;
        if (r >= 0 && r < 3 && c >= 0 && c < 4) {
            int fid = g_camp.bunker_grid[r][c];
            if (fid == FAC_EMPTY_LOT) {
                if (base_ui_is_lot_connected(r, c)) {
                    g_camp.build_r    = r;
                    g_camp.build_c    = c;
                    g_camp.base_modal = 1;
                    sfx_click();
                } else {
                    snprintf(g_camp.notice, sizeof(g_camp.notice),
                             "LOT MUST CONNECT TO EXISTING BASE TILE!");
                    sfx_beep(200, 50);
                }
            } else if (fid == FAC_GARAGE || fid == FAC_STORES || fid == FAC_MAIN_HOUSE) {
                g_camp.store_scroll = 0;
                g_camp.base_modal   = 2;
                sfx_click();
            } else if (fid == FAC_QUARTERS) {
                g_camp.store_scroll = 0;
                g_camp.base_modal   = 3;
                sfx_click();
            } else if (fid == FAC_PSI_LAB) {
                g_camp.active_tab    = 2;
                g_camp.armory_subtab = 0;
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "PSI-LAB: CLICK [PSI-TRAIN] IN AGENT DOSSIER!");
                sfx_click();
            } else if (fid == FAC_LAB || fid == FAC_UPG_LAB ||
                       fid == FAC_CONTAINMENT || fid == FAC_UPG_WORKSHOP) {
                g_camp.store_scroll = 0;
                g_camp.lab_subtab   = (fid == FAC_UPG_WORKSHOP) ? 2 : 0;
                g_camp.base_modal   = 4;
                sfx_click();
            } else {
                snprintf(g_camp.notice, sizeof(g_camp.notice), "%s", fac_defs[fid % FAC_COUNT].hover_tip);
                sfx_click();
            }
        }
    }
}

void armory_ui_render(void) {
    int mx = g_input.mx, my = g_input.my;
    int sel = g_camp.selected_soldier % MAX_ROSTER_SIZE;
    const Operative *op;
    const ItemDef *wdef;
    int bonus_hp = 0, bonus_tu = 0, has_psi_lab = 0, r, c, b;
    int roster_cnt = campaign_count_hired_operatives();
    int arv_cnt    = campaign_count_arv_squad();
    static const char *top_lbl[3] = { "AGENT", "ARV BUS", "MEMORIAL" };
    char buf[64];

    if (!g_camp.squad[sel].active) {
        for (b = 0; b < MAX_ROSTER_SIZE; b++) {
            if (g_camp.squad[b].active) { sel = b; g_camp.selected_soldier = b; break; }
        }
    }
    op   = &g_camp.squad[sel];
    wdef = item_get_def(op->weapon_id);

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            if (g_camp.bunker_grid[r][c] == FAC_QUARTERS)   bonus_hp += 15;
            if (g_camp.bunker_grid[r][c] == FAC_MAIN_HOUSE) bonus_tu = 6;
            if (g_camp.bunker_grid[r][c] == FAC_PSI_LAB)    has_psi_lab = 1;
        }
    }

    vga_rect_bevel(4, 4, 240, 162, 76, 80, 79);

    {
        int hov_prev = (mx >= 6  && mx < 27 && my >= 6 && my < 21);
        int hov_next = (mx >= 29 && mx < 50 && my >= 6 && my < 21);
        vga_rect_bevel(6,  6, 21, 15, hov_prev ? 65 : 68, hov_prev ? 96 : 162, 79);
        vga_print_shadow(13, 10, "<", hov_prev ? 96 : 1, 15);
        vga_rect_bevel(29, 6, 21, 15, hov_next ? 65 : 68, hov_next ? 96 : 162, 79);
        vga_print_shadow(36, 10, ">", hov_next ? 96 : 1, 15);
    }
    for (b = 0; b < 3; b++) {
        int bx = 53 + b * 63;
        int act = (g_camp.armory_subtab == b);
        int hov = (mx >= bx && mx < bx + 61 && my >= 6 && my < 21);
        vga_rect_bevel(bx, 6, 61, 15, act ? 65 : (hov ? 68 : 167),
                       (act || hov) ? 96 : 162, 172);
        vga_print_shadow(bx + 6, 10, top_lbl[b], (act || hov) ? 96 : 1, 15);
    }

    if (g_camp.armory_subtab == 1) {
        /* Subtab 1: ARV Bus Cargo Equip Panel (renders below top bar at y=23..163) */
        int vis[ITEM_COUNT];
        int n_vis = build_discovered_items(vis);
        int i, rows = (n_vis < 5) ? n_vis : 5;
        int s_used = campaign_store_used_space(), s_max = campaign_store_max_space();
        int a_used = campaign_arv_used_space(),   a_max = campaign_arv_max_space();
        int base_idx = (n_vis > 0) ? (g_camp.store_scroll % n_vis) : 0;
        int hov_up = (mx >= 194 && mx < 215 && my >= 25 && my < 36);
        int hov_dn = (mx >= 217 && mx < 238 && my >= 25 && my < 36);

        vga_rect_bevel(6, 23, 236, 140, 14, 80, 15);
        snprintf(buf, sizeof(buf), "STORES:%d/%d  BUS:%d/%d", s_used, s_max, a_used, a_max);
        vga_print_shadow(10, 27, buf, 96, 15);
        vga_rect_bevel(194, 25, 21, 11, hov_up ? 65 : 68, hov_up ? 96 : 162, 79);
        vga_print_shadow(199, 27, "UP", hov_up ? 96 : 1, 15);
        vga_rect_bevel(217, 25, 21, 11, hov_dn ? 65 : 68, hov_dn ? 96 : 162, 79);
        vga_print_shadow(222, 27, "DN", hov_dn ? 96 : 1, 15);

        for (i = 0; i < rows; i++) {
            int id = vis[(base_idx + i) % n_vis];
            const ItemDef *d = item_get_def(id);
            int ry = 38 + i * 24;
            int can_use = campaign_is_item_researched(id) && (d->category <= ICAT_UTILITY);
            int hov_to_bus = (mx >= 166 && mx < 199 && my >= ry + 3 && my < ry + 19);
            int hov_to_str = (mx >= 201 && mx < 234 && my >= ry + 3 && my < ry + 19);

            vga_rect_bevel(10, ry, 228, 22, 74, 68, 79);
            vga_blit_ui(12, ry + 3, ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[id]);
            vga_print_shadow(46, ry + 3, d->short_name, can_use ? 96 : 80, 15);
            snprintf(buf, sizeof(buf), "STR:%d BUS:%d", g_camp.stores[id], g_camp.arv_cargo[id]);
            vga_print(46, ry + 12, buf, 18);

            if (can_use) {
                vga_rect_bevel(166, ry + 3, 33, 16, hov_to_bus ? 65 : 68, hov_to_bus ? 96 : 162, 79);
                vga_print(169, ry + 7, ">BUS", g_camp.stores[id] > 0 ? 96 : 8);
                vga_rect_bevel(201, ry + 3, 33, 16, hov_to_str ? 65 : 68, hov_to_str ? 96 : 162, 79);
                vga_print(204, ry + 7, "<STR", g_camp.arv_cargo[id] > 0 ? 80 : 8);
            } else {
                vga_print(168, ry + 7, d->category >= ICAT_ARTIFACT ? "VAULT" : "NEED RES", 8);
            }
        }
        return;
    }

    if (g_camp.armory_subtab == 2) {
        int i, cnt = g_camp.memorial_count;
        int rows = (cnt < 4) ? cnt : 4;
        int base_m = (cnt > 0) ? (g_camp.store_scroll % cnt) : 0;
        int hov_up = (mx >= 194 && mx < 215 && my >= 25 && my < 36);
        int hov_dn = (mx >= 217 && mx < 238 && my >= 25 && my < 36);

        vga_rect_bevel(6, 23, 236, 140, 14, 80, 15);
        snprintf(buf, sizeof(buf), "KIA MEMORIAL WALL (FALLEN:%d)", cnt);
        vga_print_shadow(10, 27, buf, 96, 15);
        vga_rect_bevel(194, 25, 21, 11, hov_up ? 65 : 68, hov_up ? 96 : 162, 79);
        vga_print_shadow(199, 27, "UP", hov_up ? 96 : 1, 15);
        vga_rect_bevel(217, 25, 21, 11, hov_dn ? 65 : 68, hov_dn ? 96 : 162, 79);
        vga_print_shadow(222, 27, "DN", hov_dn ? 96 : 1, 15);

        if (cnt == 0) {
            vga_print_shadow(18, 82, "NO OPERATIVES LOST IN ACTION.", 144, 15);
            vga_print(18, 96, "ALL AGENTS ACCOUNTED FOR AT HQ!", 80);
            return;
        }
        for (i = 0; i < rows; i++) {
            const MemorialEntry *me = &g_camp.memorial[(base_m + i) % cnt];
            int ry = 38 + i * 30;
            vga_rect_bevel(10, ry, 228, 28, 74, 68, 79);
            operative_draw_rank_badge(14, ry + 4, me->rank);
            snprintf(buf, sizeof(buf), "%s %s (%s)",
                     operative_rank_prefix(me->rank), me->name, me->gender ? "F" : "M");
            vga_print_shadow(26, ry + 4, buf, 96, 15);
            snprintf(buf, sizeof(buf), "DAY %d", me->day_kia);
            vga_print(188, ry + 4, buf, 32);
            snprintf(buf, sizeof(buf), "KILLS:%d MSNS:%d - %s",
                     me->kills, me->missions, me->cause);
            vga_print(14, ry + 15, buf, 18);
        }
        return;
    }


    /* Subtab 0: Single-Operative Dossier (Left) & Full-Body Suit / ARV Bay Status (Right) */
    vga_rect_bevel(6, 23, 88, 140, 74, 68, 79);
    operative_draw_rank_badge(10, 27, op->rank);
    operative_format_full_name(op, buf, sizeof(buf));
    vga_print_shadow(21, 27, buf, 96, 15);

    snprintf(buf, sizeof(buf), "AG#%d/%d ARV:%d/%d",
             sel + 1, roster_cnt, arv_cnt, MAX_SQUAD_SIZE);
    vga_print(10, 38, buf, 80);

    vga_rect_bevel(31, 47, 36, 38, 14, 96, 79);
    vga_blit_ui(33, 48, 32, 36, spr_portraits[op->portrait_id % NUM_PORTRAITS]);

    snprintf(buf, sizeof(buf), "XP:%d/%d", op->xp,
             op->rank < RANK_COMMANDER ? operative_next_rank_xp(op->rank) : op->xp);
    vga_print(10, 88, buf, 96);

    snprintf(buf, sizeof(buf), "TU:%2d  HP:%2d", op->tu_max + bonus_tu, op->hp_max + bonus_hp);
    vga_print(10, 98, buf, 18);
    snprintf(buf, sizeof(buf), "AIM:%2d THR:%2d", op->firing_acc, op->throw_acc);
    vga_print(10, 108, buf, 80);
    snprintf(buf, sizeof(buf), "RCN:%2d EN:%2d", op->reactions, op->energy_max);
    vga_print(10, 118, buf, 2);
    snprintf(buf, sizeof(buf), "KLS:%2d MSN:%2d", op->kills, op->missions);
    vga_print(10, 128, buf, 96);

    /* Psionic stat is hidden until the operative completes Psi-Training! */
    if (op->psi_trained == 2) {
        snprintf(buf, sizeof(buf), "BRV:%2d PSI:%2d", op->bravery, op->psi_skill);
        vga_print_shadow(10, 140, buf, 144, 15);
    } else if (op->psi_trained == 1) {
        snprintf(buf, sizeof(buf), "BRV:%2d PSI:%dH", op->bravery, op->psi_hours_left);
        vga_print(10, 140, buf, 80);
    } else {
        snprintf(buf, sizeof(buf), "BRV:%2d", op->bravery);
        vga_print(10, 140, buf, 18);
    }
    vga_print(10, 151, wdef->short_name, 8);

    vga_rect_bevel(96, 23, 145, 140, 74, 68, 79);
    vga_print_shadow(100, 26, "OPERATIVE COMBAT SUIT", 96, 15);

    /* Full-Body Operative Paper-Doll Scanner Preview (x=100..153, y=36..135) */
    vga_rect_bevel(100, 36, 54, 100, 13, 68, 15);
    for (r = 44; r < 132; r += 12) {
        vga_hline(102, 151, r, 78);
    }
    vga_blit_ui(103, 38, FULLBODY_W, FULLBODY_H, spr_fullbody[op->portrait_id % NUM_PORTRAITS]);

    /* Service Dossier & ARV / Psi-Training Controls (x=157..237, y=36..135) */
    vga_rect_bevel(157, 36, 80, 100, 14, 68, 15);
    vga_print_shadow(161, 40, "DEPLOYMENT:", 80, 15);
    {
        int hov_arv = (mx >= 160 && mx < 234 && my >= 49 && my < 65);
        vga_rect_bevel(160, 49, 74, 16,
                       op->assigned_arv ? 36 : (hov_arv ? 65 : 68),
                       op->assigned_arv ? 144 : (hov_arv ? 96 : 162), 79);
        if (op->psi_trained == 1) {
            vga_print_shadow(165, 53, "IN PSI-LAB", 80, 15);
        } else if (op->assigned_arv) {
            vga_print_shadow(165, 53, "ARV:ACTIVE", 144, 15);
        } else {
            vga_print_shadow(165, 53, "HQ:RESERVE", 96, 15);
        }
    }

    vga_print_shadow(161, 69, "ARV CARGO:", 80, 15);
    snprintf(buf, sizeof(buf), "%d/%d SLOTS", campaign_arv_used_space(), campaign_arv_max_space());
    vga_print(161, 78, buf, 144);
    if (g_camp.arv_drone_item != ITEM_NONE) {
        vga_print(161, 88, item_get_def(g_camp.arv_drone_item)->short_name, 96);
    } else {
        vga_print(161, 88, "NO DRONE PAD", 8);
    }

    if (has_psi_lab) {
        int hov_psi = (mx >= 160 && mx < 234 && my >= 102 && my < 118);
        vga_rect_bevel(160, 102, 74, 16, hov_psi ? 65 : 68, hov_psi ? 96 : 162, 79);
        if (op->psi_trained == 0) {
            vga_print_shadow(165, 106, "PSI-TRAIN!", 96, 15);
        } else if (op->psi_trained == 1) {
            snprintf(buf, sizeof(buf), "TRN: %dH", op->psi_hours_left);
            vga_print_shadow(165, 106, buf, 80, 15);
        } else {
            snprintf(buf, sizeof(buf), "PSI: %d", op->psi_skill);
            vga_print_shadow(165, 106, buf, 144, 15);
        }
    } else {
        vga_print(161, 106, "EQUIP IN ARV", 8);
        vga_print(161, 115, "AT MSN START", 8);
    }

    {
        int hov_bus = (mx >= 100 && mx < 237 && my >= 139 && my < 158);
        vga_rect_bevel(100, 139, 137, 19, hov_bus ? 65 : 68, hov_bus ? 96 : 162, 79);
        vga_print_shadow(112, 145, "LOAD ARV BUS CARGO", hov_bus ? 96 : 1, 15);
    }
}

void armory_ui_handle_input(void) {
    int mx = g_input.mx, my = g_input.my;
    int sel = g_camp.selected_soldier % MAX_ROSTER_SIZE;

    /* Top Bar: [<] [>] Operative Cycle + [AGENT] [ARV BUS] [MEMORIAL] Subtabs */
    if (my >= 6 && my < 21) {
        if (mx >= 6 && mx < 27) {
            int k;
            for (k = 1; k <= MAX_ROSTER_SIZE; k++) {
                int idx = (sel + MAX_ROSTER_SIZE - k) % MAX_ROSTER_SIZE;
                if (g_camp.squad[idx].active) {
                    g_camp.selected_soldier = idx;
                    sfx_click();
                    break;
                }
            }
            return;
        }
        if (mx >= 29 && mx < 50) {
            int k;
            for (k = 1; k <= MAX_ROSTER_SIZE; k++) {
                int idx = (sel + k) % MAX_ROSTER_SIZE;
                if (g_camp.squad[idx].active) {
                    g_camp.selected_soldier = idx;
                    sfx_click();
                    break;
                }
            }
            return;
        }
        if (mx >= 53 && mx < 242) {
            g_camp.armory_subtab = (mx - 53) / 63;
            g_camp.store_scroll  = 0;
            sfx_click();
            return;
        }
    }

    if (g_camp.armory_subtab == 1) {
        /* ARV Bus Cargo Equip Screen: Scroll (y=25..36) + Transfer (y=38..158) */
        int vis[ITEM_COUNT];
        int n_vis = build_discovered_items(vis);
        int rows  = (n_vis < 5) ? n_vis : 5;
        if (my >= 25 && my < 36) {
            if (mx >= 194 && mx < 215 && n_vis > 0) {
                g_camp.store_scroll = (g_camp.store_scroll + n_vis - 1) % n_vis;
                sfx_click();
            } else if (mx >= 217 && mx < 238 && n_vis > 0) {
                g_camp.store_scroll = (g_camp.store_scroll + 1) % n_vis;
                sfx_click();
            }
            return;
        }
        if (n_vis > 0 && my >= 38 && my < 38 + rows * 24) {
            int row = (my - 38) / 24;
            int id  = vis[((g_camp.store_scroll % n_vis) + row) % n_vis];
            const ItemDef *d = item_get_def(id);
            if (campaign_is_item_researched(id) && d->category <= ICAT_UTILITY) {
                if (mx >= 166 && mx < 199) {
                    if (g_camp.stores[id] > 0 &&
                        campaign_arv_used_space() + d->store_space <= campaign_arv_max_space()) {
                        g_camp.stores[id]--;
                        g_camp.arv_cargo[id]++;
                        if (id == ITEM_QUAD_DRONE || id == ITEM_UFO_DRONE) {
                            g_camp.arv_drone_item = id;
                        }
                        sfx_click();
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 g_camp.stores[id] <= 0 ? "NONE IN BASE STORES!" : "ARV BUS CARGO FULL!");
                        sfx_beep(200, 50);
                    }
                } else if (mx >= 201 && mx < 234) {
                    if (g_camp.arv_cargo[id] > 0 &&
                        campaign_store_used_space() + d->store_space <= campaign_store_max_space()) {
                        g_camp.arv_cargo[id]--;
                        g_camp.stores[id]++;
                        if (g_camp.arv_drone_item == id && g_camp.arv_cargo[id] <= 0) {
                            g_camp.arv_drone_item = (g_camp.arv_cargo[ITEM_UFO_DRONE] > 0) ? ITEM_UFO_DRONE :
                                                    ((g_camp.arv_cargo[ITEM_QUAD_DRONE] > 0) ? ITEM_QUAD_DRONE : ITEM_NONE);
                        }
                        sfx_click();
                    } else {
                        sfx_beep(200, 50);
                    }
                }
            }
        }
        return;
    }

    if (g_camp.armory_subtab == 2) {
        if (my >= 25 && my < 36) {
            if (mx >= 194 && mx < 215 && g_camp.memorial_count > 0) {
                g_camp.store_scroll = (g_camp.store_scroll + g_camp.memorial_count - 1) % g_camp.memorial_count;
                sfx_click();
            } else if (mx >= 217 && mx < 238 && g_camp.memorial_count > 0) {
                g_camp.store_scroll = (g_camp.store_scroll + 1) % g_camp.memorial_count;
                sfx_click();
            }
        }
        return;
    }

    /* Subtab 0: Toggle ARV Assignment (x=160..234, y=49..65) */
    if (mx >= 160 && mx < 234 && my >= 49 && my < 65 && g_camp.squad[sel].active) {
        Operative *op = &g_camp.squad[sel];
        if (op->psi_trained == 1) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "%s IS IN PSI-TRAINING (%dH LEFT)!", op->name, op->psi_hours_left);
            sfx_beep(200, 50);
        } else if (op->assigned_arv) {
            if (campaign_count_arv_squad() <= 1) {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "AT LEAST 1 OPERATIVE MUST REMAIN ON THE ARV!");
                sfx_beep(200, 50);
            } else {
                op->assigned_arv = 0;
                campaign_sync_squad_units();
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "%s MOVED TO HQ RESERVE", op->name);
                sfx_click();
            }
        } else {
            if (campaign_count_arv_squad() >= MAX_SQUAD_SIZE) {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "ARV FULL (%d/%d)! UNASSIGN ANOTHER AGENT FIRST",
                         MAX_SQUAD_SIZE, MAX_SQUAD_SIZE);
                sfx_beep(200, 50);
            } else {
                op->assigned_arv = 1;
                campaign_sync_squad_units();
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "%s ASSIGNED TO ARV STRIKE TEAM!", op->name);
                sfx_click();
            }
        }
        return;
    }

    /* Subtab 0: Toggle Psi-Lab Training (x=160..234, y=102..118) */
    if (mx >= 160 && mx < 234 && my >= 102 && my < 118 &&
        base_has_facility(FAC_PSI_LAB) && g_camp.squad[sel].active) {
        Operative *op = &g_camp.squad[sel];
        if (op->psi_trained == 0) {
            if (op->assigned_arv && campaign_count_arv_squad() <= 1) {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "NEED AT LEAST 1 ACTIVE OPERATIVE ON THE ARV!");
                sfx_beep(200, 50);
            } else {
                op->psi_trained    = 1;
                op->psi_hours_left = 72;
                op->assigned_arv   = 0;
                campaign_sync_squad_units();
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "%s ENTERED PSI-LAB TRAINING (72 HRS)!", op->name);
                sfx_beep(700, 60);
            }
        } else if (op->psi_trained == 1) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "%s PSI-TRAINING IN PROGRESS (%d HRS LEFT)", op->name, op->psi_hours_left);
            sfx_click();
        } else {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "%s PSI-TRAINING COMPLETE (PSI SKILL: %d)", op->name, op->psi_skill);
            sfx_click();
        }
        return;
    }

    /* Subtab 0: Click [LOAD ARV BUS CARGO] button to open ARV Bus Cargo subtab */
    if (mx >= 100 && mx < 237 && my >= 139 && my < 158) {
        g_camp.armory_subtab = 1;
        g_camp.store_scroll  = 0;
        sfx_click();
    }
}








