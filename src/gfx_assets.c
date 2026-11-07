#include "gfx_assets.h"
#include "bmp_loader.h"
#include "vga.h"
#include <stdlib.h>
#include <string.h>

uint8_t spr_floors[FLOOR_COUNT][TILE_W * TILE_H];
uint8_t spr_walls_west[WALL_COUNT][WALL_W * WALL_H];
uint8_t spr_walls_north[WALL_COUNT][WALL_W * WALL_H];
uint8_t spr_objects[OBJ_COUNT][OBJ_W * OBJ_H];
uint8_t spr_units[NUM_UNIT_SPRITES][4][8][UNIT_W * UNIT_H];
uint8_t spr_deaths[NUM_UNIT_SPRITES][4][DEATH_W * DEATH_H];
uint8_t spr_proj[PROJ_TYPE_COUNT][8][FX_W * FX_H];
uint8_t spr_thrown[2][4][FX_W * FX_H];
uint8_t spr_muzzle[PROJ_TYPE_COUNT][FX_W * FX_H];
uint8_t spr_reticle[2][FX_W * FX_H];
uint8_t spr_splash[SPLASH_TYPE_COUNT][4][FX_W * FX_H];
uint8_t spr_portraits[NUM_PORTRAITS][32 * 36];
uint8_t spr_weapons[4][40 * 24];

uint8_t *gfx_title_bg  = NULL;
uint8_t *gfx_hidden_bg = NULL;
uint8_t *gfx_radar_bg  = NULL;

/* Helper: check if pixel (x,y) is inside a 32x16 isometric diamond */
static int in_diamond(int x, int y) {
    int dx = (x < 16) ? (15 - x) : (x - 16);
    int dy = (y < 8)  ? (7 - y)  : (y - 8);
    return (dx + dy * 2 <= 15);
}

static int is_diamond_edge(int x, int y) {
    int dx = (x < 16) ? (15 - x) : (x - 16);
    int dy = (y < 8)  ? (7 - y)  : (y - 8);
    int d = dx + dy * 2;
    return (d == 14 || d == 15);
}

static void build_floors(void) {
    int f, x, y;
    for (f = 0; f < FLOOR_COUNT; f++) {
        memset(spr_floors[f], COL_TRANS, TILE_W * TILE_H);
        if (f == FLOOR_NONE) continue;

        for (y = 0; y < TILE_H; y++) {
            for (x = 0; x < TILE_W; x++) {
                uint8_t col = 0;
                int hash = ((x * 37 + y * 17) ^ (x * y)) & 7;
                if (!in_diamond(x, y)) continue;

                switch (f) {
                    case FLOOR_GRASS:
                        col = (uint8_t)(20 + (hash & 3));
                        if (hash == 0) col = 18; /* Bright grass blade */
                        if (is_diamond_edge(x, y)) col = 24;
                        break;

                    case FLOOR_ASPHALT:
                        col = (uint8_t)(224 + 9 + (hash & 1));
                        if (is_diamond_edge(x, y)) col = 224 + 11;
                        break;

                    case FLOOR_ASPHALT_LINE:
                        col = (uint8_t)(224 + 9 + (hash & 1));
                        /* Yellow Route 309 center highway stripe along NE-SW axis */
                        if (abs((x - 16) + (y - 8) * 2) <= 2 && ((x + y) & 4)) {
                            col = 97; /* Bright highway yellow */
                        }
                        break;

                    case FLOOR_SIDEWALK:
                        col = (uint8_t)(192 + 4 + (hash & 1));
                        if (is_diamond_edge(x, y)) col = 192 + 8;
                        break;

                    case FLOOR_DINER_TILE: {
                        int u = (x - 16) + (y - 8) * 2;
                        int v = (x - 16) - (y - 8) * 2;
                        int chk = ((u >> 3) + (v >> 3)) & 1;
                        col = chk ? 2 : 11; /* Classic black & white checkerboard */
                        if (is_diamond_edge(x, y)) col = 83; /* Neon cyan trim */
                        break;
                    }

                    case FLOOR_ARCADE_CARPET:
                        col = (uint8_t)(128 + 10 + (hash & 1));
                        if (hash == 1) col = 82;  /* Neon cyan fleck */
                        if (hash == 5) col = 114; /* Neon orange fleck */
                        if (is_diamond_edge(x, y)) col = 128 + 12;
                        break;

                    case FLOOR_WOOD:
                        col = (uint8_t)(48 + 5 + ((x + y) & 1));
                        if (((x - y * 2) & 7) == 0) col = 48 + 9; /* Plank seam */
                        if (is_diamond_edge(x, y)) col = 48 + 8;
                        break;

                    case FLOOR_ROOF_TAR:
                        col = (uint8_t)(8 + (hash & 3));
                        if (is_diamond_edge(x, y)) col = 6;
                        break;

                    case FLOOR_SCORCHED:
                        col = (uint8_t)(11 + (hash & 3));
                        if (hash == 0 && (x > 10 && x < 22)) col = 146; /* Elerium residue */
                        break;
                }
                spr_floors[f][y * TILE_W + x] = col;
            }
        }
    }
}

/* Build West Wall (16x32) and North Wall (16x32) panels */
static void build_walls(void) {
    int w, x, y;
    for (w = 0; w < WALL_COUNT; w++) {
        memset(spr_walls_west[w],  COL_TRANS, WALL_W * WALL_H);
        memset(spr_walls_north[w], COL_TRANS, WALL_W * WALL_H);
        if (w == WALL_NONE) continue;

        for (x = 0; x < WALL_W; x++) {
            /* West wall slopes down-left: at x=15 top is y=0, at x=0 top is y=7 */
            int top_west  = (15 - x) / 2;
            /* North wall slopes down-right: at x=0 top is y=0, at x=15 top is y=7 */
            int top_north = x / 2;
            int is_fence    = (w == WALL_FENCE || w == WALL_FENCE_DMG);
            int wall_height = is_fence ? 12 : LEVEL_H;
            int y_start_off = is_fence ? (LEVEL_H - 12) : 0;

            for (y = 0; y < wall_height; y++) {
                int wy_w = top_west  + y_start_off + y;
                int wy_n = top_north + y_start_off + y;
                uint8_t col_w = 6, col_n = 8;

                switch (w) {
                    case WALL_BRICK:
                    case WALL_BRICK_WIN:
                    case WALL_BRICK_BROKEN:
                    case WALL_BRICK_DMG: {
                        int mortar = ((y % 4) == 0) || (((x + (y / 4) * 3) % 5) == 0);
                        col_w = mortar ? 7 : 36;
                        col_n = mortar ? 9 : 39;
                        if ((w == WALL_BRICK_WIN || w == WALL_BRICK_BROKEN) &&
                            x >= 4 && x <= 11 && y >= 5 && y <= 15) {
                            if (x == 4 || x == 11 || y == 5 || y == 15) {
                                col_w = 2; col_n = 4; /* White window frame */
                            } else if (w == WALL_BRICK_BROKEN) {
                                int shard = ((x == 5 || x == 10) && ((x + y) & 1)) ||
                                            ((y == 6 || y == 14) && ((x * 3 + y) % 3 == 0));
                                col_w = shard ? 80 : COL_TRANS;
                                col_n = shard ? 82 : COL_TRANS;
                            } else {
                                col_w = ((x + y) % 5 == 0) ? 81 : 70; /* Glass reflection */
                                col_n = ((x + y) % 5 == 0) ? 82 : 72;
                            }
                        } else if (w == WALL_BRICK_DMG) {
                            if (y < 4 && x >= 6 && x <= 10 && (y + (x & 1) < 3)) {
                                col_w = COL_TRANS; col_n = COL_TRANS;
                            } else if (x + y == 17 || x + y == 18 || (x >= 7 && x <= 9 && y >= 10 && y <= 13)) {
                                col_w = 13; col_n = 14; /* Deep blast crack / crater */
                            }
                        }
                        break;
                    }

                    case WALL_DINER_CHROME:
                    case WALL_DINER_GLASS:
                    case WALL_DINER_BROKEN:
                    case WALL_DINER_DMG:
                        if (y < 3) {
                            if ((w == WALL_DINER_BROKEN || w == WALL_DINER_DMG) && x >= 5 && x <= 11) {
                                col_w = (x == 7 && y == 1) ? 96 : 12;
                                col_n = (x == 7 && y == 1) ? 97 : 13;
                            } else {
                                col_w = 81; col_n = 83; /* Glowing neon cyan roof strip */
                            }
                        } else if (y == 3 || y > 19) {
                            col_w = 3; col_n = 5;   /* Brushed stainless chrome */
                        } else if ((w == WALL_DINER_GLASS || w == WALL_DINER_BROKEN) &&
                                   x >= 2 && x <= 13 && y >= 5 && y <= 16) {
                            if (x == 2 || x == 13 || y == 5 || y == 16) {
                                col_w = 3; col_n = 5;
                            } else if (w == WALL_DINER_BROKEN) {
                                int shard = ((x == 3 || x == 12 || y == 6 || y == 15) && ((x + y) & 1));
                                col_w = shard ? 80 : COL_TRANS;
                                col_n = shard ? 81 : COL_TRANS;
                            } else {
                                col_w = ((x + y) % 6 == 0) ? 80 : 88;
                                col_n = ((x + y) % 6 == 0) ? 81 : 90;
                            }
                        } else {
                            col_w = (y & 1) ? 4 : 34; /* Red & chrome diner ribbing */
                            col_n = (y & 1) ? 6 : 36;
                            if (w == WALL_DINER_DMG && (x - y == -4 || (x >= 6 && x <= 10 && y >= 9 && y <= 13))) {
                                col_w = 13; col_n = 14;
                            }
                        }
                        break;

                    case WALL_BARN_WOOD:
                    case WALL_BARN_DMG:
                        col_w = ((x & 3) == 0) ? 41 : 35;
                        col_n = ((x & 3) == 0) ? 43 : 38;
                        if (y == 0 || y == wall_height - 1) { col_w = 2; col_n = 4; }
                        if (w == WALL_BARN_DMG) {
                            if ((y < 3 && x >= 5 && x <= 9) ||
                                (x >= 5 && x <= 7 && y >= 9 && y <= 15) ||
                                (x >= 10 && x <= 11 && y >= 5 && y <= 10)) {
                                col_w = COL_TRANS; col_n = COL_TRANS;
                            } else if (x == 4 || x == 8 || y == 8 || y == 16) {
                                col_w = 53; col_n = 55; /* Raw splintered wood edge */
                            }
                        }
                        break;

                    case WALL_STEEL:
                    case WALL_STEEL_DMG:
                        col_w = ((x % 4) == 0) ? 160 + 9 : 160 + 5;
                        col_n = ((x % 4) == 0) ? 160 + 11 : 160 + 7;
                        if (y == 12) { col_w = 98; col_n = 100; } /* Hazard / conduit stripe */
                        if (w == WALL_STEEL_DMG) {
                            if (x >= 6 && x <= 10 && y >= 10 && y <= 14) {
                                col_w = (x == 8 && y == 12) ? 96 : 13;
                                col_n = (x == 8 && y == 12) ? 113 : 14;
                            } else if (x + y == 15 || x - y == -6) {
                                col_w = 12; col_n = 13;
                            }
                        }
                        break;

                    case WALL_DOOR_CLOSED:
                        if (x <= 2 || x >= 13 || y <= 2) {
                            col_w = 8; col_n = 10; /* Door frame */
                        } else {
                            col_w = 68; col_n = 70; /* Reinforced blue-steel door */
                            if (x == 11 && y == 12) { col_w = 96; col_n = 97; } /* Gold handle */
                        }
                        break;

                    case WALL_DOOR_OPEN:
                        if (x <= 2 || x >= 13 || y <= 3) {
                            col_w = 8; col_n = 10; /* Open door archway only */
                        } else {
                            col_w = COL_TRANS; col_n = COL_TRANS;
                        }
                        break;

                    case WALL_FENCE:
                    case WALL_FENCE_DMG:
                        if (w == WALL_FENCE_DMG && x >= 5 && x <= 10 && y < 7) {
                            col_w = COL_TRANS; col_n = COL_TRANS;
                        } else if ((x & 1) == 0 || y == 2 || y == 9) {
                            col_w = 52; col_n = 55; /* Wood picket fence */
                        } else {
                            col_w = COL_TRANS; col_n = COL_TRANS;
                        }
                        break;
                }

                /* Top wall edge highlight for crisp 3D readability */
                if (y == 0 && col_w != COL_TRANS) {
                    col_w = 4;
                    col_n = 6;
                }

                if (wy_w >= 0 && wy_w < WALL_H && col_w != COL_TRANS)
                    spr_walls_west[w][wy_w * WALL_W + x] = col_w;
                if (wy_n >= 0 && wy_n < WALL_H && col_n != COL_TRANS)
                    spr_walls_north[w][wy_n * WALL_W + x] = col_n;
            }
        }
    }
}

static void obj_rect(int id, int x0, int y0, int w, int h, uint8_t col) {
    int x, y;
    for (y = y0; y < y0 + h; y++) {
        for (x = x0; x < x0 + w; x++) {
            if ((unsigned)x < OBJ_W && (unsigned)y < OBJ_H) {
                spr_objects[id][y * OBJ_W + x] = col;
            }
        }
    }
}

static void build_objects(void) {
    int i, x, y;
    for (i = 0; i < OBJ_COUNT; i++) {
        memset(spr_objects[i], COL_TRANS, OBJ_W * OBJ_H);
    }

    /* 1. OBJ_ARCADE_CAB: Upright 90s Arcade Machine with glowing CRT screen & marquee */
    obj_rect(OBJ_ARCADE_CAB, 9,  8, 14, 26, 132); /* Purple/indigo side cabinet */
    obj_rect(OBJ_ARCADE_CAB, 10, 8, 12,  4, 96);  /* Lit yellow marquee at top */
    obj_rect(OBJ_ARCADE_CAB, 11, 13, 10, 8, 81);  /* Glowing cyan CRT monitor */
    obj_rect(OBJ_ARCADE_CAB, 13, 15,  6, 4, 80);  /* Bright CRT center glow */
    obj_rect(OBJ_ARCADE_CAB, 9,  21, 14, 3, 34);  /* Red control panel deck + trackball */
    spr_objects[OBJ_ARCADE_CAB][21 * OBJ_W + 15] = 0; /* White trackball! */
    spr_objects[OBJ_ARCADE_CAB][21 * OBJ_W + 18] = 96; /* Arcade buttons */
    obj_rect(OBJ_ARCADE_CAB, 11, 25, 10, 8, 13);  /* Coin door */
    obj_rect(OBJ_ARCADE_CAB, 13, 27,  2, 3, 114); /* Orange glowing coin reject slots */
    obj_rect(OBJ_ARCADE_CAB, 17, 27,  2, 3, 114);

    /* 2. OBJ_DINER_BOOTH: Red vinyl booth seats + chrome table */
    obj_rect(OBJ_DINER_BOOTH, 5, 20, 7, 12, 34);  /* Left red booth seat */
    obj_rect(OBJ_DINER_BOOTH, 20, 20, 7, 12, 36); /* Right red booth seat */
    obj_rect(OBJ_DINER_BOOTH, 11, 22, 10, 3, 2);  /* White Formica tabletop */
    obj_rect(OBJ_DINER_BOOTH, 15, 25, 2, 7, 7);   /* Chrome pedestal */

    /* 3. OBJ_JUKEBOX: Classic arched glowing jukebox */
    obj_rect(OBJ_JUKEBOX, 10, 10, 12, 24, 52);
    obj_rect(OBJ_JUKEBOX, 9,  11,  2, 22, 113);   /* Neon orange side tubes */
    obj_rect(OBJ_JUKEBOX, 21, 11,  2, 22, 113);
    obj_rect(OBJ_JUKEBOX, 11,  9, 10,  3, 96);    /* Gold arch top */
    obj_rect(OBJ_JUKEBOX, 12, 13,  8,  7, 81);    /* Cyan selector glass */

    /* 4. OBJ_CORN: Tall Pennsylvania Cornstalks */
    for (x = 6; x < 26; x += 4) {
        for (y = 8; y < 34; y++) {
            int sway = ((y / 4) + x) & 1;
            spr_objects[OBJ_CORN][y * OBJ_W + x + sway] = (y < 14) ? 97 : 20;
            if (y % 5 == 0) {
                spr_objects[OBJ_CORN][y * OBJ_W + x - 1] = 22;
                spr_objects[OBJ_CORN][y * OBJ_W + x + 2] = 19;
            }
        }
    }

    /* 5. OBJ_STREETLAMP: Route 309 tall streetlamp */
    obj_rect(OBJ_STREETLAMP, 14, 31, 4, 4, 8);    /* Iron base */
    obj_rect(OBJ_STREETLAMP, 15,  6, 2, 26, 6);   /* Vertical pole */
    obj_rect(OBJ_STREETLAMP, 15,  5, 7, 2, 5);    /* Overhang arm */
    obj_rect(OBJ_STREETLAMP, 20,  6, 4, 3, 96);   /* Bright sodium vapor lamp head */

    /* 6. OBJ_CAR_HOOD: Parked Police Cruiser / Sedan */
    obj_rect(OBJ_CAR_HOOD, 4, 20, 24, 12, 68);    /* Dark blue body */
    obj_rect(OBJ_CAR_HOOD, 8, 15, 16,  6, 82);    /* Windshield glass */
    obj_rect(OBJ_CAR_HOOD, 13, 13, 3,  2, 32);    /* Red roof lightbar */
    obj_rect(OBJ_CAR_HOOD, 16, 13, 3,  2, 64);    /* Blue roof lightbar */
    obj_rect(OBJ_CAR_HOOD, 6, 29,  5,  5, 13);    /* Front tire */
    obj_rect(OBJ_CAR_HOOD, 21, 29, 5,  5, 13);    /* Rear tire */

    /* 7. OBJ_UFO_CORE: Alien Scout Elerium-115 Power Source */
    obj_rect(OBJ_UFO_CORE, 8, 22, 16, 12, 165);   /* Alien alloy base */
    obj_rect(OBJ_UFO_CORE, 10, 12, 12, 12, 145);  /* Glowing Elerium sphere */
    obj_rect(OBJ_UFO_CORE, 12, 14,  8,  8, 144);  /* Bright lime core */
    obj_rect(OBJ_UFO_CORE, 14, 16,  4,  4, 0);    /* White-hot center */

    /* 8. OBJ_CRATE: Tactical Equipment Crate */
    obj_rect(OBJ_CRATE, 8, 20, 16, 13, 53);
    obj_rect(OBJ_CRATE, 8, 20, 16,  2, 50);
    obj_rect(OBJ_CRATE, 10, 22, 2, 10, 56);
    obj_rect(OBJ_CRATE, 20, 22, 2, 10, 56);

    /* 9. OBJ_STAIRS_UP: Isometric staircase to upper floor */
    for (i = 0; i < 6; i++) {
        obj_rect(OBJ_STAIRS_UP, 6 + i * 2, 28 - i * 4, 14, 4, (uint8_t)(4 + (i & 1) * 2));
        obj_rect(OBJ_STAIRS_UP, 6 + i * 2, 29 - i * 4, 14, 3, 9);
    }

    /* 10. OBJ_RUBBLE: Smoldering destroyed wall chunks */
    for (i = 0; i < 45; i++) {
        int rx = 6 + ((i * 19) % 20);
        int ry = 24 + ((i * 11) % 10);
        spr_objects[OBJ_RUBBLE][ry * OBJ_W + rx] = (i & 1) ? 8 : 39;
    }

    /* 11. OBJ_FLARE: Glowing Night Electro-Flare */
    obj_rect(OBJ_FLARE, 13, 28, 6, 4, 113); /* Orange flare casing */
    obj_rect(OBJ_FLARE, 14, 26, 4, 4, 96);  /* Bright gold halo */
    obj_rect(OBJ_FLARE, 15, 27, 2, 2, 0);   /* White-hot flare core */

    /* 12. OBJ_BARREL: Volatile Red/Yellow Hazard Fuel Drum */
    obj_rect(OBJ_BARREL, 10, 17, 12, 16, 34); /* Red steel drum */
    obj_rect(OBJ_BARREL, 10, 17, 12,  2, 32); /* Top rim */
    obj_rect(OBJ_BARREL, 10, 23, 12,  3, 96); /* Yellow hazard band */
    obj_rect(OBJ_BARREL, 14, 24,  4,  2, 15); /* Hazard diamond symbol */

    /* 13. OBJ_TRACTOR: Classic Green/Yellow PA Farm Tractor */
    obj_rect(OBJ_TRACTOR,  5, 19, 20,  9, 18); /* John-Deere style green body */
    obj_rect(OBJ_TRACTOR,  6, 13, 10,  6, 20); /* Cab frame */
    obj_rect(OBJ_TRACTOR, 19, 13,  2,  6, 12); /* Exhaust stack */
    obj_rect(OBJ_TRACTOR,  5, 23,  8,  9, 13); /* Huge rear tire */
    obj_rect(OBJ_TRACTOR,  7, 25,  4,  5, 96); /* Yellow rear wheel hub */
    obj_rect(OBJ_TRACTOR, 19, 26,  6,  6, 13); /* Front tire */
    obj_rect(OBJ_TRACTOR, 21, 28,  2,  2, 96); /* Yellow front hub */
}

/* Build 8-directional sprites for DUFF-COM Soldiers, Sectoids, Floaters, and Mutons */
static void build_units(void) {
    int type, pose, dir, x, y;
    for (type = 0; type < NUM_UNIT_SPRITES; type++) {
        for (pose = 0; pose < 4; pose++) {
            for (dir = 0; dir < 8; dir++) {
                uint8_t *dst = spr_units[type][pose][dir];
                int y_off = (pose == 1) ? 5 : ((pose >= 2) ? -1 : 0);
                int leg_l_off = (pose == 2) ? -2 : (pose == 3 ?  2 : 0);
                int leg_r_off = (pose == 2) ?  2 : (pose == 3 ? -2 : 0);
                memset(dst, COL_TRANS, UNIT_W * UNIT_H);

                /* Drop shadow under feet */
                for (y = 26; y <= 29; y++) {
                    for (x = 7; x <= 16; x++) {
                        dst[y * UNIT_W + x] = 14;
                    }
                }

                if (type < UNIT_SPR_SECTOID) {
                    /* DUFF-COM Tactical Operative Fallback (Male 0..2 & Female 3..5) */
                    int is_fem = (type >= UNIT_SPR_FEM_RED);
                    uint8_t hair_c = (type == UNIT_SPR_MALE_BLONDE || type == UNIT_SPR_FEM_BLONDE) ? 98 :
                                     (type == UNIT_SPR_FEM_RED) ? 36 : 56;
                    uint8_t skin_c = (type == UNIT_SPR_MALE_VET || type == UNIT_SPR_FEM_DARK) ? 56 : 244;
                    for (y = 20 + (pose == 1 ? 2 : 0); y < 28 + leg_l_off / 2; y++) {
                        int lx = 9 + (leg_l_off / 2);
                        if (y < UNIT_H) {
                            dst[y * UNIT_W + lx]     = 70;
                            dst[y * UNIT_W + lx + 1] = 68;
                        }
                    }
                    for (y = 20 + (pose == 1 ? 2 : 0); y < 28 + leg_r_off / 2; y++) {
                        int rx = 13 + (leg_r_off / 2);
                        if (y < UNIT_H) {
                            dst[y * UNIT_W + rx]     = 68;
                            dst[y * UNIT_W + rx + 1] = 70;
                        }
                    }
                    /* Tactical Chest Armor */
                    for (y = 12 + y_off; y < 21 + (pose == 1 ? 2 : 0); y++) {
                        int x0 = is_fem && y >= 15 + y_off ? 9 : 8;
                        int x1 = is_fem && y >= 15 + y_off ? 14 : 15;
                        for (x = x0; x <= x1; x++) {
                            dst[y * UNIT_W + x] = (x == x0 || x == x1) ? 69 : 65;
                        }
                    }
                    /* Gold DUFF-COM chest badge */
                    dst[(14 + y_off) * UNIT_W + 11] = 96;
                    dst[(14 + y_off) * UNIT_W + 12] = 96;

                    /* Hair & Skin/Visor */
                    for (y = 6 + y_off; y < 12 + y_off; y++) {
                        for (x = 9; x <= 14; x++) {
                            dst[y * UNIT_W + x] = (y < 9 + y_off || dir == 0 || dir == 1 || dir == 7) ? hair_c : skin_c;
                        }
                    }
                    if (dir >= 2 && dir <= 6) {
                        int vx = (dir == 2) ? 13 : (dir == 6) ? 9 : 10;
                        dst[(9 + y_off) * UNIT_W + vx]     = (type == 0) ? 80 : 14;
                        dst[(9 + y_off) * UNIT_W + vx + 1] = (type == 0) ? 80 : 14;
                    }

                    /* Rifle barrel pointing in `dir` */
                    {
                        static const int dx_tab[8] = { 0,  4,  6,  4,  0, -4, -6, -4};
                        static const int dy_tab[8] = {-4, -3,  0,  3,  4,  3,  0, -3};
                        int gx = 12 + dx_tab[dir];
                        int gy = 16 + y_off + dy_tab[dir];
                        int s;
                        for (s = 0; s <= 4; s++) {
                            int px = 12 + (dx_tab[dir] * s) / 4;
                            int py = 16 + y_off + (dy_tab[dir] * s) / 4;
                            if ((unsigned)px < UNIT_W && (unsigned)py < UNIT_H)
                                dst[py * UNIT_W + px] = 4;
                        }
                        if ((unsigned)gx < UNIT_W && (unsigned)gy < UNIT_H)
                            dst[gy * UNIT_W + gx] = 96;
                    }
                } else if (type == UNIT_SPR_SECTOID) {
                    /* SECTOID: Classic gray alien with oversized cranium & huge black eyes */
                    for (y = 21 + (pose == 1 ? 2 : 0); y < 28; y++) {
                        dst[y * UNIT_W + 10 + (leg_l_off / 2)] = 8;
                        dst[y * UNIT_W + 13 + (leg_r_off / 2)] = 8;
                    }
                    for (y = 14 + y_off; y < 21 + (pose == 1 ? 2 : 0); y++) {
                        for (x = 9; x <= 14; x++) dst[y * UNIT_W + x] = 7;
                    }
                    for (y = 6 + y_off; y < 14 + y_off; y++) {
                        for (x = 8; x <= 15; x++) dst[y * UNIT_W + x] = 5;
                    }
                    dst[(10 + y_off) * UNIT_W + 9]  = 15;
                    dst[(10 + y_off) * UNIT_W + 10] = 15;
                    dst[(10 + y_off) * UNIT_W + 13] = 15;
                    dst[(10 + y_off) * UNIT_W + 14] = 15;
                    dst[(16 + y_off) * UNIT_W + 15] = 144;
                    dst[(16 + y_off) * UNIT_W + 16] = 145;
                } else if (type == UNIT_SPR_FLOATER) {
                    /* FLOATER: Maroon cape + cybernetic hovering jetpack base */
                    int bob = (pose >= 2) ? -2 : 0;
                    for (y = 19 + bob; y < 26 + bob; y++) {
                        for (x = 9; x <= 14; x++) dst[y * UNIT_W + x] = (y > 23 + bob) ? 114 : 6;
                    }
                    for (y = 11 + bob; y < 19 + bob; y++) {
                        for (x = 7; x <= 16; x++) dst[y * UNIT_W + x] = 35;
                    }
                    for (y = 6 + bob; y < 11 + bob; y++) {
                        for (x = 9; x <= 14; x++) dst[y * UNIT_W + x] = 51;
                    }
                    dst[(8 + bob) * UNIT_W + 10] = 96;
                    dst[(8 + bob) * UNIT_W + 13] = 96;
                } else {
                    /* MUTON: Heavy Muscular Green/Purple Alien Shock Trooper */
                    for (y = 20 + (pose == 1 ? 2 : 0); y < 28; y++) {
                        dst[y * UNIT_W +  8 + (leg_l_off / 2)] = 132;
                        dst[y * UNIT_W +  9 + (leg_l_off / 2)] = 130;
                        dst[y * UNIT_W + 14 + (leg_r_off / 2)] = 130;
                        dst[y * UNIT_W + 15 + (leg_r_off / 2)] = 132;
                    }
                    /* Broad green-armored chest & shoulder pads */
                    for (y = 11 + y_off; y < 21 + (pose == 1 ? 2 : 0); y++) {
                        for (x = 6; x <= 17; x++) {
                            dst[y * UNIT_W + x] = (y < 14 + y_off) ? 18 : 21;
                        }
                    }
                    /* Purple/magenta cyber-head */
                    for (y = 5 + y_off; y < 11 + y_off; y++) {
                        for (x = 9; x <= 14; x++) dst[y * UNIT_W + x] = 131;
                    }
                    dst[(8 + y_off) * UNIT_W + 10] = 96;
                    dst[(8 + y_off) * UNIT_W + 13] = 96;
                    /* Heavy Plasma Cannon */
                    dst[(15 + y_off) * UNIT_W + 16] = 144;
                    dst[(15 + y_off) * UNIT_W + 17] = 145;
                    dst[(16 + y_off) * UNIT_W + 16] = 144;
                }
            }
        }
    }
}

static void build_deaths(void) {
    int type, stg, x, y;
    for (type = 0; type < NUM_UNIT_SPRITES; type++) {
        uint8_t body_hi, body_sh, head_c, leg_c, blood_c;
        if (type < UNIT_SPR_SECTOID) {
            body_hi = 65;  body_sh = 69;
            head_c  = (type == UNIT_SPR_MALE_BLONDE || type == UNIT_SPR_FEM_BLONDE) ? 98 :
                      (type == UNIT_SPR_FEM_RED) ? 36 :
                      (type == UNIT_SPR_FEM_DARK) ? 14 : 56;
            leg_c   = 68;  blood_c = 38;
        } else if (type == UNIT_SPR_SECTOID) {
            body_hi = 6;   body_sh = 8;
            head_c  = 5;   leg_c   = 8;  blood_c = 21;
        } else if (type == UNIT_SPR_FLOATER) {
            body_hi = 35;  body_sh = 39;
            head_c  = 51;  leg_c   = 7;  blood_c = 39;
        } else {
            body_hi = 18;  body_sh = 21;
            head_c  = 131; leg_c   = 130; blood_c = 21;
        }

        for (stg = 0; stg < 4; stg++) {
            uint8_t *dst = spr_deaths[type][stg];
            memset(dst, COL_TRANS, DEATH_W * DEATH_H);

            if (stg == 0) {
                /* Stage 0: Thrown backward in agony + impact burst */
                for (y = 19; y < 26; y++) {
                    dst[y * DEATH_W + 13] = leg_c;
                    dst[y * DEATH_W + 18] = leg_c;
                }
                for (y = 11; y < 19; y++) {
                    for (x = 12; x <= 19; x++) dst[y * DEATH_W + x] = (x < 16) ? body_hi : body_sh;
                }
                for (y = 5; y < 11; y++) {
                    for (x = 11; x <= 16; x++) dst[y * DEATH_W + x] = head_c;
                }
                /* Impact blood/ichor burst */
                dst[13 * DEATH_W + 15] = blood_c;
                dst[14 * DEATH_W + 17] = blood_c;
                dst[12 * DEATH_W + 19] = blood_c;
            } else if (stg == 1) {
                /* Stage 1: Buckling onto knees */
                for (y = 21; y < 26; y++) {
                    for (x = 13; x <= 20; x++) dst[y * DEATH_W + x] = leg_c;
                }
                for (y = 14; y < 22; y++) {
                    for (x = 11; x <= 19; x++) dst[y * DEATH_W + x] = (x < 15) ? body_hi : body_sh;
                }
                for (y = 9; y < 15; y++) {
                    for (x = 9; x <= 14; x++) dst[y * DEATH_W + x] = head_c;
                }
                dst[24 * DEATH_W + 15] = blood_c;
                dst[25 * DEATH_W + 16] = blood_c;
            } else if (stg == 2) {
                /* Stage 2: Pitching horizontally onto floor */
                for (y = 20; y <= 25; y++) {
                    for (x = 12; x <= 23; x++) {
                        if (in_diamond(x, y - 16))
                            dst[y * DEATH_W + x] = (x < 18) ? body_hi : leg_c;
                    }
                }
                for (y = 18; y <= 22; y++) {
                    for (x = 8; x <= 12; x++) {
                        if (in_diamond(x, y - 16)) dst[y * DEATH_W + x] = head_c;
                    }
                }
            } else {
                /* Stage 3: Persistent Prone Corpse inside the 32x16 floor diamond */
                for (y = 22; y <= 26; y++) {
                    for (x = 11; x <= 19; x++) {
                        if (in_diamond(x, y - 16)) dst[y * DEATH_W + x] = blood_c;
                    }
                }
                for (y = 21; y <= 25; y++) {
                    for (x = 11; x <= 18; x++) {
                        if (in_diamond(x, y - 16))
                            dst[y * DEATH_W + x] = (y <= 22) ? body_hi : body_sh;
                    }
                }
                for (y = 22; y <= 26; y++) {
                    for (x = 18; x <= 24; x++) {
                        if (in_diamond(x, y - 16)) dst[y * DEATH_W + x] = leg_c;
                    }
                }
                for (y = 20; y <= 24; y++) {
                    for (x = 7; x <= 11; x++) {
                        if (in_diamond(x, y - 16)) dst[y * DEATH_W + x] = head_c;
                    }
                }
            }
        }
    }
}

static void fx_pset(uint8_t *dst, int x, int y, uint8_t col) {
    if ((unsigned)x < FX_W && (unsigned)y < FX_H) {
        dst[y * FX_W + x] = col;
    }
}

static void build_fx(void) {
    /* 8 screen-space isometric flight vectors:
     * 0=Up, 1=Iso-UR(+2,-1), 2=Right, 3=Iso-DR(+2,+1),
     * 4=Down, 5=Iso-DL(-2,+1), 6=Left, 7=Iso-UL(-2,-1) */
    static const int vx_tab[8] = {  0,  2,  2,  2,  0, -2, -2, -2 };
    static const int vy_tab[8] = { -2, -1,  0,  1,  2,  1,  0, -1 };
    int d, s, t, f, x, y;

    /* 1. Directional Weapon Projectiles (Bullet, Laser Bolt, Plasma Orb+Tail) */
    for (t = 0; t < PROJ_TYPE_COUNT; t++) {
        for (d = 0; d < 8; d++) {
            uint8_t *dst = spr_proj[t][d];
            int dx = vx_tab[d], dy = vy_tab[d];
            memset(dst, COL_TRANS, FX_W * FX_H);

            if (t == PROJ_BULLET) {
                for (s = -4; s <= 2; s++) {
                    int px = 12 + (dx * s) / 2;
                    int py = 12 + (dy * s) / 2;
                    uint8_t c = (s == 2) ? 0 : (s >= 0) ? 96 : (s >= -2) ? 114 : 7;
                    fx_pset(dst, px, py, c);
                    if (s >= 0) fx_pset(dst, px + (dy != 0 ? 1 : 0), py + (dx != 0 ? 1 : 0), 99);
                }
            } else if (t == PROJ_LASER) {
                for (s = -5; s <= 4; s++) {
                    int px = 12 + (dx * s) / 2;
                    int py = 12 + (dy * s) / 2;
                    fx_pset(dst, px - 1, py, 66);
                    fx_pset(dst, px + 1, py, 66);
                    fx_pset(dst, px, py - 1, 81);
                    fx_pset(dst, px, py + 1, 81);
                    fx_pset(dst, px, py, (s >= -2 && s <= 3) ? 0 : 80);
                }
            } else {
                for (s = -5; s <= 0; s++) {
                    int px = 12 + (dx * s) / 2;
                    int py = 12 + (dy * s) / 2;
                    fx_pset(dst, px, py, (s >= -2) ? 144 : 19);
                    fx_pset(dst, px + 1, py, 21);
                }
                for (y = -2; y <= 2; y++) {
                    for (x = -2; x <= 2; x++) {
                        int r2 = x * x + y * y;
                        if (r2 <= 5) {
                            fx_pset(dst, 12 + x, 12 + y, (r2 <= 1) ? 0 : (r2 <= 3 ? 144 : 18));
                        }
                    }
                }
            }
        }
    }

    /* 2. Thrown Munitions: Spinning HE Grenade [0] & Sparking Electro-Flare [1] */
    for (f = 0; f < 4; f++) {
        uint8_t *gr = spr_thrown[0][f];
        uint8_t *fl = spr_thrown[1][f];
        memset(gr, COL_TRANS, FX_W * FX_H);
        memset(fl, COL_TRANS, FX_W * FX_H);
        for (y = 9; y <= 15; y++) {
            for (x = 9; x <= 14; x++) {
                gr[y * FX_W + x] = ((x + y + f) & 1) ? 22 : 25;
            }
        }
        fx_pset(gr, 11 + ((f == 1) ? 2 : (f == 3 ? -2 : 0)),
                    11 + ((f == 0) ? -3 : (f == 2 ? 4 : 0)), 96);

        for (s = -3; s <= 3; s++) {
            int px = 12 + ((f & 1) ? s : (f == 0 ? s : -s));
            int py = 12 + ((f & 1) ? -s : s / 2);
            fx_pset(fl, px, py, (s >= 2) ? 96 : 33);
            fx_pset(fl, px + 1, py, (s >= 2) ? 0 : 36);
        }
    }

    /* 3. Weapon Muzzle Flashes (0=Bullet, 1=Laser, 2=Plasma) */
    for (t = 0; t < PROJ_TYPE_COUNT; t++) {
        uint8_t *mz = spr_muzzle[t];
        uint8_t hi = (t == 0) ? 96  : (t == 1) ? 80 : 144;
        uint8_t lo = (t == 0) ? 113 : (t == 1) ? 66 : 18;
        memset(mz, COL_TRANS, FX_W * FX_H);
        for (y = -5; y <= 5; y++) {
            for (x = -5; x <= 5; x++) {
                int ax = abs(x), ay = abs(y);
                if (ax + ay <= 3) fx_pset(mz, 12 + x, 12 + y, (ax + ay <= 1) ? 0 : hi);
                else if ((ax <= 1 || ay <= 1 || ax == ay) && ax <= 5 && ay <= 5)
                    fx_pset(mz, 12 + x, 12 + y, lo);
            }
        }
    }

    /* 4. Targeting Reticles (0=Clear Lock-On, 1=Blocked LOS) centered at (12, 12) */
    for (t = 0; t < 2; t++) {
        uint8_t *rt = spr_reticle[t];
        uint8_t ring_c = (t == 0) ? 32 : 37;
        uint8_t brk_c  = (t == 0) ? 96 : 114;
        memset(rt, COL_TRANS, FX_W * FX_H);

        /* Corner rangefinder brackets */
        for (s = 0; s < 3; s++) {
            fx_pset(rt, 5 + s, 5, brk_c);  fx_pset(rt, 5, 5 + s, brk_c);
            fx_pset(rt, 19 - s, 5, brk_c); fx_pset(rt, 19, 5 + s, brk_c);
            fx_pset(rt, 5 + s, 19, brk_c); fx_pset(rt, 5, 19 - s, brk_c);
            fx_pset(rt, 19 - s, 19, brk_c);fx_pset(rt, 19, 19 - s, brk_c);
        }
        /* Cardinal crosshair arms */
        for (s = 3; s <= 6; s++) {
            fx_pset(rt, 12 - s, 12, ring_c);
            fx_pset(rt, 12 + s, 12, ring_c);
            fx_pset(rt, 12, 12 - s, ring_c);
            fx_pset(rt, 12, 12 + s, ring_c);
        }
        if (t == 0) {
            fx_pset(rt, 12, 12, 0);
        } else {
            fx_pset(rt, 11, 11, 33); fx_pset(rt, 13, 13, 33);
            fx_pset(rt, 13, 11, 33); fx_pset(rt, 11, 13, 33);
        }
        /* Crisp 1px black outline for visibility over all daylight/night tiles */
        for (y = 1; y < FX_H - 1; y++) {
            for (x = 1; x < FX_W - 1; x++) {
                if (rt[y * FX_W + x] == COL_TRANS) {
                    uint8_t n0 = rt[y * FX_W + (x - 1)];
                    uint8_t n1 = rt[y * FX_W + (x + 1)];
                    uint8_t n2 = rt[(y - 1) * FX_W + x];
                    uint8_t n3 = rt[(y + 1) * FX_W + x];
                    if ((n0 != COL_TRANS && n0 != 15) || (n1 != COL_TRANS && n1 != 15) ||
                        (n2 != COL_TRANS && n2 != 15) || (n3 != COL_TRANS && n3 != 15)) {
                        rt[y * FX_W + x] = 15;
                    }
                }
            }
        }
    }

    /* 5. 4-Frame Impact Splash Animations across all 6 Splash Types */
    for (t = 0; t < SPLASH_TYPE_COUNT; t++) {
        uint8_t c_core = 0, c_mid = 96, c_out = 113;
        if (t == SPLASH_LASER)          { c_mid = 80;  c_out = 66;  }
        else if (t == SPLASH_PLASMA)    { c_mid = 144; c_out = 18;  }
        else if (t == SPLASH_GLASS)     { c_mid = 81;  c_out = 4;   }
        else if (t == SPLASH_HIT_HUMAN) { c_mid = 33;  c_out = 37;  }
        else if (t == SPLASH_HIT_ALIEN) { c_mid = 144; c_out = 21;  }

        for (f = 0; f < 4; f++) {
            uint8_t *sp = spr_splash[t][f];
            int rad = 3 + f * 3;
            memset(sp, COL_TRANS, FX_W * FX_H);
            for (y = -rad; y <= rad; y++) {
                for (x = -rad; x <= rad; x++) {
                    int d2 = x * x + y * y;
                    if (f == 0 && d2 <= 9) {
                        fx_pset(sp, 12 + x, 12 + y, (d2 <= 2) ? c_core : c_mid);
                    } else if (f == 1 && d2 >= 4 && d2 <= 36 && ((x + y) & 1) == 0) {
                        fx_pset(sp, 12 + x, 12 + y, (d2 <= 14) ? c_mid : c_out);
                    } else if (f == 2 && d2 >= 16 && d2 <= 72 && ((x * 3 + y * 5) & 3) == 0) {
                        fx_pset(sp, 12 + x, 12 + y, c_out);
                    } else if (f == 3 && d2 >= 25 && d2 <= 100 && ((x * 7 + y * 3) & 7) == 0) {
                        fx_pset(sp, 12 + x, 12 + y, (t == SPLASH_BULLET) ? 8 : c_out);
                    }
                }
            }
        }
    }
}

static void build_hud_sprites(void) {
    int p, w, x, y;
    /* Soldier portraits (12 x 32x36: 0..5 Male, 6..11 Female) */
    for (p = 0; p < NUM_PORTRAITS; p++) {
        for (y = 0; y < 36; y++) {
            for (x = 0; x < 32; x++) {
                uint8_t c = 74; /* Dark blue portrait background */
                if (y >= 26 && x >= 4 && x <= 27) c = 66;
                if (y >= 9 && y < 26 && x >= 9 && x <= 22) c = (uint8_t)(242 + (p % 4));
                if (y >= 5 && y < 12 && x >= 8 && x <= 23) {
                    static const uint8_t hair_cols[12] = {56, 98, 60, 54, 14, 4, 36, 97, 14, 59, 55, 2};
                    c = hair_cols[p];
                }
                if (y == 15 && (x == 12 || x == 13 || x == 18 || x == 19)) {
                    c = (p == 0 || p == 8) ? 80 : 14;
                }
                if (y == 21 && x >= 13 && x <= 18) c = (p >= 6) ? 38 : 54;
                spr_portraits[p][y * 32 + x] = c;
            }
        }
    }

    /* Weapon Icons (40x24): 0=Assault Rifle, 1=Laser Rifle, 2=Plasma Rifle, 3=HE Grenade */
    for (w = 0; w < 4; w++) {
        memset(spr_weapons[w], COL_TRANS, 40 * 24);
        if (w == 0) {
            /* Classic X-COM Bullpup Assault Rifle */
            for (x = 6; x < 34; x++) {
                spr_weapons[w][10 * 40 + x] = 5;
                spr_weapons[w][11 * 40 + x] = 7;
            }
            for (y = 9; y < 16; y++) {
                for (x = 6; x < 14; x++) spr_weapons[w][y * 40 + x] = 54; /* Stock */
                for (x = 18; x < 22; x++) spr_weapons[w][y * 40 + x] = 8;  /* Magazine */
            }
        } else if (w == 1) {
            /* Coopersburg Prototype Laser Rifle (Blue/Cyan emitter) */
            for (x = 5; x < 35; x++) {
                spr_weapons[w][10 * 40 + x] = 66;
                spr_weapons[w][11 * 40 + x] = 81;
                spr_weapons[w][12 * 40 + x] = 68;
            }
        } else if (w == 2) {
            /* Heavy Plasma (Alien Green) */
            for (x = 6; x < 34; x++) {
                spr_weapons[w][9  * 40 + x] = 164;
                spr_weapons[w][10 * 40 + x] = 145;
                spr_weapons[w][11 * 40 + x] = 144;
                spr_weapons[w][12 * 40 + x] = 166;
            }
        } else if (w == 3) {
            /* High-Explosive Alien/Military Grenade */
            for (y = 6; y < 18; y++) {
                for (x = 14; x < 26; x++) {
                    spr_weapons[w][y * 40 + x] = ((x + y) & 1) ? 22 : 25;
                }
            }
            for (x = 17; x < 23; x++) spr_weapons[w][4 * 40 + x] = 96; /* Brass pin/fuse */
        }
    }
}

static void load_bmp_overrides(void) {
    uint8_t *sheet;
    int sw = 0, sh = 0, i, t, p, d;

    bmp_init_quantizer();

    /* 1. FLOORS.BMP (10 tiles x 32x16) */
    sheet = bmp_load_alloc("FLOORS.BMP", &sw, &sh);
    if (sheet) {
        for (i = 1; i < FLOOR_COUNT && (i + 1) * TILE_W <= sw; i++) {
            bmp_extract_tile(sheet, sw, sh, i * TILE_W, 0, TILE_W, TILE_H, spr_floors[i]);
        }
        free(sheet);
    }

    /* 2. WALLS_W.BMP & WALLS_N.BMP (10 walls x 16x32) */
    sheet = bmp_load_alloc("WALLS_W.BMP", &sw, &sh);
    if (sheet) {
        for (i = 1; i < WALL_COUNT && (i + 1) * WALL_W <= sw; i++) {
            bmp_extract_tile(sheet, sw, sh, i * WALL_W, 0, WALL_W, WALL_H, spr_walls_west[i]);
        }
        free(sheet);
    }
    sheet = bmp_load_alloc("WALLS_N.BMP", &sw, &sh);
    if (sheet) {
        for (i = 1; i < WALL_COUNT && (i + 1) * WALL_W <= sw; i++) {
            bmp_extract_tile(sheet, sw, sh, i * WALL_W, 0, WALL_W, WALL_H, spr_walls_north[i]);
        }
        free(sheet);
    }

    /* 3. OBJECTS.BMP (14 props x 32x40) */
    sheet = bmp_load_alloc("OBJECTS.BMP", &sw, &sh);
    if (sheet) {
        for (i = 1; i < OBJ_COUNT && (i + 1) * OBJ_W <= sw; i++) {
            bmp_extract_tile(sheet, sw, sh, i * OBJ_W, 0, OBJ_W, OBJ_H, spr_objects[i]);
        }
        free(sheet);
    }

    /* 4. UNITS.BMP (8 cols [dir 0..7] x 36 rows [sprite_id*4 + pose] of 24x32) */
    sheet = bmp_load_alloc("UNITS.BMP", &sw, &sh);
    if (sheet) {
        for (t = 0; t < NUM_UNIT_SPRITES; t++) {
            for (p = 0; p < 4; p++) {
                int row = t * 4 + p;
                if ((row + 1) * UNIT_H > sh) continue;
                for (d = 0; d < 8 && (d + 1) * UNIT_W <= sw; d++) {
                    bmp_extract_tile(sheet, sw, sh, d * UNIT_W, row * UNIT_H,
                                     UNIT_W, UNIT_H, spr_units[t][p][d]);
                }
            }
        }
        free(sheet);
    }

    /* 4b. DEATH.BMP (4 cols [stage 0..3] x 9 rows [sprite_id 0..8] of 32x32) */
    sheet = bmp_load_alloc("DEATH.BMP", &sw, &sh);
    if (sheet) {
        for (t = 0; t < NUM_UNIT_SPRITES && (t + 1) * DEATH_H <= sh; t++) {
            for (p = 0; p < 4 && (p + 1) * DEATH_W <= sw; p++) {
                bmp_extract_tile(sheet, sw, sh, p * DEATH_W, t * DEATH_H,
                                 DEATH_W, DEATH_H, spr_deaths[t][p]);
            }
        }
        free(sheet);
    }

    /* 4c. FX.BMP (8 cols x 8 rows of 24x24 = 192x192: Projectiles, Thrown, Muzzle, Reticles, Splashes) */
    sheet = bmp_load_alloc("FX.BMP", &sw, &sh);
    if (sheet && sw >= 8 * FX_W && sh >= 8 * FX_H) {
        for (t = 0; t < PROJ_TYPE_COUNT; t++) {
            for (d = 0; d < 8; d++) {
                bmp_extract_tile(sheet, sw, sh, d * FX_W, t * FX_H, FX_W, FX_H, spr_proj[t][d]);
            }
        }
        for (p = 0; p < 4; p++) {
            bmp_extract_tile(sheet, sw, sh, p * FX_W,       3 * FX_H, FX_W, FX_H, spr_thrown[0][p]);
            bmp_extract_tile(sheet, sw, sh, (4 + p) * FX_W, 3 * FX_H, FX_W, FX_H, spr_thrown[1][p]);
        }
        for (t = 0; t < PROJ_TYPE_COUNT; t++) {
            bmp_extract_tile(sheet, sw, sh, t * FX_W, 4 * FX_H, FX_W, FX_H, spr_muzzle[t]);
        }
        bmp_extract_tile(sheet, sw, sh, 3 * FX_W, 4 * FX_H, FX_W, FX_H, spr_reticle[0]);
        bmp_extract_tile(sheet, sw, sh, 4 * FX_W, 4 * FX_H, FX_W, FX_H, spr_reticle[1]);

        for (p = 0; p < 4; p++) {
            bmp_extract_tile(sheet, sw, sh, p * FX_W,       5 * FX_H, FX_W, FX_H, spr_splash[SPLASH_BULLET][p]);
            bmp_extract_tile(sheet, sw, sh, (4 + p) * FX_W, 5 * FX_H, FX_W, FX_H, spr_splash[SPLASH_LASER][p]);
            bmp_extract_tile(sheet, sw, sh, p * FX_W,       6 * FX_H, FX_W, FX_H, spr_splash[SPLASH_PLASMA][p]);
            bmp_extract_tile(sheet, sw, sh, (4 + p) * FX_W, 6 * FX_H, FX_W, FX_H, spr_splash[SPLASH_GLASS][p]);
            bmp_extract_tile(sheet, sw, sh, p * FX_W,       7 * FX_H, FX_W, FX_H, spr_splash[SPLASH_HIT_HUMAN][p]);
            bmp_extract_tile(sheet, sw, sh, (4 + p) * FX_W, 7 * FX_H, FX_W, FX_H, spr_splash[SPLASH_HIT_ALIEN][p]);
        }
        free(sheet);
    } else if (sheet) {
        free(sheet);
    }

    /* 5. PORTRAIT.BMP (12 portraits x 32x36: 0..5 Male, 6..11 Female) */
    sheet = bmp_load_alloc("PORTRAIT.BMP", &sw, &sh);
    if (sheet) {
        for (i = 0; i < NUM_PORTRAITS && (i + 1) * 32 <= sw; i++) {
            bmp_extract_tile(sheet, sw, sh, i * 32, 0, 32, 36, spr_portraits[i]);
        }
        free(sheet);
    }

    /* 6. Full-screen Title Background, Hidden Movement Poster & Radar Map */
    sheet = bmp_load_alloc("TITLEBG.BMP", &sw, &sh);
    if (sheet && sw == 320 && sh == 200) {
        gfx_title_bg = sheet;
    } else if (sheet) {
        free(sheet);
    }

    sheet = bmp_load_alloc("HIDDEN.BMP", &sw, &sh);
    if (sheet && sw == 232 && sh == 92) {
        gfx_hidden_bg = sheet;
    } else if (sheet) {
        free(sheet);
    }

    sheet = bmp_load_alloc("RADAR.BMP", &sw, &sh);
    if (sheet && sw == 300 && sh == 132) {
        gfx_radar_bg = sheet;
    } else if (sheet) {
        free(sheet);
    }
}

int gfx_portrait_to_unit_sprite(int portrait_id) {
    switch (portrait_id % NUM_PORTRAITS) {
        case 0: case 4:         return UNIT_SPR_MALE_VISOR;
        case 1: case 3: case 5: return UNIT_SPR_MALE_BLONDE;
        case 2:                 return UNIT_SPR_MALE_VET;
        case 6: case 10:        return UNIT_SPR_FEM_RED;
        case 7: case 11:        return UNIT_SPR_FEM_BLONDE;
        case 8: case 9:         return UNIT_SPR_FEM_DARK;
        default:                return UNIT_SPR_MALE_VISOR;
    }
}

void gfx_init_assets(void) {
    build_floors();
    build_walls();
    build_objects();
    build_units();
    build_deaths();
    build_fx();
    build_hud_sprites();
    load_bmp_overrides();
}
