#ifndef ISOMAP_H
#define ISOMAP_H

#include <stdint.h>
#include "pathfind.h"

#define MAP_W    24
#define MAP_H    24
#define MAP_LEVS 3

#define MAX_UNITS 16

/* Fog of War states */
#define FOG_UNSEEN  0
#define FOG_SEEN    1
#define FOG_VISIBLE 2

/* Local Lehigh Valley / Coopersburg Map Biomes */
enum {
    BIOME_RTE309 = 0,   /* Route 309 Diner & Duff's Arcade */
    BIOME_PA_FARM,      /* Pennsylvania Farmland & Red Barn */
    BIOME_SUBURB,       /* Coopersburg Neighborhood & Arcade Basement */
    BIOME_STEEL,        /* Bethlehem Steel Industrial Ruins */
    BIOME_COUNT
};

typedef struct {
    uint8_t floor_id;
    uint8_t wall_west;
    uint8_t wall_north;
    uint8_t wall_west_hp;   /* Structural HP of wall_west (0 = destroyed / WALL_NONE) */
    uint8_t wall_north_hp;  /* Structural HP of wall_north (0 = destroyed / WALL_NONE) */
    uint8_t obj_id;
    uint8_t light;      /* 0 (brightest) .. 15 (darkest) */
    uint8_t fog;        /* FOG_UNSEEN, FOG_SEEN, FOG_VISIBLE */
    uint8_t smoke;      /* 0..5 billowing smoke turns */
    uint8_t fire;       /* 0..4 active fire turns */
    int8_t  unit_idx;   /* -1 if empty, else index in g_units[] */
} MapCell;

typedef struct {
    int active;
    int faction;        /* 0 = DUFF-COM, 1 = ALIEN */
    int unit_type;      /* 0 = Operative, 1 = Sectoid, 2 = Floater, 3 = Muton */
    int sprite_id;      /* Index into spr_units[0..NUM_UNIT_SPRITES-1] */
    int gender;         /* 0 = Male, 1 = Female */
    char name[20];
    int portrait_id;    /* 0..5 Male portraits, 6..11 Female portraits */
    int weapon_id;      /* 0 = Assault Rifle, 1 = Laser Rifle, 2 = Plasma Rifle */
    int ammo;
    int grenades;       /* High-Explosive grenades remaining */
    int flares;         /* Night Electro-Flares remaining */
    int x, y, z;        /* Map tile coordinates */
    int dir;            /* 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW */
    int stance;         /* 0=Standing, 1=Kneeling */
    int anim_pose;      /* 0=Stand, 1=Kneel, 2=WalkA, 3=WalkB */
    int is_corpse;      /* 1=Dead body lying persistently on tile (x,y,z) */
    int death_anim;     /* 0=None, 1..4=Active death collapse stage (0..3) */
    int pixel_off_x;    /* Sub-tile pixel offset during smooth walking */
    int pixel_off_y;
    int tu, tu_max;
    int hp, hp_max;
    int energy, energy_max;
    int morale;
    int firing_acc;     /* Base firing accuracy % */
    int reactions;      /* Reaction Fire initiative stat */
    int kills;
} Unit;

typedef struct {
    int cam_x, cam_y;       /* Screen pixel offset of tile (0,0,0) top-left */
    int view_level;         /* Active camera elevation slice: 0, 1, or 2 */
    int cur_x, cur_y;       /* Hovered isometric tile coordinates */
    int selected_unit;      /* Index in g_units[] of active DUFF-COM operative */
    int biome_id;           /* Active biome (0..3) */
    int roof_cutaway;       /* 1 = Hide roofs/upper levels above view_level or cursor */
    int night_mode;         /* 0 = Daytime Coopersburg, 1 = Night Mission lighting */
    int reserve_mode;       /* 0 = Free Move, 1 = Reserve Snap (12 TU), 2 = Reserve Aimed (20 TU) */
    int fire_mode;          /* 0 = Move, 1 = Snap, 2 = Aimed, 3 = Auto, 4 = HE Grenade, 5 = Flare */
    int weapon_menu_open;   /* 1 = Popup firing mode selector open above HUD */
    int walking_active;     /* 1 = Selected unit is currently walking along g_walk_path */
    int walk_step_idx;
    int walk_subframe;      /* 0..3 interpolation frame per tile step */
    int turn_number;
    int alien_turn_active;
    int mission_ended;      /* 0 = Ongoing, 1 = Victory!, 2 = Squad Wiped */
    int aliens_killed;
    int elerium_recovered;
    int alloys_recovered;
    char status_msg[48];    /* Tactical ticker message on HUD */
    int status_timer;
} BattleState;

extern MapCell g_map[MAP_LEVS][MAP_H][MAP_W];
extern Unit g_units[MAX_UNITS];
extern BattleState g_battle;
extern PathResult g_preview_path;
extern PathResult g_walk_path;

void isomap_init(void);
void isomap_start_mission(int biome_id, int night_mode);
void isomap_load_roster(void);
void isomap_generate_biome(int biome_id);
void isomap_recalc_lighting_and_fov(void);
void isomap_update_path_preview(void);
void isomap_update_walk(void);

/* Coordinate conversions */
void isomap_tile_to_screen(int tx, int ty, int tz, int *sx, int *sy);
void isomap_screen_to_tile(int sx, int sy, int tz, int *tx, int *ty);
void isomap_center_camera_on(int tx, int ty, int tz);

/* Wall Material Properties & Structural Hit Points */
uint8_t     isomap_wall_max_hp(uint8_t wall_id);
int         isomap_wall_blocks_move(uint8_t wall_id);
int         isomap_wall_blocks_los(uint8_t wall_id);
const char *isomap_wall_name(uint8_t wall_id);
void        isomap_init_wall_hp(void);

/* Tactical actions */
int  isomap_has_los(int x0, int y0, int z0, int x1, int y1, int z1);
void isomap_select_next_soldier(void);
void isomap_toggle_kneel(void);
void isomap_turn_unit_toward(Unit *u, int tx, int ty);
void isomap_move_or_interact(int tx, int ty);
void isomap_fire_weapon(int tx, int ty);
void isomap_end_turn(void);

/* Rendering */
int  isomap_get_cursor_mode(void);
void isomap_render(void);

#endif /* ISOMAP_H */
