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
    ITEM_CORPSE_CRYPTID,        /* Recovered Cryptid Corpse */

    /* 7. Captured Live Aliens (Require Alien Containment at HQ; Sell-Only in Store) */
    ITEM_LIVE_SECTOID,          /* Captured Live Sectoid */
    ITEM_LIVE_SECTOID_CMDR,     /* Captured Live Sectoid Commander */
    ITEM_LIVE_FLOATER,          /* Captured Live Floater */
    ITEM_LIVE_MUTON,            /* Captured Live Muton */
    ITEM_LIVE_CRYPTID,          /* Captured Live Cryptid */

    /* 8. Non-Lethal Capture, Field Medicine, Psionics & Autonomous ARV Drones */
    ITEM_STUN_ROD,              /* High-Voltage Melee Stun Prod (Store Purchase) */
    ITEM_STUN_GRENADE,          /* Non-Lethal Concussion Stun Grenade (Store Purchase) */
    ITEM_MEDKIT,                /* Field Trauma Medkit (Heal & Revive Operatives) */
    ITEM_MIND_PROBE,            /* Psionic Mind Probe (Requires Psi-Lab & Psi-Training) */
    ITEM_QUAD_DRONE,            /* ARV Recon Quad-Copter w/ Onboard Rifle */
    ITEM_UFO_DRONE,             /* ARV Alloy UFO Hover-Drone w/ Plasma Emitter */

    ITEM_COUNT
};

/* Item Category Flags */
enum {
    ICAT_WEAPON     = 0,  /* Primary or sidearm firearm/energy/melee weapon */
    ICAT_AMMO       = 1,  /* Magazine, battery, or plasma cartridge */
    ICAT_GRENADE    = 2,  /* Explosive or stun thrown ordnance */
    ICAT_UTILITY    = 3,  /* Electro-flare, Medkit, Mind Probe, Drone */
    ICAT_ARTIFACT   = 4,  /* Elerium, Alloys, UFO components */
    ICAT_CORPSE     = 5,  /* Alien cadaver for autopsy or sale */
    ICAT_LIVE_ALIEN = 6   /* Captured live alien in stasis/containment */
};

/* Expandable Research Tech Tree Topics
 * Weapon research follows: Basic Weapon Type -> Specific Weapons within that type */
enum {
    TECH_NONE = -1,

    /* Laser Weapon Tree: Basic Theory -> Pistol, Rifle, Heavy Laser, Laser Cannon Turret */
    TECH_LASER_WEAPONS = 0,     /* Basic Laser Theory (Prerequisite for all Laser weapons) */
    TECH_LASER_PISTOL,          /* Coopersburg Laser Pistol & Compact Battery */
    TECH_LASER_RIFLE,           /* Coopersburg Prototype Laser Rifle */
    TECH_HEAVY_LASER,           /* Coopersburg Heavy Laser & Capacitor Pack */
    TECH_ARV_TURRET,            /* Laser Cannon Defense (ARV Roof Turret Mk1 & Base Turret) */

    /* Plasma Weapon Tree: Basic Theory -> Pistol, Rifle, Heavy Plasma, Plasma Cannon Turret */
    TECH_PLASMA_WEAPONS,        /* Basic Elerium Plasma Theory (Prerequisite for all Plasma weapons) */
    TECH_PLASMA_PISTOL,         /* Alien Plasma Pistol & Cartridge replication */
    TECH_PLASMA_RIFLE,          /* Alien Plasma Rifle & Cartridge replication */
    TECH_HEAVY_PLASMA,          /* Alien Heavy Plasma Cannon & Core replication */
    TECH_ARV_TURRET_MK2,        /* Plasma Cannon Defense (ARV Roof Turret Mk2 & Base Plasma) */

    /* Ordnance, Field Medicine & Autonomous ARV Drones */
    TECH_ALIEN_GRENADE,         /* Unlocks Alien Pulse Grenade use & Workshop mfg */
    TECH_MEDKIT,                /* Unlocks Field Trauma Medkit use, purchase & Workshop mfg */
    TECH_QUAD_DRONE,            /* Unlocks ARV Quad-Copter Rifle Drone & ARV Front Pad */
    TECH_UFO_DRONE,             /* Unlocks Upgraded Alloy UFO Plasma Drone */

    /* Xenobiology: Alien Autopsies (Require Corpse) */
    TECH_AUTOPSY_SECTOID,       /* Sectoid Biology & Cranial Receptor Report */
    TECH_AUTOPSY_FLOATER,       /* Floater Anti-Gravity Implant Report */
    TECH_AUTOPSY_MUTON,         /* Muton Heavy Bio-Armor & Combat Report */
    TECH_AUTOPSY_CRYPTID,       /* Cryptid Bio-Weapon & Parasitic Zombification Report */

    /* Xenobiology: Live Alien Interrogations (Require Alien Containment + Live Specimen) */
    TECH_LIVE_SECTOID,          /* Live Sectoid Neural Telemetry Report */
    TECH_LIVE_FLOATER,          /* Live Floater Cybernetic Control Report */
    TECH_LIVE_MUTON,            /* Live Muton Tactical Hierarchy Report */
    TECH_LIVE_CRYPTID,          /* Live Cryptid Bio-Toxin Containment Report */
    TECH_LIVE_SECTOID_CMDR,     /* Live Sectoid Commander Psionic Interrogation */

    /* Psionic Warfare Tree (Requires Live Sectoid Commander Interrogation) */
    TECH_PSI_LAB,               /* Unlocks constructing the Psi-Lab Base Facility */
    TECH_MIND_PROBE,            /* Unlocks manufacturing & using the Psionic Mind Probe */

    /* UFO Engineering & Hull Systems */
    TECH_UFO_NAVIGATION,        /* UFO Hyper-Wave Tracking & Interception Bonus */
    TECH_UFO_CONSTRUCTION,      /* Unlocks ARV Cargo Rack Expansion & Alloy Fabrication */

    TECH_COUNT
};

typedef struct {
    int id;
    const char *short_name;     /* Compact <= 8 char HUD/button name (e.g. "A.RIFLE") */
    const char *full_name;      /* Full display title (<= 22 chars) */
    int category;               /* ICAT_* */
    int ammo_item_id;           /* Matching ITEM_* ammo ID for weapons (-1 if self/none) */
    int clip_size;              /* Rounds per magazine/battery/cartridge (or charges) */
    int proj_type;              /* 0=Bullet, 1=Laser, 2=Plasma */
    int dmg_base;               /* Base damage per hit (or center explosion/stun/heal) */
    int dmg_rand;               /* Random bonus damage (0..dmg_rand) */
    int blast_radius;           /* Explosion tile radius for grenades (0 for guns) */
    int snap_tu, snap_acc;      /* Snap shot TU cost & accuracy multiplier % */
    int aimed_tu, aimed_acc;    /* Aimed shot TU cost & accuracy multiplier % */
    int auto_tu, auto_acc;      /* Auto 3-round burst TU cost & accuracy % (0 if no auto) */
    int reload_tu;              /* TU cost to reload weapon in combat (2..5) */
    int prime_tu;               /* TU cost to prime grenade in combat (3..4) */
    int req_tech;               /* TECH_* required to equip, buy, or manufacture (-1 = start) */
    int buy_price;              /* Store purchase price in $ (0 = cannot be bought) */
    int sell_price;             /* Store sell value in $ (all items > 0) */
    int delivery_hours;         /* Transit hours when bought from Store before arrival */
    int can_manufacture;        /* 1 = Workshop can build once req_tech is researched */
    int mfg_cash;               /* Workshop cash cost to manufacture 1 unit */
    int mfg_alloys;             /* Alien Alloys required to manufacture 1 unit */
    int mfg_elerium;            /* Elerium-115 required to manufacture 1 unit */
    int mfg_hours;              /* Engineer-hours required to manufacture 1 unit */
    int store_space;            /* Storage units consumed in Base Stores / ARV Cargo */
    int weight;                 /* Tactical encumbrance weight (reduces stamina & scales move TUs) */
} ItemDef;

typedef struct {
    int id;
    const char *title;          /* Research project name (<= 22 chars) */
    int req_tech;               /* Primary prerequisite TECH_* (-1 if none) */
    int req_tech2;              /* Secondary prerequisite TECH_* (-1 if none) */
    int req_item;               /* Required sample ITEM_* in Base Stores (-1 if none) */
    int req_facility;           /* Required built FAC_* at HQ (-1 if none) */
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
extern uint8_t spr_base_tiles[13][BASE_TILE_W * BASE_TILE_H];
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

