$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct Operative {
    public int active;
    public int assigned_arv;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
    public byte[] name;
    public int gender;
    public int portrait_id;
    public int weapon_id;
    public int sidearm_id;
    public int extra_clips;
    public int grenade_id;
    public int grenades;
    public int flares;
    public int special_id;
    public int rank;
    public int xp;
    public int kills;
    public int missions;
    public int tu_max;
    public int hp_max;
    public int energy_max;
    public int firing_acc;
    public int throw_acc;
    public int reactions;
    public int bravery;
    public int psi_skill;
    public int psi_trained;
    public int psi_hours_left;
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct MemorialEntry {
    public int active;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
    public byte[] name;
    public int rank;
    public int gender;
    public int portrait_id;
    public int kills;
    public int missions;
    public int day_kia;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 20)]
    public byte[] cause;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct ModalAlert {
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
    public byte[] title;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 44)]
    public byte[] line1;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 44)]
    public byte[] line2;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 44)]
    public byte[] line3;
    public byte accent_col;
    public byte sfx_type;
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct RadarEvent {
    public int active;
    public int event_num;
    public int biome_id;
    public int severity;
    public int ufo_size;
    public int is_flying;
    public int ufo_hp;
    public int ufo_hp_max;
    public int vx;
    public int vy;
    public int flight_mins;
    public int rx;
    public int ry;
    public int age_ticks;
    public int ttl_ticks;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 12)]
    public byte[] tag;
    public uint type_title;
    public uint loc_name;
    public uint intel_desc;
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct CampaignState {
    public int mode;
    public int active_tab;
    public int time_speed;
    public int selected_sector;
    public int deploy_night;
    public int missions_won;
    public int win_streak;
    public int total_kills;
    public int total_kia;
    public int total_civilians_saved;
    public int total_civilians_killed;
    public int total_map_damage;
    public int total_score;
    public int money;
    public int scientists;
    public int engineers;
    public int active_tech;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 27)]
    public byte[] tech_scientists;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 27)]
    public byte[] tech_done;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
    public byte[] _pad_tech;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 27)]
    public int[] tech_prog;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public byte[] item_seen;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
    public byte[] _pad_seen;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public int[] stores;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public byte[] mfg_queue;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
    public byte[] _pad_mfg;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public int[] mfg_prog;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public byte[] store_transit_qty;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public byte[] store_transit_hrs;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
    public byte[] _pad_transit;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 41)]
    public int[] arv_cargo;
    public int arv_turret_level;
    public int arv_drone_item;

    public int elerium;
    public int alloys;
    public int nav_units;
    public int cryo_pods;
    public int tech_pods;
    public int alien_corpses;
    public int debrief_live_captured;
    public int debrief_died_no_containment;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
    public Operative[] squad;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
    public Operative[] recruit_pool;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
    public MemorialEntry[] memorial;
    public int memorial_count;
    public int selected_soldier;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 12)]
    public int[] bunker_grid;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 48)]
    public byte[] notice;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
    public ModalAlert[] modal_queue;
    public int modal_count;
    public int modal_open;
    public int modal_anim_frame;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
    public RadarEvent[] events;
    public int next_event_num;
    public int selected_event;
    public int launched_event;
    public int launch_prompt_open;
    public int radar_tick;
    public int next_spawn_timer;
    public int report_banner_timer;
    public int last_reported_idx;
    public int valley_day;
    public int valley_minute;

    public int intercept_open;
    public int intercept_ev;
    public int intercept_tx;
    public int intercept_ty;
    public int intercept_vx;
    public int intercept_vy;
    public int intercept_cooldown;
    public int intercept_timer;
    public int intercept_shot_t;
    public int intercept_shot_x;
    public int intercept_shot_y;
    public int intercept_flash;

    public int base_modal;
    public int build_r;
    public int build_c;
    public int store_scroll;
    public int lab_subtab;
    public int log_sel;
    public int armory_subtab;
    public int game_over_reset;
}

public static class SaveGen {
    public static byte[] StrBytes(string s, int len) {
        byte[] b = new byte[len];
        if (!string.IsNullOrEmpty(s)) {
            byte[] src = Encoding.ASCII.GetBytes(s);
            int n = Math.Min(src.Length, len - 1);
            Array.Copy(src, b, n);
        }
        return b;
    }

    public static CampaignState CreateEmpty() {
        CampaignState c = new CampaignState();
        c.tech_scientists = new byte[27];
        c.tech_done = new byte[27];
        c._pad_tech = new byte[2];
        c.tech_prog = new int[27];
        c.item_seen = new byte[41];
        c._pad_seen = new byte[3];
        c.stores = new int[41];
        c.mfg_queue = new byte[41];
        c._pad_mfg = new byte[3];
        c.mfg_prog = new int[41];
        c.store_transit_qty = new byte[41];
        c.store_transit_hrs = new byte[41];
        c._pad_transit = new byte[2];
        c.arv_cargo = new int[41];
        c.squad = new Operative[16];
        for (int i = 0; i < 16; i++) c.squad[i].name = new byte[16];
        c.recruit_pool = new Operative[8];
        for (int i = 0; i < 8; i++) c.recruit_pool[i].name = new byte[16];
        c.memorial = new MemorialEntry[64];
        for (int i = 0; i < 64; i++) {
            c.memorial[i].name = new byte[16];
            c.memorial[i].cause = new byte[20];
        }
        c.bunker_grid = new int[12];
        c.notice = new byte[48];
        c.modal_queue = new ModalAlert[8];
        for (int i = 0; i < 8; i++) {
            c.modal_queue[i].title = new byte[32];
            c.modal_queue[i].line1 = new byte[44];
            c.modal_queue[i].line2 = new byte[44];
            c.modal_queue[i].line3 = new byte[44];
        }
        c.events = new RadarEvent[3];
        for (int i = 0; i < 3; i++) c.events[i].tag = new byte[12];

        c.selected_event = -1;
        c.launched_event = -1;
        c.last_reported_idx = -1;
        c.active_tech = -1;
        c.arv_drone_item = -1;
        c.next_spawn_timer = 1200;

        // Default starting base facilities
        c.bunker_grid[1 * 4 + 1] = 1; // FAC_MAIN_HOUSE
        c.bunker_grid[1 * 4 + 2] = 2; // FAC_GARAGE
        c.bunker_grid[0 * 4 + 1] = 3; // FAC_RADAR
        c.bunker_grid[1 * 4 + 0] = 4; // FAC_LAB

        string[] recNames = { "RODRIGUEZ", "CHEN", "KOVACS", "MILLER", "NOVAK", "SANTOS", "PARKER", "WEBER" };
        for (int i = 0; i < 8; i++) {
            int g = i & 1;
            c.recruit_pool[i].active = 1;
            c.recruit_pool[i].name = StrBytes(recNames[i], 16);
            c.recruit_pool[i].gender = g;
            c.recruit_pool[i].portrait_id = g != 0 ? (6 + (i % 6)) : (i % 6);
            c.recruit_pool[i].weapon_id = 1;
            c.recruit_pool[i].sidearm_id = -1;
            c.recruit_pool[i].special_id = -1;
            c.recruit_pool[i].extra_clips = 2;
            c.recruit_pool[i].grenade_id = 6;
            c.recruit_pool[i].grenades = 2;
            c.recruit_pool[i].flares = 1;
            c.recruit_pool[i].tu_max = 58;
            c.recruit_pool[i].hp_max = 42;
            c.recruit_pool[i].energy_max = 52;
            c.recruit_pool[i].firing_acc = 58;
            c.recruit_pool[i].throw_acc = 60;
            c.recruit_pool[i].reactions = 46;
            c.recruit_pool[i].bravery = 70;
            c.recruit_pool[i].psi_skill = 45 + i * 6;
        }
        return c;
    }

    public static void PopulateSquad(ref CampaignState c, int count, int minRank, int maxRank,
                                     int wep, int sidearm, int special, int gren, int psiCount) {
        string[] names = { "KELLER", "DECKER", "HARRIS", "LUCAS", "BARBER", "VANCE", "MORGAN", "REYES", "BISHOP", "HUDSON" };
        int[] genders  = { 0, 1, 0, 0, 0, 1, 0, 1, 0, 0 };
        int[] ports    = { 0, 6, 1, 2, 3, 7, 4, 8, 5, 9 };
        int[] reqXp    = { 0, 50, 120, 220, 360, 550 };
        for (int i = 0; i < count && i < 10; i++) {
            int r = minRank + ((maxRank > minRank) ? (i % (maxRank - minRank + 1)) : 0);
            c.squad[i].active = 1;
            c.squad[i].assigned_arv = (i < 8) ? 1 : 0;
            c.squad[i].name = StrBytes(names[i], 16);
            c.squad[i].gender = genders[i];
            c.squad[i].portrait_id = ports[i];
            c.squad[i].rank = r;
            c.squad[i].xp = reqXp[r] + 20;
            c.squad[i].kills = 3 + r * 3;
            c.squad[i].missions = 2 + r * 2;
            c.squad[i].weapon_id = wep;
            c.squad[i].sidearm_id = sidearm;
            c.squad[i].special_id = (i < psiCount) ? 38 : ((i % 3 == 0) ? 37 : special);
            c.squad[i].extra_clips = 2;
            c.squad[i].grenade_id = gren;
            c.squad[i].grenades = 2;
            c.squad[i].flares = 1;
            c.squad[i].tu_max = 60 + r * 4;
            c.squad[i].hp_max = 44 + r * 5;
            c.squad[i].energy_max = 54 + r * 4;
            c.squad[i].firing_acc = 62 + r * 6;
            c.squad[i].throw_acc = 62 + r * 5;
            c.squad[i].reactions = 50 + r * 5;
            c.squad[i].bravery = 70 + r * 4;
            c.squad[i].psi_skill = 76 + (i * 4) % 20;
            c.squad[i].psi_trained = (i < psiCount) ? 2 : 0;
            c.squad[i].psi_hours_left = 0;
        }
    }

    public static void SetEvent(ref CampaignState c, int idx, int evNum, int biome, int rx, int ry,
                                int ufoSz, int isFlying, int severity, string tag) {
        c.events[idx].active = 1;
        c.events[idx].event_num = evNum;
        c.events[idx].biome_id = biome;
        c.events[idx].ufo_size = ufoSz;
        c.events[idx].is_flying = isFlying;
        c.events[idx].severity = severity;
        c.events[idx].ufo_hp_max = (ufoSz == 0) ? 60 : (ufoSz == 1 ? 120 : (ufoSz == 2 ? 190 : 260));
        c.events[idx].ufo_hp = c.events[idx].ufo_hp_max;
        c.events[idx].vx = (idx % 2 == 1) ? -2 : 2;
        c.events[idx].vy = (idx % 2 == 1) ? 1 : -1;
        c.events[idx].flight_mins = isFlying != 0 ? 60 : 0;
        c.events[idx].rx = rx;
        c.events[idx].ry = ry;
        c.events[idx].ttl_ticks = 2880;
        c.events[idx].tag = StrBytes(tag, 12);
    }

    public static void WriteSave(string path, CampaignState c) {
        int sz = Marshal.SizeOf(typeof(CampaignState));
        if (sz != 9632) {
            throw new Exception("Struct size mismatch: expected 9632, got " + sz);
        }
        IntPtr ptr = Marshal.AllocHGlobal(sz);
        try {
            Marshal.StructureToPtr(c, ptr, false);
            byte[] buf = new byte[sz];
            Marshal.Copy(ptr, buf, 0, sz);
            using (BinaryWriter bw = new BinaryWriter(File.Open(path, FileMode.Create))) {
                bw.Write((uint)0x44554637U);
                bw.Write((uint)sz);
                bw.Write(buf);
            }
        } finally {
            Marshal.FreeHGlobal(ptr);
        }
    }
}
"@

# Item IDs
$ITEM_PISTOL = 0; $ITEM_ASSAULT_RIFLE = 1; $ITEM_HEAVY_RIFLE = 2
$ITEM_PISTOL_CLIP = 3; $ITEM_RIFLE_MAG = 4; $ITEM_HEAVY_MAG = 5
$ITEM_FRAG_GRENADE = 6; $ITEM_HEAVY_GRENADE = 7; $ITEM_FLARE = 8
$ITEM_LASER_PISTOL = 9; $ITEM_LASER_RIFLE = 10; $ITEM_HEAVY_LASER = 11
$ITEM_LASER_BATTERY = 12; $ITEM_HEAVY_LASER_PAK = 13
$ITEM_PLASMA_PISTOL = 14; $ITEM_PLASMA_RIFLE = 15; $ITEM_HEAVY_PLASMA = 16
$ITEM_PLASMA_PISTOL_CLIP = 17; $ITEM_PLASMA_RIFLE_CLIP = 18; $ITEM_HEAVY_PLASMA_CLIP = 19
$ITEM_ALIEN_GRENADE = 20
$ITEM_ELERIUM = 21; $ITEM_ALLOYS = 22
$ITEM_CORPSE_SECTOID = 26; $ITEM_CORPSE_FLOATER = 27; $ITEM_CORPSE_MUTON = 28; $ITEM_CORPSE_CRYPTID = 29
$ITEM_LIVE_SECTOID = 30; $ITEM_LIVE_SECTOID_CMDR = 31; $ITEM_LIVE_FLOATER = 32; $ITEM_LIVE_MUTON = 33; $ITEM_LIVE_CRYPTID = 34
$ITEM_STUN_ROD = 35; $ITEM_STUN_GRENADE = 36; $ITEM_MEDKIT = 37; $ITEM_MIND_PROBE = 38
$ITEM_QUAD_DRONE = 39; $ITEM_UFO_DRONE = 40

# 1. SAVE1.SAV: Early-Mid Game (Laser Tech, Quad-Copter Drone, Mk1 Turret, Stun Capture)
$c1 = [SaveGen]::CreateEmpty()
$c1.valley_day = 6; $c1.valley_minute = 14 * 60
$c1.missions_won = 3; $c1.win_streak = 2; $c1.total_kills = 18; $c1.total_score = 1450
$c1.money = 35000; $c1.scientists = 4; $c1.engineers = 4; $c1.alloys = 32; $c1.elerium = 8
$c1.bunker_grid[0] = 6; $c1.bunker_grid[2] = 7; $c1.bunker_grid[3] = 10; $c1.bunker_grid[7] = 8
foreach ($t in @(0,1,2,3,4,11,12,26)) { $c1.tech_done[$t] = 1 }
$c1.active_tech = 14
$c1.tech_scientists[14] = 2; $c1.tech_prog[14] = 24
$c1.tech_scientists[5]  = 2; $c1.tech_prog[5]  = 30
$c1.arv_turret_level = 1; $c1.arv_drone_item = $ITEM_QUAD_DRONE
$c1.stores[$ITEM_LASER_RIFLE] = 4; $c1.stores[$ITEM_LASER_PISTOL] = 4; $c1.stores[$ITEM_HEAVY_LASER] = 2
$c1.stores[$ITEM_LASER_BATTERY] = 12; $c1.stores[$ITEM_HEAVY_LASER_PAK] = 6
$c1.stores[$ITEM_STUN_ROD] = 3; $c1.stores[$ITEM_STUN_GRENADE] = 6; $c1.stores[$ITEM_MEDKIT] = 4
$c1.stores[$ITEM_QUAD_DRONE] = 1; $c1.stores[$ITEM_ALLOYS] = 32; $c1.stores[$ITEM_ELERIUM] = 8
$c1.stores[$ITEM_CORPSE_SECTOID] = 3; $c1.stores[$ITEM_CORPSE_FLOATER] = 1; $c1.stores[$ITEM_LIVE_SECTOID] = 1
$c1.arv_cargo[$ITEM_LASER_RIFLE] = 6; $c1.arv_cargo[$ITEM_LASER_PISTOL] = 3; $c1.arv_cargo[$ITEM_HEAVY_LASER] = 2
$c1.arv_cargo[$ITEM_LASER_BATTERY] = 16; $c1.arv_cargo[$ITEM_HEAVY_LASER_PAK] = 6
$c1.arv_cargo[$ITEM_STUN_ROD] = 3; $c1.arv_cargo[$ITEM_STUN_GRENADE] = 8; $c1.arv_cargo[$ITEM_MEDKIT] = 4
[SaveGen]::PopulateSquad([ref]$c1, 6, 1, 2, $ITEM_LASER_RIFLE, $ITEM_LASER_PISTOL, $ITEM_STUN_ROD, $ITEM_STUN_GRENADE, 0)
[SaveGen]::SetEvent([ref]$c1, 0, 1, 1, 122, 124, 1, 0, 0, "SITE-01")
[SaveGen]::SetEvent([ref]$c1, 1, 2, 0, 127, 92,  0, 0, 1, "UFO-02")
[SaveGen]::SetEvent([ref]$c1, 2, 3, 2, 80,  82,  1, 1, 1, "AIR-03")
$c1.selected_event = 0; $c1.next_event_num = 3

# 2. SAVE2.SAV: Mid-Late Game (Plasma Tech, UFO Alloy Drone, Mk2 Turret, Mfg & Store Transit)
$c2 = [SaveGen]::CreateEmpty()
$c2.valley_day = 14; $c2.valley_minute = 21 * 60 + 30
$c2.missions_won = 6; $c2.win_streak = 2; $c2.total_kills = 44; $c2.total_score = 3600
$c2.money = 58000; $c2.scientists = 6; $c2.engineers = 6; $c2.alloys = 65; $c2.elerium = 35
$c2.bunker_grid[0] = 6; $c2.bunker_grid[2] = 7; $c2.bunker_grid[3] = 10; $c2.bunker_grid[7] = 8
$c2.bunker_grid[8] = 9; $c2.bunker_grid[9] = 6; $c2.bunker_grid[10] = 5; $c2.bunker_grid[11] = 11
for ($i = 0; $i -le 19; $i++) { $c2.tech_done[$i] = 1 }
$c2.tech_done[25] = 1; $c2.tech_done[26] = 1
$c2.mfg_queue[$ITEM_UFO_DRONE] = 1; $c2.mfg_prog[$ITEM_UFO_DRONE] = 60
$c2.store_transit_qty[$ITEM_STUN_GRENADE] = 4; $c2.store_transit_hrs[$ITEM_STUN_GRENADE] = 2
$c2.arv_turret_level = 2; $c2.arv_drone_item = $ITEM_UFO_DRONE
$c2.stores[$ITEM_PLASMA_RIFLE] = 6; $c2.stores[$ITEM_HEAVY_PLASMA] = 4; $c2.stores[$ITEM_PLASMA_PISTOL] = 4
$c2.stores[$ITEM_PLASMA_RIFLE_CLIP] = 14; $c2.stores[$ITEM_HEAVY_PLASMA_CLIP] = 10; $c2.stores[$ITEM_PLASMA_PISTOL_CLIP] = 10
$c2.stores[$ITEM_ALIEN_GRENADE] = 8; $c2.stores[$ITEM_STUN_GRENADE] = 8; $c2.stores[$ITEM_STUN_ROD] = 4; $c2.stores[$ITEM_MEDKIT] = 6
$c2.stores[$ITEM_QUAD_DRONE] = 1; $c2.stores[$ITEM_UFO_DRONE] = 1; $c2.stores[$ITEM_ALLOYS] = 65; $c2.stores[$ITEM_ELERIUM] = 35
$c2.stores[$ITEM_CORPSE_MUTON] = 2; $c2.stores[$ITEM_LIVE_FLOATER] = 2
$c2.arv_cargo[$ITEM_PLASMA_RIFLE] = 6; $c2.arv_cargo[$ITEM_HEAVY_PLASMA] = 2
$c2.arv_cargo[$ITEM_PLASMA_RIFLE_CLIP] = 16; $c2.arv_cargo[$ITEM_HEAVY_PLASMA_CLIP] = 8
$c2.arv_cargo[$ITEM_ALIEN_GRENADE] = 8; $c2.arv_cargo[$ITEM_STUN_GRENADE] = 6; $c2.arv_cargo[$ITEM_STUN_ROD] = 3; $c2.arv_cargo[$ITEM_MEDKIT] = 5
[SaveGen]::PopulateSquad([ref]$c2, 7, 2, 3, $ITEM_PLASMA_RIFLE, $ITEM_PLASMA_PISTOL, $ITEM_MEDKIT, $ITEM_ALIEN_GRENADE, 0)
[SaveGen]::SetEvent([ref]$c2, 0, 1, 3, 155, 52,  2, 0, 1, "UFO-01")
[SaveGen]::SetEvent([ref]$c2, 1, 2, 1, 177, 100, 1, 0, 0, "SITE-02")
[SaveGen]::SetEvent([ref]$c2, 2, 3, 2, 157, 84,  2, 1, 1, "AIR-03")
$c2.selected_event = 0; $c2.next_event_num = 3

# 3. SAVE3.SAV: Psionics, Mind Probes, Command Dreadnought (48x48 3-Deck Ship) & Terror Raid
$c3 = [SaveGen]::CreateEmpty()
$c3.valley_day = 22; $c3.valley_minute = 11 * 60
$c3.missions_won = 9; $c3.win_streak = 2; $c3.total_kills = 72; $c3.total_score = 6200
$c3.money = 75000; $c3.scientists = 6; $c3.engineers = 6; $c3.alloys = 90; $c3.elerium = 60
$c3.bunker_grid[0] = 6; $c3.bunker_grid[2] = 7; $c3.bunker_grid[3] = 6; $c3.bunker_grid[7] = 8
$c3.bunker_grid[8] = 9; $c3.bunker_grid[9] = 12; $c3.bunker_grid[10] = 5; $c3.bunker_grid[11] = 11
for ($i = 0; $i -lt 27; $i++) { $c3.tech_done[$i] = 1 }
$c3.tech_done[20] = 0; $c3.tech_done[21] = 0
$c3.active_tech = 21; $c3.tech_scientists[21] = 4; $c3.tech_prog[21] = 45
$c3.arv_turret_level = 2; $c3.arv_drone_item = $ITEM_UFO_DRONE
$c3.stores[$ITEM_PLASMA_RIFLE] = 8; $c3.stores[$ITEM_HEAVY_PLASMA] = 6
$c3.stores[$ITEM_PLASMA_RIFLE_CLIP] = 20; $c3.stores[$ITEM_HEAVY_PLASMA_CLIP] = 16
$c3.stores[$ITEM_MIND_PROBE] = 6; $c3.stores[$ITEM_MEDKIT] = 6; $c3.stores[$ITEM_STUN_ROD] = 4
$c3.stores[$ITEM_STUN_GRENADE] = 10; $c3.stores[$ITEM_ALIEN_GRENADE] = 10
$c3.stores[$ITEM_UFO_DRONE] = 2; $c3.stores[$ITEM_QUAD_DRONE] = 1
$c3.stores[$ITEM_ALLOYS] = 90; $c3.stores[$ITEM_ELERIUM] = 60
$c3.stores[$ITEM_LIVE_SECTOID_CMDR] = 1; $c3.stores[$ITEM_LIVE_MUTON] = 2; $c3.stores[$ITEM_LIVE_CRYPTID] = 1; $c3.stores[$ITEM_CORPSE_CRYPTID] = 2
$c3.arv_cargo[$ITEM_PLASMA_RIFLE] = 6; $c3.arv_cargo[$ITEM_HEAVY_PLASMA] = 3
$c3.arv_cargo[$ITEM_PLASMA_RIFLE_CLIP] = 18; $c3.arv_cargo[$ITEM_HEAVY_PLASMA_CLIP] = 10
$c3.arv_cargo[$ITEM_MIND_PROBE] = 5; $c3.arv_cargo[$ITEM_MEDKIT] = 4; $c3.arv_cargo[$ITEM_STUN_ROD] = 3
$c3.arv_cargo[$ITEM_STUN_GRENADE] = 6; $c3.arv_cargo[$ITEM_ALIEN_GRENADE] = 6
[SaveGen]::PopulateSquad([ref]$c3, 10, 3, 4, $ITEM_PLASMA_RIFLE, $ITEM_STUN_ROD, $ITEM_MEDKIT, $ITEM_ALIEN_GRENADE, 4)
$c3.squad[8].assigned_arv = 0; $c3.squad[8].psi_trained = 1; $c3.squad[8].psi_hours_left = 12
$c3.squad[9].assigned_arv = 0; $c3.squad[9].psi_trained = 1; $c3.squad[9].psi_hours_left = 12
[SaveGen]::SetEvent([ref]$c3, 0, 1, 3, 155, 52,  3, 0, 1, "UFO-01")
[SaveGen]::SetEvent([ref]$c3, 1, 2, 2, 108, 100, 2, 0, 2, "RAID-02")
[SaveGen]::SetEvent([ref]$c3, 2, 3, 0, 133, 76,  3, 1, 1, "AIR-03")
$c3.selected_event = 0; $c3.next_event_num = 3

# 4. SAVE4.SAV: 100% Everything Unlocked Master Sandbox
$c4 = [SaveGen]::CreateEmpty()
$c4.valley_day = 30; $c4.valley_minute = 12 * 60
$c4.missions_won = 14; $c4.win_streak = 2; $c4.total_kills = 128; $c4.total_score = 12500
$c4.money = 99999; $c4.scientists = 6; $c4.engineers = 6; $c4.alloys = 150; $c4.elerium = 120
$c4.bunker_grid[0] = 6; $c4.bunker_grid[2] = 7; $c4.bunker_grid[3] = 6; $c4.bunker_grid[7] = 8
$c4.bunker_grid[8] = 9; $c4.bunker_grid[9] = 12; $c4.bunker_grid[10] = 5; $c4.bunker_grid[11] = 11
for ($i = 0; $i -lt 27; $i++) { $c4.tech_done[$i] = 1 }
for ($i = 0; $i -lt 41; $i++) { $c4.item_seen[$i] = 1; $c4.stores[$i] = 6 }
$c4.stores[$ITEM_ALLOYS] = 150; $c4.stores[$ITEM_ELERIUM] = 120
$c4.arv_turret_level = 2; $c4.arv_drone_item = $ITEM_UFO_DRONE
$c4.arv_cargo[$ITEM_HEAVY_PLASMA] = 4; $c4.arv_cargo[$ITEM_PLASMA_RIFLE] = 6
$c4.arv_cargo[$ITEM_HEAVY_PLASMA_CLIP] = 12; $c4.arv_cargo[$ITEM_PLASMA_RIFLE_CLIP] = 16
$c4.arv_cargo[$ITEM_MIND_PROBE] = 6; $c4.arv_cargo[$ITEM_MEDKIT] = 6; $c4.arv_cargo[$ITEM_STUN_ROD] = 4
$c4.arv_cargo[$ITEM_STUN_GRENADE] = 8; $c4.arv_cargo[$ITEM_ALIEN_GRENADE] = 8
[SaveGen]::PopulateSquad([ref]$c4, 10, 4, 5, $ITEM_HEAVY_PLASMA, $ITEM_MIND_PROBE, $ITEM_MEDKIT, $ITEM_ALIEN_GRENADE, 8)
[SaveGen]::SetEvent([ref]$c4, 0, 1, 3, 205, 46,  3, 0, 1, "UFO-01")
[SaveGen]::SetEvent([ref]$c4, 1, 2, 1, 122, 124, 2, 0, 0, "SITE-02")
[SaveGen]::SetEvent([ref]$c4, 2, 3, 0, 127, 92,  0, 0, 1, "UFO-03")
$c4.selected_event = 0; $c4.next_event_num = 3

# 5. SAVE5.SAV: Coopersburg HQ Home Base Retaliation Defense + Flying Scout
$c5 = [SaveGen]::CreateEmpty()
$c5.valley_day = 18; $c5.valley_minute = 15 * 60
$c5.missions_won = 8; $c5.win_streak = 0; $c5.total_kills = 60; $c5.total_score = 4900
$c5.money = 64000; $c5.scientists = 6; $c5.engineers = 6; $c5.alloys = 70; $c5.elerium = 45
$c5.bunker_grid[0] = 6; $c5.bunker_grid[2] = 7; $c5.bunker_grid[3] = 6; $c5.bunker_grid[7] = 8
$c5.bunker_grid[8] = 9; $c5.bunker_grid[9] = 12; $c5.bunker_grid[10] = 5; $c5.bunker_grid[11] = 11
for ($i = 0; $i -lt 27; $i++) { $c5.tech_done[$i] = 1 }
$c5.arv_turret_level = 2; $c5.arv_drone_item = $ITEM_UFO_DRONE
$c5.stores[$ITEM_PLASMA_RIFLE] = 8; $c5.stores[$ITEM_HEAVY_PLASMA] = 4
$c5.stores[$ITEM_PLASMA_RIFLE_CLIP] = 20; $c5.stores[$ITEM_HEAVY_PLASMA_CLIP] = 12
$c5.stores[$ITEM_ALIEN_GRENADE] = 10; $c5.stores[$ITEM_MEDKIT] = 6; $c5.stores[$ITEM_MIND_PROBE] = 4
$c5.arv_cargo[$ITEM_PLASMA_RIFLE] = 6; $c5.arv_cargo[$ITEM_HEAVY_PLASMA] = 3
$c5.arv_cargo[$ITEM_PLASMA_RIFLE_CLIP] = 18; $c5.arv_cargo[$ITEM_HEAVY_PLASMA_CLIP] = 10
$c5.arv_cargo[$ITEM_ALIEN_GRENADE] = 8; $c5.arv_cargo[$ITEM_MEDKIT] = 6; $c5.arv_cargo[$ITEM_MIND_PROBE] = 4
[SaveGen]::PopulateSquad([ref]$c5, 8, 3, 4, $ITEM_PLASMA_RIFLE, $ITEM_MIND_PROBE, $ITEM_MEDKIT, $ITEM_ALIEN_GRENADE, 4)
[SaveGen]::SetEvent([ref]$c5, 0, 1, 4, 132, 100, 2, 0, 2, "BASE-HQ")
[SaveGen]::SetEvent([ref]$c5, 1, 2, 1, 62,  110, 0, 1, 1, "AIR-02")
$c5.events[1].flight_mins = 12
$c5.selected_event = 0; $c5.selected_sector = 4; $c5.next_event_num = 2

foreach ($dir in @(".", "dist", "build")) {
    if (Test-Path $dir) {
        [SaveGen]::WriteSave((Join-Path $dir "SAVE1.SAV"), $c1)
        [SaveGen]::WriteSave((Join-Path $dir "SAVE2.SAV"), $c2)
        [SaveGen]::WriteSave((Join-Path $dir "SAVE3.SAV"), $c3)
        [SaveGen]::WriteSave((Join-Path $dir "SAVE4.SAV"), $c4)
        [SaveGen]::WriteSave((Join-Path $dir "SAVE5.SAV"), $c5)
        [SaveGen]::WriteSave((Join-Path $dir "DUFFSAVE.SAV"), $c1)
    }
}
Write-Host "Successfully generated SAVE1.SAV..SAVE5.SAV and DUFFSAVE.SAV (9640 bytes each) in ., dist/, and build/!"
