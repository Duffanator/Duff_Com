#include "campaign.h"
#include "base_ui.h"
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

/* Coopersburg HQ screen position inside the 236x132 Left CRT Radar Viewport (x=6..241, y=18..149) */
#define RADAR_X0    6
#define RADAR_Y0    18
#define RADAR_W     236
#define RADAR_H     132
#define HQ_RADAR_X  132
#define HQ_RADAR_Y  100


typedef struct {
    const char *loc_name;
    int map_x, map_y; /* Coordinates within the 300x132 Lehigh Valley map */
} ValleySite;

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

/* Compute screen-space coordinates for a Lehigh Valley site inside the 236x132 radar map */
static void site_screen_xy(int b, int s, int *sx, int *sy) {
    *sx = RADAR_X0 + (valley_sites[b & 3][s & 3].map_x * RADAR_W) / 300;
    *sy = RADAR_Y0 + valley_sites[b & 3][s & 3].map_y;
}

/* Dynamically update a UFO's biome_id and loc_name to match wherever it currently is on the radar! */
static void update_event_nearest_site(RadarEvent *ev) {
    int b, s, best_b = 0, best_s = 0, best_d = 999999;
    for (b = 0; b < 4; b++) {
        for (s = 0; s < 4; s++) {
            int sx, sy, d;
            site_screen_xy(b, s, &sx, &sy);
            d = (ev->rx - sx) * (ev->rx - sx) + (ev->ry - sy) * (ev->ry - sy);
            if (d < best_d) {
                best_d = d;
                best_b = b;
                best_s = s;
            }
        }
    }
    ev->biome_id = best_b;
    ev->loc_name = valley_sites[best_b][best_s].loc_name;
}

/* Progressive alien attack timeline: rare at game start, increasingly aggressive as days/wins advance */
static int roll_next_ufo_delay_mins(void) {
    int threat = (g_camp.valley_day - 1) + g_camp.missions_won * 2;
    if (threat <= 3) {
        return rrange_camp(1080, 1800); /* Early game: 18 to 30 hours between UFOs */
    } else if (threat <= 10) {
        return rrange_camp(540, 1020);  /* Mid game: 9 to 17 hours between UFOs */
    } else {
        return rrange_camp(240, 540);   /* Late game: 4 to 9 hours between UFOs */
    }
}

/* Compute screen-space label & click hitbox for an active radar contact */
static void event_hit_box(const RadarEvent *ev, int *lx_out, int *ly_out,
                          int *x0, int *y0, int *x1, int *y1) {
    int tw = (int)strlen(ev->tag) * 6;
    int lx = (ev->rx > 184) ? (ev->rx - tw - 6) : (ev->rx + 7);
    int ly = ev->ry - 3;

    if (lx_out) *lx_out = lx;
    if (ly_out) *ly_out = ly;

    *x0 = (lx < ev->rx - 8) ? (lx - 2) : (ev->rx - 8);
    *x1 = (lx + tw + 2 > ev->rx + 8) ? (lx + tw + 2) : (ev->rx + 8);
    *y0 = ev->ry - 8;
    *y1 = ev->ry + 8;
}

/* Queue a Command Center Event Modal Popup, pausing time & dropping speed to 1S! */
void campaign_push_modal(const char *title, const char *l1, const char *l2, const char *l3,
                         uint8_t accent_col, uint8_t sfx_type) {
    ModalAlert *m;
    g_camp.time_speed = 0; /* Always reset time passage to 1S (Realtime) on event! */

    if (g_camp.modal_count >= MAX_MODAL_QUEUE) {
        return;
    }
    m = &g_camp.modal_queue[g_camp.modal_count++];
    memset(m, 0, sizeof(*m));
    if (title) strncpy(m->title, title, sizeof(m->title) - 1);
    if (l1)    strncpy(m->line1, l1,    sizeof(m->line1) - 1);
    if (l2)    strncpy(m->line2, l2,    sizeof(m->line2) - 1);
    if (l3)    strncpy(m->line3, l3,    sizeof(m->line3) - 1);
    m->accent_col = accent_col ? accent_col : 96;
    m->sfx_type   = sfx_type;

    if (g_camp.modal_open == 0) {
        g_camp.modal_open       = 1; /* Start opening window animation */
        g_camp.modal_anim_frame = 0;
        if (g_camp.mode == 0) {
            if (sfx_type == 1)      sfx_alert();
            else if (sfx_type == 2) sfx_explosion();
            else { sfx_beep(523, 35); sfx_beep(784, 55); }
        }
    }
}

/* Spawn a new randomized Flying Alien UFO entering Lehigh Valley airspace! */
static int spawn_random_radar_event(int play_sfx) {
    int slot = -1, i, attempt;
    int biome_used[4] = { 0, 0, 0, 0 };
    int chosen_biome = 0, chosen_site = 0;
    int rx = 110, ry = 75;
    int threat = (g_camp.valley_day - 1) + g_camp.missions_won * 2;
    int size_roll = rand() % 100;
    int ufo_sz = 0;
    RadarEvent *ev;

    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        if (!g_camp.events[i].active) {
            if (slot < 0) slot = i;
        } else {
            biome_used[g_camp.events[i].biome_id & 3] = 1;
        }
    }
    if (slot < 0) return -1;

    /* Roll UFO Size (0=Small Scout, 1=Medium Harvester, 2=Large Abductor, 3=Command Ship) */
    if (threat <= 3) {
        ufo_sz = (size_roll < 75) ? 0 : 1;
    } else if (threat <= 8) {
        ufo_sz = (size_roll < 35) ? 0 : (size_roll < 78 ? 1 : 2);
    } else {
        ufo_sz = (size_roll < 15) ? 0 : (size_roll < 45 ? 1 : (size_roll < 75 ? 2 : 3));
    }

    /* Pick initial Lehigh Valley entry vector with clear separation from existing blips & HQ */
    for (attempt = 0; attempt < 24; attempt++) {
        int b = rand() & 3;
        int s = rand() & 3;
        int cand_x, cand_y, ok = 1;

        if (attempt < 14 && biome_used[b]) continue;

        site_screen_xy(b, s, &cand_x, &cand_y);
        cand_x += rrange_camp(-8, 8);
        cand_y += rrange_camp(-6, 6);
        if (cand_x < RADAR_X0 + 16)            cand_x = RADAR_X0 + 16;
        if (cand_x > RADAR_X0 + RADAR_W - 18)  cand_x = RADAR_X0 + RADAR_W - 18;
        if (cand_y < RADAR_Y0 + 16)            cand_y = RADAR_Y0 + 16;
        if (cand_y > RADAR_Y0 + RADAR_H - 12)  cand_y = RADAR_Y0 + RADAR_H - 12;

        if (abs(cand_x - HQ_RADAR_X) < 16 && abs(cand_y - HQ_RADAR_Y) < 12) ok = 0;
        for (i = 0; i < MAX_RADAR_EVENTS && ok; i++) {
            if (g_camp.events[i].active) {
                int d = abs(cand_x - g_camp.events[i].rx) + abs(cand_y - g_camp.events[i].ry);
                if (d < 34) ok = 0;
            }
        }

        chosen_biome = b;
        chosen_site  = s;
        rx = cand_x;
        ry = cand_y;
        if (ok) break;
    }

    g_camp.next_event_num++;

    ev = &g_camp.events[slot];
    memset(ev, 0, sizeof(*ev));
    ev->active      = 1;
    ev->event_num   = g_camp.next_event_num;
    ev->biome_id    = chosen_biome;
    ev->ufo_size    = ufo_sz;
    ev->is_flying   = 1;
    ev->severity    = 1;
    ev->ufo_hp_max  = (ufo_sz == 0) ? 60 : (ufo_sz == 1 ? 120 : (ufo_sz == 2 ? 190 : 260));
    ev->ufo_hp      = ev->ufo_hp_max;
    ev->vx          = (rand() & 1) ? 2 : -2;
    ev->vy          = (rand() & 1) ? 1 : -1;
    ev->flight_mins = rrange_camp(48, 108);
    ev->rx          = rx;
    ev->ry          = ry;
    ev->age_ticks   = 0;
    ev->ttl_ticks   = 960;
    ev->type_title  = (ufo_sz == 0) ? "FLYING SCOUT" :
                      (ufo_sz == 1) ? "FLYING HARVESTER" :
                      (ufo_sz == 2) ? "FLYING ABDUCTOR" : "COMMAND DREADNOUGHT";
    ev->loc_name    = valley_sites[chosen_biome][chosen_site].loc_name;
    ev->intel_desc  = "AIRBORNE UFO TRACKED OVER VALLEY";
    update_event_nearest_site(ev);

    snprintf(ev->tag, sizeof(ev->tag), "AIR-%02u", (unsigned)(ev->event_num % 100));

    g_camp.last_reported_idx   = slot;
    g_camp.report_banner_timer = 260;
    g_camp.time_speed          = 0;
    if (g_camp.selected_event < 0 || !g_camp.events[g_camp.selected_event].active) {
        g_camp.selected_event = slot;
    }

    snprintf(g_camp.notice, sizeof(g_camp.notice), "RADAR: %s OVER %s!",
             ev->type_title, ev->loc_name);

    if (play_sfx && g_camp.mode == 0) {
        char l1[44], l2[44];
        snprintf(l1, sizeof(l1), "CONTACT %s: %s", ev->tag, ev->type_title);
        snprintf(l2, sizeof(l2), "SECTOR: %s", ev->loc_name);
        campaign_push_modal("VALLEY RADAR CONTACT!", l1, l2,
                            "TRACKING BOGEY IN LEHIGH VALLEY AIRSPACE.", 80, 1);
    }
    return slot;
}


static int has_facility(int fac_id) {
    int r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            int f = g_camp.bunker_grid[r][c];
            if (f == fac_id) return 1;
            if (fac_id == FAC_MISSILE_DEF && f == FAC_OFFENSE_SYS) return 1;
        }
    }
    return 0;
}

int campaign_has_facility(int fac_type) {
    return has_facility(fac_type);
}

int campaign_count_facility(int fac_type) {
    int r, c, cnt = 0;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            if (g_camp.bunker_grid[r][c] == fac_type) cnt++;
        }
    }
    return cnt;
}

int campaign_can_research_tech(int tech_id) {
    const TechDef *td;
    if (tech_id < 0 || tech_id >= TECH_COUNT) return 0;
    if (g_camp.tech_done[tech_id]) return 0;
    td = tech_get_def(tech_id);
    if (td->req_tech != TECH_NONE && !g_camp.tech_done[td->req_tech]) return 0;
    if (td->req_tech2 != TECH_NONE && !g_camp.tech_done[td->req_tech2]) return 0;
    if (td->req_facility >= 0 && !has_facility(td->req_facility)) return 0;
    if (td->req_item != ITEM_NONE &&
        g_camp.stores[td->req_item] <= 0 &&
        g_camp.arv_cargo[td->req_item] <= 0) {
        return 0;
    }
    return 1;
}

int campaign_assigned_scientists(void) {
    int t, sum = 0;
    for (t = 0; t < TECH_COUNT; t++) {
        if (!g_camp.tech_done[t]) sum += g_camp.tech_scientists[t];
    }
    return sum;
}

int campaign_free_scientists(void) {
    int free_s = g_camp.scientists - campaign_assigned_scientists();
    return (free_s > 0) ? free_s : 0;
}

int campaign_count_hired_operatives(void) {
    int i, cnt = 0;
    for (i = 0; i < MAX_ROSTER_SIZE; i++) {
        if (g_camp.squad[i].active) cnt++;
    }
    return cnt;
}

int campaign_count_arv_squad(void) {
    int i, cnt = 0;
    for (i = 0; i < MAX_ROSTER_SIZE; i++) {
        if (g_camp.squad[i].active && g_camp.squad[i].assigned_arv &&
            g_camp.squad[i].psi_trained != 1) {
            cnt++;
        }
    }
    return cnt;
}

int campaign_total_personnel(void) {
    return campaign_count_hired_operatives() + g_camp.scientists + g_camp.engineers;
}

int campaign_max_personnel(void) {
    /* Main Colonial House HQ holds 10 berths; each Living Quarters adds +8 berths */
    return 10 + campaign_count_facility(FAC_QUARTERS) * 8;
}


int campaign_is_item_researched(int item_id) {
    const ItemDef *d = item_get_def(item_id);
    if (d->req_tech == TECH_NONE) return 1;
    if (d->req_tech >= 0 && d->req_tech < TECH_COUNT) {
        return g_camp.tech_done[d->req_tech] ? 1 : 0;
    }
    return 0;
}

int campaign_is_item_discovered(int item_id) {
    const ItemDef *d;
    if (item_id < 0 || item_id >= ITEM_COUNT) return 0;
    if (g_camp.item_seen[item_id] || g_camp.stores[item_id] > 0 || g_camp.arv_cargo[item_id] > 0) {
        return 1;
    }
    d = item_get_def(item_id);
    if (d->category <= ICAT_UTILITY && campaign_is_item_researched(item_id)) {
        return 1;
    }
    return 0;
}


int campaign_store_used_space(void) {
    int i, total = 0;
    for (i = 0; i < ITEM_COUNT; i++) {
        if (g_camp.stores[i] > 0) {
            total += g_camp.stores[i] * item_get_def(i)->store_space;
        }
    }
    return total;
}

int campaign_store_max_space(void) {
    int r, c, extra = 0;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 4; c++)
            if (g_camp.bunker_grid[r][c] == FAC_PLASMA_WORK) extra += 40;
    return 80 + extra;
}

int campaign_arv_used_space(void) {
    int i, total = 0;
    for (i = 0; i < ITEM_COUNT; i++) {
        if (g_camp.arv_cargo[i] > 0) {
            total += g_camp.arv_cargo[i] * item_get_def(i)->store_space;
        }
    }
    return total;
}

int campaign_arv_max_space(void) {
    return g_camp.tech_done[TECH_UFO_CONSTRUCTION] ? 110 : 80;
}

static int take_arv_or_base_item(int id) {
    if (id < 0 || id >= ITEM_COUNT) return 0;
    if (g_camp.arv_cargo[id] > 0) {
        g_camp.arv_cargo[id]--;
        return 1;
    }
    if (g_camp.stores[id] > 0) {
        g_camp.stores[id]--;
        return 1;
    }
    return 0;
}

static void return_unit_item_to_camp(int id) {
    const ItemDef *d;
    if (id < 0 || id >= ITEM_COUNT) return;
    d = item_get_def(id);
    if (d->category <= ICAT_UTILITY && campaign_is_item_researched(id) &&
        campaign_arv_used_space() + d->store_space <= campaign_arv_max_space()) {
        g_camp.arv_cargo[id]++;
    } else {
        g_camp.stores[id]++;
    }
}

void campaign_sync_squad_units(void) {
    static const int wep_prio[] = {
        ITEM_PLASMA_RIFLE, ITEM_HEAVY_PLASMA, ITEM_LASER_RIFLE, ITEM_HEAVY_LASER,
        ITEM_ASSAULT_RIFLE, ITEM_HEAVY_RIFLE, ITEM_PLASMA_PISTOL, ITEM_LASER_PISTOL, ITEM_PISTOL
    };
    static const int gren_prio[] = {
        ITEM_ALIEN_GRENADE, ITEM_STUN_GRENADE, ITEM_FRAG_GRENADE, ITEM_HEAVY_GRENADE
    };
    int r, k, slot = 0;
    int bonus_hp = has_facility(FAC_MEDBAY) ? 15 : 0;
    int bonus_tu = has_facility(FAC_ARCADE_REC) ? 6 : 0;

    /* Clear all 8 squad unit slots first */
    for (slot = 0; slot < MAX_SQUAD_SIZE; slot++) {
        if (g_camp.mode == 1 &&
            (unsigned)g_units[slot].x < MAP_W &&
            (unsigned)g_units[slot].y < MAP_H &&
            (unsigned)g_units[slot].z < MAP_LEVS &&
            g_map[g_units[slot].z][g_units[slot].y][g_units[slot].x].unit_idx == slot) {
            g_map[g_units[slot].z][g_units[slot].y][g_units[slot].x].unit_idx = -1;
        }
        g_units[slot].active     = 0;
        g_units[slot].hp_max     = 0;
        g_units[slot].hp         = 0;
        g_units[slot].roster_idx = -1;
        g_units[slot].name[0]    = '\0';
    }

    slot = 0;
    for (r = 0; r < MAX_ROSTER_SIZE && slot < MAX_SQUAD_SIZE; r++) {
        const Operative *op = &g_camp.squad[r];
        Unit *u;
        int wep;
        int cur_slot;

        if (!op->active || !op->assigned_arv || op->psi_trained == 1) continue;

        cur_slot = slot++;
        u = &g_units[cur_slot];
        wep = (item_is_weapon(op->weapon_id) && campaign_is_item_researched(op->weapon_id))
              ? op->weapon_id : ITEM_ASSAULT_RIFLE;
        u->active      = 1;
        if (g_camp.mode == 1 &&
            (unsigned)u->x < MAP_W && (unsigned)u->y < MAP_H && (unsigned)u->z < MAP_LEVS) {
            g_map[u->z][u->y][u->x].unit_idx = (int8_t)cur_slot;
        }
        u->faction     = FACTION_DUFFCOM;
        u->orig_faction= FACTION_DUFFCOM;
        u->unit_type   = 0;
        u->roster_idx  = r;
        operative_format_full_name(op, u->name, sizeof(u->name));
        u->gender      = op->gender;
        u->portrait_id = op->portrait_id % NUM_PORTRAITS;
        u->sprite_id   = gfx_portrait_to_unit_sprite(u->portrait_id);
        u->rank        = op->rank;
        u->xp_start    = op->xp;
        u->xp_gained   = 0;
        u->kills       = 0;
        u->tu_max      = op->tu_max + bonus_tu;
        u->tu          = u->tu_max;
        u->hp_max      = op->hp_max + bonus_hp;
        u->hp          = u->hp_max;
        u->energy_max      = op->energy_max;
        u->base_energy_max = op->energy_max;
        u->energy          = u->energy_max;
        u->firing_acc  = op->firing_acc;
        u->throw_acc   = op->throw_acc;
        u->reactions   = op->reactions;
        u->bravery     = op->bravery ? op->bravery : 70;
        u->morale      = 100;
        u->psi_skill   = op->psi_skill;
        u->psi_trained = op->psi_trained;
        u->is_corpse      = 0;
        u->is_unconscious = 0;
        u->stun_dmg       = 0;
        u->bleed_turns    = 0;
        u->breakdown_turns    = 0;
        u->mind_control_turns = 0;
        u->is_zombie      = 0;
        u->is_drone       = 0;

        if (g_camp.mode == 1) {
            /* Active Mission Deployment: draw initial operative equipment from ARV Bus Cargo! */
            int chosen_wep = ITEM_NONE;
            u->active_hand = 0;
            u->rhand_item  = ITEM_NONE;
            u->rhand_ammo  = 0;
            u->lhand_item  = ITEM_NONE;
            u->lhand_ammo  = 0;
            for (k = 0; k < INV_BELT_SMALL; k++)  u->belt_small[k] = ITEM_NONE;
            for (k = 0; k < INV_BELT_LARGE; k++)  u->belt_large[k] = ITEM_NONE;
            for (k = 0; k < INV_BACKPACK_SZ; k++) u->backpack[k]   = ITEM_NONE;
            memset(u->slot_prime, -1, sizeof(u->slot_prime));

            if (item_is_weapon(wep) && campaign_is_item_researched(wep) && take_arv_or_base_item(wep)) {
                chosen_wep = wep;
            } else {
                for (k = 0; k < (int)(sizeof(wep_prio) / sizeof(wep_prio[0])); k++) {
                    int cand = wep_prio[k];
                    if (campaign_is_item_researched(cand) && take_arv_or_base_item(cand)) {
                        chosen_wep = cand;
                        break;
                    }
                }
            }
            if (chosen_wep >= 0) {
                int ammo_id = item_get_def(chosen_wep)->ammo_item_id;
                u->rhand_item = chosen_wep;
                u->rhand_ammo = item_get_def(chosen_wep)->clip_size;
                if (ammo_id >= 0 && campaign_is_item_researched(ammo_id)) {
                    for (k = 0; k < 2; k++) {
                        if (take_arv_or_base_item(ammo_id)) {
                            if (!isomap_give_unit_item(u, ammo_id)) {
                                return_unit_item_to_camp(ammo_id);
                                break;
                            }
                        }
                    }
                }
            }
            /* Issue Special Tactical Gear (Medkit, Mind Probe, Stun Rod) if preferred or in cargo */
            if (op->special_id >= 0 && campaign_is_item_researched(op->special_id) &&
                take_arv_or_base_item(op->special_id)) {
                if (!isomap_give_unit_item(u, op->special_id)) {
                    return_unit_item_to_camp(op->special_id);
                }
            } else {
                if (u->psi_trained == 2 && campaign_is_item_researched(ITEM_MIND_PROBE) &&
                    take_arv_or_base_item(ITEM_MIND_PROBE)) {
                    if (!isomap_give_unit_item(u, ITEM_MIND_PROBE)) return_unit_item_to_camp(ITEM_MIND_PROBE);
                } else if (campaign_is_item_researched(ITEM_MEDKIT) && take_arv_or_base_item(ITEM_MEDKIT)) {
                    if (!isomap_give_unit_item(u, ITEM_MEDKIT)) return_unit_item_to_camp(ITEM_MEDKIT);
                } else if (take_arv_or_base_item(ITEM_STUN_ROD)) {
                    if (!isomap_give_unit_item(u, ITEM_STUN_ROD)) return_unit_item_to_camp(ITEM_STUN_ROD);
                }
            }
            for (k = 0; k < (int)(sizeof(gren_prio) / sizeof(gren_prio[0])); k++) {
                int g_cand = gren_prio[k];
                if (campaign_is_item_researched(g_cand) && take_arv_or_base_item(g_cand)) {
                    if (!isomap_give_unit_item(u, g_cand)) {
                        return_unit_item_to_camp(g_cand);
                    }
                    break;
                }
            }
            if (take_arv_or_base_item(ITEM_FLARE)) {
                if (!isomap_give_unit_item(u, ITEM_FLARE)) {
                    return_unit_item_to_camp(ITEM_FLARE);
                }
            }
            isomap_sync_unit_inventory(u);
        } else {
            /* Startup / Snapshot preview before mission launch: populate slots without depleting ARV cargo */
            u->weapon_id   = wep;
            u->sidearm_id  = op->sidearm_id;
            u->special_id  = op->special_id;
            u->ammo        = item_get_def(wep)->clip_size;
            u->extra_clips = (op->extra_clips > 0) ? op->extra_clips : 2;
            u->grenade_id  = (op->grenade_id >= 0 && campaign_is_item_researched(op->grenade_id))
                             ? op->grenade_id : ITEM_FRAG_GRENADE;
            u->grenades    = op->grenades;
            u->flares      = op->flares;
            isomap_init_unit_slots_from_summary(u);
        }
    }
    if (g_camp.mode == 1) {
        isomap_recalc_lighting_and_fov();
    }
}

void campaign_init(void) {
    memset(&g_camp, 0, sizeof(g_camp));
    g_camp.mode = 0;
    g_camp.active_tab = 0;
    g_camp.time_speed = 0;
    g_camp.selected_sector = 0;
    g_camp.selected_soldier = 0;
    g_camp.selected_event = -1;
    g_camp.launched_event = -1;
    g_camp.last_reported_idx = -1;
    g_camp.valley_day = 1;
    g_camp.valley_minute = 13 * 60 + 30; /* 13:30 Daylight in Coopersburg */
    g_camp.deploy_night = 0;

    /* Phase 1 Starting Economy, Scientists, Engineers, Base Stores & ARV Cargo */
    g_camp.money          = 12000;
    g_camp.scientists     = 2;
    g_camp.engineers      = 2;
    g_camp.active_tech    = TECH_LASER_WEAPONS;
    g_camp.tech_scientists[TECH_LASER_WEAPONS] = 2; /* Assign starting 2 scientists */
    g_camp.arv_drone_item = ITEM_NONE;
    g_camp.elerium        = 0;
    g_camp.alloys         = 10;

    g_camp.stores[ITEM_PISTOL]        = 4;
    g_camp.stores[ITEM_ASSAULT_RIFLE] = 4;
    g_camp.stores[ITEM_HEAVY_RIFLE]   = 2;
    g_camp.stores[ITEM_PISTOL_CLIP]   = 8;
    g_camp.stores[ITEM_RIFLE_MAG]     = 12;
    g_camp.stores[ITEM_HEAVY_MAG]     = 6;
    g_camp.stores[ITEM_FRAG_GRENADE]  = 8;
    g_camp.stores[ITEM_HEAVY_GRENADE] = 4;
    g_camp.stores[ITEM_FLARE]         = 8;
    g_camp.stores[ITEM_ALLOYS]        = 10;

    g_camp.arv_cargo[ITEM_ASSAULT_RIFLE] = 6;
    g_camp.arv_cargo[ITEM_PISTOL]        = 5;
    g_camp.arv_cargo[ITEM_HEAVY_RIFLE]   = 1;
    g_camp.arv_cargo[ITEM_RIFLE_MAG]     = 14;
    g_camp.arv_cargo[ITEM_PISTOL_CLIP]   = 6;
    g_camp.arv_cargo[ITEM_HEAVY_MAG]     = 3;
    g_camp.arv_cargo[ITEM_FRAG_GRENADE]  = 8;
    g_camp.arv_cargo[ITEM_HEAVY_GRENADE] = 2;
    g_camp.arv_cargo[ITEM_FLARE]         = 6;

    /* Initialize starting 5 operatives in the 16-slot HQ roster & replacement recruit pool */
    operative_load_starting_squad(g_camp.squad, MAX_ROSTER_SIZE);
    operative_refresh_recruit_pool(g_camp.recruit_pool, MAX_RECRUIT_POOL,
                                   g_camp.squad, MAX_ROSTER_SIZE);

    /* Initial Coopersburg Colonial House Base Layout:
     * Center [1][1] = Main Colonial House HQ
     * [1][2] = Armored Response Vehicle Garage
     * [0][1] = Homemade Radar Unit
     * [1][0] = Homemade Lab / Workshop */
    g_camp.bunker_grid[1][1] = FAC_MAIN_HOUSE;
    g_camp.bunker_grid[1][2] = FAC_GARAGE;
    g_camp.bunker_grid[0][1] = FAC_RADAR;
    g_camp.bunker_grid[1][0] = FAC_LAB;


    /* Give the player quiet preparation time at game start before the first UFO arrives! */
    g_camp.next_spawn_timer = rrange_camp(960, 1440); /* ~16 to 24 campaign hours */
    snprintf(g_camp.notice, sizeof(g_camp.notice),
             "HQ ONLINE: USE TIME BUTTONS (1S..1D) TO SCAN");
}

static void start_ufo_interception(int ev_idx) {
    if (ev_idx < 0 || ev_idx >= MAX_RADAR_EVENTS || !g_camp.events[ev_idx].active) return;
    g_camp.launch_prompt_open = 0;
    g_camp.intercept_open     = 1;
    g_camp.intercept_ev       = ev_idx;
    g_camp.intercept_tx       = 124;
    g_camp.intercept_ty       = 78;
    g_camp.intercept_vx       = (rand() & 1) ? 2 : -2;
    g_camp.intercept_vy       = (rand() & 1) ? 1 : -1;
    g_camp.intercept_cooldown = 0;
    g_camp.intercept_timer    = 12 * 70; /* 12 seconds engagement window at 70Hz */
    g_camp.intercept_shot_t   = 0;
    g_camp.intercept_flash    = 0;
    g_camp.time_speed         = 0;
    sfx_alert();
}

static void tick_ufo_interception(void) {
    int ev_idx = g_camp.intercept_ev;
    RadarEvent *ev;
    if (ev_idx < 0 || ev_idx >= MAX_RADAR_EVENTS || !g_camp.events[ev_idx].active) {
        g_camp.intercept_open = 0;
        return;
    }
    ev = &g_camp.events[ev_idx];

    if (g_camp.intercept_cooldown > 0) g_camp.intercept_cooldown--;
    if (g_camp.intercept_shot_t > 0)   g_camp.intercept_shot_t--;
    if (g_camp.intercept_flash > 0)    g_camp.intercept_flash--;

    /* Evasive UFO motion inside the CRT targeting scope */
    if ((g_camp.radar_tick & 1) == 0) {
        g_camp.intercept_tx += g_camp.intercept_vx;
        g_camp.intercept_ty += g_camp.intercept_vy;
        if (g_camp.intercept_tx < 42 || g_camp.intercept_tx > 206) {
            g_camp.intercept_vx = -g_camp.intercept_vx;
        }
        if (g_camp.intercept_ty < 36 || g_camp.intercept_ty > 122) {
            g_camp.intercept_vy = -g_camp.intercept_vy;
        }
        if ((rand() % 28) == 0) {
            int spd = (ev->ufo_size == 0) ? 3 : 2;
            g_camp.intercept_vx = ((rand() & 1) ? 1 : -1) * (1 + (rand() % spd));
            g_camp.intercept_vy = ((rand() & 1) ? 1 : -1) * (1 + (rand() & 1));
        }
    }

    /* While pursuing, the UFO also moves across the Lehigh Valley Radar map! */
    if ((g_camp.radar_tick % 35) == 0) {
        ev->rx += ev->vx;
        ev->ry += ev->vy;
        if (ev->rx < RADAR_X0 + 16 || ev->rx > RADAR_X0 + RADAR_W - 18) ev->vx = -ev->vx;
        if (ev->ry < RADAR_Y0 + 16 || ev->ry > RADAR_Y0 + RADAR_H - 12) ev->vy = -ev->vy;
        update_event_nearest_site(ev);
    }

    g_camp.intercept_timer--;
    if (g_camp.intercept_timer <= 0) {
        g_camp.intercept_open = 0;
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "%s EVADED LOCK OVER %s!", ev->tag, ev->loc_name);
        sfx_beep(220, 60);
    }
}



static void resolve_flying_ufo_pass(int i, RadarEvent *ev) {
    int has_offense = has_facility(FAC_TURRET);
    int roll = rand() % 100;
    char l1[44], l2[44];
    update_event_nearest_site(ev);

    /* Without base offense: ~32% X-COM shootdown, ~32% Landed UFO, ~36% Flies across & disappears!
     * With base offense (if player ignored intercept): ~50% Landed UFO, ~50% Flies across & disappears! */
    if (!has_offense && (roll < 32)) {
        /* Regional X-COM Interceptor shoots down the UFO -> Clean-up Mission! */
        ev->is_flying  = 0;
        ev->severity   = 0;
        ev->age_ticks  = 0;
        ev->ttl_ticks  = 1440;
        ev->type_title = (ev->ufo_size == 0) ? "CRASHED SCOUT" :
                         (ev->ufo_size == 1) ? "CRASHED HARVESTER" :
                         (ev->ufo_size == 2) ? "CRASHED ABDUCTOR" : "DOWNED COMMAND SHIP";
        ev->intel_desc = "SHOT DOWN BY X-COM - CLEAN UP!";
        snprintf(ev->tag, sizeof(ev->tag), "SITE-%02u", (unsigned)(ev->event_num % 100));
        g_camp.time_speed          = 0;
        g_camp.last_reported_idx   = i;
        g_camp.report_banner_timer = 260;
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "X-COM SHOT DOWN %s @ %s!", ev->tag, ev->loc_name);
        snprintf(l1, sizeof(l1), "%s DOWN AT %s", ev->type_title, ev->loc_name);
        campaign_push_modal("UFO SHOT DOWN BY X-COM!", l1,
                            "REGIONAL INTERCEPTORS DOWNED THE CRAFT.",
                            "DEPLOY ARV SQUAD FOR CRASH SITE SALVAGE!", 144, 2);

    } else if ((!has_offense && roll < 64) || (has_offense && roll < 52)) {
        /* UFO lands intact -> Aliens are operating on the ground! */
        ev->is_flying  = 0;
        ev->severity   = (rand() % 100 < 40) ? 2 : 1;
        ev->age_ticks  = 0;
        ev->ttl_ticks  = 960;
        ev->type_title = (ev->ufo_size == 0) ? "LANDED SCOUT" :
                         (ev->ufo_size == 1) ? "LANDED HARVESTER" :
                         (ev->ufo_size == 2) ? "LANDED ABDUCTOR" : "LANDED COMMAND SHIP";
        ev->intel_desc = "UFO LANDED INTACT - ALIENS ATTACKING!";
        snprintf(ev->tag, sizeof(ev->tag), "UFO-%02u", (unsigned)(ev->event_num % 100));
        g_camp.time_speed          = 0;
        g_camp.last_reported_idx   = i;
        g_camp.report_banner_timer = 260;
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "ALERT: %s @ %s!", ev->type_title, ev->loc_name);
        snprintf(l1, sizeof(l1), "%s AT %s", ev->type_title, ev->loc_name);
        campaign_push_modal("ALIEN UFO TOUCHDOWN!", l1,
                            "HOSTILE FORCES DEPLOYING IN SECTOR!",
                            "SELECT CONTACT ON RADAR TO LAUNCH ARV.", 96, 1);
    } else {
        /* UFO flies straight across the radar screen and disappears without a mission! */
        snprintf(l1, sizeof(l1), "%s (%s) FLEW PAST VALLEY", ev->tag, ev->type_title);
        snprintf(l2, sizeof(l2), "LAST TRACKED OVER %s", ev->loc_name);
        ev->active = 0;
        if (g_camp.selected_event == i) {
            g_camp.selected_event = first_active_event();
        }
        snprintf(g_camp.notice, sizeof(g_camp.notice),
                 "%s FLEW ACROSS VALLEY & ESCAPED!", ev->tag);
        campaign_push_modal("UFO LEFT VALLEY AIRSPACE!", l1, l2,
                            has_offense ? "BOGEY ESCAPED BEFORE INTERCEPTION."
                                        : "BUILD BASE OFFENSE TURRETS TO SHOOT UFOS!", 33, 0);
    }
}

static void advance_campaign_minutes(int mins) {
    int m, i, t;
    for (m = 0; m < mins; m++) {
        /* If an Event Modal just popped up, immediately stop advancing further minutes! */
        if (g_camp.modal_open != 0) break;

        g_camp.valley_minute++;
        if (g_camp.valley_minute >= 1440) {
            g_camp.valley_minute = 0;
            g_camp.valley_day++;
        }

        /* Hourly Base Progression: Research, Workshop Manufacturing, Store Deliveries, Psi-Training */
        if ((g_camp.valley_minute % 60) == 0) {
            /* 1. Multi-Project Scientist Research Progression */
            for (t = 0; t < TECH_COUNT; t++) {
                if (!g_camp.tech_done[t] && g_camp.tech_scientists[t] > 0) {
                    const TechDef *td = tech_get_def(t);
                    g_camp.tech_prog[t] += g_camp.tech_scientists[t];
                    if (g_camp.tech_prog[t] >= td->research_cost) {
                        char l1[44];
                        g_camp.tech_prog[t]       = td->research_cost;
                        g_camp.tech_done[t]       = 1;
                        g_camp.tech_scientists[t] = 0; /* Return scientists to available pool */
                        g_camp.log_sel            = t;
                        if (t == TECH_ARV_TURRET && g_camp.arv_turret_level < 1) {
                            g_camp.arv_turret_level = 1;
                        } else if (t == TECH_ARV_TURRET_MK2) {
                            g_camp.arv_turret_level = 2;
                        }
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "RESEARCH COMPLETE: %s!", td->title);
                        snprintf(l1, sizeof(l1), "PROJECT: %s", td->title);
                        campaign_push_modal("RESEARCH PROJECT COMPLETED!", l1,
                                            td->unlock_summary,
                                            "REPORT LOGGED IN UFOPAEDIA RESEARCH LOG.", 80, 0);
                    }
                }
            }

            /* 2. Workshop Timed Item Manufacturing */
            if (g_camp.engineers > 0) {
                int active_orders = 0;
                for (i = 0; i < ITEM_COUNT; i++) {
                    if (g_camp.mfg_queue[i] > 0) active_orders++;
                }
                if (active_orders > 0) {
                    int pts_each = g_camp.engineers / active_orders;
                    int rem_pts  = g_camp.engineers % active_orders;
                    for (i = 0; i < ITEM_COUNT; i++) {
                        if (g_camp.mfg_queue[i] > 0) {
                            const ItemDef *id_def = item_get_def(i);
                            int need = (id_def->mfg_hours > 0) ? id_def->mfg_hours : 12;
                            int add  = pts_each + (rem_pts > 0 ? 1 : 0);
                            if (rem_pts > 0) rem_pts--;
                            g_camp.mfg_prog[i] += add;
                            if (g_camp.mfg_prog[i] >= need) {
                                char l1[44];
                                g_camp.mfg_prog[i] -= need;
                                g_camp.mfg_queue[i]--;
                                if (g_camp.mfg_queue[i] == 0) g_camp.mfg_prog[i] = 0;
                                return_unit_item_to_camp(i);
                                g_camp.item_seen[i] = 1;
                                if (i == ITEM_QUAD_DRONE || i == ITEM_UFO_DRONE) {
                                    g_camp.arv_drone_item = i;
                                }
                                snprintf(g_camp.notice, sizeof(g_camp.notice),
                                         "WORKSHOP BUILT: %s!", id_def->full_name);
                                snprintf(l1, sizeof(l1), "BUILT: 1x %s", id_def->full_name);
                                campaign_push_modal("WORKSHOP PRODUCTION COMPLETE!", l1,
                                                    "ENGINEERS COMPLETED FABRICATION.",
                                                    "ITEM STORED IN HQ / ARV ARMORY.", 144, 0);
                            }
                        }
                    }
                }
            }

            /* 3. Quartermaster Store Delivery Transit */
            for (i = 0; i < ITEM_COUNT; i++) {
                if (g_camp.store_transit_qty[i] > 0) {
                    if (g_camp.store_transit_hrs[i] > 1) {
                        g_camp.store_transit_hrs[i]--;
                    } else {
                        const ItemDef *id_def = item_get_def(i);
                        int qty = g_camp.store_transit_qty[i], k;
                        char l1[44];
                        g_camp.store_transit_hrs[i] = 0;
                        g_camp.store_transit_qty[i] = 0;
                        for (k = 0; k < qty; k++) {
                            return_unit_item_to_camp(i);
                        }
                        g_camp.item_seen[i] = 1;
                        if (i == ITEM_QUAD_DRONE || i == ITEM_UFO_DRONE) {
                            g_camp.arv_drone_item = i;
                        }
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "DELIVERY ARRIVED: +%d %s!", qty, id_def->short_name);
                        snprintf(l1, sizeof(l1), "RECEIVED: %dx %s", qty, id_def->full_name);
                        campaign_push_modal("QUARTERMASTER DELIVERY ARRIVED!", l1,
                                            "SHIPMENT UNLOADED AT COOPERSBURG HQ.",
                                            "ITEMS NOW AVAILABLE FOR SQUAD USE!", 96, 0);
                    }
                }
            }

            /* 4. Psi-Lab Operative Psionic Training */
            for (i = 0; i < MAX_ROSTER_SIZE; i++) {
                Operative *op = &g_camp.squad[i];
                if (op->active && op->psi_trained == 1) {
                    if (op->psi_hours_left > 1) {
                        op->psi_hours_left--;
                    } else {
                        char l1[44], l2[44];
                        op->psi_hours_left = 0;
                        op->psi_trained    = 2; /* Revealed & Psi-Capable! */
                        if (campaign_count_arv_squad() < MAX_SQUAD_SIZE) {
                            op->assigned_arv = 1;
                        }
                        snprintf(g_camp.notice, sizeof(g_camp.notice),
                                 "PSI-TRAINING DONE: %s (PSI:%d)!", op->name, op->psi_skill);
                        snprintf(l1, sizeof(l1), "OPERATIVE: %s %s",
                                 operative_rank_prefix(op->rank), op->name);
                        snprintf(l2, sizeof(l2), "REVEALED PSIONIC ABILITY: %d", op->psi_skill);
                        campaign_push_modal("PSIONIC TRAINING COMPLETED!", l1, l2,
                                            "AGENT AVAILABLE FOR MISSIONS & MIND PROBE!", 81, 0);
                    }
                }
            }
        }

        for (i = 0; i < MAX_RADAR_EVENTS; i++) {
            RadarEvent *ev = &g_camp.events[i];
            if (!ev->active) continue;

            if (ev->is_flying) {
                if ((g_camp.valley_minute & 3) == 0) {
                    ev->rx += ev->vx;
                    ev->ry += ev->vy;
                    if (ev->rx < RADAR_X0 + 16)           { ev->rx = RADAR_X0 + 16;           ev->vx = -ev->vx; }
                    if (ev->rx > RADAR_X0 + RADAR_W - 18) { ev->rx = RADAR_X0 + RADAR_W - 18; ev->vx = -ev->vx; }
                    if (ev->ry < RADAR_Y0 + 16)           { ev->ry = RADAR_Y0 + 16;           ev->vy = -ev->vy; }
                    if (ev->ry > RADAR_Y0 + RADAR_H - 12) { ev->ry = RADAR_Y0 + RADAR_H - 12; ev->vy = -ev->vy; }
                    update_event_nearest_site(ev);
                }
                if (--ev->flight_mins <= 0) {
                    resolve_flying_ufo_pass(i, ev);
                }
            } else {
                if (++ev->age_ticks >= ev->ttl_ticks) {
                    char l1[44];
                    snprintf(l1, sizeof(l1), "%s (%s) DEPARTED SECTOR", ev->tag, ev->type_title);
                    ev->active = 0;
                    snprintf(g_camp.notice, sizeof(g_camp.notice),
                             "EXPIRED: %s DEPARTED SECTOR", ev->tag);
                    if (g_camp.selected_event == i) {
                        g_camp.selected_event = first_active_event();
                    }
                    campaign_push_modal("ALIEN CONTACT LOST!", l1,
                                        "ALIEN FORCES COMPLETED OPERATIONS AND",
                                        "DEPARTED BEFORE SQUAD INTERVENTION.", 33, 0);
                }
            }
        }

        if (--g_camp.next_spawn_timer <= 0) {
            int max_simul = (g_camp.valley_day <= 4 && g_camp.missions_won < 2) ? 1 :
                            (g_camp.valley_day <= 10 ? 2 : MAX_RADAR_EVENTS);
            if (count_active_events() < max_simul) {
                spawn_random_radar_event(1);
            }
            g_camp.next_spawn_timer = roll_next_ufo_delay_mins();
        }
    }
}

void campaign_tick(void) {
    int mins_to_advance = 0;

    g_camp.radar_tick++;
    if (g_camp.report_banner_timer > 0) g_camp.report_banner_timer--;

    /* Animate Command Center Event Modal Popup if active; pause all time passing! */
    if (g_camp.modal_open != 0) {
        g_camp.time_speed = 0;
        if (g_camp.modal_open == 1) {
            if (g_camp.modal_anim_frame < 8) g_camp.modal_anim_frame++;
            else g_camp.modal_open = 2;
        } else if (g_camp.modal_open == 3) {
            if (g_camp.modal_anim_frame > 0) {
                g_camp.modal_anim_frame--;
            } else {
                int q;
                for (q = 0; q < g_camp.modal_count - 1; q++) {
                    g_camp.modal_queue[q] = g_camp.modal_queue[q + 1];
                }
                if (g_camp.modal_count > 0) g_camp.modal_count--;
                if (g_camp.modal_count > 0) {
                    g_camp.modal_open       = 1;
                    g_camp.modal_anim_frame = 0;
                    sfx_beep(523, 30);
                } else {
                    g_camp.modal_open = 0;
                }
            }
        }
        return;
    }

    if (g_camp.intercept_open) {
        tick_ufo_interception();
        return;
    }
    if (g_camp.launch_prompt_open) return;

    if (g_camp.time_speed == 0) {
        if ((g_camp.radar_tick % 70) == 0) mins_to_advance = 1;
    } else if (g_camp.time_speed == 1) {
        if ((g_camp.radar_tick % 14) == 0) mins_to_advance = 1;
    } else if (g_camp.time_speed == 2) {
        mins_to_advance = 1;
    } else {
        mins_to_advance = 20;
    }

    if (mins_to_advance > 0) {
        advance_campaign_minutes(mins_to_advance);
    }
}


static void launch_selected_mission(void) {
    int ev_idx = g_camp.selected_event;

    if (ev_idx >= 0 && ev_idx < MAX_RADAR_EVENTS && g_camp.events[ev_idx].active) {
        g_camp.selected_sector = g_camp.events[ev_idx].biome_id;
        g_camp.launched_event  = ev_idx;
        g_battle.ufo_class     = g_camp.events[ev_idx].ufo_size;
    } else {
        g_camp.launched_event = -1;
        g_battle.ufo_class    = 0;
    }

    g_camp.launch_prompt_open = 0;
    g_camp.deploy_night       = is_valley_night();
    sfx_laser();
    isomap_start_mission(g_camp.selected_sector, g_camp.deploy_night);

    /* Apply Operative Ranks, Stats, Portraits, Armory Loadouts & Bunker Perks to Squad */
    g_camp.mode = 1;
    campaign_sync_squad_units();
}

static void show_alien_victory_gameover_screen(void);

void campaign_on_mission_complete(void) {
    int i, k;
    int kia_cnt = 0;
    int promo_cnt = 0;
    int focus_slot = -1;
    char promo_name[24] = "";
    char recruit_name[24] = "";
    int mission_won = (g_battle.mission_ended == 1);
    int squad_wiped = (g_battle.mission_ended == 2);
    int contested_retreat = (!mission_won && !squad_wiped &&
                             (g_battle.turn_number > 1 || g_battle.aliens_killed > 0));

    g_camp.debrief_live_captured       = 0;
    g_camp.debrief_died_no_containment = 0;

    for (i = 0; i < MAX_SQUAD_SIZE; i++) {
        Unit *u = &g_units[i];
        int r_idx = u->roster_idx;
        Operative *op;
        int dead_on_field;
        int left_outside_bus;
        int fell_in_action;

        if (r_idx < 0 || r_idx >= MAX_ROSTER_SIZE || !g_camp.squad[r_idx].active) continue;
        op = &g_camp.squad[r_idx];

        /* Note: unconscious operatives (u->is_unconscious && u->hp > 0) are recovered ALIVE if mission_won! */
        dead_on_field    = (!u->active || u->hp <= 0 || u->is_zombie || (u->is_corpse && !u->is_unconscious));
        left_outside_bus = (!mission_won && !dead_on_field && (u->is_unconscious || !isomap_unit_in_bus(u)));
        fell_in_action   = squad_wiped || dead_on_field || left_outside_bus;

        if (fell_in_action) {
            kia_cnt++;
            g_camp.total_kia++;
            operative_record_memorial(g_camp.memorial, MAX_MEMORIAL, op,
                                      g_camp.valley_day,
                                      left_outside_bus ? "MIA (LEFT BEHIND)" : "KIA IN ACTION");
            if (g_camp.memorial_count < MAX_MEMORIAL) g_camp.memorial_count++;

            /* On Squad Wipe, all carried weapons and equipment on the team are permanently LOST!
             * And fallen operatives leave an open berth (or if 0 operatives remain at HQ, 1 rookie joins). */
            op->active       = 0;
            op->assigned_arv = 0;
            if (!squad_wiped && mission_won) {
                /* If squad won the mission, recover fallen teammate's gear from the battlefield */
                if (u->rhand_item >= 0) { return_unit_item_to_camp(u->rhand_item); u->rhand_item = ITEM_NONE; }
                if (u->lhand_item >= 0) { return_unit_item_to_camp(u->lhand_item); u->lhand_item = ITEM_NONE; }
                for (k = 0; k < INV_BELT_SMALL; k++) {
                    if (u->belt_small[k] >= 0) { return_unit_item_to_camp(u->belt_small[k]); u->belt_small[k] = ITEM_NONE; }
                }
                for (k = 0; k < INV_BELT_LARGE; k++) {
                    if (u->belt_large[k] >= 0) { return_unit_item_to_camp(u->belt_large[k]); u->belt_large[k] = ITEM_NONE; }
                }
                for (k = 0; k < INV_BACKPACK_SZ; k++) {
                    if (u->backpack[k] >= 0) { return_unit_item_to_camp(u->backpack[k]); u->backpack[k] = ITEM_NONE; }
                }
            }
        } else {
            int surv_xp = 0;
            int gained;

            /* Record surviving operative's preferred primary weapon and return all carried slot items to ARV / Base Stores */
            if (u->rhand_item >= 0 && item_is_weapon(u->rhand_item) && campaign_is_item_researched(u->rhand_item)) {
                op->weapon_id = u->rhand_item;
            } else if (u->lhand_item >= 0 && item_is_weapon(u->lhand_item) && campaign_is_item_researched(u->lhand_item)) {
                op->weapon_id = u->lhand_item;
            }
            if (u->rhand_item >= 0) { return_unit_item_to_camp(u->rhand_item); u->rhand_item = ITEM_NONE; }
            if (u->lhand_item >= 0) { return_unit_item_to_camp(u->lhand_item); u->lhand_item = ITEM_NONE; }
            for (k = 0; k < INV_BELT_SMALL; k++) {
                if (u->belt_small[k] >= 0) { return_unit_item_to_camp(u->belt_small[k]); u->belt_small[k] = ITEM_NONE; }
            }
            for (k = 0; k < INV_BELT_LARGE; k++) {
                if (u->belt_large[k] >= 0) { return_unit_item_to_camp(u->belt_large[k]); u->belt_large[k] = ITEM_NONE; }
            }
            for (k = 0; k < INV_BACKPACK_SZ; k++) {
                if (u->backpack[k] >= 0) { return_unit_item_to_camp(u->backpack[k]); u->backpack[k] = ITEM_NONE; }
            }

            op->kills += u->kills;
            if (u->throw_acc > op->throw_acc) {
                op->throw_acc = u->throw_acc;
            }
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
                    focus_slot = r_idx;
                }
                promo_cnt++;
            }
        }
    }

    /* If Squad Wiped, any deployed ARV Drone on the mission is also lost! */
    if (squad_wiped && g_battle.bus_drone_launched && g_camp.arv_drone_item != ITEM_NONE) {
        int dr_id = g_camp.arv_drone_item;
        if (g_camp.arv_cargo[dr_id] > 0) g_camp.arv_cargo[dr_id]--;
        else if (g_camp.stores[dr_id] > 0) g_camp.stores[dr_id]--;
        if (g_camp.arv_cargo[dr_id] <= 0 && g_camp.stores[dr_id] <= 0) {
            g_camp.arv_drone_item = ITEM_NONE;
        }
    }

    /* Ensure at least 1 active operative exists at HQ if the entire roster was wiped out */
    if (campaign_count_hired_operatives() == 0) {
        operative_hire_replacement(&g_camp.squad[0], g_camp.recruit_pool, MAX_RECRUIT_POOL,
                                   g_camp.squad, MAX_ROSTER_SIZE);
        g_camp.squad[0].assigned_arv = 1;
        operative_format_full_name(&g_camp.squad[0], recruit_name, sizeof(recruit_name));
        focus_slot = 0;
    } else if (campaign_count_arv_squad() == 0) {
        /* Auto-assign an available reserve operative at HQ to the ARV */
        for (i = 0; i < MAX_ROSTER_SIZE; i++) {
            if (g_camp.squad[i].active && g_camp.squad[i].psi_trained != 1) {
                g_camp.squad[i].assigned_arv = 1;
                focus_slot = i;
                break;
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
    g_camp.total_map_damage += g_battle.map_damage;

    /* Count rescued vs lost civilians and update total campaign score */
    {
        int civ_rescued = 0;
        if (mission_won) {
            civ_rescued = g_battle.civilians_saved;
        } else if (!squad_wiped) {
            for (i = 0; i < MAX_UNITS; i++) {
                if (g_units[i].active && g_units[i].faction == FACTION_CIVILIAN &&
                    isomap_unit_in_bus(&g_units[i])) {
                    civ_rescued++;
                }
            }
        }
        g_battle.civilians_saved       = civ_rescued;
        g_camp.total_civilians_saved  += civ_rescued;
        g_camp.total_civilians_killed += g_battle.civilians_killed;
        g_camp.total_score            += g_battle.mission_score;
        g_camp.money                  += civ_rescued * 120 - g_battle.civilians_killed_by_xcom * 250;
        if (g_camp.money < 0) g_camp.money = 0;
    }

    /* Sync recovered materials, artifacts & fallen/captured aliens into Base Stores (only if not wiped!) */
    if (!squad_wiped) {
        g_camp.stores[ITEM_ELERIUM]  += g_battle.elerium_recovered;
        g_camp.stores[ITEM_ALLOYS]   += g_battle.alloys_recovered;
        g_camp.stores[ITEM_UFO_NAV]  += g_battle.nav_recovered;
        g_camp.stores[ITEM_UFO_CRYO] += g_battle.cryo_recovered;
        g_camp.stores[ITEM_UFO_POD]  += g_battle.pods_recovered;
    }

    if (mission_won) {
        int has_containment = has_facility(FAC_CONTAINMENT);
        for (i = 0; i < MAX_UNITS; i++) {
            const Unit *au = &g_units[i];
            if (au->orig_faction == FACTION_ALIEN && !au->is_zombie &&
                (au->is_corpse || au->is_unconscious)) {
                int c_id = (au->unit_type == 5) ? ITEM_CORPSE_CRYPTID :
                           (au->unit_type == 3) ? ITEM_CORPSE_MUTON :
                           (au->unit_type == 2) ? ITEM_CORPSE_FLOATER : ITEM_CORPSE_SECTOID;
                int l_id = (au->unit_type == 5) ? ITEM_LIVE_CRYPTID :
                           (au->unit_type == 3) ? ITEM_LIVE_MUTON :
                           (au->unit_type == 2) ? ITEM_LIVE_FLOATER :
                           (au->alien_rank == 3) ? ITEM_LIVE_SECTOID_CMDR : ITEM_LIVE_SECTOID;
                int w_id = au->weapon_id;
                int a_id = (w_id >= 0) ? item_get_def(w_id)->ammo_item_id : ITEM_NONE;

                if (au->is_unconscious && au->hp > 0) {
                    if (has_containment) {
                        g_camp.stores[l_id]++;
                        g_camp.debrief_live_captured++;
                    } else {
                        /* Captured alien dies upon return to HQ without Alien Containment! */
                        g_camp.stores[c_id]++;
                        g_camp.debrief_died_no_containment++;
                    }
                } else {
                    g_camp.stores[c_id]++;
                }
                if (item_is_weapon(w_id)) g_camp.stores[w_id]++;
                if (item_is_ammo(a_id))   g_camp.stores[a_id]++;
                if (au->grenades > 0)     g_camp.stores[ITEM_ALIEN_GRENADE] += au->grenades;
            }
        }
        g_camp.money += 1500 + g_battle.aliens_killed * 250;
        g_camp.win_streak++;
    } else {
        if (!squad_wiped) {
            g_camp.money += g_battle.aliens_killed * 150;
        }
        g_camp.win_streak = 0;
    }

    for (i = 0; i < ITEM_COUNT; i++) {
        if (g_camp.stores[i] > 0 || g_camp.arv_cargo[i] > 0) {
            g_camp.item_seen[i] = 1;
        }
    }

    if (g_camp.debrief_died_no_containment > 0) {
        char l1[44];
        snprintf(l1, sizeof(l1), "%d CAPTURED ALIEN(S) DIED AT HQ!", g_camp.debrief_died_no_containment);
        campaign_push_modal("NO ALIEN CONTAINMENT AT HQ!", l1,
                            "LIVE SPECIMENS EXPIRED UPON RETURN.",
                            "BUILD ALIEN CONTAINMENT TO KEEP LIVE ALIENS!", 33, 1);
    } else if (g_camp.debrief_live_captured > 0) {
        char l1[44];
        snprintf(l1, sizeof(l1), "%d LIVE ALIEN(S) SECURED IN CONTAINMENT!", g_camp.debrief_live_captured);
        campaign_push_modal("LIVE ALIEN SPECIMEN SECURED!", l1,
                            "ASSIGN SCIENTISTS IN THE RESEARCH TAB TO",
                            "INTERROGATE CAPTURED LIVE SPECIMENS!", 80, 0);
    }
    if (squad_wiped) {
        campaign_push_modal("SQUAD WIPED IN ACTION!",
                            "ALL DEPLOYED OPERATIVES & GEAR LOST!",
                            "REPLENISH WEAPONS FROM STORES, STORE OR",
                            "WORKSHOP & HIRE NEW OPERATIVES AT HQ.", 33, 2);
    }



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
        g_camp.next_spawn_timer = roll_next_ufo_delay_mins();
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

    /* If Aliens won a Home Base Defense mission -> Game Over Screen & Full Reset! */
    if (g_battle.biome_id == BIOME_BASE_DEFENSE && !mission_won) {
        show_alien_victory_gameover_screen();
        return;
    }

    /* Rare Alien Retaliation Attack on Duff-COM Home Base after winning streaks! */
    if (mission_won && g_camp.win_streak >= 3) {
        int atk_chance = (g_camp.win_streak - 2) * 6;
        if (atk_chance > 24) atk_chance = 24;
        if ((rand() % 100) < atk_chance) {
            RadarEvent *bev = &g_camp.events[0];
            g_camp.win_streak = 0;
            g_camp.next_event_num++;
            memset(bev, 0, sizeof(*bev));
            bev->active     = 1;
            bev->event_num  = g_camp.next_event_num;
            bev->biome_id   = BIOME_BASE_DEFENSE;
            bev->ufo_size   = 2;
            bev->is_flying  = 0;
            bev->severity   = 2;
            bev->rx         = HQ_RADAR_X;
            bev->ry         = HQ_RADAR_Y;
            bev->ttl_ticks  = 2880;
            bev->type_title = "BASE RETALIATION";
            bev->loc_name   = "COOPERSBURG HQ BASE";
            bev->intel_desc = "ALIENS ATTACKING DUFF-COM HOME BASE!";
            snprintf(bev->tag, sizeof(bev->tag), "BASE-HQ");
            g_camp.selected_event     = 0;
            g_camp.selected_sector    = BIOME_BASE_DEFENSE;
            g_camp.deploy_night       = is_valley_night();
            g_camp.launch_prompt_open = 1;
            g_camp.active_tab         = 0;
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "RED ALERT: ALIENS ATTACKING DUFF-COM HQ!");
            sfx_alert();
        }
    }

    g_camp.mode = 0;
}

static void show_alien_victory_gameover_screen(void) {
    char buf[64];
    int f;


    sfx_explosion();
    vga_clear(15);
    vga_rect_bevel(18, 16, 284, 168, 36, 96, 45);
    vga_rect_fill(22, 20, 276, 160, 14);

    vga_rect_bevel(28, 26, 264, 22, 41, 96, 45);
    vga_print_shadow(56, 30, "ALIEN VICTORY - GAME OVER", 96, 15);
    vga_print_shadow(44, 39, "COOPERSBURG HQ HAS BEEN OVERRUN!", 32, 15);

    vga_print(34, 62, "THE ALIEN ASSAULT FORCE BREACHED THE", 18);
    vga_print(34, 74, "COLONIAL HOUSE BASE AND DESTROYED THE", 18);
    vga_print(34, 86, "LEHIGH VALLEY RESISTANCE COMMAND.", 18);

    vga_hline(34, 286, 100, 68);
    snprintf(buf, sizeof(buf), "DAYS SURVIVED: %d   MISSIONS WON: %d",
             g_camp.valley_day, g_camp.missions_won);
    vga_print_shadow(38, 108, buf, 80, 15);

    snprintf(buf, sizeof(buf), "ALIENS KILLED: %d   OPERATIVES KIA: %d",
             g_camp.total_kills, g_camp.total_kia);
    vga_print_shadow(38, 118, buf, 96, 15);
    snprintf(buf, sizeof(buf), "CIVS SAVED: %d  LOST: %d  SCORE: %d",
             g_camp.total_civilians_saved, g_camp.total_civilians_killed, g_camp.total_score);
    vga_print_shadow(38, 129, buf, 80, 15);
    vga_hline(34, 286, 140, 68);

    vga_print_shadow(44, 154, "CLICK OR PRESS ANY BUTTON TO RESET", 96, 15);
    vga_flip();

    for (f = 0; f < 45; f++) vga_vsync();
    input_flush();
    for (;;) {
        input_poll();
        if (g_input.left_clicked || g_input.right_clicked || g_input.last_key != 0) {
            sfx_click();
            break;
        }
        vga_vsync();
    }
    input_flush();

    campaign_init();
    isomap_init();
    campaign_sync_squad_units();
    g_camp.game_over_reset = 1;
}

int campaign_save_game(const char *filename) {
    FILE *fp = fopen(filename ? filename : "DUFFSAVE.SAV", "wb");
    uint32_t magic = 0x44554637U;
    uint32_t sz    = (uint32_t)sizeof(CampaignState);
    if (!fp) return 0;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&sz,    sizeof(sz),    1, fp);
    fwrite(&g_camp, sizeof(CampaignState), 1, fp);
    fclose(fp);
    snprintf(g_camp.notice, sizeof(g_camp.notice), "CAMPAIGN SAVED TO %s!",
             filename ? filename : "DUFFSAVE.SAV");
    return 1;
}

int campaign_load_game(const char *filename) {
    FILE *fp = fopen(filename ? filename : "DUFFSAVE.SAV", "rb");
    uint32_t magic = 0, sz = 0;
    CampaignState loaded;
    int i;
    if (!fp) {
        snprintf(g_camp.notice, sizeof(g_camp.notice), "NO SAVE FILE FOUND!");
        return 0;
    }
    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&sz,    sizeof(sz),    1, fp) != 1 ||
        magic != 0x44554637U || sz != (uint32_t)sizeof(CampaignState) ||
        fread(&loaded, sizeof(CampaignState), 1, fp) != 1) {
        fclose(fp);
        snprintf(g_camp.notice, sizeof(g_camp.notice), "INVALID OR OLD SAVE FILE!");
        return 0;
    }
    fclose(fp);

    g_camp = loaded;
    g_camp.mode               = 0;
    g_camp.launch_prompt_open = 0;
    g_camp.intercept_open     = 0;
    g_camp.base_modal         = 0;
    g_camp.time_speed         = 0;
    g_camp.modal_open         = 0;
    g_camp.modal_count        = 0;

    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        RadarEvent *ev = &g_camp.events[i];
        if (!ev->active) continue;
        if (ev->biome_id == BIOME_BASE_DEFENSE) {
            ev->type_title = "BASE RETALIATION";
            ev->loc_name   = "COOPERSBURG HQ BASE";
            ev->intel_desc = "ALIENS ATTACKING DUFF-COM HOME BASE!";
        } else {
            update_event_nearest_site(ev);
            if (ev->is_flying) {
                ev->type_title = (ev->ufo_size == 0) ? "SCOUT UFO" :
                                 (ev->ufo_size == 1) ? "HARVESTER UFO" :
                                 (ev->ufo_size == 2) ? "ABDUCTOR SHIP" : "COMMAND DREADNOUGHT";
                ev->intel_desc = "AIRBORNE UFO TRACKED OVER VALLEY";
            } else {
                ev->type_title = (ev->severity == 0) ? "CRASHED UFO" : "LANDED UFO";
                ev->intel_desc = "ALIEN CONTACT IN LEHIGH VALLEY";
            }
        }
    }
    campaign_sync_squad_units();
    snprintf(g_camp.notice, sizeof(g_camp.notice), "LOADED SAVED GAME (DAY %d)!", g_camp.valley_day);
    return 1;
}

static void radar_pset_clip(int x, int y, uint8_t col) {
    if (x >= 7 && x <= 240 && y >= 28 && y <= 148) {
        vga_pset(x, y, col);
    }
}

static void draw_radar_sweep_ray(int cx, int cy, int dx, int dy, uint8_t col) {
    int s;
    for (s = 4; s <= 130; s += 2) {
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

static void render_intercept_modal(void) {
    const RadarEvent *ev = &g_camp.events[g_camp.intercept_ev];
    int mx = g_input.mx, my = g_input.my;
    int tx = g_camp.intercept_tx, ty = g_camp.intercept_ty;
    int ufo_r = (ev->ufo_size == 0) ? 5 : (ev->ufo_size == 1 ? 8 : 11);
    int hp_w  = (ev->ufo_hp_max > 0) ? ((ev->ufo_hp * 86) / ev->ufo_hp_max) : 0;
    int hov_abort = (mx >= 164 && mx < 234 && my >= 139 && my < 156);
    char buf[64];

    vga_rect_bevel(8, 12, 232, 148, 15, 80, 26);
    vga_rect_fill(10, 14, 228, 144, g_camp.intercept_flash > 0 ? 36 : 30);

    vga_hline(12, 236, 78, 28);
    vga_vline(124, 26, 134, 28);
    draw_radar_pulse_box(124, 78, 36, 26);
    draw_radar_pulse_box(124, 78, 18, 26);

    vga_rect_fill(10, 14, 228, 12, 15);
    snprintf(buf, sizeof(buf), "INTERCEPT: %s", ev->tag);
    vga_print_shadow(13, 16, buf, 96, 15);

    vga_rect_bevel(104, 16, 88, 7, 14, 10, 15);
    if (hp_w > 0) vga_rect_fill(105, 17, hp_w, 5, 144);

    snprintf(buf, sizeof(buf), "%2ds", (g_camp.intercept_timer + 69) / 70);
    vga_print_shadow(204, 16, buf, 32, 15);

    vga_rect_fill(tx - ufo_r, ty - 2, ufo_r * 2 + 1, 5, g_camp.intercept_flash ? 96 : 7);
    vga_rect_fill(tx - (ufo_r - 2), ty - 4, (ufo_r - 2) * 2 + 1, 3, 80);
    vga_hline(tx - ufo_r - 2, tx + ufo_r + 2, ty, 144);

    if (g_camp.intercept_shot_t > 0) {
        uint8_t bcol = (g_camp.intercept_shot_t & 1) ? 96 : 80;
        vga_line(70,  136, g_camp.intercept_shot_x, g_camp.intercept_shot_y, bcol);
        vga_line(178, 136, g_camp.intercept_shot_x, g_camp.intercept_shot_y, bcol);
        draw_radar_pulse_box(g_camp.intercept_shot_x, g_camp.intercept_shot_y, 5, 96);
    }

    if (mx >= 14 && mx <= 234 && my >= 28 && my <= 134) {
        uint8_t rcol = (g_camp.intercept_cooldown == 0) ? 96 : 33;
        vga_hline(mx - 7, mx + 7, my, rcol);
        vga_vline(mx, my - 7, my + 7, rcol);
    }

    vga_rect_fill(10, 137, 228, 21, 15);
    snprintf(buf, sizeof(buf), "OVER: %s", ev->loc_name);
    vga_print(13, 140, buf, 80);
    vga_print(13, 149, g_camp.intercept_cooldown == 0 ? "L-CLICK UFO TO FIRE!" : "RECHARGING BATTERY...", 96);

    vga_rect_bevel(164, 139, 70, 17, hov_abort ? 65 : 167, hov_abort ? 96 : 162, 172);
    vga_print_shadow(173, 144, "BREAK OFF", hov_abort ? 96 : 1, 15);
}


static void render_launch_modal(const RadarEvent *ev) {
    char buf[80];
    int mx = g_input.mx, my = g_input.my;
    int has_offense = has_facility(FAC_TURRET);
    int is_night    = is_valley_night();
    int hr          = (g_camp.valley_minute / 60) % 24;
    int mn          = g_camp.valley_minute % 60;
    int hov_launch  = (mx >= 16  && mx < 122 && my >= 126 && my < 146);
    int hov_cancel  = (mx >= 126 && mx < 232 && my >= 126 && my < 146);
    static const char *sz_names[4] = {
        "SMALL SCOUT", "MEDIUM HARVESTER", "LARGE ABDUCTOR", "CMD DREADNOUGHT"
    };

    vga_rect_fill(14, 38, 226, 118, 15);
    vga_rect_bevel(10, 34, 228, 118, 76, 96, 79);
    vga_rect_fill(14, 38, 220, 110, 14);

    vga_rect_bevel(16, 40, 216, 13, 36, 96, 45);
    snprintf(buf, sizeof(buf), ">> CONTACT #%02u: %s <<",
             (unsigned)(ev->event_num % 100), sz_names[ev->ufo_size & 3]);
    vga_print_shadow(16 + (216 - vga_text_width(buf)) / 2, 43, buf, 96, 15);

    snprintf(buf, sizeof(buf), "STATUS: %s (%s)", ev->type_title, ev->tag);
    vga_print_shadow(18, 58, buf, 96, 15);

    snprintf(buf, sizeof(buf), "SECTOR: %s", ev->loc_name);
    vga_print_shadow(18, 69, buf, 80, 15);

    vga_print_sm(18, 81, ev->intel_desc, 18);

    if (ev->is_flying) {
        vga_rect_bevel(16, 95, 216, 14, 13, 80, 15);
        snprintf(buf, sizeof(buf), "UFO HULL: %d/%d HP (%s)",
                 ev->ufo_hp, ev->ufo_hp_max,
                 has_offense ? "TURRET READY" : "NO BASE OFFENSE");
        vga_print_shadow(20, 99, buf, has_offense ? 144 : 96, 15);

        vga_print_shadow(20, 114,
                         has_offense ? "ENGAGE UFO WITH BASE DEFENSE?"
                                     : "AWAIT X-COM INTERCEPT OR LANDING?",
                         96, 15);

        vga_rect_bevel(16, 126, 106, 20, hov_launch ? 34 : 36, 96, 45);
        vga_print_shadow(24, 133, has_offense ? "SHOOT DOWN!" : "TRACK PASS", 96, 15);
    } else {
        vga_rect_bevel(16, 95, 216, 14, is_night ? 131 : 67, 80, 79);
        snprintf(buf, sizeof(buf), "TIME: %02d:%02d - %s",
                 hr, mn, is_night ? "NIGHT MISSION (DARK)" : "DAYLIGHT MISSION");
        vga_print_shadow(16 + (216 - vga_text_width(buf)) / 2, 99,
                         buf, is_night ? 80 : 96, 15);

        vga_print_shadow(22, 114, "DEPLOY ARMORED RESPONSE BUS?", 96, 15);

        vga_rect_bevel(16, 126, 106, 20, hov_launch ? 34 : 36, 96, 45);
        vga_print_shadow(24, 133, "DEPLOY SQUAD", 96, 15);
    }

    vga_rect_bevel(126, 126, 106, 20, hov_cancel ? 65 : 167, hov_cancel ? 96 : 162, 172);
    vga_print_shadow(148, 133, "STAND DOWN", hov_cancel ? 96 : 1, 15);
}


static void render_radar_tab(void) {
    int i, y, x, hov_event = -1;
    int active_cnt = count_active_events();
    int sweep_idx = (g_camp.radar_tick / 3) & 31;
    int trail_idx = (sweep_idx - 1) & 31;
    int has_offense = has_facility(FAC_TURRET);
    char hdr_buf[64];
    char bar_buf[80];

    /* Left CRT Lehigh Valley Radar Frame (x=4, y=4, w=240, h=162) */
    vga_rect_bevel(4, 4, 240, 162, 15, 20, 26);

    if (gfx_radar_bg) {
        /* Scale 300x132 RADAR.BMP horizontally into the 236x132 CRT viewport (x=6..241, y=18..149) */
        for (y = 0; y < RADAR_H; y++) {
            const uint8_t *src_row = gfx_radar_bg + y * 300;
            uint8_t *dst_row = vga_backbuf + (RADAR_Y0 + y) * SCREEN_W + RADAR_X0;
            for (x = 0; x < RADAR_W; x++) {
                dst_row[x] = src_row[(x * 300) / RADAR_W];
            }
        }
    } else {
        vga_rect_fill(RADAR_X0, RADAR_Y0, RADAR_W, RADAR_H, 30);
        for (i = 34; i < 146; i += 20) vga_hline(RADAR_X0, RADAR_X0 + RADAR_W - 1, i, 28);
        for (i = 28; i < 240; i += 26) vga_vline(i, RADAR_Y0, RADAR_Y0 + RADAR_H - 1, 28);
        vga_line(84,  20, 105, 54, 70);
        vga_line(105, 54, 155, 50, 70);
        vga_line(155, 50, 218, 44, 70);
        vga_line(78,  20, 135, 80, 98);
        vga_line(135, 80, 132, 100, 98);
        vga_line(132, 100, 146, 146, 98);
    }

    draw_radar_sweep_ray(HQ_RADAR_X, HQ_RADAR_Y, sweep_dx[trail_idx], sweep_dy[trail_idx], 25);
    draw_radar_sweep_ray(HQ_RADAR_X, HQ_RADAR_Y, sweep_dx[sweep_idx], sweep_dy[sweep_idx], 19);

    vga_print_shadow(72,  44, "ALLENTOWN",  22, 15);
    vga_print_shadow(136, 36, "BETHLEHEM",  22, 15);
    vga_print_shadow(192, 34, "EASTON",     22, 15);
    vga_print_shadow(56,  74, "EMMAUS",     22, 15);
    vga_print_shadow(14,  96, "MACUNGIE",   22, 15);
    vga_print_shadow(114, 128,"QUAKERTOWN", 22, 15);
    vga_print_shadow(178, 128,"NOCKAMIXON", 22, 15);

    vga_hline(HQ_RADAR_X - 4, HQ_RADAR_X + 4, HQ_RADAR_Y, 80);
    vga_vline(HQ_RADAR_X, HQ_RADAR_Y - 4, HQ_RADAR_Y + 4, 80);
    vga_rect_fill(HQ_RADAR_X - 1, HQ_RADAR_Y - 1, 3, 3, 96);
    vga_print_shadow(HQ_RADAR_X + 6, HQ_RADAR_Y - 3, "HQ", 80, 15);

    vga_rect_fill(6, 6, 236, 11, 15);
    vga_hline(6, 241, 17, 25);
    snprintf(hdr_buf, sizeof(hdr_buf), "VALLEY RADAR  CONTACTS:%d  %s",
             active_cnt, has_offense ? "AA:READY" : "AA:X-COM");
    vga_print_shadow(10, 8, hdr_buf, active_cnt > 0 ? 96 : 19, 15);


    /* Plot Active Flying & Grounded Alien Contacts across the Lehigh Valley Radar! */
    for (i = 0; i < MAX_RADAR_EVENTS; i++) {
        const RadarEvent *ev = &g_camp.events[i];
        int lx, ly, x0, y0, x1, y1;
        int is_hov = 0, is_sel = (g_camp.selected_event == i);
        int pulse_r, dot_r, ch_r;
        uint8_t core_col, ring_col, txt_col;

        if (!ev->active) continue;

        event_hit_box(ev, &lx, &ly, &x0, &y0, &x1, &y1);
        if (!g_camp.launch_prompt_open && !g_camp.intercept_open &&
            g_input.mx >= x0 && g_input.mx <= x1 &&
            g_input.my >= y0 && g_input.my <= y1) {
            is_hov = 1;
            hov_event = i;
        }

        if (ev->is_flying) {            /* Flying UFO Overhead: Electric Cyan */
            core_col = is_hov ? 96 : 80;
            ring_col = 66;
            txt_col  = is_hov ? 96 : 80;
        } else if (ev->severity == 2) { /* Alien Attack / Raid: Red-Gold */
            core_col = ((g_camp.radar_tick & 8) || is_hov) ? 32 : 96;
            ring_col = 34;
            txt_col  = is_hov ? 96 : 32;
        } else if (ev->severity == 1) { /* Landed UFO: Gold-Orange */
            core_col = is_hov ? 0 : 96;
            ring_col = 113;
            txt_col  = 96;
        } else {                        /* Shot-Down Crash Site: Elerium Lime */
            core_col = is_hov ? 96 : 144;
            ring_col = 18;
            txt_col  = is_hov ? 96 : 144;
        }

        /* Size of the ship is indicated by the size of the dot on the radar map! */
        dot_r = 1 + (ev->ufo_size & 3);
        ch_r  = 3 + (ev->ufo_size & 3) * 2;

        if (ev->is_flying) {
            radar_pset_clip(ev->rx - ev->vx * 2, ev->ry - ev->vy * 2, 66);
            radar_pset_clip(ev->rx - ev->vx * 3, ev->ry - ev->vy * 3, 70);
        }

        pulse_r = ch_r + 1 + (((g_camp.radar_tick / 5) + i * 2) % 5);
        draw_radar_pulse_box(ev->rx, ev->ry, pulse_r, ring_col);

        vga_hline(ev->rx - ch_r, ev->rx + ch_r, ev->ry, core_col);
        vga_vline(ev->rx, ev->ry - ch_r, ev->ry + ch_r, core_col);
        vga_rect_fill(ev->rx - dot_r, ev->ry - dot_r, dot_r * 2 + 1, dot_r * 2 + 1, core_col);
        vga_pset(ev->rx, ev->ry, 15);

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

    vga_rect_fill(6, 150, 236, 13, 15);
    vga_hline(6, 241, 150, 25);
    if (hov_event >= 0) {
        const RadarEvent *ev = &g_camp.events[hov_event];
        snprintf(bar_buf, sizeof(bar_buf), "%s @ %s", ev->type_title, ev->loc_name);
        vga_print_sm(10, 154, bar_buf, 96);
    } else if (g_camp.report_banner_timer > 0 &&
               g_camp.last_reported_idx >= 0 &&
               g_camp.events[g_camp.last_reported_idx].active) {
        const RadarEvent *ev = &g_camp.events[g_camp.last_reported_idx];
        uint8_t acol = (g_camp.radar_tick & 8) ? 96 : 32;
        snprintf(bar_buf, sizeof(bar_buf), "ALERT: %s @ %s", ev->type_title, ev->loc_name);
        vga_print_sm(10, 154, bar_buf, acol);
    } else if (active_cnt > 0) {
        vga_print_sm(10, 154, "CLICK RADAR CONTACT TO INTERCEPT OR DEPLOY SQUAD", 80);
    } else {
        vga_print_sm(10, 154, "NO ACTIVE CONTACTS - USE RIGHT MENU TIME (1H/1D)", 19);
    }

    if (g_camp.intercept_open &&
        g_camp.intercept_ev >= 0 &&
        g_camp.intercept_ev < MAX_RADAR_EVENTS &&
        g_camp.events[g_camp.intercept_ev].active) {
        render_intercept_modal();
    } else if (g_camp.launch_prompt_open &&
               g_camp.selected_event >= 0 &&
               g_camp.selected_event < MAX_RADAR_EVENTS &&
               g_camp.events[g_camp.selected_event].active) {
        render_launch_modal(&g_camp.events[g_camp.selected_event]);
    } else {
        g_camp.launch_prompt_open = 0;
        g_camp.intercept_open     = 0;
    }
}


static void render_bunker_tab(void) {
    base_ui_render();
}


static void render_armory_tab(void) {
    armory_ui_render();
}




static void render_right_side_menu(void) {
    int t;
    int mx = g_input.mx, my = g_input.my;
    int hr = (g_camp.valley_minute / 60) % 24;
    int mn = g_camp.valley_minute % 60;
    int night = is_valley_night();
    static const char *spd_lbl[4] = { "1S", "5M", "1H", "1D" };
    static const char *tabs[3] = {
        "1.RADAR",
        "2.BASE ",
        "3.ARMOR"
    };
    char buf[32];

    /* Right-Hand Command Panel Frame (x=246, y=4, w=70, h=162) */
    vga_rect_bevel(246, 4, 70, 162, 76, 80, 79);

    /* 1. Campaign Day & 24h Clock Readout (x=249, y=7, w=64, h=19) */
    vga_rect_bevel(249, 7, 64, 19, 14, 68, 15);
    snprintf(buf, sizeof(buf), "DAY %d %s", g_camp.valley_day, night ? "NGT" : "DAY");
    vga_print_shadow(253, 9, buf, night ? 80 : 96, 15);
    snprintf(buf, sizeof(buf), " %02d:%02d", hr, mn);
    vga_print(255, 17, buf, 18);

    /* 2. Time-Passing Selection Buttons (2x2 Grid: 1S=Realtime, 5M, 1H=1Hr/s, 1D=1Day/s) */
    for (t = 0; t < 4; t++) {
        int bx = 249 + (t & 1) * 33;
        int by = 28 + (t >> 1) * 16;
        int act = (g_camp.time_speed == t);
        int hov = (!g_camp.launch_prompt_open && !g_camp.intercept_open &&
                   mx >= bx && mx < bx + 31 && my >= by && my < by + 14);
        vga_rect_bevel(bx, by, 31, 14, act ? 36 : (hov ? 65 : 68),
                       (act || hov) ? 96 : 162, 79);
        vga_print_shadow(bx + 9, by + 4, spd_lbl[t], act ? 96 : (hov ? 80 : 1), 15);
    }

    /* 3. Screen Navigation Buttons (1.RADAR, 2.BASE, 3.ARMOR) */
    for (t = 0; t < 3; t++) {
        int by = 63 + t * 25;
        int act = (g_camp.active_tab == t);
        int hov = (!g_camp.launch_prompt_open && !g_camp.intercept_open &&
                   mx >= 249 && mx < 313 && my >= by && my < by + 22);
        vga_rect_bevel(249, by, 64, 22, act ? 65 : (hov ? 68 : 167),
                       (act || hov) ? 96 : 162, 172);
        vga_print_shadow(259, by + 7, tabs[t], (act || hov) ? 96 : 1, 15);
    }

    /* 4. Options Menu Button on Right-Hand Menu (y=140..162) */
    {
        int hov_opt = (!g_camp.launch_prompt_open && !g_camp.intercept_open &&
                       mx >= 249 && mx < 313 && my >= 140 && my < 162);
        vga_rect_bevel(249, 140, 64, 22, hov_opt ? 65 : 68, hov_opt ? 96 : 162, 172);
        vga_print_shadow(260, 147, "OPTIONS", hov_opt ? 96 : 1, 15);
    }
}

static void render_event_modal(void) {
    int f = g_camp.modal_anim_frame;
    int full_w = 248, full_h = 126;
    int w, h, x0, y0, y, x;
    const ModalAlert *m;
    int mx = g_input.mx, my = g_input.my;
    int hov_ack;

    if (g_camp.modal_open == 0 || g_camp.modal_count <= 0) return;
    if (f < 1) f = 1;
    if (f > 8) f = 8;

    m  = &g_camp.modal_queue[0];
    w  = (full_w * f) / 8;
    h  = (full_h * f) / 8;
    x0 = (SCREEN_W - w) / 2;
    y0 = 26 + (full_h - h) / 2;

    /* Shadow & Outer Duff-COM Bevel Frame */
    vga_rect_fill(x0 + 3, y0 + 3, w, h, 15);
    vga_rect_bevel(x0, y0, w, h, 65, m->accent_col ? m->accent_col : 96, 172);

    if (w > 8 && h > 8) {
        vga_rect_fill(x0 + 2, y0 + 2, w - 4, h - 4, 14);
        if (gfx_modal_bg && gfx_modal_w > 0 && gfx_modal_h > 0 && f == 8) {
            int bw = (gfx_modal_w < full_w - 4) ? gfx_modal_w : (full_w - 4);
            int bh = (gfx_modal_h < full_h - 4) ? gfx_modal_h : (full_h - 4);
            int ox = (full_w - 4 - bw) / 2;
            int oy = (full_h - 4 - bh) / 2;
            for (y = 0; y < bh; y++) {
                const uint8_t *src = gfx_modal_bg + y * gfx_modal_w;
                uint8_t *dst = vga_backbuf + (y0 + 2 + oy + y) * SCREEN_W + (x0 + 2 + ox);
                for (x = 0; x < bw; x++) {
                    dst[x] = src[x];
                }
            }
        } else {
            /* CRT scanline & Duff-COM watermark bars */
            for (y = y0 + 4; y < y0 + h - 4; y += 3) {
                vga_hline(x0 + 4, x0 + w - 5, y, 15);
            }
        }
    }

    if (f < 7) return;

    /* Header Strip */
    vga_rect_bevel(x0 + 6, y0 + 6, w - 12, 16, 36, m->accent_col ? m->accent_col : 96, 45);
    vga_print_shadow(x0 + 12, y0 + 11, "DUFF-COM PRIORITY DISPATCH", 80, 15);

    /* Event Title & 3 Content Lines */
    vga_rect_bevel(x0 + 8, y0 + 26, w - 16, 66, 15, 68, 13);
    vga_print_shadow(x0 + (w - vga_text_width(m->title)) / 2, y0 + 32,
                     m->title, m->accent_col ? m->accent_col : 96, 15);
    vga_hline(x0 + 14, x0 + w - 15, y0 + 42, 68);
    vga_print_sm(x0 + (w - vga_text_width_sm(m->line1)) / 2, y0 + 49, m->line1, 0);
    vga_print_sm(x0 + (w - vga_text_width_sm(m->line2)) / 2, y0 + 61, m->line2, 80);
    vga_print_sm(x0 + (w - vga_text_width_sm(m->line3)) / 2, y0 + 73, m->line3, 18);

    /* Acknowledge Button */
    hov_ack = (mx >= x0 + 64 && mx < x0 + 184 && my >= y0 + 97 && my < y0 + 119);
    vga_rect_bevel(x0 + 64, y0 + 97, 120, 22, hov_ack ? 34 : 36, 96, 45);
    vga_print_shadow(x0 + 82, y0 + 105, "[ ACKNOWLEDGE ]", 96, 15);
}

void campaign_render(void) {
    char res_buf[64];

    vga_clear(14);

    if (g_camp.active_tab == 0)      render_radar_tab();
    else if (g_camp.active_tab == 1) render_bunker_tab();
    else                             render_armory_tab();

    render_right_side_menu();

    /* Bottom Status & Resource Strip */
    vga_rect_bevel(0, 168, SCREEN_W, 32, 166, 162, 171);
    snprintf(res_buf, sizeof(res_buf), "$%d ALLOY:%d ELER:%d CIV:%d/%d SCORE:%d",
             g_camp.money, g_camp.alloys, g_camp.elerium,
             g_camp.total_civilians_saved,
             g_camp.total_civilians_saved + g_camp.total_civilians_killed,
             g_camp.total_score);
    vga_print_shadow(8, 173, res_buf, 80, 15);
    vga_print(8, 185, g_camp.notice, 96);

    if (g_camp.modal_open != 0) {
        render_event_modal();
    }
}

static void fire_intercept_battery(int aim_x, int aim_y) {
    int ev_idx = g_camp.intercept_ev;
    RadarEvent *ev;
    int hit_r;
    if (ev_idx < 0 || ev_idx >= MAX_RADAR_EVENTS || !g_camp.events[ev_idx].active) return;
    if (g_camp.intercept_cooldown > 0) return;

    ev = &g_camp.events[ev_idx];
    g_camp.intercept_shot_x   = aim_x;
    g_camp.intercept_shot_y   = aim_y;
    g_camp.intercept_shot_t   = 10;
    g_camp.intercept_cooldown = 16;
    sfx_laser();

    hit_r = 9 + ev->ufo_size * 3 + (g_camp.tech_done[TECH_UFO_NAVIGATION] ? 4 : 0);
    if (abs(aim_x - g_camp.intercept_tx) <= hit_r &&
        abs(aim_y - g_camp.intercept_ty) <= hit_r) {
        int dmg = 28 + (rand() % 13);
        ev->ufo_hp -= dmg;
        g_camp.intercept_flash = 8;
        sfx_explosion();


        if (ev->ufo_hp <= 0) {
            /* UFO Shot Down! Wherever it was downed on the radar map becomes the mission biome! */
            ev->ufo_hp    = 0;
            ev->is_flying = 0;
            ev->severity  = 0; /* Crash Site (damaged UFO on ground) */
            ev->age_ticks = 0;
            ev->ttl_ticks = 1440;
            update_event_nearest_site(ev);
            ev->type_title = (ev->ufo_size == 0) ? "CRASHED SCOUT" :
                             (ev->ufo_size == 1) ? "CRASHED HARVESTER" :
                             (ev->ufo_size == 2) ? "DOWNED ABDUCTOR" : "DOWNED COMMAND SHIP";
            ev->intel_desc = "SHOT DOWN BY DUFF-COM BATTERY!";
            snprintf(ev->tag, sizeof(ev->tag), "SITE-%02u", (unsigned)(ev->event_num % 100));

            g_camp.intercept_open     = 0;
            g_camp.selected_event     = ev_idx;
            g_camp.selected_sector    = ev->biome_id;
            g_camp.deploy_night       = is_valley_night();
            g_camp.launch_prompt_open = 1;
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "SPLASH ONE! %s DOWN AT %s!", ev->tag, ev->loc_name);
            campaign_push_modal("UFO SHOT DOWN BY HQ BATTERY!",
                                ev->type_title,
                                ev->loc_name,
                                "STRIKE TEAM READY TO DEPLOY TO CRASH SITE",
                                144, 2);
        } else {
            snprintf(g_camp.notice, sizeof(g_camp.notice),
                     "DIRECT HIT ON %s! (%d/%d HP)", ev->tag, ev->ufo_hp, ev->ufo_hp_max);
        }
    } else {
        snprintf(g_camp.notice, sizeof(g_camp.notice), "MISSED %s - LEAD TARGET!", ev->tag);
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

    /* -1. Priority Event Modal Popup Acknowledgment */
    if (g_camp.modal_open != 0) {
        if (g_camp.modal_open <= 2) {
            int x0 = (SCREEN_W - 248) / 2;
            int y0 = 26;
            if (g_input.last_key == KEY_ENTER || g_input.last_key == ' ' ||
                g_input.last_key == KEY_ESC || g_input.right_clicked ||
                (g_input.left_clicked &&
                 mx >= x0 + 64 && mx < x0 + 184 && my >= y0 + 97 && my < y0 + 119)) {
                g_camp.modal_open = 3;
                sfx_beep(520, 25);
                sfx_beep(390, 25);
            }
        }
        return;
    }

    /* 0. Active UFO Shoot-Down Interception Mini-Game Input */
    if (g_camp.intercept_open) {
        if (g_input.last_key == KEY_ESC || g_input.right_clicked) {
            g_camp.intercept_open = 0;
            sfx_click();
            return;
        }
        if (g_input.last_key == KEY_ENTER || g_input.last_key == ' ') {
            fire_intercept_battery(g_camp.intercept_tx, g_camp.intercept_ty);
            return;
        }
        if (g_input.left_clicked) {
            if (mx >= 164 && mx < 234 && my >= 139 && my < 156) {
                g_camp.intercept_open = 0;
                sfx_click();
                return;
            }
            if (mx >= 12 && mx < 236 && my >= 26 && my < 136) {
                fire_intercept_battery(mx, my);
                return;
            }
        }
        return;
    }

    /* 1. Handle Modal Launch / Intercept Dialog if Open */
    if (g_camp.launch_prompt_open) {
        int sel_ev = g_camp.selected_event;
        RadarEvent *ev = (sel_ev >= 0 && sel_ev < MAX_RADAR_EVENTS) ? &g_camp.events[sel_ev] : NULL;

        if (g_input.last_key == KEY_ENTER || g_input.last_key == 'y' || g_input.last_key == 'Y') {
            if (ev && ev->is_flying) {
                if (has_facility(FAC_TURRET)) {
                    start_ufo_interception(sel_ev);
                } else {
                    g_camp.launch_prompt_open = 0;
                    advance_campaign_minutes(ev->flight_mins > 0 ? ev->flight_mins : 1);
                }
            } else {
                launch_selected_mission();
            }
            return;
        }
        if (g_input.last_key == KEY_ESC || g_input.last_key == 'n' || g_input.last_key == 'N' ||
            g_input.right_clicked) {
            g_camp.launch_prompt_open = 0;
            sfx_click();
            return;
        }
        if (g_input.left_clicked) {
            if (mx >= 16 && mx < 122 && my >= 126 && my < 146) {
                if (ev && ev->is_flying) {
                    if (has_facility(FAC_TURRET)) {
                        start_ufo_interception(sel_ev);
                    } else {
                        g_camp.launch_prompt_open = 0;
                        advance_campaign_minutes(ev->flight_mins > 0 ? ev->flight_mins : 1);
                    }
                } else {
                    launch_selected_mission();
                }
                return;
            }
            if ((mx >= 126 && mx < 232 && my >= 126 && my < 146) ||
                mx < 10 || mx >= 238 || my < 34 || my >= 152) {
                g_camp.launch_prompt_open = 0;
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
            g_camp.time_speed = (g_camp.time_speed + 1) % 4;
            sfx_click();
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
        if (g_input.last_key == KEY_UP || g_input.last_key == KEY_LEFT) {
            int k;
            for (k = 1; k <= MAX_ROSTER_SIZE; k++) {
                int idx = (g_camp.selected_soldier + MAX_ROSTER_SIZE - k) % MAX_ROSTER_SIZE;
                if (g_camp.squad[idx].active) {
                    g_camp.selected_soldier = idx;
                    sfx_click();
                    break;
                }
            }
        }
        if (g_input.last_key == KEY_DOWN || g_input.last_key == KEY_RIGHT) {
            int k;
            for (k = 1; k <= MAX_ROSTER_SIZE; k++) {
                int idx = (g_camp.selected_soldier + k) % MAX_ROSTER_SIZE;
                if (g_camp.squad[idx].active) {
                    g_camp.selected_soldier = idx;
                    sfx_click();
                    break;
                }
            }
        }
        if (g_input.last_key == KEY_ENTER || g_input.last_key == ' ') {
            g_camp.armory_subtab = (g_camp.armory_subtab + 1) % 3;
            g_camp.store_scroll  = 0;
            sfx_click();
            return;
        }
    }

    if (!g_input.left_clicked) return;

    /* 3. Right-Hand Side Menu Clicks (x=246..319) */
    if (mx >= 246) {
        int t;
        for (t = 0; t < 4; t++) {
            int bx = 249 + (t & 1) * 33;
            int by = 28 + (t >> 1) * 16;
            if (mx >= bx && mx < bx + 31 && my >= by && my < by + 14) {
                static const char *spd_desc[4] = {
                    "TIME SPEED: 1S (REALTIME)",
                    "TIME SPEED: 5M PER SECOND",
                    "TIME SPEED: 1 HOUR PER SEC",
                    "TIME SPEED: 1 DAY PER SEC"
                };
                g_camp.time_speed = t;
                snprintf(g_camp.notice, sizeof(g_camp.notice), "%s", spd_desc[t]);
                sfx_click();
                return;
            }
        }
        for (t = 0; t < 3; t++) {
            int by = 63 + t * 25;
            if (mx >= 249 && mx < 313 && my >= by && my < by + 22) {
                g_camp.active_tab = t;
                sfx_click();
                return;
            }
        }
        if (mx >= 249 && mx < 313 && my >= 140 && my < 162) {
            g_settings.menu_open = 1;
            sfx_click();
            return;
        }
        return;
    }

    if (g_camp.active_tab == 0) {
        int i;
        /* Clicking an Active Alien Event Blip on the Lehigh Valley Radar */
        for (i = 0; i < MAX_RADAR_EVENTS; i++) {
            const RadarEvent *ev = &g_camp.events[i];
            int lx, ly, x0, y0, x1, y1;
            if (!ev->active) continue;
            event_hit_box(ev, &lx, &ly, &x0, &y0, &x1, &y1);
            if (mx >= x0 && mx <= x1 && my >= y0 && my <= y1) {
                g_camp.time_speed         = 0;
                g_camp.selected_event     = i;
                g_camp.selected_sector    = ev->biome_id;
                g_camp.deploy_night       = is_valley_night();
                g_camp.launch_prompt_open = 1;
                sfx_click();
                return;
            }
        }

        if (mx >= 6 && mx < 242 && my >= 18 && my < 150) {
            if (count_active_events() > 0) {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "CLICK A PULSING ALIEN CONTACT BLIP!");
            } else {
                snprintf(g_camp.notice, sizeof(g_camp.notice),
                         "SCANNING VALLEY... USE RIGHT MENU TIME (1H/1D)");
            }
            sfx_beep(300, 20);
        }
    } else if (g_camp.active_tab == 1) {
        base_ui_handle_input();
    } else if (g_camp.active_tab == 2) {
        armory_ui_handle_input();
    }
}



