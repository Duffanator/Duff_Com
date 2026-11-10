# DUFF-COM: Playtest Fixes & Feature Implementation Plan

This document tracks the multi-phase refactoring and feature additions for **DUFF-COM: Coopersburg Defense Force**.

---

## Phase 1: Core Data Models — Items, Ammo, Tech Tree, Economy, Starting Squad & Memorial
**Status:** *Completed*


### 1.1 Weapons, Ammo & Equipment Registry (`src/items.c/.h`)
- [x] **Starting Human Weapons & Ammo (Researched at Start)**:
  - `ITEM_PISTOL` (9mm Service Pistol) + `ITEM_PISTOL_CLIP` (9mm Clip, 12 rds)
  - `ITEM_ASSAULT_RIFLE` (5.56mm Assault Rifle) + `ITEM_RIFLE_MAG` (5.56mm Mag, 20 rds)
  - `ITEM_HEAVY_RIFLE` (7.62mm Heavy Rifle) + `ITEM_HEAVY_MAG` (7.62mm Drum, 15 rds)
  - `ITEM_FRAG_GRENADE` (Light Frag Grenade — standard damage/blast radius)
  - `ITEM_HEAVY_GRENADE` (Heavy HE Satchel Grenade — higher damage & wider blast radius)
  - `ITEM_FLARE` (Night Electro-Flare)
- [x] **Laser Tier (Requires Research; Manufactured or Purchased once Researched)**:
  - `ITEM_LASER_PISTOL` (Laser Pistol) + `ITEM_LASER_BATTERY` (Laser Cell Battery, 15 shots)
  - `ITEM_LASER_RIFLE` (Prototype Laser Rifle) + `ITEM_LASER_BATTERY` (Laser Cell Battery, 12 shots)
  - `ITEM_HEAVY_LASER` (Heavy Laser Cannon) + `ITEM_HEAVY_LASER_PAK` (High-Cap Laser Pack, 8 shots)
- [x] **Alien Plasma Tier (Requires Research to Use, Buy, or Manufacture; Uses Elerium-115)**:
  - `ITEM_PLASMA_PISTOL` (Alien Plasma Pistol) + `ITEM_PLASMA_PISTOL_CLIP` (Plasma Pistol Cartridge, 15 shots)
  - `ITEM_PLASMA_RIFLE` (Alien Plasma Rifle) + `ITEM_PLASMA_RIFLE_CLIP` (Plasma Rifle Cartridge, 12 shots)
  - `ITEM_HEAVY_PLASMA` (Heavy Plasma Cannon) + `ITEM_HEAVY_PLASMA_CLIP` (Heavy Plasma Core, 10 shots)
  - `ITEM_ALIEN_GRENADE` (Alien Elerium Pulse Grenade — heavy blast & damage)
- [x] **Recovered Alien Artifacts & Corpses (Sellable & Researchable; Never Purchasable)**:
  - `ITEM_ELERIUM` (Elerium-115 — can never be manufactured, only recovered from missions)
  - `ITEM_ALLOYS` (Alien Alloys)
  - `ITEM_UFO_NAV` (UFO Navigation Console)
  - `ITEM_UFO_CRYO` (Alien Cryo-Stasis Chamber)
  - `ITEM_UFO_POD` (Alien Ordnance / Tech Pod)
  - `ITEM_CORPSE_SECTOID`, `ITEM_CORPSE_FLOATER`, `ITEM_CORPSE_MUTON`
- [x] **Alien Race Loadout Tables**:
  - Sectoids: Plasma Pistol (70%) or Plasma Rifle (30%) + matching cartridges + 25% chance Alien Grenade.
  - Floaters: Plasma Rifle (65%) or Plasma Pistol (35%) + matching cartridges + 40% chance Alien Grenade.
  - Mutons: Heavy Plasma (65%) or Plasma Rifle (35%) + matching cartridges + 55% chance Alien Grenade.
- [x] **Graphical Item & Ammo Sprites**:
  - Procedural pixel-art sprites for every weapon, magazine/battery/cartridge, grenade, flare, and artifact for Store/Equip UI (`32x16`) and Battlefield ground tiles (`16x12`).

### 1.2 Money & Compensation System
- [x] Start new campaign with `$12,000` funding.
- [x] Award cash bounties for completing missions and neutralizing aliens.
- [x] Full Store Buy/Sell pricing table:
  - Cannot buy unresearched weapons/ammo, but can sell any collected item/artifact for cash.

### 1.3 Research Tree & Armored Response Vehicle (ARV) Upgrades
- [x] Research topics for Laser Weapons, Alien Plasma Weapons & Cartridges, Alien Grenades, Alien Autopsies, UFO Subsystems, and ARV Upgrades:
  - `TECH_UFO_CONSTRUCTION` (Expanded ARV Weapon/Equipment Storage Rack)
  - `TECH_ARV_TURRET` (ARV Roof-Mounted Defensive Fire System)
  - `TECH_ARV_TURRET_MK2` (ARV Fire Control Upgrade: +Range, +Accuracy, Lower Cooldown)

### 1.4 Starting Operatives, Throw Stat & KIA Memorial (`src/operative.c/.h`)
- [x] Fixed starting squad on new game:
  1. **KELLER** (Male, random male face `0..5`)
  2. **DECKER** (Female, random female face `6..11`)
  3. **HARR** (Male, random male face `0..5`)
  4. **LUCAS** (Male, random male face `0..5`)
  5. **BARBER** (Male, random male face `0..5`)
- [x] Expand `recruit_name_pool[]` to 120+ surnames for long campaigns.
- [x] Add `throw_acc` (Throwing Accuracy %) stat to `Operative` and `Unit` that increases with XP and rank promotions.
- [x] Persistent `MemorialEntry` list recording all KIA / left-behind operatives.

---


## Phase 2: Valley Radar Screen Overhaul & UFO Interception
**Status:** *Completed*

- [x] Move top navigation buttons (`RADAR`, `BASE`, `ARMORY`, `OPTIONS`) to a right-hand side vertical menu (`x = 246..319`).
- [x] Add time-passing speed buttons on the right-hand menu (`1S` = Realtime, `5M`, `1H` = 1 hr/sec, `1D` = 1 day/sec).
- [x] Slow down UFO spawn frequency at the start of the campaign; progressively increase alien aggression as days/wins advance.
- [x] Track flying UFOs moving across the Valley radar map with 3 dot sizes:
  - Small (Scout), Medium (Cruiser), Large (Battleship/Assault Ship — more aliens & Elerium/tech).
- [x] Before Duff-COM builds offensive systems, regional X-COM interceptors shoot down flying UFOs to create crash cleanup missions.
- [x] Landed UFOs (`crashed == 0`) spawn completely undamaged on the tactical map (no hull breaches or scorch marks).
- [x] If base offensive systems are built, clicking a flying UFO launches a **Shoot-Down Interception Mini-Game**; wherever the UFO is downed determines the mission's biome.


---

## Phase 3: Colonial House Base, Limited Stores, Research Lab & Store
**Status:** *Completed*

- [x] Center tile (`[1][1]`) is the **Main Colonial House**; starting connected tiles are **Main House**, **Armored Vehicle Garage**, **Homemade Radar Unit**, and **Homemade Lab/Workshop**.
- [x] Render all base facilities graphically with mouse-over tooltips instead of plain brown/gray grid boxes.
- [x] Clicking an empty lot connected to existing base tiles opens a construction menu (Alien Containment, Upgraded Lab, Upgraded Workshop, Missile Defense, Living Quarters, Equipment Stores, Offensive UFO Systems).
- [x] View Base Stores of equipment and collected alien artifacts with limited capacity (expandable via Equipment Store tiles).
- [x] Access the **Buy/Sell Store**, **Research Lab / Log**, **Workshop Manufacturing**, and **ARV Loadout** from the base screen.


---

## Phase 4: Squad Armory & ARV Loadout Screen
**Status:** *Completed*

- [x] Single-operative armory view with `[<]` / `[>]` cycling (`[AGENT]`).
- [x] Lock operative faces and genders (remove Face/Gender toggle buttons).
- [x] Assign researched weapons, ammo magazines/batteries/cartridges, grenades, and flares to operatives and to the **Armored Response Vehicle** (`[ARV BUS]`, respecting ARV cargo capacity).
- [x] View the persistent **KIA Memorial Wall** (`[MEMORIAL]`).


---

## Phase 5: Battlefield Changes — ARV Bus, Ground Items, Ammo, Throwing, Alien Heads & UFO Core Mini-Nuke
**Status:** *Completed*

- [x] **Pre-Mission Equip Menu**: Equip operatives from the ARV before clicking `[START MISSION]`.
- [x] **Makeshift Armored Response Vehicle (Resistance Bus)**:
  - Operatives start inside the armored bus (with rear/side exit ramp).
  - Operatives can re-enter the ARV at any time during the mission to equip remaining weapons/ammo from the vehicle.
  - **Extraction / KIA Rule**: Killing all aliens auto-extracts safely and recovers all ground items; manually aborting/retreating extracts operatives inside the bus and marks any operative left outside on the battlefield as **KIA** (`MIA (LEFT BEHIND)`) in the Memorial.
- [x] **ARV Defensive Firepower & Smart Alien AI**:
  - Researched/built ARV roof turret fires on aliens within range (with cooldown, accuracy, and range upgrades).
  - Aliens do not suicidally rush the bus; as fewer aliens remain they become more cautious (with a small random chance of blundering into range) and can shoot/disable the turret if abused.
- [x] **Ground Item Persistence, Corpse Looting & `[INV]` HUD Button**:
  - Weapons, ammo clips, and items dropped by dead operatives/aliens or by players persist on the ground with `.BMP` pixel-art sprites and are collected at mission victory.
  - Bottom HUD `[INV]` button inspects current operative equipment, ground items, or corpses on the current tile.
- [x] **Throw Weapon / Ammo Mechanic**:
  - Operatives can throw weapons, ammo, and grenades to other tiles/operatives using `throw_acc` (bad throws can scatter wildly; successful throws build XP).
- [x] **Bottom-Right Spotted Alien Head Indicators**:
  - Show 1..N clickable alien head icons in the bottom-right corner above the HUD for every alien the selected operative can see; clicking an indicator centers the camera on that alien.
- [x] **UFO Power Core Mini-Nuke**:
  - Shooting an intact `OBJ_UFO_CORE` multiple times has an escalating chance to trigger a massive radius 5–6 nuclear-style Elerium meltdown explosion that destroys most of the UFO and surrounding area.


---

## Phase 6: Home Base Defense Mission & Save/Load Game System
**Status:** *Completed*

- [x] **Home Base Defense Mission**:
  - Winning streaks slightly increase the rare chance of an alien retaliation attack on Duff-COM HQ.
  - Tactical map is an exact replica of the player's current Colonial House Base layout and surrounding yard; operatives start in the Living Quarters with access to all base stores.
  - If aliens win the Base Defense mission, display an **Alien Victory Game Over Screen** and reset to the main title screen.
- [x] **Save / Load Game System (`campaign_save_game` / `campaign_load_game` in `src/campaign.c` & `src/settings.c`)**:
  - Persist and reload full campaign state (`DUFFSAVE.SAV`) across game restarts or after a mission goes badly via the Options menu (`[SAVE GAME]` / `[LOAD GAME]`).


