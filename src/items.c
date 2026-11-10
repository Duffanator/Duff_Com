#include "items.h"
#include "gfx_assets.h"
#include "bmp_loader.h"
#include "vga.h"
#include <stdlib.h>
#include <string.h>

uint8_t spr_item_icons[ITEM_COUNT][ITEM_ICON_W * ITEM_ICON_H];
uint8_t spr_ground_items[ITEM_COUNT][GROUND_ITEM_W * GROUND_ITEM_H];
uint8_t spr_base_tiles[12][BASE_TILE_W * BASE_TILE_H];
int     has_base_bmp = 0;


static const ItemDef item_defs[ITEM_COUNT] = {
    /* 1. Human Ballistic Weapons (Researched at Start: req_tech = TECH_NONE) */
    {
        ITEM_PISTOL, "PISTOL", "9MM SERVICE PISTOL", ICAT_WEAPON,
        ITEM_PISTOL_CLIP, 12, PROJ_BULLET,
        24, 10, 0,
        10, 68,  16, 105,  18, 36,
        TECH_NONE, 250, 125, 0, 0, 0, 0, 1
    },
    {
        ITEM_ASSAULT_RIFLE, "A.RIFLE", "5.56MM ASSAULT RIFLE", ICAT_WEAPON,
        ITEM_RIFLE_MAG, 20, PROJ_BULLET,
        32, 14, 0,
        12, 72,  20, 120,  18, 40,
        TECH_NONE, 550, 275, 0, 0, 0, 0, 2
    },
    {
        ITEM_HEAVY_RIFLE, "HVY.RFL", "7.62MM HEAVY RIFLE", ICAT_WEAPON,
        ITEM_HEAVY_MAG, 15, PROJ_BULLET,
        46, 18, 0,
        15, 68,  24, 125,  22, 35,
        TECH_NONE, 850, 425, 0, 0, 0, 0, 3
    },


    /* 2. Human Ballistic Ammo Magazines */
    {
        ITEM_PISTOL_CLIP, "9MM.CLP", "9MM PISTOL CLIP (12)", ICAT_AMMO,
        ITEM_NONE, 12, PROJ_BULLET,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_NONE, 35, 15, 0, 0, 0, 0, 1
    },
    {
        ITEM_RIFLE_MAG, "556.MAG", "5.56MM RIFLE MAG (20)", ICAT_AMMO,
        ITEM_NONE, 20, PROJ_BULLET,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_NONE, 60, 25, 0, 0, 0, 0, 1
    },
    {
        ITEM_HEAVY_MAG, "762.DRM", "7.62MM HEAVY DRUM (15)", ICAT_AMMO,
        ITEM_NONE, 15, PROJ_BULLET,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_NONE, 90, 40, 0, 0, 0, 0, 1
    },

    /* 3. Human Thrown Munitions & Utility */
    {
        ITEM_FRAG_GRENADE, "FRAG.GR", "LIGHT FRAG GRENADE", ICAT_GRENADE,
        ITEM_NONE, 1, PROJ_BULLET,
        55, 20, 2,
        16, 100, 0, 0, 0, 0,
        TECH_NONE, 100, 50, 0, 0, 0, 0, 1
    },
    {
        ITEM_HEAVY_GRENADE, "HE.CHRG", "HEAVY HE SATCHEL", ICAT_GRENADE,
        ITEM_NONE, 1, PROJ_BULLET,
        85, 25, 3,
        20, 100, 0, 0, 0, 0,
        TECH_NONE, 220, 110, 0, 0, 0, 0, 1
    },
    {
        ITEM_FLARE, "E-FLARE", "NIGHT ELECTRO-FLARE", ICAT_UTILITY,
        ITEM_NONE, 1, PROJ_LASER,
        0, 0, 0,
        10, 100, 0, 0, 0, 0,
        TECH_NONE, 40, 20, 0, 0, 0, 0, 1
    },

    /* 4. Coopersburg Prototype Laser Weapons & Batteries */
    {
        ITEM_LASER_PISTOL, "LSR.PST", "LASER PISTOL", ICAT_WEAPON,
        ITEM_LASER_BATTERY, 15, PROJ_LASER,
        38, 14, 0,
        10, 74,  16, 112,  18, 44,
        TECH_LASER_WEAPONS, 900, 450, 1, 300, 2, 0, 1
    },
    {
        ITEM_LASER_RIFLE, "LSR.RFL", "PROTOTYPE LASER RIFLE", ICAT_WEAPON,
        ITEM_LASER_BATTERY, 12, PROJ_LASER,
        52, 18, 0,
        12, 78,  20, 124,  18, 46,
        TECH_LASER_WEAPONS, 1400, 700, 1, 500, 4, 0, 2
    },
    {
        ITEM_HEAVY_LASER, "HVY.LSR", "HEAVY LASER CANNON", ICAT_WEAPON,
        ITEM_HEAVY_LASER_PAK, 8, PROJ_LASER,
        74, 24, 0,
        16, 74,  25, 128,  22, 38,
        TECH_HEAVY_LASER, 2100, 1050, 1, 800, 6, 0, 3
    },

    {
        ITEM_LASER_BATTERY, "LSR.BAT", "LASER CELL BATTERY", ICAT_AMMO,
        ITEM_NONE, 12, PROJ_LASER,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_LASER_WEAPONS, 120, 60, 1, 40, 1, 0, 1
    },
    {
        ITEM_HEAVY_LASER_PAK, "HVY.PAK", "HIGH-CAP LASER PACK", ICAT_AMMO,
        ITEM_NONE, 8, PROJ_LASER,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_HEAVY_LASER, 200, 100, 1, 75, 1, 0, 1
    },

    /* 5. Alien Elerium Plasma Weapons, Cartridges & Grenades */
    {
        ITEM_PLASMA_PISTOL, "PLS.PST", "ALIEN PLASMA PISTOL", ICAT_WEAPON,
        ITEM_PLASMA_PISTOL_CLIP, 15, PROJ_PLASMA,
        52, 18, 0,
        11, 75,  18, 115,  18, 44,
        TECH_PLASMA_PISTOL, 2200, 1100, 1, 700, 3, 1, 1
    },
    {
        ITEM_PLASMA_RIFLE, "PLS.RFL", "ALIEN PLASMA RIFLE", ICAT_WEAPON,
        ITEM_PLASMA_RIFLE_CLIP, 12, PROJ_PLASMA,
        72, 22, 0,
        12, 80,  20, 126,  18, 48,
        TECH_PLASMA_RIFLE, 3400, 1700, 1, 1100, 5, 2, 2
    },
    {
        ITEM_HEAVY_PLASMA, "HVY.PLS", "HEAVY PLASMA CANNON", ICAT_WEAPON,
        ITEM_HEAVY_PLASMA_CLIP, 10, PROJ_PLASMA,
        98, 28, 0,
        15, 78,  23, 130,  20, 50,
        TECH_HEAVY_PLASMA, 4800, 2400, 1, 1600, 8, 4, 3
    },

    {
        ITEM_PLASMA_PISTOL_CLIP, "PP.CART", "PLASMA PISTOL CLIP", ICAT_AMMO,
        ITEM_NONE, 15, PROJ_PLASMA,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_PLASMA_PISTOL, 350, 175, 1, 120, 1, 1, 1
    },
    {
        ITEM_PLASMA_RIFLE_CLIP, "PR.CART", "PLASMA RIFLE CLIP", ICAT_AMMO,
        ITEM_NONE, 12, PROJ_PLASMA,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_PLASMA_RIFLE, 500, 250, 1, 180, 1, 1, 1
    },
    {
        ITEM_HEAVY_PLASMA_CLIP, "HP.CORE", "HEAVY PLASMA CORE", ICAT_AMMO,
        ITEM_NONE, 10, PROJ_PLASMA,
        0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_HEAVY_PLASMA, 750, 375, 1, 250, 2, 2, 1
    },
    {
        ITEM_ALIEN_GRENADE, "ALN.GRN", "ALIEN PULSE GRENADE", ICAT_GRENADE,
        ITEM_NONE, 1, PROJ_PLASMA,
        110, 30, 3,
        18, 100, 0, 0, 0, 0,
        TECH_ALIEN_GRENADE, 600, 300, 1, 200, 1, 1, 1
    },

    /* 6. Recovered Alien Materials, UFO Subsystems & Corpses (Never Purchasable!) */
    {
        ITEM_ELERIUM, "ELERIUM", "ELERIUM-115 POD", ICAT_ARTIFACT,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_NONE, 0, 400, 0, 0, 0, 0, 1
    },
    {
        ITEM_ALLOYS, "ALLOYS", "ALIEN HULL ALLOYS", ICAT_ARTIFACT,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_NONE, 0, 150, 1, 100, 0, 0, 1
    },
    {
        ITEM_UFO_NAV, "UFO.NAV", "UFO NAVIGATION CONSOLE", ICAT_ARTIFACT,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_UFO_NAVIGATION, 0, 800, 0, 0, 0, 0, 2
    },
    {
        ITEM_UFO_CRYO, "CRYO.PD", "ALIEN CRYO-CHAMBER", ICAT_ARTIFACT,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_UFO_CONSTRUCTION, 0, 950, 0, 0, 0, 0, 2
    },
    {
        ITEM_UFO_POD, "TECH.PD", "ALIEN ORDNANCE POD", ICAT_ARTIFACT,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_UFO_CONSTRUCTION, 0, 1100, 0, 0, 0, 0, 2
    },
    {
        ITEM_CORPSE_SECTOID, "SECTOID", "SECTOID CORPSE", ICAT_CORPSE,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_AUTOPSY_SECTOID, 0, 350, 0, 0, 0, 0, 2
    },
    {
        ITEM_CORPSE_FLOATER, "FLOATER", "FLOATER CORPSE", ICAT_CORPSE,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_AUTOPSY_FLOATER, 0, 450, 0, 0, 0, 0, 2
    },
    {
        ITEM_CORPSE_MUTON, "MUTON", "MUTON CORPSE", ICAT_CORPSE,
        ITEM_NONE, 0, 0,  0, 0, 0,  0, 0, 0, 0, 0, 0,
        TECH_AUTOPSY_MUTON, 0, 650, 0, 0, 0, 0, 3
    }
};


static const TechDef tech_defs[TECH_COUNT] = {
    {
        TECH_LASER_WEAPONS, "LASER WEAPONRY",
        TECH_NONE, ITEM_NONE, 40,
        "UNLOCKS LASER PISTOL, RIFLE & BATTERY",
        {
            "FOCUSED SOLID-STATE RUBY/CYAN DIODE ARRAYS.",
            "ALLOWS DUFF-COM WORKSHOP TO BUILD & RECHARGE",
            "LASER PISTOLS, PROTOTYPE RIFLES & BATTERIES."
        }
    },
    {
        TECH_HEAVY_LASER, "HEAVY LASER CANNON",
        TECH_LASER_WEAPONS, ITEM_NONE, 60,
        "UNLOCKS HEAVY LASER & HIGH-CAP PACK",
        {
            "TRIPLE-CAPACITOR HIGH-OUTPUT THERMAL EMITTER.",
            "PUNCHES CLEAN THROUGH BRICK AND SAUCER HULLS.",
            "USES HIGH-CAP LASER PACKS IN STORE/WORKSHOP."
        }
    },
    {
        TECH_PLASMA_PISTOL, "ALIEN PLASMA PISTOL",
        TECH_NONE, ITEM_PLASMA_PISTOL, 55,
        "USE & BUILD PLASMA PISTOL + CARTRIDGE",
        {
            "COMPACT ELERIUM-PUMPED MAGNETIC PLASMA SIDEARM.",
            "DECODED ALIEN GRIP SAFETY & CONTAINMENT COILS.",
            "CARTRIDGES CAN BE BUILT USING ELERIUM-115."
        }
    },
    {
        TECH_PLASMA_RIFLE, "ALIEN PLASMA RIFLE",
        TECH_NONE, ITEM_PLASMA_RIFLE, 75,
        "USE & BUILD PLASMA RIFLE + CARTRIDGE",
        {
            "PRIMARY ALIEN INFANTRY ACCELERATOR RIFLE.",
            "FIRES SUPERHEATED IONIZED GREEN PLASMA BOLTS.",
            "OPERATIVES CAN NOW WIELD & RELOAD THIS RIFLE."
        }
    },
    {
        TECH_HEAVY_PLASMA, "HEAVY PLASMA CANNON",
        TECH_PLASMA_RIFLE, ITEM_HEAVY_PLASMA, 100,
        "USE & BUILD HEAVY PLASMA + POWER CORE",
        {
            "DEVASTATING MUTON-GRADE HEAVY PLASMA WEAPON.",
            "VAPORIZES REINFORCED STEEL & HEAVY ARMOR.",
            "REQUIRES ELERIUM-115 CORES FOR RELOADING."
        }
    },
    {
        TECH_ALIEN_GRENADE, "ALIEN PULSE GRENADE",
        TECH_NONE, ITEM_ALIEN_GRENADE, 50,
        "USE & BUILD ALIEN ELERIUM GRENADES",
        {
            "TIMED ELERIUM-115 MICRO-IMPLOSION CHARGE.",
            "CALIBRATED DETONATOR DIAL FOR HUMAN HANDS.",
            "DELIVERS MASSIVE BLAST DAMAGE ACROSS 3 TILES."
        }
    },
    {
        TECH_AUTOPSY_SECTOID, "SECTOID AUTOPSY",
        TECH_NONE, ITEM_CORPSE_SECTOID, 35,
        "+10% SQUAD AIM VS SECTOIDS",
        {
            "OVERSIZED CRANIAL LOBE WITH FRAGILE RIBCAGE.",
            "GENETICALLY ENGINEERED SCOUT & TECH CREW.",
            "REVEALS VITAL WEAK POINTS FOR SQUAD TARGETING."
        }
    },
    {
        TECH_AUTOPSY_FLOATER, "FLOATER AUTOPSY",
        TECH_NONE, ITEM_CORPSE_FLOATER, 45,
        "+10% SQUAD AIM VS FLOATERS",
        {
            "CYBERNETIC ANTI-GRAVITY UNIT FUSED TO SPINE.",
            "SYNTHETIC ORGAN CASING IS VULNERABLE TO SHOTS.",
            "DATA IMPROVES ANTI-AIR & TACTICAL ACCURACY."
        }
    },
    {
        TECH_AUTOPSY_MUTON, "MUTON AUTOPSY",
        TECH_NONE, ITEM_CORPSE_MUTON, 65,
        "+10% SQUAD AIM VS MUTONS",
        {
            "HEAVILY MUSCLED BIO-ARMORED SHOCK TROOPER.",
            "REDUNDANT ADRENAL GLANDS ABSORB LIGHT FIRE.",
            "HIGH-ENERGY LASER OR PLASMA FIRE RECOMMENDED."
        }
    },
    {
        TECH_UFO_NAVIGATION, "UFO NAVIGATION",
        TECH_NONE, ITEM_UFO_NAV, 60,
        "EARLY RADAR TRACKING & INTERCEPT LOCK",
        {
            "HYPER-WAVE TELEMETRY & GRAVITY-DRIVE CHARTS.",
            "EXTENDS HOMEMADE RADAR DETECTION RANGE AND",
            "IMPROVES BASE OFFENSIVE INTERCEPT TARGETING."
        }
    },
    {
        TECH_UFO_CONSTRUCTION, "ARV CARGO EXPANSION",
        TECH_NONE, ITEM_ALLOYS, 50,
        "EXPANDS ARMORED BUS GEAR RACK (+14 SLOTS)",
        {
            "LIGHTWEIGHT ALIEN ALLOY BRACING & GEAR RACKS",
            "INSTALLED INSIDE THE ARMORED RESPONSE BUS.",
            "DOUBLES VEHICLE WEAPON & AMMO CAPACITY!"
        }
    },
    {
        TECH_ARV_TURRET, "ARV DEFENSE TURRET",
        TECH_LASER_WEAPONS, ITEM_ALLOYS, 70,
        "UNLOCKS BUS ROOF AUTO-TURRET (MK1)",
        {
            "ROOF-MOUNTED AUTOMATED DEFENSE TURRET FOR",
            "THE ARMORED RESPONSE VEHICLE. FIRES AT ALIENS",
            "APPROACHING THE BUS WITH A 3-TURN COOLDOWN."
        }
    },
    {
        TECH_ARV_TURRET_MK2, "ARV TURRET MK2 UPGR",
        TECH_ARV_TURRET, ITEM_UFO_NAV, 90,
        "UPGRADES BUS TURRET RANGE, ACC & COOLDOWN",
        {
            "INTEGRATES UFO TARGETING SENSORS INTO THE BUS",
            "ROOF TURRET: +3 TILE RANGE, +20% ACCURACY, AND",
            "REDUCES RECHARGE COOLDOWN TO 2 TURNS!"
        }
    }
};

const ItemDef *item_get_def(int item_id) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return &item_defs[ITEM_ASSAULT_RIFLE];
    return &item_defs[item_id];
}

const TechDef *tech_get_def(int tech_id) {
    if (tech_id < 0 || tech_id >= TECH_COUNT) return &tech_defs[0];
    return &tech_defs[tech_id];
}

int item_is_weapon(int item_id) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return 0;
    return (item_defs[item_id].category == ICAT_WEAPON);
}

int item_is_ammo(int item_id) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return 0;
    return (item_defs[item_id].category == ICAT_AMMO);
}

int item_is_grenade(int item_id) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return 0;
    return (item_defs[item_id].category == ICAT_GRENADE);
}

int item_legacy_weapon_class(int item_id) {
    if (item_id < 0 || item_id >= ITEM_COUNT) return 0;
    if (item_defs[item_id].proj_type == PROJ_LASER)  return 1;
    if (item_defs[item_id].proj_type == PROJ_PLASMA) return 2;
    return 0;
}

void items_roll_alien_loadout(int alien_unit_type, int campaign_day,
                              int *out_weapon_id, int *out_ammo_id,
                              int *out_ammo_rds, int *out_grenade_id, int *out_grenades) {
    int r = rand() % 100;
    int wep = ITEM_PLASMA_RIFLE;
    int gren_chance = 25;

    if (alien_unit_type == 1) {
        /* Sectoid: 70% Plasma Pistol (early) or 50% later, rest Plasma Rifle */
        int pistol_pct = (campaign_day < 10) ? 70 : 50;
        wep = (r < pistol_pct) ? ITEM_PLASMA_PISTOL : ITEM_PLASMA_RIFLE;
        gren_chance = 25;
    } else if (alien_unit_type == 2) {
        /* Floater: 65% Plasma Rifle, 35% Plasma Pistol (or Heavy Plasma late game) */
        if (campaign_day > 18 && r < 20) wep = ITEM_HEAVY_PLASMA;
        else wep = (r < 65) ? ITEM_PLASMA_RIFLE : ITEM_PLASMA_PISTOL;
        gren_chance = 40;
    } else {
        /* Muton: 65% Heavy Plasma Cannon, 35% Plasma Rifle */
        wep = (r < 65) ? ITEM_HEAVY_PLASMA : ITEM_PLASMA_RIFLE;
        gren_chance = 55;
    }

    if (out_weapon_id)  *out_weapon_id  = wep;
    if (out_ammo_id)    *out_ammo_id    = item_defs[wep].ammo_item_id;
    if (out_ammo_rds)   *out_ammo_rds   = item_defs[wep].clip_size;
    if (out_grenade_id) *out_grenade_id = ITEM_ALIEN_GRENADE;
    if (out_grenades)   *out_grenades   = ((rand() % 100) < gren_chance) ? 1 : 0;
}

static void fill_buf_rect(uint8_t *buf, int bw, int bh, int x, int y, int w, int h, uint8_t col) {
    int py, px;
    for (py = y; py < y + h; py++) {
        if ((unsigned)py >= (unsigned)bh) continue;
        for (px = x; px < x + w; px++) {
            if ((unsigned)px < (unsigned)bw) {
                buf[py * bw + px] = col;
            }
        }
    }
}

void items_init_assets(void) {
    int i;
    for (i = 0; i < ITEM_COUNT; i++) {
        uint8_t *ico = spr_item_icons[i];
        uint8_t *gnd = spr_ground_items[i];
        const ItemDef *d = &item_defs[i];
        uint8_t main_c = 7, trim_c = 100, glow_c = 96;

        memset(ico, COL_TRANS, ITEM_ICON_W * ITEM_ICON_H);
        memset(gnd, COL_TRANS, GROUND_ITEM_W * GROUND_ITEM_H);

        if (d->proj_type == PROJ_LASER) {
            main_c = 74; trim_c = 80; glow_c = 83;
        } else if (d->proj_type == PROJ_PLASMA || i == ITEM_ELERIUM) {
            main_c = 9;  trim_c = 20; glow_c = 18;
        }

        switch (d->category) {
            case ICAT_WEAPON: {
                int is_pistol = (i == ITEM_PISTOL || i == ITEM_LASER_PISTOL || i == ITEM_PLASMA_PISTOL);
                int is_heavy  = (i == ITEM_HEAVY_RIFLE || i == ITEM_HEAVY_LASER || i == ITEM_HEAVY_PLASMA);
                int bx = is_pistol ? 9 : 3;
                int bw = is_pistol ? 13 : (is_heavy ? 26 : 23);
                int bh = is_heavy ? 5 : 3;
                /* UI 32x16 Weapon Silhouette */
                fill_buf_rect(ico, 32, 16, bx - 1, 5, bw + 2, bh + 5, 15);
                fill_buf_rect(ico, 32, 16, bx, 6, bw, bh, main_c);
                fill_buf_rect(ico, 32, 16, bx + 2, 6 + bh, 4, 4, trim_c);
                if (!is_pistol) {
                    fill_buf_rect(ico, 32, 16, bx + bw / 2, 6 + bh, 3, 4, glow_c);
                }
                fill_buf_rect(ico, 32, 16, bx + bw - 3, 7, 3, 1, glow_c);
                /* Ground 16x12 Weapon Sprite */
                fill_buf_rect(gnd, 16, 12, is_pistol ? 4 : 1, 4, is_pistol ? 8 : 14, 4, 15);
                fill_buf_rect(gnd, 16, 12, is_pistol ? 5 : 2, 5, is_pistol ? 6 : 12, 2, main_c);
                fill_buf_rect(gnd, 16, 12, 6, 7, 3, 2, glow_c);
                break;
            }
            case ICAT_AMMO:
                /* Magazine / Battery / Plasma Cartridge */
                fill_buf_rect(ico, 32, 16, 10, 2, 12, 12, 15);
                fill_buf_rect(ico, 32, 16, 11, 3, 10, 10, main_c);
                fill_buf_rect(ico, 32, 16, 12, 4, 8,  2,  glow_c);
                fill_buf_rect(ico, 32, 16, 12, 8, 8,  2,  trim_c);
                /* Ground 16x12 Magazine */
                fill_buf_rect(gnd, 16, 12, 4, 3, 8, 6, 15);
                fill_buf_rect(gnd, 16, 12, 5, 4, 6, 4, main_c);
                fill_buf_rect(gnd, 16, 12, 6, 5, 4, 2, glow_c);
                break;

            case ICAT_GRENADE:
            case ICAT_UTILITY: {
                uint8_t gc = (i == ITEM_FLARE) ? 96 : (i == ITEM_ALIEN_GRENADE ? 18 : (i == ITEM_HEAVY_GRENADE ? 34 : 21));
                fill_buf_rect(ico, 32, 16, 11, 3, 10, 10, 15);
                fill_buf_rect(ico, 32, 16, 12, 4, 8,  8,  gc);
                fill_buf_rect(ico, 32, 16, 14, 2, 4,  2,  98);
                fill_buf_rect(gnd, 16, 12, 5, 3, 6, 6, 15);
                fill_buf_rect(gnd, 16, 12, 6, 4, 4, 4, gc);
                break;
            }
            default: {
                uint8_t ac = (i == ITEM_ELERIUM) ? 113 : (i == ITEM_ALLOYS ? 7 : 80);
                if (d->category == ICAT_CORPSE) ac = 20;
                fill_buf_rect(ico, 32, 16, 8, 2, 16, 12, 15);
                fill_buf_rect(ico, 32, 16, 9, 3, 14, 10, ac);
                fill_buf_rect(ico, 32, 16, 11, 5, 10, 4, glow_c);
                fill_buf_rect(gnd, 16, 12, 3, 3, 10, 6, 15);
                fill_buf_rect(gnd, 16, 12, 4, 4, 8,  4, ac);
                break;
            }
        }
    }

    /* Load external 24-bit .BMP sprite sheets from assets/ (ITEMS.BMP, GNDITEMS.BMP, BASE.BMP) */
    {
        uint8_t *sheet;
        int sw = 0, sh = 0;

        sheet = bmp_load_alloc("ITEMS.BMP", &sw, &sh);
        if (sheet) {
            for (i = 0; i < ITEM_COUNT && (i + 1) * ITEM_ICON_W <= sw && sh >= ITEM_ICON_H; i++) {
                bmp_extract_tile(sheet, sw, sh, i * ITEM_ICON_W, 0,
                                 ITEM_ICON_W, ITEM_ICON_H, spr_item_icons[i]);
            }
            free(sheet);
        }

        sheet = bmp_load_alloc("GNDITEMS.BMP", &sw, &sh);
        if (sheet) {
            for (i = 0; i < ITEM_COUNT && (i + 1) * GROUND_ITEM_W <= sw && sh >= GROUND_ITEM_H; i++) {
                bmp_extract_tile(sheet, sw, sh, i * GROUND_ITEM_W, 0,
                                 GROUND_ITEM_W, GROUND_ITEM_H, spr_ground_items[i]);
            }
            free(sheet);
        }

        sheet = bmp_load_alloc("BASE.BMP", &sw, &sh);
        if (sheet) {
            for (i = 0; i < 12 && (i + 1) * BASE_TILE_W <= sw && sh >= BASE_TILE_H; i++) {
                bmp_extract_tile(sheet, sw, sh, i * BASE_TILE_W, 0,
                                 BASE_TILE_W, BASE_TILE_H, spr_base_tiles[i]);
            }
            has_base_bmp = 1;
            free(sheet);
        }
    }
}



