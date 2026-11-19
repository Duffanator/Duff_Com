# art_src â€” source art and rebuild commands

`assets/*.BMP` are the files the game loads. They are **edited in place by the tools below** (not regenerated
from scratch), so the old `tools/gen_*.ps1` scripts must NOT be re-run or they will overwrite this art.

```
art_src/units/   high-res source sheets (4x2 grids, 8 facing directions, magenta #FF00FF background)
art_src/floors/  top-down seamless texture swatch sheets
art_src/build/   generated previews (safe to delete)
tools/art/       pack_sheet.ps1 (units/walls/objects), pack_floors.ps1 (iso floor tiles), shared C# helpers
```

Both tools quantize to the game's real 16-ramp x 16-shade palette (mirror of `vga_build_palette` in `src/vga.c`),
so lighting/shade tables keep working.

## Soldier (UNITS.BMP, sprite type 0 = UNIT_SPR_MALE_VISOR; rows = type*4 + pose: 0 stand, 1 kneel, 2/3 walk)
```powershell
$sc = 0.072258064516129   # common scale, printed by the stand run - reuse so all poses are the same size
powershell -File tools\art\pack_sheet.ps1 -Source art_src\units\soldier_male_visor_stand_raw.jpg -Cols 4 -Rows 2 -Outline -Fit 0.84 -InjectSheet assets\UNITS.BMP -InjectRow 0
powershell -File tools\art\pack_sheet.ps1 -Source art_src\units\soldier_male_visor_kneel_raw.jpg -Cols 4 -Rows 2 -Outline -ScaleOverride $sc -MirrorCells "5:3" -InjectSheet assets\UNITS.BMP -InjectRow 1
powershell -File tools\art\pack_sheet.ps1 -Source art_src\units\soldier_male_visor_walkA_raw.jpg -Cols 4 -Rows 2 -Outline -ScaleOverride $sc -InjectSheet assets\UNITS.BMP -InjectRow 2
powershell -File tools\art\pack_sheet.ps1 -Source art_src\units\soldier_male_visor_walkB_raw.jpg -Cols 4 -Rows 2 -Outline -ScaleOverride $sc -InjectSheet assets\UNITS.BMP -InjectRow 3
```

## Floors (FLOORS.BMP tiles 1-8)
```powershell
powershell -File tools\art\pack_floors.ps1 -Source art_src\floors\floors_raw.jpg -TileIdList "1,2,3,4,5,6,7,8" -Zoom 1.5 -Contrast 1.3 -Saturation 1.25 -Brightness 14 -Sheet assets\FLOORS.BMP
```

## Batch rebuild + safety check
`powershell -File tools\art\build_all.ps1` rebuilds every entry in `art_src/manifest.json` into `assets/*.BMP` and then
compares the result with git HEAD: it fails (exit 2) if any cell/tile NOT listed in the manifest changed.
`-Only <unit id>` rebuilds one unit, `-VerifyOnly` just runs the check. Missing `kneel`/`walkA`/`walkB` entries default to
the stand sheet (used for aliens, civilians and drones that don't need every pose).

## Generation queue (`art_src/jobs.json`)
39 pending image jobs, each with its prompt, mode (`fresh` or `edit` of a reference sheet), reference and output file:
- Phase 1: 5 remaining operatives (stand, kneel, walkA each; edits of the finished male-visor soldier)
- Phase 2: Sectoid, Floater, Muton, Cryptid, Zombie (stand + walkA) and the 2 drones (stand)
- Phase 3: 6 civilians (2 fresh bases + edits)
- Later phases (not yet queued): death animations (DEATH.BMP), walls, remaining floors 9-18, objects, items, portraits.
After a job's image exists, add the unit to `manifest.json` (`type` = sprite id from `gfx_assets.h`) and run `build_all.ps1`.

## Original X-COM art (local only)
`orig_assets/` (the ripped X-COM sheets) and `assets_xcom/` (derived overlay sheets) are **gitignored** so the public repo
never contains Take-Two's art; the game still runs on our own art without them.
`powershell -File tools\art\pack_xcom.ps1` reads `art_src/xcom_manifest.json` (cell coordinates on the 33x41-pitch X-COM
sheets, crop mode, target index), quantizes to the game palette and writes `assets_xcom/<SHEET>.BMP` (starting from the
committed `assets/<SHEET>.BMP`). `build_and_run.bat` copies `assets_xcom\*.BMP` over `dist\assets` when present.
Done so far: UFO objects (core, nav console, computer, cryo tank, pod, grav-lift), UFO deck floor, UFO doors (W+N, closed+open).
Still to do: UFO hull walls / damaged hull / corner fairings / roof caps, and the layered alien unit sheets (Sectoid, Floater, Muton, Chryssalid, Zombie).

### X-COM layered units (`tools\art\pack_xcom_units.ps1`)
Reads the `units` array in `art_src/xcom_manifest.json`. Each pose is a list of sheet rows stacked bottom->top (legs, arms, body+head);
8 columns = the engine's 8 directions (col 3 faces the camera, same numbering as `calc_dir_8`). The 32x40 composite is cropped to the
game's 24x32 cell (offset 4,6 because the engine draws units at tile+(4,-18) vs X-COM's tile+(0,-24)); `"scale": 0.8` shrinks tall units
(Floater, Muton) about the feet instead of cropping their heads. Writes `assets_xcom/UNITS.BMP` and `assets_xcom/DEATH.BMP` (gitignored).
Done: Sectoid (type 6), Floater (7), Muton (8), Chryssalid/Cryptid (15), Zombie (16; frame-grid sheet via the `grid` option). Zombie death still uses the old art (the X-COM sheet only has the Chryssalid hatching animation) - TODO: generate a matching zombie death sheet.


## Status (updated)
Done & built: all 6 operatives, 6 civilians (2 still use the stand sheet for walking: civ_male_work, civ_fem_blonde - walkA pending image quota),
both drones, zombie death animation; X-COM art for UFO objects/doors/deck floor and the 5 alien types.
`build_all.ps1` now also handles `"deaths"` entries (4-frame strips -> DEATH.BMP) and the manifest supports per-unit `cellMargin`
(ignore generator grid lines), `bottomMargin` (hover height) and `mirror.stand` (fix swapped front-3/4 cells).
Gotcha: when composing images in PowerShell/GDI+, never use DrawImageUnscaled with images of a different DPI (it silently rescales) - use DrawImage with an explicit size.
Remaining: death animations for operatives/civilians/drones, walls + objects + remaining floors for the PA locations, UFO hull walls, 2 civilian walk sheets.

## PA walls / objects / floors (done)
- `tools\art\pack_walls.ps1`: projects flat front-on wall panels (`art_src/walls/*.jpg`, 4x2 per sheet) into the engine's 16x32 West/North wall shapes
  (bottom edge slopes 0.5 px/px, height 24 = LEVEL_H; West walls are shaded 0.86x as a depth cue; magenta = see-through holes). Walls 1-16 are done.
- Objects (`art_src/props`): OBJECTS ids 1-5 (arcade cab, diner booth, jukebox, corn, street lamp), 8-10 (crate, stairs, rubble), 11-12 (flare, barrel).
  `pack_sheet.ps1 -InjectColList` maps sheet cells to the non-contiguous object ids.
- Floors 9 (scorch), 16-18 (ARV deck, ARV roof, drone pad) from `floors_C_raw.jpg`.
- Everything is in `manifest.json` (`walls`, `objects`, `floors`) and verified by `build_all.ps1`.
Still old art: 2-tile vehicles (OBJ 6, 13-27), UFO hull walls 17/18 and corner fairings (OBJ 28-31), UFO roof floors 11-15, ARV walls 21-26, FX/items/HUD/portraits.

## Vehicles (done)
`art_src/vehicles/vehicles_raw.jpg` (2x2: cruiser, tractor, sedan, pickup, all facing lower-right) -> `pack_sheet.ps1` (48x48 canvases, common scale)
-> `pack_vehicles.ps1` splits each along canvas x=24 into the engine's back/front tile sprites: +X pair (front drawn at back+(16,8)) and +Y pair
(mirrored car, front drawn at back+(-16,8)). Object ids per vehicle are in `art_src/vehicle_kinds.json`. Note the +Y versions are mirrored, so their light
comes from the top-right; regenerate a lower-left-facing sheet if that bothers you.
