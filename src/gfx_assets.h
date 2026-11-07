#ifndef GFX_ASSETS_H
#define GFX_ASSETS_H

#include <stdint.h>

#define TILE_W   32
#define TILE_H   16
#define LEVEL_H  24
#define WALL_W   16
#define WALL_H   32
#define OBJ_W    32
#define OBJ_H    40
#define UNIT_W   24
#define UNIT_H   32
#define DEATH_W  32
#define DEATH_H  32
#define FX_W     24
#define FX_H     24

#define NUM_UNIT_SPRITES 9
#define NUM_PORTRAITS    12

/* Unit Sprite Sheet Row Groups (each has 4 poses x 8 directions of 24x32) */
enum {
    UNIT_SPR_MALE_VISOR = 0, /* Male Operative: dark/brown crew-cut & visor */
    UNIT_SPR_MALE_BLONDE,    /* Male Operative: blonde/silver flat-top */
    UNIT_SPR_MALE_VET,       /* Male Operative: dark-skinned buzzcut veteran */
    UNIT_SPR_FEM_RED,        /* Female Operative: redhead/brunette ponytail */
    UNIT_SPR_FEM_BLONDE,     /* Female Operative: blonde/platinum shoulder hair */
    UNIT_SPR_FEM_DARK,       /* Female Operative: dark bob / braided vanguard */
    UNIT_SPR_SECTOID,        /* Alien Sectoid */
    UNIT_SPR_FLOATER,        /* Alien Floater */
    UNIT_SPR_MUTON           /* Alien Muton */
};

/* Floor Tile IDs */
enum {
    FLOOR_NONE = 0,
    FLOOR_GRASS,        /* Pennsylvania green turf */
    FLOOR_ASPHALT,      /* Route 309 dark asphalt */
    FLOOR_ASPHALT_LINE, /* Route 309 center yellow dashed line */
    FLOOR_SIDEWALK,     /* Concrete sidewalk paving */
    FLOOR_DINER_TILE,   /* Coopersburg Diner black/white/cyan checker floor */
    FLOOR_ARCADE_CARPET,/* Duff's Arcade neon-flecked dark indigo carpet */
    FLOOR_WOOD,         /* Barn / upstairs wood planks */
    FLOOR_ROOF_TAR,     /* Flat roof gravel/tar */
    FLOOR_SCORCHED,     /* UFO crash scorch mark */
    FLOOR_COUNT
};

/* Wall Tile IDs (used for both West and North walls of a cell) */
enum {
    WALL_NONE = 0,
    WALL_BRICK,         /* Coopersburg red brick wall (70 HP) */
    WALL_BRICK_WIN,     /* Red brick wall with glass window (50 HP) */
    WALL_DINER_CHROME,  /* Route 309 Diner chrome & neon siding (85 HP) */
    WALL_DINER_GLASS,   /* Diner panoramic booth window (60 HP) */
    WALL_BARN_WOOD,     /* Red Pennsylvania barn plank wall (45 HP) */
    WALL_STEEL,         /* Industrial / UFO metallic bulkhead (130 HP) */
    WALL_DOOR_CLOSED,   /* Swinging/sliding door - closed (38 HP) */
    WALL_DOOR_OPEN,     /* Door frame - open (38 HP) */
    WALL_FENCE,         /* Low wooden/chain perimeter fence (20 HP) */
    WALL_BRICK_BROKEN,  /* Brick wall with shattered jagged window opening */
    WALL_DINER_BROKEN,  /* Diner wall with shattered booth window & broken neon */
    WALL_BRICK_DMG,     /* Cracked, cratered & scorched red brick wall */
    WALL_DINER_DMG,     /* Buckled, scorched diner chrome & dented cladding */
    WALL_BARN_DMG,      /* Splintered PA barn planks with blown-out gaps */
    WALL_STEEL_DMG,     /* Scorched, dented steel bulkhead w/ ruptured conduit */
    WALL_FENCE_DMG,     /* Half-shattered fence with snapped rails & pickets */
    WALL_COUNT
};

/* Object / Prop IDs */
enum {
    OBJ_NONE = 0,
    OBJ_ARCADE_CAB,     /* Glowing 90s Arcade Cabinet ("DUFF-COM" / "POLYBIUS") */
    OBJ_DINER_BOOTH,    /* Red vinyl diner booth & table */
    OBJ_JUKEBOX,        /* Glowing Wurlitzer jukebox */
    OBJ_CORN,           /* Tall Pennsylvania cornstalks (blocks LOS, flammable!) */
    OBJ_STREETLAMP,     /* Route 309 streetlamp (emits light at night) */
    OBJ_CAR_HOOD,       /* Parked Cruiser / Sedan */
    OBJ_UFO_CORE,       /* Glowing Alien Elerium Power Core */
    OBJ_CRATE,          /* Supply / Ammo crate */
    OBJ_STAIRS_UP,      /* Stairs leading up to next elevation level */
    OBJ_RUBBLE,         /* Destroyed wall / object rubble */
    OBJ_FLARE,          /* Throwable Night Electro-Flare (bright light radius) */
    OBJ_BARREL,         /* Explosive Fuel / Chemical Drum (detonates when shot!) */
    OBJ_TRACTOR,        /* Pennsylvania Farm Tractor */
    OBJ_COUNT
};

extern uint8_t spr_floors[FLOOR_COUNT][TILE_W * TILE_H];
extern uint8_t spr_walls_west[WALL_COUNT][WALL_W * WALL_H];
extern uint8_t spr_walls_north[WALL_COUNT][WALL_W * WALL_H];
extern uint8_t spr_objects[OBJ_COUNT][OBJ_W * OBJ_H];

/* Unit sprites: [sprite_id 0..8][pose: 0=Stand, 1=Kneel, 2=WalkA, 3=WalkB][facing 0..7] */
extern uint8_t spr_units[NUM_UNIT_SPRITES][4][8][UNIT_W * UNIT_H];

/* Death collapse & persistent corpse sprites: [sprite_id 0..8][stage: 0..2 collapse, 3=corpse] */
extern uint8_t spr_deaths[NUM_UNIT_SPRITES][4][DEATH_W * DEATH_H];

/* Weapon Projectile Types */
enum {
    PROJ_BULLET = 0, /* Assault Rifle high-velocity brass/copper tracer round */
    PROJ_LASER  = 1, /* Coopersburg Prototype electric-cyan laser pulse bolt */
    PROJ_PLASMA = 2, /* Alien / Heavy Elerium green plasma orb & ionized tail */
    PROJ_TYPE_COUNT = 3
};

/* Impact Splash Animation Types (4 frames of 24x24 each) */
enum {
    SPLASH_BULLET    = 0, /* Ballistic ricochet sparks, dust puff & debris chips */
    SPLASH_LASER     = 1, /* Cyan/blue thermal laser ionization burst & molten sparks */
    SPLASH_PLASMA    = 2, /* Green Elerium plasma splash & glowing bio-droplets */
    SPLASH_GLASS     = 3, /* Shattering window glass shards */
    SPLASH_HIT_HUMAN = 4, /* Ballistic impact on operative armor + crimson spray */
    SPLASH_HIT_ALIEN = 5, /* Ballistic impact on alien + neon-green bio-ichor burst */
    SPLASH_TYPE_COUNT = 6
};

/* Weapon Projectiles, Thrown Munitions, Muzzle Flashes, Reticles & Impact Splashes (24x24) */
extern uint8_t spr_proj[PROJ_TYPE_COUNT][8][FX_W * FX_H];
extern uint8_t spr_thrown[2][4][FX_W * FX_H];   /* [0=Grenade, 1=Flare][spin 0..3] */
extern uint8_t spr_muzzle[PROJ_TYPE_COUNT][FX_W * FX_H];
extern uint8_t spr_reticle[2][FX_W * FX_H];     /* [0=Clear Lock-On, 1=Blocked LOS] */
extern uint8_t spr_splash[SPLASH_TYPE_COUNT][4][FX_W * FX_H];

/* Portraits (12 x 32x36: 0..5 Male, 6..11 Female) & Weapon Icons (40x24) for the HUD */
extern uint8_t spr_portraits[NUM_PORTRAITS][32 * 36];
extern uint8_t spr_weapons[4][40 * 24];

/* Optional full-frame BMP graphic resources loaded from assets/*.BMP */
extern uint8_t *gfx_title_bg;   /* 320x200 Title Screen artwork (TITLEBG.BMP) */
extern uint8_t *gfx_hidden_bg;  /* 232x92 Hidden Movement artwork (HIDDEN.BMP) */
extern uint8_t *gfx_radar_bg;   /* 300x132 Full-Width Lehigh Valley Radar Map (RADAR.BMP) */

void gfx_init_assets(void);
int  gfx_portrait_to_unit_sprite(int portrait_id);

#endif /* GFX_ASSETS_H */
