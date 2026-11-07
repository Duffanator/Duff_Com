# DUFF-COM: Coopersburg Defense Force

**DUFF-COM** (*Lehigh Valley Defense Force*) is an original 32-bit Protected Mode MS-DOS isometric turn-based tactical strategy game inspired by *X-COM: UFO Defense* (1994), custom-built in C for an arcade cabinet with trackball and arcade button controls.

Command an underground defense bunker beneath Coopersburg, Pennsylvania, track UFO incursions across a sweeping Lehigh Valley CRT radar, upgrade your squad's weaponry with salvaged alien technology, and deploy a 5-operative Skyranger strike team to procedurally generated Pennsylvania battlefields.

---

## Gameplay Overview

### 1. Lehigh Valley Command Center (HQ)
Between missions, manage the defense of Eastern Pennsylvania across three interactive HQ screens:
- **Valley Radar (`1. VALLEY RADAR`)**: Full-width sweeping CRT radar centered on Coopersburg, PA (spanning PA Route 309, I-78, I-476, Saucon Creek, and Lake Nockamixon). Monitor real-time Crash Sites, Landed UFOs, and Alien Terror Raids across 16 real-world Lehigh Valley locations with a live campaign clock and Day/Night mission deployment.
- **Coopersburg Underground Bunker (`2. BUNKER BASE`)**: Expand a `3x4` subterranean base grid using recovered **Alien Alloys** and **Elerium-115**. Facilities include the Access Lift, Skyranger Hangar, Radar Array, Duff's Underground Arcade Rec Room, Laser Lab, Medbay (`+HP` squad bonus), Plasma Lab, and Defense Turrets.
- **Squad Armory (`3. SQUAD ARMORY`)**: Inspect your 5 operatives, track individual kill counts, cycle operative portraits/genders, and equip **Assault Rifles**, **Coopersburg Prototype Laser Rifles**, and **Heavy Elerium Plasma Rifles**.

### 2. Isometric Tactical Battlescape
Deploy into `24x24` multi-elevation (`3`-story) procedurally generated isometric combat zones across **4 local Pennsylvania biomes**:
- **Coopersburg Route 309 Diner & Duff's Arcade**: Chrome & neon diner booths, Wurlitzer jukebox, asphalt highway lanes, and glowing 90s arcade cabinets.
- **Quakertown Farmland & Red Barn**: Two-story wooden haylofts, farm tractors, perimeter fencing, and tall flammable cornfields that block line-of-sight.
- **Coopersburg Suburbs & Arcade Basement**: Neighborhood homes, porches, backyards, and residential arcade rec rooms.
- **Bethlehem Steel Industrial Ruins**: Blast-furnace catwalks, heavy steel bulkheads, slag yards, and volatile fuel drums.

#### Tactical Mechanics
- **Action Point System**: Manage Time Units (`TU`), Health (`HP`), and Energy (`EN`) across standing, kneeling (`+15%` accuracy), multi-floor stair climbing, and door interactions.
- **Combat & Munitions**: Choose between **Snap**, **Aimed**, and **Auto** fire modes, lob **High-Explosive Grenades**, or throw **Night Electro-Flares** to illuminate dark sectors.
- **Reaction Fire**: Reserve TUs for Snap or Aimed overwatch shots to automatically intercept **Sectoids**, **Floaters**, and **Mutons** moving into squad line-of-sight during the *Hidden Movement* alien phase.
- **Destructible Environments & Dynamic Lighting**: Raycast gunfire and explosions shatter diner/brick windows, breach walls into rubble, detonate explosive barrels, ignite spreading smoke and fire, and dynamically recalculate 16-level per-tile lighting and fog of war.

---

## Technical Highlights (MS-DOS)

- **CPU / Target**: 32-bit Protected Mode MS-DOS (`DJGPP` C99 + DPMI).
- **Graphics**: Double-buffered VGA **Mode 13h** (`320x200`, 256 colors at 70 Hz) featuring 16-level palette shade tables for real-time day/night lighting, streetlamps, and muzzle flashes.
- **Digital Sound Effects**: Direct **Sound Blaster DSP + ISA DMA** audio engine (`11,025 Hz` `.WAV` playback with procedural synthesis & PC Speaker fallbacks).
- **FM Music**: Real-time 9-channel hardware **AdLib / Sound Blaster OPL2 (`YM3812`)** FM synthesizer driven on the 70 Hz vertical retrace—featuring a 90s industrial metal title track and an atmospheric sci-fi tactical soundtrack.
- **Arcade Input**: `INT 33h` trackball/mouse driver with dynamic lock-on reticles, edge-of-screen camera panning, and full arcade button / keyboard bindings.

---

## Controls

### Command Center (HQ)
| Input | Action |
| :--- | :--- |
| **Trackball / Mouse + Left Click** | Switch HQ tabs, select pulsing radar UFO contacts, build bunker rooms, equip weapons |
| **`1` / `2` / `3` or `Tab`** | Switch between Radar, Bunker Base, and Squad Armory |
| **`Arrow Keys`** | Cycle active radar UFO contacts |
| **`S` / `Space`** | Fast-scan Lehigh Valley radar for alien contacts |
| **`Enter`** | Open Skyranger launch confirmation for selected radar contact |
| **`T`** *(in Launch Prompt)* | Toggle Day / Night mission deployment |
| **`Esc` / `O`** | Open Options / Quit to DOS menu |

### Tactical Battlescape
| Input | Action |
| :--- | :--- |
| **Trackball / Mouse** | Aim cursor / move to screen edge to pan camera |
| **Left Click** | Walk to tile, interact, click HUD buttons, or fire selected weapon |
| **Right Click** | Turn operative toward cursor, open/close adjacent doors, or cancel fire mode |
| **`1` – `5` or `F`** | Select firing mode (`1` Snap, `2` Aimed, `3` Auto, `4` HE Grenade, `5` Flare) or toggle weapon menu (`F`) |
| **`Tab` / `Space`** | Select next operative (`NXT`) |
| **`K`** | Toggle Standing / Kneeling stance (`KNL`) |
| **`R`** | Toggle building roof / upper-floor cutaway (`ROF`) |
| **`PgUp` / `U` & `PgDn` / `D`** | Change camera view elevation level (`UP` / `DN`) |
| **`C`** | Center camera on active operative |
| **`Arrow Keys`** | Pan tactical camera |
| **`N`** | Toggle Day / Night lighting (`NGT`) |
| **`Enter`** | End DUFF-COM turn (`END`) |
| **`M`** | Return to Lehigh Valley HQ |
| **`P`** | Save `320x200` screenshot to `SNAP.BMP` |
| **`Esc` / `O`** | Open in-game Options menu (`OPT`) |

---

## Customization

- **[`ROSTER.CFG`](ROSTER.CFG)**: Edit the 5 lines in `ROSTER.CFG` to customize your starting squad's names. Optional `M:` or `F:` prefixes (e.g., `F: SGT. DECKER`) lock an operative's gender and portrait pool.
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
powershell -ExecutionPolicy Bypass -File tools\gen_sfx.ps1
```

---

## Project Layout

```text
Duff_Com/
├── assets/               # 24-bit BMP sprite sheets/artwork & 11,025 Hz WAV audio
├── src/                  # C99 MS-DOS engine & gameplay source code
│   ├── main.c            # Game loop, title screen & arcade input dispatch
│   ├── campaign.c/.h     # Lehigh Valley CRT radar, bunker base & armory
│   ├── isomap.c/.h       # Isometric renderer, raycast combat, LOS, lighting & AI
│   ├── procgen.c/.h      # Procedural generator for all 4 PA combat biomes
│   ├── pathfind.c/.h     # 3D multi-elevation A* pathfinding
│   ├── hud.c/.h          # Tactical control bar, stat gauges & popup weapon menu
│   ├── gfx_assets.c/.h   # Sprite sheet slicing & palette remapping
│   ├── bmp_loader.c/.h   # Uncompressed BMP loader & color-ramp quantizer
│   ├── vga.c/.h          # Mode 13h VGA driver, primitives & light shade tables
│   ├── sound.c/.h        # Sound Blaster DSP + ISA DMA digital audio driver
│   ├── music.c/.h        # AdLib / OPL2 (YM3812) 9-channel FM music engine
│   ├── input.c/.h        # INT 33h mouse/trackball & BIOS keyboard polling
│   └── settings.c/.h     # In-game options modal & SETTINGS.CFG persistence
├── tools/                # PowerShell/C# procedural sprite & SFX asset generators
├── ROSTER.CFG            # Customizable 5-soldier starting squad roster
├── SETTINGS.CFG          # Default audio, animation & scroll speed preferences
├── dosbox-duffcom.conf   # DOSBox-X hardware & display configuration
├── build_and_run.bat     # One-step DJGPP release build & DOSBox-X launcher
└── CMakeLists.txt        # CMake build definition
```
