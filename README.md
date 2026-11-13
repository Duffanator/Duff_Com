# DUFF-COM: Coopersburg Defense Force

**DUFF-COM** (*Lehigh Valley Defense Force*) is an original 32-bit Protected Mode MS-DOS isometric turn-based tactical strategy game inspired by *X-COM: UFO Defense* (1994), custom-built in C for an arcade cabinet with trackball and arcade button controls.

Command a fortified Colonial House compound in Coopersburg, Pennsylvania, track UFO incursions across a sweeping Lehigh Valley CRT radar, research alien technology and psionic warfare, manufacture experimental weaponry and autonomous combat drones, and deploy up to an 8-operative Armored Response Vehicle (ARV) strike team to procedurally generated Eastern Pennsylvania battlefields.

---

## Gameplay Overview

### 1. Lehigh Valley Command Center (HQ)
Between missions, manage the defense of the Lehigh Valley across three interactive HQ screens with a real-time campaign clock (`1S`, `5M`, `1H`, `1D` time compression):

- **Valley Radar (`1. RADAR`)**:
  - Sweeping CRT radar centered on Coopersburg, PA (spanning PA Route 309, I-78, I-476, Saucon Creek, and Lake Nockamixon) tracking **4 UFO Classes** scaled by radar blip size: **Small Scout**, **Medium Harvester**, **Large Abductor**, and **2-Story Command Dreadnought**.
  - **Active Airspace & Interception**: Entering UFOs may land, get shot down by regional X-COM forces, launch Terror Raids, assault Coopersburg HQ directly, or fly across the valley and escape airspace unless intercepted by HQ's **Base Offense System** interactive turret mini-game.
  - **Clock-Driven Day/Night Deployment**: Deploying the Armored Response Bus uses the live campaign clock (`DAYLIGHT MISSION` vs. `NIGHT MISSION (DARK)`), allowing you to deploy immediately or stand down and advance the clock.
  - **Priority Dispatch Modals**: Animated `MODAL.BMP` popup alerts automatically pause time compression (`1S`) and sound an alarm whenever research completes, workshop runs finish, store shipments arrive, psi-training concludes, or UFO contacts appear.

- **Coopersburg Colonial House Base (`2. BASE`)**:
  - Expand a connected `3x4` compound starting with the **Colonial House HQ**, **ARV Bus Garage**, **Homemade Radar**, and **Homemade Lab/Workshop**. Build connected yard facilities including the **Storage Shed**, **Living Quarters**, **Upgraded Lab**, **Upgraded Workshop**, **Alien Containment**, **Missile Defense**, **Base Offense System**, and **Psionic Amplifier Lab (`PSI-LAB`)**.
  - **22-Topic Research Tree & Scientist Allocation**: Assign individual scientists (`[-]` / `[+]`) across multiple active research projects simultaneously with `0%–100%` completion tracking. Research Laser and Plasma weapon hierarchies, Field Trauma Medkits, ARV Roof Turrets, Recon & Plasma Drones, Alien Autopsies, Live Alien Interrogations, and Psionic Warfare, and review unlocked lore entries in the **Research Log (`RES.LOG`)**.
  - **Time-Based Workshop Manufacturing**: Assign engineers to build researched weapons, ammunition, armor, ARV turrets, and drones over per-item build hours using funds, **Alien Alloys**, and **Elerium-115**.
  - **Supply Store, Transit Times & Salvage Economy**: Purchase standard equipment and ammunition with realistic transit delivery timers (`IN TRANSIT`), hire Operatives, Scientists, and Engineers up to Living Quarters capacity, and sell recovered **Alien Corpses**, **Captured Live Aliens**, and UFO artifacts for funding.

- **Squad Armory & Roster (`3. ARMOR`)**:
  - Manage up to **16 Operatives** across Living Quarters and assign up to **8 Operatives** to the **Armored Response Vehicle (ARV)** strike team, keep reserves at HQ, or enroll soldiers in **Psionic Training** (`72h` course in the Psi-Lab to reveal hidden **Psionic Skill (`PSI`)**).
  - Inspect operative ranks (**Rookie** → **Squaddie** → **Sergeant** → **Captain** → **Colonel** → **Commander**), combat stats (`TU`, `HP`, `STM`, `AIM`, `THR`, `RCN`, `BRV`, `PSI`), paper-doll equipment loadouts, and honor fallen heroes on the **KIA Memorial Wall**.

### 2. Isometric Tactical Battlescape
Deploy the Armored Response Bus into multi-elevation (`3`-story) procedurally generated isometric combat zones—standard `24x24` sectors for Scouts and Harvesters, or **`2x` larger `48x48` battlefields** for Large Abductors and **2-Story Command Dreadnoughts**—across **6 Pennsylvania combat biomes**:
- **Coopersburg Route 309 Diner & Duff's Arcade**: Chrome & neon diner booths, Wurlitzer jukebox, asphalt highway lanes, and glowing 90s arcade cabinets.
- **PA Farmland & Red Barn**: Two-story wooden haylofts, farm tractors, perimeter fencing, and tall flammable cornfields that block line-of-sight.
- **Coopersburg Suburbs & Arcade Basement**: Neighborhood homes, porches, backyards, and residential arcade rec rooms.
- **Bethlehem Steel Industrial Ruins**: Blast-furnace catwalks, heavy steel bulkheads, slag yards, and volatile fuel drums.
- **Lake Nockamixon Forest Camp**: Dense pine woods, campfires, ranger cabins, and narrow forest trails.
- **Coopersburg HQ Base Defense**: Defend your own Colonial House compound and yard facilities against direct alien retaliation strikes.

#### Tactical Mechanics
- **Paper-Doll Loadout & Inventory (`INV`)**: Pre-mission and in-battle drag-and-drop inventory featuring Right/Left Hand slots, 4 Small Belt slots, 2 Large Belt holsters, `3x2` Backpack, weight encumbrance, weapon reloading (`10–20 TU` via `[RLD]` or drag-and-drop), and grenade fuse priming (`0–6` turns, `50%` throw TU cost).
- **ARV Roof Defense Turret & Autonomous Drones**:
  - Install a rotating **8-directional ARV Roof Turret** (Mk1 Laser / Mk2 Plasma) on the rear of the Armored Bus that automatically rotates to face and fire on approaching aliens each turn.
  - Deploy an autonomous hovering **ARV Quad-Copter Rifle Drone** or **Alloy UFO Plasma Drone** from the front roof landing pad.
- **Field Trauma Medkits, Unconsciousness & Bleed-Out**:
  - Wounded operatives can fall unconscious (`UNCON`) instead of dying immediately and bleed out over `3–5` turns unless revived/stabilized with a **Field Trauma Medkit** or evacuated by winning the mission. If the squad is wiped out, all carried equipment and weapons are lost.
- **Squad Morale & Mental Breakdowns**:
  - Rapid squad or civilian casualties erode operative morale (resisted by **Bravery**), risking temporary `1–2` turn **Mental Breakdowns** (Panicked Flight, Weapon Drop, or Berserk fire).
- **Progressive Alien Hierarchy, Live Capture & Psionic Warfare**:
  - **Sectoids**: Early-game aliens ranked from Trooper, Technician, and Leader up to **Sectoid Commander** (found aboard 2-story Command Dreadnoughts, camping inside the UFO and using Psionic Mind Control to puppet DUFF-COM operatives for `2` turns).
  - **Floaters**: Mid-game hovering assault units with higher hit points.
  - **Mutons**: Late-game heavy armored shock troops.
  - **Cryptids & Zombies**: Fast, terrifying late-game bio-weapons (up to 2 per mission) that infect operatives and civilians in melee, transforming them into hostile **Zombies**.
  - **Stunning & Live Capture**: Subdue aliens via wound shock, **High-Voltage Stun Rods**, or **Concussion Stun Grenades** to secure live specimens for **Alien Containment** and interrogation.
  - **Psionic Mind Probe**: Psi-trained operatives can wield the **Psionic Mind Probe** to mind-control enemy aliens on the battlefield.
- **Sub-Voxel Raycast Ballistics & Dynamic Lighting**: High-precision sub-tile raycast gunfire and explosions shatter windows, breach walls without clipping through cover, detonate volatile barrels, spread smoke and fire, and dynamically recalculate 16-level per-tile lighting and fog of war.

---

## Technical Highlights (MS-DOS)

- **CPU / Target**: 32-bit Protected Mode MS-DOS (`DJGPP` C99 + DPMI).
- **Graphics**: Double-buffered VGA **Mode 13h** (`320x200`, 256 colors at 70 Hz) with dual bitmap fonts (`5x7` primary font and compact `3x5` UI font) and 16-level palette shade tables for real-time day/night lighting, streetlamps, and muzzle flashes.
- **Digital Sound Effects**: Direct **Sound Blaster DSP + ISA DMA** audio engine (`11,025 Hz` `.WAV` playback with procedural synthesis & PC Speaker fallbacks).
- **FM Music**: Real-time 9-channel hardware **AdLib / Sound Blaster OPL2 (`YM3812`)** FM synthesizer driven on the 70 Hz vertical retrace—featuring a 90s industrial metal title track and an atmospheric sci-fi tactical soundtrack.
- **Arcade Input**: `INT 33h` trackball/mouse driver with dynamic lock-on reticles, edge-of-screen camera panning, drag-and-drop inventory, and full arcade button / keyboard bindings.

---

## Controls

### Command Center (HQ)
| Input | Action |
| :--- | :--- |
| **Trackball / Mouse + Left Click** | Switch HQ tabs, set time compression (`1S`/`5M`/`1H`/`1D`), select radar contacts, manage base/research/workshop/store, assign ARV operatives |
| **`1` / `2` / `3` or `Tab`** | Switch between Valley Radar, Colonial Base, and Squad Armory |
| **`Arrow Keys`** | Cycle active radar UFO contacts or scroll lists |
| **`S` / `Space`** | Advance radar scan / acknowledge Priority Dispatch modal |
| **`Enter`** | Open ARV launch confirmation for selected radar contact or acknowledge modal |
| **`Esc` / `O`** | Open Options / Save & Load / Quit to DOS menu |

### Tactical Battlescape
| Input | Action |
| :--- | :--- |
| **Trackball / Mouse** | Aim cursor / move to screen edge to pan camera |
| **Left Click** | Walk to tile, interact, click HUD buttons, drag inventory items, or fire/use selected item |
| **Right Click** | Turn operative toward cursor, open/close adjacent doors, cancel fire mode, or quick-unequip in inventory |
| **`I`** | Open / close Paper-Doll Tactical Inventory & ARV Loadout (`INV`) |
| **`1` – `5` or `F`** | Select action/firing mode (`1` Snap, `2` Aimed, `3` Auto/Special, `4` Belt Grenade, `5` Belt Item) or toggle weapon menu (`F`) |
| **`Tab` / `Space`** | Select next conscious operative (`NXT`) |
| **`K`** | Toggle Standing / Kneeling stance (`KNL`) |
| **`R`** | Toggle building roof / upper-floor cutaway (`ROF`) |
| **`PgUp` / `U` & `PgDn` / `D`** | Change camera view elevation level (`UP` / `DN`) |
| **`C`** | Center camera on active operative |
| **`Arrow Keys`** | Pan tactical camera |
| **`N`** | Toggle Day / Night lighting preview (`NGT`) |
| **`Enter`** | End DUFF-COM turn (`END`) |
| **`M`** | Abort mission / Evacuate operatives in ARV (`ABT`) |
| **`P`** | Save `320x200` screenshot to `SNAP.BMP` |
| **`Esc` / `O`** | Open in-game Options menu (`OPT`) |

---

## Customization

- **[`ROSTER.CFG`](ROSTER.CFG)**: Edit `ROSTER.CFG` to customize your starting squad's names (all operatives begin as Rookies and earn rank prefixes in combat). Optional `M:` or `F:` prefixes (e.g., `F: DECKER`) lock an operative's gender and portrait pool.
- **[`SETTINGS.CFG`](SETTINGS.CFG)**: Stores persistent preferences (also editable from the in-game `OPT` menu) for animation speed (`ANIM_SPEED`), OPL2 music volume (`MUSIC_VOL`), Sound Blaster SFX (`SFX`), and trackball edge-scroll speed (`SCROLL_SPEED`).

---

## Building & Running

### Prerequisites
- **[DJGPP](https://github.com/andrewwutw/build-djgpp)** (`i586-pc-msdosdjgpp-gcc`) and **[DOSBox-X](https://dosbox-x.com/)** installed under `%USERPROFILE%\.dos` (or `.\.dos` in the project root — automatically installed if using the VS Code `dos-dev` toolchain).

### Build & Launch (Windows)
Run the included batch script to compile `dist/DUFFCOM.EXE`, stage game assets/configs, and launch directly in DOSBox-X:
```bat
build_and_run.bat
```
Or open the folder in **Visual Studio Code** and press **`F5`** (*DOS Build & Run (DUFF-COM)*).

### Regenerating Art & Audio Assets *(Optional)*
All game sprites and Sound Blaster `.WAV` effects are already included in `assets/`. To regenerate them from source scripts on Windows PowerShell:
```powershell
powershell -ExecutionPolicy Bypass -File tools\gen_sprites.ps1
powershell -ExecutionPolicy Bypass -File tools\gen_items.ps1
powershell -ExecutionPolicy Bypass -File tools\gen_base.ps1
powershell -ExecutionPolicy Bypass -File tools\gen_portraits.ps1
powershell -ExecutionPolicy Bypass -File tools\gen_new_assets.ps1
powershell -ExecutionPolicy Bypass -File tools\gen_sfx.ps1
```

---

## Project Layout

```text
Duff_Com/
├── assets/               # 24-bit BMP sprite sheets/artwork & 11,025 Hz WAV audio
├── src/                  # C99 MS-DOS engine & gameplay source code
│   ├── main.c            # Game loop, title screen & arcade input dispatch
│   ├── campaign.c/.h     # Lehigh Valley CRT radar, clock, UFO interception & modals
│   ├── base_ui.c/.h      # Colonial House base builder, research, workshop, store & roster
│   ├── items.c/.h        # Item database, 22-topic research tree, manufacturing & loadouts
│   ├── operative.c/.h    # X-COM rank progression, XP, morale, psi stats & recruit pool
│   ├── isomap.c/.h       # Isometric renderer, sub-voxel raycast combat, LOS, lighting & AI
│   ├── procgen.c/.h      # Procedural generator for all 6 biomes & 4 multi-story UFO classes
│   ├── pathfind.c/.h     # 3D multi-elevation A* pathfinding
│   ├── hud.c/.h          # Tactical HUD, paper-doll inventory & beginning-of-turn splash
│   ├── gfx_assets.c/.h   # Sprite sheet slicing & palette remapping
│   ├── bmp_loader.c/.h   # Uncompressed BMP loader & color-ramp quantizer
│   ├── vga.c/.h          # Mode 13h VGA driver, 5x7 & 3x5 fonts, primitives & shade tables
│   ├── sound.c/.h        # Sound Blaster DSP + ISA DMA digital audio driver
│   ├── music.c/.h        # AdLib / OPL2 (YM3812) 9-channel FM music engine
│   ├── input.c/.h        # INT 33h mouse/trackball & BIOS keyboard polling
│   └── settings.c/.h     # In-game options modal, campaign save/load & SETTINGS.CFG
├── tools/                # PowerShell/C# procedural sprite & SFX asset generators
├── ROSTER.CFG            # Customizable starting squad roster
├── SETTINGS.CFG          # Default audio, animation & scroll speed preferences
├── dosbox-duffcom.conf   # DOSBox-X hardware & display configuration
├── build_and_run.bat     # One-step DJGPP release build & DOSBox-X launcher
└── CMakeLists.txt        # CMake build definition
```
