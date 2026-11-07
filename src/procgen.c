#include "procgen.h"
#include "isomap.h"
#include "gfx_assets.h"
#include "pathfind.h"
#include <stdlib.h>
#include <string.h>

/* Spatial Reservation Tags during Procedural Layout */
enum {
    ZONE_FREE    = 0,
    ZONE_ROAD    = 1,
    ZONE_BLDG    = 2,
    ZONE_DOORWAY = 3, /* Protected porch/stairway threshold: never blocked by props */
    ZONE_UFO     = 4,
    ZONE_LZ      = 5
};

typedef struct {
    int valid;
    int x0, y0, x1, y1;
    int stories;
} BldgRect;

static uint8_t zone_map[MAP_H][MAP_W];
static uint8_t reach_mask[MAP_LEVS][MAP_H][MAP_W];

static BldgRect g_bldgs[6];
static int      g_num_bldgs = 0;
static BldgRect g_ufo_rect;
static int      g_lz_x0, g_lz_y0, g_lz_x1, g_lz_y1, g_lz_side;

static int rrange(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (rand() % (hi - lo + 1));
}

static int calc_dir_vec(int dx, int dy) {
    if (dx == 0 && dy < 0)  return 0;
    if (dx > 0  && dy < 0)  return 1;
    if (dx > 0  && dy == 0) return 2;
    if (dx > 0  && dy > 0)  return 3;
    if (dx == 0 && dy > 0)  return 4;
    if (dx < 0  && dy > 0)  return 5;
    if (dx < 0  && dy == 0) return 6;
    return 7;
}

static void init_blank_map(uint8_t base_floor) {
    int x, y, z;
    memset(g_map, 0, sizeof(g_map));
    memset(zone_map, ZONE_FREE, sizeof(zone_map));
    memset(g_bldgs, 0, sizeof(g_bldgs));
    memset(&g_ufo_rect, 0, sizeof(g_ufo_rect));
    g_num_bldgs = 0;

    for (z = 0; z < MAP_LEVS; z++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                g_map[z][y][x].unit_idx = -1;
                g_map[z][y][x].fog      = FOG_UNSEEN;
                if (z == 0) {
                    g_map[0][y][x].floor_id = base_floor;
                }
            }
        }
    }
}

static void reserve_lz(void) {
    int x, y;
    g_lz_side = rand() % 4;
    if (g_lz_side == 0) {        /* South Edge */
        g_lz_x0 = rrange(4, 15); g_lz_x1 = g_lz_x0 + 4;
        g_lz_y0 = 19;            g_lz_y1 = 21;
    } else if (g_lz_side == 1) { /* North Edge */
        g_lz_x0 = rrange(4, 15); g_lz_x1 = g_lz_x0 + 4;
        g_lz_y0 = 2;             g_lz_y1 = 4;
    } else if (g_lz_side == 2) { /* West Edge */
        g_lz_x0 = 2;             g_lz_x1 = 4;
        g_lz_y0 = rrange(4, 15); g_lz_y1 = g_lz_y0 + 4;
    } else {                     /* East Edge */
        g_lz_x0 = 19;            g_lz_x1 = 21;
        g_lz_y0 = rrange(4, 15); g_lz_y1 = g_lz_y0 + 4;
    }

    for (y = g_lz_y0; y <= g_lz_y1; y++) {
        for (x = g_lz_x0; x <= g_lz_x1; x++) {
            zone_map[y][x] = ZONE_LZ;
        }
    }
}

/* Check if [x0..x1, y0..y1] is clear for a building/UFO, with 1-tile alley clearance */
static int lot_available(int x0, int y0, int x1, int y1, int allow_road_overlap) {
    int x, y;
    if (x0 < 2 || y0 < 2 || x1 > MAP_W - 3 || y1 > MAP_H - 3) return 0;

    for (y = y0 - 1; y <= y1 + 1; y++) {
        for (x = x0 - 1; x <= x1 + 1; x++) {
            uint8_t zt = zone_map[y][x];
            int inside = (x >= x0 && x <= x1 && y >= y0 && y <= y1);
            if (inside) {
                if (zt == ZONE_FREE) continue;
                if (allow_road_overlap && zt == ZONE_ROAD) continue;
                return 0;
            } else {
                /* 1-tile perimeter halo cannot touch another building, UFO, or LZ */
                if (zt == ZONE_BLDG || zt == ZONE_UFO || zt == ZONE_LZ) return 0;
            }
        }
    }
    return 1;
}

static void mark_threshold_clear(int x, int y) {
    if ((unsigned)x < MAP_W && (unsigned)y < MAP_H) {
        if (g_map[0][y][x].obj_id != OBJ_STAIRS_UP && g_map[0][y][x].obj_id != OBJ_UFO_CORE) {
            g_map[0][y][x].obj_id = OBJ_NONE;
        }
        if (zone_map[y][x] != ZONE_LZ && zone_map[y][x] != ZONE_BLDG && zone_map[y][x] != ZONE_UFO) {
            zone_map[y][x] = ZONE_DOORWAY;
        }
    }
}

/* Carve a doorway on side (0=North, 1=East, 2=South, 3=West) of rectangle (x0,y0)-(x1,y1) */
static void carve_rect_door(int x0, int y0, int x1, int y1, int side, int pos, uint8_t door_wall) {
    if (side == 0) {        /* North Wall */
        g_map[0][y0][pos].wall_north = door_wall;
        mark_threshold_clear(pos, y0);
        mark_threshold_clear(pos, y0 - 1);
    } else if (side == 2) { /* South Wall */
        g_map[0][y1 + 1][pos].wall_north = door_wall;
        mark_threshold_clear(pos, y1);
        mark_threshold_clear(pos, y1 + 1);
    } else if (side == 3) { /* West Wall */
        g_map[0][pos][x0].wall_west = door_wall;
        mark_threshold_clear(x0, pos);
        mark_threshold_clear(x0 - 1, pos);
    } else {                /* East Wall */
        g_map[0][pos][x1 + 1].wall_west = door_wall;
        mark_threshold_clear(x1, pos);
        mark_threshold_clear(x1 + 1, pos);
    }
}

static void place_stairs_at(int z, int sx, int sy) {
    if ((unsigned)sx >= MAP_W || (unsigned)sy >= MAP_H || z < 0 || z >= MAP_LEVS - 1) return;
    if (g_map[z][sy][sx].floor_id == FLOOR_NONE) {
        g_map[z][sy][sx].floor_id = FLOOR_WOOD;
    }
    g_map[z][sy][sx].obj_id     = OBJ_STAIRS_UP;
    g_map[z + 1][sy][sx].obj_id = OBJ_NONE;
    if (g_map[z + 1][sy][sx].floor_id == FLOOR_NONE) {
        g_map[z + 1][sy][sx].floor_id = FLOOR_WOOD;
    }
}

static void place_stairs_safe(int sx, int sy) {
    place_stairs_at(0, sx, sy);
}

static void place_prop_if_clear(int x, int y, int z, uint8_t obj_id) {
    if ((unsigned)x >= MAP_W || (unsigned)y >= MAP_H || (unsigned)z >= MAP_LEVS) return;
    if (z == 0 && (zone_map[y][x] == ZONE_DOORWAY || zone_map[y][x] == ZONE_LZ)) return;
    if (g_map[z][y][x].floor_id == FLOOR_NONE) return;
    if (g_map[z][y][x].obj_id != OBJ_NONE) return;
    /* Never block top landing of a staircase from the level below! */
    if (z > 0 && g_map[z - 1][y][x].obj_id == OBJ_STAIRS_UP) return;
    /* Never block adjacent doorways on any story */
    if (g_map[z][y][x].wall_west == WALL_DOOR_CLOSED || g_map[z][y][x].wall_west == WALL_DOOR_OPEN) return;
    if (g_map[z][y][x].wall_north == WALL_DOOR_CLOSED || g_map[z][y][x].wall_north == WALL_DOOR_OPEN) return;
    if (x + 1 < MAP_W && (g_map[z][y][x + 1].wall_west == WALL_DOOR_CLOSED || g_map[z][y][x + 1].wall_west == WALL_DOOR_OPEN)) return;
    if (y + 1 < MAP_H && (g_map[z][y + 1][x].wall_north == WALL_DOOR_CLOSED || g_map[z][y + 1][x].wall_north == WALL_DOOR_OPEN)) return;
    g_map[z][y][x].obj_id = obj_id;
}

/* Construct a watertight 1- or 2-story rectangular structure */
static BldgRect build_structure(int x0, int y0, int x1, int y1, int stories,
                                uint8_t floor0, uint8_t floor1,
                                uint8_t wall_main, uint8_t wall_win, int win_pct,
                                int roof_parapet) {
    BldgRect b;
    int x, y, z;
    b.valid = 1; b.x0 = x0; b.y0 = y0; b.x1 = x1; b.y1 = y1; b.stories = stories;

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            zone_map[y][x] = ZONE_BLDG;
            g_map[0][y][x].obj_id   = OBJ_NONE;
            g_map[0][y][x].floor_id = floor0;
            if (stories == 1) {
                g_map[1][y][x].floor_id = FLOOR_ROOF_TAR;
            } else {
                g_map[1][y][x].floor_id = floor1;
                g_map[2][y][x].floor_id = FLOOR_ROOF_TAR;
            }
        }
    }

    for (z = 0; z < stories; z++) {
        for (y = y0; y <= y1; y++) {
            int mid = (y > y0 && y < y1);
            g_map[z][y][x0].wall_west     = (mid && (rand() % 100) < win_pct) ? wall_win : wall_main;
            g_map[z][y][x1 + 1].wall_west = (mid && (rand() % 100) < win_pct) ? wall_win : wall_main;
        }
        for (x = x0; x <= x1; x++) {
            int mid = (x > x0 && x < x1);
            g_map[z][y0][x].wall_north     = (mid && (rand() % 100) < win_pct) ? wall_win : wall_main;
            g_map[z][y1 + 1][x].wall_north = (mid && (rand() % 100) < win_pct) ? wall_win : wall_main;
        }
    }

    if (stories == 1 && roof_parapet) {
        for (y = y0; y <= y1; y++) g_map[1][y][x0].wall_west = WALL_FENCE;
        for (x = x0; x <= x1; x++) g_map[1][y0][x].wall_north = WALL_FENCE;
    }

    if (g_num_bldgs < 6) {
        g_bldgs[g_num_bldgs++] = b;
    }
    return b;
}

/* Search randomized candidate coordinates for a free building lot */
static int find_random_lot(int w, int h, int min_x, int max_x, int min_y, int max_y,
                           int allow_road, int *out_x0, int *out_y0) {
    int attempt;
    if (max_x - w + 1 < min_x || max_y - h + 1 < min_y) return 0;
    for (attempt = 0; attempt < 90; attempt++) {
        int x0 = rrange(min_x, max_x - w + 1);
        int y0 = rrange(min_y, max_y - h + 1);
        if (lot_available(x0, y0, x0 + w - 1, y0 + h - 1, allow_road)) {
            *out_x0 = x0;
            *out_y0 = y0;
            return 1;
        }
    }
    return 0;
}

/* Try preferred (w,h) in sub-region first; if tight, fall back to 5x5 anywhere on map */
static int find_lot_flex(int *io_w, int *io_h, int min_x, int max_x, int min_y, int max_y,
                         int *out_x0, int *out_y0) {
    if (find_random_lot(*io_w, *io_h, min_x, max_x, min_y, max_y, 0, out_x0, out_y0)) {
        return 1;
    }
    *io_w = 5;
    *io_h = 5;
    return find_random_lot(5, 5, 2, 21, 2, 21, 0, out_x0, out_y0);
}

/* Place Crashed or Landed UFO (4x4 Scout or 5x5 Cruiser, intact or crash-breached!) */
static void place_procedural_ufo(void) {
    int sz = (rand() % 100 < 35) ? 4 : 5; /* 35% 4x4 Scout, 65% 5x5 Cruiser */
    int crashed = (rand() % 100 < 45);
    int min_x = 2, max_x = 21, min_y = 2, max_y = 21;
    int ux = 9, uy = 2;
    int x, y, door_side;

    /* Bias UFO into the half of the sector opposite the DUFF-COM Drop Zone */
    if (g_lz_side == 0)      { max_y = 9;  }
    else if (g_lz_side == 1) { min_y = 14; }
    else if (g_lz_side == 2) { min_x = 14; }
    else                     { max_x = 9;  }

    if (!find_random_lot(sz, sz, min_x, max_x, min_y, max_y, 1, &ux, &uy)) {
        (void)find_random_lot(sz, sz, 2, 21, 2, 21, 1, &ux, &uy);
    }

    g_ufo_rect.valid = 1;
    g_ufo_rect.x0 = ux; g_ufo_rect.y0 = uy;
    g_ufo_rect.x1 = ux + sz - 1; g_ufo_rect.y1 = uy + sz - 1;
    g_ufo_rect.stories = 1;

    /* Optional scorched skid-furrow trailing behind a crash-landed UFO */
    if (crashed) {
        int sx_dir = (ux + sz / 2 < 12) ? -1 : 1;
        int sy_dir = (uy + sz / 2 < 12) ? -1 : 1;
        int k;
        for (k = 1; k <= 4; k++) {
            int tx = ux + (sz / 2) + sx_dir * k;
            int ty = uy + (sz / 2) + sy_dir * (k / 2);
            if ((unsigned)tx < MAP_W && (unsigned)ty < MAP_H && zone_map[ty][tx] != ZONE_LZ) {
                g_map[0][ty][tx].floor_id = FLOOR_SCORCHED;
                if (k == 1) g_map[0][ty][tx].smoke = 3;
                if (k == 2 && (rand() & 1)) g_map[0][ty][tx].obj_id = OBJ_RUBBLE;
            }
        }
    }

    for (y = uy; y < uy + sz; y++) {
        for (x = ux; x < ux + sz; x++) {
            zone_map[y][x] = ZONE_UFO;
            g_map[0][y][x].obj_id   = OBJ_NONE;
            g_map[0][y][x].floor_id = FLOOR_SCORCHED;
            g_map[1][y][x].floor_id = FLOOR_ROOF_TAR;
            if (x == ux)          g_map[0][y][x].wall_west = WALL_STEEL;
            if (x == ux + sz - 1) g_map[0][y][x + 1].wall_west = WALL_STEEL;
            if (y == uy)          g_map[0][y][x].wall_north = WALL_STEEL;
            if (y == uy + sz - 1) g_map[0][y + 1][x].wall_north = WALL_STEEL;
        }
    }

    /* Orient main UFO airlock door toward the center of the battlefield */
    {
        int cx = ux + sz / 2, cy = uy + sz / 2;
        if (abs(12 - cy) >= abs(12 - cx)) {
            door_side = (cy <= 12) ? 2 : 0;
        } else {
            door_side = (cx <= 12) ? 1 : 3;
        }
        if (door_side == 0 || door_side == 2) {
            carve_rect_door(ux, uy, ux + sz - 1, uy + sz - 1, door_side, ux + sz / 2, WALL_DOOR_CLOSED);
        } else {
            carve_rect_door(ux, uy, ux + sz - 1, uy + sz - 1, door_side, uy + sz / 2, WALL_DOOR_CLOSED);
        }

        /* Crash-landed saucers can also have a blown-out hull breach! */
        if (crashed) {
            int b_side = (door_side + 1 + (rand() & 1)) & 3;
            int b_pos  = ((b_side == 0 || b_side == 2) ? ux : uy) + 1 + (rand() % (sz - 2));
            carve_rect_door(ux, uy, ux + sz - 1, uy + sz - 1, b_side, b_pos, WALL_NONE);
            if (b_side == 0)      { g_map[0][uy][b_pos].obj_id = OBJ_RUBBLE; g_map[0][uy][b_pos].smoke = 3; }
            else if (b_side == 2) { g_map[0][uy + sz - 1][b_pos].obj_id = OBJ_RUBBLE; g_map[0][uy + sz - 1][b_pos].fire = 2; }
            else if (b_side == 3) { g_map[0][b_pos][ux].obj_id = OBJ_RUBBLE; g_map[0][b_pos][ux].smoke = 3; }
            else                  { g_map[0][b_pos][ux + sz - 1].obj_id = OBJ_RUBBLE; g_map[0][b_pos][ux + sz - 1].fire = 2; }
        }
    }

    /* Glowing Elerium-115 Power Core inside UFO center + Hull Roof Access Ladder */
    g_map[0][uy + sz / 2][ux + sz / 2].obj_id = OBJ_UFO_CORE;
    place_stairs_at(0, ux, uy);
    if (sz == 5) {
        place_prop_if_clear(ux + 1, uy + 1, 0, OBJ_CRATE);
    }
}

/* Lay a paved highway or neighborhood street across the map */
static void paint_road_strip(int vertical, int start_coord, int asphalt_w, int with_lamps, int num_cars) {
    int i, w, c;
    for (i = 0; i < MAP_W; i++) {
        int s_lo = start_coord - 1;
        int s_hi = start_coord + asphalt_w;
        if (vertical) {
            if ((unsigned)s_lo < MAP_W) {
                g_map[0][i][s_lo].floor_id = FLOOR_SIDEWALK;
                if (zone_map[i][s_lo] == ZONE_FREE) zone_map[i][s_lo] = ZONE_ROAD;
            }
            if ((unsigned)s_hi < MAP_W) {
                g_map[0][i][s_hi].floor_id = FLOOR_SIDEWALK;
                if (zone_map[i][s_hi] == ZONE_FREE) zone_map[i][s_hi] = ZONE_ROAD;
            }
            for (w = 0; w < asphalt_w; w++) {
                int rx = start_coord + w;
                if ((unsigned)rx < MAP_W) {
                    g_map[0][i][rx].floor_id = (w == (asphalt_w - 1) / 2) ? FLOOR_ASPHALT_LINE : FLOOR_ASPHALT;
                    if (zone_map[i][rx] == ZONE_FREE) zone_map[i][rx] = ZONE_ROAD;
                }
            }
        } else {
            if ((unsigned)s_lo < MAP_H) {
                g_map[0][s_lo][i].floor_id = FLOOR_SIDEWALK;
                if (zone_map[s_lo][i] == ZONE_FREE) zone_map[s_lo][i] = ZONE_ROAD;
            }
            if ((unsigned)s_hi < MAP_H) {
                g_map[0][s_hi][i].floor_id = FLOOR_SIDEWALK;
                if (zone_map[s_hi][i] == ZONE_FREE) zone_map[s_hi][i] = ZONE_ROAD;
            }
            for (w = 0; w < asphalt_w; w++) {
                int ry = start_coord + w;
                if ((unsigned)ry < MAP_H) {
                    g_map[0][ry][i].floor_id = (w == (asphalt_w - 1) / 2) ? FLOOR_ASPHALT_LINE : FLOOR_ASPHALT;
                    if (zone_map[ry][i] == ZONE_FREE) zone_map[ry][i] = ZONE_ROAD;
                }
            }
        }
    }

    if (with_lamps) {
        int step = rrange(5, 7);
        for (i = rrange(2, 4); i < MAP_W - 2; i += step) {
            if (vertical) {
                place_prop_if_clear(start_coord - 1, i, 0, OBJ_STREETLAMP);
                place_prop_if_clear(start_coord + asphalt_w, i + 1, 0, OBJ_STREETLAMP);
            } else {
                place_prop_if_clear(i, start_coord - 1, 0, OBJ_STREETLAMP);
                place_prop_if_clear(i + 1, start_coord + asphalt_w, 0, OBJ_STREETLAMP);
            }
        }
    }

    for (c = 0; c < num_cars; c++) {
        int pos  = rrange(2, MAP_W - 3);
        int lane = start_coord + (rand() % asphalt_w);
        if (vertical) place_prop_if_clear(lane, pos, 0, OBJ_CAR_HOOD);
        else          place_prop_if_clear(pos, lane, 0, OBJ_CAR_HOOD);
    }
}

/* Furnish a Chrome Diner interior with booths, jukebox, doors & roof stairs */
static void furnish_diner(BldgRect b, int road_side) {
    int mx = (b.x0 + b.x1) / 2;
    int my = (b.y0 + b.y1) / 2;
    int alt_side = (road_side + 2) & 3;
    int x, y;

    carve_rect_door(b.x0, b.y0, b.x1, b.y1, road_side,
                    (road_side == 0 || road_side == 2) ? mx : my, WALL_DOOR_CLOSED);
    carve_rect_door(b.x0, b.y0, b.x1, b.y1, alt_side,
                    (alt_side == 0 || alt_side == 2) ? mx : my, WALL_DOOR_CLOSED);

    place_stairs_safe(b.x0, b.y0);
    place_prop_if_clear(b.x0, b.y1, 0, OBJ_JUKEBOX);

    for (y = b.y0 + 1; y <= b.y1 - 1; y += 2) {
        place_prop_if_clear(b.x1, y, 0, OBJ_DINER_BOOTH);
    }
    for (x = b.x0 + 2; x <= b.x1 - 1; x += 2) {
        if (rand() & 1) place_prop_if_clear(x, b.y0, 0, OBJ_DINER_BOOTH);
    }
}

/* Furnish Duff's Arcade interior with glowing 90s arcade cabinets, jukebox & upstairs crates */
static void furnish_arcade(BldgRect b, int main_side) {
    int mx = (b.x0 + b.x1) / 2;
    int my = (b.y0 + b.y1) / 2;
    int side2 = (main_side + 1) & 3;
    int x, y;

    carve_rect_door(b.x0, b.y0, b.x1, b.y1, main_side,
                    (main_side == 0 || main_side == 2) ? mx : my, WALL_DOOR_CLOSED);
    carve_rect_door(b.x0, b.y0, b.x1, b.y1, side2,
                    (side2 == 0 || side2 == 2) ? mx : my, WALL_DOOR_CLOSED);

    place_stairs_safe(b.x1, b.y1);
    place_prop_if_clear(b.x1, b.y0, 0, OBJ_JUKEBOX);

    if ((b.x1 - b.x0) >= (b.y1 - b.y0)) {
        for (x = b.x0 + 1; x <= b.x1 - 1; x += 2) {
            place_prop_if_clear(x, b.y0 + 1, 0, OBJ_ARCADE_CAB);
            if (b.y1 - b.y0 >= 4) place_prop_if_clear(x, b.y1 - 1, 0, OBJ_ARCADE_CAB);
        }
    } else {
        for (y = b.y0 + 1; y <= b.y1 - 1; y += 2) {
            place_prop_if_clear(b.x0 + 1, y, 0, OBJ_ARCADE_CAB);
            if (b.x1 - b.x0 >= 4) place_prop_if_clear(b.x1 - 1, y, 0, OBJ_ARCADE_CAB);
        }
    }

    if (b.stories >= 2) {
        place_stairs_at(1, b.x0, b.y1);
        place_prop_if_clear(b.x0 + 1, b.y0 + 1, 1, OBJ_CRATE);
        place_prop_if_clear(b.x1 - 1, b.y0 + 1, 1, OBJ_CRATE);
    }
}

/* Plant a fenced Pennsylvania cornfield patch with open entry gaps */
static void place_corn_patch(int x0, int y0, int x1, int y1) {
    int x, y;
    int style = rand() % 3; /* 0=checkerboard, 1=N-S rows, 2=E-W rows */
    int fenced = (rand() % 100 < 75);

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            int plant = (style == 0) ? (((x + y) & 1) == 0) :
                        (style == 1) ? (((x - x0) % 2) == 0) : (((y - y0) % 2) == 0);
            if (zone_map[y][x] != ZONE_FREE) continue;
            if (fenced && y == y0 && x != (x0 + x1) / 2) g_map[0][y][x].wall_north = WALL_FENCE;
            if (fenced && x == x0 && y != (y0 + y1) / 2) g_map[0][y][x].wall_west  = WALL_FENCE;
            if (plant && (rand() % 100 < 82)) {
                place_prop_if_clear(x, y, 0, OBJ_CORN);
            }
        }
    }
}

/* =========================================================================
 * BIOME 0: COOPERSBURG ROUTE 309 DINER & DUFF'S ARCADE
 * ========================================================================= */
static void gen_biome_rte309(void) {
    int vert_hwy;
    int hwy_pos    = rrange(9, 12);
    int swap_sides = rand() & 1;
    int bx, by, w, h;

    init_blank_map(FLOOR_GRASS);
    reserve_lz();
    vert_hwy = (g_lz_side <= 1) ? ((rand() % 100) < 70) : ((rand() % 100) < 35);

    paint_road_strip(vert_hwy, hwy_pos, 4, 1, 0);
    place_procedural_ufo();

    /* Place Route 309 Chrome Diner & 2-Story Duff's Arcade across the highway! */
    w = rrange(5, 6); h = rrange(6, 7);
    if (vert_hwy) {
        int d_minx = swap_sides ? (hwy_pos + 5) : 2;
        int d_maxx = swap_sides ? 21 : (hwy_pos - 2);
        int a_minx = swap_sides ? 2 : (hwy_pos + 5);
        int a_maxx = swap_sides ? (hwy_pos - 2) : 21;

        if (find_lot_flex(&w, &h, d_minx, d_maxx, 2, 21, &bx, &by)) {
            BldgRect d = build_structure(bx, by, bx + w - 1, by + h - 1, 1,
                                         FLOOR_DINER_TILE, FLOOR_ROOF_TAR,
                                         WALL_DINER_CHROME, WALL_DINER_GLASS, 65, 1);
            furnish_diner(d, swap_sides ? 3 : 1);
        }

        w = rrange(5, 6); h = rrange(6, 7);
        if (find_lot_flex(&w, &h, a_minx, a_maxx, 2, 21, &bx, &by)) {
            BldgRect a = build_structure(bx, by, bx + w - 1, by + h - 1, 2,
                                         FLOOR_ARCADE_CARPET, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 45, 0);
            furnish_arcade(a, swap_sides ? 1 : 3);
        }
    } else {
        int d_miny = swap_sides ? (hwy_pos + 5) : 2;
        int d_maxy = swap_sides ? 21 : (hwy_pos - 2);
        int a_miny = swap_sides ? 2 : (hwy_pos + 5);
        int a_maxy = swap_sides ? (hwy_pos - 2) : 21;

        w = rrange(6, 7); h = rrange(5, 6);
        if (find_lot_flex(&w, &h, 2, 21, d_miny, d_maxy, &bx, &by)) {
            BldgRect d = build_structure(bx, by, bx + w - 1, by + h - 1, 1,
                                         FLOOR_DINER_TILE, FLOOR_ROOF_TAR,
                                         WALL_DINER_CHROME, WALL_DINER_GLASS, 65, 1);
            furnish_diner(d, swap_sides ? 0 : 2);
        }

        w = rrange(6, 7); h = rrange(5, 6);
        if (find_lot_flex(&w, &h, 2, 21, a_miny, a_maxy, &bx, &by)) {
            BldgRect a = build_structure(bx, by, bx + w - 1, by + h - 1, 2,
                                         FLOOR_ARCADE_CARPET, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 45, 0);
            furnish_arcade(a, swap_sides ? 2 : 0);
        }
    }

    /* Optional 3rd Roadside Garage / Service Shop or Corn Lot */
    if (find_random_lot(4, 4, 2, 21, 2, 21, 0, &bx, &by)) {
        if (rand() & 1) {
            BldgRect g = build_structure(bx, by, bx + 3, by + 3, 1,
                                         FLOOR_SIDEWALK, FLOOR_ROOF_TAR,
                                         WALL_BRICK, WALL_BRICK_WIN, 35, 0);
            carve_rect_door(g.x0, g.y0, g.x1, g.y1, 2, g.x0 + 1, WALL_DOOR_OPEN);
            carve_rect_door(g.x0, g.y0, g.x1, g.y1, 0, g.x0 + 2, WALL_DOOR_CLOSED);
            place_stairs_at(0, g.x0, g.y0);
            place_prop_if_clear(g.x0 + 2, g.y0 + 2, 0, OBJ_BARREL);
            place_prop_if_clear(g.x0 + 1, g.y0 + 1, 0, OBJ_CRATE);
        } else {
            place_corn_patch(bx, by, bx + 3, by + 3);
        }
    }

    if (find_random_lot(4, 4, 2, 21, 2, 21, 0, &bx, &by)) {
        place_corn_patch(bx, by, bx + 3, by + 3);
    }

    /* Scatter parked Route 309 cars, service alley barrels & crates */
    {
        int i;
        for (i = 0; i < 3; i++) {
            int pos = rrange(2, MAP_W - 3);
            int lane = hwy_pos + (rand() % 4);
            if (vert_hwy) place_prop_if_clear(lane, pos, 0, OBJ_CAR_HOOD);
            else          place_prop_if_clear(pos, lane, 0, OBJ_CAR_HOOD);
        }
        for (i = 0; i < 4; i++) {
            int rx = rrange(2, 21), ry = rrange(2, 21);
            if (zone_map[ry][rx] == ZONE_FREE) {
                place_prop_if_clear(rx, ry, 0, (i & 1) ? OBJ_BARREL : OBJ_CRATE);
            }
        }
    }
}

/* =========================================================================
 * BIOME 1: QUAKERTOWN / PENNSYLVANIA FARMLAND & RED BARN
 * ========================================================================= */
static void gen_biome_pa_farm(void) {
    int vert_lane = rand() & 1;
    int lane_pos  = rrange(10, 13);
    int bx, by, bw, bh, i;

    init_blank_map(FLOOR_GRASS);
    reserve_lz();

    /* Rural Farm Dirt/Gravel Lane */
    for (i = 0; i < MAP_W; i++) {
        if (vert_lane) {
            g_map[0][i][lane_pos].floor_id     = FLOOR_SIDEWALK;
            g_map[0][i][lane_pos + 1].floor_id = FLOOR_SIDEWALK;
            if (zone_map[i][lane_pos] == ZONE_FREE)     zone_map[i][lane_pos] = ZONE_ROAD;
            if (zone_map[i][lane_pos + 1] == ZONE_FREE) zone_map[i][lane_pos + 1] = ZONE_ROAD;
        } else {
            g_map[0][lane_pos][i].floor_id     = FLOOR_SIDEWALK;
            g_map[0][lane_pos + 1][i].floor_id = FLOOR_SIDEWALK;
            if (zone_map[lane_pos][i] == ZONE_FREE)     zone_map[lane_pos][i] = ZONE_ROAD;
            if (zone_map[lane_pos + 1][i] == ZONE_FREE) zone_map[lane_pos + 1][i] = ZONE_ROAD;
        }
    }

    place_procedural_ufo();

    /* 2-Story Red Pennsylvania Barn with Randomized Hayloft */
    bw = rrange(6, 7);
    bh = rrange(6, 7);
    if (find_random_lot(bw, bh, 2, 21, 2, 21, 0, &bx, &by)) {
        BldgRect barn = build_structure(bx, by, bx + bw - 1, by + bh - 1, 2,
                                        FLOOR_WOOD, FLOOR_WOOD,
                                        WALL_BARN_WOOD, WALL_BRICK_WIN, 30, 0);
        int bay_side = rand() & 3;
        int side_door = (bay_side + 1 + (rand() & 1)) & 3;
        int mx = (barn.x0 + barn.x1) / 2;
        int my = (barn.y0 + barn.y1) / 2;
        int loft_style = rand() & 1;
        int lx, ly;

        /* Cut out half of z=1 floor to form an authentic open barn hayloft! */
        for (ly = barn.y0; ly <= barn.y1; ly++) {
            for (lx = barn.x0; lx <= barn.x1; lx++) {
                int keep_loft = loft_style ? (lx >= mx || ly <= barn.y0 + 1)
                                           : (ly >= my || lx <= barn.x0 + 1);
                if (!keep_loft) {
                    g_map[1][ly][lx].floor_id = FLOOR_NONE;
                }
            }
        }

        /* Wide 2-tile open Barn Doors + Secondary Door */
        if (bay_side == 0 || bay_side == 2) {
            carve_rect_door(barn.x0, barn.y0, barn.x1, barn.y1, bay_side, mx,     WALL_DOOR_OPEN);
            carve_rect_door(barn.x0, barn.y0, barn.x1, barn.y1, bay_side, mx + 1, WALL_DOOR_OPEN);
        } else {
            carve_rect_door(barn.x0, barn.y0, barn.x1, barn.y1, bay_side, my,     WALL_DOOR_OPEN);
            carve_rect_door(barn.x0, barn.y0, barn.x1, barn.y1, bay_side, my + 1, WALL_DOOR_OPEN);
        }
        carve_rect_door(barn.x0, barn.y0, barn.x1, barn.y1, side_door,
                        (side_door == 0 || side_door == 2) ? mx : my, WALL_DOOR_CLOSED);

        place_stairs_safe(loft_style ? barn.x1 : barn.x0, loft_style ? barn.y0 : barn.y1);
        place_stairs_at(1, barn.x0, barn.y0);
        place_prop_if_clear(mx, my, 0, OBJ_TRACTOR);
        place_prop_if_clear(barn.x0 + 1, barn.y0 + 1, 0, OBJ_CRATE);
        place_prop_if_clear(barn.x1 - 1, barn.y1 - 1, 0, OBJ_BARREL);
        place_prop_if_clear(loft_style ? barn.x1 - 1 : barn.x0 + 1,
                            loft_style ? barn.y0 + 1 : barn.y1 - 1, 1, OBJ_CRATE);
    }

    /* Secondary Farmhouse or Equipment Shed */
    if (find_random_lot(5, 5, 2, 21, 2, 21, 0, &bx, &by)) {
        int is_house = rand() & 1;
        BldgRect sh = build_structure(bx, by, bx + 4, by + 4, 1,
                                      FLOOR_WOOD, FLOOR_ROOF_TAR,
                                      is_house ? WALL_BRICK : WALL_BARN_WOOD,
                                      WALL_BRICK_WIN, 50, 0);
        carve_rect_door(sh.x0, sh.y0, sh.x1, sh.y1, 2, sh.x0 + 2, WALL_DOOR_CLOSED);
        carve_rect_door(sh.x0, sh.y0, sh.x1, sh.y1, 0, sh.x0 + 2, is_house ? WALL_DOOR_CLOSED : WALL_DOOR_OPEN);
        place_stairs_at(0, sh.x0, sh.y0);
        place_prop_if_clear(sh.x0 + 1, sh.y0 + 1, 0, is_house ? OBJ_DINER_BOOTH : OBJ_CRATE);
        place_prop_if_clear(sh.x1 - 1, sh.y1 - 1, 0, is_house ? OBJ_JUKEBOX : OBJ_BARREL);
    }

    /* 3 Randomized Fenced Pennsylvania Cornfields */
    for (i = 0; i < 3; i++) {
        int cw = rrange(4, 6), ch = rrange(4, 6);
        if (find_random_lot(cw, ch, 2, 21, 2, 21, 0, &bx, &by)) {
            place_corn_patch(bx, by, bx + cw - 1, by + ch - 1);
        }
    }

    /* Outdoor Farm Tractors, Fuel Drums & Supply Crates */
    for (i = 0; i < 6; i++) {
        int rx = rrange(2, 21), ry = rrange(2, 21);
        if (zone_map[ry][rx] == ZONE_FREE) {
            uint8_t obj = (i < 2) ? OBJ_TRACTOR : ((i & 1) ? OBJ_BARREL : OBJ_CRATE);
            place_prop_if_clear(rx, ry, 0, obj);
        }
    }
}

/* =========================================================================
 * BIOME 2: COOPERSBURG NEIGHBORHOOD & ARCADE BASEMENT / WING
 * ========================================================================= */
static void gen_biome_suburb(void) {
    int vert_st, st_pos;
    int bx, by, i;

    init_blank_map(FLOOR_GRASS);
    reserve_lz();

    if (g_lz_side == 0)      { vert_st = 0; st_pos = 18; }
    else if (g_lz_side == 1) { vert_st = 0; st_pos = 3;  }
    else if (g_lz_side == 2) { vert_st = 1; st_pos = 3;  }
    else                     { vert_st = 1; st_pos = 18; }

    /* 40% chance street cuts right through the middle of the neighborhood instead! */
    if ((rand() % 100) < 40) {
        vert_st = rand() & 1;
        st_pos  = rrange(10, 12);
    }

    paint_road_strip(vert_st, st_pos, 3, 1, 3);
    place_procedural_ufo();

    /* Try Split-Wing Mega-Arcade Residence (50%), otherwise Twin Suburban Houses! */
    if ((rand() & 1) && find_random_lot(11, 6, 2, 21, 2, 21, 0, &bx, &by)) {
        int split_x = bx + 5;
        int x, y;
        BldgRect h = build_structure(bx, by, bx + 10, by + 5, 1,
                                     FLOOR_WOOD, FLOOR_ROOF_TAR,
                                     WALL_BRICK, WALL_BRICK_WIN, 65, 1);
        /* Convert East Half into Duff's Neon Arcade Wing with Interior Partition */
        for (y = h.y0; y <= h.y1; y++) {
            for (x = split_x; x <= h.x1; x++) {
                g_map[0][y][x].floor_id = FLOOR_ARCADE_CARPET;
            }
            g_map[0][y][split_x].wall_west = (y == h.y0 + 2 || y == h.y0 + 3) ? WALL_DOOR_OPEN : WALL_BRICK;
        }
        carve_rect_door(h.x0, h.y0, h.x1, h.y1, 0, bx + 2,      WALL_DOOR_CLOSED);
        carve_rect_door(h.x0, h.y0, h.x1, h.y1, 0, split_x + 2, WALL_DOOR_CLOSED);
        carve_rect_door(h.x0, h.y0, h.x1, h.y1, 2, bx + 2,      WALL_DOOR_CLOSED);
        carve_rect_door(h.x0, h.y0, h.x1, h.y1, 2, split_x + 3, WALL_DOOR_CLOSED);

        place_stairs_safe(h.x1, h.y1);
        place_prop_if_clear(bx + 1, by + 1, 0, OBJ_DINER_BOOTH);
        place_prop_if_clear(bx + 1, by + 4, 0, OBJ_JUKEBOX);
        for (x = split_x + 1; x <= h.x1 - 1; x += 2) {
            place_prop_if_clear(x, by + 1, 0, OBJ_ARCADE_CAB);
            place_prop_if_clear(x, by + 4, 0, OBJ_ARCADE_CAB);
        }
    } else {
        /* House 1: Duff's 2-Story Suburban Arcade House */
        int w1 = rrange(6, 7), h1 = rrange(5, 6);
        if (find_lot_flex(&w1, &h1, 2, 21, 2, 21, &bx, &by)) {
            BldgRect a = build_structure(bx, by, bx + w1 - 1, by + h1 - 1, 2,
                                         FLOOR_ARCADE_CARPET, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 60, 0);
            furnish_arcade(a, 2);
            carve_rect_door(a.x0, a.y0, a.x1, a.y1, 0, (a.x0 + a.x1) / 2, WALL_DOOR_CLOSED);
        }
        /* House 2: Neighbor's Brick Colonial Home */
        w1 = rrange(5, 6); h1 = rrange(5, 6);
        if (find_random_lot(w1, h1, 2, 21, 2, 21, 0, &bx, &by)) {
            BldgRect n = build_structure(bx, by, bx + w1 - 1, by + h1 - 1, 2,
                                         FLOOR_WOOD, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 65, 0);
            carve_rect_door(n.x0, n.y0, n.x1, n.y1, 2, (n.x0 + n.x1) / 2, WALL_DOOR_CLOSED);
            carve_rect_door(n.x0, n.y0, n.x1, n.y1, 0, (n.x0 + n.x1) / 2, WALL_DOOR_CLOSED);
            place_stairs_safe(n.x0, n.y0);
            place_stairs_at(1, n.x0, n.y1);
            place_prop_if_clear(n.x1 - 1, n.y0 + 1, 0, OBJ_DINER_BOOTH);
            place_prop_if_clear(n.x1 - 1, n.y1 - 1, 0, OBJ_DINER_BOOTH);
            place_prop_if_clear(n.x1 - 1, n.y0 + 1, 1, OBJ_CRATE);
        }
    }

    /* Backyard Hedges, Fences & Patio Props */
    if (find_random_lot(4, 4, 2, 21, 2, 21, 0, &bx, &by)) {
        place_corn_patch(bx, by, bx + 3, by + 3);
    }
    for (i = 0; i < 4; i++) {
        int rx = rrange(2, 21), ry = rrange(2, 21);
        if (zone_map[ry][rx] == ZONE_FREE) {
            place_prop_if_clear(rx, ry, 0, (i == 0) ? OBJ_DINER_BOOTH : ((i & 1) ? OBJ_CRATE : OBJ_BARREL));
        }
    }
}

/* =========================================================================
 * BIOME 3: BETHLEHEM STEEL INDUSTRIAL RUINS
 * ========================================================================= */
static void build_catwalk_between(BldgRect a, BldgRect b) {
    int x, y;
    if (!a.valid || !b.valid) return;

    /* Check horizontal catwalk alignment (overlapping Y range) */
    {
        int ov_y0 = (a.y0 > b.y0) ? a.y0 : b.y0;
        int ov_y1 = (a.y1 < b.y1) ? a.y1 : b.y1;
        if (ov_y1 >= ov_y0) {
            int cy = (ov_y0 + ov_y1) / 2;
            int x_start = (a.x1 < b.x0) ? (a.x1 + 1) : (b.x1 + 1);
            int x_end   = (a.x1 < b.x0) ? (b.x0 - 1) : (a.x0 - 1);
            int clear_span = (x_start <= x_end);
            for (x = x_start; x <= x_end && clear_span; x++) {
                if (zone_map[cy][x] == ZONE_BLDG || zone_map[cy][x] == ZONE_UFO) clear_span = 0;
            }
            if (clear_span) {
                for (x = x_start; x <= x_end; x++) {
                    g_map[1][cy][x].floor_id       = FLOOR_ROOF_TAR;
                    g_map[1][cy][x].wall_north     = WALL_FENCE;
                    g_map[1][cy + 1][x].wall_north = WALL_FENCE;
                }
                g_map[1][cy][x_start].wall_west     = WALL_NONE;
                g_map[1][cy][x_end + 1].wall_west   = WALL_NONE;
                return;
            }
        }
    }

    /* Check vertical catwalk alignment (overlapping X range) */
    {
        int ov_x0 = (a.x0 > b.x0) ? a.x0 : b.x0;
        int ov_x1 = (a.x1 < b.x1) ? a.x1 : b.x1;
        if (ov_x1 >= ov_x0) {
            int cx = (ov_x0 + ov_x1) / 2;
            int y_start = (a.y1 < b.y0) ? (a.y1 + 1) : (b.y1 + 1);
            int y_end   = (a.y1 < b.y0) ? (b.y0 - 1) : (a.y0 - 1);
            int clear_span = (y_start <= y_end);
            for (y = y_start; y <= y_end && clear_span; y++) {
                if (zone_map[y][cx] == ZONE_BLDG || zone_map[y][cx] == ZONE_UFO) clear_span = 0;
            }
            if (clear_span) {
                for (y = y_start; y <= y_end; y++) {
                    g_map[1][y][cx].floor_id     = FLOOR_ROOF_TAR;
                    g_map[1][y][cx].wall_west    = WALL_FENCE;
                    g_map[1][y][cx + 1].wall_west= WALL_FENCE;
                }
                g_map[1][y_start][cx].wall_north   = WALL_NONE;
                g_map[1][y_end + 1][cx].wall_north = WALL_NONE;
            }
        }
    }
}

static void gen_biome_steel(void) {
    BldgRect towers[3];
    int num_towers = 0;
    int bx, by, i, s;

    init_blank_map(FLOOR_ASPHALT);
    reserve_lz();

    /* Random scorched blast-furnace slag zones on ground floor */
    for (s = 0; s < 5; s++) {
        int sx = rrange(2, 19), sy = rrange(2, 19);
        int dy, dx;
        for (dy = 0; dy < 3; dy++) {
            for (dx = 0; dx < 3; dx++) {
                g_map[0][sy + dy][sx + dx].floor_id = (s & 1) ? FLOOR_SCORCHED : FLOOR_SIDEWALK;
            }
        }
    }

    place_procedural_ufo();

    /* Build 2 to 3 Multi-Story Steel Blast-Furnace Towers */
    memset(towers, 0, sizeof(towers));
    for (i = 0; i < 3; i++) {
        int tw = rrange(5, 6), th = rrange(5, 6);
        if (find_random_lot(tw, th, 2, 21, 2, 21, 0, &bx, &by)) {
            BldgRect t = build_structure(bx, by, bx + tw - 1, by + th - 1, 1,
                                         FLOOR_SIDEWALK, FLOOR_ROOF_TAR,
                                         WALL_STEEL, WALL_BRICK_WIN, 20, 1);
            int mx = (t.x0 + t.x1) / 2;
            int my = (t.y0 + t.y1) / 2;
            carve_rect_door(t.x0, t.y0, t.x1, t.y1, 0, mx, WALL_DOOR_OPEN);
            carve_rect_door(t.x0, t.y0, t.x1, t.y1, 2, mx, WALL_DOOR_CLOSED);
            carve_rect_door(t.x0, t.y0, t.x1, t.y1, (i & 1) ? 1 : 3, my, WALL_DOOR_OPEN);
            place_stairs_safe((i & 1) ? t.x1 : t.x0, (i & 1) ? t.y1 : t.y0);
            place_prop_if_clear(mx, my, 0, (i == 0) ? OBJ_BARREL : OBJ_CRATE);
            towers[num_towers++] = t;
        }
    }

    /* Connect aligned Furnace Towers with Elevated Level-1 Steel Catwalks! */
    if (num_towers >= 2) build_catwalk_between(towers[0], towers[1]);
    if (num_towers >= 3) {
        build_catwalk_between(towers[0], towers[2]);
        build_catwalk_between(towers[1], towers[2]);
    }

    /* Scatter Explosive Fuel Barrel Clusters, Cargo Crates, Industrial Floodlights & Wrecks */
    for (i = 0; i < 12; i++) {
        int rx = rrange(2, 21), ry = rrange(2, 21);
        if (zone_map[ry][rx] == ZONE_FREE) {
            uint8_t obj = (i < 6) ? OBJ_BARREL : (i < 9) ? OBJ_CRATE :
                          (i < 11) ? OBJ_STREETLAMP : OBJ_CAR_HOOD;
            place_prop_if_clear(rx, ry, 0, obj);
        }
    }
}

/* =========================================================================
 * 3D BFS REACHABILITY SELF-HEALING & TACTICAL UNIT DEPLOYMENT
 * ========================================================================= */
static void compute_reachability(int start_x, int start_y) {
    static int16_t qx[MAP_LEVS * MAP_H * MAP_W];
    static int16_t qy[MAP_LEVS * MAP_H * MAP_W];
    static int8_t  qz[MAP_LEVS * MAP_H * MAP_W];
    static const int dx_tab[10] = { 0,  1,  1,  1,  0, -1, -1, -1,  0,  0 };
    static const int dy_tab[10] = {-1, -1,  0,  1,  1,  1,  0, -1,  0,  0 };
    static const int dz_tab[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  1, -1 };
    int head = 0, tail = 0;

    memset(reach_mask, 0, sizeof(reach_mask));
    reach_mask[0][start_y][start_x] = 1;
    qx[tail] = (int16_t)start_x;
    qy[tail] = (int16_t)start_y;
    qz[tail] = 0;
    tail++;

    while (head < tail) {
        int cx = qx[head], cy = qy[head], cz = qz[head];
        int d;
        head++;
        for (d = 0; d < 10; d++) {
            int nx = cx + dx_tab[d], ny = cy + dy_tab[d], nz = cz + dz_tab[d];
            int scost = 0;
            if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H || (unsigned)nz >= MAP_LEVS) continue;
            if (reach_mask[nz][ny][nx]) continue;
            if (pathfind_can_step(cx, cy, cz, nx, ny, nz, 0, &scost)) {
                reach_mask[nz][ny][nx] = 1;
                qx[tail] = (int16_t)nx;
                qy[tail] = (int16_t)ny;
                qz[tail] = (int8_t)nz;
                tail++;
            }
        }
    }
}

static int is_blocking_prop(uint8_t obj_id) {
    return (obj_id != OBJ_NONE && obj_id != OBJ_STAIRS_UP && obj_id != OBJ_RUBBLE);
}

static void open_edge_between(int z, int x0, int y0, int x1, int y1) {
    uint8_t *wptr = NULL;
    if (x1 == x0 + 1)      wptr = &g_map[z][y0][x1].wall_west;
    else if (x1 == x0 - 1) wptr = &g_map[z][y0][x0].wall_west;
    else if (y1 == y0 + 1) wptr = &g_map[z][y1][x0].wall_north;
    else if (y1 == y0 - 1) wptr = &g_map[z][y0][x0].wall_north;

    if (wptr && isomap_wall_blocks_move(*wptr)) {
        *wptr = (*wptr == WALL_FENCE || *wptr == WALL_FENCE_DMG) ? WALL_NONE : WALL_DOOR_OPEN;
    }
}

/* Guarantee every floor tile across ALL levels (z = 0, 1, 2) has a valid walkable path from the LZ! */
static void ensure_all_floors_reachable(int lz_cx, int lz_cy) {
    static const int odx[4] = { 0,  1,  0, -1 };
    static const int ody[4] = {-1,  0,  1,  0 };
    int i, pass;

    if (is_blocking_prop(g_map[0][lz_cy][lz_cx].obj_id)) {
        g_map[0][lz_cy][lz_cx].obj_id = OBJ_NONE;
    }
    compute_reachability(lz_cx, lz_cy);

    /* 1. First verify every ground-floor building & UFO interior has an open path */
    for (i = -1; i < g_num_bldgs; i++) {
        BldgRect r = (i < 0) ? g_ufo_rect : g_bldgs[i];
        int x, y, any_reach = 0;
        if (!r.valid) continue;
        for (y = r.y0; y <= r.y1 && !any_reach; y++) {
            for (x = r.x0; x <= r.x1; x++) {
                if (reach_mask[0][y][x]) { any_reach = 1; break; }
            }
        }
        if (!any_reach) {
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 0, (r.x0 + r.x1) / 2, WALL_DOOR_OPEN);
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 2, (r.x0 + r.x1) / 2, WALL_DOOR_OPEN);
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 3, (r.y0 + r.y1) / 2, WALL_DOOR_OPEN);
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 1, (r.y0 + r.y1) / 2, WALL_DOOR_OPEN);
            compute_reachability(lz_cx, lz_cy);
        }
    }

    /* 2. Audit every floor tile on z = 0, 1, 2; repair isolated pockets & missing upper-level stairs! */
    for (pass = 0; pass < 48; pass++) {
        int uz = -1, ux = -1, uy = -1;
        int z, y, x;

        for (z = 0; z < MAP_LEVS && uz < 0; z++) {
            for (y = 0; y < MAP_H && uz < 0; y++) {
                for (x = 0; x < MAP_W; x++) {
                    if (g_map[z][y][x].floor_id == FLOOR_NONE) continue;
                    if (is_blocking_prop(g_map[z][y][x].obj_id)) continue;
                    if (!reach_mask[z][y][x]) {
                        uz = z; ux = x; uy = y;
                        break;
                    }
                }
            }
        }
        if (uz < 0) break; /* 100% of walkable floor tiles across ALL levels are reachable! */

        {
            uint8_t in_comp[MAP_H][MAP_W];
            int16_t cx_q[MAP_W * MAP_H], cy_q[MAP_W * MAP_H];
            int qh = 0, qt = 0, repaired = 0, k, d;

            memset(in_comp, 0, sizeof(in_comp));
            in_comp[uy][ux] = 1;
            cx_q[qt] = (int16_t)ux;
            cy_q[qt] = (int16_t)uy;
            qt++;

            while (qh < qt) {
                int px = cx_q[qh], py = cy_q[qh];
                qh++;
                for (d = 0; d < 4; d++) {
                    int nx = px + odx[d], ny = py + ody[d];
                    if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H) continue;
                    if (in_comp[ny][nx]) continue;
                    if (g_map[uz][ny][nx].floor_id == FLOOR_NONE) continue;
                    if (reach_mask[uz][ny][nx]) continue;
                    in_comp[ny][nx] = 1;
                    cx_q[qt] = (int16_t)nx;
                    cy_q[qt] = (int16_t)ny;
                    qt++;
                }
            }

            /* A. Same-level wall/fence separating `in_comp` from an already-reachable tile on `uz` */
            for (k = 0; k < qt && !repaired; k++) {
                int px = cx_q[k], py = cy_q[k];
                if (is_blocking_prop(g_map[uz][py][px].obj_id)) continue;
                for (d = 0; d < 4; d++) {
                    int rx = px + odx[d], ry = py + ody[d];
                    if ((unsigned)rx >= MAP_W || (unsigned)ry >= MAP_H) continue;
                    if (reach_mask[uz][ry][rx]) {
                        open_edge_between(uz, px, py, rx, ry);
                        repaired = 1;
                        break;
                    }
                }
            }

            /* B. Removable blocking prop separating `in_comp` from a reachable tile on `uz` */
            for (k = 0; k < qt && !repaired; k++) {
                int px = cx_q[k], py = cy_q[k];
                uint8_t pobj = g_map[uz][py][px].obj_id;
                if (!is_blocking_prop(pobj) || pobj == OBJ_UFO_CORE) continue;
                for (d = 0; d < 4; d++) {
                    int rx = px + odx[d], ry = py + ody[d];
                    if ((unsigned)rx >= MAP_W || (unsigned)ry >= MAP_H) continue;
                    if (reach_mask[uz][ry][rx]) {
                        g_map[uz][py][px].obj_id = OBJ_NONE;
                        open_edge_between(uz, px, py, rx, ry);
                        repaired = 1;
                        break;
                    }
                }
            }

            /* C. Upper floor (uz > 0) missing staircase access from reachable tile on `uz - 1` */
            if (uz > 0 && !repaired) {
                int best_sx = -1, best_sy = -1, best_score = -1;
                for (k = 0; k < qt; k++) {
                    int sx = cx_q[k], sy = cy_q[k];
                    uint8_t low_obj = g_map[uz - 1][sy][sx].obj_id;
                    uint8_t up_obj  = g_map[uz][sy][sx].obj_id;
                    int score = 10;
                    if (!reach_mask[uz - 1][sy][sx]) continue;
                    if (low_obj == OBJ_STAIRS_UP || low_obj == OBJ_UFO_CORE) continue;
                    if (up_obj == OBJ_STAIRS_UP  || up_obj == OBJ_UFO_CORE) continue;
                    if (uz - 1 > 0 && g_map[uz - 2][sy][sx].obj_id == OBJ_STAIRS_UP) continue;
                    if (uz == 1 && zone_map[sy][sx] == ZONE_LZ) continue;
                    if (low_obj == OBJ_NONE) score += 20;
                    if (up_obj == OBJ_NONE)  score += 20;
                    if (uz == 1 && zone_map[sy][sx] != ZONE_DOORWAY) score += 10;
                    if (score > best_score) {
                        best_score = score;
                        best_sx = sx;
                        best_sy = sy;
                    }
                }
                if (best_sx >= 0) {
                    place_stairs_at(uz - 1, best_sx, best_sy);
                    for (d = 0; d < 4; d++) {
                        int nx = best_sx + odx[d], ny = best_sy + ody[d];
                        if ((unsigned)nx < MAP_W && (unsigned)ny < MAP_H && in_comp[ny][nx]) {
                            open_edge_between(uz, best_sx, best_sy, nx, ny);
                        }
                    }
                    repaired = 1;
                }
            }

            /* D. Upper floor overhang: place exterior landing staircase from adjacent reachable `uz - 1` tile */
            if (uz > 0 && !repaired) {
                for (k = 0; k < qt && !repaired; k++) {
                    int px = cx_q[k], py = cy_q[k];
                    if (is_blocking_prop(g_map[uz][py][px].obj_id)) continue;
                    for (d = 0; d < 4; d++) {
                        int ax = px + odx[d], ay = py + ody[d];
                        if ((unsigned)ax >= MAP_W || (unsigned)ay >= MAP_H) continue;
                        if (!reach_mask[uz - 1][ay][ax]) continue;
                        if (g_map[uz - 1][ay][ax].obj_id != OBJ_NONE) continue;
                        if (uz - 1 > 0 && g_map[uz - 2][ay][ax].obj_id == OBJ_STAIRS_UP) continue;
                        if (uz == 1 && zone_map[ay][ax] == ZONE_LZ) continue;
                        place_stairs_at(uz - 1, ax, ay);
                        open_edge_between(uz, ax, ay, px, py);
                        repaired = 1;
                        break;
                    }
                }
            }

            /* E. Fallback: step directly toward LZ and clear wall/prop along the way */
            if (!repaired) {
                int step_x = ux + ((lz_cx > ux) ? 1 : (lz_cx < ux ? -1 : 0));
                int step_y = (step_x == ux) ? (uy + ((lz_cy > uy) ? 1 : -1)) : uy;
                if ((unsigned)step_x < MAP_W && (unsigned)step_y < MAP_H) {
                    if (g_map[uz][step_y][step_x].floor_id == FLOOR_NONE) {
                        g_map[uz][step_y][step_x].floor_id = FLOOR_WOOD;
                    }
                    if (is_blocking_prop(g_map[uz][step_y][step_x].obj_id) &&
                        g_map[uz][step_y][step_x].obj_id != OBJ_UFO_CORE) {
                        g_map[uz][step_y][step_x].obj_id = OBJ_NONE;
                    }
                    open_edge_between(uz, ux, uy, step_x, step_y);
                }
            }
        }

        compute_reachability(lz_cx, lz_cy);
    }
}

static int tile_free_for_spawn(int x, int y, int z, const ProcSpawnPlan *plan, int placed_aliens) {
    int i;
    if ((unsigned)x >= MAP_W || (unsigned)y >= MAP_H || (unsigned)z >= MAP_LEVS) return 0;
    if (g_map[z][y][x].floor_id == FLOOR_NONE) return 0;
    if (g_map[z][y][x].obj_id != OBJ_NONE && g_map[z][y][x].obj_id != OBJ_RUBBLE) return 0;
    if (z > 0 && g_map[z - 1][y][x].obj_id == OBJ_STAIRS_UP) return 0;
    if (!reach_mask[z][y][x]) return 0;

    for (i = 0; i < 5; i++) {
        if (z == 0 && plan->squad_x[i] == x && plan->squad_y[i] == y) return 0;
    }
    for (i = 0; i < placed_aliens; i++) {
        if (plan->alien_x[i] == x && plan->alien_y[i] == y && plan->alien_z[i] == z) return 0;
    }
    return 1;
}

static int min_squad_dist(int x, int y, const ProcSpawnPlan *plan) {
    int i, best = 999;
    for (i = 0; i < 5; i++) {
        int d = abs(x - plan->squad_x[i]) + abs(y - plan->squad_y[i]);
        if (d < best) best = d;
    }
    return best;
}

static int any_squad_has_los(int x, int y, int z, const ProcSpawnPlan *plan) {
    int i;
    for (i = 0; i < 5; i++) {
        int d = abs(x - plan->squad_x[i]) + abs(y - plan->squad_y[i]);
        if (d <= 17 && isomap_has_los(plan->squad_x[i], plan->squad_y[i], 0, x, y, z)) {
            return 1;
        }
    }
    return 0;
}

static void populate_spawn_plan(int biome_id, ProcSpawnPlan *plan) {
    int cx = (g_lz_x0 + g_lz_x1) / 2;
    int cy = (g_lz_y0 + g_lz_y1) / 2;
    int a_count = 5 + (rand() & 1); /* 5 or 6 aliens per mission */
    int a;

    memset(plan, 0, sizeof(*plan));
    plan->squad_dir = (g_lz_side == 0) ? 0 : (g_lz_side == 1) ? 4 : (g_lz_side == 2) ? 2 : 6;

    /* 5-Agent DUFF-COM Skyranger Landing Wedge inside ZONE_LZ */
    if (g_lz_side == 0 || g_lz_side == 1) {
        int front_y = (g_lz_side == 0) ? g_lz_y0 : g_lz_y1;
        int back_y  = (g_lz_side == 0) ? (g_lz_y0 + 1) : (g_lz_y1 - 1);
        plan->squad_x[0] = cx;     plan->squad_y[0] = front_y;
        plan->squad_x[1] = cx - 1; plan->squad_y[1] = back_y;
        plan->squad_x[2] = cx + 1; plan->squad_y[2] = back_y;
        plan->squad_x[3] = cx - 2; plan->squad_y[3] = back_y;
        plan->squad_x[4] = cx + 2; plan->squad_y[4] = back_y;
    } else {
        int front_x = (g_lz_side == 2) ? g_lz_x1 : g_lz_x0;
        int back_x  = (g_lz_side == 2) ? (g_lz_x1 - 1) : (g_lz_x0 + 1);
        plan->squad_x[0] = front_x; plan->squad_y[0] = cy;
        plan->squad_x[1] = back_x;  plan->squad_y[1] = cy - 1;
        plan->squad_x[2] = back_x;  plan->squad_y[2] = cy + 1;
        plan->squad_x[3] = back_x;  plan->squad_y[3] = cy - 2;
        plan->squad_x[4] = back_x;  plan->squad_y[4] = cy + 2;
    }

    ensure_all_floors_reachable(plan->squad_x[0], plan->squad_y[0]);

    plan->num_aliens = a_count;
    for (a = 0; a < a_count; a++) {
        int utype, found = 0, tries;
        int ax = 12, ay = 12, az = 0;

        /* Role-based + Biome-weighted randomized alien species */
        if (a == 0) {
            utype = (biome_id == BIOME_STEEL || (rand() & 1)) ? 3 : 1; /* UFO Commander: Muton or Sectoid */
            plan->alien_name[a] = (utype == 3) ? "MUTON COMMANDER" : "SECTOID LEADER";
        } else if (a == 1) {
            utype = 2; /* Rooftop / Hayloft / Catwalk Floater Sniper */
            plan->alien_name[a] = "FLOATER SNIPER";
        } else if (a == 2) {
            utype = (rand() % 100 < 55) ? 1 : 3; /* Building Interior Lurker */
            plan->alien_name[a] = (utype == 3) ? "MUTON ENFORCER" : "SECTOID STALKER";
        } else {
            int r = rand() % 100;
            utype = (r < 45) ? 1 : (r < 78) ? 2 : 3;
            plan->alien_name[a] = (utype == 1) ? "SECTOID SCOUT" :
                                  (utype == 2) ? "FLOATER PATROL" : "MUTON WARRIOR";
        }

        /* Role 0: Inside UFO Hull */
        if (a == 0 && g_ufo_rect.valid) {
            for (tries = 0; tries < 40 && !found; tries++) {
                int tx = rrange(g_ufo_rect.x0, g_ufo_rect.x1);
                int ty = rrange(g_ufo_rect.y0, g_ufo_rect.y1);
                if (tile_free_for_spawn(tx, ty, 0, plan, a)) {
                    ax = tx; ay = ty; az = 0; found = 1;
                }
            }
        }
        /* Role 1: Elevated z=1 Rooftop, Hayloft, or Catwalk Overwatch */
        else if (a == 1) {
            for (tries = 0; tries < 90 && !found; tries++) {
                int tx = rrange(2, 21), ty = rrange(2, 21);
                if (tile_free_for_spawn(tx, ty, 1, plan, a) &&
                    min_squad_dist(tx, ty, plan) >= 10 &&
                    !any_squad_has_los(tx, ty, 1, plan)) {
                    ax = tx; ay = ty; az = 1; found = 1;
                }
            }
        }
        /* Role 2: Inside a Diner / Arcade / Barn / House / Steel Tower */
        else if (a == 2 && g_num_bldgs > 0) {
            for (tries = 0; tries < 70 && !found; tries++) {
                BldgRect b = g_bldgs[rand() % g_num_bldgs];
                int tx = rrange(b.x0, b.x1), ty = rrange(b.y0, b.y1);
                if (tile_free_for_spawn(tx, ty, 0, plan, a) &&
                    min_squad_dist(tx, ty, plan) >= 9 &&
                    !any_squad_has_los(tx, ty, 0, plan)) {
                    ax = tx; ay = ty; az = 0; found = 1;
                }
            }
        }

        /* General Flanking Patrol / Fallback Placement (no Turn-1 spawn-camp LOS!) */
        for (tries = 0; tries < 150 && !found; tries++) {
            int tx = rrange(1, 22), ty = rrange(1, 22);
            if (zone_map[ty][tx] == ZONE_LZ) continue;
            if (!tile_free_for_spawn(tx, ty, 0, plan, a)) continue;
            if (min_squad_dist(tx, ty, plan) < (tries < 100 ? 11 : 7)) continue;
            if (tries < 110 && any_squad_has_los(tx, ty, 0, plan)) continue;
            ax = tx; ay = ty; az = 0; found = 1;
        }

        /* Deterministic safety scan so a unit is NEVER placed on a blocked tile */
        if (!found) {
            int tx, ty, best_d = -1;
            for (ty = 1; ty <= 22; ty++) {
                for (tx = 1; tx <= 22; tx++) {
                    if (tile_free_for_spawn(tx, ty, 0, plan, a)) {
                        int d = min_squad_dist(tx, ty, plan);
                        if (d > best_d) {
                            best_d = d; ax = tx; ay = ty; az = 0; found = 1;
                        }
                    }
                }
            }
        }

        plan->alien_type[a] = utype;
        plan->alien_x[a]    = ax;
        plan->alien_y[a]    = ay;
        plan->alien_z[a]    = az;
        plan->alien_dir[a]  = calc_dir_vec(plan->squad_x[0] - ax, plan->squad_y[0] - ay);
    }
}

void procgen_build_battlefield(int biome_id, ProcSpawnPlan *out_plan) {
    int b = biome_id % BIOME_COUNT;
    if (b == BIOME_RTE309)       gen_biome_rte309();
    else if (b == BIOME_PA_FARM) gen_biome_pa_farm();
    else if (b == BIOME_SUBURB)  gen_biome_suburb();
    else                         gen_biome_steel();

    if (out_plan) {
        populate_spawn_plan(b, out_plan);
    } else {
        ensure_all_floors_reachable((g_lz_x0 + g_lz_x1) / 2, (g_lz_y0 + g_lz_y1) / 2);
    }
    isomap_init_wall_hp();
}
