#include "procgen.h"
#include "isomap.h"
#include "gfx_assets.h"
#include "pathfind.h"
#include "campaign.h"
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

#define MAX_PROC_BLDGS 12
#define LOT_MAX_X      (MAP_W - 3)
#define LOT_MAX_Y      (MAP_H - 3)

static uint8_t zone_map[MAP_H][MAP_W];
static uint8_t reach_mask[MAP_LEVS][MAP_H][MAP_W];

static BldgRect g_bldgs[MAX_PROC_BLDGS];
static int      g_num_bldgs = 0;
static BldgRect g_ufo_rect;
static int      g_ufo_sz = 5;
static int      g_ufo_door_side = 2;
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
                g_map[z][y][x].unit_idx    = -1;
                g_map[z][y][x].gnd_item[0] = -1;
                g_map[z][y][x].gnd_item[1] = -1;
                g_map[z][y][x].gnd_item[2] = -1;
                g_map[z][y][x].gnd_item[3] = -1;
                g_map[z][y][x].fog         = FOG_UNSEEN;
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
        g_lz_x0 = rrange(4, MAP_W - 9); g_lz_x1 = g_lz_x0 + 4;
        g_lz_y0 = MAP_H - 5;            g_lz_y1 = MAP_H - 3;
    } else if (g_lz_side == 1) { /* North Edge */
        g_lz_x0 = rrange(4, MAP_W - 9); g_lz_x1 = g_lz_x0 + 4;
        g_lz_y0 = 2;                    g_lz_y1 = 4;
    } else if (g_lz_side == 2) { /* West Edge */
        g_lz_x0 = 2;                    g_lz_x1 = 4;
        g_lz_y0 = rrange(4, MAP_H - 9); g_lz_y1 = g_lz_y0 + 4;
    } else {                     /* East Edge */
        g_lz_x0 = MAP_W - 5;            g_lz_x1 = MAP_W - 3;
        g_lz_y0 = rrange(4, MAP_H - 9); g_lz_y1 = g_lz_y0 + 4;
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
        uint8_t obj = g_map[0][y][x].obj_id;
        if (!isomap_is_lift_obj(obj) && !isomap_is_ufo_corner_obj(obj) && obj != OBJ_UFO_CORE) {
            isomap_clear_prop_at(0, y, x, OBJ_NONE, 0);
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
    isomap_clear_prop_at(z, sy, sx, OBJ_NONE, 0);
    isomap_clear_prop_at(z + 1, sy, sx, OBJ_NONE, 0);
    g_map[z][sy][sx].obj_id = OBJ_STAIRS_UP;
    if (g_map[z + 1][sy][sx].floor_id == FLOOR_NONE) {
        g_map[z + 1][sy][sx].floor_id = FLOOR_WOOD;
    }
}

static void place_stairs_safe(int sx, int sy) {
    place_stairs_at(0, sx, sy);
}

static int prop_tile_clear(int x, int y, int z) {
    if ((unsigned)x >= MAP_W || (unsigned)y >= MAP_H || (unsigned)z >= MAP_LEVS) return 0;
    if (z == 0 && (zone_map[y][x] == ZONE_DOORWAY || zone_map[y][x] == ZONE_LZ)) return 0;
    if (g_map[z][y][x].floor_id == FLOOR_NONE) return 0;
    if (g_map[z][y][x].obj_id != OBJ_NONE) return 0;
    /* Never block top landing of a staircase or UFO grav-lift from the level below! */
    if (z > 0 && isomap_is_lift_obj(g_map[z - 1][y][x].obj_id)) return 0;
    /* Never block adjacent doorways or alien airlocks on any story */
    if (isomap_is_door_closed(g_map[z][y][x].wall_west) || isomap_is_door_open(g_map[z][y][x].wall_west)) return 0;
    if (isomap_is_door_closed(g_map[z][y][x].wall_north) || isomap_is_door_open(g_map[z][y][x].wall_north)) return 0;
    if (x + 1 < MAP_W && (isomap_is_door_closed(g_map[z][y][x + 1].wall_west) || isomap_is_door_open(g_map[z][y][x + 1].wall_west))) return 0;
    if (y + 1 < MAP_H && (isomap_is_door_closed(g_map[z][y + 1][x].wall_north) || isomap_is_door_open(g_map[z][y + 1][x].wall_north))) return 0;
    return 1;
}

static void place_prop_if_clear(int x, int y, int z, uint8_t obj_id) {
    if (!prop_tile_clear(x, y, z)) return;
    g_map[z][y][x].obj_id = obj_id;
}

/* Place a 2-tile vehicle (2x1 along +X when along_y==0, or 1x2 along +Y when along_y==1) */
static int place_vehicle_if_clear(int x, int y, int z, int along_y, int veh_kind) {
    int x2 = along_y ? x : (x + 1);
    int y2 = along_y ? (y + 1) : y;
    int bx = along_y ? x : (x - 1);
    int by = along_y ? (y - 1) : y;
    int fx = along_y ? x2 : (x2 + 1);
    int fy = along_y ? (y2 + 1) : y2;
    uint8_t obj_back, obj_front;

    if (!prop_tile_clear(x, y, z) || !prop_tile_clear(x2, y2, z)) return 0;
    if (z == 0 && zone_map[y][x] != zone_map[y2][x2]) return 0;

    /* Never allow a 2-tile vehicle to straddle a wall or fence edge */
    if (!along_y && g_map[z][y][x2].wall_west != WALL_NONE) return 0;
    if (along_y  && g_map[z][y2][x].wall_north != WALL_NONE) return 0;

    /* Leave at least a 1-tile walking gap ahead/behind other parked vehicles */
    if ((unsigned)bx < MAP_W && (unsigned)by < MAP_H &&
        isomap_is_vehicle_obj(g_map[z][by][bx].obj_id)) return 0;
    if ((unsigned)fx < MAP_W && (unsigned)fy < MAP_H &&
        isomap_is_vehicle_obj(g_map[z][fy][fx].obj_id)) return 0;

    switch (veh_kind) {
        case VEH_SEDAN:
            obj_back  = along_y ? OBJ_SEDAN_BACK_Y   : OBJ_SEDAN_BACK_X;
            obj_front = along_y ? OBJ_SEDAN_FRONT_Y  : OBJ_SEDAN_FRONT_X;
            break;
        case VEH_PICKUP:
            obj_back  = along_y ? OBJ_PICKUP_BACK_Y  : OBJ_PICKUP_BACK_X;
            obj_front = along_y ? OBJ_PICKUP_FRONT_Y : OBJ_PICKUP_FRONT_X;
            break;
        case VEH_TRACTOR:
            obj_back  = along_y ? OBJ_TRACTOR_BACK_Y  : OBJ_TRACTOR_BACK_X;
            obj_front = along_y ? OBJ_TRACTOR_FRONT_Y : OBJ_TRACTOR_FRONT_X;
            break;
        default: /* VEH_CRUISER */
            obj_back  = along_y ? OBJ_CAR_BACK_Y  : OBJ_CAR_BACK_X;
            obj_front = along_y ? OBJ_CAR_FRONT_Y : OBJ_CAR_FRONT_X;
            break;
    }

    g_map[z][y][x].obj_id   = obj_back;
    g_map[z][y2][x2].obj_id = obj_front;
    return 1;
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
            isomap_clear_prop_at(0, y, x, OBJ_NONE, 0);
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

    if (g_num_bldgs < MAX_PROC_BLDGS) {
        g_bldgs[g_num_bldgs++] = b;
    }
    return b;
}

/* Search randomized candidate coordinates for a free building lot */
static int find_random_lot(int w, int h, int min_x, int max_x, int min_y, int max_y,
                           int allow_road, int *out_x0, int *out_y0) {
    int attempt;
    if (max_x - w + 1 < min_x || max_y - h + 1 < min_y) return 0;
    for (attempt = 0; attempt < 140; attempt++) {
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
    return find_random_lot(5, 5, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, out_x0, out_y0);
}

/* Map canonical UFO coordinates (rx, ry in 0..sz-1; ry=0 Forward Bridge, ry=sz-1 Aft Airlock) to world (wx, wy) */
static void ufo_rot_xy(int ux, int uy, int sz, int door_side, int rx, int ry, int *wx, int *wy) {
    if (door_side == 2) {        /* South Airlock */
        *wx = ux + rx;
        *wy = uy + ry;
    } else if (door_side == 0) { /* North Airlock */
        *wx = ux + (sz - 1 - rx);
        *wy = uy + (sz - 1 - ry);
    } else if (door_side == 1) { /* East Airlock */
        *wx = ux + ry;
        *wy = uy + (sz - 1 - rx);
    } else {                     /* West Airlock */
        *wx = ux + (sz - 1 - ry);
        *wy = uy + rx;
    }
}

static void place_ufo_edge_between(int x0, int y0, int x1, int y1, uint8_t wall_type) {
    if (x1 == x0 + 1 && y1 == y0)      g_map[0][y0][x1].wall_west  = wall_type;
    else if (x0 == x1 + 1 && y1 == y0) g_map[0][y0][x0].wall_west  = wall_type;
    else if (y1 == y0 + 1 && x1 == x0) g_map[0][y1][x0].wall_north = wall_type;
    else if (y0 == y1 + 1 && x1 == x0) g_map[0][y0][x0].wall_north = wall_type;
}

static void place_ufo_prop_local(int ux, int uy, int sz, int door_side, int rx, int ry, uint8_t obj_id) {
    int wx, wy;
    ufo_rot_xy(ux, uy, sz, door_side, rx, ry, &wx, &wy);
    if ((unsigned)wx < MAP_W && (unsigned)wy < MAP_H && g_map[0][wy][wx].obj_id == OBJ_NONE) {
        g_map[0][wy][wx].obj_id = obj_id;
    }
}

/* Place Rounded Futuristic Alien Saucer (5x5 Scout or 7x7 Multi-Room Cruiser/Battleship, Landed or Crashed!) */
static void place_procedural_ufo(void) {
    int ufo_class = 0; /* 0=Small Scout (5x5), 1=Medium Cruiser (7x7), 2=Large Assault Ship (7x7 Heavy) */
    int sz = 5;
    int crashed = 0;   /* Unless shot down (severity == 0), landed UFOs are completely undamaged! */
    int min_x = 2, max_x = LOT_MAX_X, min_y = 2, max_y = LOT_MAX_Y;
    int ux, uy, ux1, uy1, cx, cy, x, y, door_side;

    /* Sync UFO hull class & crash state with the active Lehigh Valley Radar Incident! */
    if (g_camp.launched_event >= 0 && g_camp.launched_event < MAX_RADAR_EVENTS &&
        g_camp.events[g_camp.launched_event].active) {
        const RadarEvent *ev = &g_camp.events[g_camp.launched_event];
        crashed   = (ev->severity == 0) ? 1 : 0;
        ufo_class = ev->ufo_size;
        sz        = (ufo_class == 0) ? 5 : 7;
    }


    ux = (MAP_W - sz) / 2;
    uy = 3;

    /* Bias UFO into the far sector opposite the DUFF-COM Drop Zone */
    if (g_lz_side == 0)      { max_y = (MAP_H * 2) / 5;  }
    else if (g_lz_side == 1) { min_y = MAP_H - 1 - (MAP_H * 2) / 5; }
    else if (g_lz_side == 2) { min_x = MAP_W - 1 - (MAP_W * 2) / 5; }
    else                     { max_x = (MAP_W * 2) / 5;  }

    if (!find_random_lot(sz, sz, min_x, max_x, min_y, max_y, 1, &ux, &uy)) {
        (void)find_random_lot(sz, sz, 2, LOT_MAX_X, 2, LOT_MAX_Y, 1, &ux, &uy);
    }

    ux1 = ux + sz - 1;
    uy1 = uy + sz - 1;
    cx  = ux + sz / 2;
    cy  = uy + sz / 2;

    g_ufo_rect.valid = 1;
    g_ufo_rect.x0 = ux;  g_ufo_rect.y0 = uy;
    g_ufo_rect.x1 = ux1; g_ufo_rect.y1 = uy1;
    g_ufo_rect.stories = 1;
    g_ufo_sz = sz;

    /* Optional scorched skid-furrow trailing behind a crash-landed UFO */
    if (crashed) {
        int sx_dir = (cx < MAP_W / 2) ? -1 : 1;
        int sy_dir = (cy < MAP_H / 2) ? -1 : 1;
        int k;
        for (k = 1; k <= 6; k++) {
            int tx = cx + sx_dir * (sz / 2 + k);
            int ty = cy + sy_dir * ((k + 1) / 2);
            if ((unsigned)tx < MAP_W && (unsigned)ty < MAP_H && zone_map[ty][tx] != ZONE_LZ) {
                g_map[0][ty][tx].floor_id = FLOOR_SCORCHED;
                if (k == 1) g_map[0][ty][tx].smoke = (uint8_t)(2 + (rand() % 3));
                else if (k <= 3 && (rand() & 1)) g_map[0][ty][tx].smoke = (uint8_t)(1 + (rand() & 1));
                if (k == 2 && (rand() & 1)) isomap_clear_prop_at(0, ty, tx, OBJ_RUBBLE, 0);
            }
        }
    }

    /* Build Rounded Saucer Hull: curved 3D corner fairings + glowing alien deck & domed roof */
    for (y = uy; y <= uy1; y++) {
        for (x = ux; x <= ux1; x++) {
            int is_n = (x == ux  && y == uy);
            int is_e = (x == ux1 && y == uy);
            int is_s = (x == ux1 && y == uy1);
            int is_w = (x == ux  && y == uy1);

            zone_map[y][x] = ZONE_UFO;
            isomap_clear_prop_at(0, y, x, OBJ_NONE, 0);
            g_map[0][y][x].wall_west  = WALL_NONE;
            g_map[0][y][x].wall_north = WALL_NONE;

            if (is_n || is_e || is_s || is_w) {
                /* Keep outdoor ground outside curved saucer arc; cap z=1 with rounded dome rim */
                if (crashed && ((x + y) & 1)) {
                    g_map[0][y][x].floor_id = FLOOR_SCORCHED;
                }
                g_map[0][y][x].obj_id = is_n ? OBJ_UFO_CORNER_N :
                                        is_e ? OBJ_UFO_CORNER_E :
                                        is_s ? OBJ_UFO_CORNER_S : OBJ_UFO_CORNER_W;
                g_map[1][y][x].floor_id = is_n ? FLOOR_UFO_ROOF_N :
                                          is_e ? FLOOR_UFO_ROOF_E :
                                          is_s ? FLOOR_UFO_ROOF_S : FLOOR_UFO_ROOF_W;
            } else {
                g_map[0][y][x].floor_id = FLOOR_UFO_DECK;
                g_map[1][y][x].floor_id = FLOOR_UFO_ROOF;
            }
        }
    }

    /* Clear exterior south/east boundary edges, then place convex saucer hull along straight spans */
    for (x = ux; x <= ux1; x++) g_map[0][uy1 + 1][x].wall_north = WALL_NONE;
    for (y = uy; y <= uy1; y++) g_map[0][y][ux1 + 1].wall_west  = WALL_NONE;

    for (x = ux + 1; x <= ux1 - 1; x++) {
        g_map[0][uy][x].wall_north      = WALL_UFO_HULL;
        g_map[0][uy1 + 1][x].wall_north = WALL_UFO_HULL;
    }
    for (y = uy + 1; y <= uy1 - 1; y++) {
        g_map[0][y][ux].wall_west      = WALL_UFO_HULL;
        g_map[0][y][ux1 + 1].wall_west = WALL_UFO_HULL;
    }

    /* Orient main oval alien iris airlock toward the center of the battlefield */
    {
        int mid_x = MAP_W / 2, mid_y = MAP_H / 2;
        if (abs(mid_y - cy) >= abs(mid_x - cx)) {
            door_side = (cy <= mid_y) ? 2 : 0;
        } else {
            door_side = (cx <= mid_x) ? 1 : 3;
        }
        g_ufo_door_side = door_side;

        if (door_side == 0 || door_side == 2) {
            carve_rect_door(ux, uy, ux1, uy1, door_side, cx, WALL_UFO_DOOR_CLOSED);
        } else {
            carve_rect_door(ux, uy, ux1, uy1, door_side, cy, WALL_UFO_DOOR_CLOSED);
        }
    }

    /* Furnish Alien Saucer Interior: Dedicated Navigation Area, Reactor Core, Cryo, Pods & Grav-Lift */
    if (sz == 7) {
        int rx;
        /* Internal Cruiser Bulkhead + Centered Alien Airlock sealing the Navigation Bridge (ry=0..1) */
        for (rx = 0; rx < sz; rx++) {
            int x_fwd, y_fwd, x_aft, y_aft;
            uint8_t bw = (rx == sz / 2) ? WALL_UFO_DOOR_CLOSED : WALL_UFO_HULL;
            ufo_rot_xy(ux, uy, sz, door_side, rx, 1, &x_fwd, &y_fwd);
            ufo_rot_xy(ux, uy, sz, door_side, rx, 2, &x_aft, &y_aft);
            place_ufo_edge_between(x_fwd, y_fwd, x_aft, y_aft, bw);
        }

        /* Forward Navigation Bridge: Twin Holo Star-Maps + Alien Flight Telemetry Banks */
        place_ufo_prop_local(ux, uy, sz, door_side, 1, 0, OBJ_UFO_COMPUTER);
        place_ufo_prop_local(ux, uy, sz, door_side, 2, 0, OBJ_UFO_NAV);
        place_ufo_prop_local(ux, uy, sz, door_side, 4, 0, OBJ_UFO_NAV);
        place_ufo_prop_local(ux, uy, sz, door_side, 5, 0, OBJ_UFO_COMPUTER);

        /* Main Reactor & Bio-Stasis Deck: Central Elerium Core, Cryo Chambers, Tech Pods & Grav-Lift */
        place_ufo_prop_local(ux, uy, sz, door_side, 3, 3, OBJ_UFO_CORE);
        place_ufo_prop_local(ux, uy, sz, door_side, 1, 2, OBJ_UFO_CRYO);
        place_ufo_prop_local(ux, uy, sz, door_side, 5, 2, OBJ_UFO_CRYO);
        place_ufo_prop_local(ux, uy, sz, door_side, 1, 4, OBJ_UFO_POD);
        place_ufo_prop_local(ux, uy, sz, door_side, 2, 5, OBJ_UFO_POD);
        place_ufo_prop_local(ux, uy, sz, door_side, 4, 5, OBJ_UFO_POD);
        place_ufo_prop_local(ux, uy, sz, door_side, 5, 4, OBJ_UFO_GRAVLIFT);

        /* Large Assault Ships carry a Twin Elerium Reactor Core & extra Stasis/Ordnance Pods! */
        if (ufo_class >= 2) {
            place_ufo_prop_local(ux, uy, sz, door_side, 3, 4, OBJ_UFO_CORE);
            place_ufo_prop_local(ux, uy, sz, door_side, 3, 5, OBJ_UFO_POD);
            place_ufo_prop_local(ux, uy, sz, door_side, 1, 3, OBJ_UFO_CRYO);
            place_ufo_prop_local(ux, uy, sz, door_side, 5, 3, OBJ_UFO_CRYO);
        }
    } else {

        /* 5x5 Scout Saucer: Forward Navigation Alcove + Central Reactor + Cryo/Pod Bays + Grav-Lift */
        place_ufo_prop_local(ux, uy, sz, door_side, 1, 0, OBJ_UFO_NAV);
        place_ufo_prop_local(ux, uy, sz, door_side, 3, 0, OBJ_UFO_COMPUTER);
        place_ufo_prop_local(ux, uy, sz, door_side, 2, 2, OBJ_UFO_CORE);
        place_ufo_prop_local(ux, uy, sz, door_side, 0, 2, OBJ_UFO_CRYO);
        place_ufo_prop_local(ux, uy, sz, door_side, 3, 3, OBJ_UFO_POD);
        place_ufo_prop_local(ux, uy, sz, door_side, 3, 1, OBJ_UFO_GRAVLIFT);
    }

    /* Crash-landed saucers get scorched battle-damaged hull plates + a smoking flank hull breach! */
    if (crashed) {
        int br_rx = (rand() & 1) ? 0 : (sz - 1);
        int br_ry = (sz == 7) ? 4 : 3;
        int bx, by, b_side, b_pos;
        uint8_t b_smoke = (uint8_t)(3 + (rand() % 3));
        uint8_t b_fire  = (uint8_t)(2 + (rand() & 1));

        ufo_rot_xy(ux, uy, sz, door_side, br_rx, br_ry, &bx, &by);
        if (by == uy)       { b_side = 0; b_pos = bx; }
        else if (by == uy1) { b_side = 2; b_pos = bx; }
        else if (bx == ux)  { b_side = 3; b_pos = by; }
        else                { b_side = 1; b_pos = by; }

        carve_rect_door(ux, uy, ux1, uy1, b_side, b_pos, WALL_NONE);
        g_map[0][by][bx].floor_id = FLOOR_SCORCHED;
        isomap_clear_prop_at(0, by, bx, OBJ_RUBBLE, b_smoke);
        g_map[0][by][bx].fire = b_fire;

        /* Scar neighboring hull segments with ruptured plasma conduits */
        if (b_side == 0 || b_side == 2) {
            int wy_edge = (b_side == 0) ? uy : (uy1 + 1);
            if (b_pos - 1 > ux  && g_map[0][wy_edge][b_pos - 1].wall_north == WALL_UFO_HULL)
                g_map[0][wy_edge][b_pos - 1].wall_north = WALL_UFO_HULL_DMG;
            if (b_pos + 1 < ux1 && g_map[0][wy_edge][b_pos + 1].wall_north == WALL_UFO_HULL)
                g_map[0][wy_edge][b_pos + 1].wall_north = WALL_UFO_HULL_DMG;
        } else {
            int wx_edge = (b_side == 3) ? ux : (ux1 + 1);
            if (b_pos - 1 > uy  && g_map[0][b_pos - 1][wx_edge].wall_west == WALL_UFO_HULL)
                g_map[0][b_pos - 1][wx_edge].wall_west = WALL_UFO_HULL_DMG;
            if (b_pos + 1 < uy1 && g_map[0][b_pos + 1][wx_edge].wall_west == WALL_UFO_HULL)
                g_map[0][b_pos + 1][wx_edge].wall_west = WALL_UFO_HULL_DMG;
        }
    }
}

/* Lay a paved highway or neighborhood street across the map */
static void paint_road_strip(int vertical, int start_coord, int asphalt_w, int with_lamps, int num_cars) {
    int road_len = vertical ? MAP_H : MAP_W;
    int i, w, c;
    for (i = 0; i < road_len; i++) {
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
        for (i = rrange(2, 4); i < road_len - 2; i += step) {
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
        int vk = (c == 0) ? VEH_SEDAN : ((c == 1) ? VEH_CRUISER : VEH_PICKUP);
        int tries;
        for (tries = 0; tries < 24; tries++) {
            int pos  = rrange(2, road_len - 4);
            int lane = start_coord + (rand() % asphalt_w);
            if (vertical) {
                if (place_vehicle_if_clear(lane, pos, 0, 1, vk)) break;
            } else {
                if (place_vehicle_if_clear(pos, lane, 0, 0, vk)) break;
            }
        }
    }
}

/* Lay a perpendicular side-street or farm lane spur without overwriting primary asphalt */
static void paint_side_road(int vertical, int start_coord, int seg_lo, int seg_hi,
                            int width, uint8_t road_floor, int with_walk) {
    int i, w;
    for (i = seg_lo; i <= seg_hi; i++) {
        if (vertical) {
            if ((unsigned)i >= MAP_H) continue;
            if (with_walk) {
                int s0 = start_coord - 1, s1 = start_coord + width;
                if ((unsigned)s0 < MAP_W && g_map[0][i][s0].floor_id == FLOOR_GRASS) {
                    g_map[0][i][s0].floor_id = FLOOR_SIDEWALK;
                    if (zone_map[i][s0] == ZONE_FREE) zone_map[i][s0] = ZONE_ROAD;
                }
                if ((unsigned)s1 < MAP_W && g_map[0][i][s1].floor_id == FLOOR_GRASS) {
                    g_map[0][i][s1].floor_id = FLOOR_SIDEWALK;
                    if (zone_map[i][s1] == ZONE_FREE) zone_map[i][s1] = ZONE_ROAD;
                }
            }
            for (w = 0; w < width; w++) {
                int rx = start_coord + w;
                if ((unsigned)rx < MAP_W) {
                    uint8_t cur = g_map[0][i][rx].floor_id;
                    if (cur != FLOOR_ASPHALT && cur != FLOOR_ASPHALT_LINE) {
                        g_map[0][i][rx].floor_id = road_floor;
                    }
                    if (zone_map[i][rx] == ZONE_FREE) zone_map[i][rx] = ZONE_ROAD;
                }
            }
        } else {
            if ((unsigned)i >= MAP_W) continue;
            if (with_walk) {
                int s0 = start_coord - 1, s1 = start_coord + width;
                if ((unsigned)s0 < MAP_H && g_map[0][s0][i].floor_id == FLOOR_GRASS) {
                    g_map[0][s0][i].floor_id = FLOOR_SIDEWALK;
                    if (zone_map[s0][i] == ZONE_FREE) zone_map[s0][i] = ZONE_ROAD;
                }
                if ((unsigned)s1 < MAP_H && g_map[0][s1][i].floor_id == FLOOR_GRASS) {
                    g_map[0][s1][i].floor_id = FLOOR_SIDEWALK;
                    if (zone_map[s1][i] == ZONE_FREE) zone_map[s1][i] = ZONE_ROAD;
                }
            }
            for (w = 0; w < width; w++) {
                int ry = start_coord + w;
                if ((unsigned)ry < MAP_H) {
                    uint8_t cur = g_map[0][ry][i].floor_id;
                    if (cur != FLOOR_ASPHALT && cur != FLOOR_ASPHALT_LINE) {
                        g_map[0][ry][i].floor_id = road_floor;
                    }
                    if (zone_map[ry][i] == ZONE_FREE) zone_map[ry][i] = ZONE_ROAD;
                }
            }
        }
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
    int hwy_pos    = rrange(MAP_W / 2 - 4, MAP_W / 2 + 1);
    int swap_sides = rand() & 1;
    int bx, by, w, h, k;

    init_blank_map(FLOOR_GRASS);
    reserve_lz();
    vert_hwy = (g_lz_side <= 1) ? ((rand() % 100) < 70) : ((rand() % 100) < 35);

    paint_road_strip(vert_hwy, hwy_pos, 4, 1, 0);
    /* Secondary perpendicular access street branching off Route 309 */
    {
        int spur_pos = rrange(8, (vert_hwy ? MAP_H : MAP_W) - 10);
        if (rand() & 1) {
            paint_side_road(!vert_hwy, spur_pos, 0, hwy_pos, 2, FLOOR_ASPHALT, 1);
        } else {
            paint_side_road(!vert_hwy, spur_pos, hwy_pos + 3,
                            (vert_hwy ? MAP_W : MAP_H) - 1, 2, FLOOR_ASPHALT, 1);
        }
    }
    place_procedural_ufo();

    /* Place Route 309 Chrome Diner & 2-Story Duff's Arcade across the highway! */
    w = rrange(6, 7); h = rrange(6, 8);
    if (vert_hwy) {
        int d_minx = swap_sides ? (hwy_pos + 5) : 2;
        int d_maxx = swap_sides ? LOT_MAX_X : (hwy_pos - 2);
        int a_minx = swap_sides ? 2 : (hwy_pos + 5);
        int a_maxx = swap_sides ? (hwy_pos - 2) : LOT_MAX_X;

        if (find_lot_flex(&w, &h, d_minx, d_maxx, 2, LOT_MAX_Y, &bx, &by)) {
            BldgRect d = build_structure(bx, by, bx + w - 1, by + h - 1, 1,
                                         FLOOR_DINER_TILE, FLOOR_ROOF_TAR,
                                         WALL_DINER_CHROME, WALL_DINER_GLASS, 65, 1);
            furnish_diner(d, swap_sides ? 3 : 1);
        }

        w = rrange(6, 7); h = rrange(6, 8);
        if (find_lot_flex(&w, &h, a_minx, a_maxx, 2, LOT_MAX_Y, &bx, &by)) {
            BldgRect a = build_structure(bx, by, bx + w - 1, by + h - 1, 2,
                                         FLOOR_ARCADE_CARPET, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 45, 0);
            furnish_arcade(a, swap_sides ? 1 : 3);
        }
    } else {
        int d_miny = swap_sides ? (hwy_pos + 5) : 2;
        int d_maxy = swap_sides ? LOT_MAX_Y : (hwy_pos - 2);
        int a_miny = swap_sides ? 2 : (hwy_pos + 5);
        int a_maxy = swap_sides ? (hwy_pos - 2) : LOT_MAX_Y;

        w = rrange(6, 8); h = rrange(6, 7);
        if (find_lot_flex(&w, &h, 2, LOT_MAX_X, d_miny, d_maxy, &bx, &by)) {
            BldgRect d = build_structure(bx, by, bx + w - 1, by + h - 1, 1,
                                         FLOOR_DINER_TILE, FLOOR_ROOF_TAR,
                                         WALL_DINER_CHROME, WALL_DINER_GLASS, 65, 1);
            furnish_diner(d, swap_sides ? 0 : 2);
        }

        w = rrange(6, 8); h = rrange(6, 7);
        if (find_lot_flex(&w, &h, 2, LOT_MAX_X, a_miny, a_maxy, &bx, &by)) {
            BldgRect a = build_structure(bx, by, bx + w - 1, by + h - 1, 2,
                                         FLOOR_ARCADE_CARPET, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 45, 0);
            furnish_arcade(a, swap_sides ? 2 : 0);
        }
    }

    /* 3rd Building: Roadside Auto Service Garage / Body Shop */
    if (find_random_lot(5, 5, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
        BldgRect g = build_structure(bx, by, bx + 4, by + 4, 1,
                                     FLOOR_SIDEWALK, FLOOR_ROOF_TAR,
                                     WALL_BRICK, WALL_BRICK_WIN, 35, 0);
        carve_rect_door(g.x0, g.y0, g.x1, g.y1, 2, g.x0 + 2, WALL_DOOR_OPEN);
        carve_rect_door(g.x0, g.y0, g.x1, g.y1, 0, g.x0 + 2, WALL_DOOR_CLOSED);
        place_stairs_at(0, g.x0, g.y0);
        place_prop_if_clear(g.x0 + 3, g.y0 + 3, 0, OBJ_BARREL);
        place_prop_if_clear(g.x0 + 1, g.y0 + 2, 0, OBJ_CRATE);
    }

    /* 4th Building: Route 309 Roadside Motel / Mini-Mart Office */
    w = rrange(5, 6); h = rrange(5, 6);
    if (find_random_lot(w, h, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
        int st = 1 + (rand() & 1);
        BldgRect m = build_structure(bx, by, bx + w - 1, by + h - 1, st,
                                     FLOOR_WOOD, FLOOR_WOOD,
                                     WALL_BRICK, WALL_BRICK_WIN, 55, 1);
        carve_rect_door(m.x0, m.y0, m.x1, m.y1, 2, (m.x0 + m.x1) / 2, WALL_DOOR_CLOSED);
        carve_rect_door(m.x0, m.y0, m.x1, m.y1, 0, (m.x0 + m.x1) / 2, WALL_DOOR_CLOSED);
        place_stairs_safe(m.x0, m.y0);
        if (st >= 2) {
            place_stairs_at(1, m.x1, m.y1);
            place_prop_if_clear(m.x0 + 1, m.y0 + 1, 1, OBJ_CRATE);
        }
        place_prop_if_clear(m.x1 - 1, m.y0 + 1, 0, OBJ_DINER_BOOTH);
        place_prop_if_clear(m.x1 - 1, m.y1 - 1, 0, OBJ_JUKEBOX);
    }

    /* Undeveloped PA Corn / Tall Brush Lots behind the strip */
    for (k = 0; k < 2; k++) {
        int cw = rrange(4, 6), ch = rrange(4, 6);
        if (find_random_lot(cw, ch, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
            place_corn_patch(bx, by, bx + cw - 1, by + ch - 1);
        }
    }

    /* Scatter parked Route 309 2-tile vehicles, service alley barrels & crates */
    {
        int i, n_veh = rrange(5, 6);
        int road_len = vert_hwy ? MAP_H : MAP_W;
        for (i = 0; i < n_veh; i++) {
            int vk = (i == 0 || i == 3) ? VEH_CRUISER :
                     (i == 1 || i == 4) ? VEH_SEDAN   : VEH_PICKUP;
            int tries;
            for (tries = 0; tries < 32; tries++) {
                int pos  = rrange(2, road_len - 4);
                int lane = hwy_pos + (rand() % 4);
                if (vert_hwy) {
                    if (place_vehicle_if_clear(lane, pos, 0, 1, vk)) break;
                } else {
                    if (place_vehicle_if_clear(pos, lane, 0, 0, vk)) break;
                }
            }
        }
        for (i = 0; i < 8; i++) {
            int rx = rrange(2, LOT_MAX_X), ry = rrange(2, LOT_MAX_Y);
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
    int lane_pos  = rrange(MAP_W / 2 - 3, MAP_W / 2 + 2);
    int lane_len  = vert_lane ? MAP_H : MAP_W;
    int bx, by, bw, bh, i;

    init_blank_map(FLOOR_GRASS);
    reserve_lz();

    /* Rural Farm Dirt/Gravel Lane + Perpendicular Farmyard Track */
    paint_side_road(vert_lane, lane_pos, 0, lane_len - 1, 2, FLOOR_SIDEWALK, 0);
    {
        int spur = rrange(8, lane_len - 9);
        if (rand() & 1) {
            paint_side_road(!vert_lane, spur, 0, lane_pos + 1, 2, FLOOR_SIDEWALK, 0);
        } else {
            paint_side_road(!vert_lane, spur, lane_pos,
                            (vert_lane ? MAP_W : MAP_H) - 1, 2, FLOOR_SIDEWALK, 0);
        }
    }

    place_procedural_ufo();

    /* 2-Story Red Pennsylvania Barn with Randomized Hayloft */
    bw = rrange(7, 8);
    bh = rrange(7, 8);
    if (find_random_lot(bw, bh, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
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
        {
            int barn_ay = (bay_side == 0 || bay_side == 2) ? 1 : 0;
            int placed_barn_tr = 0, pass_ay, dy_off, dx_off;
            for (pass_ay = 0; pass_ay < 2 && !placed_barn_tr; pass_ay++) {
                int ay = pass_ay ? (1 - barn_ay) : barn_ay;
                for (dy_off = -1; dy_off <= 1 && !placed_barn_tr; dy_off++) {
                    for (dx_off = -1; dx_off <= 1 && !placed_barn_tr; dx_off++) {
                        int tx = mx + dx_off, ty = my + dy_off;
                        if (tx > barn.x0 && (tx + (ay ? 0 : 1)) < barn.x1 &&
                            ty > barn.y0 && (ty + (ay ? 1 : 0)) < barn.y1) {
                            placed_barn_tr = place_vehicle_if_clear(tx, ty, 0, ay, VEH_TRACTOR);
                        }
                    }
                }
            }
        }
        place_prop_if_clear(barn.x0 + 1, barn.y0 + 1, 0, OBJ_CRATE);
        place_prop_if_clear(barn.x1 - 1, barn.y1 - 1, 0, OBJ_BARREL);
        place_prop_if_clear(loft_style ? barn.x1 - 1 : barn.x0 + 1,
                            loft_style ? barn.y0 + 1 : barn.y1 - 1, 1, OBJ_CRATE);
    }

    /* 2-Story Brick Pennsylvania Farmhouse */
    if (find_random_lot(6, 6, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
        BldgRect fh = build_structure(bx, by, bx + 5, by + 5, 2,
                                      FLOOR_WOOD, FLOOR_WOOD,
                                      WALL_BRICK, WALL_BRICK_WIN, 55, 0);
        carve_rect_door(fh.x0, fh.y0, fh.x1, fh.y1, 2, fh.x0 + 2, WALL_DOOR_CLOSED);
        carve_rect_door(fh.x0, fh.y0, fh.x1, fh.y1, 0, fh.x0 + 3, WALL_DOOR_CLOSED);
        place_stairs_safe(fh.x0, fh.y0);
        place_stairs_at(1, fh.x0, fh.y1);
        place_prop_if_clear(fh.x0 + 1, fh.y0 + 1, 0, OBJ_DINER_BOOTH);
        place_prop_if_clear(fh.x1 - 1, fh.y1 - 1, 0, OBJ_JUKEBOX);
        place_prop_if_clear(fh.x1 - 1, fh.y0 + 1, 1, OBJ_CRATE);
    }

    /* Wooden Equipment Shed / Feed Store */
    if (find_random_lot(5, 5, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
        BldgRect sh = build_structure(bx, by, bx + 4, by + 4, 1,
                                      FLOOR_WOOD, FLOOR_ROOF_TAR,
                                      WALL_BARN_WOOD, WALL_BRICK_WIN, 35, 0);
        carve_rect_door(sh.x0, sh.y0, sh.x1, sh.y1, 2, sh.x0 + 2, WALL_DOOR_OPEN);
        carve_rect_door(sh.x0, sh.y0, sh.x1, sh.y1, 0, sh.x0 + 2, WALL_DOOR_CLOSED);
        place_stairs_at(0, sh.x0, sh.y0);
        place_prop_if_clear(sh.x0 + 1, sh.y0 + 1, 0, OBJ_CRATE);
        place_prop_if_clear(sh.x1 - 1, sh.y1 - 1, 0, OBJ_BARREL);
    }

    /* 5 Randomized Fenced Pennsylvania Cornfields */
    for (i = 0; i < 5; i++) {
        int cw = rrange(5, 7), ch = rrange(5, 7);
        if (find_random_lot(cw, ch, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
            place_corn_patch(bx, by, bx + cw - 1, by + ch - 1);
        }
    }

    /* Outdoor 2-Tile Farm Tractors / Farm Pickup, Fuel Drums & Supply Crates */
    for (i = 0; i < 3; i++) {
        int vk = (i == 1) ? VEH_PICKUP : VEH_TRACTOR;
        int placed = 0, tries;
        if (i >= 1 && (rand() & 1)) {
            for (tries = 0; tries < 24 && !placed; tries++) {
                int pos  = rrange(2, lane_len - 4);
                int lane = lane_pos + (rand() & 1);
                placed = vert_lane ? place_vehicle_if_clear(lane, pos, 0, 1, vk)
                                   : place_vehicle_if_clear(pos, lane, 0, 0, vk);
            }
        }
        for (tries = 0; tries < 40 && !placed; tries++) {
            int ay = rand() & 1;
            int rx = rrange(2, ay ? LOT_MAX_X : (LOT_MAX_X - 1));
            int ry = rrange(2, ay ? (LOT_MAX_Y - 1) : LOT_MAX_Y);
            if (zone_map[ry][rx] == ZONE_FREE) {
                placed = place_vehicle_if_clear(rx, ry, 0, ay, vk);
            }
        }
    }
    for (i = 0; i < 8; i++) {
        int rx = rrange(2, LOT_MAX_X), ry = rrange(2, LOT_MAX_Y);
        if (zone_map[ry][rx] == ZONE_FREE) {
            uint8_t obj = (i < 4) ? OBJ_BARREL : (i < 7 ? OBJ_CRATE : OBJ_STREETLAMP);
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

    if (g_lz_side == 0)      { vert_st = 0; st_pos = MAP_H - 6; }
    else if (g_lz_side == 1) { vert_st = 0; st_pos = 3;         }
    else if (g_lz_side == 2) { vert_st = 1; st_pos = 3;         }
    else                     { vert_st = 1; st_pos = MAP_W - 6; }

    /* 55% chance main street cuts right through the middle of the neighborhood! */
    if ((rand() % 100) < 55) {
        vert_st = rand() & 1;
        st_pos  = rrange(MAP_W / 2 - 3, MAP_W / 2 + 1);
    }

    paint_road_strip(vert_st, st_pos, 3, 1, 0);
    /* Perpendicular suburban residential cross-street */
    {
        int cross_pos = rrange(9, (vert_st ? MAP_H : MAP_W) - 10);
        paint_side_road(!vert_st, cross_pos, 0, (vert_st ? MAP_W : MAP_H) - 1,
                        2, FLOOR_ASPHALT, 1);
    }
    place_procedural_ufo();

    /* House 1 & 2: Split-Wing Mega-Arcade Residence (50%) or Twin 2-Story Suburban Homes! */
    if ((rand() & 1) && find_random_lot(11, 6, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
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
        int w1 = rrange(6, 7), h1 = rrange(5, 6);
        if (find_lot_flex(&w1, &h1, 2, LOT_MAX_X, 2, LOT_MAX_Y, &bx, &by)) {
            BldgRect a = build_structure(bx, by, bx + w1 - 1, by + h1 - 1, 2,
                                         FLOOR_ARCADE_CARPET, FLOOR_WOOD,
                                         WALL_BRICK, WALL_BRICK_WIN, 60, 0);
            furnish_arcade(a, 2);
            carve_rect_door(a.x0, a.y0, a.x1, a.y1, 0, (a.x0 + a.x1) / 2, WALL_DOOR_CLOSED);
        }
    }

    /* Neighbor's 2-Story Brick Colonial Home */
    {
        int w2 = rrange(5, 6), h2 = rrange(5, 6);
        if (find_random_lot(w2, h2, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
            BldgRect n = build_structure(bx, by, bx + w2 - 1, by + h2 - 1, 2,
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

    /* Corner Suburban Ranch House */
    if (find_random_lot(6, 5, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
        BldgRect r = build_structure(bx, by, bx + 5, by + 4, 1,
                                     FLOOR_WOOD, FLOOR_ROOF_TAR,
                                     WALL_BRICK, WALL_BRICK_WIN, 60, 1);
        carve_rect_door(r.x0, r.y0, r.x1, r.y1, 2, r.x0 + 2, WALL_DOOR_CLOSED);
        carve_rect_door(r.x0, r.y0, r.x1, r.y1, 1, r.y0 + 2, WALL_DOOR_CLOSED);
        place_stairs_safe(r.x0, r.y0);
        place_prop_if_clear(r.x0 + 1, r.y0 + 3, 0, OBJ_DINER_BOOTH);
        place_prop_if_clear(r.x1 - 1, r.y0 + 1, 0, OBJ_JUKEBOX);
    }

    /* Detached Neighborhood Garage / Workshop */
    if (find_random_lot(5, 4, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
        BldgRect g = build_structure(bx, by, bx + 4, by + 3, 1,
                                     FLOOR_SIDEWALK, FLOOR_ROOF_TAR,
                                     WALL_BRICK, WALL_BRICK_WIN, 30, 0);
        carve_rect_door(g.x0, g.y0, g.x1, g.y1, 2, g.x0 + 2, WALL_DOOR_OPEN);
        carve_rect_door(g.x0, g.y0, g.x1, g.y1, 0, g.x0 + 2, WALL_DOOR_CLOSED);
        place_stairs_at(0, g.x0, g.y0);
        place_prop_if_clear(g.x1 - 1, g.y1 - 1, 0, OBJ_BARREL);
        place_prop_if_clear(g.x0 + 1, g.y0 + 1, 0, OBJ_CRATE);
    }

    /* Parked 2-Tile Street Vehicles (placed after UFO & doorways so paths stay clear) */
    {
        int st_len = vert_st ? MAP_H : MAP_W;
        for (i = 0; i < 5; i++) {
            int vk = (i == 0 || i == 3) ? VEH_SEDAN : ((i == 1) ? VEH_CRUISER : VEH_PICKUP);
            int tries;
            for (tries = 0; tries < 32; tries++) {
                int pos  = rrange(2, st_len - 4);
                int lane = st_pos + (rand() % 3);
                if (vert_st) {
                    if (place_vehicle_if_clear(lane, pos, 0, 1, vk)) break;
                } else {
                    if (place_vehicle_if_clear(pos, lane, 0, 0, vk)) break;
                }
            }
        }
    }

    /* Backyard Hedges, Fenced Garden Plots & Patio Props */
    for (i = 0; i < 3; i++) {
        int gw = rrange(4, 5), gh = rrange(4, 5);
        if (find_random_lot(gw, gh, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
            place_corn_patch(bx, by, bx + gw - 1, by + gh - 1);
        }
    }
    for (i = 0; i < 7; i++) {
        int rx = rrange(2, LOT_MAX_X), ry = rrange(2, LOT_MAX_Y);
        if (zone_map[ry][rx] == ZONE_FREE) {
            place_prop_if_clear(rx, ry, 0, (i < 2) ? OBJ_DINER_BOOTH : ((i & 1) ? OBJ_CRATE : OBJ_BARREL));
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
    BldgRect towers[5];
    int num_towers = 0;
    int bx, by, i, j, s;

    init_blank_map(FLOOR_ASPHALT);
    reserve_lz();

    /* Random scorched blast-furnace slag zones across ground floor */
    for (s = 0; s < 9; s++) {
        int sx = rrange(2, MAP_W - 5), sy = rrange(2, MAP_H - 5);
        int dy, dx;
        for (dy = 0; dy < 3; dy++) {
            for (dx = 0; dx < 3; dx++) {
                g_map[0][sy + dy][sx + dx].floor_id = (s & 1) ? FLOOR_SCORCHED : FLOOR_SIDEWALK;
            }
        }
    }

    place_procedural_ufo();

    /* Build 4 to 5 Steel Blast-Furnace Towers & 2-Story Central Foundry */
    memset(towers, 0, sizeof(towers));
    for (i = 0; i < 5; i++) {
        int tw = rrange(5, 7), th = rrange(5, 7);
        int stories = (i == 0) ? 2 : 1;
        if (find_random_lot(tw, th, 2, LOT_MAX_X, 2, LOT_MAX_Y, 0, &bx, &by)) {
            BldgRect t = build_structure(bx, by, bx + tw - 1, by + th - 1, stories,
                                         FLOOR_SIDEWALK, FLOOR_SIDEWALK,
                                         WALL_STEEL, WALL_BRICK_WIN, 22, 1);
            int mx = (t.x0 + t.x1) / 2;
            int my = (t.y0 + t.y1) / 2;
            carve_rect_door(t.x0, t.y0, t.x1, t.y1, 0, mx, WALL_DOOR_OPEN);
            carve_rect_door(t.x0, t.y0, t.x1, t.y1, 2, mx, WALL_DOOR_CLOSED);
            carve_rect_door(t.x0, t.y0, t.x1, t.y1, (i & 1) ? 1 : 3, my, WALL_DOOR_OPEN);
            place_stairs_safe((i & 1) ? t.x1 : t.x0, (i & 1) ? t.y1 : t.y0);
            if (stories >= 2) {
                place_stairs_at(1, t.x0, t.y1);
                place_prop_if_clear(t.x0 + 1, t.y0 + 1, 1, OBJ_CRATE);
            }
            place_prop_if_clear(mx, my, 0, (i & 1) ? OBJ_CRATE : OBJ_BARREL);
            towers[num_towers++] = t;
        }
    }

    /* Connect aligned Furnace Towers with Elevated Level-1 Steel Catwalks! */
    for (i = 0; i < num_towers; i++) {
        for (j = i + 1; j < num_towers; j++) {
            build_catwalk_between(towers[i], towers[j]);
        }
    }

    /* Scatter 2-Tile Industrial Yard Vehicles, Explosive Barrel Clusters, Crates & Floodlights */
    for (i = 0; i < 4; i++) {
        int vk = (i == 1) ? VEH_CRUISER : VEH_PICKUP;
        int tries;
        for (tries = 0; tries < 40; tries++) {
            int ay = rand() & 1;
            int rx = rrange(2, ay ? LOT_MAX_X : (LOT_MAX_X - 1));
            int ry = rrange(2, ay ? (LOT_MAX_Y - 1) : LOT_MAX_Y);
            if (zone_map[ry][rx] == ZONE_FREE) {
                if (place_vehicle_if_clear(rx, ry, 0, ay, vk)) break;
            }
        }
    }
    for (i = 0; i < 18; i++) {
        int rx = rrange(2, LOT_MAX_X), ry = rrange(2, LOT_MAX_Y);
        if (zone_map[ry][rx] == ZONE_FREE) {
            uint8_t obj = (i < 9) ? OBJ_BARREL : (i < 14) ? OBJ_CRATE : OBJ_STREETLAMP;
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
    return (obj_id != OBJ_NONE && !isomap_is_lift_obj(obj_id) && obj_id != OBJ_RUBBLE);
}

static int is_protected_ufo_prop(uint8_t obj_id) {
    return (isomap_is_ufo_corner_obj(obj_id) ||
            obj_id == OBJ_UFO_CORE     ||
            obj_id == OBJ_UFO_NAV      ||
            obj_id == OBJ_UFO_COMPUTER ||
            obj_id == OBJ_UFO_CRYO     ||
            obj_id == OBJ_UFO_POD      ||
            obj_id == OBJ_UFO_GRAVLIFT);
}

static void open_edge_between(int z, int x0, int y0, int x1, int y1) {
    uint8_t *wptr = NULL;
    if (x1 == x0 + 1)      wptr = &g_map[z][y0][x1].wall_west;
    else if (x1 == x0 - 1) wptr = &g_map[z][y0][x0].wall_west;
    else if (y1 == y0 + 1) wptr = &g_map[z][y1][x0].wall_north;
    else if (y1 == y0 - 1) wptr = &g_map[z][y0][x0].wall_north;

    if (wptr && isomap_wall_blocks_move(*wptr)) {
        if (*wptr == WALL_FENCE || *wptr == WALL_FENCE_DMG) {
            *wptr = WALL_NONE;
        } else if (*wptr == WALL_UFO_HULL || *wptr == WALL_UFO_HULL_DMG || *wptr == WALL_UFO_DOOR_CLOSED) {
            *wptr = WALL_UFO_DOOR_OPEN;
        } else {
            *wptr = WALL_DOOR_OPEN;
        }
    }
}

/* Guarantee every floor tile across ALL levels (z = 0, 1, 2) has a valid walkable path from the LZ! */
static void ensure_all_floors_reachable(int lz_cx, int lz_cy) {
    static const int odx[4] = { 0,  1,  0, -1 };
    static const int ody[4] = {-1,  0,  1,  0 };
    int i, pass;

    if (is_blocking_prop(g_map[0][lz_cy][lz_cx].obj_id)) {
        isomap_clear_prop_at(0, lz_cy, lz_cx, OBJ_NONE, 0);
    }
    compute_reachability(lz_cx, lz_cy);

    /* 1. First verify every ground-floor building & UFO interior has an open path */
    for (i = -1; i < g_num_bldgs; i++) {
        BldgRect r = (i < 0) ? g_ufo_rect : g_bldgs[i];
        uint8_t dwall = (i < 0) ? WALL_UFO_DOOR_OPEN : WALL_DOOR_OPEN;
        int x, y, any_reach = 0;
        if (!r.valid) continue;
        for (y = r.y0; y <= r.y1 && !any_reach; y++) {
            for (x = r.x0; x <= r.x1; x++) {
                if (reach_mask[0][y][x]) { any_reach = 1; break; }
            }
        }
        if (!any_reach) {
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 0, (r.x0 + r.x1) / 2, dwall);
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 2, (r.x0 + r.x1) / 2, dwall);
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 3, (r.y0 + r.y1) / 2, dwall);
            carve_rect_door(r.x0, r.y0, r.x1, r.y1, 1, (r.y0 + r.y1) / 2, dwall);
            compute_reachability(lz_cx, lz_cy);
        }
    }

    /* 2. Audit every floor tile on z = 0, 1, 2; repair isolated pockets & missing upper-level stairs! */
    for (pass = 0; pass < 96; pass++) {
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
            static uint8_t in_comp[MAP_H][MAP_W];
            static int16_t cx_q[MAP_W * MAP_H], cy_q[MAP_W * MAP_H];
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
                if (!is_blocking_prop(pobj) || is_protected_ufo_prop(pobj)) continue;
                for (d = 0; d < 4; d++) {
                    int rx = px + odx[d], ry = py + ody[d];
                    if ((unsigned)rx >= MAP_W || (unsigned)ry >= MAP_H) continue;
                    if (reach_mask[uz][ry][rx]) {
                        isomap_clear_prop_at(uz, py, px, OBJ_NONE, 0);
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
                    if (isomap_is_lift_obj(low_obj) || is_protected_ufo_prop(low_obj)) continue;
                    if (isomap_is_lift_obj(up_obj)  || is_protected_ufo_prop(up_obj))  continue;
                    if (uz - 1 > 0 && isomap_is_lift_obj(g_map[uz - 2][sy][sx].obj_id)) continue;
                    if (uz == 1 && (zone_map[sy][sx] == ZONE_LZ || zone_map[sy][sx] == ZONE_UFO)) continue;
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
                        if (uz - 1 > 0 && isomap_is_lift_obj(g_map[uz - 2][ay][ax].obj_id)) continue;
                        if (uz == 1 && (zone_map[ay][ax] == ZONE_LZ || zone_map[ay][ax] == ZONE_UFO)) continue;
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
                        !is_protected_ufo_prop(g_map[uz][step_y][step_x].obj_id)) {
                        isomap_clear_prop_at(uz, step_y, step_x, OBJ_NONE, 0);
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
    uint8_t fid;
    if ((unsigned)x >= MAP_W || (unsigned)y >= MAP_H || (unsigned)z >= MAP_LEVS) return 0;
    fid = g_map[z][y][x].floor_id;
    if (fid == FLOOR_NONE) return 0;
    if (fid >= FLOOR_UFO_ROOF_N && fid <= FLOOR_UFO_ROOF_W) return 0;
    if (g_map[z][y][x].obj_id != OBJ_NONE && g_map[z][y][x].obj_id != OBJ_RUBBLE) return 0;
    if (z > 0 && isomap_is_lift_obj(g_map[z - 1][y][x].obj_id)) return 0;
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

static int min_other_alien_dist(int x, int y, const ProcSpawnPlan *plan, int placed_aliens) {
    int i, best = 999;
    for (i = 0; i < placed_aliens; i++) {
        int d = abs(x - plan->alien_x[i]) + abs(y - plan->alien_y[i]);
        if (d < best) best = d;
    }
    return best;
}

static int any_squad_has_los(int x, int y, int z, const ProcSpawnPlan *plan) {
    int i;
    for (i = 0; i < 5; i++) {
        int d = abs(x - plan->squad_x[i]) + abs(y - plan->squad_y[i]);
        if (d <= 18 && isomap_has_los(plan->squad_x[i], plan->squad_y[i], 0, x, y, z)) {
            return 1;
        }
    }
    return 0;
}

/* Construct the hastily-armored Resistance Response Bus with exit ramp inside ZONE_LZ */
static void place_armored_response_vehicle(void) {
    int x, y;
    for (y = g_lz_y0; y <= g_lz_y1; y++) {
        for (x = g_lz_x0; x <= g_lz_x1; x++) {
            isomap_clear_prop_at(0, y, x, OBJ_NONE, 0);
            g_map[0][y][x].wall_west  = WALL_NONE;
            g_map[0][y][x].wall_north = WALL_NONE;
            g_map[0][y][x].floor_id   = FLOOR_WOOD;     /* Welded floorboards inside bus */
            g_map[1][y][x].floor_id   = FLOOR_ROOF_TAR; /* Welded sheet-metal roof */
        }
    }
    for (x = g_lz_x0; x <= g_lz_x1; x++) g_map[0][g_lz_y1 + 1][x].wall_north = WALL_NONE;
    for (y = g_lz_y0; y <= g_lz_y1; y++) g_map[0][y][g_lz_x1 + 1].wall_west  = WALL_NONE;

    /* Enclose bus hull in welded steel plates, leaving the exit ramp side open! */
    for (x = g_lz_x0; x <= g_lz_x1; x++) {
        if (g_lz_side != 0) {
            g_map[0][g_lz_y0][x].wall_north = (x == (g_lz_x0 + g_lz_x1) / 2) ? WALL_STEEL_DMG : WALL_STEEL;
        } else {
            g_map[0][g_lz_y0][x].floor_id   = FLOOR_ASPHALT; /* Exit ramp lip */
            g_map[1][g_lz_y0][x].floor_id   = FLOOR_NONE;
        }
        if (g_lz_side != 1) {
            g_map[0][g_lz_y1 + 1][x].wall_north = (x == (g_lz_x0 + g_lz_x1) / 2) ? WALL_STEEL_DMG : WALL_STEEL;
        } else {
            g_map[0][g_lz_y1][x].floor_id   = FLOOR_ASPHALT;
            g_map[1][g_lz_y1][x].floor_id   = FLOOR_NONE;
        }
    }
    for (y = g_lz_y0; y <= g_lz_y1; y++) {
        if (g_lz_side != 3) {
            g_map[0][y][g_lz_x0].wall_west = (y == (g_lz_y0 + g_lz_y1) / 2) ? WALL_STEEL_DMG : WALL_STEEL;
        } else {
            g_map[0][y][g_lz_x0].floor_id  = FLOOR_ASPHALT;
            g_map[1][y][g_lz_x0].floor_id  = FLOOR_NONE;
        }
        if (g_lz_side != 2) {
            g_map[0][y][g_lz_x1 + 1].wall_west = (y == (g_lz_y0 + g_lz_y1) / 2) ? WALL_STEEL_DMG : WALL_STEEL;
        } else {
            g_map[0][y][g_lz_x1].floor_id  = FLOOR_ASPHALT;
            g_map[1][y][g_lz_x1].floor_id  = FLOOR_NONE;
        }
    }
}

/* =========================================================================
 * BIOME 4: DUFF-COM COLONIAL HOUSE HOME BASE DEFENSE (EXACT REPLICA!)
 * ========================================================================= */
static void gen_biome_base_defense(void) {
    int r, c, q_r = 1, q_c = 1;

    init_blank_map(FLOOR_GRASS);

    /* Find Living Quarters (or Main Colonial House at [1][1]) for Squad Starting Location */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            if (g_camp.bunker_grid[r][c] == FAC_QUARTERS) {
                q_r = r;
                q_c = c;
            }
        }
    }
    g_lz_side = 1;
    g_lz_x0   = 5 + q_c * 6;
    g_lz_x1   = g_lz_x0 + 4;
    g_lz_y0   = 7 + q_r * 6 + 1;
    g_lz_y1   = g_lz_y0 + 2;

    /* Coopersburg Front Street & Perimeter Yard */
    paint_road_strip(0, 27, 3, 1, 2);

    /* Construct Exact Tactical Replica of the Player's 3x4 Colonial House Base Grid! */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            int fid = g_camp.bunker_grid[r][c];
            int bx0 = 5 + c * 6, by0 = 7 + r * 6;
            int bx1 = bx0 + 5,   by1 = by0 + 5;
            int mx  = (bx0 + bx1) / 2, my = (by0 + by1) / 2;

            if (fid == FAC_EMPTY_LOT) {
                if ((r + c) & 1) place_prop_if_clear(mx, my, 0, OBJ_CRATE);
                continue;
            }

            if (fid == FAC_MAIN_HOUSE || fid == FAC_QUARTERS) {
                build_structure(bx0, by0, bx1, by1, 2,
                                FLOOR_WOOD, FLOOR_WOOD,
                                WALL_BRICK, WALL_BRICK_WIN, 35, 1);
                place_stairs_safe(bx0, by0);
                if (fid == FAC_MAIN_HOUSE) {
                    place_prop_if_clear(bx1 - 1, by0 + 1, 0, OBJ_ARCADE_CAB);
                    place_prop_if_clear(bx1 - 1, by1 - 1, 0, OBJ_JUKEBOX);
                } else {
                    place_prop_if_clear(bx1 - 1, by0 + 1, 0, OBJ_DINER_BOOTH);
                }
            } else if (fid == FAC_GARAGE) {
                build_structure(bx0, by0, bx1, by1, 1,
                                FLOOR_ASPHALT, FLOOR_ROOF_TAR,
                                WALL_STEEL, WALL_STEEL, 0, 1);
                place_vehicle_if_clear(bx0 + 1, by0 + 2, 0, 0, VEH_PICKUP);
                place_prop_if_clear(bx1 - 1, by0 + 1, 0, OBJ_CRATE);
            } else if (fid == FAC_RADAR || fid == FAC_MISSILE_DEF || fid == FAC_OFFENSE_SYS) {
                int x, y;
                for (y = by0; y <= by1; y++)
                    for (x = bx0; x <= bx1; x++)
                        g_map[0][y][x].floor_id = FLOOR_SIDEWALK;
                for (x = bx0; x <= bx1; x++) {
                    if (x != mx) g_map[0][by0][x].wall_north = WALL_FENCE;
                }
                for (y = by0; y <= by1; y++) {
                    if (y != my) g_map[0][y][bx0].wall_west = WALL_FENCE;
                }
                place_prop_if_clear(mx, my, 0,

                                    (fid == FAC_RADAR) ? OBJ_UFO_NAV :
                                    (fid == FAC_OFFENSE_SYS ? OBJ_UFO_CORE : OBJ_UFO_POD));
                continue;
            } else if (fid == FAC_LAB || fid == FAC_UPG_LAB || fid == FAC_CONTAINMENT) {
                build_structure(bx0, by0, bx1, by1, 1,
                                (fid == FAC_CONTAINMENT) ? FLOOR_UFO_DECK : FLOOR_DINER_TILE,
                                FLOOR_ROOF_TAR,
                                WALL_STEEL, WALL_DINER_GLASS, 35, 1);
                place_prop_if_clear(bx0 + 1, by0 + 1, 0, OBJ_UFO_COMPUTER);
                place_prop_if_clear(bx1 - 1, by0 + 1, 0, OBJ_UFO_CRYO);
            } else {
                /* FAC_STORES or FAC_UPG_WORKSHOP */
                build_structure(bx0, by0, bx1, by1, 1,
                                FLOOR_SIDEWALK, FLOOR_ROOF_TAR,
                                WALL_STEEL, WALL_BRICK_WIN, 20, 1);
                place_prop_if_clear(bx0 + 1, by0 + 1, 0, OBJ_CRATE);
                place_prop_if_clear(bx1 - 1, by0 + 1, 0,
                                    (fid == FAC_UPG_WORKSHOP) ? OBJ_UFO_POD : OBJ_BARREL);
            }

            /* Carve doorways on all 4 sides so connected base facilities link together! */
            carve_rect_door(bx0, by0, bx1, by1, 0, mx, WALL_DOOR_OPEN);
            carve_rect_door(bx0, by0, bx1, by1, 2, mx, WALL_DOOR_OPEN);
            carve_rect_door(bx0, by0, bx1, by1, 1, my, WALL_DOOR_OPEN);
            carve_rect_door(bx0, by0, bx1, by1, 3, my, WALL_DOOR_OPEN);
        }
    }
}

static void populate_spawn_plan(int biome_id, ProcSpawnPlan *plan) {
    int cx = (g_lz_x0 + g_lz_x1) / 2;
    int cy = (g_lz_y0 + g_lz_y1) / 2;
    int ufo_class = (g_ufo_sz == 7) ? 1 : 0;
    int a_count;
    int a;

    if (biome_id != BIOME_BASE_DEFENSE) {
        place_armored_response_vehicle();
    }


    if (g_camp.launched_event >= 0 && g_camp.launched_event < MAX_RADAR_EVENTS &&
        g_camp.events[g_camp.launched_event].active) {
        ufo_class = g_camp.events[g_camp.launched_event].ufo_size;
    }

    /* Larger UFOs carry more aliens (Small Scout: 4..5, Medium Cruiser: 6..7, Large Ship: 8) */
    if (ufo_class == 0)      a_count = 4 + (rand() & 1);
    else if (ufo_class == 1) a_count = 6 + (rand() & 1);
    else                     a_count = 8;

    memset(plan, 0, sizeof(*plan));
    plan->squad_dir = (g_lz_side == 0) ? 0 : (g_lz_side == 1) ? 4 : (g_lz_side == 2) ? 2 : 6;
    plan->bus_x0 = g_lz_x0; plan->bus_y0 = g_lz_y0;
    plan->bus_x1 = g_lz_x1; plan->bus_y1 = g_lz_y1;


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
        int ax = MAP_W / 2, ay = MAP_H / 2, az = 0;

        /* Role-based + Biome-weighted randomized alien species */
        if (a == 0) {
            utype = (biome_id == BIOME_STEEL || (rand() & 1)) ? 3 : 1; /* UFO Commander: Muton or Sectoid */
            plan->alien_name[a] = (utype == 3) ? "MUTON COMMANDER" : "SECTOID LEADER";
        } else if (a == 1) {
            utype = 2; /* Rooftop / Hayloft / Catwalk Floater Sniper */
            plan->alien_name[a] = "FLOATER SNIPER";
        } else if (a == 2 || a == 3) {
            utype = (rand() % 100 < 55) ? 1 : 3; /* Building / Cornfield Lurker */
            plan->alien_name[a] = (utype == 3) ? "MUTON ENFORCER" : "SECTOID STALKER";
        } else if (a == 4 && g_ufo_sz == 7) {
            utype = (rand() % 100 < 45) ? 1 : (rand() & 1 ? 2 : 3); /* Cruiser Reactor/Stasis Crew */
            plan->alien_name[a] = (utype == 1) ? "SECTOID NAVIGATOR" :
                                  (utype == 2) ? "FLOATER TECH" : "MUTON GUARD";
        } else {
            int r = rand() % 100;
            utype = (r < 45) ? 1 : (r < 78) ? 2 : 3;
            plan->alien_name[a] = (utype == 1) ? "SECTOID SCOUT" :
                                  (utype == 2) ? "FLOATER PATROL" : "MUTON WARRIOR";
        }

        /* Role 0: Guarding UFO Navigation Area / Bridge */
        if (a == 0 && g_ufo_rect.valid) {
            for (tries = 0; tries < 40 && !found; tries++) {
                int rx = rrange(1, g_ufo_sz - 2);
                int ry = rrange(0, 1);
                int tx, ty;
                ufo_rot_xy(g_ufo_rect.x0, g_ufo_rect.y0, g_ufo_sz, g_ufo_door_side, rx, ry, &tx, &ty);
                if (tile_free_for_spawn(tx, ty, 0, plan, a)) {
                    ax = tx; ay = ty; az = 0; found = 1;
                }
            }
            for (tries = 0; tries < 50 && !found; tries++) {
                int tx = rrange(g_ufo_rect.x0, g_ufo_rect.x1);
                int ty = rrange(g_ufo_rect.y0, g_ufo_rect.y1);
                if (tile_free_for_spawn(tx, ty, 0, plan, a)) {
                    ax = tx; ay = ty; az = 0; found = 1;
                }
            }
        }
        /* Role 4 on 7x7 Cruiser: Guarding UFO Main Reactor & Stasis Deck */
        else if (a == 4 && g_ufo_sz == 7 && g_ufo_rect.valid) {
            for (tries = 0; tries < 50 && !found; tries++) {
                int rx = rrange(1, g_ufo_sz - 2);
                int ry = rrange(2, g_ufo_sz - 2);
                int tx, ty;
                ufo_rot_xy(g_ufo_rect.x0, g_ufo_rect.y0, g_ufo_sz, g_ufo_door_side, rx, ry, &tx, &ty);
                if (tile_free_for_spawn(tx, ty, 0, plan, a) && !any_squad_has_los(tx, ty, 0, plan)) {
                    ax = tx; ay = ty; az = 0; found = 1;
                }
            }
        }
        /* Role 1: Elevated z=1 Rooftop, Saucer Dome, Hayloft, or Catwalk Overwatch */
        else if (a == 1) {
            for (tries = 0; tries < 140 && !found; tries++) {
                int tx = rrange(2, LOT_MAX_X), ty = rrange(2, LOT_MAX_Y);
                if (tile_free_for_spawn(tx, ty, 1, plan, a) &&
                    min_squad_dist(tx, ty, plan) >= 16 &&
                    !any_squad_has_los(tx, ty, 1, plan)) {
                    ax = tx; ay = ty; az = 1; found = 1;
                }
            }
        }
        /* Roles 2 & 3: Inside Separate Diners / Arcades / Barns / Houses / Cornfields / Steel Towers */
        else if ((a == 2 || a == 3) && g_num_bldgs > 0) {
            for (tries = 0; tries < 100 && !found; tries++) {
                BldgRect b = g_bldgs[rand() % g_num_bldgs];
                int tx = rrange(b.x0, b.x1), ty = rrange(b.y0, b.y1);
                if (tile_free_for_spawn(tx, ty, 0, plan, a) &&
                    min_squad_dist(tx, ty, plan) >= 14 &&
                    (tries >= 70 || min_other_alien_dist(tx, ty, plan, a) >= 7) &&
                    !any_squad_has_los(tx, ty, 0, plan)) {
                    ax = tx; ay = ty; az = 0; found = 1;
                }
            }
        }

        /* General Flanking Patrol / Dispersed Sector Placement (no Turn-1 spawn-camp LOS!) */
        for (tries = 0; tries < 200 && !found; tries++) {
            int tx = rrange(1, MAP_W - 2), ty = rrange(1, MAP_H - 2);
            if (zone_map[ty][tx] == ZONE_LZ) continue;
            if (!tile_free_for_spawn(tx, ty, 0, plan, a)) continue;
            if (min_squad_dist(tx, ty, plan) < (tries < 130 ? 16 : 10)) continue;
            if (tries < 120 && min_other_alien_dist(tx, ty, plan, a) < 6) continue;
            if (tries < 160 && any_squad_has_los(tx, ty, 0, plan)) continue;
            ax = tx; ay = ty; az = 0; found = 1;
        }

        /* Deterministic safety scan so a unit is NEVER placed on a blocked tile */
        if (!found) {
            int tx, ty, best_d = -1;
            for (ty = 1; ty <= MAP_H - 2; ty++) {
                for (tx = 1; tx <= MAP_W - 2; tx++) {
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
        plan->alien_dir[a]  = calc_dir_vec((MAP_W / 2) - ax, (MAP_H / 2) - ay);
    }
}

void procgen_build_battlefield(int biome_id, ProcSpawnPlan *out_plan) {
    int b = biome_id % BIOME_COUNT;
    if (b == BIOME_RTE309)            gen_biome_rte309();
    else if (b == BIOME_PA_FARM)      gen_biome_pa_farm();
    else if (b == BIOME_SUBURB)       gen_biome_suburb();
    else if (b == BIOME_BASE_DEFENSE) gen_biome_base_defense();
    else                              gen_biome_steel();


    if (out_plan) {
        populate_spawn_plan(b, out_plan);
    } else {
        ensure_all_floors_reachable((g_lz_x0 + g_lz_x1) / 2, (g_lz_y0 + g_lz_y1) / 2);
    }
    isomap_init_wall_hp();
}
