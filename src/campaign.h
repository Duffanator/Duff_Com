#ifndef CAMPAIGN_H
#define CAMPAIGN_H

#include "operative.h"
#include "items.h"

#define MAX_RADAR_EVENTS 3

/* Facility IDs for the Coopersburg Colonial House Base (3x4 grid) */
enum {
    FAC_EMPTY_LOT = 0,      /* Grass Yard Lot (click if connected to build addon) */
    FAC_MAIN_HOUSE,         /* Center Tile [1][1]: Main Colonial House HQ */
    FAC_GARAGE,             /* Starting Tile: Armored Response Vehicle Garage */
    FAC_RADAR,              /* Starting Tile: Homemade Radar Unit */
    FAC_LAB,                /* Starting Tile: Homemade Lab / Workshop */
    FAC_STORES,             /* Addon: Equipment Stores (+40 Store Space) */
    FAC_QUARTERS,           /* Addon: Living Quarters & Medbay (+15 Squad HP) */
    FAC_CONTAINMENT,        /* Addon: Alien Containment Area (+Autopsy Speed) */
    FAC_UPG_LAB,            /* Addon: Upgraded Research Lab (+2 Scientists) */
    FAC_UPG_WORKSHOP,       /* Addon: Upgraded Workshop (Alloy/Elerium Fab) */
    FAC_MISSILE_DEF,        /* Addon: Missile Defense System (UFO Interception) */
    FAC_OFFENSE_SYS,        /* Addon: Heavy Offensive UFO System (High-Dmg AA) */
    FAC_COUNT
};

/* Compatibility aliases */
#define FAC_DIRT        FAC_EMPTY_LOT
#define FAC_LIFT        FAC_MAIN_HOUSE
#define FAC_HANGAR      FAC_GARAGE
#define FAC_ARCADE_REC  FAC_MAIN_HOUSE
#define FAC_MEDBAY      FAC_QUARTERS
#define FAC_PLASMA_WORK FAC_STORES
#define FAC_TURRET      FAC_MISSILE_DEF


typedef struct {
    int active;             /* 1 = live radar contact on Lehigh Valley map */
    int event_num;          /* Sequential dispatch ID (#01, #02, ...) */
    int biome_id;           /* 0=Rte309, 1=PA Farm, 2=Suburb, 3=Bethlehem Steel */
    int severity;           /* 0=Crash Site (Green), 1=Landed UFO (Gold), 2=Alien Raid (Red) */
    int ufo_size;           /* 0=Small Scout, 1=Medium Cruiser, 2=Large Assault Ship */
    int is_flying;          /* 1=Flying overhead on radar, 0=Landed or Crashed on ground */
    int ufo_hp, ufo_hp_max; /* Hull integrity for UFO interception mini-game */
    int vx, vy;             /* Velocity vector when flying overhead on radar */
    int flight_mins;        /* Campaign minutes remaining before landing or X-COM shootdown */
    int rx, ry;             /* Screen coordinates on radar */

    int age_ticks;          /* Frames since detected */
    int ttl_ticks;          /* Frames until UFO departs if ignored */
    char tag[12];           /* Short radar callsign: "UFO-01", "RAID-02", "CRASH-03" */
    const char *type_title; /* e.g. "LANDED SCOUT", "TERROR SITE", "CRASHED CRUISER" */
    const char *loc_name;   /* e.g. "COOPERSBURG RTE 309", "QUAKERTOWN PA FARM" */
    const char *intel_desc; /* Local emergency report (<= 33 chars) */
} RadarEvent;

typedef struct {
    int mode;               /* 0 = Command Center (Radar/Bunker/Armory), 1 = Tactical Battlescape */
    int active_tab;         /* 0 = Lehigh Valley Radar, 1 = Colonial House Base, 2 = Squad Armory */
    int time_speed;         /* 0 = 1S (Realtime), 1 = 5M/sec, 2 = 1H/sec, 3 = 1D/sec */
    int selected_sector;    /* 0..3 (Biome 0..3 of launched mission), 4 = Home Base Defense */
    int deploy_night;       /* 0 = Day Mission, 1 = Night Mission */
    int missions_won;
    int win_streak;         /* Consecutive victories (increases rare Home Base Defense chance) */
    int total_kills;
    int total_kia;          /* Career fallen operatives (permadeath memorial count) */

    /* Phase 1: Money, Scientists, Stores, ARV Cargo & Research State */
    int money;              /* Duff-COM funds in dollars ($12,000 at start) */
    int scientists;         /* Total scientists at base (starts at 2) */
    int active_tech;        /* Currently researched TECH_* topic (-1 if none) */
    uint8_t tech_done[TECH_COUNT];  /* 1 = Research completed & logged */
    int tech_prog[TECH_COUNT];      /* Accumulated scientist-hours per topic */
    uint8_t item_seen[ITEM_COUNT];  /* 1 = Item has been collected at least once or researched */
    int stores[ITEM_COUNT];         /* Base Equipment Store inventory counts */

    int arv_cargo[ITEM_COUNT];      /* Weapons/Ammo/Gear loaded into Armored Response Vehicle */
    int arv_turret_level;           /* 0 = None, 1 = Mk1 Roof Auto-Turret, 2 = Mk2 Upgraded Turret */

    int elerium;
    int alloys;
    int nav_units;          /* Recovered intact UFO Navigation / Flight Computers */
    int cryo_pods;          /* Recovered intact Alien Cryo-Stasis Chambers */
    int tech_pods;          /* Recovered intact Alien Tech / Ordnance Pods */
    int alien_corpses;      /* Recovered Alien Corpses in HQ Vault */
    Operative squad[MAX_SQUAD_SIZE];         /* Active 5-agent strike squad */
    Operative recruit_pool[MAX_RECRUIT_POOL];/* Ready-to-hire rookie pool for replacements/barracks */
    MemorialEntry memorial[MAX_MEMORIAL];    /* Persistent KIA Memorial Wall */
    int memorial_count;
    int selected_soldier;   /* Highlighted operative in 3.SQUAD ARMORY (0..4) */
    int bunker_grid[3][4];  /* 3 rows x 4 cols Coopersburg Colonial House Base grid */
    char notice[48];

    /* Lehigh Valley Radar & Random Alien Event System */
    RadarEvent events[MAX_RADAR_EVENTS];
    int next_event_num;
    int selected_event;     /* Index in events[0..2] currently selected/launched (-1 if none) */
    int launched_event;     /* Index in events[0..2] currently in progress (-1 if none) */
    int launch_prompt_open; /* 1 = Confirmation modal asking to launch mission is open */
    int radar_tick;         /* Animation tick for CRT sweep beam & pulsing target rings */
    int next_spawn_timer;   /* Countdown frames until next random Alien Event report */
    int report_banner_timer;/* Countdown frames for flashing incoming alert bar */
    int last_reported_idx;  /* Most recently reported event slot */
    int valley_day;         /* Campaign day counter (1, 2, ...) */
    int valley_minute;      /* Minutes since midnight (0..1439) */

    /* Phase 2: Base Offensive UFO Shoot-Down Mini-Game State */
    int intercept_open;     /* 1 = CRT UFO Interception Mini-Game modal active */
    int intercept_ev;       /* Index in events[0..2] currently being engaged */
    int intercept_tx;       /* UFO X position inside targeting scope (40..200) */
    int intercept_ty;       /* UFO Y position inside targeting scope (36..126) */
    int intercept_vx;       /* UFO horizontal evasion speed */
    int intercept_vy;       /* UFO vertical evasion speed */
    int intercept_cooldown; /* Frames until base offensive battery can fire again */
    int intercept_timer;    /* Frames remaining before UFO escapes engagement range */
    int intercept_shot_t;   /* Animation frames for active anti-air bolt/missile */
    int intercept_shot_x;
    int intercept_shot_y;
    int intercept_flash;    /* Impact/explosion flash timer */

    /* Phase 3: Colonial House Base Sub-Modals */
    int base_modal;         /* 0=Overview, 1=Construct, 2=Stores/ARV, 3=Buy/Sell Store, 4=Research/Log/Mfg */
    int build_r, build_c;   /* Target (row, col) for FAC_EMPTY_LOT construction menu */
    int store_scroll;       /* Scroll offset in Stores / Buy-Sell lists */
    int lab_subtab;         /* 0=Assign Research, 1=Research Log, 2=Workshop Mfg */
    int log_sel;            /* Selected completed tech in Research Log */
    int armory_subtab;      /* 0=Single Operative Loadout, 1=ARV Bus Cargo, 2=KIA Memorial */
    int game_over_reset;    /* 1=Alien Victory on Base Defense reset game to Title Screen */
} CampaignState;





extern CampaignState g_camp;

void campaign_init(void);
void campaign_sync_squad_units(void);
void campaign_tick(void);
void campaign_on_mission_complete(void);
void campaign_render(void);
void campaign_handle_input(void);

/* Phase 1 Helper Queries for Research, Stores & ARV Capacity */
int  campaign_is_item_researched(int item_id);
int  campaign_is_item_discovered(int item_id);
int  campaign_store_used_space(void);

int  campaign_store_max_space(void);
int  campaign_arv_used_space(void);
int  campaign_arv_max_space(void);

/* Phase 6: Persistent Save / Load Game System */
int  campaign_save_game(const char *filename);
int  campaign_load_game(const char *filename);

#endif /* CAMPAIGN_H */


