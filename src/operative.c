#include "operative.h"
#include "items.h"
#include "vga.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *rank_names[RANK_COUNT] = {
    "ROOKIE",
    "SQUADDIE",
    "SERGEANT",
    "CAPTAIN",
    "COLONEL",
    "COMMANDER"
};

static const char *rank_prefixes[RANK_COUNT] = {
    "RKI.",
    "SQD.",
    "SGT.",
    "CPT.",
    "COL.",
    "CMDR."
};

static const char *rank_shorts[RANK_COUNT] = {
    "RKI",
    "SQD",
    "SGT",
    "CPT",
    "COL",
    "CMD"
};

/* Cumulative XP required to reach each Classic 1994 X-COM Rank */
static const int rank_xp_reqs[RANK_COUNT] = {
    0,      /* RANK_ROOKIE    */
    75,     /* RANK_SQUADDIE  */
    180,    /* RANK_SERGEANT  */
    320,    /* RANK_CAPTAIN   */
    500,    /* RANK_COLONEL   */
    720     /* RANK_COMMANDER */
};

/* Permanent stat increases granted upon promotion into Rank r (1..5) */
static const int rank_gain_tu[RANK_COUNT]  = { 0, 4, 4, 5, 5, 6 }; /* +24 TU total  */
static const int rank_gain_aim[RANK_COUNT] = { 0, 6, 6, 6, 6, 6 }; /* +30 AIM total */
static const int rank_gain_thr[RANK_COUNT] = { 0, 5, 5, 5, 5, 6 }; /* +26 THR total */
static const int rank_gain_hp[RANK_COUNT]  = { 0, 4, 4, 5, 5, 7 }; /* +25 HP total  */
static const int rank_gain_en[RANK_COUNT]  = { 0, 5, 5, 5, 6, 8 }; /* +29 EN total  */
static const int rank_gain_rcn[RANK_COUNT] = { 0, 6, 6, 7, 7, 9 }; /* +35 RCN total */

static const char *default_starting_names[MAX_SQUAD_SIZE] = {
    "KELLER",
    "DECKER",
    "HARR",
    "LUCAS",
    "BARBER"
};

static const int default_starting_genders[MAX_SQUAD_SIZE] = {
    0,  /* KELLER: Male   */
    1,  /* DECKER: Female */
    0,  /* HARR:   Male   */
    0,  /* LUCAS:  Male   */
    0   /* BARBER: Male   */
};

/* Expanded pool of 125 classic X-COM / Pennsylvania / 90s sci-fi operative surnames (all <= 9 chars) */
static const char *recruit_name_pool[] = {
    "VASQUEZ",   "HUDSON",    "HICKS",     "RIPLEY",    "BISHOP",
    "DRAKE",     "APONE",     "FROST",     "DIETRICH",  "FERRO",
    "KOVACS",    "NOVAK",     "VANCE",     "MERCER",    "STRYKER",
    "CROSS",     "REZNOR",    "GALLAGHER", "SNYDER",    "MILLER",
    "SCHULTZ",   "ZIMMERMAN", "KOWALSKI",  "BRENNAN",   "CALLAHAN",
    "DELGADO",   "MORALES",   "TANAKA",    "SATO",      "CHEN",
    "VOLKOV",    "SOKOLOV",   "LINDQVIST", "BERGER",    "HOFFMAN",
    "WAGNER",    "STEELE",    "HAWKE",     "ROURKE",    "BOONE",
    "CARTER",    "REYES",     "FOSTER",    "HAYES",     "GORMAN",
    "CROWE",     "WIERZBOWSKI","SPUNKMEYER","BURKE",    "NEWMAN",
    "YODER",     "KAUFFMAN",  "LANDIS",    "DETWEILER", "MOYER",
    "STAUFFER",  "FRETZ",     "BEIDLER",   "SHULER",    "GEISSINGER",
    "KRAUSS",    "KLINE",     "WEAVER",    "HELLER",    "BACHMAN",
    "HARTMAN",   "REINHARD",  "SCHAEFFER", "DIEHL",     "ROTH",
    "TROXELL",   "TREXLER",   "FENSTER",   "DEIBERT",   "ZIEGLER",
    "METZGER",   "LUTZ",      "WERNER",    "BECKER",    "FISCHER",
    "WEBER",     "RICHTER",   "KLEIN",     "WOLF",      "NEUMANN",
    "SCHWARZ",   "BRAUN",     "KRUEGER",   "KOCH",      "LEHMANN",
    "O'CONNOR",  "SULLIVAN",  "MURPHY",    "WALSH",     "MCBRIDE",
    "DONOVAN",   "QUINN",     "DOYLE",     "NOLAN",     "PHELAN",
    "ROSSI",     "CONTI",     "GRECO",     "MARINO",    "LOMBARDI",
    "NAKAMURA",  "KOBAYASHI", "YAMAMOTO",  "TAKAHASHI", "WATANABE",
    "LIN",       "HUANG",     "ZHAO",      "CHOI",      "PARK",
    "ORLOV",     "PETROV",    "IVANOV",    "KOZLOV",    "MOROZOV",
    "SLADE",     "DECKARD",   "MACREADY",  "CHILDS",    "PLISSKEN"
};


#define NUM_RECRUIT_NAMES (int)(sizeof(recruit_name_pool) / sizeof(recruit_name_pool[0]))

static int clamp_rank(int rank) {
    if (rank < 0) return 0;
    if (rank >= RANK_COUNT) return RANK_COUNT - 1;
    return rank;
}

static int ci_starts_with_token(const char *str, const char *tok, const char **after_out) {
    size_t i = 0;
    while (tok[i] != '\0') {
        char a = (char)toupper((unsigned char)str[i]);
        char b = (char)toupper((unsigned char)tok[i]);
        if (a != b) return 0;
        i++;
    }
    if (str[i] == '.') i++;
    if (str[i] != ' ' && str[i] != '\t') return 0;
    while (str[i] == ' ' || str[i] == '\t') i++;
    if (str[i] == '\0') return 0; /* Never strip if nothing follows the prefix */
    *after_out = str + i;
    return 1;
}

const char *operative_strip_rank_prefix(const char *raw_name) {
    static const char *known_prefixes[] = {
        "COMMANDER", "CMDR", "CMD",
        "COLONEL",   "COL",
        "CAPTAIN",   "CAPT", "CPT",
        "MAJOR",     "MAJ",
        "LIEUTENANT","LT",
        "SERGEANT",  "SGT",
        "CORPORAL",  "CPL",
        "SPECIALIST","SPEC", "SPC",
        "SQUADDIE",  "SQD",
        "PRIVATE",   "PVT",  "PFC",
        "ROOKIE",    "RKI",
        "GEN"
    };
    const char *p = raw_name ? raw_name : "";
    unsigned k;

    while (*p == ' ' || *p == '\t') p++;
    for (k = 0; k < sizeof(known_prefixes) / sizeof(known_prefixes[0]); k++) {
        const char *after = NULL;
        if (ci_starts_with_token(p, known_prefixes[k], &after)) {
            return after;
        }
    }
    return p;
}

const char *operative_rank_name(int rank) {
    return rank_names[clamp_rank(rank)];
}

const char *operative_rank_prefix(int rank) {
    return rank_prefixes[clamp_rank(rank)];
}

const char *operative_rank_short(int rank) {
    return rank_shorts[clamp_rank(rank)];
}

int operative_rank_xp_req(int rank) {
    return rank_xp_reqs[clamp_rank(rank)];
}

int operative_next_rank_xp(int rank) {
    if (rank < 0) return rank_xp_reqs[1];
    if (rank >= RANK_COMMANDER) return rank_xp_reqs[RANK_COMMANDER];
    return rank_xp_reqs[rank + 1];
}

int operative_rank_for_xp(int xp) {
    int r = RANK_ROOKIE;
    while (r + 1 < RANK_COUNT && xp >= rank_xp_reqs[r + 1]) {
        r++;
    }
    return r;
}

int operative_kill_xp(int alien_unit_type) {
    if (alien_unit_type == 3) return XP_KILL_MUTON;
    if (alien_unit_type == 2) return XP_KILL_FLOATER;
    return XP_KILL_SECTOID;
}

void operative_format_full_name(const Operative *op, char *out, size_t out_sz) {
    int max_len;
    if (!out || out_sz == 0) return;
    if (!op) {
        out[0] = '\0';
        return;
    }
    max_len = ((int)out_sz - 1 < OP_FULLNAME_MAX) ? ((int)out_sz - 1) : OP_FULLNAME_MAX;
    snprintf(out, out_sz, "%s %s", operative_rank_prefix(op->rank), op->name);
    if ((int)strlen(out) > max_len) {
        out[max_len] = '\0';
    }
}

void operative_init_rookie(Operative *op, const char *raw_name,
                           int gender, int portrait_id, int weapon_id) {
    const char *clean;
    size_t i = 0;

    if (!op) return;
    memset(op, 0, sizeof(*op));
    op->active = 1;
    op->gender = gender ? 1 : 0;
    op->portrait_id = portrait_id;
    /* Default starting weapon is always a researched human ballistic weapon */
    op->weapon_id   = (weapon_id >= 0 && item_is_weapon(weapon_id)) ? weapon_id : ITEM_ASSAULT_RIFLE;
    op->sidearm_id  = ITEM_PISTOL;
    op->extra_clips = 2;
    op->grenade_id  = ITEM_FRAG_GRENADE;
    op->grenades    = 2;
    op->flares      = 2;
    op->rank = RANK_ROOKIE;
    op->xp = 0;
    op->kills = 0;
    op->missions = 0;

    clean = operative_strip_rank_prefix(raw_name);
    while (clean[i] != '\0' && clean[i] != '\r' && clean[i] != '\n' && i + 1 < sizeof(op->name)) {
        op->name[i] = (char)toupper((unsigned char)clean[i]);
        i++;
    }
    while (i > 0 && (op->name[i - 1] == ' ' || op->name[i - 1] == '\t')) {
        i--;
    }
    op->name[i] = '\0';
    if (op->name[0] == '\0') {
        strncpy(op->name, "RECRUIT", sizeof(op->name) - 1);
    }

    /* Classic X-COM Rookie baseline stats with slight individual talent variation */
    op->tu_max     = 52 + (rand() % 3); /* 52..54 Base TU (58..60 with HQ Arcade Rec Room) */
    op->hp_max     = 40 + (rand() % 4); /* 40..43 Base HP (55..58 with HQ Medbay)          */
    op->energy_max = 55 + (rand() % 4); /* 55..58 Stamina                                  */
    op->firing_acc = 56 + (rand() % 4); /* 56..59% Base Aim                                */
    op->throw_acc  = 54 + (rand() % 6); /* 54..59% Base Throwing Accuracy                  */
    op->reactions  = 44 + (rand() % 5); /* 44..48 Reaction Initiative                      */
}

int operative_add_xp(Operative *op, int xp_gain) {
    int promotions = 0;
    if (!op || !op->active || xp_gain <= 0) return 0;

    op->xp += xp_gain;
    while (op->rank < RANK_COMMANDER && op->xp >= rank_xp_reqs[op->rank + 1]) {
        op->rank++;
        op->tu_max     += rank_gain_tu[op->rank];
        op->firing_acc += rank_gain_aim[op->rank];
        op->throw_acc  += rank_gain_thr[op->rank];
        op->hp_max     += rank_gain_hp[op->rank];
        op->energy_max += rank_gain_en[op->rank];
        op->reactions  += rank_gain_rcn[op->rank];
        if (op->firing_acc > 99) op->firing_acc = 99;
        if (op->throw_acc  > 99) op->throw_acc  = 99;
        promotions++;
    }
    return promotions;
}

void operative_load_starting_squad(Operative *squad, int squad_count) {
    int male_pool[6] = { 0, 1, 2, 3, 4, 5 };
    int fem_pool[6]  = { 6, 7, 8, 9, 10, 11 };
    int m_idx = 0, f_idx = 0, i;

    if (!squad || squad_count <= 0) return;

    /* Shuffle male & female portrait pools so each new game gives random gender-matched faces */
    for (i = 5; i > 0; i--) {
        int jm = rand() % (i + 1);
        int jf = rand() % (i + 1);
        int tm = male_pool[i]; male_pool[i] = male_pool[jm]; male_pool[jm] = tm;
        int tf = fem_pool[i];  fem_pool[i]  = fem_pool[jf];  fem_pool[jf]  = tf;
    }

    /* Starting operatives at the start of a new game are ALWAYS:
     * 0: KELLER (Male, random male face)
     * 1: DECKER (Female, random female face)
     * 2: HARR   (Male, random male face)
     * 3: LUCAS  (Male, random male face)
     * 4: BARBER (Male, random male face)
     * And all start with human Assault Rifles, Pistols & Frag Grenades! */
    for (i = 0; i < squad_count; i++) {
        int idx  = i % MAX_SQUAD_SIZE;
        int g    = default_starting_genders[idx];
        int port = (g == 0) ? male_pool[m_idx++ % 6] : fem_pool[f_idx++ % 6];
        operative_init_rookie(&squad[i], default_starting_names[idx], g, port, ITEM_ASSAULT_RIFLE);
    }
}


static int name_in_use(const char *cand,
                       const Operative *squad, int squad_count,
                       const Operative *pool,  int pool_count) {
    int i;
    for (i = 0; i < squad_count; i++) {
        if (squad && squad[i].active && strcmp(squad[i].name, cand) == 0) return 1;
    }
    for (i = 0; i < pool_count; i++) {
        if (pool && pool[i].active && strcmp(pool[i].name, cand) == 0) return 1;
    }
    return 0;
}

static int pick_recruit_portrait(int gender, const Operative *squad, int squad_count) {
    int base = gender ? 6 : 0;
    int start = rand() % 6;
    int k, s;
    for (k = 0; k < 6; k++) {
        int cand = base + ((start + k) % 6);
        int used = 0;
        for (s = 0; s < squad_count; s++) {
            if (squad && squad[s].active && squad[s].portrait_id == cand) {
                used = 1;
                break;
            }
        }
        if (!used) return cand;
    }
    return base + start;
}

static void generate_single_recruit(Operative *out_op,
                                    const Operative *squad, int squad_count,
                                    const Operative *pool,  int pool_count) {
    int start = rand() % NUM_RECRUIT_NAMES;
    const char *chosen_name = recruit_name_pool[start];
    int k;
    int gender = rand() & 1;
    int port = pick_recruit_portrait(gender, squad, squad_count);

    for (k = 0; k < NUM_RECRUIT_NAMES; k++) {
        const char *cand = recruit_name_pool[(start + k) % NUM_RECRUIT_NAMES];
        if (!name_in_use(cand, squad, squad_count, pool, pool_count)) {
            chosen_name = cand;
            break;
        }
    }
    /* Newly hired / replacement Rookies issue standard Assault Rifle */
    operative_init_rookie(out_op, chosen_name, gender, port, ITEM_ASSAULT_RIFLE);
}

void operative_record_memorial(MemorialEntry *memorial, int max_entries,
                               const Operative *fallen, int day_kia, const char *cause) {
    int i, slot = -1;
    if (!memorial || max_entries <= 0 || !fallen) return;

    for (i = 0; i < max_entries; i++) {
        if (!memorial[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        /* Shift oldest entry out if memorial is full */
        for (i = 0; i < max_entries - 1; i++) {
            memorial[i] = memorial[i + 1];
        }
        slot = max_entries - 1;
    }

    memset(&memorial[slot], 0, sizeof(MemorialEntry));
    memorial[slot].active = 1;
    strncpy(memorial[slot].name, fallen->name, OP_NAME_LEN - 1);
    memorial[slot].rank        = fallen->rank;
    memorial[slot].gender      = fallen->gender;
    memorial[slot].portrait_id = fallen->portrait_id;
    memorial[slot].kills       = fallen->kills;
    memorial[slot].missions    = fallen->missions;
    memorial[slot].day_kia     = (day_kia > 0) ? day_kia : 1;
    strncpy(memorial[slot].cause, cause ? cause : "KIA IN ACTION", sizeof(memorial[slot].cause) - 1);
}


void operative_refresh_recruit_pool(Operative *pool, int pool_count,
                                    const Operative *squad, int squad_count) {
    int i;
    if (!pool || pool_count <= 0) return;
    for (i = 0; i < pool_count; i++) {
        if (!pool[i].active) {
            generate_single_recruit(&pool[i], squad, squad_count, pool, pool_count);
        }
    }
}

void operative_hire_replacement(Operative *dst_slot,
                                Operative *pool, int pool_count,
                                const Operative *squad, int squad_count) {
    int i;
    if (!dst_slot) return;

    operative_refresh_recruit_pool(pool, pool_count, squad, squad_count);
    if (pool && pool_count > 0 && pool[0].active) {
        Operative hired = pool[0];
        /* Ensure portrait looks distinct from surviving squadmates if possible */
        dst_slot->active = 0;
        hired.portrait_id = pick_recruit_portrait(hired.gender, squad, squad_count);
        *dst_slot = hired;

        for (i = 0; i < pool_count - 1; i++) {
            pool[i] = pool[i + 1];
        }
        memset(&pool[pool_count - 1], 0, sizeof(Operative));
        generate_single_recruit(&pool[pool_count - 1], squad, squad_count, pool, pool_count);
    } else {
        dst_slot->active = 0;
        generate_single_recruit(dst_slot, squad, squad_count, NULL, 0);
    }
}

/* Render a crisp 9x7 VGA Mode 13h military Rank Insignia patch at (x, y) */
void operative_draw_rank_badge(int x, int y, int rank) {
    int r = clamp_rank(rank);
    uint8_t bg  = (r == RANK_COMMANDER) ? 40 : (r >= RANK_CAPTAIN ? 75 : 14);
    uint8_t bdr = (r == RANK_COMMANDER) ? 99 : (r >= RANK_COLONEL ? 101 : 78);

    vga_rect_fill(x, y, 9, 7, bg);
    vga_hline(x, x + 8, y,     bdr);
    vga_hline(x, x + 8, y + 6, 15);
    vga_vline(x,     y, y + 6, bdr);
    vga_vline(x + 8, y, y + 6, 15);

    switch (r) {
        case RANK_ROOKIE:
            /* Subtle bronze recruit bar */
            vga_hline(x + 3, x + 5, y + 3, 100);
            vga_pset(x + 4, y + 3, 98);
            break;

        case RANK_SQUADDIE:
            /* Single Gold Chevron */
            vga_pset(x + 4, y + 2, 96);
            vga_pset(x + 3, y + 3, 96); vga_pset(x + 5, y + 3, 96);
            vga_pset(x + 2, y + 4, 98); vga_pset(x + 6, y + 4, 98);
            break;

        case RANK_SERGEANT:
            /* Double Stacked Gold Chevrons */
            vga_pset(x + 4, y + 1, 96);
            vga_pset(x + 3, y + 2, 96); vga_pset(x + 5, y + 2, 96);
            vga_pset(x + 2, y + 3, 96); vga_pset(x + 6, y + 3, 96);
            vga_pset(x + 4, y + 3, 98);
            vga_pset(x + 3, y + 4, 98); vga_pset(x + 5, y + 4, 98);
            vga_pset(x + 2, y + 5, 99); vga_pset(x + 6, y + 5, 99);
            break;

        case RANK_CAPTAIN:
            /* Twin Silver-Cyan Officer Railroad Bars */
            vga_vline(x + 2, y + 1, y + 5, 80);
            vga_vline(x + 3, y + 1, y + 5, 82);
            vga_vline(x + 5, y + 1, y + 5, 80);
            vga_vline(x + 6, y + 1, y + 5, 82);
            vga_pset(x + 2, y + 1, 0);  vga_pset(x + 5, y + 1, 0);
            vga_pset(x + 4, y + 2, 83); vga_pset(x + 4, y + 4, 83);
            break;

        case RANK_COLONEL:
            /* Spread Gold Eagle / Diamond Insignia */
            vga_pset(x + 4, y + 1, 0);
            vga_pset(x + 1, y + 2, 96); vga_hline(x + 3, x + 5, y + 2, 96); vga_pset(x + 7, y + 2, 96);
            vga_hline(x + 2, x + 6, y + 3, 96);
            vga_pset(x + 4, y + 3, 0);
            vga_hline(x + 3, x + 5, y + 4, 98);
            vga_pset(x + 4, y + 5, 99);
            break;

        case RANK_COMMANDER:
            /* Radiant Gold Commander Star on Crimson Backing */
            vga_pset(x + 4, y + 1, 96);
            vga_hline(x + 2, x + 6, y + 2, 96);
            vga_hline(x + 3, x + 5, y + 3, 96);
            vga_pset(x + 4, y + 3, 0);
            vga_pset(x + 2, y + 4, 96); vga_pset(x + 4, y + 4, 98); vga_pset(x + 6, y + 4, 96);
            vga_pset(x + 2, y + 5, 98); vga_pset(x + 6, y + 5, 98);
            break;
    }
}
