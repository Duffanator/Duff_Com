#ifndef CAMPAIGN_H
#define CAMPAIGN_H

#include "operative.h"

#define MAX_RADAR_EVENTS 3

typedef struct {
    int active;             /* 1 = live radar contact on Lehigh Valley map */
    int event_num;          /* Sequential dispatch ID (#01, #02, ...) */
    int biome_id;           /* 0=Rte309, 1=PA Farm, 2=Suburb, 3=Bethlehem Steel */
    int severity;           /* 0=Crash Site (Green), 1=Landed UFO (Gold), 2=Alien Raid (Red) */
    int rx, ry;             /* Screen coordinates on full-width radar (10..309, 30..161) */
    int age_ticks;          /* Frames since detected */
    int ttl_ticks;          /* Frames until UFO departs if ignored */
    char tag[12];           /* Short radar callsign: "UFO-01", "RAID-02", "CRASH-03" */
    const char *type_title; /* e.g. "LANDED SCOUT", "TERROR SITE", "CRASHED CRUISER" */
    const char *loc_name;   /* e.g. "COOPERSBURG RTE 309", "QUAKERTOWN PA FARM" */
    const char *intel_desc; /* Local emergency report (<= 33 chars) */
} RadarEvent;

typedef struct {
    int mode;               /* 0 = Command Center (Radar/Bunker/Armory), 1 = Tactical Battlescape */
    int active_tab;         /* 0 = Lehigh Valley Radar, 1 = Coopersburg Bunker, 2 = Squad Armory */
    int selected_sector;    /* 0..3 (Biome 0..3 of launched mission) */
    int deploy_night;       /* 0 = Day Mission, 1 = Night Mission */
    int missions_won;
    int total_kills;
    int total_kia;          /* Career fallen operatives (permadeath memorial count) */
    int elerium;
    int alloys;
    int nav_units;          /* Recovered intact UFO Navigation / Flight Computers */
    int cryo_pods;          /* Recovered intact Alien Cryo-Stasis Chambers */
    int tech_pods;          /* Recovered intact Alien Tech / Ordnance Pods */
    int alien_corpses;      /* Recovered Alien Corpses in HQ Vault */
    Operative squad[MAX_SQUAD_SIZE];         /* Active 5-agent Skyranger strike squad */
    Operative recruit_pool[MAX_RECRUIT_POOL];/* Ready-to-hire rookie pool for replacements/future barracks */
    int selected_soldier;   /* Highlighted operative in 3.SQUAD ARMORY (0..4) */
    int bunker_grid[3][4];  /* 3 rows x 4 cols underground Coopersburg Bunker facilities */
    char notice[48];

    /* Full-Width Lehigh Valley Radar & Random Alien Event System */
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
} CampaignState;

extern CampaignState g_camp;

void campaign_init(void);
void campaign_sync_squad_units(void);
void campaign_tick(void);
void campaign_on_mission_complete(void);
void campaign_render(void);
void campaign_handle_input(void);

#endif /* CAMPAIGN_H */
