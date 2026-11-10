#ifndef OPERATIVE_H
#define OPERATIVE_H

#include <stddef.h>
#include <stdint.h>

#define MAX_SQUAD_SIZE     5
#define MAX_RECRUIT_POOL   8   /* Ready for HQ "Hire Operatives" Barracks UI */
#define MAX_MEMORIAL       64  /* Persistent KIA Memorial Wall capacity */
#define OP_NAME_LEN        16  /* Base operative callsign/surname without rank prefix */
#define OP_FULLNAME_MAX    15  /* Max visible characters for "<RANK> <NAME>" in HUD/Armory */

/* Classic 1994 X-COM Operative Rank Ladder (lowest to highest) */
enum {
    RANK_ROOKIE = 0,
    RANK_SQUADDIE,
    RANK_SERGEANT,
    RANK_CAPTAIN,
    RANK_COLONEL,
    RANK_COMMANDER,
    RANK_COUNT
};

/* Experience Point (XP) Awards */
#define XP_COMBAT_HIT       10  /* Landing a weapon/grenade hit on a living Alien */
#define XP_THROW_SUCCESS    6   /* Accurate field throw of weapon/ammo/grenade */
#define XP_KILL_SECTOID     20  /* Eliminating a Sectoid */
#define XP_KILL_FLOATER     25  /* Eliminating a Floater */
#define XP_KILL_MUTON       35  /* Eliminating a Muton */
#define XP_MISSION_VICTORY  45  /* Surviving a victorious mission */
#define XP_NIGHT_BONUS      10  /* Extra survival XP for Night Ops victory */
#define XP_MISSION_RETREAT  15  /* Surviving a contested tactical retreat */

typedef struct {
    int active;             /* 1 = active operative, 0 = empty slot */
    char name[OP_NAME_LEN]; /* Clean callsign/surname without rank prefix (e.g. "KELLER") */
    int gender;             /* 0 = Male, 1 = Female */
    int portrait_id;        /* 0..5 Male portraits, 6..11 Female portraits */
    int weapon_id;          /* ITEM_* primary weapon ID (starts with ITEM_ASSAULT_RIFLE or ITEM_PISTOL) */
    int sidearm_id;         /* Optional secondary weapon ID (e.g. ITEM_PISTOL or ITEM_NONE) */
    int extra_clips;        /* Number of spare magazines/batteries/cartridges carried (0..4) */
    int grenade_id;         /* ITEM_FRAG_GRENADE, ITEM_HEAVY_GRENADE, or ITEM_ALIEN_GRENADE */
    int grenades;           /* Number of grenades carried (0..4) */
    int flares;             /* Number of night electro-flares carried (0..3) */

    int rank;               /* RANK_ROOKIE (0) .. RANK_COMMANDER (5) */
    int xp;                 /* Cumulative experience points */
    int kills;              /* Career confirmed alien kills */
    int missions;           /* Career missions completed/survived */

    /* Core Operative Stats (increase permanently with each rank promotion!) */
    int tu_max;             /* Base Max Time Units (before HQ Arcade bonus) */
    int hp_max;             /* Base Max Health (before HQ Medbay bonus) */
    int energy_max;         /* Base Max Stamina / Energy */
    int firing_acc;         /* Base Aim / Firing Accuracy % */
    int throw_acc;          /* Base Throwing Accuracy % (for grenades & throwing gear) */
    int reactions;          /* Base Reaction Fire initiative */
} Operative;

/* Persistent Fallen Operative Memorial Record */
typedef struct {
    int active;
    char name[OP_NAME_LEN];
    int rank;
    int gender;
    int portrait_id;
    int kills;
    int missions;
    int day_kia;            /* Campaign day fallen */
    char cause[20];         /* e.g. "KIA IN ACTION" or "MIA (LEFT BEHIND)" */
} MemorialEntry;

/* Rank titles, prefixes & XP thresholds */
const char *operative_rank_name(int rank);
const char *operative_rank_prefix(int rank);
const char *operative_rank_short(int rank);
int         operative_rank_xp_req(int rank);
int         operative_next_rank_xp(int rank);
int         operative_rank_for_xp(int xp);
int         operative_kill_xp(int alien_unit_type);

/* Formatting & Roster Name Sanitization */
const char *operative_strip_rank_prefix(const char *raw_name);
void        operative_format_full_name(const Operative *op, char *out, size_t out_sz);

/* Lifecycle, Promotions & Recruit Pool / Permadeath Replacement */
void        operative_init_rookie(Operative *op, const char *raw_name,
                                  int gender, int portrait_id, int weapon_id);
int         operative_add_xp(Operative *op, int xp_gain);
void        operative_load_starting_squad(Operative *squad, int squad_count);
void        operative_refresh_recruit_pool(Operative *pool, int pool_count,
                                           const Operative *squad, int squad_count);
void        operative_hire_replacement(Operative *dst_slot,
                                       Operative *pool, int pool_count,
                                       const Operative *squad, int squad_count);
void        operative_record_memorial(MemorialEntry *memorial, int max_entries,
                                      const Operative *fallen, int day_kia, const char *cause);

/* 9x7 Mode 13h Pixel-Art Rank Insignia Badge Renderer */
void        operative_draw_rank_badge(int x, int y, int rank);

#endif /* OPERATIVE_H */

