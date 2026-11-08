#include "campaign.h"
#include "vga.h"
#include "input.h"
#include "sound.h"
#include "settings.h"
#include "gfx_assets.h"
#include "isomap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CampaignState g_camp;

/* Coopersburg HQ screen position on the full-width 300x132 CRT radar (offset 10, 30) */
#define HQ_RADAR_X  168
#define HQ_RADAR_Y  112

/* Facility IDs for the Coopersburg Underground Bunker (3x4 grid) */
enum {
    FAC_DIRT = 0,
    FAC_LIFT,
    FAC_HANGAR,
    FAC_RADAR,
    FAC_ARCADE_REC,
    FAC_LAB,
    FAC_MEDBAY,
    FAC_PLASMA_WORK,
    FAC_TURRET
};

static const char *fac_names[] = {
    "EXCAVATE",
    "ACCESS LIFT",
    "SKYRANGER",
    "RADAR ARRAY",
    "ARCADE ROOM",
    "LASER LAB",
    "MEDBAY +HP",
    "PLASMA LAB",
    "DEF TURRET"
};

typedef struct {
    const char *loc_name;
    int map_x, map_y; /* Coordinates within the 300x132 Lehigh Valley map */
} ValleySite;

typedef struct {
    int severity;           /* 0=Crash (Lime), 1=Landed UFO (Gold), 2=Alien Raid (Red) */
    const char *prefix;     /* Callsign prefix: "SITE", "UFO", "RAID" */
    const char *type_title; /* Headline in report modal */
    const char *intel_desc; /* Dispatch summary (<= 33 chars) */
} IncidentTemplate;

/* 4 Real-World Lehigh Valley Sub-Locations per Biome across the 300x132 Radar Map */
static const ValleySite valley_sites[4][4] = {
    /* BIOME 0: Route 309 Commercial / Diner & Arcade Corridor */
    {
        { "COOPERSBURG RTE 309", 154,  74 },
        { "CENTER VALLEY 309",   162,  58 },
        { "SOUTH ALLENTOWN 309", 128,  44 },
        { "RICHLAND 309 PLAZA",  166,  94 }
    },
    /* BIOME 1: Rural Pennsylvania Farmland & Red Barn */
    {
        { "QUAKERTOWN PA FARM",  148, 106 },
        { "PLEASANT VLY BARN",   218,  82 },
        { "UPPER MILFORD FARM",   72,  92 },
        { "NOCKAMIXON PA FARM",  242,  98 }
    },
    /* BIOME 2: Residential Suburbs & Suburban Arcade House */
    {
        { "COOPERSBURG SUBURB",  130,  82 },
        { "EMMAUS SOUTH HILLS",   94,  64 },
        { "MACUNGIE BOROUGH",     48,  76 },
        { "SAUCON VLY ESTATES",  192,  66 }
    },
    /* BIOME 3: Bethlehem Steel & Lehigh River Industrial Belt */
    {
        { "BETH. STEEL RUINS",   190,  34 },
        { "SOUTH SIDE FOUNDRY",  170,  42 },
        { "HELLERTOWN SLAG YD",  208,  50 },
        { "EASTON RAIL & MILL",  254,  28 }
    }
};

/* 4 Randomized Alien Event Templates per Biome */
static const IncidentTemplate incident_templates[4][4] = {
    /* BIOME 0: Route 309 */
    {
        { 2, "RAID", "TERROR SITE",    "SECTOIDS AT CHROME DINER & ARCADE" },
        { 1, "UFO",  "LANDED SCOUT",   "SAUCER PARKED ON ROUTE 309 LANES"  },
        { 0, "SITE", "CRASHED UFO",    "DOWNED CRUISER BY DUFF'S ARCADE"   },
        { 2, "RAID", "HIGHWAY RAID",   "PA TROOPERS PINNED ON ROUTE 309"   }
    },
    /* BIOME 1: PA Farm */
    {
        { 0, "SITE", "CRASHED SCOUT",  "BURNING UFO IN PA CORNFIELD"       },
        { 2, "RAID", "FARM INCURSION", "FLOATERS SPOTTED IN BARN HAYLOFT"  },
        { 1, "UFO",  "LANDED CRUISER", "UFO HARVESTING BY RED PA BARN"     },
        { 1, "UFO",  "BARN LANDING",   "PLASMA GLOW INSIDE PA DAIRY BARN"  }
    },
    /* BIOME 2: Suburbs */
    {
        { 2, "RAID", "SUBURBAN RAID",  "ALIENS IN HOMES & ARCADE BASEMENT" },
        { 0, "SITE", "CRASHED SCOUT",  "SMOLDERING SAUCER ON CUL-DE-SAC"   },
        { 1, "UFO",  "POWER DRAIN",    "ARCADE GRID SURGE - UFOS SIGHTED"  },
        { 2, "RAID", "NIGHT STALKERS", "911 CALLS: SECTOIDS IN BACKYARDS"  }
    },
    /* BIOME 3: Bethlehem Steel */
    {
        { 1, "UFO",  "LANDED CRUISER", "MUTON FORCE AT BLAST FURNACES"     },
        { 1, "UFO",  "ALLOY HARVEST",  "UFO SIPHONING MILL POWER GRID"     },
        { 2, "RAID", "HEAVY ASSAULT",  "PLASMA FIRE ON FURNACE CATWALKS"   },
        { 0, "SITE", "CRASHED CRUISER","DOWNED MOTHERSHIP IN SLAG YARD"    }
    }
};

/* 32-direction circular vector table (radius ~140) for rotating CRT radar sweep */
static const int16_t sweep_dx[32] = {
      0,  27,  54,  78,  99, 116, 129, 137,
    140, 137, 129, 116,  99,  78,  54,  27,
      0, -27, -54, -78, -99,-116,-129,-137,
   -140,-137,-129,-116, -99, -78, -54, -27
};
static const int16_t sweep_dy[32] = {
   -140,-137,-129,-116, -99, -78, -54, -27,
      0,  27,  54,  78,  99, 116, 129, 137,
    140, 137, 129, 116,  99,  78,  54,  27,
      0, -27, -54, -78, -99,-116,-129,-137
};

static int rrange_camp(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (rand() % (hi - lo + 1));
}

static int is_valley_night(void) {
    int hr = (g_camp.valley_minute / 60) % 24;
    return (hr < 6 || hr >= 20);
}

static int count_active_events(void) {
    int i, cnt = 0;
    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        if (g_camp.events[i].active) cnt++;
    }
    return cnt;
}

static int first_active_event(void) {
    int i;
    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        if (g_camp.events[i].active) return i;
    }
    return -1;
}

/* Compute screen-space label & click hitbox for an active radar contact */
static void event_hit_box(const RadarEvent *ev, int *lx_out, int *ly_out,
                          int *x0, int *y0, int *x1, int *y1) {
    int tw = (int)strlen(ev->tag) * 6;
    int lx = (ev->rx > 246) ? (ev->rx - tw - 6) : (ev->rx + 7);
    int ly = ev->ry - 3;

    if (lx_out) *lx_out = lx;
    if (ly_out) *ly_out = ly;

    *x0 = (lx < ev->rx - 8) ? (lx - 2) : (ev->rx - 8);
    *x1 = (lx + tw + 2 > ev->rx + 8) ? (lx + tw + 2) : (ev->rx + 8);
    *y0 = ev->ry - 8;
    *y1 = ev->ry + 8;
}

/* Spawn a new randomized Alien Event on the Lehigh Valley Radar! */
static int spawn_random_radar_event(int play_sfx) {
    int slot = -1, i, attempt;
    int biome_used[4] = { 0, 0, 0, 0 };
    int chosen_biome = 0, chosen_site = 0, chosen_inc = 0;
    int rx = 140, ry = 80;
    RadarEvent *ev;

    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        if (!g_camp.events[i].active) {
            if (slot < 0) slot = i;
        } else {
            biome_used[g_camp.events[i].biome_id & 3] = 1;
        }
    }
    if (slot < 0) return -1;

    /* Pick biome + Lehigh Valley site with clear separation from existing blips & HQ */
    for (attempt = 0; attempt < 24; attempt++) {
        int b = rand() & 3;
        int s = rand() & 3;
        int cand_x, cand_y, ok = 1;

        if (attempt < 14 && biome_used[b]) continue;

        cand_x = 10 + valley_sites[b][s].map_x + rrange_camp(-5, 5);
        cand_y = 30 + valley_sites[b][s].map_y + rrange_camp(-4, 4);
        if (cand_x < 28)  cand_x = 28;
        if (cand_x > 282) cand_x = 282;
        if (cand_y < 50)  cand_y = 50;
        if (cand_y > 141) cand_y = 141;

        if (abs(cand_x - HQ_RADAR_X) < 16 && abs(cand_y - HQ_RADAR_Y) < 12) ok = 0;
        for (i = 0; i < MAX_RADAR_EVENTS && ok; i++) {
            if (g_camp.events[i].active) {
                int d = abs(cand_x - g_camp.events[i].rx) + abs(cand_y - g_camp.events[i].ry);
                if (d < 42) ok = 0;
            }
        }

        chosen_biome = b;
        chosen_site  = s;
        rx = cand_x;
        ry = cand_y;
        if (ok) break;
    }

    chosen_inc = rand() & 3;
    g_camp.next_event_num++;

    ev = &g_camp.events[slot];
    memset(ev, 0, sizeof(*ev));
    ev->active     = 1;
    ev->event_num  = g_camp.next_event_num;
    ev->biome_id   = chosen_biome;
    ev->severity   = incident_templates[chosen_biome][chosen_inc].severity;
    ev->rx         = rx;
    ev->ry         = ry;
    ev->age_ticks  = 0;
    ev->ttl_ticks  = rrange_camp(2600, 3800); /* ~37 to 54 seconds on radar */
    ev->type_title = incident_templates[chosen_biome][chosen_inc].type_title;
    ev->loc_name   = valley_sites[chosen_biome][chosen_site].loc_name;
    ev->intel_desc = incident_templates[chosen_biome][chosen_inc].intel_desc;

    snprintf(ev->tag, sizeof(ev->tag), "%s-%02u",
             incident_templates[chosen_biome][chosen_inc].prefix,
             (unsigned)(ev->event_num % 100));

    g_camp.last_reported_idx   = slot;
    g_camp.report_banner_timer = 260;
    if (g_camp.selected_event < 0 || !g_camp.events[g_camp.selected_event].active) {
        g_camp.selected_event = slot;
    }

    snprintf(g_camp.notice, sizeof(g_camp.notice), "NEW: %s @ %s",
             ev->type_title, ev->loc_name);

    if (play_sfx && g_camp.mode == 0) {
        sfx_alert();
    }
    return slot;
}

static int has_facility(int fac_id) {
    int r, c;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 4; c++)
            if (g_camp.bunker_grid[r][c] == fac_id) return 1;
    return 0;
}

void campaign_sync_squad_units(void) {
    int i;
    int bonus_hp = has_facility(FAC_MEDBAY) ? 15 : 0;
    int bonus_tu = has_facility(FAC_ARCADE_REC) ? 6 : 0;

    for (i = 0; i < MAX_SQUAD_SIZE; i++) {
        const Operative *op = &g_camp.squad[i];
        Unit *u = &g_units[i];
        operative_format_full_name(op, u->name, sizeof(u->name));
        u->gender      = op->gender;
        u->portrait_id = op->portrait_id % NUM_PORTRAITS;
        u->sprite_id   = gfx_portrait_to_unit_sprite(u->portrait_id);
        u->weapon_id   = op->weapon_id % 3;
        u->ammo        = (u->weapon_id == 1) ? 99 : 30;
        u->rank        = op->rank;
        u->xp_start    = op->xp;
        u->xp_gained   = 0;
        u->kills       = 0;
        u->tu_max      = op->tu_max + bonus_tu;
        u->tu          = u->tu_max;
        u->hp_max      = op->hp_max + bonus_hp;
        u->hp          = u->hp_max;
        u->energy_max  = op->energy_max;
        u->energy      = u->energy_max;
        u->firing_acc  = op->firing_acc;
        u->reactions   = op->reactions;
        u->morale      = 100;
    }
}

void campaign_init(void) {
    memset(&g_camp, 0, sizeof(g_camp));
    g_camp.mode = 0;
    g_camp.active_tab = 0;
    g_camp.selected_sector = 0;
    g_camp.selected_soldier = 0;
    g_camp.selected_event = -1;
    g_camp.launched_event = -1;
    g_camp.last_reported_idx = -1;
    g_camp.valley_day = 1;
    g_camp.valley_minute = 13 * 60 + 30; /* 13:30 Daylight in Coopersburg */
    g_camp.deploy_night = 0;
    g_camp.elerium = 20;
    g_camp.alloys  = 30;

    /* Initialize starting squad at lowest rank (ROOKIE) & populate replacement recruit pool */
    operative_load_starting_squad(g_camp.squad, MAX_SQUAD_SIZE);
    operative_refresh_recruit_pool(g_camp.recruit_pool, MAX_RECRUIT_POOL,
                                   g_camp.squad, MAX_SQUAD_SIZE);

    /* Initial Coopersburg Underground Bunker Layout */
    g_camp.bunker_grid[0][0] = FAC_RADAR;
    g_camp.bunker_grid[0][1] = FAC_LIFT;
    g_camp.bunker_grid[0][2] = FAC_HANGAR;
    g_camp.bunker_grid[0][3] = FAC_ARCADE_REC; /* Duff's Underground Arcade Rec Room! */
    g_camp.bunker_grid[1][1] = FAC_LAB;

    /* Report an initial Alien Event immediately on the Valley Radar & schedule the next! */
    spawn_random_radar_event(0);
    g_camp.next_spawn_timer = rrange_camp(280, 440);
}

void campaign_tick(void) {
    int i, active_cnt;

    g_camp.radar_tick++;
    if (g_camp.launch_prompt_open) {
        return; /* Pause contact timers while Commander reviews the Launch Mission prompt */
    }

    /* Advance Lehigh Valley 24-hour clock smoothly */
    if ((g_camp.radar_tick % 5) == 0) {
        g_camp.valley_minute++;
        if (g_camp.valley_minute >= 1440) {
            g_camp.valley_minute = 0;
            g_camp.valley_day++;
        }
    }

    if (g_camp.report_banner_timer > 0) {
        g_camp.report_banner_timer--;
    }

    /* Age active Alien Events; unaddressed UFOs eventually leave the Valley */
    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        RadarEvent *ev = &g_camp.events[i];
        if (!ev->active) continue;
        ev->age_ticks++;
        if (ev->age_ticks >= ev->ttl_ticks) {
            ev->active = 0;
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "LOST CONTACT: %s LEFT SECTOR", ev->tag);
            sfx_beep(220, 35);
            if (g_camp.selected_event == i) {
                g_camp.selected_event = first_active_event();
            }
        }
    }

    /* Periodically detect & report new random Alien Events across the Lehigh Valley */
    active_cnt = count_active_events();
    if (active_cnt == 0 && g_camp.next_spawn_timer > 85) {
        g_camp.next_spawn_timer = 85; /* Never leave radar empty for more than ~1.2s */
    }

    g_camp.next_spawn_timer--;
    if (g_camp.next_spawn_timer <= 0) {
        if (active_cnt < MAX_RADAR_EVENTS) {
            spawn_random_radar_event(1);
        }
        g_camp.next_spawn_timer = has_facility(FAC_RADAR) ?
                                  rrange_camp(320, 520) : rrange_camp(480, 720);
    }
}

static void launch_selected_mission(void) {
    int ev_idx = g_camp.selected_event;

    if (ev_idx >= 0 && ev_idx < MAX_RADAR_EVENTS && g_camp.events[ev_idx].active) {
        g_camp.selected_sector = g_camp.events[ev_idx].biome_id;
        g_camp.launched_event  = ev_idx;
    } else {
        g_camp.launched_event = -1;
    }

    g_camp.launch_prompt_open = 0;
    sfx_laser();
    isomap_start_mission(g_camp.selected_sector, g_camp.deploy_night);

    /* Apply Operative Ranks, Stats, Portraits, Armory Loadouts & Bunker Perks to Squad */
    campaign_sync_squad_units();
    g_camp.mode = 1;
}

void campaign_on_mission_complete(void) {
    int i;
    int kia_cnt = 0;
    int promo_cnt = 0;
    int focus_slot = -1;
    char promo_name[24] = "";
    char recruit_name[24] = "";
    int mission_won = (g_battle.mission_ended == 1);
    int contested_retreat = (!mission_won && g_battle.mission_ended == 0 &&
                             (g_battle.turn_number > 1 || g_battle.aliens_killed > 0));

    for (i = 0; i < MAX_SQUAD_SIZE; i++) {
        Operative *op = &g_camp.squad[i];
        Unit *u = &g_units[i];
        int fell_in_action = (!u->active || u->is_corpse || u->hp <= 0);

        if (fell_in_action) {
            kia_cnt++;
            g_camp.total_kia++;
            operative_hire_replacement(op, g_camp.recruit_pool, MAX_RECRUIT_POOL,
                                       g_camp.squad, MAX_SQUAD_SIZE);
            if (recruit_name[0] == '\0') {
                operative_format_full_name(op, recruit_name, sizeof(recruit_name));
            }
            if (focus_slot < 0) focus_slot = i;
        } else {
            int surv_xp = 0;
            int gained;
            op->kills += u->kills;
            if (mission_won) {
                surv_xp = XP_MISSION_VICTORY + (g_battle.night_mode ? XP_NIGHT_BONUS : 0);
                op->missions++;
            } else if (contested_retreat || u->xp_gained > 0) {
                surv_xp = XP_MISSION_RETREAT;
                op->missions++;
            }
            gained = u->xp_gained + surv_xp;
            if (operative_add_xp(op, gained) > 0) {
                if (promo_cnt == 0) {
                    operative_format_full_name(op, promo_name, sizeof(promo_name));
                    focus_slot = i;
                }
                promo_cnt++;
            }
        }
    }

    if (focus_slot >= 0) {
        g_camp.selected_soldier = focus_slot;
    }

    g_camp.total_kills   += g_battle.aliens_killed;
    g_camp.elerium       += g_battle.elerium_recovered;
    g_camp.alloys        += g_battle.alloys_recovered;
    g_camp.nav_units     += g_battle.nav_recovered;
    g_camp.cryo_pods     += g_battle.cryo_recovered;
    g_camp.tech_pods     += g_battle.pods_recovered;
    g_camp.alien_corpses += g_battle.corpses_recovered;

    /* Clear the completed Alien Event from the radar & advance Valley time +3 hrs */
    if (g_camp.launched_event >= 0 && g_camp.launched_event < MAX_RADAR_EVENTS) {
        g_camp.events[g_camp.launched_event].active = 0;
        g_camp.launched_event = -1;
    }
    g_camp.selected_event = first_active_event();
    g_camp.valley_minute += 180;
    while (g_camp.valley_minute >= 1440) {
        g_camp.valley_minute -= 1440;
        g_camp.valley_day++;
    }
    if (count_active_events() == 0) {
        g_camp.next_spawn_timer = 75;
    }

    if (promo_cnt > 0) {
        sfx_beep(523, 45);
        sfx_beep(659, 45);
        sfx_beep(784, 75);
    }

    if (mission_won) {
        g_camp.missions_won++;
        if (promo_cnt == 1 && kia_cnt == 0) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "PROMOTED: %s! (+%dA +%dE)",
                     promo_name, g_battle.alloys_recovered, g_battle.elerium_recovered);
        } else if (promo_cnt > 1 && kia_cnt == 0) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "VICTORY! %d PROMOTED (%s)!", promo_cnt, promo_name);
        } else if (kia_cnt == 1) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "1 KIA! %s JOINED SQUAD", recruit_name);
        } else if (kia_cnt > 1) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "VICTORY! %d KIA - NEW ROOKIES ASSIGNED", kia_cnt);
        } else if (g_battle.nav_recovered + g_battle.cryo_recovered + g_battle.pods_recovered > 0) {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "SECURED! +%dA +%dE +%dNAV +%dCRYO +%dPOD",
                     g_battle.alloys_recovered, g_battle.elerium_recovered,
                     g_battle.nav_recovered, g_battle.cryo_recovered, g_battle.pods_recovered);
        } else {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "VICTORY! +%d ALLOYS, +%d ELERIUM (+XP)",
                     g_battle.alloys_recovered, g_battle.elerium_recovered);
        }
    } else if (kia_cnt == 1) {
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "1 KIA! %s JOINED SQUAD", recruit_name);
    } else if (kia_cnt > 1) {
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "%d KIA! NEW ROOKIES ASSIGNED AT HQ", kia_cnt);
    } else if (promo_cnt > 0) {
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "HQ RETURN: %s PROMOTED!", promo_name);
    } else {
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "SKYRANGER RETURNED TO COOPERSBURG HQ");
    }
    g_camp.mode = 0;
}

/* Clip a single pixel strictly inside the active CRT map area (x=11..308, y=42..148) */
static void radar_pset_clip(int x, int y, uint8_t col) {
    if (x >= 11 && x <= 308 && y >= 42 && y <= 148) {
        vga_pset(x, y, col);
    }
}

static void draw_radar_sweep_ray(int cx, int cy, int dx, int dy, uint8_t col) {
    int s;
    for (s = 4; s <= 150; s += 2) {
        int px = cx + (dx * s) / 100;
        int py = cy + (dy * s) / 100;
        radar_pset_clip(px, py, col);
    }
}

static void draw_radar_pulse_box(int cx, int cy, int r, uint8_t col) {
    int k;
    for (k = -r; k <= r; k++) {
        radar_pset_clip(cx + k, cy - r, col);
        radar_pset_clip(cx + k, cy + r, col);
        radar_pset_clip(cx - r, cy + k, col);
        radar_pset_clip(cx + r, cy + k, col);
    }
}

static void render_launch_modal(const RadarEvent *ev) {
    char buf[80];
    int mx = g_input.mx, my = g_input.my;
    int hov_time   = (mx >= 34  && mx < 286 && my >= 95  && my < 109);
    int hov_launch = (mx >= 36  && mx < 156 && my >= 126 && my < 146);
    int hov_cancel = (mx >= 164 && mx < 284 && my >= 126 && my < 146);

    /* Drop shadow + beveled Command Dispatch Dialog centered over the radar */
    vga_rect_fill(28, 42, 272, 118, 15);
    vga_rect_bevel(24, 38, 272, 118, 76, 96, 79);
    vga_rect_fill(28, 42, 264, 110, 14);

    /* Header Banner */
    vga_rect_bevel(30, 44, 260, 13, 36, 96, 45);
    snprintf(buf, sizeof(buf), ">> ALIEN INCIDENT REPORT #%02u <<", (unsigned)(ev->event_num % 100));
    vga_print_shadow(67, 47, buf, 96, 15);

    /* Incident Details */
    snprintf(buf, sizeof(buf), "EVENT:  %s (%s)", ev->type_title, ev->tag);
    vga_print_shadow(34, 62, buf, 96, 15);

    snprintf(buf, sizeof(buf), "SECTOR: %s", ev->loc_name);
    vga_print_shadow(34, 73, buf, 80, 15);

    snprintf(buf, sizeof(buf), "INTEL:  %s", ev->intel_desc);
    vga_print(34, 84, buf, 18);

    /* Clickable Daylight / Night Ops Condition Badge */
    vga_rect_bevel(34, 95, 252, 14, g_camp.deploy_night ? 131 : 67,
                   hov_time ? 96 : 80, 79);
    vga_print_shadow(40, 99,
                     g_camp.deploy_night ? "TIME: NIGHT OPS (DARK)   [CLICK: DAY]"
                                         : "TIME: DAYLIGHT OPS       [CLICK: NGT]",
                     96, 15);

    /* Launch Confirmation Question */
    vga_print_shadow(55, 114, "LAUNCH SKYRANGER INTERCEPT MISSION?", 96, 15);

    /* [LAUNCH MISSION] & [STAND DOWN] Buttons */
    vga_rect_bevel(36, 126, 120, 20, hov_launch ? 34 : 36, 96, 45);
    vga_print_shadow(54, 133, "LAUNCH MISSION", 96, 15);

    vga_rect_bevel(164, 126, 120, 20, hov_cancel ? 65 : 167, hov_cancel ? 96 : 162, 172);
    vga_print_shadow(194, 133, "STAND DOWN", hov_cancel ? 96 : 1, 15);
}

static void render_radar_tab(void) {
    int i, hov_event = -1;
    int active_cnt = count_active_events();
    int sweep_idx = (g_camp.radar_tick / 3) & 31;
    int trail_idx = (sweep_idx - 1) & 31;
    int hr = (g_camp.valley_minute / 60) % 24;
    int mn = g_camp.valley_minute % 60;
    int night = is_valley_night();
    char hdr_buf[64];
    char clk_buf[64];
    char bar_buf[80];

    /* Full-Width CRT Lehigh Valley Radar Frame (x=8, y=28, w=304, h=136) */
    vga_rect_bevel(8, 28, 304, 136, 15, 20, 26);

    if (gfx_radar_bg) {
        vga_blit_ui(10, 30, 300, 132, gfx_radar_bg);
    } else {
        /* Full-width 300x132 procedural CRT fallback */
        vga_rect_fill(10, 30, 300, 132, 30);
        for (i = 44; i < 156; i += 20) vga_hline(10, 309, i, 28);
        for (i = 36; i < 308; i += 30) vga_vline(i, 30, 161, 28);
        vga_line(108, 32, 134, 66, 70);
        vga_line(134, 66, 198, 62, 70);
        vga_line(198, 62, 278, 56, 70);
        vga_line(100, 32, 172, 92, 98);
        vga_line(172, 92, 168, 112, 98);
        vga_line(168, 112, 186, 158, 98);
    }

    /* Rotating CRT Radar Sweep Beam centered on Coopersburg HQ */
    draw_radar_sweep_ray(HQ_RADAR_X, HQ_RADAR_Y, sweep_dx[trail_idx], sweep_dy[trail_idx], 25);
    draw_radar_sweep_ray(HQ_RADAR_X, HQ_RADAR_Y, sweep_dx[sweep_idx], sweep_dy[sweep_idx], 19);

    /* Geographic Town Labels across the Full-Width Lehigh Valley Map */
    vga_print_shadow(96,  56, "ALLENTOWN",  22, 15);
    vga_print_shadow(176, 48, "BETHLEHEM",  22, 15);
    vga_print_shadow(244, 46, "EASTON",     22, 15);
    vga_print_shadow(74,  86, "EMMAUS",     22, 15);
    vga_print_shadow(24, 108, "MACUNGIE",   22, 15);
    vga_print_shadow(150,140, "QUAKERTOWN", 22, 15);
    vga_print_shadow(224,140, "NOCKAMIXON", 22, 15);

    /* Coopersburg DUFF-COM HQ Marker (Cyan Base Diamond) */
    vga_hline(HQ_RADAR_X - 4, HQ_RADAR_X + 4, HQ_RADAR_Y, 80);
    vga_vline(HQ_RADAR_X, HQ_RADAR_Y - 4, HQ_RADAR_Y + 4, 80);
    vga_rect_fill(HQ_RADAR_X - 1, HQ_RADAR_Y - 1, 3, 3, 96);
    vga_print_shadow(HQ_RADAR_X + 6, HQ_RADAR_Y - 3, "HQ", 80, 15);

    /* Top CRT Telemetry Header Strip & [SCAN >>] Button */
    vga_rect_fill(10, 30, 300, 11, 15);
    vga_hline(10, 309, 41, 25);
    snprintf(hdr_buf, sizeof(hdr_buf), "VALLEY RADAR  CONTACTS:%d", active_cnt);
    vga_print_shadow(14, 32, hdr_buf, active_cnt > 0 ? 96 : 19, 15);

    snprintf(clk_buf, sizeof(clk_buf), "D%d %02d:%02d %s",
             g_camp.valley_day, hr, mn, night ? "NGT" : "DAY");
    vga_print_shadow(178, 32, clk_buf, night ? 80 : 96, 15);

    {
        int hov_scan = (!g_camp.launch_prompt_open &&
                        g_input.mx >= 260 && g_input.mx < 308 &&
                        g_input.my >= 31  && g_input.my < 41);
        vga_rect_bevel(260, 31, 48, 9, hov_scan ? 22 : 27, hov_scan ? 96 : 20, 29);
        vga_print(264, 32, "SCAN >>", hov_scan ? 96 : 18);
    }

    /* Plot Active Random Alien Events across the Lehigh Valley Radar! */
    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        const RadarEvent *ev = &g_camp.events[i];
        int lx, ly, x0, y0, x1, y1;
        int is_hov = 0, is_sel = (g_camp.selected_event == i);
        int pulse_r;
        uint8_t core_col, ring_col, txt_col;

        if (!ev->active) continue;

        event_hit_box(ev, &lx, &ly, &x0, &y0, &x1, &y1);
        if (!g_camp.launch_prompt_open &&
            g_input.mx >= x0 && g_input.mx <= x1 &&
            g_input.my >= y0 && g_input.my <= y1) {
            is_hov = 1;
            hov_event = i;
        }

        if (ev->severity == 2) {        /* Alien Raid / Terror: Red-Gold */
            core_col = ((g_camp.radar_tick & 8) || is_hov) ? 32 : 96;
            ring_col = 34;
            txt_col  = is_hov ? 96 : 32;
        } else if (ev->severity == 1) { /* Landed UFO: Gold-Orange */
            core_col = is_hov ? 0 : 96;
            ring_col = 113;
            txt_col  = 96;
        } else {                        /* Crash Site: Elerium Lime */
            core_col = is_hov ? 96 : 144;
            ring_col = 18;
            txt_col  = is_hov ? 96 : 144;
        }

        /* Initial wide detection ping + continuous pulsing sonar ring */
        if (ev->age_ticks < 36) {
            draw_radar_pulse_box(ev->rx, ev->ry, 6 + (ev->age_ticks / 2), 96);
        }
        pulse_r = 5 + (((g_camp.radar_tick / 5) + i * 2) % 6);
        draw_radar_pulse_box(ev->rx, ev->ry, pulse_r, ring_col);

        /* UFO / Incident Blip Reticle */
        vga_hline(ev->rx - 5, ev->rx + 5, ev->ry, core_col);
        vga_vline(ev->rx, ev->ry - 5, ev->ry + 5, core_col);
        vga_rect_fill(ev->rx - 2, ev->ry - 2, 5, 5, core_col);
        vga_pset(ev->rx, ev->ry, 15);

        /* Hover / Selected Target Lock Brackets around Blip + Callsign */
        if (is_hov || is_sel) {
            uint8_t bcol = (is_hov || g_camp.launch_prompt_open) ? 96 : 80;
            vga_hline(x0, x0 + 3, y0, bcol); vga_vline(x0, y0, y0 + 3, bcol);
            vga_hline(x1 - 3, x1, y0, bcol); vga_vline(x1, y0, y0 + 3, bcol);
            vga_hline(x0, x0 + 3, y1, bcol); vga_vline(x0, y1 - 3, y1, bcol);
            vga_hline(x1 - 3, x1, y1, bcol); vga_vline(x1, y1 - 3, y1, bcol);
        }

        vga_rect_fill(lx - 1, ly - 1, (int)strlen(ev->tag) * 6 + 1, 8, 15);
        vga_print_shadow(lx, ly, ev->tag, txt_col, 15);
    }

    /* Bottom CRT Dispatch / Target Lock Strip */
    vga_rect_fill(10, 149, 300, 13, 15);
    vga_hline(10, 309, 149, 25);
    if (hov_event >= 0) {
        const RadarEvent *ev = &g_camp.events[hov_event];
        snprintf(bar_buf, sizeof(bar_buf), "LOCK: %s - %s [CLICK]",
                 ev->type_title, ev->loc_name);
        vga_print_shadow(14, 152, bar_buf, 96, 15);
    } else if (g_camp.report_banner_timer > 0 &&
               g_camp.last_reported_idx >= 0 &&
               g_camp.events[g_camp.last_reported_idx].active) {
        const RadarEvent *ev = &g_camp.events[g_camp.last_reported_idx];
        uint8_t acol = (g_camp.radar_tick & 8) ? 96 : 32;
        snprintf(bar_buf, sizeof(bar_buf), "ALERT: %s - %s!",
                 ev->type_title, ev->loc_name);
        vga_print_shadow(14, 152, bar_buf, acol, 15);
    } else if (active_cnt > 0) {
        vga_print_shadow(14, 152, "CLICK AN ALIEN EVENT ON RADAR TO LAUNCH MISSION", 80, 15);
    } else {
        vga_print_shadow(14, 152, "SWEEPING LEHIGH VALLEY AIRSPACE FOR UFOS...", 19, 15);
    }

    /* Modal Launch Mission Confirmation Dialog when an Alien Event was clicked! */
    if (g_camp.launch_prompt_open &&
        g_camp.selected_event >= 0 &&
        g_camp.selected_event < MAX_RADAR_EVENTS &&
        g_camp.events[g_camp.selected_event].active) {
        render_launch_modal(&g_camp.events[g_camp.selected_event]);
    } else {
        g_camp.launch_prompt_open = 0;
    }
}

static void render_bunker_tab(void) {
    int r, c;
    char vault_buf[64];
    /* Coopersburg Surface Grass & Recovered UFO Tech Vault Header */
    vga_rect_bevel(8, 28, 304, 19, 22, 18, 26);
    vga_print_shadow(14, 30, "COOPERSBURG UNDERGROUND HQ (ALLOYS:15/ROOM)", 96, 15);
    snprintf(vault_buf, sizeof(vault_buf), "UFO VAULT: NAV:%d  CRYO:%d  PODS:%d  BODIES:%d",
             g_camp.nav_units, g_camp.cryo_pods, g_camp.tech_pods, g_camp.alien_corpses);
    vga_print_shadow(14, 39, vault_buf, 80, 15);

    /* 3x4 Underground Facility Grid */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            int bx = 8 + c * 76;
            int by = 48 + r * 38;
            int fid = g_camp.bunker_grid[r][c];
            int hov = (g_input.mx >= bx && g_input.mx < bx + 74 &&
                       g_input.my >= by && g_input.my < by + 36);
            uint8_t bg = (fid == FAC_DIRT) ? 58 : (fid == FAC_ARCADE_REC ? 132 : 168);

            vga_rect_bevel(bx, by, 74, 36, bg, hov ? 96 : 163, 172);
            vga_print_shadow(bx + 5, by + 6, fac_names[fid], fid == FAC_DIRT ? 52 : 96, 15);

            if (fid == FAC_DIRT) {
                vga_print(bx + 5, by + 20, "[BUILD 15A]", hov ? 96 : 50);
            } else if (fid == FAC_ARCADE_REC) {
                vga_print(bx + 5, by + 20, "+6 MAX TU!", 80);
            } else if (fid == FAC_MEDBAY) {
                vga_print(bx + 5, by + 20, "+15 MAX HP!", 18);
            } else if (fid == FAC_LAB) {
                vga_print(bx + 5, by + 20, "LASERS OK", 80);
            } else if (fid == FAC_PLASMA_WORK) {
                vga_print(bx + 5, by + 20, "PLASMA OK", 144);
            } else {
                vga_print(bx + 5, by + 20, "ONLINE", 18);
            }
        }
    }
}

static void render_armory_tab(void) {
    int i;
    int sel = g_camp.selected_soldier % MAX_SQUAD_SIZE;
    const Operative *op = &g_camp.squad[sel];
    int bonus_hp = has_facility(FAC_MEDBAY) ? 15 : 0;
    int bonus_tu = has_facility(FAC_ARCADE_REC) ? 6 : 0;
    static const char *wep_short[3] = { "RIFLE", "LASER", "PLASMA" };
    static const char *wep_full[3]  = { "ASSAULT RIFLE", "LASER RIFLE", "HEAVY PLASMA" };
    char kbuf[32];

    /* Left Panel: Selected Operative Portrait, Rank, XP & Stats Dossier (x=8, y=28, w=86, h=136) */
    vga_rect_bevel(8, 28, 86, 136, 76, 80, 79);
    {
        const char *rname = operative_rank_name(op->rank);
        int hdr_w = 11 + (int)strlen(rname) * 6;
        int hdr_x = 8 + (86 - hdr_w) / 2;
        operative_draw_rank_badge(hdr_x, 31, op->rank);
        vga_print_shadow(hdr_x + 11, 31, rname, 96, 15);
    }

    vga_print(14, 43, op->gender ? "F" : "M", op->gender ? 96 : 80);
    snprintf(kbuf, sizeof(kbuf), "#%d", sel + 1);
    vga_print(73, 43, kbuf, 2);

    vga_rect_bevel(33, 40, 36, 38, 14, 96, 79);
    vga_blit_ui(35, 41, 32, 36, spr_portraits[op->portrait_id % NUM_PORTRAITS]);

    /* XP Readout & Progress Bar toward Next Promotion */
    {
        int fill = 74;
        if (op->rank < RANK_COMMANDER) {
            int cur_req = operative_rank_xp_req(op->rank);
            int nxt_req = operative_next_rank_xp(op->rank);
            int span = (nxt_req > cur_req) ? (nxt_req - cur_req) : 1;
            fill = ((op->xp - cur_req) * 74) / span;
            if (fill < 0) fill = 0;
            if (fill > 74) fill = 74;
            snprintf(kbuf, sizeof(kbuf), "XP:%d/%d", op->xp, nxt_req);
        } else {
            snprintf(kbuf, sizeof(kbuf), "XP:%d MAX", op->xp);
        }
        vga_print(13, 80, kbuf, 96);
        vga_rect_bevel(13, 89, 76, 5, 14, 10, 15);
        if (fill > 0) {
            vga_rect_fill(14, 90, fill, 3, (op->rank == RANK_COMMANDER) ? 96 : 80);
        }
    }

    snprintf(kbuf, sizeof(kbuf), "TU:%2d  HP:%2d", op->tu_max + bonus_tu, op->hp_max + bonus_hp);
    vga_print(13, 97, kbuf, 18);

    snprintf(kbuf, sizeof(kbuf), "AIM:%2d RCN:%2d", op->firing_acc, op->reactions);
    vga_print(12, 107, kbuf, 80);

    snprintf(kbuf, sizeof(kbuf), "KLS:%2d MSN:%2d", op->kills, op->missions);
    vga_print(12, 117, kbuf, 2);

    vga_print(12, 127, wep_full[op->weapon_id % 3], 96);

    /* Side-by-Side [FACE] & [MALE]/[FEM] Customization Buttons */
    {
        int hov_face = (g_input.mx >= 12 && g_input.mx < 49 && g_input.my >= 139 && g_input.my < 157);
        int hov_sex  = (g_input.mx >= 51 && g_input.mx < 89 && g_input.my >= 139 && g_input.my < 157);
        vga_rect_bevel(12, 139, 37, 18, hov_face ? 65 : 68, hov_face ? 96 : 162, 79);
        vga_print_shadow(19, 145, "FACE", 96, 15);

        vga_rect_bevel(51, 139, 38, 18, hov_sex ? 65 : 68, hov_sex ? 96 : 162, 79);
        vga_print_shadow(op->gender ? 61 : 58, 145, op->gender ? "FEM" : "MALE", 80, 15);
    }

    /* Right Panel: 5-Agent Squad Roster, Ranks, Stats & Weapon Equip List (x=98, y=28, w=214, h=136) */
    vga_rect_bevel(98, 28, 214, 136, 76, 80, 79);
    vga_print_shadow(104, 33, "SQUAD ROSTER (CLICK AGENT/WEP)", 96, 15);

    for (i = 0; i < MAX_SQUAD_SIZE; i++) {
        const Operative *rop = &g_camp.squad[i];
        char full_name[24];
        char sbuf[32];
        int ry = 45 + i * 23;
        int w = rop->weapon_id % 3;
        int is_sel = (i == sel);
        int hov = (g_input.mx >= 103 && g_input.mx < 307 && g_input.my >= ry && g_input.my < ry + 21);

        vga_rect_bevel(103, ry, 204, 21, is_sel ? 65 : (hov ? 68 : 74), (is_sel || hov) ? 96 : 68, 79);
        operative_draw_rank_badge(107, ry + 3, rop->rank);
        operative_format_full_name(rop, full_name, sizeof(full_name));
        vga_print_shadow(118, ry + 3, full_name, is_sel ? 96 : 0, 15);

        snprintf(sbuf, sizeof(sbuf), "TU:%d AIM:%d K:%d",
                 rop->tu_max + bonus_tu, rop->firing_acc, rop->kills);
        vga_print(107, ry + 12, sbuf, is_sel ? 80 : 18);

        vga_blit_ui(215, ry - 1, 40, 24, spr_weapons[w]);
        vga_print_shadow(257, ry + 7, wep_short[w], 80, 15);
    }
}

void campaign_render(void) {
    int t;
    static const char *tabs[3] = {
        "1.VALLEY RADAR",
        "2.BUNKER BASE",
        "3.SQUAD ARMORY"
    };
    char res_buf[64];

    vga_clear(14);

    /* Top Command Tab Bar */
    for (t = 0; t < 3; t++) {
        int tx = 8 + t * 102;
        int act = (g_camp.active_tab == t);
        int hov = (!g_camp.launch_prompt_open &&
                   g_input.mx >= tx && g_input.mx < tx + 100 &&
                   g_input.my >= 4  && g_input.my < 24);
        vga_rect_bevel(tx, 4, 100, 20, act ? 65 : (hov ? 68 : 167), (act || hov) ? 96 : 162, 172);
        vga_print_shadow(tx + 8, 10, tabs[t], (act || hov) ? 96 : 1, 15);
    }

    if (g_camp.active_tab == 0)      render_radar_tab();
    else if (g_camp.active_tab == 1) render_bunker_tab();
    else                             render_armory_tab();

    /* Bottom Status & Resource Strip + Trackball [OPTIONS] Button */
    vga_rect_bevel(0, 168, SCREEN_W, 32, 166, 162, 171);
    snprintf(res_buf, sizeof(res_buf), "ALLOY:%d ELER:%d TECH:%d WINS:%d KILLS:%d KIA:%d",
             g_camp.alloys, g_camp.elerium,
             g_camp.nav_units + g_camp.cryo_pods + g_camp.tech_pods,
             g_camp.missions_won, g_camp.total_kills, g_camp.total_kia);
    vga_print_shadow(8, 173, res_buf, 80, 15);
    vga_print(8, 185, g_camp.notice, 96);
    {
        int hov_opt = (!g_camp.launch_prompt_open &&
                       g_input.mx >= 262 && g_input.mx < 316 &&
                       g_input.my >= 181 && g_input.my < 196);
        vga_rect_bevel(262, 181, 54, 15, hov_opt ? 65 : 68, hov_opt ? 96 : 162, 172);
        vga_print_shadow(268, 185, "OPTIONS", hov_opt ? 96 : 1, 15);
    }
}

static void trigger_fast_scan(void) {
    g_camp.valley_minute += 150;
    while (g_camp.valley_minute >= 1440) {
        g_camp.valley_minute -= 1440;
        g_camp.valley_day++;
    }
    g_camp.radar_tick += 24;
    if (count_active_events() < MAX_RADAR_EVENTS) {
        spawn_random_radar_event(1);
        g_camp.next_spawn_timer = rrange_camp(340, 540);
    } else {
        snprintf(g_camp.notice, sizeof(g_camp.notice), "3 ACTIVE CONTACTS ON RADAR - CLICK ONE!");
        sfx_click();
    }
}

static void cycle_selected_event(int delta) {
    int i, cur = (g_camp.selected_event >= 0) ? g_camp.selected_event : 0;
    for (i = 1; i <= MAX_RADAR_EVENTS; i++) {
        int idx = (cur + i * delta + MAX_RADAR_EVENTS * 4) % MAX_RADAR_EVENTS;
        if (g_camp.events[idx].active) {
            g_camp.selected_event = idx;
            sfx_click();
            return;
        }
    }
}

void campaign_handle_input(void) {
    int mx = g_input.mx, my = g_input.my;

    /* 1. Handle Modal "LAUNCH SKYRANGER INTERCEPT MISSION?" Dialog if Open */
    if (g_camp.launch_prompt_open) {
        if (g_input.last_key == KEY_ENTER || g_input.last_key == 'y' || g_input.last_key == 'Y') {
            launch_selected_mission();
            return;
        }
        if (g_input.last_key == KEY_ESC || g_input.last_key == 'n' || g_input.last_key == 'N' ||
            g_input.right_clicked) {
            g_camp.launch_prompt_open = 0;
            sfx_click();
            return;
        }
        if (g_input.last_key == 't' || g_input.last_key == 'T') {
            g_camp.deploy_night = !g_camp.deploy_night;
            sfx_click();
            return;
        }
        if (g_input.left_clicked) {
            /* [LAUNCH MISSION] button */
            if (mx >= 36 && mx < 156 && my >= 126 && my < 146) {
                launch_selected_mission();
                return;
            }
            /* [STAND DOWN] button or click outside dialog */
            if ((mx >= 164 && mx < 284 && my >= 126 && my < 146) ||
                mx < 24 || mx >= 296 || my < 38 || my >= 156) {
                g_camp.launch_prompt_open = 0;
                sfx_click();
                return;
            }
            /* Day / Night Ops toggle row */
            if (mx >= 34 && mx < 286 && my >= 95 && my < 109) {
                g_camp.deploy_night = !g_camp.deploy_night;
                sfx_click();
                return;
            }
        }
        return;
    }

    /* 2. Keyboard Shortcuts in Command Center */
    if (g_input.last_key == '1') { g_camp.active_tab = 0; sfx_click(); }
    if (g_input.last_key == '2') { g_camp.active_tab = 1; sfx_click(); }
    if (g_input.last_key == '3') { g_camp.active_tab = 2; sfx_click(); }
    if (g_input.last_key == KEY_TAB) { g_camp.active_tab = (g_camp.active_tab + 1) % 3; sfx_click(); }

    if (g_camp.active_tab == 0) {
        if (g_input.last_key == KEY_UP || g_input.last_key == KEY_LEFT) {
            cycle_selected_event(-1);
        }
        if (g_input.last_key == KEY_DOWN || g_input.last_key == KEY_RIGHT) {
            cycle_selected_event(1);
        }
        if (g_input.last_key == 's' || g_input.last_key == 'S' || g_input.last_key == ' ') {
            trigger_fast_scan();
            return;
        }
        if (g_input.last_key == KEY_ENTER) {
            if (g_camp.selected_event < 0 || !g_camp.events[g_camp.selected_event].active) {
                g_camp.selected_event = first_active_event();
            }
            if (g_camp.selected_event >= 0 && g_camp.events[g_camp.selected_event].active) {
                g_camp.selected_sector = g_camp.events[g_camp.selected_event].biome_id;
                g_camp.deploy_night    = is_valley_night();
                g_camp.launch_prompt_open = 1;
                sfx_click();
            }
            return;
        }
    } else if (g_camp.active_tab == 2) {
        if (g_input.last_key == KEY_UP) {
            g_camp.selected_soldier = (g_camp.selected_soldier + MAX_SQUAD_SIZE - 1) % MAX_SQUAD_SIZE;
            sfx_click();
        }
        if (g_input.last_key == KEY_DOWN) {
            g_camp.selected_soldier = (g_camp.selected_soldier + 1) % MAX_SQUAD_SIZE;
            sfx_click();
        }
        if (g_input.last_key == KEY_ENTER || g_input.last_key == ' ') {
            int s = g_camp.selected_soldier % MAX_SQUAD_SIZE;
            g_camp.squad[s].weapon_id = (g_camp.squad[s].weapon_id + 1) % 3;
            sfx_click();
            return;
        }
    }

    if (!g_input.left_clicked) return;

    /* Bottom-Right [OPTIONS] Button */
    if (mx >= 262 && mx < 316 && my >= 181 && my < 196) {
        g_settings.menu_open = 1;
        sfx_click();
        return;
    }

    /* Top Tab Bar Clicks */
    if (my >= 4 && my < 24) {
        int t = (mx - 8) / 102;
        if (t >= 0 && t < 3) {
            g_camp.active_tab = t;
            sfx_click();
            return;
        }
    }

    if (g_camp.active_tab == 0) {
        int i;
        /* Top-Right [SCAN >>] Button inside CRT Header */
        if (mx >= 260 && mx < 308 && my >= 31 && my < 41) {
            trigger_fast_scan();
            return;
        }

        /* Clicking an Active Alien Event Blip on the Full-Width Lehigh Valley Radar */
        for (i = 0; i < MAX_RADAR_EVENTS; i++) {
            const RadarEvent *ev = &g_camp.events[i];
            int lx, ly, x0, y0, x1, y1;
            if (!ev->active) continue;
            event_hit_box(ev, &lx, &ly, &x0, &y0, &x1, &y1);
            if (mx >= x0 && mx <= x1 && my >= y0 && my <= y1) {
                g_camp.selected_event     = i;
                g_camp.selected_sector    = ev->biome_id;
                g_camp.deploy_night       = is_valley_night();
                g_camp.launch_prompt_open = 1;
                sfx_click();
                return;
            }
        }

        /* Clicked empty radar terrain */
        if (mx >= 10 && mx < 310 && my >= 42 && my < 162) {
            if (count_active_events() > 0) {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "CLICK A PULSING ALIEN CONTACT BLIP!");
            } else {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "SCANNING VALLEY... CLICK [SCAN >>] OR WAIT");
            }
            sfx_beep(300, 20);
        }
    } else if (g_camp.active_tab == 1) {
        /* Clicking an Underground Bunker Slot to Construct a New Facility (Cost: 15 Alloys) */
        if (mx >= 8 && mx < 312 && my >= 48 && my < 162) {
            int c = (mx - 8) / 76;
            int r = (my - 48) / 38;
            if (r >= 0 && r < 3 && c >= 0 && c < 4) {
                if (g_camp.bunker_grid[r][c] == FAC_DIRT) {
                    if (g_camp.alloys >= 15) {
                        static int next_build = FAC_MEDBAY;
                        g_camp.alloys -= 15;
                        g_camp.bunker_grid[r][c] = next_build;
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "BUILT %s IN COOPERSBURG HQ!", fac_names[next_build]);
                        next_build = (next_build == FAC_MEDBAY) ? FAC_PLASMA_WORK :
                                     (next_build == FAC_PLASMA_WORK) ? FAC_TURRET : FAC_ARCADE_REC;
                        sfx_beep(700, 50);
                    } else {
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "NEED 15 ALIEN ALLOYS TO EXCAVATE & BUILD!");
                        sfx_beep(200, 50);
                    }
                }
            }
        }
    } else if (g_camp.active_tab == 2) {
        int sel = g_camp.selected_soldier % MAX_SQUAD_SIZE;
        Operative *op = &g_camp.squad[sel];
        /* Left Bio Panel: [FACE] button cycles portraits within current gender */
        if (mx >= 12 && mx < 49 && my >= 139 && my < 157) {
            int base = op->gender ? 6 : 0;
            int next_off = ((op->portrait_id - base) + 1) % 6;
            op->portrait_id = base + next_off;
            sfx_click();
            return;
        }
        /* Left Bio Panel: [MALE]/[FEM] button toggles gender & assigns matching portrait */
        if (mx >= 51 && mx < 89 && my >= 139 && my < 157) {
            op->gender ^= 1;
            op->portrait_id = (op->gender ? 6 : 0) + (rand() % 6);
            sfx_click();
            return;
        }
        /* Right Squad List: Clicking an agent selects them; clicking weapon or re-clicking cycles weapon */
        if (mx >= 103 && mx < 307 && my >= 45 && my < 160) {
            int idx = (my - 45) / 23;
            if (idx >= 0 && idx < MAX_SQUAD_SIZE) {
                if (g_camp.selected_soldier == idx || mx >= 210) {
                    g_camp.squad[idx].weapon_id = (g_camp.squad[idx].weapon_id + 1) % 3;
                }
                g_camp.selected_soldier = idx;
                sfx_click();
            }
        }
    }
}
