#ifndef ITEMS_H
#define ITEMS_H

#include <stdint.h>

/* =========================================================================
 * DUFF-COM: Unified Weapons, Ammo, Equipment, Artifacts & Research Registry
 * ========================================================================= */

enum {
    ITEM_NONE = -1,

    /* 1. Human Ballistic Weapons (Researched at Game Start) */
    ITEM_PISTOL = 0,            /* 9mm Service Pistol */
    ITEM_ASSAULT_RIFLE,         /* 5.56mm Assault Rifle */
    ITEM_HEAVY_RIFLE,           /* 7.62mm Heavy Battle Rifle */

    /* 2. Human Ballistic Ammo Magazines (Researched at Start) */
    ITEM_PISTOL_CLIP,           /* 9mm Pistol Magazine (12 rds) */
    ITEM_RIFLE_MAG,             /* 5.56mm Rifle Magazine (20 rds) */
    ITEM_HEAVY_MAG,             /* 7.62mm Heavy Drum Mag (15 rds) */

    /* 3. Human Thrown Munitions & Utility (Researched at Start) */
    ITEM_FRAG_GRENADE,          /* Light M67 Frag Grenade (Radius 2, 55 dmg) */
    ITEM_HEAVY_GRENADE,         /* Heavy HE Satchel Charge (Radius 3, 85 dmg) */
    ITEM_FLARE,                 /* Night Electro-Flare (Radius 7 light) */

    /* 4. Coopersburg Prototype Laser Weapons & Batteries (Researched) */
    ITEM_LASER_PISTOL,          /* Coopersburg Laser Pistol */
    ITEM_LASER_RIFLE,           /* Coopersburg Prototype Laser Rifle */
    ITEM_HEAVY_LASER,           /* Coopersburg Heavy Laser Cannon */
    ITEM_LASER_BATTERY,         /* Compact Laser Cell Battery (Pistol/Rifle) */
    ITEM_HEAVY_LASER_PAK,       /* High-Capacity Laser Capacitor Pack (Heavy Laser) */

    /* 5. Alien Elerium Plasma Weapons, Cartridges & Grenades */
    ITEM_PLASMA_PISTOL,         /* Alien Plasma Pistol */
    ITEM_PLASMA_RIFLE,          /* Alien Plasma Rifle */
    ITEM_HEAVY_PLASMA,          /* Alien Heavy Plasma Cannon */
    ITEM_PLASMA_PISTOL_CLIP,    /* Plasma Pistol Cartridge (Requires Elerium-115) */
    ITEM_PLASMA_RIFLE_CLIP,     /* Plasma Rifle Cartridge (Requires Elerium-115) */
    ITEM_HEAVY_PLASMA_CLIP,     /* Heavy Plasma Power Core (Requires Elerium-115) */
    ITEM_ALIEN_GRENADE,         /* Alien Elerium Pulse Grenade (Radius 3, 110 dmg) */

    /* 6. Recovered Alien Materials, UFO Subsystems & Corpses */
    ITEM_ELERIUM,               /* Elerium-115 Pod (NEVER manufacturable; mission only!) */
    ITEM_ALLOYS,                /* Alien Hull Alloys */
    ITEM_UFO_NAV,               /* Intact UFO Navigation / Flight Computer */
    ITEM_UFO_CRYO,              /* Intact Alien Cryo-Stasis Bio-Chamber */
    ITEM_UFO_POD,               /* Intact Alien Ordnance / Tech Pod */
    ITEM_CORPSE_SECTOID,        /* Recovered Sectoid Corpse */
    ITEM_CORPSE_FLOATER,        /* Recovered Floater Corpse */
    ITEM_CORPSE_MUTON,          /* Recovered Muton Corpse */

    ITEM_COUNT
};

/* Item Category Flags */
enum {
    ICAT_WEAPON   = 0,  /* Primary or sidearm firearm/energy weapon */
    ICAT_AMMO     = 1,  /* Magazine, battery, or plasma cartridge */
    ICAT_GRENADE  = 2,  /* Explosive thrown ordnance */
    ICAT_UTILITY  = 3,  /* Electro-flare / tactical utility */
    ICAT_ARTIFACT = 4,  /* Elerium, Alloys, UFO components */
    ICAT_CORPSE   = 5   /* Alien cadaver for autopsy or sale */
};

/* Research Tech Tree Topics */

enum {
    TECH_NONE = -1,
    TECH_LASER_WEAPONS = 0,     /* Unlocks Laser Pistol, Laser Rifle & Laser Battery */
    TECH_HEAVY_LASER,           /* Unlocks Heavy Laser Cannon & High-Cap Laser Pack */
    TECH_PLASMA_PISTOL,         /* Unlocks Alien Plasma Pistol & Cartridge use/mfg */
    TECH_PLASMA_RIFLE,          /* Unlocks Alien Plasma Rifle & Cartridge use/mfg */
    TECH_HEAVY_PLASMA,          /* Unlocks Heavy Plasma Cannon & Core use/mfg */
    TECH_ALIEN_GRENADE,         /* Unlocks Alien Pulse Grenade use/mfg */
    TECH_AUTOPSY_SECTOID,       /* Sectoid Biology & Psionic Cranium Report */
    TECH_AUTOPSY_FLOATER,       /* Floater Anti-Gravity Implant Report */
    TECH_AUTOPSY_MUTON,         /* Muton Heavy Bio-Armor & Combat Report */
    TECH_UFO_NAVIGATION,        /* UFO Hyper-Wave Tracking & Interception Bonus */
    TECH_UFO_CONSTRUCTION,      /* Unlocks ARV Cargo Rack Expansion & Alloy Fabrication */
    TECH_ARV_TURRET,            /* Unlocks ARV Roof-Mounted Defensive Turret Mk1 */
    TECH_ARV_TURRET_MK2,        /* Unlocks ARV Turret Mk2 (+Range, +Acc, Lower Cooldown) */
    TECH_COUNT
};

typedef struct {
    int id;
    const char *short_name;     /* Compact <= 8 char HUD/button name (e.g. "A.RIFLE") */
    const char *full_name;      /* Full display title (<= 22 chars) */
    int category;               /* ICAT_* */
    int ammo_item_id;           /* Matching ITEM_* ammo ID for weapons (-1 if self/none) */
    int clip_size;              /* Rounds per magazine/battery/cartridge */
    int proj_type;              /* 0=Bullet, 1=Laser, 2=Plasma */
    int dmg_base;               /* Base damage per hit (or center explosion dmg) */
    int dmg_rand;               /* Random bonus damage (0..dmg_rand) */
    int blast_radius;           /* Explosion tile radius for grenades (0 for guns) */
    int snap_tu, snap_acc;      /* Snap shot TU cost & accuracy multiplier % */
    int aimed_tu, aimed_acc;    /* Aimed shot TU cost & accuracy multiplier % */
    int auto_tu, auto_acc;      /* Auto 3-round burst TU cost & accuracy % (0 if no auto) */
    int req_tech;               /* TECH_* required to equip, buy, or manufacture (-1 = start) */
    int buy_price;              /* Store purchase price in $ (0 = cannot be bought) */
    int sell_price;             /* Store sell value in $ (all items > 0) */
    int can_manufacture;        /* 1 = Workshop can build once req_tech is researched */
    int mfg_cash;               /* Workshop cash cost to manufacture 1 unit */
    int mfg_alloys;             /* Alien Alloys required to manufacture 1 unit */
    int mfg_elerium;            /* Elerium-115 required to manufacture 1 unit */
    int store_space;            /* Storage units consumed in Base Stores / ARV Cargo */
    int weight;                 /* Tactical encumbrance weight (reduces stamina & scales move TUs) */
} ItemDef;

typedef struct {
    int id;
    const char *title;          /* Research project name (<= 22 chars) */
    int req_tech;               /* Prerequisite TECH_* (-1 if none) */
    int req_item;               /* Required sample ITEM_* in Base Stores (-1 if none) */
    int research_cost;          /* Scientist-hours required to complete */
    const char *unlock_summary; /* Short description of what this unlocks */
    const char *log_entry[3];   /* 3-line lore/technical report for the Research Log */
} TechDef;

/* Procedural UI Icons (32x16), Battlefield Ground Sprites (16x12) & Base Tiles (54x37) loaded from .BMP */
#define ITEM_ICON_W   32
#define ITEM_ICON_H   16
#define GROUND_ITEM_W 16
#define GROUND_ITEM_H 12
#define BASE_TILE_W   54
#define BASE_TILE_H   37

extern uint8_t spr_item_icons[ITEM_COUNT][ITEM_ICON_W * ITEM_ICON_H];
extern uint8_t spr_ground_items[ITEM_COUNT][GROUND_ITEM_W * GROUND_ITEM_H];
extern uint8_t spr_base_tiles[12][BASE_TILE_W * BASE_TILE_H];
extern int     has_base_bmp;


void           items_init_assets(void);
const ItemDef *item_get_def(int item_id);
const TechDef *tech_get_def(int tech_id);

/* Query helpers */
int item_is_weapon(int item_id);
int item_is_ammo(int item_id);
int item_is_grenade(int item_id);
int item_fits_hand(int item_id);
int item_fits_small_belt(int item_id);
int item_fits_large_belt(int item_id);
int item_fits_backpack(int item_id);
int item_legacy_weapon_class(int item_id); /* Maps any weapon to 0=Ballistic, 1=Laser, 2=Plasma */

/* Roll a randomized starting weapon + ammo + grenade loadout for an alien race (1=Sectoid, 2=Floater, 3=Muton) */
void items_roll_alien_loadout(int alien_unit_type, int campaign_day,
                              int *out_weapon_id, int *out_ammo_id,
                              int *out_ammo_rds, int *out_grenade_id, int *out_grenades);

#endif /* ITEMS_H */

