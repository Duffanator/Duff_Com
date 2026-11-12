#include "gfx_assets.h"
#include "items.h"
#include "bmp_loader.h"
#include "vga.h"

#include <stdlib.h>
#include <string.h>

uint8_t spr_floors[FLOOR_COUNT][TILE_W * TILE_H];
uint8_t spr_walls_west[WALL_COUNT][WALL_W * WALL_H];
uint8_t spr_walls_north[WALL_COUNT][WALL_W * WALL_H];
uint8_t spr_objects[OBJ_COUNT][OBJ_W * OBJ_H];
uint8_t spr_units[NUM_UNIT_SPRITES][4][8][UNIT_W * UNIT_H];
uint8_t spr_unit_shadow[UNIT_W * UNIT_H];
uint8_t spr_deaths[NUM_UNIT_SPRITES][4][DEATH_W * DEATH_H];
uint8_t spr_proj[PROJ_TYPE_COUNT][8][FX_W * FX_H];
uint8_t spr_thrown[2][4][FX_W * FX_H];
uint8_t spr_muzzle[PROJ_TYPE_COUNT][FX_W * FX_H];
uint8_t spr_reticle[2][FX_W * FX_H];
uint8_t spr_splash[SPLASH_TYPE_COUNT][4][FX_W * FX_H];
uint8_t spr_portraits[NUM_PORTRAITS][32 * 36];
uint8_t spr_fullbody[NUM_PORTRAITS][FULLBODY_W * FULLBODY_H];
uint8_t spr_weapons[4][40 * 24];

uint8_t *gfx_title_bg  = NULL;
uint8_t *gfx_hidden_bg = NULL;
int      gfx_hidden_w  = 0;
int      gfx_hidden_h  = 0;
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

                    case FLOOR_UFO_DECK: {
                        int u = (x - 16) + (y - 8) * 2 + 64;
                        int v = (x - 16) - (y - 8) * 2 + 64;
                        if ((u % 12 == 0) || (v % 12 == 0))
                            col = ((u + v) % 24 == 0) ? 81 : 88;
                        else if ((u % 12 == 1) || (v % 12 == 1))
                            col = 132;
                        else
                            col = (uint8_t)(136 + (hash & 3));
                        break;
                    }

                    case FLOOR_UFO_ROOF:
                    case FLOOR_UFO_ROOF_N:
                    case FLOOR_UFO_ROOF_E:
                    case FLOOR_UFO_ROOF_S:
                    case FLOOR_UFO_ROOF_W: {
                        if (f != FLOOR_UFO_ROOF) {
                            int uf2 = x + 2 * y - 15;
                            int vf2 = 2 * y - x + 16;
                            int pu2 = (f == FLOOR_UFO_ROOF_N || f == FLOOR_UFO_ROOF_W) ? 30 : 0;
                            int pv2 = (f == FLOOR_UFO_ROOF_N || f == FLOOR_UFO_ROOF_E) ? 30 : 0;
                            int d2  = (uf2 - pu2) * (uf2 - pu2) + (vf2 - pv2) * (vf2 - pv2);
                            if (d2 > 930) { col = COL_TRANS; break; }
                            if (d2 >= 720) { col = (d2 >= 830) ? 162 : 81; break; }
                        }
                        col = (uint8_t)(is_diamond_edge(x, y) ? 132 : (162 + (hash & 3)));
                        if (x == 16 && y == 8) col = 81;
                        break;
                    }

                    case FLOOR_ARV_DECK: {
                        int u = (x - 16) + (y - 8) * 2 + 64;
                        int v = (x - 16) - (y - 8) * 2 + 64;
                        if (is_diamond_edge(x, y))
                            col = ((x + y) & 2) ? 97 : 12; /* Hazard yellow/black deck rim */
                        else if (((u % 6 == 0) && (v % 6 == 0)) || (((u + 3) % 6 == 0) && ((v + 3) % 6 == 0)))
                            col = 5; /* Raised steel diamond-plate tread stud */
                        else
                            col = (uint8_t)(9 + (hash & 1)); /* Gunmetal steel deck */
                        break;
                    }

                    case FLOOR_ARV_ROOF: {
                        int u = (x - 16) + (y - 8) * 2 + 64;
                        int dist = abs(x - 16) + abs(y - 8) * 2;
                        if (is_diamond_edge(x, y))
                            col = 25; /* Dark armored roof seam bevel */
                        else if (dist <= 4)
                            col = (dist <= 2) ? 8 : 97; /* Rooftop hatch / turret ring */
                        else if ((u % 6) == 0)
                            col = 21; /* Structural roof rib */
                        else
                            col = (uint8_t)(23 + (hash & 1)); /* Olive-drab armored roof */
                        break;
                    }
                }
                if (col != COL_TRANS) spr_floors[f][y * TILE_W + x] = col;
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

                    case WALL_UFO_HULL:
                    case WALL_UFO_HULL_DMG: {
                        int z = 23 - y;
                        int bow = ((z - 12) * (z - 12)) / 36;
                        int rib = ((x + bow) & 7) == 0;
                        if (z == 12) {
                            col_w = ((x % 6) == 0) ? 144 : 81;
                            col_n = ((x % 6) == 0) ? 146 : 83;
                        } else if (z == 11 || z == 13) {
                            col_w = 138; col_n = 140;
                        } else if (z >= 14) {
                            int step = (23 - z) / 2;
                            col_w = (uint8_t)(rib ? (133 + step) : (161 + step));
                            col_n = (uint8_t)(rib ? (136 + step) : (164 + step));
                        } else {
                            int step = (10 - z) / 2;
                            col_w = (uint8_t)(rib ? (137 + step) : (132 + step));
                            col_n = (uint8_t)(rib ? (140 + step) : (135 + step));
                        }
                        if (w == WALL_UFO_HULL_DMG && x >= 5 && x <= 10 && y >= 9 && y <= 14) {
                            col_w = (x == 8 && y == 11) ? 144 : 13;
                            col_n = (x == 8 && y == 11) ? 146 : 14;
                        }
                        break;
                    }

                    case WALL_UFO_DOOR_CLOSED:
                    case WALL_UFO_DOOR_OPEN: {
                        int dx2 = (x * 2 - 15) * 3;
                        int dy2 = (y * 2 - 25) * 2;
                        int ell = dx2 * dx2 + dy2 * dy2;
                        if (ell > 960) {
                            col_w = (y < 11) ? 163 : 134;
                            col_n = (y < 11) ? 166 : 137;
                        } else if (ell >= 680) {
                            col_w = 81; col_n = 83;
                        } else if (w == WALL_UFO_DOOR_OPEN) {
                            col_w = COL_TRANS; col_n = COL_TRANS;
                        } else if (ell <= 120) {
                            col_w = 144; col_n = 146;
                        } else {
                            col_w = 133; col_n = 136;
                        }
                        break;
                    }

                    case WALL_ARV_HULL:
                    case WALL_ARV_WIN:
                    case WALL_ARV_WHEEL:
                    case WALL_ARV_FRONT:
                    case WALL_ARV_DOOR_OPEN: {
                        if (w == WALL_ARV_DOOR_OPEN) {
                            /* Open 2-tile hydraulic side doorway header + thin side strut */
                            if (y <= 1) {
                                col_w = 22; col_n = 24;
                            } else if (y <= 3) {
                                int hz = ((x + y) & 2) != 0;
                                col_w = hz ? 97 : 13;
                                col_n = hz ? 98 : 14;
                            } else if (x == 0 && y <= 20) {
                                col_w = 8; col_n = 10;
                            } else {
                                col_w = COL_TRANS; col_n = COL_TRANS;
                            }
                            break;
                        }

                        /* Base military olive-drab / gunmetal armored vehicle body */
                        if (y >= 21 && w != WALL_ARV_WHEEL) {
                            /* Undercarriage ground clearance beneath vehicle skirt */
                            if (y == 21 && x >= 3 && x <= 12) {
                                col_w = 13; col_n = 14;
                            } else {
                                col_w = COL_TRANS; col_n = COL_TRANS;
                            }
                            break;
                        }

                        if (y == 12 || y == 13) {
                            /* Gold DUFF-COM tactical vehicle stripe */
                            col_w = 97; col_n = 99;
                        } else if (y >= 14) {
                            /* Lower angled gunmetal rock-slider armor skirt */
                            col_w = ((x % 4) == 0) ? 10 : 8;
                            col_n = ((x % 4) == 0) ? 12 : 10;
                        } else {
                            /* Upper sloped olive-drab armor plates with panel seams */
                            col_w = (x == 0 || x == 15) ? 25 : 22;
                            col_n = (x == 0 || x == 15) ? 27 : 24;
                        }

                        if (w == WALL_ARV_WIN) {
                            /* Emergency roof strobe beacon + bulletproof cab window */
                            if ((y == 1 || y == 2) && x >= 6 && x <= 9) {
                                col_w = (x <= 7) ? 33 : 80;
                                col_n = (x <= 7) ? 35 : 81;
                            } else if (y >= 5 && y <= 11 && x >= 2 && x <= 13) {
                                if (x == 2 || x == 7 || x == 8 || x == 13 || y == 5 || y == 11) {
                                    col_w = 11; col_n = 12;
                                } else {
                                    col_w = ((x + y) % 4 == 0) ? 80 : 70;
                                    col_n = ((x + y) % 4 == 0) ? 81 : 72;
                                }
                            }
                        } else if (w == WALL_ARV_WHEEL) {
                            /* Flared fender arch + heavy off-road tactical tire */
                            int dxw = x * 2 - 15;
                            int dyw = y * 2 - 35;
                            int r2  = dxw * dxw + dyw * dyw;
                            if (y >= 21 && (x <= 2 || x >= 13)) {
                                col_w = COL_TRANS; col_n = COL_TRANS;
                            } else if (r2 <= 22) {
                                col_w = (r2 <= 6) ? 11 : 4;  /* Steel wheel rim + hub */
                                col_n = (r2 <= 6) ? 12 : 6;
                            } else if (r2 <= 115) {
                                col_w = ((x + y) & 1) ? 13 : 14; /* Treaded rubber tire */
                                col_n = ((x + y) & 1) ? 14 : 15;
                            } else if (r2 <= 155 && y <= 19) {
                                col_w = 7; col_n = 9; /* Heavy wheel-well fender arch */
                            }
                        } else if (w == WALL_ARV_FRONT) {
                            /* Front windshield visor, radiator grille, headlights & bullbar */
                            if (y >= 4 && y <= 10 && x >= 2 && x <= 13) {
                                if (x == 2 || x == 7 || x == 8 || x == 13 || y == 4 || y == 10) {
                                    col_w = 11; col_n = 12;
                                } else {
                                    col_w = ((x + y) % 4 == 0) ? 80 : 70;
                                    col_n = ((x + y) % 4 == 0) ? 81 : 72;
                                }
                            } else if (y >= 13 && y <= 16 && ((x >= 1 && x <= 3) || (x >= 12 && x <= 14))) {
                                col_w = 96; col_n = 97; /* Twin halogen headlights */
                            } else if (y == 17 && ((x >= 1 && x <= 3) || (x >= 12 && x <= 14))) {
                                col_w = 113; col_n = 114; /* Amber fog/turn lamps */
                            } else if (y >= 12 && y <= 18 && x >= 5 && x <= 10) {
                                col_w = (x & 1) ? 6 : 13; /* Vertical-slat armored radiator grille */
                                col_n = (x & 1) ? 8 : 14;
                            } else if (y >= 19 && y <= 20) {
                                col_w = 5; col_n = 7; /* Heavy steel front bullbar bumper */
                            }
                        }
                        break;
                    }
                }

                /* Top wall edge highlight for crisp 3D readability */
                if (y == 0 && col_w != COL_TRANS && w < WALL_UFO_HULL) {
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

static uint8_t shade_vehicle_mat(int veh_kind, uint8_t mat, int tone) {
    switch (mat) {
        case 1:
            if (veh_kind == VEH_CRUISER) return (uint8_t)(tone == 0 ? 67 : (tone == 1 ? 69 : 72));
            if (veh_kind == VEH_SEDAN)   return (uint8_t)(tone == 0 ? 35 : (tone == 1 ? 37 : 40));
            if (veh_kind == VEH_PICKUP)  return (uint8_t)(tone == 0 ? 50 : (tone == 1 ? 52 : 55));
            return (uint8_t)(tone == 0 ? 18 : (tone == 1 ? 20 : 23));
        case 2:
            if (veh_kind == VEH_CRUISER) return (uint8_t)(tone == 0 ? 1 : (tone == 1 ? 3 : 5));
            return (uint8_t)(tone == 0 ? 96 : (tone == 1 ? 98 : 101));
        case 3:  return (uint8_t)(tone == 0 ? 81 : (tone == 1 ? 83 : 86));
        case 4:  return (uint8_t)(tone == 0 ? 11 : (tone == 1 ? 12 : 13));
        case 5:  return (uint8_t)(tone == 0 ? 2  : (tone == 1 ? 4  : 6));
        case 6:  return 96;
        case 7:  return (uint8_t)(tone == 2 ? 35 : 33);
        case 8:  return (uint8_t)(tone == 2 ? 67 : 65);
        case 9:  return (uint8_t)(tone == 0 ? 9  : (tone == 1 ? 11 : 12));
        case 10: return 97;
        default: return 8;
    }
}

static int valid_veh_canvas_pt(int cx, int cy, int along_y) {
    if ((unsigned)cx >= 48 || (unsigned)cy >= 48) return 0;
    if (!along_y) return (cx <= 31 && cy <= 39) || (cx >= 16 && cy >= 8);
    return (cx >= 16 && cy <= 39) || (cx <= 31 && cy >= 8);
}

static void build_two_tile_vehicle(int back_id, int front_id, int veh_kind, int along_y) {
    static uint8_t m[32][16][20];
    uint8_t canv[48][48];
    uint8_t is_shad[48][48];
    uint8_t add_out[48][48];
    int u, v, z, s, cx, cy;

    memset(m, 0, sizeof(m));
    memset(canv, COL_TRANS, sizeof(canv));
    memset(is_shad, 0, sizeof(is_shad));
    memset(add_out, 0, sizeof(add_out));

    if (veh_kind == VEH_CRUISER || veh_kind == VEH_SEDAN) {
        int is_pol = (veh_kind == VEH_CRUISER);
        int a;
        for (u = 2; u <= 29; u++) {
            int top_z = (u == 2) ? 6 : (u <= 26) ? 7 : (u <= 28) ? 6 : 5;
            for (v = 3; v <= 12; v++) {
                for (z = 2; z <= top_z; z++) {
                    m[u][v][z] = (uint8_t)((is_pol && u >= 11 && u <= 18) ? 2 : 1);
                }
            }
        }
        if (!is_pol) {
            for (u = 4; u <= 27; u++) { m[u][3][3] = 9; m[u][12][3] = 9; }
        } else {
            m[14][3][5] = 10; m[15][3][5] = 10;
            m[14][12][5] = 10; m[15][12][5] = 10;
        }
        for (v = 3; v <= 12; v++) {
            m[2][v][2] = 5; m[29][v][2] = 5;
            if (v >= 6 && v <= 9) { m[29][v][3] = 9; m[29][v][4] = 9; }
        }
        for (v = 4; v <= 5; v++) {
            m[29][v][4] = 6; m[29][15 - v][4] = 6;
            m[2][v][4]  = 7; m[2][15 - v][4]  = 7;
        }
        m[29][3][4] = 10; m[29][12][4] = 10;

        for (u = 8; u <= 20; u++) {
            int max_z = (u == 8) ? 9 : (u == 9) ? 11 :
                        (u <= 17) ? 12 : (u == 18) ? 11 : (u == 19) ? 10 : 8;
            for (v = 4; v <= 11; v++) {
                for (z = 8; z <= max_z; z++) {
                    int roof_cap = (z == 12 && u >= 10 && u <= 17);
                    int f_glass  = (u >= 18 && v >= 5 && v <= 10);
                    int r_glass  = (u <= 9  && v >= 5 && v <= 10);
                    int s_glass  = ((v == 4 || v == 11) && u >= 10 && u <= 17 && u != 14 && z <= 11);
                    if (roof_cap) m[u][v][z] = (uint8_t)(is_pol ? 2 : 1);
                    else if (f_glass || r_glass || s_glass) m[u][v][z] = 3;
                    else m[u][v][z] = (uint8_t)(is_pol ? 2 : 1);
                }
            }
        }
        if (is_pol) {
            for (u = 13; u <= 15; u++)
                for (v = 4; v <= 11; v++)
                    m[u][v][13] = (uint8_t)((v <= 6) ? 7 : (v <= 8 ? 5 : 8));
        } else {
            for (u = 13; u <= 16; u++)
                for (v = 6; v <= 9; v++)
                    m[u][v][12] = 9;
        }
        for (a = 0; a < 2; a++) {
            int au = a ? 21 : 5;
            for (u = au; u <= au + 5; u++)
                for (z = 2; z <= 5; z++) { m[u][3][z] = 9; m[u][12][z] = 9; }
            for (u = au + 1; u <= au + 4; u++) {
                for (z = 0; z <= 4; z++) {
                    uint8_t wmat;
                    if ((u == au + 1 || u == au + 4) && (z == 0 || z == 4)) continue;
                    wmat = (uint8_t)((u >= au + 2 && u <= au + 3 && z >= 1 && z <= 2) ? 5 : 4);
                    m[u][2][z] = wmat; m[u][3][z] = wmat;
                    m[u][13][z] = wmat; m[u][12][z] = wmat;
                }
            }
        }
    } else if (veh_kind == VEH_PICKUP) {
        int a;
        for (u = 2; u <= 13; u++) {
            for (v = 3; v <= 12; v++) {
                for (z = 2; z <= 8; z++) {
                    int bwall = (u == 2 || v == 3 || v == 12);
                    if (z <= 4) m[u][v][z] = (uint8_t)(bwall ? 1 : 9);
                    else if (bwall) m[u][v][z] = 1;
                }
            }
        }
        for (v = 3; v <= 12; v++)
            for (z = 9; z <= 12; z++)
                if (v <= 4 || v >= 11 || z == 12) m[13][v][z] = 9;
        for (u = 14; u <= 21; u++) {
            int max_z = (u == 21) ? 10 : 13;
            for (v = 3; v <= 12; v++) {
                for (z = 2; z <= max_z; z++) {
                    if (z <= 8) m[u][v][z] = 1;
                    else if (v >= 4 && v <= 11) {
                        int roof = (z == 13 && u <= 19);
                        int ws   = (u >= 20 && v >= 5 && v <= 10);
                        int sw   = ((v == 4 || v == 11) && u >= 15 && u <= 19 && z <= 12);
                        m[u][v][z] = (uint8_t)(roof ? 1 : ((ws || sw) ? 3 : 1));
                    }
                }
            }
        }
        for (u = 21; u <= 29; u++) {
            int top_z = (u >= 28) ? 7 : 8;
            for (v = 3; v <= 12; v++)
                for (z = 2; z <= top_z; z++)
                    if (m[u][v][z] == 0) m[u][v][z] = 1;
        }
        for (v = 3; v <= 12; v++) {
            m[2][v][2] = 5; m[29][v][2] = 5;
            if (v >= 5 && v <= 10) { m[29][v][4] = 5; m[29][v][5] = 9; m[29][v][6] = 5; }
        }
        for (v = 3; v <= 4; v++) {
            m[29][v][5] = 6; m[29][15 - v][5] = 6;
            m[2][v][5]  = 7; m[2][15 - v][5]  = 7;
            m[2][v][6]  = 7; m[2][15 - v][6]  = 7;
        }
        for (a = 0; a < 2; a++) {
            int au = a ? 21 : 5;
            for (u = au; u <= au + 5; u++)
                for (z = 2; z <= 5; z++) { m[u][3][z] = 9; m[u][12][z] = 9; }
            for (u = au + 1; u <= au + 4; u++) {
                for (z = 0; z <= 5; z++) {
                    uint8_t wmat;
                    if ((u == au + 1 || u == au + 4) && (z == 0 || z == 5)) continue;
                    wmat = (uint8_t)((u >= au + 2 && u <= au + 3 && z >= 2 && z <= 3) ? 5 : 4);
                    m[u][2][z] = wmat; m[u][3][z] = wmat;
                    m[u][13][z] = wmat; m[u][12][z] = wmat;
                }
            }
        }
    } else {
        for (v = 6; v <= 9; v++) { m[2][v][4] = 9; m[3][v][4] = 9; }
        for (u = 4; u <= 13; u++)
            for (v = 4; v <= 11; v++)
                for (z = 4; z <= 8; z++) m[u][v][z] = 1;
        for (u = 4; u <= 12; u++) {
            for (v = 2; v <= 4; v++) { m[u][v][9] = 1; m[u][15 - v][9] = 1; }
            if (u >= 5 && u <= 11)
                for (v = 2; v <= 4; v++) { m[u][v][10] = 1; m[u][15 - v][10] = 1; }
        }
        for (u = 6; u <= 9; u++)
            for (v = 6; v <= 9; v++)
                for (z = 9; z <= (u <= 7 ? 12 : 10); z++) m[u][v][z] = 9;
        for (z = 9; z <= 12; z++) { m[11][7][z] = 9; m[11][8][z] = 9; }
        for (z = 9; z <= 15; z++) {
            m[5][4][z] = 2; m[5][11][z] = 2;
            m[12][4][z] = 2; m[12][11][z] = 2;
        }
        for (u = 5; u <= 12; u++)
            for (v = 4; v <= 11; v++) m[u][v][16] = 1;
        for (u = 13; u <= 28; u++) {
            int top_z = (u == 28) ? 10 : 11;
            for (v = 5; v <= 10; v++) {
                for (z = 5; z <= top_z; z++) {
                    int stripe = (z == 9 && (v == 5 || v == 10) && u >= 14 && u <= 27);
                    int louver = (z >= 7 && z <= 8 && (v == 5 || v == 10) &&
                                  (u == 18 || u == 20 || u == 22 || u == 24));
                    m[u][v][z] = (uint8_t)(stripe ? 2 : (louver ? 9 : 1));
                }
            }
        }
        for (v = 6; v <= 9; v++)
            for (z = 6; z <= 9; z++) m[28][v][z] = 9;
        m[28][5][9] = 6;  m[28][10][9] = 6;
        m[28][5][10] = 6; m[28][10][10] = 6;
        for (z = 11; z <= 17; z++) {
            m[22][6][z] = 9;
            if (z >= 13 && z <= 15) m[23][6][z] = 9;
        }
        for (u = 25; u <= 26; u++)
            for (v = 4; v <= 11; v++) m[u][v][3] = 9;
        for (u = 3; u <= 11; u++) {
            for (z = 0; z <= 8; z++) {
                int d2 = (u - 7) * (u - 7) + (z - 4) * (z - 4);
                uint8_t wmat;
                if (d2 > 20) continue;
                wmat = (uint8_t)((d2 <= 5) ? 2 : 4);
                for (v = 2; v <= 3; v++) { m[u][v][z] = wmat; m[u][15 - v][z] = wmat; }
            }
        }
        for (u = 23; u <= 28; u++) {
            for (z = 0; z <= 5; z++) {
                int du2 = u * 2 - 51, dz2 = z * 2 - 5;
                int d2 = du2 * du2 + dz2 * dz2;
                uint8_t wmat;
                if (d2 > 34) continue;
                wmat = (uint8_t)((d2 <= 8) ? 2 : 4);
                for (v = 3; v <= 4; v++) { m[u][v][z] = wmat; m[u][15 - v][z] = wmat; }
            }
        }
    }

    for (u = 3; u <= 28; u++) {
        for (v = 3; v <= 12; v++) {
            cx = !along_y ? (16 + u - v) : (31 - u + v);
            cy = 24 + ((u + v) >> 1);
            if (valid_veh_canvas_pt(cx, cy, along_y)) {
                canv[cy][cx] = 14;
                is_shad[cy][cx] = 1;
            }
        }
    }

    for (s = 0; s <= 46; s++) {
        int u_min = (s > 15) ? (s - 15) : 0;
        int u_max = (s < 31) ? s : 31;
        for (u = u_min; u <= u_max; u++) {
            v = s - u;
            for (z = 0; z < 20; z++) {
                uint8_t mat = m[u][v][z];
                int top_o, v_o, u_o, left_o, right_o, tone;
                uint8_t col;
                if (!mat) continue;
                top_o = (z + 1 >= 20 || m[u][v][z + 1] == 0);
                v_o   = (v + 1 >= 16 || m[u][v + 1][z] == 0);
                u_o   = (u + 1 >= 32 || m[u + 1][v][z] == 0);
                if (!top_o && !v_o && !u_o) continue;
                left_o  = !along_y ? v_o : u_o;
                right_o = !along_y ? u_o : v_o;
                tone = top_o ? 0 : (left_o && !right_o) ? 1 : (right_o && !left_o) ? 2 : 1;
                col = shade_vehicle_mat(veh_kind, mat, tone);
                if (mat == 3 && ((u + v + z) % 5 == 0)) col = 80;
                cx = !along_y ? (16 + u - v) : (31 - u + v);
                cy = 24 + ((u + v) >> 1) - z;
                if (valid_veh_canvas_pt(cx, cy, along_y)) {
                    canv[cy][cx] = col;
                    is_shad[cy][cx] = 0;
                }
            }
        }
    }

    for (cy = 0; cy < 48; cy++) {
        for (cx = 0; cx < 48; cx++) {
            static const int odx[4] = { -1, 1, 0, 0 };
            static const int ody[4] = {  0, 0, -1, 1 };
            int d;
            if (canv[cy][cx] != COL_TRANS || !valid_veh_canvas_pt(cx, cy, along_y)) continue;
            for (d = 0; d < 4; d++) {
                int nx = cx + odx[d], ny = cy + ody[d];
                if ((unsigned)nx < 48 && (unsigned)ny < 48 &&
                    canv[ny][nx] != COL_TRANS && !is_shad[ny][nx]) {
                    add_out[cy][cx] = 1;
                    break;
                }
            }
        }
    }
    for (cy = 0; cy < 48; cy++) {
        for (cx = 0; cx < 48; cx++) {
            if (add_out[cy][cx]) canv[cy][cx] = 14;
        }
    }

    for (cy = 0; cy < 48; cy++) {
        for (cx = 0; cx < 48; cx++) {
            if (canv[cy][cx] == COL_TRANS) continue;
            if (!along_y) {
                if (cx < 16 || cy < 8 || (cx < 24 && cy <= 39)) {
                    spr_objects[back_id][cy * OBJ_W + cx] = canv[cy][cx];
                } else {
                    spr_objects[front_id][(cy - 8) * OBJ_W + (cx - 16)] = canv[cy][cx];
                }
            } else {
                if (cx >= 32 || cy < 8 || (cx >= 24 && cy <= 39)) {
                    spr_objects[back_id][cy * OBJ_W + (cx - 16)] = canv[cy][cx];
                } else {
                    spr_objects[front_id][(cy - 8) * OBJ_W + cx] = canv[cy][cx];
                }
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

    /* 6 & 14..16: Full 2-Tile Coopersburg Police Cruiser (+X & +Y) */
    build_two_tile_vehicle(OBJ_CAR_BACK_X, OBJ_CAR_FRONT_X, VEH_CRUISER, 0);
    build_two_tile_vehicle(OBJ_CAR_BACK_Y, OBJ_CAR_FRONT_Y, VEH_CRUISER, 1);

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

    /* 13 & 17..19: Full 2-Tile Pennsylvania Farm Tractor (+X & +Y) */
    build_two_tile_vehicle(OBJ_TRACTOR_BACK_X, OBJ_TRACTOR_FRONT_X, VEH_TRACTOR, 0);
    build_two_tile_vehicle(OBJ_TRACTOR_BACK_Y, OBJ_TRACTOR_FRONT_Y, VEH_TRACTOR, 1);

    /* 20..23: Full 2-Tile 90s Civilian Sedan (+X & +Y) */
    build_two_tile_vehicle(OBJ_SEDAN_BACK_X, OBJ_SEDAN_FRONT_X, VEH_SEDAN, 0);
    build_two_tile_vehicle(OBJ_SEDAN_BACK_Y, OBJ_SEDAN_FRONT_Y, VEH_SEDAN, 1);

    /* 24..27: Full 2-Tile Utility / Farm Pickup Truck (+X & +Y) */
    build_two_tile_vehicle(OBJ_PICKUP_BACK_X, OBJ_PICKUP_FRONT_X, VEH_PICKUP, 0);
    build_two_tile_vehicle(OBJ_PICKUP_BACK_Y, OBJ_PICKUP_FRONT_Y, VEH_PICKUP, 1);

    /* 28..31: 3D Curved Alien Saucer Hull Corner Fairings (N, E, S, W) */
    for (i = 0; i < 4; i++) {
        int slot = OBJ_UFO_CORNER_N + i;
        int pu2 = (i == 0 || i == 3) ? 30 : 0;
        int pv2 = (i == 0 || i == 1) ? 30 : 0;
        int s, su, z;
        for (y = 0; y < TILE_H; y++) {
            for (x = 0; x < TILE_W; x++) {
                int uf2, vf2, d2;
                if (!in_diamond(x, y)) continue;
                uf2 = x + 2 * y - 15;
                vf2 = 2 * y - x + 16;
                d2  = (uf2 - pu2) * (uf2 - pu2) + (vf2 - pv2) * (vf2 - pv2);
                if (d2 <= 810) {
                    spr_objects[slot][(24 + y) * OBJ_W + x] = spr_floors[FLOOR_UFO_DECK][y * TILE_W + x];
                }
            }
        }
        for (s = 0; s <= 62; s++) {
            int su_min = (s > 31) ? (s - 31) : 0;
            int su_max = (s < 31) ? s : 31;
            for (su = su_min; su <= su_max; su++) {
                int sv = s - su;
                int du = abs(su - pu2), dv = abs(sv - pv2);
                int r2 = du * du + dv * dv;
                int cx = (31 + su - sv) >> 1;
                if (r2 < 520 || r2 > 950) continue;
                for (z = 0; z < 24; z++) {
                    int cy = 23 + ((su + sv) >> 2) - z;
                    uint8_t col;
                    if ((unsigned)cx >= OBJ_W || (unsigned)cy >= OBJ_H) continue;
                    if (z == 23)      col = (r2 >= 760) ? 162 : 81;
                    else if (z == 12) col = ((su + sv) % 6 == 0) ? 144 : 81;
                    else if (z >= 14) col = (uint8_t)((du >= dv ? 162 : 165) + (23 - z) / 3);
                    else              col = (uint8_t)((du >= dv ? 132 : 135) + (11 - z) / 2);
                    spr_objects[slot][cy * OBJ_W + cx] = col;
                }
            }
        }
    }

    /* 32. OBJ_UFO_NAV: Alien Navigation Console + Floating 3D Hologram */
    obj_rect(OBJ_UFO_NAV, 6,  24, 20, 11, 134);
    obj_rect(OBJ_UFO_NAV, 7,  23, 18,  4, 83);
    obj_rect(OBJ_UFO_NAV, 6,  30, 20,  1, 81);
    obj_rect(OBJ_UFO_NAV, 11, 11, 10,  9, 82);
    obj_rect(OBJ_UFO_NAV, 9,  15, 14,  2, 80);
    obj_rect(OBJ_UFO_NAV, 15, 14,  2,  2, 96);

    /* 33. OBJ_UFO_COMPUTER: Alien Hyper-Wave Flight Computer Spire */
    obj_rect(OBJ_UFO_COMPUTER, 8,  9, 16, 26, 134);
    obj_rect(OBJ_UFO_COMPUTER, 9,  9, 14,  2, 162);
    obj_rect(OBJ_UFO_COMPUTER, 11, 13, 3, 16, 81);
    obj_rect(OBJ_UFO_COMPUTER, 18, 13, 3, 16, 210);
    obj_rect(OBJ_UFO_COMPUTER, 15, 15, 2, 12, 144);

    /* 34. OBJ_UFO_CRYO: Alien Cryo-Stasis Bio-Chamber */
    obj_rect(OBJ_UFO_CRYO, 9,   8, 14, 27, 83);
    obj_rect(OBJ_UFO_CRYO, 9,   8, 14,  4, 133);
    obj_rect(OBJ_UFO_CRYO, 9,  30, 14,  5, 135);
    obj_rect(OBJ_UFO_CRYO, 14, 15,  4, 10, 91);
    obj_rect(OBJ_UFO_CRYO, 11, 12,  1, 17, 80);

    /* 35. OBJ_UFO_POD: Alien Artifact / Ordnance Containment Pod */
    obj_rect(OBJ_UFO_POD, 8,  20, 16, 15, 134);
    obj_rect(OBJ_UFO_POD, 12, 20,  8,  3, 144);
    obj_rect(OBJ_UFO_POD, 8,  27, 16,  2, 81);
    obj_rect(OBJ_UFO_POD, 14, 27,  4,  2, 113);

    /* 36. OBJ_UFO_GRAVLIFT: Alien Anti-Gravity Lift Pad + Energy Rings */
    obj_rect(OBJ_UFO_GRAVLIFT, 6, 28, 20, 7, 134);
    obj_rect(OBJ_UFO_GRAVLIFT, 9, 29, 14, 5, 81);
    obj_rect(OBJ_UFO_GRAVLIFT, 8, 21, 16, 1, 80);
    obj_rect(OBJ_UFO_GRAVLIFT, 9, 14, 14, 1, 129);
    obj_rect(OBJ_UFO_GRAVLIFT, 10, 8, 12, 1, 80);
}

/* Build 8-directional sprites for DUFF-COM Soldiers, Sectoids, Floaters, and Mutons */
static void build_units(void) {
    int type, pose, dir, x, y;

    /* Build standalone unit ground shadow sprite (rendered on floor below unit) */
    memset(spr_unit_shadow, COL_TRANS, UNIT_W * UNIT_H);
    for (y = 26; y <= 29; y++) {
        for (x = 5; x <= 18; x++) {
            int d2 = (x - 12) * (x - 12) + (y - 28) * (y - 28) * 4;
            if (d2 <= 28) {
                spr_unit_shadow[y * UNIT_W + x] = (d2 <= 14) ? 14 : 13;
            }
        }
    }

    for (type = 0; type < NUM_UNIT_SPRITES; type++) {
        for (pose = 0; pose < 4; pose++) {
            for (dir = 0; dir < 8; dir++) {
                uint8_t *dst = spr_units[type][pose][dir];
                int y_off = (pose == 1) ? 5 : ((pose >= 2) ? -1 : 0);
                int leg_l_off = (pose == 2) ? -2 : (pose == 3 ?  2 : 0);
                int leg_r_off = (pose == 2) ?  2 : (pose == 3 ? -2 : 0);
                memset(dst, COL_TRANS, UNIT_W * UNIT_H);

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
                    /* FLOATER: Maroon cape + cybernetic hovering jetpack base (elevated 3px above floor shadow) */
                    int bob = ((pose >= 2) ? -2 : 0) - 3;
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
                } else if (type == UNIT_SPR_MUTON) {
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
                } else {
                    /* COOPERSBURG LOCAL CIVILIANS (Fallback for 9..14, overridden by UNITS.BMP) */
                    int c_idx = type - UNIT_SPR_CIV_MALE;
                    int is_fem = (c_idx >= 3);
                    static const uint8_t c_hair[6] = { 54, 98, 14, 36, 98, 14 };
                    static const uint8_t c_jkt_hi[6] = { 114, 34, 21, 80, 96, 35 };
                    static const uint8_t c_jkt_sh[6] = {  52, 38, 25, 83,100, 39 };
                    uint8_t hair_c = c_hair[c_idx % 6];
                    uint8_t skin_c = (c_idx == 2) ? 56 : 244;
                    uint8_t jkt_hi = c_jkt_hi[c_idx % 6];
                    uint8_t jkt_sh = c_jkt_sh[c_idx % 6];
                    uint8_t tee_c  = is_fem ? 81 : 2;
                    /* Denim jeans & shoes */
                    for (y = 20 + (pose == 1 ? 2 : 0); y < 28 + leg_l_off / 2; y++) {
                        int lx = 9 + (leg_l_off / 2);
                        if (y < UNIT_H) {
                            dst[y * UNIT_W + lx]     = (y >= 26) ? 54 : 67;
                            dst[y * UNIT_W + lx + 1] = (y >= 26) ? 54 : 69;
                        }
                    }
                    for (y = 20 + (pose == 1 ? 2 : 0); y < 28 + leg_r_off / 2; y++) {
                        int rx = 13 + (leg_r_off / 2);
                        if (y < UNIT_H) {
                            dst[y * UNIT_W + rx]     = (y >= 26) ? 54 : 69;
                            dst[y * UNIT_W + rx + 1] = (y >= 26) ? 54 : 67;
                        }
                    }
                    /* Unarmed civilian jacket & shirt (no armor or rifle) */
                    for (y = 12 + y_off; y < 21 + (pose == 1 ? 2 : 0); y++) {
                        int x0 = is_fem ? 9 : 8;
                        int x1 = is_fem ? 14 : 15;
                        for (x = x0; x <= x1; x++) {
                            if ((x == 11 || x == 12) && dir >= 2 && dir <= 6)
                                dst[y * UNIT_W + x] = tee_c;
                            else
                                dst[y * UNIT_W + x] = (x == x0 || x == x1) ? jkt_sh : jkt_hi;
                        }
                    }
                    /* Unarmed swinging/cowering hands at sides */
                    dst[(17 + y_off) * UNIT_W + 7]  = skin_c;
                    dst[(17 + y_off) * UNIT_W + 16] = skin_c;
                    /* Head & hair (no visor) */
                    for (y = 6 + y_off; y < 12 + y_off; y++) {
                        for (x = 9; x <= 14; x++) {
                            dst[y * UNIT_W + x] = (y < 9 + y_off || dir == 0 || dir == 1 || dir == 7) ? hair_c : skin_c;
                        }
                    }
                    if (is_fem) {
                        /* Shoulder-length hair locks */
                        dst[(10 + y_off) * UNIT_W + 8]  = hair_c;
                        dst[(11 + y_off) * UNIT_W + 8]  = hair_c;
                        dst[(10 + y_off) * UNIT_W + 15] = hair_c;
                        dst[(11 + y_off) * UNIT_W + 15] = hair_c;
                    }
                    if (dir >= 2 && dir <= 6) {
                        int ex = (dir == 2) ? 13 : (dir == 6) ? 9 : 10;
                        dst[(9 + y_off) * UNIT_W + ex]     = 14;
                        dst[(9 + y_off) * UNIT_W + ex + 1] = 14;
                    }
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
        } else if (type == UNIT_SPR_MUTON) {
            body_hi = 18;  body_sh = 21;
            head_c  = 131; leg_c   = 130; blood_c = 21;
        } else {
            static const uint8_t c_hair[6]   = { 54, 98, 14, 36, 98, 14 };
            static const uint8_t c_jkt_hi[6] = { 114, 34, 21, 80, 96, 35 };
            static const uint8_t c_jkt_sh[6] = {  52, 38, 25, 83,100, 39 };
            int c_idx = (type - UNIT_SPR_CIV_MALE) % 6;
            body_hi = c_jkt_hi[c_idx];
            body_sh = c_jkt_sh[c_idx];
            head_c  = c_hair[c_idx];
            leg_c   = 67;  blood_c = 38;
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
    static const uint8_t hair_cols[12] = {56, 98, 60, 54, 14, 4, 36, 97, 14, 59, 55, 2};
    int p, w, x, y;
    /* Soldier portraits (12 x 32x36: 0..5 Male, 6..11 Female) */
    for (p = 0; p < NUM_PORTRAITS; p++) {
        for (y = 0; y < 36; y++) {
            for (x = 0; x < 32; x++) {
                uint8_t c = 74; /* Dark blue portrait background */
                if (y >= 26 && x >= 4 && x <= 27) c = 66;
                if (y >= 9 && y < 26 && x >= 9 && x <= 22) c = (uint8_t)(242 + (p % 4));
                if (y >= 5 && y < 12 && x >= 8 && x <= 23) {
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

    /* Full-Body Paper-Doll Operative Sprites (12 x 48x96: 0..5 Male, 6..11 Female) */
    for (p = 0; p < NUM_PORTRAITS; p++) {
        int is_fem = (p >= 6);
        uint8_t skin_c = (uint8_t)(242 + (p % 4));
        uint8_t hair_c = hair_cols[p];
        uint8_t *fb = spr_fullbody[p];
        memset(fb, COL_TRANS, FULLBODY_W * FULLBODY_H);

        /* Floor pedestal shadow (y=87..92) */
        for (y = 87; y <= 92; y++) {
            for (x = 10; x < 38; x++) {
                int dx = x - 24, dy = (y - 89) * 3;
                if (dx * dx + dy * dy <= 144) {
                    fb[y * FULLBODY_W + x] = (dx * dx + dy * dy > 100) ? 68 : 13;
                }
            }
        }
        /* Legs, Knee Pads & Combat Boots (y=49..88) */
        for (y = 49; y <= 88; y++) {
            int l0 = is_fem ? 16 : 15, l1 = 22;
            int r0 = 25, r1 = is_fem ? 31 : 32;
            uint8_t col = (y >= 78) ? 10 : (y >= 62 && y <= 67) ? 81 : 67;
            for (x = l0; x <= l1; x++) fb[y * FULLBODY_W + x] = (x == l0 || x == l1) ? 14 : col;
            for (x = r0; x <= r1; x++) fb[y * FULLBODY_W + x] = (x == r0 || x == r1) ? 14 : col;
        }
        /* Tactical Utility Belt (y=44..48) */
        for (y = 44; y <= 48; y++) {
            for (x = 14; x <= 33; x++) {
                fb[y * FULLBODY_W + x] = (x >= 22 && x <= 25) ? 96 : 11;
            }
        }
        /* Armored Chest Cuirass & Shoulder Pauldrons (y=22..43) */
        for (y = 22; y <= 43; y++) {
            int hw = is_fem ? (y < 32 ? 9 : 7) : (y < 32 ? 10 : 8);
            for (x = 24 - hw; x <= 23 + hw; x++) {
                fb[y * FULLBODY_W + x] = (x == 24 - hw || x == 23 + hw) ? 14 : 66;
            }
            /* Left & Right Arms / Pauldrons */
            for (x = 8; x <= 14; x++)  fb[y * FULLBODY_W + x] = (y < 30) ? 65 : 68;
            for (x = 33; x <= 39; x++) fb[y * FULLBODY_W + x] = (y < 30) ? 65 : 68;
        }
        /* Head, Hair & Visor (y=5..21) */
        for (y = 5; y <= 21; y++) {
            for (x = 18; x <= 29; x++) {
                if (y <= 9) fb[y * FULLBODY_W + x] = hair_c;
                else if (y <= 19) fb[y * FULLBODY_W + x] = skin_c;
                else fb[y * FULLBODY_W + x] = 68;
            }
            if ((y == 11 || y == 12) && (p == 0 || p == 4 || p == 8)) {
                for (x = 19; x <= 28; x++) fb[y * FULLBODY_W + x] = 80;
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

    /* 4. UNITS.BMP (8 cols [dir 0..7] x 60 rows [sprite_id*4 + pose] of 24x32) */
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

    /* 4b. DEATH.BMP (4 cols [stage 0..3] x 15 rows [sprite_id 0..14] of 32x32) */
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
        memset(spr_unit_shadow, COL_TRANS, 8 * UNIT_W);
        bmp_extract_tile(sheet, sw, sh, 5 * FX_W, 4 * FX_H, FX_W, FX_H, spr_unit_shadow + 8 * UNIT_W);

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

    /* 5b. FULLBODY.BMP (12 full-body operative paper-doll figures x 48x96) */
    sheet = bmp_load_alloc("FULLBODY.BMP", &sw, &sh);
    if (sheet) {
        for (i = 0; i < NUM_PORTRAITS && (i + 1) * FULLBODY_W <= sw && sh >= FULLBODY_H; i++) {
            bmp_extract_tile(sheet, sw, sh, i * FULLBODY_W, 0, FULLBODY_W, FULLBODY_H, spr_fullbody[i]);
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
    if (sheet && sw > 0 && sw <= SCREEN_W && sh > 0 && sh <= SCREEN_H) {
        gfx_hidden_bg = sheet;
        gfx_hidden_w  = sw;
        gfx_hidden_h  = sh;
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
    items_init_assets();
}

