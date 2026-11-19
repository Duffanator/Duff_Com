<#
.SYNOPSIS
  Builds game unit sprites (UNITS.BMP rows + DEATH.BMP cells) from the LAYERED original X-COM unit sheets.

.DESCRIPTION
  X-COM unit sheets store body parts (legs, arms, body+head) as separate 32x40 cells: 8 columns = 8 directions,
  one sheet row per part/frame.  For every game pose we stack a list of sheet rows (bottom -> top) per direction,
  crop the 32x40 composite to the game's 24x32 cell, quantize to the game palette and paste it into
  assets_xcom/UNITS.BMP (starting from the committed assets/UNITS.BMP).  Reads the "units" array of art_src/xcom_manifest.json.

  Geometry: the engine draws a unit cell at (tile_x+4, tile_y-18); X-COM draws its 32x40 cell at (tile_x, tile_y-24).
  So game cell (gx,gy) = X-COM (gx+4, gy+6) by default (cropX=4, cropY=6).  Death cells (32x32) drawn at tile_y-16 -> cropY 8.
  Cell origin on the sheet = (col*33, row*41).
#>
param(
  [string]$Manifest = "art_src\xcom_manifest.json",
  [string]$Only = "",
  [string]$PreviewDir = "art_src\build"
)
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs')
$m = Get-Content $Manifest -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force assets_xcom, $PreviewDir | Out-Null

function Load-Bmp([string]$path) {
  $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($path))
  return [System.Drawing.Bitmap]::FromStream($ms)
}
function Is-Key($c) { return ($c.A -lt 128) -or ($c.R -gt 240 -and $c.G -lt 15 -and $c.B -gt 240) }
function Clone24($bmp) { $c = $bmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb); $bmp.Dispose(); return $c }

# start from the previous overlay if it exists (so several runs/units accumulate), else from the committed sheets
function Open-Sheet([string]$name) {
  $ov = "assets_xcom\$name.BMP"
  if (Test-Path $ov) { return Clone24 (Load-Bmp $ov) }
  return Clone24 (Load-Bmp "assets\$name.BMP")
}
$units = Open-Sheet 'UNITS'
$death = Open-Sheet 'DEATH'
$cache = @{}
$report = @()

# Composite one 32x40 X-COM frame for direction $d from stacked sheet rows -> int[40*32] of colours (ARGB, 0 = transparent)
function Composite($src, [int[]]$rows, [int]$d) {
  $buf = New-Object 'int[]' (32 * 40)
  foreach ($r in $rows) {
    $sx = $d * 33; $sy = $r * 41
    for ($y = 0; $y -lt 40; $y++) { for ($x = 0; $x -lt 32; $x++) {
      $c = $src.GetPixel($sx + $x, $sy + $y)
      if (-not (Is-Key $c)) { $buf[$y * 32 + $x] = $c.ToArgb() }
    } }
  }
  return $buf
}
function Paste($sheet, $buf, [int]$cropX, [int]$cropY, [int]$w, [int]$h, [int]$dx, [int]$dy, [ref]$lost) {
  # pixels of the 32x40 frame outside the crop window are counted as lost
  for ($y = 0; $y -lt 40; $y++) { for ($x = 0; $x -lt 32; $x++) {
    if ($buf[$y * 32 + $x] -ne 0 -and ($x -lt $cropX -or $x -ge $cropX + $w -or $y -lt $cropY -or $y -ge $cropY + $h)) { $lost.Value++ }
  } }
  for ($y = 0; $y -lt $h; $y++) { for ($x = 0; $x -lt $w; $x++) {
    $v = $buf[($cropY + $y) * 32 + ($cropX + $x)]
    if ($v -eq 0) { $col = [System.Drawing.Color]::FromArgb(255, 0, 255) }
    else { $c = [System.Drawing.Color]::FromArgb($v); $col = [GamePalette]::ToColor([GamePalette]::Nearest($c.R, $c.G, $c.B)) }
    $sheet.SetPixel($dx + $x, $dy + $y, $col)
  } }
}

# Frame-grid sheets (e.g. Zombie, ripped as sequential frames on a coloured background): frame f -> 32x40 buffer
function Composite-Grid($src, $grid, [int]$frame) {
  $buf = New-Object 'int[]' (32 * 40)
  $col = $frame % [int]$grid.cols; $row = [math]::Floor($frame / [int]$grid.cols)
  $sx = [int]$grid.ox + $col * [int]$grid.px; $sy = [int]$grid.oy + $row * [int]$grid.py
  $bg = $grid.bg; $tol = [int]$grid.tol
  for ($y = 0; $y -lt 40; $y++) { for ($x = 0; $x -lt 32; $x++) {
    $c = $src.GetPixel($sx + $x, $sy + $y)
    $dist = [math]::Abs($c.R - [int]$bg[0]) + [math]::Abs($c.G - [int]$bg[1]) + [math]::Abs($c.B - [int]$bg[2])
    if ($dist -gt $tol) { $buf[$y * 32 + $x] = $c.ToArgb() }
  } }
  return $buf
}
# Scaled paste: area-sampled shrink of the 32x40 frame about the feet anchor (src 16,36 -> game 12,30), for tall aliens
function Paste-Scaled($sheet, $buf, [double]$sc, [int]$dx, [int]$dy) {
  $SS = 4
  for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 24; $x++) {
    $n = 0; $r = 0.0; $g = 0.0; $b = 0.0
    for ($j = 0; $j -lt $SS; $j++) { for ($i = 0; $i -lt $SS; $i++) {
      $sx = [int][math]::Floor(16 + (($x + ($i + 0.5) / $SS) - 12) / $sc)
      $sy = [int][math]::Floor(36 + (($y + ($j + 0.5) / $SS) - 30) / $sc)
      if ($sx -lt 0 -or $sx -ge 32 -or $sy -lt 0 -or $sy -ge 40) { continue }
      $v = $buf[$sy * 32 + $sx]
      if ($v -ne 0) { $c = [System.Drawing.Color]::FromArgb($v); $n++; $r += $c.R; $g += $c.G; $b += $c.B }
    } }
    if ($n -ge ($SS * $SS / 2)) { $col = [GamePalette]::ToColor([GamePalette]::Nearest($r / $n, $g / $n, $b / $n)) }
    else { $col = [System.Drawing.Color]::FromArgb(255, 0, 255) }
    $sheet.SetPixel($dx + $x, $dy + $y, $col)
  } }
}
foreach ($u in $m.units) {
  if ($Only -and $u.name -ne $Only) { continue }
  if (-not $cache.ContainsKey($u.src)) { $cache[$u.src] = Load-Bmp (Join-Path 'orig_assets' $u.src) }
  $src = $cache[$u.src]
  $cx = if ($null -ne $u.cropX) { [int]$u.cropX } else { 4 }
  $cy = if ($null -ne $u.cropY) { [int]$u.cropY } else { 6 }
  $lost = 0
  foreach ($p in $u.poses) {                      # pose index 0..3 -> sheet row type*4+pose
    for ($d = 0; $d -lt 8; $d++) {
      if ($u.grid) { $buf = Composite-Grid $src $u.grid ([int]$p.frameBase + $d) } else { $buf = Composite $src ([int[]]$p.parts) $d }
      if ($u.scale -and [double]$u.scale -ne 1.0) { Paste-Scaled $units $buf ([double]$u.scale) ($d * 24) (([int]$u.type * 4 + [int]$p.pose) * 32) }
      else { Paste $units $buf $cx $cy 24 32 ($d * 24) (([int]$u.type * 4 + [int]$p.pose) * 32) ([ref]$lost) }
    }
  }
  if ($u.death -and -not $u.grid) {                                 # 4 death stages from listed cells of one sheet row
    $dcy = if ($null -ne $u.death.cropY) { [int]$u.death.cropY } else { 8 }
    for ($stage = 0; $stage -lt 4; $stage++) {
      $col = [int]$u.death.cols[[math]::Min($stage, $u.death.cols.Count - 1)]
      $buf = New-Object 'int[]' (32 * 40)
      $sx = $col * 33; $sy = [int]$u.death.row * 41
      for ($y = 0; $y -lt 40; $y++) { for ($x = 0; $x -lt 32; $x++) { $c = $src.GetPixel($sx + $x, $sy + $y); if (-not (Is-Key $c)) { $buf[$y * 32 + $x] = $c.ToArgb() } } }
      $dl = 0
      Paste $death $buf 0 $dcy 32 32 ($stage * 32) ([int]$u.type * 32) ([ref]$dl)
      $lost += $dl
    }
  }
  $report += ("{0,-10} type {1,2}: done, {2} source pixels fell outside the crop window" -f $u.name, $u.type, $lost)
}
$units.Save((Join-Path (Get-Location) 'assets_xcom\UNITS.BMP'), [System.Drawing.Imaging.ImageFormat]::Bmp)
$death.Save((Join-Path (Get-Location) 'assets_xcom\DEATH.BMP'), [System.Drawing.Imaging.ImageFormat]::Bmp)
$report | ForEach-Object { Write-Host $_ }


