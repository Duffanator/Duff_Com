#include "isomap.h"
#include "operative.h"
#include "procgen.h"
#include "vga.h"
#include "gfx_assets.h"
#include "input.h"
#include "sound.h"
#include "music.h"
#include "settings.h"
#include "hud.h"
#include "dos_compat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MapCell g_map[MAP_LEVS][MAP_H][MAP_W];
Unit g_units[MAX_UNITS];
BattleState g_battle;
PathResult g_preview_path;
PathResult g_walk_path;

static int last_prev_ux = -1, last_prev_uy = -1, last_prev_uz = -1;
static int last_prev_cx = -1, last_prev_cy = -1, last_prev_cz = -1;
static int combat_anim_depth = 0;

static const char *default_squad_names[5] = {
    "RKI. KELLER",
    "RKI. DECKER",
    "RKI. HARR",
    "RKI. LUCAS",
    "RKI. BARBER"
};

static const char *biome_titles[BIOME_COUNT] = {
    "COOPERSBURG PA - RTE 309",
    "PA FARMLAND & RED BARN",
    "SUBURB & ARCADE BASEMENT",
    "BETHLEHEM STEEL RUINS"
};

static void kill_unit(int victim_idx, int killer_idx);
static void detonate_explosion(int cx, int cy, int cz, int attacker_idx);
static void execute_raycast_shot(int shooter_idx, int tx, int ty, int tz, int acc);

void isomap_tile_to_screen(int tx, int ty, int tz, int *sx, int *sy) {
    *sx = g_battle.cam_x + (tx - ty) * (TILE_W / 2);
    *sy = g_battle.cam_y + (tx + ty) * (TILE_H / 2) - tz * LEVEL_H;
}

void isomap_screen_to_tile(int sx, int sy, int tz, int *tx, int *ty) {
    int rx = sx - g_battle.cam_x - (TILE_W / 2);
    int ry = sy - g_battle.cam_y + tz * LEVEL_H - (TILE_H / 2);
    int num_x = rx + 2 * ry + 16;
    int num_y = 2 * ry - rx + 16;
    *tx = (num_x >= 0) ? (num_x / 32) : ((num_x - 31) / 32);
    *ty = (num_y >= 0) ? (num_y / 32) : ((num_y - 31) / 32);
    if (*tx < 0) *tx = 0;
    if (*tx >= MAP_W) *tx = MAP_W - 1;
    if (*ty < 0) *ty = 0;
    if (*ty >= MAP_H) *ty = MAP_H - 1;
}

static void clamp_camera(void) {
    int min_cx = (SCREEN_W / 2) - (MAP_W - 1) * (TILE_W / 2) - 48;
    int max_cx = (SCREEN_W / 2) + (MAP_H - 1) * (TILE_W / 2) + 16;
    int min_cy = (VIEWPORT_H / 2) - (MAP_W + MAP_H - 2) * (TILE_H / 2) - 40;
    int max_cy = (VIEWPORT_H / 2) + (MAP_LEVS - 1) * LEVEL_H + 32;

    if (g_battle.cam_x < min_cx) g_battle.cam_x = min_cx;
    if (g_battle.cam_x > max_cx) g_battle.cam_x = max_cx;
    if (g_battle.cam_y < min_cy) g_battle.cam_y = min_cy;
    if (g_battle.cam_y > max_cy) g_battle.cam_y = max_cy;
}

void isomap_center_camera_on(int tx, int ty, int tz) {
    g_battle.cam_x = (SCREEN_W / 2) - (tx - ty) * (TILE_W / 2) - (TILE_W / 2);
    g_battle.cam_y = (VIEWPORT_H / 2) - (tx + ty) * (TILE_H / 2) + tz * LEVEL_H - (TILE_H / 2);
    clamp_camera();
}

static void set_status(const char *msg) {
    strncpy(g_battle.status_msg, msg, sizeof(g_battle.status_msg) - 1);
    g_battle.status_msg[sizeof(g_battle.status_msg) - 1] = '\0';
    g_battle.status_timer = 90;
}

static void invalidate_path_preview(void) {
    last_prev_ux = -1;
}

static void check_victory_state(void) {
    int i, z, y, x, aliens_alive = 0, squad_alive = 0;
    for (i = 0; i < MAX_UNITS; i++) {
        if (!g_units[i].active) continue;
        if (g_units[i].faction == 0) squad_alive++;
        else aliens_alive++;
    }
    if (aliens_alive == 0 && g_battle.mission_ended == 0) {
        int nav_cnt = 0, cryo_cnt = 0, pod_cnt = 0, core_cnt = 0, hull_cnt = 0, corpse_cnt = 0;
        for (z = 0; z < MAP_LEVS; z++) {
            for (y = 0; y < MAP_H; y++) {
                for (x = 0; x < MAP_W; x++) {
                    uint8_t obj = g_map[z][y][x].obj_id;
                    if (obj == OBJ_UFO_NAV || obj == OBJ_UFO_COMPUTER) nav_cnt++;
                    else if (obj == OBJ_UFO_CRYO) cryo_cnt++;
                    else if (obj == OBJ_UFO_POD)  pod_cnt++;
                    else if (obj == OBJ_UFO_CORE) core_cnt++;
                    if (g_map[z][y][x].wall_west == WALL_UFO_HULL)  hull_cnt++;
                    if (g_map[z][y][x].wall_north == WALL_UFO_HULL) hull_cnt++;
                }
            }
        }
        for (i = 0; i < MAX_UNITS; i++) {
            if (g_units[i].is_corpse && g_units[i].faction == 1) corpse_cnt++;
        }
        g_battle.mission_ended     = 1;
        g_battle.nav_recovered     = nav_cnt;
        g_battle.cryo_recovered    = cryo_cnt;
        g_battle.pods_recovered    = pod_cnt;
        g_battle.corpses_recovered = corpse_cnt;
        g_battle.elerium_recovered += (core_cnt > 0 ? 15 * core_cnt : 8) + pod_cnt * 4;
        g_battle.alloys_recovered  += (hull_cnt >= 14 ? 28 : 18) + pod_cnt * 5;
        game_wait_frames(16);
        set_status("SECTOR CLEARED! UFO & TECH SECURED!");
        sfx_beep(660, 80);
        sfx_beep(880, 120);
    } else if (squad_alive == 0 && g_battle.mission_ended == 0) {
        g_battle.mission_ended = 2;
        set_status("SQUAD WIPED OUT! MISSION FAILED!");
    }
}

void isomap_load_roster(void) {
    FILE *fp = fopen("ROSTER.CFG", "r");
    int i = 0;
    if (!fp) fp = fopen("assets/ROSTER.CFG", "r");
    if (!fp) return;
    while (i < 5 && !feof(fp)) {
        char line[64];
        if (fgets(line, sizeof(line), fp)) {
            char *p = line;
            char *nl = strpbrk(line, "\r\n");
            if (nl) *nl = '\0';
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\0') continue;

            /* Support optional "M: NAME" or "F: NAME" gender tag in ROSTER.CFG */
            if (((p[0] == 'M' || p[0] == 'm' || p[0] == 'F' || p[0] == 'f') &&
                 (p[1] == ':' || p[1] == ','))) {
                int want_fem = (p[0] == 'F' || p[0] == 'f');
                p += 2;
                while (*p == ' ' || *p == '\t') p++;
                g_units[i].gender = want_fem;
                if (want_fem && g_units[i].portrait_id < 6) {
                    g_units[i].portrait_id = 6 + (g_units[i].portrait_id % 6);
                } else if (!want_fem && g_units[i].portrait_id >= 6) {
                    g_units[i].portrait_id = g_units[i].portrait_id % 6;
                }
                g_units[i].sprite_id = gfx_portrait_to_unit_sprite(g_units[i].portrait_id);
            }

            {
                const char *clean = operative_strip_rank_prefix(p);
                snprintf(g_units[i].name, sizeof(g_units[i].name), "%s %s",
                         operative_rank_prefix(g_units[i].rank), clean);
                g_units[i].name[OP_FULLNAME_MAX] = '\0';
            }
            i++;
        }
    }
    fclose(fp);
}

void isomap_generate_biome(int biome_id) {
    procgen_build_battlefield(biome_id, NULL);
}

void isomap_generate_coopersburg_309(void) {
    isomap_generate_biome(BIOME_RTE309);
}

static void spawn_unit(int idx, int faction, int type, const char *name,
                       int x, int y, int z, int dir, int weapon_id, int portrait_id) {
    Unit *u = &g_units[idx];
    memset(u, 0, sizeof(*u));
    u->active = 1;
    u->faction = faction;
    u->unit_type = type;
    strncpy(u->name, name, sizeof(u->name) - 1);
    u->portrait_id = portrait_id % NUM_PORTRAITS;
    u->gender = (u->portrait_id >= 6) ? 1 : 0;
    if (faction == 0) {
        u->sprite_id = gfx_portrait_to_unit_sprite(u->portrait_id);
    } else {
        u->sprite_id = (type == 1) ? UNIT_SPR_SECTOID :
                       (type == 2) ? UNIT_SPR_FLOATER : UNIT_SPR_MUTON;
    }
    u->weapon_id = weapon_id;
    u->ammo = (weapon_id == 1) ? 99 : 30;
    u->grenades = (faction == 0) ? 2 : 1;
    u->flares   = (faction == 0) ? 2 : 0;
    u->x = x; u->y = y; u->z = z;
    u->dir = dir;
    u->stance = 0;
    u->anim_pose = 0;
    u->rank = RANK_ROOKIE;
    u->xp_start = 0;
    u->xp_gained = 0;
    u->tu_max = (faction == 0) ? 53 : (type == 3 ? 62 : 54);
    u->tu = u->tu_max;
    u->hp_max = (faction == 0) ? 41 : (type == 3 ? 65 : (type == 2 ? 45 : 32));
    u->hp = u->hp_max;
    u->energy_max = (faction == 0) ? 56 : 60;
    u->energy = u->energy_max;
    u->morale = 100;
    u->firing_acc = (faction == 0) ? 57 : (type == 3 ? 65 : 52);
    u->reactions  = (faction == 0) ? 46 : (type == 3 ? 55 : 42);
    g_map[z][y][x].unit_idx = (int8_t)idx;
}

int isomap_is_door_closed(uint8_t wall_id) {
    return (wall_id == WALL_DOOR_CLOSED || wall_id == WALL_UFO_DOOR_CLOSED);
}

int isomap_is_door_open(uint8_t wall_id) {
    return (wall_id == WALL_DOOR_OPEN || wall_id == WALL_UFO_DOOR_OPEN);
}

int isomap_is_lift_obj(uint8_t obj_id) {
    return (obj_id == OBJ_STAIRS_UP || obj_id == OBJ_UFO_GRAVLIFT);
}

int isomap_is_ufo_corner_obj(uint8_t obj_id) {
    return (obj_id >= OBJ_UFO_CORNER_N && obj_id <= OBJ_UFO_CORNER_W);
}

uint8_t isomap_wall_max_hp(uint8_t wall_id) {
    switch (wall_id) {
        case WALL_NONE:             return 0;
        case WALL_FENCE:
        case WALL_FENCE_DMG:        return 20;
        case WALL_DOOR_CLOSED:
        case WALL_DOOR_OPEN:        return 38;
        case WALL_BARN_WOOD:
        case WALL_BARN_DMG:         return 45;
        case WALL_BRICK_WIN:
        case WALL_BRICK_BROKEN:     return 50;
        case WALL_DINER_GLASS:
        case WALL_DINER_BROKEN:     return 60;
        case WALL_UFO_DOOR_CLOSED:
        case WALL_UFO_DOOR_OPEN:    return 65;
        case WALL_BRICK:
        case WALL_BRICK_DMG:        return 70;
        case WALL_DINER_CHROME:
        case WALL_DINER_DMG:        return 85;
        case WALL_STEEL:
        case WALL_STEEL_DMG:        return 130;
        case WALL_UFO_HULL:
        case WALL_UFO_HULL_DMG:     return 160;
        default:                    return 50;
    }
}

int isomap_wall_blocks_move(uint8_t wall_id) {
    return (wall_id != WALL_NONE && !isomap_is_door_open(wall_id));
}

int isomap_wall_blocks_los(uint8_t wall_id) {
    switch (wall_id) {
        case WALL_BRICK:
        case WALL_BRICK_DMG:
        case WALL_DINER_CHROME:
        case WALL_DINER_DMG:
        case WALL_BARN_WOOD:
        case WALL_BARN_DMG:
        case WALL_STEEL:
        case WALL_STEEL_DMG:
        case WALL_DOOR_CLOSED:
        case WALL_UFO_HULL:
        case WALL_UFO_HULL_DMG:
        case WALL_UFO_DOOR_CLOSED:
            return 1;
        default:
            return 0;
    }
}

const char *isomap_wall_name(uint8_t wall_id) {
    switch (wall_id) {
        case WALL_FENCE:
        case WALL_FENCE_DMG:        return "FENCE";
        case WALL_DOOR_CLOSED:
        case WALL_DOOR_OPEN:        return "DOOR";
        case WALL_UFO_DOOR_CLOSED:
        case WALL_UFO_DOOR_OPEN:    return "AIRLOCK";
        case WALL_BARN_WOOD:
        case WALL_BARN_DMG:         return "WOOD";
        case WALL_BRICK_WIN:
        case WALL_DINER_GLASS:      return "WINDOW";
        case WALL_BRICK_BROKEN:
        case WALL_DINER_BROKEN:     return "FRAME";
        case WALL_BRICK:
        case WALL_BRICK_DMG:        return "BRICK";
        case WALL_DINER_CHROME:
        case WALL_DINER_DMG:        return "CHROME";
        case WALL_STEEL:
        case WALL_STEEL_DMG:        return "STEEL";
        case WALL_UFO_HULL:
        case WALL_UFO_HULL_DMG:     return "UFO HULL";
        default:                    return "WALL";
    }
}

void isomap_init_wall_hp(void) {
    int z, y, x;
    for (z = 0; z < MAP_LEVS; z++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                g_map[z][y][x].wall_west_hp  = isomap_wall_max_hp(g_map[z][y][x].wall_west);
                g_map[z][y][x].wall_north_hp = isomap_wall_max_hp(g_map[z][y][x].wall_north);
            }
        }
    }
}

int isomap_is_vehicle_obj(uint8_t obj_id) {
    if (obj_id == OBJ_CAR_HOOD || obj_id == OBJ_TRACTOR) return 1;
    if (obj_id >= OBJ_CAR_FRONT_X && obj_id <= OBJ_PICKUP_FRONT_Y) return 1;
    return 0;
}

int isomap_get_vehicle_partner(int x, int y, int z, int *out_px, int *out_py) {
    uint8_t obj, want = OBJ_NONE;
    int dx = 0, dy = 0, px, py;

    if ((unsigned)x >= MAP_W || (unsigned)y >= MAP_H || (unsigned)z >= MAP_LEVS) return 0;
    obj = g_map[z][y][x].obj_id;

    switch (obj) {
        case OBJ_CAR_BACK_X:     dx =  1; dy =  0; want = OBJ_CAR_FRONT_X;     break;
        case OBJ_CAR_FRONT_X:    dx = -1; dy =  0; want = OBJ_CAR_BACK_X;      break;
        case OBJ_CAR_BACK_Y:     dx =  0; dy =  1; want = OBJ_CAR_FRONT_Y;     break;
        case OBJ_CAR_FRONT_Y:    dx =  0; dy = -1; want = OBJ_CAR_BACK_Y;      break;
        case OBJ_TRACTOR_BACK_X: dx =  1; dy =  0; want = OBJ_TRACTOR_FRONT_X; break;
        case OBJ_TRACTOR_FRONT_X:dx = -1; dy =  0; want = OBJ_TRACTOR_BACK_X;  break;
        case OBJ_TRACTOR_BACK_Y: dx =  0; dy =  1; want = OBJ_TRACTOR_FRONT_Y; break;
        case OBJ_TRACTOR_FRONT_Y:dx =  0; dy = -1; want = OBJ_TRACTOR_BACK_Y;  break;
        case OBJ_SEDAN_BACK_X:   dx =  1; dy =  0; want = OBJ_SEDAN_FRONT_X;   break;
        case OBJ_SEDAN_FRONT_X:  dx = -1; dy =  0; want = OBJ_SEDAN_BACK_X;    break;
        case OBJ_SEDAN_BACK_Y:   dx =  0; dy =  1; want = OBJ_SEDAN_FRONT_Y;   break;
        case OBJ_SEDAN_FRONT_Y:  dx =  0; dy = -1; want = OBJ_SEDAN_BACK_Y;    break;
        case OBJ_PICKUP_BACK_X:  dx =  1; dy =  0; want = OBJ_PICKUP_FRONT_X;  break;
        case OBJ_PICKUP_FRONT_X: dx = -1; dy =  0; want = OBJ_PICKUP_BACK_X;   break;
        case OBJ_PICKUP_BACK_Y:  dx =  0; dy =  1; want = OBJ_PICKUP_FRONT_Y;  break;
        case OBJ_PICKUP_FRONT_Y: dx =  0; dy = -1; want = OBJ_PICKUP_BACK_Y;   break;
        default: return 0;
    }

    px = x + dx;
    py = y + dy;
    if ((unsigned)px >= MAP_W || (unsigned)py >= MAP_H) return 0;
    if (g_map[z][py][px].obj_id != want) return 0;
    if (out_px) *out_px = px;
    if (out_py) *out_py = py;
    return 1;
}

void isomap_clear_prop_at(int z, int y, int x, uint8_t new_obj, uint8_t min_smoke) {
    int px = 0, py = 0;
    int has_partner;

    if ((unsigned)x >= MAP_W || (unsigned)y >= MAP_H || (unsigned)z >= MAP_LEVS) return;
    has_partner = isomap_get_vehicle_partner(x, y, z, &px, &py);

    g_map[z][y][x].obj_id = new_obj;
    if (min_smoke > 0 && g_map[z][y][x].smoke < min_smoke) {
        g_map[z][y][x].smoke = min_smoke;
    }
    if (new_obj == OBJ_RUBBLE && g_map[z][y][x].floor_id == FLOOR_GRASS) {
        g_map[z][y][x].floor_id = FLOOR_SCORCHED;
    }

    if (has_partner) {
        g_map[z][py][px].obj_id = new_obj;
        if (min_smoke > 0 && g_map[z][py][px].smoke < min_smoke) {
            g_map[z][py][px].smoke = min_smoke;
        }
        if (new_obj == OBJ_RUBBLE && g_map[z][py][px].floor_id == FLOOR_GRASS) {
            g_map[z][py][px].floor_id = FLOOR_SCORCHED;
        }
    }
}

/* Apply structural damage to a West (is_north=0) or North (is_north=1) wall edge; returns 2=destroyed, 1=damaged, 0=none */
static int damage_wall_edge(int z, int wy, int wx, int is_north, int dmg, int silent, int *out_glass_shattered) {
    uint8_t *wptr, *hp_ptr;
    const char *mname;
    int max_hp, rem;
    char msg[48];

    if (out_glass_shattered) *out_glass_shattered = 0;
    if ((unsigned)wx >= MAP_W || (unsigned)wy >= MAP_H || (unsigned)z >= MAP_LEVS) return 0;

    wptr   = is_north ? &g_map[z][wy][wx].wall_north    : &g_map[z][wy][wx].wall_west;
    hp_ptr = is_north ? &g_map[z][wy][wx].wall_north_hp : &g_map[z][wy][wx].wall_west_hp;
    if (*wptr == WALL_NONE) return 0;

    mname  = isomap_wall_name(*wptr);
    max_hp = isomap_wall_max_hp(*wptr);
    if (*hp_ptr == 0) *hp_ptr = (uint8_t)max_hp;

    if (*wptr == WALL_BRICK_WIN || *wptr == WALL_DINER_GLASS) {
        if (out_glass_shattered) *out_glass_shattered = 1;
    }

    rem = (int)(*hp_ptr) - dmg;
    if (rem <= 0) {
        int is_standalone = (*wptr == WALL_FENCE || *wptr == WALL_FENCE_DMG ||
                             isomap_is_door_closed(*wptr) || isomap_is_door_open(*wptr));
        *wptr   = WALL_NONE;
        *hp_ptr = 0;

        if (g_map[z][wy][wx].floor_id != FLOOR_NONE &&
            g_map[z][wy][wx].obj_id == OBJ_NONE &&
            !(z > 0 && isomap_is_lift_obj(g_map[z - 1][wy][wx].obj_id))) {
            g_map[z][wy][wx].obj_id = OBJ_RUBBLE;
        }
        {
            uint8_t bsmoke = (uint8_t)(1 + (rand() & 1));
            if (g_map[z][wy][wx].smoke < bsmoke) g_map[z][wy][wx].smoke = bsmoke;
        }

        if (!silent) {
            snprintf(msg, sizeof(msg), is_standalone ? "%s DESTROYED!" : "%s WALL BREACHED!", mname);
            set_status(msg);
            isomap_recalc_lighting_and_fov();
            invalidate_path_preview();
        }
        return 2;
    }

    *hp_ptr = (uint8_t)rem;
    if (*wptr == WALL_BRICK_WIN) {
        *wptr = (rem * 2 <= max_hp) ? WALL_BRICK_DMG : WALL_BRICK_BROKEN;
    } else if (*wptr == WALL_DINER_GLASS) {
        *wptr = (rem * 2 <= max_hp) ? WALL_DINER_DMG : WALL_DINER_BROKEN;
    } else if (rem * 2 <= max_hp) {
        switch (*wptr) {
            case WALL_BRICK:
            case WALL_BRICK_BROKEN: *wptr = WALL_BRICK_DMG;    break;
            case WALL_DINER_CHROME:
            case WALL_DINER_BROKEN: *wptr = WALL_DINER_DMG;    break;
            case WALL_BARN_WOOD:    *wptr = WALL_BARN_DMG;     break;
            case WALL_STEEL:        *wptr = WALL_STEEL_DMG;    break;
            case WALL_FENCE:        *wptr = WALL_FENCE_DMG;    break;
            case WALL_UFO_HULL:     *wptr = WALL_UFO_HULL_DMG; break;
            default: break;
        }
    }

    if (!silent) {
        if (out_glass_shattered && *out_glass_shattered) {
            snprintf(msg, sizeof(msg), "WINDOW SHATTERED! (%d/%d HP)", rem, max_hp);
        } else {
            snprintf(msg, sizeof(msg), "%s HIT! (%d/%d HP)", mname, rem, max_hp);
        }
        set_status(msg);
    }
    return 1;
}

static int indoor_ceiling_blocks(int prev_x, int prev_y, int prev_z, int x, int y, int z) {
    int uz = (z > prev_z) ? z : prev_z;
    int lz = (z > prev_z) ? prev_z : z;
    if (g_map[uz][prev_y][prev_x].floor_id != FLOOR_NONE &&
        g_map[uz][y][x].floor_id != FLOOR_NONE &&
        !isomap_is_lift_obj(g_map[lz][prev_y][prev_x].obj_id) &&
        !isomap_is_lift_obj(g_map[lz][y][x].obj_id)) {
        return 1;
    }
    return 0;
}

static int is_los_blocking_prop(uint8_t obj_id) {
    return (obj_id == OBJ_CORN || obj_id == OBJ_ARCADE_CAB ||
            isomap_is_ufo_corner_obj(obj_id) ||
            obj_id == OBJ_UFO_COMPUTER || obj_id == OBJ_UFO_CRYO);
}

static int los_ortho_step_blocked(int x0, int y0, int z, int x1, int y1) {
    if (x1 > x0) return isomap_wall_blocks_los(g_map[z][y0][x1].wall_west);
    if (x1 < x0) return isomap_wall_blocks_los(g_map[z][y0][x0].wall_west);
    if (y1 > y0) return isomap_wall_blocks_los(g_map[z][y1][x0].wall_north);
    if (y1 < y0) return isomap_wall_blocks_los(g_map[z][y0][x0].wall_north);
    return 0;
}

/* True 3D Bresenham Line-of-Sight raycaster across isometric tiles (accounts for walls, ceilings & smoke) */
int isomap_has_los(int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int dz = abs(z1 - z0), sz = z0 < z1 ? 1 : -1;
    int dm = (dx > dy) ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
    int i, x = x0, y = y0, z = z0;
    int ex = dm / 2, ey = dm / 2, ez = dm / 2;
    int smoke_accum = 0;

    for (i = 0; i < dm; i++) {
        int prev_x = x, prev_y = y, prev_z = z;
        ex -= dx; if (ex < 0) { ex += dm; x += sx; }
        ey -= dy; if (ey < 0) { ey += dm; y += sy; }
        ez -= dz; if (ez < 0) { ez += dm; z += sz; }

        if (z != prev_z && indoor_ceiling_blocks(prev_x, prev_y, prev_z, x, y, z)) {
            return 0;
        }

        if (x != prev_x && y != prev_y) {
            /* Diagonal step: prevent sight leaking diagonally between walls and curved UFO corner hull props */
            int path_a_blk = los_ortho_step_blocked(prev_x, prev_y, z, x, prev_y) ||
                             los_ortho_step_blocked(x, prev_y, z, x, y) ||
                             isomap_is_ufo_corner_obj(g_map[z][prev_y][x].obj_id);
            int path_b_blk = los_ortho_step_blocked(prev_x, prev_y, z, prev_x, y) ||
                             los_ortho_step_blocked(prev_x, y, z, x, y) ||
                             isomap_is_ufo_corner_obj(g_map[z][y][prev_x].obj_id);
            if (path_a_blk && path_b_blk) return 0;
        } else {
            if (x > prev_x) {
                if (isomap_wall_blocks_los(g_map[z][y][x].wall_west)) return 0;
            } else if (x < prev_x) {
                if (isomap_wall_blocks_los(g_map[z][prev_y][prev_x].wall_west)) return 0;
            }
            if (y > prev_y) {
                if (isomap_wall_blocks_los(g_map[z][y][x].wall_north)) return 0;
            } else if (y < prev_y) {
                if (isomap_wall_blocks_los(g_map[z][prev_y][prev_x].wall_north)) return 0;
            }
        }

        if (x == x1 && y == y1 && z == z1) break;

        if (g_map[z][y][x].smoke > 0) {
            int s_dens = g_map[z][y][x].smoke;
            if (s_dens > 4) s_dens = 4;
            smoke_accum += s_dens;
            if (smoke_accum >= 4) return 0; /* Dense smoke blocks vision! */
        }
        if (is_los_blocking_prop(g_map[z][y][x].obj_id)) {
            return 0;
        }
    }
    return 1;
}

void isomap_recalc_lighting_and_fov(void) {
    int x, y, z, i;
    int base_shade = g_battle.night_mode ? 9 : 1;

    for (z = 0; z < MAP_LEVS; z++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                if (g_map[z][y][x].fog == FOG_VISIBLE) {
                    g_map[z][y][x].fog = FOG_SEEN;
                }
                g_map[z][y][x].light = (uint8_t)base_shade;
            }
        }
    }

    /* Point light sources: Streetlamps, Electro-Flares, Fire, Arcade, Jukebox & Alien UFO Tech */
    for (z = 0; z < MAP_LEVS; z++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                uint8_t obj = g_map[z][y][x].obj_id;
                int rad = 0;
                if (obj == OBJ_FLARE) rad = 7;
                else if (obj == OBJ_STREETLAMP || obj == OBJ_UFO_CORE) rad = 5;
                else if (g_map[z][y][x].fire > 0) rad = 4;
                else if (obj == OBJ_ARCADE_CAB   || obj == OBJ_JUKEBOX  ||
                         obj == OBJ_UFO_NAV      || obj == OBJ_UFO_COMPUTER ||
                         obj == OBJ_UFO_CRYO     || obj == OBJ_UFO_GRAVLIFT) rad = 3;
                else if (obj == OBJ_UFO_POD) rad = 2;
                if (rad > 0) {
                    int dy, dx;
                    for (dy = -rad; dy <= rad; dy++) {
                        for (dx = -rad; dx <= rad; dx++) {
                            int nx = x + dx, ny = y + dy;
                            int dist = abs(dx) + abs(dy);
                            if ((unsigned)nx < MAP_W && (unsigned)ny < MAP_H && dist <= rad) {
                                int lit = dist;
                                if (lit < g_map[z][ny][nx].light) {
                                    g_map[z][ny][nx].light = (uint8_t)lit;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /* Squad Line-of-Sight visibility check (bounded to 18-tile sight radius per operative) */
    for (i = 0; i < MAX_UNITS; i++) {
        Unit *u = &g_units[i];
        int tz, ty, tx;
        int y0, y1, x0, x1;
        if (!u->active || u->faction != 0) continue;
        y0 = (u->y > 18) ? (u->y - 18) : 0;
        y1 = (u->y + 18 < MAP_H) ? (u->y + 18) : (MAP_H - 1);
        x0 = (u->x > 18) ? (u->x - 18) : 0;
        x1 = (u->x + 18 < MAP_W) ? (u->x + 18) : (MAP_W - 1);
        for (tz = 0; tz < MAP_LEVS; tz++) {
            for (ty = y0; ty <= y1; ty++) {
                for (tx = x0; tx <= x1; tx++) {
                    int dist, max_sight;
                    if (g_map[tz][ty][tx].fog == FOG_VISIBLE) continue;
                    dist = abs(tx - u->x) + abs(ty - u->y);
                    max_sight = (g_battle.night_mode && g_map[tz][ty][tx].light > 6) ? 9 : 18;
                    if (dist <= max_sight && isomap_has_los(u->x, u->y, u->z, tx, ty, tz)) {
                        g_map[tz][ty][tx].fog = FOG_VISIBLE;
                    }
                }
            }
        }
    }

    /* Keep both halves of any spotted 2-tile vehicle and curved UFO saucer corners revealed cleanly */
    for (z = 0; z < MAP_LEVS; z++) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int px, py;
                uint8_t f = g_map[z][y][x].fog;
                if (isomap_is_ufo_corner_obj(g_map[z][y][x].obj_id) && f == FOG_UNSEEN) {
                    static const int ndx[4] = { 1, -1, 0, 0 };
                    static const int ndy[4] = { 0, 0, 1, -1 };
                    int d;
                    for (d = 0; d < 4; d++) {
                        int nx = x + ndx[d], ny = y + ndy[d];
                        if ((unsigned)nx < MAP_W && (unsigned)ny < MAP_H &&
                            g_map[z][ny][nx].fog > g_map[z][y][x].fog) {
                            g_map[z][y][x].fog = g_map[z][ny][nx].fog;
                        }
                    }
                    if (z + 1 < MAP_LEVS && g_map[z][y][x].fog != FOG_UNSEEN &&
                        g_map[z + 1][y][x].floor_id >= FLOOR_UFO_ROOF_N &&
                        g_map[z + 1][y][x].fog < g_map[z][y][x].fog) {
                        g_map[z + 1][y][x].fog = g_map[z][y][x].fog;
                    }
                    continue;
                }
                if (f == FOG_UNSEEN || !isomap_is_vehicle_obj(g_map[z][y][x].obj_id)) continue;
                if (isomap_get_vehicle_partner(x, y, z, &px, &py)) {
                    if (g_map[z][py][px].fog < f) {
                        g_map[z][py][px].fog = f;
                    }
                }
            }
        }
    }
}

void isomap_update_path_preview(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    if (!u->active || g_battle.walking_active || g_battle.fire_mode > 0) {
        g_preview_path.valid = 0;
        return;
    }
    if (u->x == last_prev_ux && u->y == last_prev_uy && u->z == last_prev_uz &&
        g_battle.cur_x == last_prev_cx && g_battle.cur_y == last_prev_cy &&
        g_battle.view_level == last_prev_cz) {
        return;
    }
    last_prev_ux = u->x; last_prev_uy = u->y; last_prev_uz = u->z;
    last_prev_cx = g_battle.cur_x; last_prev_cy = g_battle.cur_y; last_prev_cz = g_battle.view_level;

    pathfind_compute(u->x, u->y, u->z,
                     g_battle.cur_x, g_battle.cur_y, g_battle.view_level,
                     u->unit_type == 2, &g_preview_path);
}

void isomap_start_mission(int biome_id, int night_mode) {
    static const int def_wep[5]  = { 1, 0, 2, 0, 1 };
    static const int def_port[5] = { 0, 6, 1, 7, 2 };
    ProcSpawnPlan plan;
    int i;

    memset(&g_battle, 0, sizeof(g_battle));
    memset(g_units, 0, sizeof(g_units));
    memset(&g_preview_path, 0, sizeof(g_preview_path));
    memset(&g_walk_path, 0, sizeof(g_walk_path));
    invalidate_path_preview();

    g_battle.biome_id = biome_id % BIOME_COUNT;
    g_battle.view_level = 0;
    g_battle.roof_cutaway = 1;
    g_battle.night_mode = night_mode;
    g_battle.selected_unit = 0;
    g_battle.turn_number = 1;
    set_status(biome_titles[g_battle.biome_id]);

    /* Procedurally generate thematic layout, UFO crash/landing & verified spawn coordinates! */
    procgen_build_battlefield(g_battle.biome_id, &plan);

    for (i = 0; i < 5; i++) {
        spawn_unit(i, 0, 0, default_squad_names[i],
                   plan.squad_x[i], plan.squad_y[i], 0,
                   plan.squad_dir, def_wep[i], def_port[i]);
    }

    isomap_load_roster();

    for (i = 0; i < plan.num_aliens && (5 + i) < MAX_UNITS; i++) {
        spawn_unit(5 + i, 1, plan.alien_type[i], plan.alien_name[i],
                   plan.alien_x[i], plan.alien_y[i], plan.alien_z[i],
                   plan.alien_dir[i], 2, 0);
    }

    g_battle.cur_x = g_units[0].x;
    g_battle.cur_y = g_units[0].y;
    isomap_recalc_lighting_and_fov();
    isomap_center_camera_on(g_units[0].x, g_units[0].y, g_units[0].z);
}

void isomap_init(void) {
    isomap_start_mission(BIOME_RTE309, 0);
}

static void set_soldier_status(const Unit *u) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s - AIM:%d%% RCN:%d", u->name, u->firing_acc, u->reactions);
    set_status(buf);
}

void isomap_select_next_soldier(void) {
    int i, start = g_battle.selected_unit;
    if (g_battle.walking_active) return;
    for (i = 1; i <= MAX_UNITS; i++) {
        int idx = (start + i) % MAX_UNITS;
        if (g_units[idx].active && g_units[idx].faction == 0) {
            g_battle.selected_unit = idx;
            g_battle.view_level = g_units[idx].z;
            g_battle.cur_x = g_units[idx].x;
            g_battle.cur_y = g_units[idx].y;
            g_battle.fire_mode = 0;
            g_battle.weapon_menu_open = 0;
            invalidate_path_preview();
            isomap_center_camera_on(g_units[idx].x, g_units[idx].y, g_units[idx].z);
            set_soldier_status(&g_units[idx]);
            sfx_click();
            return;
        }
    }
}

void isomap_toggle_kneel(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    int cost = u->stance ? 8 : 4;
    if (!u->active || u->faction != 0 || g_battle.walking_active) return;
    if (u->tu < cost) {
        set_status("NOT ENOUGH TIME UNITS!");
        sfx_beep(220, 60);
        return;
    }
    u->tu -= cost;
    u->stance = !u->stance;
    u->anim_pose = u->stance ? 1 : 0;
    set_status(u->stance ? "KNEELING (+15% ACCURACY)" : "STANDING UP");
    sfx_click();
}

static int calc_dir_8(int dx, int dy) {
    if (dx == 0 && dy < 0)  return 0;
    if (dx > 0  && dy < 0)  return 1;
    if (dx > 0  && dy == 0) return 2;
    if (dx > 0  && dy > 0)  return 3;
    if (dx == 0 && dy > 0)  return 4;
    if (dx < 0  && dy > 0)  return 5;
    if (dx < 0  && dy == 0) return 6;
    return 7;
}

static int open_single_door_edge(uint8_t *wptr) {
    if (*wptr == WALL_DOOR_CLOSED)     { *wptr = WALL_DOOR_OPEN;     return 1; }
    if (*wptr == WALL_UFO_DOOR_CLOSED) { *wptr = WALL_UFO_DOOR_OPEN; return 2; }
    return 0;
}

static int auto_open_doors_between(int x0, int y0, int z, int x1, int y1) {
    int opened = 0, r;
    if (x1 > x0) {
        r = open_single_door_edge(&g_map[z][y0][x1].wall_west);
        if (r > opened) opened = r;
    } else if (x1 < x0) {
        r = open_single_door_edge(&g_map[z][y0][x0].wall_west);
        if (r > opened) opened = r;
    }
    if (y1 > y0) {
        r = open_single_door_edge(&g_map[z][y1][x0].wall_north);
        if (r > opened) opened = r;
    } else if (y1 < y0) {
        r = open_single_door_edge(&g_map[z][y0][x0].wall_north);
        if (r > opened) opened = r;
    }
    return opened;
}

void isomap_turn_unit_toward(Unit *u, int tx, int ty) {
    int dx = tx - u->x;
    int dy = ty - u->y;
    int target_dir;
    if (!u->active || g_battle.walking_active) return;
    if (dx == 0 && dy == 0) return;

    if (abs(dx) + abs(dy) == 1 && u->tu >= 3) {
        uint8_t *door_ptr = NULL;
        if (dx == 1)       door_ptr = &g_map[u->z][u->y][u->x + 1].wall_west;
        else if (dx == -1) door_ptr = &g_map[u->z][u->y][u->x].wall_west;
        else if (dy == 1)  door_ptr = &g_map[u->z][u->y + 1][u->x].wall_north;
        else if (dy == -1) door_ptr = &g_map[u->z][u->y][u->x].wall_north;

        if (door_ptr && (isomap_is_door_closed(*door_ptr) || isomap_is_door_open(*door_ptr))) {
            int is_ufo_door = (*door_ptr == WALL_UFO_DOOR_CLOSED || *door_ptr == WALL_UFO_DOOR_OPEN);
            if (is_ufo_door) {
                *door_ptr = (*door_ptr == WALL_UFO_DOOR_CLOSED) ? WALL_UFO_DOOR_OPEN : WALL_UFO_DOOR_CLOSED;
            } else {
                *door_ptr = (*door_ptr == WALL_DOOR_CLOSED) ? WALL_DOOR_OPEN : WALL_DOOR_CLOSED;
            }
            u->tu -= 3;
            u->dir = calc_dir_8(dx, dy);
            isomap_recalc_lighting_and_fov();
            invalidate_path_preview();
            if (is_ufo_door) {
                set_status(isomap_is_door_open(*door_ptr) ? "ALIEN AIRLOCK OPENED (3 TU)" : "ALIEN AIRLOCK CLOSED (3 TU)");
            } else {
                set_status(isomap_is_door_open(*door_ptr) ? "DOOR OPENED (3 TU)" : "DOOR CLOSED (3 TU)");
            }
            sfx_door();
            return;
        }
    }

    target_dir = calc_dir_8(dx, dy);
    if (u->dir != target_dir && u->tu >= 1) {
        u->dir = target_dir;
        u->tu -= 1;
        isomap_recalc_lighting_and_fov();
        sfx_click();
    }
}

/* Check if an opposing unit triggers Reaction Fire (Overwatch) when `mover_idx` steps */
static int check_reaction_fire_against(int mover_idx) {
    Unit *m = &g_units[mover_idx];
    int i;
    if (!m->active) return 0;

    for (i = 0; i < MAX_UNITS; i++) {
        Unit *watcher = &g_units[i];
        int req_tu = 12;
        if (!watcher->active || watcher->faction == m->faction) continue;
        if (watcher->tu < req_tu) continue;
        if (watcher->faction == 0 && g_map[m->z][m->y][m->x].fog != FOG_VISIBLE) continue;
        if (abs(watcher->x - m->x) + abs(watcher->y - m->y) > 12) continue;
        if (!isomap_has_los(watcher->x, watcher->y, watcher->z, m->x, m->y, m->z)) continue;

        /* Classic X-COM Reaction Score comparison */
        if ((watcher->tu * watcher->reactions) > (m->tu * m->reactions) && (rand() % 100) < 35) {
            int msx, msy;
            watcher->tu -= req_tu;
            watcher->dir = calc_dir_8(m->x - watcher->x, m->y - watcher->y);
            g_battle.view_level = m->z;
            isomap_tile_to_screen(m->x, m->y, m->z, &msx, &msy);
            if (msx < 24 || msx > SCREEN_W - 56 || msy < 20 || msy > VIEWPORT_H - 24) {
                isomap_center_camera_on(m->x, m->y, m->z);
            }
            set_status(watcher->faction == 0 ? "DUFF-COM REACTION FIRE!" : "ALIEN REACTION FIRE!");
            isomap_render();
            hud_render();
            vga_flip();
            sfx_alert();
            game_wait_frames(settings_alien_post_shot_frames() / 2);
            execute_raycast_shot(i, m->x, m->y, m->z, watcher->firing_acc);
            check_victory_state();
            return 1;
        }
    }
    return 0;
}

void isomap_update_walk(void) {
    Unit *u = &g_units[g_battle.selected_unit];
    PathStep *st;
    int reserve_tu = (g_battle.reserve_mode == 1) ? 12 : (g_battle.reserve_mode == 2) ? 20 : 0;
    int sx0, sy0, sx1, sy1;

    if (!g_battle.walking_active) return;
    if (!u->active || g_battle.walk_step_idx >= g_walk_path.length) {
        g_battle.walking_active = 0;
        u->anim_pose = u->stance ? 1 : 0;
        u->pixel_off_x = 0;
        u->pixel_off_y = 0;
        invalidate_path_preview();
        return;
    }

    st = &g_walk_path.steps[g_battle.walk_step_idx];

    if (g_battle.walk_subframe == 0) {
        int door_opened = 0;
        if (u->tu - (int)st->tu_cost < reserve_tu) {
            set_status(reserve_tu > 0 ? "STOPPED: TUs RESERVED FOR SHOT!" : "OUT OF TIME UNITS!");
            sfx_alert();
            g_battle.walking_active = 0;
            u->anim_pose = u->stance ? 1 : 0;
            u->pixel_off_x = 0;
            u->pixel_off_y = 0;
            invalidate_path_preview();
            return;
        }
        if (u->stance) u->stance = 0;
        if (st->x != u->x || st->y != u->y) {
            u->dir = calc_dir_8(st->x - u->x, st->y - u->y);
        }
        door_opened = auto_open_doors_between(u->x, u->y, u->z, st->x, st->y);
        if (door_opened) {
            set_status(door_opened == 2 ? "ALIEN AIRLOCK OPENED" : "DOOR OPENED");
            sfx_door();
        }
        if (!door_opened) {
            sfx_step();
        }
    }

    {
        int walk_frames = settings_walk_frames();
        int half_step   = (walk_frames >= 4) ? (walk_frames / 2) : 2;

        g_battle.walk_subframe++;
        u->anim_pose = ((g_battle.walk_subframe / half_step) & 1) ? 2 : 3;

        isomap_tile_to_screen(u->x,  u->y,  u->z,  &sx0, &sy0);
        isomap_tile_to_screen(st->x, st->y, st->z, &sx1, &sy1);
        u->pixel_off_x = ((sx1 - sx0) * g_battle.walk_subframe) / walk_frames;
        u->pixel_off_y = ((sy1 - sy0) * g_battle.walk_subframe) / walk_frames;

        if (g_battle.walk_subframe >= walk_frames) {
        int prev_z = u->z;
        int i, newly_spotted = 0;
        uint8_t prev_vis[MAX_UNITS];

        for (i = 0; i < MAX_UNITS; i++) {
            Unit *a = &g_units[i];
            prev_vis[i] = (a->active && a->faction == 1 && g_map[a->z][a->y][a->x].fog == FOG_VISIBLE);
        }

        g_map[u->z][u->y][u->x].unit_idx = -1;
        u->x = st->x;
        u->y = st->y;
        u->z = st->z;
        u->tu -= st->tu_cost;
        if (u->energy >= 2) u->energy -= 2;
        u->pixel_off_x = 0;
        u->pixel_off_y = 0;
        g_map[u->z][u->y][u->x].unit_idx = (int8_t)g_battle.selected_unit;

        if (u->z != prev_z) {
            int low_z = (u->z < prev_z) ? u->z : prev_z;
            int used_grav = (g_map[low_z][u->y][u->x].obj_id == OBJ_UFO_GRAVLIFT);
            g_battle.view_level = u->z;
            if (used_grav) {
                set_status(u->z > prev_z ? "ASCENDED ALIEN GRAV-LIFT" : "DESCENDED ALIEN GRAV-LIFT");
            } else {
                set_status(u->z > prev_z ? "CLIMBED STAIRS TO UPPER LEVEL" : "DESCENDED STAIRS");
            }
        }

        isomap_recalc_lighting_and_fov();

        /* Check if an Alien triggers Reaction Fire against our moving operative! */
        if (check_reaction_fire_against(g_battle.selected_unit)) {
            g_battle.walking_active = 0;
            u->anim_pose = 0;
            invalidate_path_preview();
            return;
        }

        for (i = 0; i < MAX_UNITS; i++) {
            Unit *a = &g_units[i];
            if (a->active && a->faction == 1 && !prev_vis[i] &&
                g_map[a->z][a->y][a->x].fog == FOG_VISIBLE) {
                newly_spotted = 1;
                break;
            }
        }

        g_battle.walk_subframe = 0;
        g_battle.walk_step_idx++;

        if (newly_spotted) {
            set_status("ALIEN SPOTTED! MOVEMENT HALTED");
            sfx_alert();
            g_battle.walking_active = 0;
            u->anim_pose = 0;
            invalidate_path_preview();
        } else if (g_battle.walk_step_idx >= g_walk_path.length) {
            g_battle.walking_active = 0;
            u->anim_pose = 0;
            invalidate_path_preview();
        }
        }
    }
}

void isomap_move_or_interact(int tx, int ty) {
    Unit *u = &g_units[g_battle.selected_unit];
    if (!u->active || u->faction != 0) return;

    if (g_battle.walking_active) {
        g_walk_path.length = g_battle.walk_step_idx + 1;
        set_status("MOVEMENT HALTED");
        return;
    }

    {
        int8_t uid = g_map[g_battle.view_level][ty][tx].unit_idx;
        if (uid >= 0 && g_units[uid].active && g_units[uid].faction == 0 && uid != g_battle.selected_unit) {
            g_battle.selected_unit = uid;
            g_battle.view_level = g_units[uid].z;
            g_battle.weapon_menu_open = 0;
            invalidate_path_preview();
            set_soldier_status(&g_units[uid]);
            sfx_click();
            return;
        }
        if (uid >= 0 && g_units[uid].active && g_units[uid].faction == 1 &&
            g_map[g_battle.view_level][ty][tx].fog == FOG_VISIBLE) {
            if (!isomap_has_los(u->x, u->y, u->z, tx, ty, g_battle.view_level)) {
                set_status("LINE OF FIRE BLOCKED!");
                sfx_beep(190, 50);
                return;
            }
            g_battle.fire_mode = 1;
            isomap_fire_weapon(tx, ty);
            g_battle.fire_mode = 0;
            return;
        }
    }

    if (pathfind_compute(u->x, u->y, u->z, tx, ty, g_battle.view_level,
                         u->unit_type == 2, &g_walk_path) && g_walk_path.length > 0) {
        g_battle.walking_active = 1;
        g_battle.walk_step_idx = 0;
        g_battle.walk_subframe = 0;
    } else {
        set_status("NO PATH TO DESTINATION!");
        sfx_beep(180, 40);
    }
}

/* Play agonized death scream, 4-stage collapse animation, and leave persistent corpse in place! */
static void kill_unit(int victim_idx, int killer_idx) {
    Unit *v;
    char msg[48];
    int visible, stage;

    if (victim_idx < 0 || victim_idx >= MAX_UNITS || !g_units[victim_idx].active) return;
    combat_anim_depth++;
    v = &g_units[victim_idx];

    v->hp = 0;
    v->pixel_off_x = 0;
    v->pixel_off_y = 0;

    if (victim_idx == g_battle.selected_unit) {
        g_battle.walking_active = 0;
        g_battle.fire_mode = 0;
        g_battle.weapon_menu_open = 0;
    }

    {
        int kill_xp = 0;
        if (killer_idx >= 0 && killer_idx < MAX_UNITS && killer_idx != victim_idx &&
            g_units[killer_idx].faction != v->faction) {
            g_units[killer_idx].kills++;
            if (g_units[killer_idx].faction == 0 && v->faction == 1) {
                kill_xp = operative_kill_xp(v->unit_type);
                g_units[killer_idx].xp_gained += kill_xp;
            }
        }
        if (v->faction == 1) {
            g_battle.aliens_killed++;
            if (kill_xp > 0) {
                snprintf(msg, sizeof(msg), "%s ELIMINATED! (+%d XP)", v->name, kill_xp);
            } else {
                snprintf(msg, sizeof(msg), "%s ELIMINATED!", v->name);
            }
        } else {
            snprintf(msg, sizeof(msg), "%s KIA!", v->name);
        }
    }
    set_status(msg);

    /* Trigger species- or gender-specific digital Sound Blaster death scream! */
    sound_play_death(v->faction, v->unit_type, v->gender);

    visible = (v->faction == 0 || g_map[v->z][v->y][v->x].fog == FOG_VISIBLE);
    if (visible) {
        int vsx, vsy;
        g_battle.view_level = v->z;
        isomap_tile_to_screen(v->x, v->y, v->z, &vsx, &vsy);
        if (vsx < 8 || vsx > SCREEN_W - 40 || vsy < 16 || vsy > VIEWPORT_H - 20) {
            isomap_center_camera_on(v->x, v->y, v->z);
        }

        for (stage = 1; stage <= 4; stage++) {
            /* If a hovering Floater dies over open air, drop onto ground floor on final prone stage */
            if (stage == 4) {
                if (g_map[v->z][v->y][v->x].unit_idx == victim_idx) {
                    g_map[v->z][v->y][v->x].unit_idx = -1;
                }
                while (v->z > 0 &&
                       g_map[v->z][v->y][v->x].floor_id == FLOOR_NONE &&
                       !isomap_is_lift_obj(g_map[v->z - 1][v->y][v->x].obj_id)) {
                    v->z--;
                }
                g_battle.view_level = v->z;
            }
            v->death_anim = stage;
            isomap_render();
            hud_render();
            vga_flip();
            game_wait_frames(settings_death_step_frames());
        }
    } else {
        /* Unseen kill in Fog-of-War: let the scream in the dark ring out cleanly! */
        game_wait_frames(settings_death_step_frames() * 3);
    }

    if (g_map[v->z][v->y][v->x].unit_idx == victim_idx) {
        g_map[v->z][v->y][v->x].unit_idx = -1;
    }
    while (v->z > 0 &&
           g_map[v->z][v->y][v->x].floor_id == FLOOR_NONE &&
           !isomap_is_lift_obj(g_map[v->z - 1][v->y][v->x].obj_id)) {
        v->z--;
    }

    v->active     = 0;
    v->death_anim = 0;
    v->is_corpse  = 1;

    if (v->faction == 0) {
        isomap_recalc_lighting_and_fov();
    }

    /* If the currently selected operative fell in action, switch to next living operative */
    if (victim_idx == g_battle.selected_unit) {
        int k;
        for (k = 1; k <= MAX_UNITS; k++) {
            int nidx = (victim_idx + k) % MAX_UNITS;
            if (g_units[nidx].active && g_units[nidx].faction == 0) {
                g_battle.selected_unit = nidx;
                g_battle.view_level = g_units[nidx].z;
                g_battle.cur_x = g_units[nidx].x;
                g_battle.cur_y = g_units[nidx].y;
                invalidate_path_preview();
                break;
            }
        }
    }
    combat_anim_depth--;
}

/* Check whether blast smoke can drift across a 1-tile step without passing through solid walls */
static int smoke_step_unblocked(int x0, int y0, int z, int x1, int y1) {
    if ((unsigned)x1 >= MAP_W || (unsigned)y1 >= MAP_H) return 0;
    if (z > 0 && g_map[z][y1][x1].floor_id == FLOOR_NONE) return 0;
    if (x1 > x0 && isomap_wall_blocks_los(g_map[z][y0][x1].wall_west)) return 0;
    if (x1 < x0 && isomap_wall_blocks_los(g_map[z][y0][x0].wall_west)) return 0;
    if (y1 > y0 && isomap_wall_blocks_los(g_map[z][y1][x0].wall_north)) return 0;
    if (y1 < y0 && isomap_wall_blocks_los(g_map[z][y0][x0].wall_north)) return 0;
    return 1;
}

/* Detonate a 3x3 High-Explosive blast at (cx, cy, cz) with Fire & Billowing Smoke! */
static void detonate_explosion(int cx, int cy, int cz, int attacker_idx) {
    int sx, sy, r, dy, dx, k;
    int dead_list[MAX_UNITS];
    int num_dead = 0;
    int smoke_area = 0, area_bonus = 0;
    uint8_t corn_mask[3][3];
    combat_anim_depth++;
    isomap_tile_to_screen(cx, cy, cz, &sx, &sy);
    memset(corn_mask, 0, sizeof(corn_mask));

    sfx_explosion();
    for (r = 4; r <= 24; r += 5) {
        isomap_render();
        hud_render();
        vga_rect_fill(sx + 16 - r, sy + 8 - r / 2, r * 2, r, (r < 14) ? 96 : 113);
        vga_flip();
        game_wait_frames(settings_explode_step_frames());
    }

    /* Epicenter blast (90 structural dmg) hits all 4 enclosing walls of (cx, cy, cz) */
    damage_wall_edge(cz, cy,     cx,     0, 90, 1, NULL);
    damage_wall_edge(cz, cy,     cx,     1, 90, 1, NULL);
    damage_wall_edge(cz, cy,     cx + 1, 0, 90, 1, NULL);
    damage_wall_edge(cz, cy + 1, cx,     1, 90, 1, NULL);

    /* Apply structural, prop, scorch, and unit HP damage across the 3x3 blast core */
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = cx + dx, ny = cy + dy;
            MapCell *cell;
            if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H) continue;
            cell = &g_map[cz][ny][nx];
            if (cell->obj_id == OBJ_CORN) {
                corn_mask[dy + 1][dx + 1] = 1;
            }
            if (!(dx == 0 && dy == 0)) {
                damage_wall_edge(cz, ny,     nx,     0, 48, 1, NULL);
                damage_wall_edge(cz, ny,     nx,     1, 48, 1, NULL);
                damage_wall_edge(cz, ny,     nx + 1, 0, 32, 1, NULL);
                damage_wall_edge(cz, ny + 1, nx,     1, 32, 1, NULL);
            }

            if (cell->obj_id != OBJ_NONE && !isomap_is_lift_obj(cell->obj_id) && cell->obj_id != OBJ_UFO_CORE) {
                if (!isomap_is_ufo_corner_obj(cell->obj_id) || (dx == 0 && dy == 0)) {
                    isomap_clear_prop_at(cz, ny, nx, OBJ_RUBBLE, (uint8_t)(2 + (rand() & 1)));
                }
            }
            if (cell->floor_id == FLOOR_GRASS || cell->floor_id == FLOOR_DINER_TILE) {
                cell->floor_id = FLOOR_SCORCHED;
            }
            if (cell->unit_idx >= 0 && g_units[cell->unit_idx].active) {
                int uid = cell->unit_idx;
                g_units[uid].hp -= 28;
                if (attacker_idx >= 0 && attacker_idx < MAX_UNITS &&
                    g_units[attacker_idx].faction == 0 && g_units[uid].faction == 1) {
                    g_units[attacker_idx].xp_gained += XP_COMBAT_HIT;
                }
                if (g_units[uid].hp <= 0 && num_dead < MAX_UNITS) {
                    dead_list[num_dead++] = uid;
                }
            }
        }
    }

    /* Measure effective explosion/smoke area (reachable blast tiles + nearby active smoke/fire) */
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int nx = cx + dx, ny = cy + dy;
            int mdist = abs(dx) + abs(dy);
            if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H) continue;
            if (g_map[cz][ny][nx].smoke > 0 || g_map[cz][ny][nx].fire > 0) {
                smoke_area++;
            } else if (abs(dx) <= 1 && abs(dy) <= 1) {
                if ((dx == 0 && dy == 0) || smoke_step_unblocked(cx, cy, cz, nx, ny)) {
                    smoke_area++;
                }
            } else if (mdist == 2) {
                int mx = cx + dx / 2, my = cy + dy / 2;
                if (smoke_step_unblocked(cx, cy, cz, mx, my) &&
                    smoke_step_unblocked(mx, my, cz, nx, ny)) {
                    smoke_area++;
                }
            }
        }
    }
    area_bonus = (smoke_area >= 12) ? 2 : (smoke_area >= 7 ? 1 : 0);

    /* Seed randomized fire & smoke durations scaled by area size and distance from epicenter */
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = cx + dx, ny = cy + dy;
            int mdist = abs(dx) + abs(dy);
            int had_corn = corn_mask[dy + 1][dx + 1];
            int new_fire = 0, new_smoke = 0;
            MapCell *cell;

            if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H) continue;
            if (cz > 0 && g_map[cz][ny][nx].floor_id == FLOOR_NONE) continue;
            if (mdist == 1 && !smoke_step_unblocked(cx, cy, cz, nx, ny)) continue;
            if (mdist == 2 &&
                !(smoke_step_unblocked(cx, cy, cz, nx, cy) && smoke_step_unblocked(nx, cy, cz, nx, ny)) &&
                !(smoke_step_unblocked(cx, cy, cz, cx, ny) && smoke_step_unblocked(cx, ny, cz, nx, ny))) {
                continue;
            }

            cell = &g_map[cz][ny][nx];
            if (mdist == 0) {
                new_fire  = 2 + (rand() & 1) + had_corn;
                new_smoke = 4 + area_bonus + (rand() & 1) + had_corn;
            } else if (mdist == 1) {
                if (had_corn || (rand() % 100) < 55) {
                    new_fire = 1 + (rand() & 1) + had_corn;
                }
                new_smoke = 3 + area_bonus + (rand() % 2) - (rand() & 1) + had_corn;
            } else {
                if (had_corn || (rand() % 100) < 35) {
                    new_fire = 1 + (rand() & 1);
                }
                new_smoke = 2 + (area_bonus > 0 ? 1 : 0) + (rand() % 2) - (rand() & 1) + had_corn;
            }

            if (new_fire > 4)  new_fire = 4;
            if (new_smoke < 2) new_smoke = 2;
            if (new_smoke > 7) new_smoke = 7;

            if (new_fire > (int)cell->fire)   cell->fire  = (uint8_t)new_fire;
            if (new_smoke > (int)cell->smoke) cell->smoke = (uint8_t)new_smoke;
        }
    }

    /* Ragged outer smoke wisps on the 5x5 diamond fringe (clears in 1..3 turns) */
    {
        static const int fdx[8] = {  2, -2,  0,  0,  2,  2, -2, -2 };
        static const int fdy[8] = {  0,  0,  2, -2,  1, -1,  1, -1 };
        int fi;
        for (fi = 0; fi < 8; fi++) {
            int nx = cx + fdx[fi], ny = cy + fdy[fi];
            int mx = cx + (fdx[fi] / 2), my = cy + (fdy[fi] / 2);
            int chance = (fi < 4) ? 55 : 30;
            if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H) continue;
            if ((rand() % 100) >= chance) continue;
            if (smoke_step_unblocked(cx, cy, cz, mx, my) &&
                smoke_step_unblocked(mx, my, cz, nx, ny)) {
                int wisp = 1 + (area_bonus >= 2 ? 1 : 0) + (rand() & 1);
                if (wisp > 3) wisp = 3;
                if (wisp > (int)g_map[cz][ny][nx].smoke) {
                    g_map[cz][ny][nx].smoke = (uint8_t)wisp;
                }
            }
        }
    }
    isomap_recalc_lighting_and_fov();
    invalidate_path_preview();
    set_status("HIGH-EXPLOSIVE DETONATION!");
    for (k = 0; k < num_dead; k++) {
        kill_unit(dead_list[k], attacker_idx);
    }
    check_victory_state();
    combat_anim_depth--;
}

/* Ensure Sound Blaster single-channel gunshot crack plays ~70ms before impact SFX at point-blank */
static void wait_min_shot_frames(int step_idx) {
    int elapsed = (step_idx + 1) * settings_bolt_frames();
    if (elapsed < 5) {
        game_wait_frames(5 - elapsed);
    }
}

/* Map screen-space flight vector (vx, vy) to nearest of 8 isometric projectile sprite angles */
static int calc_screen_proj_dir(int vx, int vy) {
    static const int ux[8] = {    0,   89,  100,   89,    0,  -89, -100,  -89 };
    static const int uy[8] = { -100,  -45,    0,   45,  100,   45,    0,  -45 };
    int k, best_k = 2, best_dot = -1000000;
    for (k = 0; k < 8; k++) {
        int dot = vx * ux[k] + vy * uy[k];
        if (dot > best_dot) {
            best_dot = dot;
            best_k = k;
        }
    }
    return best_k;
}

/* Animate a 4-frame 24x24 weapon/surface impact splash centered at screen (cx, cy) */
static void play_impact_splash(int splash_type, int cx, int cy) {
    int f;
    int step_frames = settings_impact_step_frames();
    if (splash_type < 0 || splash_type >= SPLASH_TYPE_COUNT) {
        splash_type = SPLASH_BULLET;
    }
    if (cx < -FX_W || cx >= SCREEN_W + FX_W || cy < -FX_H || cy >= VIEWPORT_H + FX_H) {
        game_wait_frames(step_frames * 2);
        return;
    }
    for (f = 0; f < 4; f++) {
        isomap_render();
        hud_render();
        vga_blit_shaded(cx - (FX_W / 2), cy - (FX_H / 2),
                        FX_W, FX_H, spr_splash[splash_type][f], 0, VIEWPORT_H);
        vga_flip();
        game_wait_frames(step_frames);
    }
}

/* Smoothly fly directional projectile sprite from (x0, y0) to (x1, y1) across `frames` retraces */
static void animate_proj_segment(int proj_type, int ang_idx, int x0, int y0, int x1, int y1, int frames) {
    int sub;
    if (frames < 1) frames = 1;
    for (sub = 1; sub <= frames; sub++) {
        int bx = x0 + ((x1 - x0) * sub) / frames;
        int by = y0 + ((y1 - y0) * sub) / frames;
        isomap_render();
        hud_render();
        vga_blit_shaded(bx - (FX_W / 2), by - (FX_H / 2),
                        FX_W, FX_H, spr_proj[proj_type][ang_idx & 7], 0, VIEWPORT_H);
        vga_vsync();
        music_tick();
        vga_flip();
    }
}

/* Resolve projectile crossing a West (is_north=0) or North (is_north=1) wall edge.
 * Returns 1 if the projectile stops on the wall, or 0 if it continues onward. */
static int resolve_shot_wall_edge(int wx, int wy, int z, int is_north,
                                  int direct_aim, int stray_shot,
                                  int proj_type, int ang_idx, int bolt_frames,
                                  int wall_dmg, int surf_splash, int step_i,
                                  int *prev_bx, int *prev_by) {
    uint8_t w = is_north ? g_map[z][wy][wx].wall_north : g_map[z][wy][wx].wall_west;
    int wsx, wsy, wcx, wcy;
    int is_intact_glass;

    if (w == WALL_NONE || isomap_is_door_open(w)) return 0;

    /* Waist-high fences & broken window frames let aimed shots pass over/through
     * unless the shooter deliberately targeted this tile edge (or fired a stray shot). */
    if (!isomap_wall_blocks_los(w)) {
        if (!direct_aim && !(stray_shot && (rand() & 1))) {
            return 0;
        }
    }

    isomap_tile_to_screen(wx, wy, z, &wsx, &wsy);
    wcx = wsx + (is_north ? 24 : 8);
    wcy = wsy - 6;
    animate_proj_segment(proj_type, ang_idx, *prev_bx, *prev_by, wcx, wcy, bolt_frames);
    *prev_bx = wcx;
    *prev_by = wcy;

    is_intact_glass = (w == WALL_BRICK_WIN || w == WALL_DINER_GLASS);

    /* Shooting AT a target behind intact glass shatters the pane and keeps flying! */
    if (is_intact_glass && !direct_aim) {
        int glass_shat = 0;
        damage_wall_edge(z, wy, wx, is_north, 15, 0, &glass_shat);
        wait_min_shot_frames(step_i);
        sfx_glass();
        play_impact_splash(SPLASH_GLASS, wcx, wcy);
        return 0;
    }

    /* Direct wall/fence/door/frame impact: apply material HP damage and breach at 0 HP! */
    {
        int glass_shat = 0;
        damage_wall_edge(z, wy, wx, is_north, wall_dmg, 0, &glass_shat);
        wait_min_shot_frames(step_i);
        if (is_intact_glass || glass_shat) {
            sfx_glass();
            play_impact_splash(SPLASH_GLASS, wcx, wcy);
        } else {
            sfx_hit();
            play_impact_splash(surf_splash, wcx, wcy);
        }
    }
    return 1;
}

/* True 3D Bresenham Raycast Weapon Projectile & Surface Splash Impact Handler */
static void execute_raycast_shot(int shooter_idx, int tx, int ty, int tz, int acc) {
    static const int barrel_dx[8] = {  6,  9,  6,  0, -6, -9, -6,  0 };
    static const int barrel_dy[8] = { -3,  0,  3,  6,  3,  0, -3, -6 };
    Unit *u = &g_units[shooter_idx];
    int roll = rand() % 100;
    int stray_shot = (roll >= acc);
    int aim_x = tx, aim_y = ty;
    int dx, sx, dy, sy, dz, sz, dm, i;
    int x = u->x, y = u->y, z = u->z;
    int ex, ey, ez;
    int proj_type   = (u->faction == 1) ? PROJ_PLASMA : (u->weapon_id % PROJ_TYPE_COUNT);
    int surf_splash = (proj_type == PROJ_LASER)  ? SPLASH_LASER  :
                      (proj_type == PROJ_PLASMA) ? SPLASH_PLASMA : SPLASH_BULLET;
    int wall_dmg    = (proj_type == PROJ_PLASMA) ? (40 + (rand() % 13)) :
                      (proj_type == PROJ_LASER)  ? (28 + (rand() % 10)) :
                                                   (20 + (rand() % 9));
    int aim_has_target = 0;
    int start_bx, start_by, end_sx, end_sy, prev_bx, prev_by, ang_idx;
    int bolt_frames = settings_bolt_frames();

    combat_anim_depth++;

    if ((unsigned)tx < MAP_W && (unsigned)ty < MAP_H && (unsigned)tz < MAP_LEVS) {
        MapCell *tc = &g_map[tz][ty][tx];
        if ((tc->unit_idx >= 0 && g_units[tc->unit_idx].active) || tc->obj_id == OBJ_BARREL) {
            aim_has_target = 1;
        }
    }

    if (stray_shot && aim_has_target) {
        aim_x += (rand() % 3) - 1;
        aim_y += (rand() % 3) - 1;
        if (aim_x < 0) aim_x = 0;
        if (aim_x >= MAP_W) aim_x = MAP_W - 1;
        if (aim_y < 0) aim_y = 0;
        if (aim_y >= MAP_H) aim_y = MAP_H - 1;
        if (aim_x == x && aim_y == y && tz == z) {
            aim_x = tx;
            aim_y = ty;
        }
    }

    dx = abs(aim_x - x); sx = x < aim_x ? 1 : -1;
    dy = abs(aim_y - y); sy = y < aim_y ? 1 : -1;
    dz = abs(tz - z);    sz = z < tz ? 1 : -1;
    dm = (dx > dy) ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
    if (dm < 1) dm = 1;
    ex = dm / 2; ey = dm / 2; ez = dm / 2;

    /* Compute gun barrel screen origin & overall projectile screen heading */
    {
        int mx0, my0, dest_by;
        MapCell *aim_cell = &g_map[tz][aim_y][aim_x];
        isomap_tile_to_screen(u->x, u->y, u->z, &mx0, &my0);
        isomap_tile_to_screen(aim_x, aim_y, tz, &end_sx, &end_sy);

        start_bx = mx0 + 16 + barrel_dx[u->dir & 7];
        start_by = my0 - (u->stance ? 3 : 6) + barrel_dy[u->dir & 7];
        dest_by  = (aim_cell->unit_idx >= 0 || aim_cell->obj_id != OBJ_NONE) ? (end_sy - 3) : (end_sy + 6);
        ang_idx  = calc_screen_proj_dir((end_sx + 16) - start_bx, dest_by - start_by);

        /* Weapon-specific Sound Blaster firing audio + custom pixel-art muzzle flash sprite */
        sound_play_weapon((u->faction == 1) ? 2 : u->weapon_id);
        isomap_render();
        hud_render();
        vga_blit_shaded(start_bx - (FX_W / 2), start_by - (FX_H / 2),
                        FX_W, FX_H, spr_muzzle[proj_type], 0, VIEWPORT_H);
        vga_flip();
        game_wait_frames(settings_muzzle_frames());
    }

    prev_bx = start_bx;
    prev_by = start_by;

    for (i = 0; i < dm; i++) {
        int prev_x = x, prev_y = y, prev_z = z;
        int px1, py1, cur_bx, cur_by;
        int direct_aim;
        MapCell *cell;

        ex -= dx; if (ex < 0) { ex += dm; x += sx; }
        ey -= dy; if (ey < 0) { ey += dm; y += sy; }
        ez -= dz; if (ez < 0) { ez += dm; z += sz; }

        /* 0. Solid indoor ceiling/floor blocks cross-elevation shots (except stairwells) */
        if (z != prev_z && indoor_ceiling_blocks(prev_x, prev_y, prev_z, x, y, z)) {
            int cz = (z > prev_z) ? z : prev_z;
            isomap_tile_to_screen(x, y, cz, &px1, &py1);
            cur_bx = px1 + 16;
            cur_by = py1 + 8;
            animate_proj_segment(proj_type, ang_idx, prev_bx, prev_by, cur_bx, cur_by, bolt_frames);
            set_status("SHOT HIT CEILING!");
            wait_min_shot_frames(i);
            sfx_hit();
            play_impact_splash(surf_splash, cur_bx, cur_by);
            break;
        }

        direct_aim = (!aim_has_target && x == tx && y == ty && z == tz);

        /* 1. Check West-wall boundary crossing (+x or -x) */
        if (x != prev_x) {
            int wx = (x > prev_x) ? x : prev_x;
            int wy = (x > prev_x) ? y : prev_y;
            if (resolve_shot_wall_edge(wx, wy, z, 0, direct_aim, stray_shot,
                                       proj_type, ang_idx, bolt_frames,
                                       wall_dmg, surf_splash, i, &prev_bx, &prev_by)) {
                break;
            }
        }

        /* 2. Check North-wall boundary crossing (+y or -y) */
        if (y != prev_y) {
            int wx = (y > prev_y) ? x : prev_x;
            int wy = (y > prev_y) ? y : prev_y;
            if (resolve_shot_wall_edge(wx, wy, z, 1, direct_aim, stray_shot,
                                       proj_type, ang_idx, bolt_frames,
                                       wall_dmg, surf_splash, i, &prev_bx, &prev_by)) {
                break;
            }
        }

        cell = &g_map[z][y][x];
        isomap_tile_to_screen(x, y, z, &px1, &py1);
        cur_bx = px1 + 16;
        if (cell->unit_idx >= 0 && cell->unit_idx != shooter_idx && g_units[cell->unit_idx].active) {
            cur_by = py1 - 4;
        } else if (cell->obj_id != OBJ_NONE) {
            cur_by = py1 - 2;
        } else if (i == dm - 1) {
            cur_by = py1 + 6;
        } else {
            cur_by = py1 - 2;
        }

        /* Fly weapon-specific projectile sprite smoothly across this tile segment */
        animate_proj_segment(proj_type, ang_idx, prev_bx, prev_by, cur_bx, cur_by, bolt_frames);
        prev_bx = cur_bx;
        prev_by = cur_by;

        /* 3. Check living unit impact at (x, y, z) */
        if (cell->unit_idx >= 0 && cell->unit_idx != shooter_idx && g_units[cell->unit_idx].active) {
            int hit_idx = cell->unit_idx;
            Unit *target = &g_units[hit_idx];
            int dmg = (proj_type == PROJ_PLASMA) ? (26 + (rand() % 18)) :
                      (proj_type == PROJ_LASER)  ? (22 + (rand() % 15)) :
                                                   (16 + (rand() % 13));
            int unit_splash = (proj_type == PROJ_BULLET)
                              ? (target->faction == 1 ? SPLASH_HIT_ALIEN : SPLASH_HIT_HUMAN)
                              : surf_splash;
            target->hp -= dmg;
            if (u->faction == 0 && target->faction == 1) {
                u->xp_gained += XP_COMBAT_HIT;
            }
            wait_min_shot_frames(i);
            set_status(target->faction == 1
                       ? (u->faction == 0 ? "DIRECT HIT ON ALIEN! (+10 XP)" : "DIRECT HIT ON ALIEN!")
                       : "OPERATIVE WOUNDED!");
            sfx_hit();
            play_impact_splash(unit_splash, cur_bx, cur_by);
            if (target->hp <= 0) {
                kill_unit(hit_idx, shooter_idx);
            }
            break;
        }

        /* 4. Check volatile fuel barrel impact */
        if (cell->obj_id == OBJ_BARREL) {
            cell->obj_id = OBJ_RUBBLE;
            wait_min_shot_frames(i);
            sfx_hit();
            play_impact_splash(surf_splash, cur_bx, cur_by);
            detonate_explosion(x, y, z, shooter_idx);
            set_status("FUEL BARREL DETONATED!");
            break;
        }

        /* 5. Check blocking prop impact or end-of-trajectory ground/object impact */
        if (i == dm - 1 || (x == aim_x && y == aim_y && z == tz) ||
            is_los_blocking_prop(cell->obj_id)) {
            if (cell->obj_id != OBJ_NONE) {
                int hit_splash = surf_splash;
                if (isomap_is_ufo_corner_obj(cell->obj_id)) {
                    set_status("SHOT DEFLECTED OFF UFO HULL!");
                } else if (!isomap_is_lift_obj(cell->obj_id) && cell->obj_id != OBJ_UFO_CORE &&
                           cell->obj_id != OBJ_RUBBLE        && cell->obj_id != OBJ_FLARE) {
                    uint8_t old_obj = cell->obj_id;
                    int was_veh = isomap_is_vehicle_obj(old_obj);
                    uint8_t psmoke = (uint8_t)((was_veh ? 2 : 1) + (rand() % 3));
                    isomap_clear_prop_at(z, y, x, OBJ_RUBBLE, psmoke);
                    if (old_obj == OBJ_UFO_NAV) {
                        set_status("UFO NAV CONSOLE DESTROYED!");
                    } else if (old_obj == OBJ_UFO_COMPUTER) {
                        set_status("UFO FLIGHT COMPUTER DESTROYED!");
                    } else if (old_obj == OBJ_UFO_CRYO) {
                        set_status("ALIEN CRYO-POD SHATTERED!");
                        hit_splash = SPLASH_GLASS;
                    } else if (old_obj == OBJ_UFO_POD) {
                        set_status("ALIEN TECH POD DESTROYED!");
                    } else {
                        set_status(was_veh ? "VEHICLE WRECKED!" : "COVER DESTROYED!");
                    }
                } else {
                    set_status("SHOT HIT OBSTACLE!");
                }
                wait_min_shot_frames(i);
                if (hit_splash == SPLASH_GLASS) sfx_glass();
                else                            sfx_hit();
                play_impact_splash(hit_splash, cur_bx, cur_by);
            } else {
                /* Open ground impact (or drop to ground below if shot into upper-level air) */
                int gz = z;
                while (gz > 0 && g_map[gz][y][x].floor_id == FLOOR_NONE) gz--;
                if (gz != z) {
                    int gsx, gsy;
                    isomap_tile_to_screen(x, y, gz, &gsx, &gsy);
                    cur_bx = gsx + 16;
                    cur_by = gsy + 6;
                }
                set_status((aim_x != tx || aim_y != ty) ? "SHOT MISSED!" : "SHOT HIT GROUND!");
                wait_min_shot_frames(i);
                sfx_hit();
                play_impact_splash(surf_splash, cur_bx, cur_by);
            }
            break;
        }
    }
    combat_anim_depth--;
}

void isomap_fire_weapon(int tx, int ty) {
    Unit *u = &g_units[g_battle.selected_unit];
    int mode = g_battle.fire_mode ? g_battle.fire_mode : 1;
    int tu_cost = (mode == 1) ? 12 : (mode == 2) ? 20 : (mode == 3) ? 18 : (mode == 4) ? 15 : 8;
    int shots   = (mode == 3) ? 3 : 1;
    int s;

    if (!u->active || u->faction != 0 || g_battle.walking_active) return;
    if (u->tu < tu_cost) {
        set_status("NOT ENOUGH TUs TO ATTACK!");
        sfx_beep(200, 60);
        return;
    }

    /* Mode 5: Throw Night Electro-Flare with tumbling spark arc! */
    if (mode == 5) {
        int sx0, sy0, sx1, sy1, step;
        if (u->flares <= 0) {
            set_status("NO ELECTRO-FLARES LEFT!");
            sfx_beep(200, 60);
            return;
        }
        if (abs(tx - u->x) + abs(ty - u->y) > 12) {
            set_status("OUT OF FLARE THROW RANGE!");
            sfx_beep(200, 60);
            return;
        }
        isomap_turn_unit_toward(u, tx, ty);
        u->tu -= tu_cost;
        u->flares--;
        g_battle.fire_mode = 0;
        combat_anim_depth++;

        isomap_tile_to_screen(u->x, u->y, u->z, &sx0, &sy0);
        isomap_tile_to_screen(tx, ty, g_battle.view_level, &sx1, &sy1);
        for (step = 1; step <= 8; step++) {
            int gx = sx0 + 16 + ((sx1 - sx0) * step) / 8;
            int gy = sy0 - 4 + ((sy1 + 10 - sy0) * step) / 8 - (16 - (step - 4) * (step - 4));
            isomap_render();
            hud_render();
            vga_blit_shaded(gx - (FX_W / 2), gy - (FX_H / 2),
                            FX_W, FX_H, spr_thrown[1][step & 3], 0, VIEWPORT_H);
            vga_flip();
            game_wait_frames(settings_grenade_step_frames());
        }
        if (g_map[g_battle.view_level][ty][tx].obj_id == OBJ_NONE ||
            g_map[g_battle.view_level][ty][tx].obj_id == OBJ_RUBBLE) {
            g_map[g_battle.view_level][ty][tx].obj_id = OBJ_FLARE;
        } else {
            static const int fdx[4] = { 1, -1, 0, 0 };
            static const int fdy[4] = { 0, 0, 1, -1 };
            int d;
            for (d = 0; d < 4; d++) {
                int fx = tx + fdx[d], fy = ty + fdy[d];
                if ((unsigned)fx < MAP_W && (unsigned)fy < MAP_H &&
                    g_map[g_battle.view_level][fy][fx].floor_id != FLOOR_NONE &&
                    g_map[g_battle.view_level][fy][fx].obj_id == OBJ_NONE) {
                    g_map[g_battle.view_level][fy][fx].obj_id = OBJ_FLARE;
                    break;
                }
            }
        }
        isomap_recalc_lighting_and_fov();
        invalidate_path_preview();
        set_status("ELECTRO-FLARE DEPLOYED!");
        sfx_click();
        play_impact_splash(SPLASH_BULLET, sx1 + 16, sy1 + 6);
        combat_anim_depth--;
        return;
    }

    /* Mode 4: Throw High-Explosive Grenade with spinning pineapple sprite! */
    if (mode == 4) {
        int sx0, sy0, sx1, sy1, step;
        if (u->grenades <= 0) {
            set_status("NO HE GRENADES REMAINING!");
            sfx_beep(200, 60);
            return;
        }
        if (abs(tx - u->x) + abs(ty - u->y) > 10) {
            set_status("TARGET OUT OF THROW RANGE!");
            sfx_beep(200, 60);
            return;
        }
        isomap_turn_unit_toward(u, tx, ty);
        u->tu -= tu_cost;
        u->grenades--;
        g_battle.fire_mode = 0;
        combat_anim_depth++;

        isomap_tile_to_screen(u->x, u->y, u->z, &sx0, &sy0);
        isomap_tile_to_screen(tx, ty, g_battle.view_level, &sx1, &sy1);
        for (step = 1; step <= 10; step++) {
            int gx = sx0 + 16 + ((sx1 - sx0) * step) / 10;
            int gy = sy0 - 4 + ((sy1 + 10 - sy0) * step) / 10 - (25 - (step - 5) * (step - 5)) * 3 / 4;
            isomap_render();
            hud_render();
            vga_blit_shaded(gx - (FX_W / 2), gy - (FX_H / 2),
                            FX_W, FX_H, spr_thrown[0][step & 3], 0, VIEWPORT_H);
            vga_flip();
            game_wait_frames(settings_grenade_step_frames());
        }
        detonate_explosion(tx, ty, g_battle.view_level, g_battle.selected_unit);
        isomap_recalc_lighting_and_fov();
        invalidate_path_preview();
        combat_anim_depth--;
        return;
    }

    /* Prevent firing at operative's own tile, while allowing free-fire breaching of walls/doors/fences! */
    if (tx == u->x && ty == u->y && g_battle.view_level == u->z) {
        sfx_beep(190, 50);
        return;
    }

    isomap_turn_unit_toward(u, tx, ty);
    u->tu -= tu_cost;
    if (u->ammo > 0) u->ammo--;
    g_battle.fire_mode = 0;
    combat_anim_depth++;

    for (s = 0; s < shots && u->active; s++) {
        int acc = u->firing_acc + (u->stance ? 15 : 0) + (mode == 2 ? 25 : (mode == 3 ? -15 : 0));
        execute_raycast_shot(g_battle.selected_unit, tx, ty, g_battle.view_level, acc);
        if (s + 1 < shots && u->active) game_wait_frames(settings_burst_gap_frames());
    }
    combat_anim_depth--;

    isomap_recalc_lighting_and_fov();
    invalidate_path_preview();
    check_victory_state();
}

/* Render full-screen black Hidden Movement splash screen (with HIDDEN.BMP) into vga_backbuf */
void isomap_draw_hidden_screen(void) {
    char turn_buf[48];
    const char *btitle = biome_titles[g_battle.biome_id % BIOME_COUNT];

    vga_clear(COL_BLACK);

    if (gfx_hidden_bg && gfx_hidden_w == SCREEN_W && gfx_hidden_h == SCREEN_H) {
        memcpy(vga_backbuf, gfx_hidden_bg, SCREEN_W * SCREEN_H);
        vga_rect_bevel(40, 8, 240, 24, 14, 145, 13);
        vga_print_shadow(115, 12, "HIDDEN MOVEMENT", 96, 15);
        vga_print(91, 22, "ALIEN ACTIVITY DETECTED", 145);
    } else {
        int bw = (gfx_hidden_bg && gfx_hidden_w > 0) ? gfx_hidden_w : 232;
        int bh = (gfx_hidden_bg && gfx_hidden_h > 0) ? gfx_hidden_h : 92;
        int bx = (SCREEN_W - bw) / 2;
        int by = (SCREEN_H - bh) / 2;

        if (bh <= 116) {
            vga_rect_bevel(40, 16, 240, 26, 14, 145, 13);
            vga_print_shadow(115, 20, "HIDDEN MOVEMENT", 96, 15);
            vga_print(91, 31, "ALIEN ACTIVITY DETECTED", 145);

            vga_rect_bevel(40, 158, 240, 26, 14, 68, 13);
            vga_print_shadow((SCREEN_W - vga_text_width(btitle)) / 2, 162, btitle, 80, 15);
            snprintf(turn_buf, sizeof(turn_buf), "TURN %d - ENEMY PHASE", g_battle.turn_number);
            vga_print((SCREEN_W - vga_text_width(turn_buf)) / 2, 173, turn_buf, 3);
        }

        vga_rect_bevel(bx - 4, by - 4, bw + 8, bh + 8, COL_BLACK, 68, 13);
        if (gfx_hidden_bg) {
            vga_blit_ui(bx, by, bw, bh, gfx_hidden_bg);
        } else {
            vga_rect_fill(bx, by, bw, bh, 78);
            vga_print_shadow(98, by + 26, "HIDDEN MOVEMENT", 96, 15);
            vga_print_shadow(64, by + 46, "ALIEN ACTIVITY IN COOPERSBURG", 80, 15);
            vga_print_shadow((SCREEN_W - vga_text_width(btitle)) / 2, by + 66, btitle, 2, 15);
        }

        if (bh > 116) {
            vga_rect_bevel(40, 8, 240, 22, 14, 145, 13);
            vga_print_shadow(115, 12, "HIDDEN MOVEMENT", 96, 15);
            vga_print(91, 21, "ALIEN ACTIVITY DETECTED", 145);
        }
    }
}

static void show_hidden_movement_screen(void) {
    isomap_draw_hidden_screen();
    vga_flip();
}

/* Render the DUFF-COM Operatives Turn splash screen into vga_backbuf */
void isomap_draw_turn_splash(void) {
    char buf[48];
    const char *btitle = biome_titles[g_battle.biome_id % BIOME_COUNT];
    int k, alive_cnt = 0;

    vga_clear(COL_BLACK);

    /* 1. Top DUFF-COM Turn Banner Plaque (y = 14..43) */
    vga_rect_bevel(24, 14, 272, 30, 66, 96, 79);
    vga_rect_fill(26, 16, 268, 26, 75);
    vga_print_shadow(88, 19, "DUFF-COM OPERATIVES TURN", 96, 15);
    snprintf(buf, sizeof(buf), "TURN %d - %s",
             g_battle.turn_number,
             g_battle.night_mode ? "NIGHT TACTICAL OPS" : "DAYLIGHT TACTICAL OPS");
    vga_print_shadow((SCREEN_W - vga_text_width(buf)) / 2, 31, buf, 80, 15);

    /* 2. Center Squad Readiness & Portrait Panel (y = 50..149) */
    vga_rect_bevel(24, 50, 272, 100, 14, 65, 79);
    vga_rect_fill(26, 52, 268, 12, 76);
    vga_print_shadow(64, 55, "STRIKE TEAM READY - TUs RESTORED", 18, 15);

    for (k = 0; k < MAX_SQUAD_SIZE; k++) {
        const Unit *u = &g_units[k];
        int cx = 30 + k * 53;
        int alive = (u->active && !u->is_corpse && u->hp > 0);
        int is_sel = (alive && k == g_battle.selected_unit);
        const char *raw_call = operative_strip_rank_prefix(u->name);
        char call_buf[8];

        if (alive) alive_cnt++;

        vga_rect_bevel(cx, 67, 48, 79,
                       alive ? 75 : 13,
                       is_sel ? 96 : (alive ? 66 : 11),
                       15);

        /* Portrait frame (36x40) + 32x36 Operative Portrait */
        vga_rect_bevel(cx + 6, 70, 36, 40, 15, alive ? 163 : 12, 14);
        if (alive) {
            vga_blit_ui(cx + 8, 72, 32, 36, spr_portraits[u->portrait_id % NUM_PORTRAITS]);
            operative_draw_rank_badge(cx + 9, 73, u->rank);
        } else {
            vga_blit_shaded(cx + 8, 72, 32, 36,
                            spr_portraits[u->portrait_id % NUM_PORTRAITS], 10, SCREEN_H);
            vga_rect_fill(cx + 8, 97, 32, 11, 45);
            vga_print_shadow(cx + 15, 99, "KIA", 33, 15);
        }

        /* Compact Operative Callsign */
        strncpy(call_buf, raw_call, 7);
        call_buf[7] = '\0';
        vga_print_shadow(cx + (48 - vga_text_width(call_buf)) / 2, 113,
                         call_buf, alive ? (is_sel ? 96 : 0) : 9, 15);

        /* HP & TU Readiness Bars */
        if (alive) {
            int hp_w = (u->hp_max > 0) ? ((u->hp * 38) / u->hp_max) : 0;
            int tu_w = (u->tu_max > 0) ? ((u->tu * 38) / u->tu_max) : 0;
            if (hp_w < 1)  hp_w = 1;
            if (hp_w > 38) hp_w = 38;
            if (tu_w < 0)  tu_w = 0;
            if (tu_w > 38) tu_w = 38;

            vga_rect_bevel(cx + 4, 123, 40, 5, 13, 10, 15);
            vga_rect_fill(cx + 5, 124, hp_w, 3, 33);

            vga_rect_bevel(cx + 4, 130, 40, 5, 13, 10, 15);
            if (tu_w > 0) {
                vga_rect_fill(cx + 5, 131, tu_w, 3, 18);
            }
            vga_print(cx + 9, 137, "READY", 80);
        } else {
            vga_print(cx + 6, 129, "FALLEN", 36);
        }
    }

    /* 3. Bottom Sector & Active Operative Summary Plaque (y = 156..185) */
    vga_rect_bevel(24, 156, 272, 30, 14, 66, 79);
    vga_print_shadow((SCREEN_W - vga_text_width(btitle)) / 2, 161, btitle, 96, 15);
    snprintf(buf, sizeof(buf), "%d OF %d OPERATIVES READY FOR ORDERS",
             alive_cnt, MAX_SQUAD_SIZE);
    vga_print((SCREEN_W - vga_text_width(buf)) / 2, 173, buf, 1);
}

/* Render & hold the DUFF-COM Operatives Turn splash screen before returning to battle view */
static void show_operatives_turn_splash(void) {
    int f, total_frames;

    isomap_draw_turn_splash();
    sfx_click();
    vga_flip();
    input_flush();

    total_frames = settings_turn_splash_frames();
    for (f = 0; f < total_frames; f++) {
        vga_vsync();
        music_tick();
        if (f >= 35) {
            input_poll();
            if (g_input.left_clicked || g_input.right_clicked ||
                g_input.last_key == KEY_ENTER || g_input.last_key == KEY_SPACE ||
                g_input.last_key == KEY_ESC) {
                sfx_click();
                break;
            }
        }
    }
    input_flush();
}

void isomap_end_turn(void) {
    int i, z, y, x;
    int hidden_screen_up = 1;
    int need_hidden_pause = 0;
    int any_alien_seen = 0;

    if (g_battle.walking_active || g_battle.mission_ended != 0) return;

    g_battle.fire_mode = 0;
    g_battle.weapon_menu_open = 0;
    sfx_click();
    combat_anim_depth++;

    /* 1. Show HIDDEN.BMP splash screen until alien motion or shooting is seen */
    show_hidden_movement_screen();
    game_wait_frames(settings_hidden_banner_frames());

    /* 2. Tactical Alien AI: Guard/Patrol until alerted, then hunt & fire Plasma if in LOS */
    for (i = 0; i < MAX_UNITS && g_battle.mission_ended == 0; i++) {
        Unit *a = &g_units[i];
        int best_s = -1, best_dist = 999;
        int best_los_s = -1, best_los_dist = 999;
        int ally_spotted = 0;
        int alerted = 0;
        int s, step;
        int ended_with_shot = 0;

        if (!a->active || a->faction != 1) continue;
        a->tu = a->tu_max;

        /* Scan active DUFF-COM operatives for proximity & Line-of-Sight */
        for (s = 0; s < MAX_UNITS; s++) {
            if (g_units[s].active && g_units[s].faction == 0) {
                Unit *op = &g_units[s];
                int d = abs(op->x - a->x) + abs(op->y - a->y);
                int max_ai_sight = (g_battle.night_mode && g_map[op->z][op->y][op->x].light > 6) ? 11 : 19;
                if (d < best_dist) {
                    best_dist = d;
                    best_s = s;
                }
                if (d <= max_ai_sight && d < best_los_dist &&
                    isomap_has_los(a->x, a->y, a->z, op->x, op->y, op->z)) {
                    best_los_dist = d;
                    best_los_s = s;
                }
            }
        }
        if (best_s < 0) continue;

        /* Nearby aliens alert each other if a squadmate within 14 tiles is engaged/spotted */
        for (s = 0; s < MAX_UNITS; s++) {
            if (s != i && g_units[s].active && g_units[s].faction == 1) {
                Unit *ally = &g_units[s];
                if (g_map[ally->z][ally->y][ally->x].fog == FOG_VISIBLE &&
                    abs(ally->x - a->x) + abs(ally->y - a->y) <= 14) {
                    ally_spotted = 1;
                    break;
                }
            }
        }

        alerted = (g_map[a->z][a->y][a->x].fog == FOG_VISIBLE) ||
                  (best_los_s >= 0) ||
                  (best_dist <= 12) ||
                  ally_spotted ||
                  (g_battle.turn_number >= 14 && (((i + g_battle.turn_number) & 1) == 0));

        /* Compute 3D A* path: hunt operative when alerted, or lurk/patrol locally when unalerted! */
        {
            PathResult ai_path;
            int has_path = 0;
            int max_steps = 0;
            int path_limit = 0;

            if (alerted) {
                int goal_s = (best_los_s >= 0) ? best_los_s : best_s;
                Unit *target = &g_units[goal_s];
                g_map[target->z][target->y][target->x].unit_idx = -1;
                has_path = pathfind_compute(a->x, a->y, a->z,
                                            target->x, target->y, target->z,
                                            a->unit_type == 2, &ai_path);
                g_map[target->z][target->y][target->x].unit_idx = (int8_t)goal_s;
                max_steps = 3;
                path_limit = ai_path.length - 1;
            } else {
                int rad = 0, roll = rand() % 100;
                if (i == 5) {               /* UFO Commander: guards UFO interior */
                    if (roll < 45) { rad = 2; max_steps = 1; }
                } else if (i == 6) {        /* Rooftop / Hayloft Sniper: shifts overwatch */
                    if (roll < 50) { rad = 4; max_steps = 2; }
                } else if (i <= 8) {        /* Interior Building / Cornfield Lurkers */
                    if (roll < 60) { rad = 4; max_steps = 2; }
                } else {                    /* Roaming Sector Scouts */
                    if (roll < 85) { rad = 8; max_steps = 3; }
                }

                if (max_steps > 0) {
                    int tries;
                    for (tries = 0; tries < 10 && !has_path; tries++) {
                        int wx = a->x + (rand() % (rad * 2 + 1)) - rad;
                        int wy = a->y + (rand() % (rad * 2 + 1)) - rad;
                        int wz = a->z;
                        if (wx < 1 || wx >= MAP_W - 1 || wy < 1 || wy >= MAP_H - 1) continue;
                        if (wx == a->x && wy == a->y) continue;
                        if (g_map[wz][wy][wx].floor_id == FLOOR_NONE) continue;
                        if (g_map[wz][wy][wx].unit_idx != -1) continue;
                        if (g_map[wz][wy][wx].obj_id != OBJ_NONE &&
                            g_map[wz][wy][wx].obj_id != OBJ_RUBBLE) continue;
                        if (abs(wx - g_units[best_s].x) + abs(wy - g_units[best_s].y) < 9) continue;
                        has_path = pathfind_compute(a->x, a->y, a->z,
                                                    wx, wy, wz,
                                                    a->unit_type == 2, &ai_path);
                    }
                    path_limit = ai_path.length;
                }
                if (!has_path) {
                    a->dir = (a->dir + ((rand() & 1) ? 1 : 7)) & 7;
                }
            }

            for (step = 0; has_path && step < max_steps && step < path_limit &&
                           a->active && a->tu >= 18 && g_battle.mission_ended == 0; step++) {
                PathStep *st = &ai_path.steps[step];
                int prev_ax = a->x, prev_ay = a->y, prev_az = a->z;
                int was_vis = (g_map[prev_az][prev_ay][prev_ax].fog == FOG_VISIBLE);
                int now_vis, door_opened, hold_fire_pos = 0;

                for (s = 0; s < MAX_UNITS; s++) {
                    if (g_units[s].active && g_units[s].faction == 0) {
                        int d_op = abs(g_units[s].x - a->x) + abs(g_units[s].y - a->y);
                        if (d_op <= 9 && isomap_has_los(a->x, a->y, a->z,
                                                        g_units[s].x, g_units[s].y, g_units[s].z)) {
                            hold_fire_pos = 1;
                            break;
                        }
                    }
                }
                if (hold_fire_pos) break; /* Optimal firing position reached! */
                if (g_map[st->z][st->y][st->x].unit_idx != -1) break;

                door_opened = auto_open_doors_between(a->x, a->y, a->z, st->x, st->y);
                if (door_opened) {
                    isomap_recalc_lighting_and_fov();
                    was_vis = (g_map[prev_az][prev_ay][prev_ax].fog == FOG_VISIBLE);
                }
                now_vis = (g_map[st->z][st->y][st->x].fog == FOG_VISIBLE);

                if (was_vis || now_vis) {
                    /* Alien motion spotted by operatives! Cut from HIDDEN.BMP to battle view */
                    int sx0, sy0, sx1, sy1, csx, csy, f;
                    int astep_frames = settings_alien_step_frames();
                    int half_step    = (astep_frames >= 4) ? (astep_frames / 2) : 2;
                    int cam_tx       = now_vis ? st->x : prev_ax;
                    int cam_ty       = now_vis ? st->y : prev_ay;
                    int cam_tz       = now_vis ? st->z : prev_az;
                    int was_hidden   = hidden_screen_up;

                    if (hidden_screen_up) {
                        if (need_hidden_pause) {
                            game_wait_frames(settings_hidden_banner_frames());
                            need_hidden_pause = 0;
                        }
                        hidden_screen_up = 0;
                    }
                    any_alien_seen = 1;
                    ended_with_shot = 0;

                    if (prev_ax != st->x || prev_ay != st->y) {
                        a->dir = calc_dir_8(st->x - prev_ax, st->y - prev_ay);
                    }
                    a->tu -= st->tu_cost;

                    isomap_tile_to_screen(cam_tx, cam_ty, cam_tz, &csx, &csy);
                    if (was_hidden || g_battle.view_level != cam_tz ||
                        csx < 36 || csx > SCREEN_W - 68 || csy < 28 || csy > VIEWPORT_H - 28) {
                        g_battle.view_level = cam_tz;
                        isomap_center_camera_on(cam_tx, cam_ty, cam_tz);
                    }
                    isomap_tile_to_screen(prev_ax, prev_ay, prev_az, &sx0, &sy0);
                    isomap_tile_to_screen(st->x,   st->y,   st->z,   &sx1, &sy1);

                    set_status("ALIEN MOVEMENT SPOTTED!");
                    if (door_opened) sfx_door();
                    else             sfx_step();

                    if (now_vis) {
                        g_map[prev_az][prev_ay][prev_ax].unit_idx = -1;
                        a->x = st->x;
                        a->y = st->y;
                        a->z = st->z;
                        g_map[a->z][a->y][a->x].unit_idx = (int8_t)i;

                        for (f = 1; f <= astep_frames; f++) {
                            a->anim_pose   = ((f / half_step) & 1) ? 2 : 3;
                            a->pixel_off_x = ((sx0 - sx1) * (astep_frames - f)) / astep_frames;
                            a->pixel_off_y = ((sy0 - sy1) * (astep_frames - f)) / astep_frames;
                            isomap_render();
                            hud_render();
                            vga_vsync();
                            music_tick();
                            vga_flip();
                        }
                    } else {
                        /* Alien stepping from visible tile into cover/fog */
                        for (f = 1; f <= half_step; f++) {
                            a->anim_pose   = ((f / half_step) & 1) ? 2 : 3;
                            a->pixel_off_x = ((sx1 - sx0) * f) / astep_frames;
                            a->pixel_off_y = ((sy1 - sy0) * f) / astep_frames;
                            isomap_render();
                            hud_render();
                            vga_vsync();
                            music_tick();
                            vga_flip();
                        }
                        g_map[prev_az][prev_ay][prev_ax].unit_idx = -1;
                        a->x = st->x;
                        a->y = st->y;
                        a->z = st->z;
                        g_map[a->z][a->y][a->x].unit_idx = (int8_t)i;
                        a->anim_pose   = 0;
                        a->pixel_off_x = 0;
                        a->pixel_off_y = 0;
                        isomap_render();
                        hud_render();
                        vga_flip();
                        game_wait_frames(settings_alien_post_shot_frames() / 2);
                        ended_with_shot = 1;
                    }
                    a->anim_pose   = 0;
                    a->pixel_off_x = 0;
                    a->pixel_off_y = 0;
                } else {
                    /* Unseen step: if battle screen was open from an earlier visible step, return to HIDDEN.BMP */
                    if (!hidden_screen_up) {
                        show_hidden_movement_screen();
                        hidden_screen_up = 1;
                        need_hidden_pause = 1;
                    }
                    g_map[prev_az][prev_ay][prev_ax].unit_idx = -1;
                    a->x = st->x;
                    a->y = st->y;
                    a->z = st->z;
                    a->tu -= st->tu_cost;
                    if (a->x != prev_ax || a->y != prev_ay) {
                        a->dir = calc_dir_8(a->x - prev_ax, a->y - prev_ay);
                    }
                    g_map[a->z][a->y][a->x].unit_idx = (int8_t)i;
                    if (door_opened) {
                        sfx_door();
                    }
                }

                /* If a DUFF-COM soldier with Reserved TUs spots this moving alien, trigger Reaction Fire! */
                if (now_vis && check_reaction_fire_against(i)) {
                    ended_with_shot = 1;
                }
            }
        }

        if (g_battle.mission_ended != 0) break;

        /* If alien still alive and has Line-of-Sight to any DUFF-COM operative in sight range, fire Plasma! */
        if (a->active && a->tu >= 15) {
            int shot_s = -1, shot_dist = 999;
            for (s = 0; s < MAX_UNITS; s++) {
                if (g_units[s].active && g_units[s].faction == 0) {
                    Unit *op = &g_units[s];
                    int d = abs(op->x - a->x) + abs(op->y - a->y);
                    int max_ai_sight = (g_battle.night_mode && g_map[op->z][op->y][op->x].light > 6) ? 11 : 19;
                    if (d <= max_ai_sight && d < shot_dist &&
                        isomap_has_los(a->x, a->y, a->z, op->x, op->y, op->z)) {
                        shot_dist = d;
                        shot_s = s;
                    }
                }
            }
            if (shot_s >= 0) {
                Unit *target = &g_units[shot_s];
                int show_z = target->z;
                int tsx, tsy;
                int was_hidden = hidden_screen_up;

                if (g_map[a->z][a->y][a->x].fog == FOG_VISIBLE && a->z > show_z) {
                    show_z = a->z;
                }
                if (hidden_screen_up) {
                    if (need_hidden_pause) {
                        game_wait_frames(settings_hidden_banner_frames());
                        need_hidden_pause = 0;
                    }
                    hidden_screen_up = 0;
                }
                any_alien_seen = 1;
                ended_with_shot = 1;

                a->tu -= 15;
                a->dir = calc_dir_8(target->x - a->x, target->y - a->y);
                g_battle.view_level = show_z;
                isomap_tile_to_screen(target->x, target->y, target->z, &tsx, &tsy);
                if (was_hidden || tsx < 28 || tsx > SCREEN_W - 60 || tsy < 24 || tsy > VIEWPORT_H - 24) {
                    isomap_center_camera_on(target->x, target->y, target->z);
                }
                set_status("ALIEN PLASMA FIRE!");
                isomap_render();
                hud_render();
                vga_flip();
                execute_raycast_shot(i, target->x, target->y, target->z, a->firing_acc);
                check_victory_state();
                if (g_battle.mission_ended != 0) break;
                isomap_render();
                hud_render();
                vga_flip();
                game_wait_frames(settings_alien_post_shot_frames());
            }
        }

        /* Once visible alien motion or shooting completes, return to HIDDEN.BMP! */
        if (!hidden_screen_up && g_battle.mission_ended == 0) {
            if (!ended_with_shot) {
                game_wait_frames((settings_alien_post_shot_frames() * 3) / 4);
            }
            show_hidden_movement_screen();
            hidden_screen_up = 1;
            need_hidden_pause = 1;
        }
    }

    /* 3. Update Smoke dissipation & Fire burn-down at end of full turn */
    if (g_battle.mission_ended == 0) {
        static uint8_t prev_active[MAP_LEVS][MAP_H][MAP_W];
        for (z = 0; z < MAP_LEVS; z++) {
            for (y = 0; y < MAP_H; y++) {
                for (x = 0; x < MAP_W; x++) {
                    const MapCell *sc = &g_map[z][y][x];
                    prev_active[z][y][x] = (uint8_t)(sc->fire > 0 || sc->smoke > 0);
                }
            }
        }

        for (z = 0; z < MAP_LEVS; z++) {
            for (y = 0; y < MAP_H; y++) {
                for (x = 0; x < MAP_W; x++) {
                    MapCell *c = &g_map[z][y][x];
                    if (c->fire > 0) {
                        int dy, dx, patch_sz = 0;
                        for (dy = -1; dy <= 1; dy++) {
                            for (dx = -1; dx <= 1; dx++) {
                                int nx = x + dx, ny = y + dy;
                                if ((unsigned)nx < MAP_W && (unsigned)ny < MAP_H &&
                                    prev_active[z][ny][nx]) {
                                    patch_sz++;
                                }
                            }
                        }

                        c->fire--;
                        if (c->obj_id == OBJ_CORN) c->obj_id = OBJ_RUBBLE;
                        if (c->floor_id == FLOOR_GRASS || c->floor_id == FLOOR_DINER_TILE ||
                            c->floor_id == FLOOR_WOOD) {
                            c->floor_id = FLOOR_SCORCHED;
                        }

                        if (c->fire > 0) {
                            /* Actively burning tiles emit steady rising smoke */
                            uint8_t min_s = (uint8_t)(2 + (rand() & 1));
                            if (c->smoke > 0) c->smoke--;
                            if (c->smoke < min_s) c->smoke = min_s;
                        } else {
                            /* Fire just went out: smolder for a finite randomized turn count based on patch area */
                            int smolder = (patch_sz >= 6) ? (2 + (rand() % 3)) :
                                          (patch_sz >= 3) ? (2 + (rand() & 1)) :
                                                            (1 + (rand() & 1));
                            if (c->smoke > 0) c->smoke--;
                            if ((int)c->smoke < smolder) c->smoke = (uint8_t)smolder;
                            if (c->smoke > 5) c->smoke = 5;
                        }

                        if (c->unit_idx >= 0 && g_units[c->unit_idx].active) {
                            int burn_idx = c->unit_idx;
                            g_units[burn_idx].hp -= 8;
                            if (g_units[burn_idx].hp <= 0) {
                                int burn_vis = (g_units[burn_idx].faction == 0 ||
                                                c->fog == FOG_VISIBLE);
                                if (burn_vis && hidden_screen_up) {
                                    if (need_hidden_pause) {
                                        game_wait_frames(settings_hidden_banner_frames());
                                        need_hidden_pause = 0;
                                    }
                                    hidden_screen_up = 0;
                                    any_alien_seen = 1;
                                }
                                kill_unit(burn_idx, -1);
                                check_victory_state();
                                if (!hidden_screen_up && g_battle.mission_ended == 0) {
                                    show_hidden_movement_screen();
                                    hidden_screen_up = 1;
                                    need_hidden_pause = 1;
                                }
                            }
                        }
                    } else if (c->smoke > 0) {
                        /* Non-burning smoke counts down each turn until completely clear (0) */
                        c->smoke--;
                    }
                }
            }
        }
    }

    /* Hold HIDDEN.BMP cleanly before transitioning to the Operatives Turn splash */
    if (g_battle.mission_ended == 0) {
        if (!any_alien_seen || need_hidden_pause) {
            game_wait_frames(settings_hidden_banner_frames());
        }
    }

    /* 4. Refill DUFF-COM Squad Time Units & Energy for next turn */
    for (i = 0; i < MAX_UNITS; i++) {
        if (g_units[i].active && g_units[i].faction == 0) {
            g_units[i].tu = g_units[i].tu_max;
            g_units[i].energy += 20;
            if (g_units[i].energy > g_units[i].energy_max)
                g_units[i].energy = g_units[i].energy_max;
        }
    }

    g_battle.turn_number++;

    if (!g_units[g_battle.selected_unit].active || g_units[g_battle.selected_unit].faction != 0) {
        for (i = 0; i < MAX_UNITS; i++) {
            if (g_units[i].active && g_units[i].faction == 0) {
                g_battle.selected_unit = i;
                break;
            }
        }
    }
    if (g_units[g_battle.selected_unit].active && g_units[g_battle.selected_unit].faction == 0) {
        Unit *sel = &g_units[g_battle.selected_unit];
        g_battle.view_level = sel->z;
        g_battle.cur_x = sel->x;
        g_battle.cur_y = sel->y;
        isomap_center_camera_on(sel->x, sel->y, sel->z);
    }

    isomap_recalc_lighting_and_fov();
    invalidate_path_preview();
    check_victory_state();

    /* 5. Display Operatives Turn splash screen for a few seconds, then return to battle view */
    if (g_battle.mission_ended == 0) {
        show_operatives_turn_splash();
        set_status("DUFF-COM TURN READY");
    }

    input_flush();
    combat_anim_depth--;
}

/* Trace exact aiming ray to report any wall/fence/door that a direct shot will strike.
 * Returns: 1 = strikes destructible wall (*out_w, *out_hp), -1 = blocked by ceiling/self, 0 = clear to tile */
static int preview_ray_wall(int x0, int y0, int z0, int tx, int ty, int tz,
                            uint8_t *out_w, uint8_t *out_hp) {
    int dx, sx, dy, sy, dz, sz, dm, i;
    int x = x0, y = y0, z = z0;
    int ex, ey, ez;
    int aim_has_target = 0;

    *out_w  = WALL_NONE;
    *out_hp = 0;
    if ((unsigned)tx >= MAP_W || (unsigned)ty >= MAP_H || (unsigned)tz >= MAP_LEVS) return 0;
    if (x0 == tx && y0 == ty && z0 == tz) return -1;

    {
        MapCell *tc = &g_map[tz][ty][tx];
        if ((tc->fog == FOG_VISIBLE && tc->unit_idx >= 0 && g_units[tc->unit_idx].active) ||
            (tc->fog != FOG_UNSEEN && tc->obj_id == OBJ_BARREL)) {
            aim_has_target = 1;
        }
    }

    dx = abs(tx - x); sx = x < tx ? 1 : -1;
    dy = abs(ty - y); sy = y < ty ? 1 : -1;
    dz = abs(tz - z); sz = z < tz ? 1 : -1;
    dm = (dx > dy) ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
    if (dm < 1) return 0;
    ex = dm / 2; ey = dm / 2; ez = dm / 2;

    for (i = 0; i < dm; i++) {
        int prev_x = x, prev_y = y, prev_z = z;
        int edge_seen;
        int direct_aim;

        ex -= dx; if (ex < 0) { ex += dm; x += sx; }
        ey -= dy; if (ey < 0) { ey += dm; y += sy; }
        ez -= dz; if (ez < 0) { ez += dm; z += sz; }

        if (z != prev_z && indoor_ceiling_blocks(prev_x, prev_y, prev_z, x, y, z)) {
            return -1;
        }

        edge_seen  = (g_map[z][prev_y][prev_x].fog != FOG_UNSEEN || g_map[z][y][x].fog != FOG_UNSEEN);
        direct_aim = (!aim_has_target && x == tx && y == ty && z == tz);

        if (x != prev_x) {
            int wx = (x > prev_x) ? x : prev_x;
            int wy = (x > prev_x) ? y : prev_y;
            uint8_t w  = g_map[z][wy][wx].wall_west;
            uint8_t hp = g_map[z][wy][wx].wall_west_hp;
            if (w != WALL_NONE && !isomap_is_door_open(w) && edge_seen) {
                if (isomap_wall_blocks_los(w) || direct_aim) {
                    *out_w  = w;
                    *out_hp = hp ? hp : isomap_wall_max_hp(w);
                    return 1;
                }
            }
        }

        if (y != prev_y) {
            int wx = (y > prev_y) ? x : prev_x;
            int wy = (y > prev_y) ? y : prev_y;
            uint8_t w  = g_map[z][wy][wx].wall_north;
            uint8_t hp = g_map[z][wy][wx].wall_north_hp;
            if (w != WALL_NONE && !isomap_is_door_open(w) && edge_seen) {
                if (isomap_wall_blocks_los(w) || direct_aim) {
                    *out_w  = w;
                    *out_hp = hp ? hp : isomap_wall_max_hp(w);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int isomap_get_cursor_mode(void) {
    Unit *sel_u = &g_units[g_battle.selected_unit];

    if (combat_anim_depth > 0 || g_settings.menu_open || g_battle.weapon_menu_open ||
        g_battle.mission_ended != 0 || g_battle.walking_active ||
        g_input.my >= HUD_Y || !sel_u->active || sel_u->faction != 0) {
        return CURSOR_ARROW;
    }

    if (g_battle.fire_mode > 0) {
        int clear;
        if (g_battle.fire_mode >= 4) {
            int max_rng = (g_battle.fire_mode == 5) ? 12 : 10;
            clear = (abs(g_battle.cur_x - sel_u->x) + abs(g_battle.cur_y - sel_u->y) <= max_rng);
        } else {
            uint8_t hit_w, hit_hp;
            int w_hit = preview_ray_wall(sel_u->x, sel_u->y, sel_u->z,
                                         g_battle.cur_x, g_battle.cur_y, g_battle.view_level,
                                         &hit_w, &hit_hp);
            int aiming_alien = 0;
            if ((unsigned)g_battle.cur_x < MAP_W &&
                (unsigned)g_battle.cur_y < MAP_H &&
                (unsigned)g_battle.view_level < MAP_LEVS) {
                MapCell *tc = &g_map[g_battle.view_level][g_battle.cur_y][g_battle.cur_x];
                if (tc->fog == FOG_VISIBLE && tc->unit_idx >= 0 &&
                    g_units[tc->unit_idx].active && g_units[tc->unit_idx].faction == 1) {
                    aiming_alien = 1;
                }
            }
            if (w_hit < 0) {
                clear = 0;
            } else if (aiming_alien) {
                clear = isomap_has_los(sel_u->x, sel_u->y, sel_u->z,
                                       g_battle.cur_x, g_battle.cur_y, g_battle.view_level);
            } else if (w_hit == 1) {
                clear = 1;
            } else {
                clear = isomap_has_los(sel_u->x, sel_u->y, sel_u->z,
                                       g_battle.cur_x, g_battle.cur_y, g_battle.view_level);
            }
        }
        return clear ? CURSOR_RETICLE_CLEAR : CURSOR_RETICLE_BLOCKED;
    }

    /* In Move mode (fire_mode == 0), hovering a spotted living Alien triggers a Snap Shot on click! */
    if ((unsigned)g_battle.cur_x < MAP_W &&
        (unsigned)g_battle.cur_y < MAP_H &&
        (unsigned)g_battle.view_level < MAP_LEVS) {
        MapCell *c = &g_map[g_battle.view_level][g_battle.cur_y][g_battle.cur_x];
        if (c->fog == FOG_VISIBLE && c->unit_idx >= 0 &&
            g_units[c->unit_idx].active && g_units[c->unit_idx].faction == 1) {
            int clear = isomap_has_los(sel_u->x, sel_u->y, sel_u->z,
                                       g_battle.cur_x, g_battle.cur_y, g_battle.view_level);
            return clear ? CURSOR_RETICLE_CLEAR : CURSOR_RETICLE_BLOCKED;
        }
    }

    return CURSOR_ARROW;
}

static void draw_3d_cursor_box(int tx, int ty, int tz, uint8_t col) {
    int sx, sy;
    int top_x, top_y, right_x, right_y, bot_x, bot_y, left_x, left_y;
    isomap_tile_to_screen(tx, ty, tz, &sx, &sy);

    top_x   = sx + 16; top_y   = sy;
    right_x = sx + 31; right_y = sy + 8;
    bot_x   = sx + 16; bot_y   = sy + 15;
    left_x  = sx;      left_y  = sy + 8;

    vga_line_clip(top_x,   top_y,   right_x, right_y, col, VIEWPORT_H);
    vga_line_clip(right_x, right_y, bot_x,   bot_y,   col, VIEWPORT_H);
    vga_line_clip(bot_x,   bot_y,   left_x,  left_y,  col, VIEWPORT_H);
    vga_line_clip(left_x,  left_y,  top_x,   top_y,   col, VIEWPORT_H);

    vga_line_clip(top_x,   top_y,   top_x,   top_y   - LEVEL_H, col, VIEWPORT_H);
    vga_line_clip(right_x, right_y, right_x, right_y - LEVEL_H, col, VIEWPORT_H);
    vga_line_clip(bot_x,   bot_y,   bot_x,   bot_y   - LEVEL_H, col, VIEWPORT_H);
    vga_line_clip(left_x,  left_y,  left_x,  left_y  - LEVEL_H, col, VIEWPORT_H);

    vga_line_clip(top_x,   top_y   - LEVEL_H, right_x, right_y - LEVEL_H, col, VIEWPORT_H);
    vga_line_clip(right_x, right_y - LEVEL_H, bot_x,   bot_y   - LEVEL_H, col, VIEWPORT_H);
    vga_line_clip(bot_x,   bot_y   - LEVEL_H, left_x,  left_y  - LEVEL_H, col, VIEWPORT_H);
    vga_line_clip(left_x,  left_y  - LEVEL_H, top_x,   top_y   - LEVEL_H, col, VIEWPORT_H);
}

/* =========================================================================
 * Real-Time Animated Billowing Smoke & Flickering Fire Helpers
 * ========================================================================= */
static const int8_t smoke_sway_lut[16] = {
    0, 1, 2, 2, 3, 2, 2, 1, 0, -1, -2, -2, -3, -2, -2, -1
};

static void draw_smoke_puff(int cx, int cy, int rad,
                            uint8_t hi_col, uint8_t mid_col, uint8_t lo_col,
                            int stipple_mode) {
    int dx, dy;

    if (cx + rad + 2 < 0 || cx - rad - 2 >= SCREEN_W ||
        cy + rad + 1 < 0 || cy - rad - 1 >= VIEWPORT_H) {
        return;
    }

    if (rad <= 1) {
        for (dy = -1; dy <= 1; dy++) {
            int py = cy + dy;
            if (py < 0 || py >= VIEWPORT_H) continue;
            for (dx = -1; dx <= 1; dx++) {
                int px = cx + dx;
                uint8_t col;
                if (abs(dx) + abs(dy) > 1) continue;
                if (px < 0 || px >= SCREEN_W) continue;
                if (stipple_mode == 2 && (((px + (py << 1)) & 3) != 0)) continue;
                if (stipple_mode == 1 && (((px + py) & 1) != 0)) continue;
                col = (dy < 0) ? hi_col : ((dy > 0 || dx > 0) ? lo_col : mid_col);
                vga_backbuf[py * SCREEN_W + px] = col;
            }
        }
        return;
    }

    {
        int rx = rad + 1;
        int ry = rad;
        int rx2 = rx * rx;
        int ry2 = ry * ry;
        int max_d = rx2 * ry2;
        int rim_d = (max_d * 3) >> 2;

        for (dy = -ry; dy <= ry; dy++) {
            int py = cy + dy;
            int dy_term;
            if (py < 0 || py >= VIEWPORT_H) continue;
            dy_term = dy * dy * rx2;

            for (dx = -rx; dx <= rx; dx++) {
                int px = cx + dx;
                int d;
                uint8_t col;

                if (px < 0 || px >= SCREEN_W) continue;
                d = dx * dx * ry2 + dy_term;
                if (d > max_d) continue;

                if (stipple_mode == 2) {
                    if (((px + (py << 1)) & 3) != 0) continue;
                } else if (stipple_mode == 1) {
                    if (((px + py) & 1) != 0) continue;
                } else if (d > rim_d) {
                    if (((px + py) & 1) != 0) continue;
                }

                if (dy < 0 && dx <= (rx >> 1) && (dy * 2 < -ry || dx < 0)) {
                    col = hi_col;
                } else if (dy > (ry >> 1) || (dy >= 0 && dx > (rx >> 1))) {
                    col = lo_col;
                } else {
                    col = mid_col;
                }
                vga_backbuf[py * SCREEN_W + px] = col;
            }
        }
    }
}

static void draw_tile_fire(int sx, int sy, int tx, int ty, int tz, const MapCell *cell) {
    int fire_lvl = (cell->fire > 5) ? 5 : (int)cell->fire;
    int f_shade = (cell->fog == FOG_SEEN) ? 5 : 0;
    uint32_t tseed = (uint32_t)(tx * 53 + ty * 97 + tz * 29);
    int f_tick = (int)(g_battle.anim_tick / 3);
    int num_tongues = 4 + fire_lvl * 2;
    int e, i, s;

    /* Glowing ember bed across the scorched isometric diamond */
    for (e = 0; e < 6; e++) {
        uint32_t eh = tseed + (uint32_t)(e * 43);
        int eu = (int)(eh & 7) - 4;
        int ev = (int)((eh >> 3) & 7) - 4;
        int ex = sx + 16 + (eu - ev) * 2;
        int ey = sy + 8 + (eu + ev);
        uint8_t ecol = (((f_tick + e) & 3) == 0) ? 96 : ((((f_tick + e) & 1) != 0) ? 113 : 34);
        vga_pset_clip(ex, ey, vga_shade_table[f_shade][ecol], VIEWPORT_H);
        vga_pset_clip(ex + 1, ey, vga_shade_table[f_shade][116], VIEWPORT_H);
    }

    /* Flickering flame tongues */
    for (i = 0; i < num_tongues; i++) {
        uint32_t h = tseed + (uint32_t)(i * 71);
        int iso_u = (int)(h & 7) - 4;
        int iso_v = (int)((h >> 3) & 7) - 4;
        int bx = sx + 16 + (iso_u - iso_v) * 2;
        int by = sy + 8 + (iso_u + iso_v);
        int phase = f_tick + (int)(h & 15) + i * 3;
        int fh = 4 + fire_lvl + ((phase * 5 + i) & 7);
        int k;

        for (k = 0; k < fh; k++) {
            int sway_idx = (phase + (k >> 1)) & 3;
            int sway = (sway_idx == 1) ? 1 : ((sway_idx == 3) ? -1 : 0);
            int fx = bx + sway;
            int fy = by - k;
            uint8_t c_core, c_edge;

            if (k * 4 < fh) {
                c_core = 96;  c_edge = 112;
            } else if (k * 3 < fh * 2) {
                c_core = 112; c_edge = 114;
            } else {
                c_core = 115; c_edge = 34;
            }
            vga_pset_clip(fx, fy, vga_shade_table[f_shade][c_core], VIEWPORT_H);
            if (k * 2 < fh) {
                vga_pset_clip(fx + 1, fy, vga_shade_table[f_shade][c_edge], VIEWPORT_H);
            }
        }
    }

    /* Rising hot sparks above active flames */
    if (cell->fog == FOG_VISIBLE) {
        for (s = 0; s < 3; s++) {
            int sp_age = (f_tick + s * 7 + (int)(tseed & 15)) & 15;
            if (sp_age < 14) {
                int sp_u = (int)((tseed >> (s * 2)) & 3) - 2;
                int sp_x = sx + 16 + sp_u * 3 + (((sp_age + s) & 2) ? 1 : -1);
                int sp_y = sy + 4 - sp_age;
                uint8_t sp_col = (sp_age < 5) ? 96 : ((sp_age < 10) ? 112 : 35);
                vga_pset_clip(sp_x, sp_y, sp_col, VIEWPORT_H);
            }
        }
    }
}

static void draw_tile_smoke(int sx, int sy, int tx, int ty, int tz,
                            const MapCell *cell, int shade,
                            int front_pass, int force_translucent) {
    int vis_lvl = (cell->smoke > 6) ? 6 : (int)cell->smoke;
    int num_puffs = 2 + vis_lvl * 2;
    int max_rise = 13 + vis_lvl * 2;
    int eff_shade = (shade * 2) / 3;
    uint32_t tseed = (uint32_t)(tx * 131 + ty * 257 + tz * 67);
    int s_clock = (int)(g_battle.anim_tick / 3);
    int p;

    if (eff_shade > 10) eff_shade = 10;

    for (p = 0; p < num_puffs; p++) {
        uint32_t pseed = tseed + (uint32_t)(p * 89);
        int iso_u = (int)(pseed & 7) - 4;
        int iso_v = (int)((pseed >> 3) & 7) - 4;
        int is_front = ((iso_u + iso_v) >= 0) ? 1 : 0;
        int age, rise, sway_raw, sway, drift, cx, cy;
        int peak_rad, rad, stipple;
        uint8_t hi_col, mid_col, lo_col;

        if (is_front != front_pass) continue;

        age = (s_clock + (int)((tseed >> 2) & 63) + (p * 64) / num_puffs) & 63;
        rise = (age * max_rise) >> 6;
        sway_raw = smoke_sway_lut[((age >> 2) + p + (int)(tseed & 15)) & 15];
        sway = (age < 12) ? (sway_raw >> 1) : sway_raw;
        drift = age >> 4;

        cx = sx + 16 + (iso_u - iso_v) * 2 + sway + drift;
        cy = sy + 6 + (iso_u + iso_v) - rise;

        peak_rad = (vis_lvl >= 5) ? 4 : ((vis_lvl >= 3) ? 3 : 2);
        if (vis_lvl == 1) peak_rad = ((p & 1) == 0) ? 2 : 1;

        if (age < 8) {
            rad = (peak_rad > 2) ? 2 : 1;
        } else if (age < 18) {
            rad = (peak_rad > 1) ? (peak_rad - 1) : 1;
        } else if (age < 40) {
            rad = peak_rad;
        } else if (age < 54) {
            rad = (peak_rad > 1) ? (peak_rad - 1) : 1;
        } else {
            rad = (peak_rad >= 4) ? 2 : 1;
        }

        if (vis_lvl == 1) {
            stipple = (age >= 36) ? 2 : 1;
        } else if (vis_lvl == 2) {
            stipple = (age >= 46) ? 2 : ((age >= 24) ? 1 : 0);
        } else {
            stipple = (age >= 52) ? 2 : ((age >= 38) ? 1 : 0);
        }
        if (force_translucent && stipple == 0) stipple = 1;

        if (cell->fire > 0 && age < 18) {
            int f_sh = (cell->fog == FOG_SEEN) ? 5 : 0;
            hi_col  = vga_shade_table[f_sh][(age < 10) ? 114 : 197];
            mid_col = vga_shade_table[eff_shade][201];
            lo_col  = vga_shade_table[eff_shade][12];
        } else {
            uint8_t base_hi  = (vis_lvl >= 4 || cell->fire > 0) ? 5 : 4;
            uint8_t base_mid = (vis_lvl >= 4 || cell->fire > 0) ? 8 : 7;
            uint8_t base_lo  = (vis_lvl >= 4 || cell->fire > 0) ? 11 : 10;
            if (age >= 40) {
                base_hi++;
                base_mid++;
            }
            hi_col  = vga_shade_table[eff_shade][base_hi];
            mid_col = vga_shade_table[eff_shade][base_mid];
            lo_col  = vga_shade_table[eff_shade][base_lo];
            if (hi_col <= 15 && hi_col > 11)   hi_col = 11;
            if (mid_col <= 15 && mid_col > 12)  mid_col = 12;
            if (lo_col <= 15 && lo_col > 13)    lo_col = 13;
        }

        draw_smoke_puff(cx, cy, rad, hi_col, mid_col, lo_col, stipple);
    }
}

void isomap_render(void) {
    int z, y, x, u_idx;
    int max_z = g_battle.roof_cutaway ? g_battle.view_level : (MAP_LEVS - 1);
    int cmode;
    Unit *sel_u = &g_units[g_battle.selected_unit];
    int8_t unit_draw_x[MAX_UNITS];
    int8_t unit_draw_y[MAX_UNITS];
    int8_t unit_draw_z[MAX_UNITS];

    clamp_camera();
    cmode = isomap_get_cursor_mode();

    if (!g_settings.menu_open) {
        g_battle.anim_tick++;
    }

    /* Precompute instantaneous foot-center tile (draw_x, draw_y, draw_z) for every visible unit
     * so sub-tile walking smoothly crosses wall/door/prop boundaries at the exact 50% midpoint! */
    for (u_idx = 0; u_idx < MAX_UNITS; u_idx++) {
        Unit *u = &g_units[u_idx];
        unit_draw_x[u_idx] = -1;
        unit_draw_y[u_idx] = -1;
        unit_draw_z[u_idx] = -1;
        if (!u->active) continue;
        if (u->faction != 0 && g_map[u->z][u->y][u->x].fog != FOG_VISIBLE) continue;

        {
            int dx = u->x, dy = u->y, dz = u->z;
            int changing_z = (g_battle.walking_active &&
                              u_idx == g_battle.selected_unit &&
                              g_battle.walk_step_idx < g_walk_path.length &&
                              g_walk_path.steps[g_battle.walk_step_idx].z != u->z);
            if (!changing_z && (u->pixel_off_x != 0 || u->pixel_off_y != 0) &&
                abs(u->pixel_off_y) <= TILE_H) {
                int usx, usy;
                isomap_tile_to_screen(u->x, u->y, u->z, &usx, &usy);
                isomap_screen_to_tile(usx + (TILE_W / 2) + u->pixel_off_x,
                                      usy + (TILE_H / 2) + u->pixel_off_y,
                                      u->z, &dx, &dy);
                if (g_map[dz][dy][dx].fog == FOG_UNSEEN) {
                    dx = u->x;
                    dy = u->y;
                }
            }
            unit_draw_x[u_idx] = (int8_t)dx;
            unit_draw_y[u_idx] = (int8_t)dy;
            unit_draw_z[u_idx] = (int8_t)dz;
        }
    }

    vga_rect_fill(0, 0, SCREEN_W, VIEWPORT_H, g_battle.night_mode ? 15 : 77);

    for (z = 0; z <= max_z; z++) {
        /* PASS 1: Draw all Floor Diamonds on level `z` first so adjacent floor tiles
         * NEVER draw overtop of operatives or aliens walking across tile boundaries! */
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                MapCell *cell = &g_map[z][y][x];
                int sx, sy, shade;

                if (cell->fog == FOG_UNSEEN || cell->floor_id == FLOOR_NONE) continue;
                isomap_tile_to_screen(x, y, z, &sx, &sy);
                if (sx < -40 || sx >= SCREEN_W + 16 || sy < -32 || sy >= VIEWPORT_H + 32) continue;

                shade = cell->light + (cell->fog == FOG_SEEN ? 4 : 0);
                if (shade > 14) shade = 14;

                /* Keep stairwell & UFO grav-lift openings cut out on upper floors! */
                if (z > 0 && isomap_is_lift_obj(g_map[z - 1][y][x].obj_id)) {
                    uint8_t lobj = g_map[z - 1][y][x].obj_id;
                    uint8_t rcol = (lobj == OBJ_UFO_GRAVLIFT) ? 81 : 119;
                    if (g_map[z - 1][y][x].fog == FOG_UNSEEN) {
                        vga_blit_shaded(sx, sy, OBJ_W, OBJ_H,
                                        spr_objects[lobj], shade, VIEWPORT_H);
                    }
                    vga_line_clip(sx + 16, sy,      sx + 31, sy + 8,  rcol, VIEWPORT_H);
                    vga_line_clip(sx + 31, sy + 8,  sx + 16, sy + 15, rcol, VIEWPORT_H);
                    vga_line_clip(sx + 16, sy + 15, sx,      sy + 8,  rcol, VIEWPORT_H);
                    vga_line_clip(sx,      sy + 8,  sx + 16, sy,      rcol, VIEWPORT_H);
                    continue;
                }
                vga_blit_shaded(sx, sy, TILE_W, TILE_H, spr_floors[cell->floor_id], shade, VIEWPORT_H);
            }
        }

        /* PASS 2: Draw Walls, Doors, Objects, Fire/Smoke, Units & 3D Cursor in Painter's Order */
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                MapCell *cell = &g_map[z][y][x];
                int sx, sy, shade;

                if (cell->fog == FOG_UNSEEN) {
                    int show_w = (cell->wall_west != WALL_NONE && x > 0 &&
                                  g_map[z][y][x - 1].fog != FOG_UNSEEN);
                    int show_n = (cell->wall_north != WALL_NONE && y > 0 &&
                                  g_map[z][y - 1][x].fog != FOG_UNSEEN);
                    if (!show_w && !show_n) continue;
                    isomap_tile_to_screen(x, y, z, &sx, &sy);
                    if (sx < -48 || sx >= SCREEN_W + 24 || sy < -40 || sy >= VIEWPORT_H + 40) continue;
                    if (show_w) {
                        MapCell *wc = &g_map[z][y][x - 1];
                        int wshade = wc->light + (wc->fog == FOG_SEEN ? 4 : 0);
                        if (wshade > 14) wshade = 14;
                        vga_blit_shaded(sx, sy - LEVEL_H, WALL_W, WALL_H,
                                        spr_walls_west[cell->wall_west], wshade, VIEWPORT_H);
                    }
                    if (show_n) {
                        MapCell *nc = &g_map[z][y - 1][x];
                        int nshade = nc->light + (nc->fog == FOG_SEEN ? 4 : 0);
                        if (nshade > 14) nshade = 14;
                        vga_blit_shaded(sx + 16, sy - LEVEL_H, WALL_W, WALL_H,
                                        spr_walls_north[cell->wall_north], nshade, VIEWPORT_H);
                    }
                    continue;
                }
                isomap_tile_to_screen(x, y, z, &sx, &sy);
                if (sx < -48 || sx >= SCREEN_W + 24 || sy < -40 || sy >= VIEWPORT_H + 40) continue;

                shade = cell->light + (cell->fog == FOG_SEEN ? 4 : 0);
                if (shade > 14) shade = 14;

                /* 2. West Wall & North Wall */
                if (cell->wall_west != WALL_NONE) {
                    vga_blit_shaded(sx, sy - LEVEL_H, WALL_W, WALL_H,
                                    spr_walls_west[cell->wall_west], shade, VIEWPORT_H);
                }
                if (cell->wall_north != WALL_NONE) {
                    vga_blit_shaded(sx + 16, sy - LEVEL_H, WALL_W, WALL_H,
                                    spr_walls_north[cell->wall_north], shade, VIEWPORT_H);
                }

                /* 3. Object / Prop (unify light shading across both halves of 2-tile vehicles) */
                if (cell->obj_id != OBJ_NONE) {
                    int obj_shade = shade;
                    int px, py;
                    if (isomap_is_vehicle_obj(cell->obj_id) &&
                        isomap_get_vehicle_partner(x, y, z, &px, &py)) {
                        MapCell *pc = &g_map[z][py][px];
                        int p_shade = pc->light + (pc->fog == FOG_SEEN ? 4 : 0);
                        if (p_shade > 14) p_shade = 14;
                        obj_shade = (shade + p_shade) / 2;
                    }
                    vga_blit_shaded(sx, sy - 24, OBJ_W, OBJ_H,
                                    spr_objects[cell->obj_id], obj_shade, VIEWPORT_H);
                }

                /* 3a. Persistent Corpses lying in place on (x, y, z) */
                for (u_idx = 0; u_idx < MAX_UNITS; u_idx++) {
                    Unit *cu = &g_units[u_idx];
                    if (cu->is_corpse && cu->z == z && cu->y == y && cu->x == x) {
                        vga_blit_shaded(sx, sy - 16, DEATH_W, DEATH_H,
                                        spr_deaths[cu->sprite_id % NUM_UNIT_SPRITES][3],
                                        shade, VIEWPORT_H);
                    }
                }

                /* 3b. Active Fire & Back-Half Billowing Smoke Clouds */
                if (cell->fire > 0) {
                    draw_tile_fire(sx, sy, x, y, z, cell);
                }
                if (cell->smoke > 0) {
                    draw_tile_smoke(sx, sy, x, y, z, cell, shade, 0, 0);
                }

                /* 4. Units whose instantaneous foot position occupies (x, y, z),
                 *    wrapped by Front-Half Billowing Smoke & Top Tactical Markers */
                {
                    int tile_has_live_unit = 0;

                    for (u_idx = 0; u_idx < MAX_UNITS; u_idx++) {
                        if (unit_draw_z[u_idx] == z &&
                            unit_draw_y[u_idx] == y &&
                            unit_draw_x[u_idx] == x) {
                            Unit *u = &g_units[u_idx];
                            int base_sx, base_sy, ux, uy, u_shade;

                            isomap_tile_to_screen(u->x, u->y, u->z, &base_sx, &base_sy);
                            ux = base_sx + 4 + u->pixel_off_x;
                            uy = base_sy - 18 + u->pixel_off_y;
                            u_shade = cell->light;
                            if (u_shade > 14) u_shade = 14;

                            if (u->death_anim >= 1 && u->death_anim <= 4) {
                                vga_blit_shaded(base_sx, base_sy - 16, DEATH_W, DEATH_H,
                                                spr_deaths[u->sprite_id % NUM_UNIT_SPRITES][u->death_anim - 1],
                                                u_shade, VIEWPORT_H);
                            } else {
                                tile_has_live_unit = 1;
                                vga_blit_shaded(ux, uy, UNIT_W, UNIT_H,
                                                spr_units[u->sprite_id % NUM_UNIT_SPRITES][u->anim_pose & 3][u->dir],
                                                u_shade, VIEWPORT_H);
                            }
                        }
                    }

                    if (cell->smoke > 0) {
                        draw_tile_smoke(sx, sy, x, y, z, cell, shade, 1, tile_has_live_unit);
                    }

                    if (tile_has_live_unit) {
                        for (u_idx = 0; u_idx < MAX_UNITS; u_idx++) {
                            if (unit_draw_z[u_idx] == z &&
                                unit_draw_y[u_idx] == y &&
                                unit_draw_x[u_idx] == x) {
                                Unit *u = &g_units[u_idx];
                                int base_sx, base_sy, ux, uy;

                                if (u->death_anim >= 1 && u->death_anim <= 4) continue;
                                isomap_tile_to_screen(u->x, u->y, u->z, &base_sx, &base_sy);
                                ux = base_sx + 4 + u->pixel_off_x;
                                uy = base_sy - 18 + u->pixel_off_y;

                                if (u_idx == g_battle.selected_unit) {
                                    vga_line_clip(ux + 12, uy - 6, ux + 9,  uy - 9, 96, VIEWPORT_H);
                                    vga_line_clip(ux + 12, uy - 6, ux + 15, uy - 9, 96, VIEWPORT_H);
                                    vga_line_clip(ux + 9,  uy - 9, ux + 15, uy - 9, 96, VIEWPORT_H);
                                } else if (u->faction == 1 && uy - 4 >= 0 && uy - 2 <= VIEWPORT_H) {
                                    vga_rect_fill(ux + 10, uy - 4, 4, 2, 32);
                                }
                            }
                        }
                    }
                }

                /* Draw 3D Cursor Box */
                if (combat_anim_depth == 0 && g_input.my < HUD_Y &&
                    !g_battle.weapon_menu_open && !g_settings.menu_open &&
                    z == g_battle.view_level && x == g_battle.cur_x && y == g_battle.cur_y) {
                    uint8_t box_col = (cmode != CURSOR_ARROW) ? 32 : 96;
                    draw_3d_cursor_box(x, y, z, box_col);
                }
            }
        }
    }

    /* Draw 3D A* Path Preview Dots */
    if (combat_anim_depth == 0 && cmode == CURSOR_ARROW &&
        g_input.my < HUD_Y && !g_battle.walking_active &&
        !g_battle.weapon_menu_open && !g_settings.menu_open &&
        g_battle.fire_mode == 0 && g_preview_path.valid && g_preview_path.length > 0) {
        int i;
        int reserve_tu = (g_battle.reserve_mode == 1) ? 12 : (g_battle.reserve_mode == 2) ? 20 : 0;
        for (i = 0; i < g_preview_path.length; i++) {
            PathStep *st = &g_preview_path.steps[i];
            int psx, psy;
            uint8_t dot_col = 18;
            if (st->z > max_z) continue;
            if ((int)st->total_tu > sel_u->tu) dot_col = 33;
            else if ((int)st->total_tu > sel_u->tu - reserve_tu) dot_col = 96;

            isomap_tile_to_screen(st->x, st->y, st->z, &psx, &psy);
            vga_rect_fill(psx + 14, psy + 6, 4, 3, 15);
            vga_rect_fill(psx + 15, psy + 7, 2, 2, dot_col);

            if (i == g_preview_path.length - 1 && psy + 2 < VIEWPORT_H - 8) {
                char tu_str[16];
                int rem = sel_u->tu - (int)st->total_tu;
                snprintf(tu_str, sizeof(tu_str), "TU:%d", rem);
                vga_print_shadow(psx + 6, psy - 10, tu_str, dot_col, 15);
            }
        }
    }

    /* Draw Hit Chance / Throw Range Readout above Target Tile when Reticle is Active */
    if (cmode != CURSOR_ARROW && sel_u->active) {
        int sx1, sy1;
        int clear = (cmode == CURSOR_RETICLE_CLEAR);
        char acc_str[24];
        isomap_tile_to_screen(g_battle.cur_x, g_battle.cur_y, g_battle.view_level, &sx1, &sy1);

        if (g_battle.fire_mode == 5) {
            snprintf(acc_str, sizeof(acc_str), clear ? "FLARE OK" : "TOO FAR");
        } else if (g_battle.fire_mode == 4) {
            snprintf(acc_str, sizeof(acc_str), clear ? "GRENADE OK" : "OUT OF RANGE");
        } else {
            int eff_mode = (g_battle.fire_mode > 0) ? g_battle.fire_mode : 1;
            int acc = sel_u->firing_acc + (sel_u->stance ? 15 : 0) +
                      (eff_mode == 2 ? 25 : (eff_mode == 3 ? -15 : 0));
            if (acc > 99) acc = 99;
            if (g_battle.fire_mode == 0) {
                snprintf(acc_str, sizeof(acc_str), clear ? "SNAP:%d%%" : "BLOCKED", acc);
            } else {
                uint8_t hit_w = WALL_NONE, hit_hp = 0;
                int w_hit = preview_ray_wall(sel_u->x, sel_u->y, sel_u->z,
                                             g_battle.cur_x, g_battle.cur_y, g_battle.view_level,
                                             &hit_w, &hit_hp);
                if (w_hit == 1) {
                    snprintf(acc_str, sizeof(acc_str), "%s:%dHP",
                             isomap_wall_name(hit_w), (int)hit_hp);
                } else if (w_hit < 0) {
                    snprintf(acc_str, sizeof(acc_str), "BLOCKED");
                } else {
                    snprintf(acc_str, sizeof(acc_str), clear ? "HIT:%d%%" : "BLOCKED", acc);
                }
            }
        }
        if (sy1 - 15 >= 0 && sy1 - 15 < VIEWPORT_H - 8) {
            vga_print_shadow(sx1 + 2, sy1 - 15, acc_str, clear ? 96 : 33, 15);
        }
    } else if (combat_anim_depth == 0 && cmode == CURSOR_ARROW &&
               g_input.my < HUD_Y && !g_battle.walking_active &&
               !g_battle.weapon_menu_open && !g_settings.menu_open &&
               (unsigned)g_battle.cur_x < MAP_W && (unsigned)g_battle.cur_y < MAP_H) {
        MapCell *hc = &g_map[g_battle.view_level][g_battle.cur_y][g_battle.cur_x];
        if (hc->fog != FOG_UNSEEN) {
            const char *utag = NULL;
            switch (hc->obj_id) {
                case OBJ_UFO_CORE:     utag = "ELERIUM CORE"; break;
                case OBJ_UFO_NAV:      utag = "UFO NAV";      break;
                case OBJ_UFO_COMPUTER: utag = "FLIGHT COMP";  break;
                case OBJ_UFO_CRYO:     utag = "CRYO-POD";     break;
                case OBJ_UFO_POD:      utag = "TECH POD";     break;
                case OBJ_UFO_GRAVLIFT: utag = "GRAV-LIFT";    break;
                default: break;
            }
            if (utag) {
                int hsx, hsy;
                isomap_tile_to_screen(g_battle.cur_x, g_battle.cur_y, g_battle.view_level, &hsx, &hsy);
                if (hsy - 22 >= 0 && hsy - 22 < VIEWPORT_H - 8) {
                    vga_print_shadow(hsx + 16 - vga_text_width(utag) / 2, hsy - 22, utag, 80, 15);
                }
            }
        }
    }

    /* Top-left compact tactical overlay: Location & Elevation Level */
    vga_print_shadow(4, 4, biome_titles[g_battle.biome_id], 96, 15);
    {
        char lev_buf[32];
        snprintf(lev_buf, sizeof(lev_buf), "LVL:%d  %s",
                 g_battle.view_level, g_battle.night_mode ? "NIGHT" : "DAY");
        vga_print_shadow(236, 4, lev_buf, 80, 15);
    }

    /* Mission Victory / Debrief Overlay Banner when sector is cleared */
    if (g_battle.mission_ended != 0) {
        char stat_buf[48];
        int i, kia_cnt = 0, promo_cnt = 0;
        int first_promo_idx = -1, first_promo_rank = 0;
        int surv_xp = (g_battle.mission_ended == 1)
                      ? (XP_MISSION_VICTORY + (g_battle.night_mode ? XP_NIGHT_BONUS : 0)) : 0;

        for (i = 0; i < MAX_SQUAD_SIZE; i++) {
            if (!g_units[i].active || g_units[i].is_corpse || g_units[i].hp <= 0) {
                kia_cnt++;
            } else {
                int end_xp = g_units[i].xp_start + g_units[i].xp_gained + surv_xp;
                int new_r  = operative_rank_for_xp(end_xp);
                if (new_r > g_units[i].rank) {
                    if (first_promo_idx < 0) {
                        first_promo_idx  = i;
                        first_promo_rank = new_r;
                    }
                    promo_cnt++;
                }
            }
        }

        vga_rect_bevel(28, 10, 264, 128, 74, 96, 79);
        vga_rect_fill(32, 14, 256, 120, 14);
        vga_print_shadow(62, 18,
                         g_battle.mission_ended == 1 ? "MISSION ACCOMPLISHED!" : "MISSION FAILED!",
                         g_battle.mission_ended == 1 ? 96 : 33, 15);
        vga_print(40, 30, biome_titles[g_battle.biome_id], 80);

        snprintf(stat_buf, sizeof(stat_buf), "ALIENS KILLED: %-2d      KIA: %d",
                 g_battle.aliens_killed, kia_cnt);
        vga_print(40, 41, stat_buf, kia_cnt > 0 ? 113 : 18);

        snprintf(stat_buf, sizeof(stat_buf), "SALVAGE: +%d ALLOYS  +%d ELERIUM",
                 g_battle.alloys_recovered, g_battle.elerium_recovered);
        vga_print(40, 51, stat_buf, 144);

        if (g_battle.mission_ended == 1) {
            snprintf(stat_buf, sizeof(stat_buf), "TECH: +%d NAV +%d CRYO +%d POD +%d BODY",
                     g_battle.nav_recovered, g_battle.cryo_recovered,
                     g_battle.pods_recovered, g_battle.corpses_recovered);
            vga_print(40, 61, stat_buf, 81);
            snprintf(stat_buf, sizeof(stat_buf), "SURVIVAL XP: +%d XP (+COMBAT XP)", surv_xp);
            vga_print(40, 71, stat_buf, 80);
        } else {
            vga_print(40, 66, "ENTIRE SQUAD LOST IN ACTION!", 33);
        }

        if (promo_cnt == 1 && first_promo_idx >= 0) {
            const char *base_n = operative_strip_rank_prefix(g_units[first_promo_idx].name);
            snprintf(stat_buf, sizeof(stat_buf), "PROMOTED: %s %s!",
                     operative_rank_prefix(first_promo_rank), base_n);
            vga_print_shadow(40, 84, stat_buf, 96, 15);
            vga_print(40, 95, "RANK UP: +MAX TU, +AIM, +HP, +RCN!", 18);
        } else if (promo_cnt > 1) {
            snprintf(stat_buf, sizeof(stat_buf), "PROMOTIONS: %d OPERATIVES RANKED UP!", promo_cnt);
            vga_print_shadow(40, 84, stat_buf, 96, 15);
            vga_print(40, 95, "RANK UP: +MAX TU, +AIM, +HP, +RCN!", 18);
        } else if (g_battle.mission_ended == 1) {
            vga_print(40, 84, "SQUAD GAINED COMBAT EXPERIENCE!", 96);
            if (kia_cnt > 0) {
                snprintf(stat_buf, sizeof(stat_buf), "%d KIA - NEW ROOKIE%s ASSIGNED AT HQ",
                         kia_cnt, kia_cnt > 1 ? "S" : "");
                vga_print(40, 95, stat_buf, 113);
            } else {
                vga_print(40, 95, "NO CASUALTIES - FULL SQUAD INTACT", 2);
            }
        } else {
            vga_print(40, 84, "PERMADEATH: FALLEN AGENTS RECORDED", 113);
            vga_print(40, 95, "HQ ASSIGNING REPLACEMENT ROOKIES...", 2);
        }

        vga_print_shadow(56, 117, "CLICK OR PRESS ENTER FOR HQ!", 96, 15);
    }
}
