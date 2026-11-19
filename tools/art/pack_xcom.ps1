<#
.SYNOPSIS
  Imports sprites from the ORIGINAL X-COM sheets in orig_assets/ into overlay copies of the game sheets.

.DESCRIPTION
  Reads art_src/xcom_manifest.json. For every target sheet (OBJECTS, FLOORS, WALLS_W, WALLS_N, ...) it starts from
  the committed assets/<TARGET>.BMP, pastes each listed X-COM cell (palette-quantized to the game palette) and writes
  assets_xcom/<TARGET>.BMP.  assets_xcom/ and orig_assets/ are gitignored (copyrighted art): the repo stays
  clean and build_and_run.bat copies assets_xcom\*.* over assets\ when it exists.

  X-COM sheets use a 33x41 pitch (32x40 cells + 1px gap).  Cell origin = (ox + col*33, oy + row*41).
  crop modes:  full  = whole 32x40 cell (objects)
               floor = bottom 16 rows (the iso diamond) -> 32x16
               wallW = left 16 columns, top 32 rows -> 16x32
               wallN = right 16 columns, top 32 rows -> 16x32
#>
param(
  [string]$Manifest = "art_src\xcom_manifest.json",
  [string]$PreviewDir = "art_src\build"
)
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs')
$m = Get-Content $Manifest -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force assets_xcom | Out-Null

function Load-Bmp([string]$path) {
  $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($path))
  return [System.Drawing.Bitmap]::FromStream($ms)
}
$srcCache = @{}
function Get-Src([string]$name) {
  if (-not $srcCache.ContainsKey($name)) { $srcCache[$name] = Load-Bmp (Join-Path 'orig_assets' $name) }
  return $srcCache[$name]
}
function Is-Key($c) { return ($c.A -lt 128) -or ($c.R -gt 240 -and $c.G -lt 15 -and $c.B -gt 240) }

foreach ($t in $m.targets) {
  $tmp = Load-Bmp (Join-Path 'assets' ($t.name + '.BMP'))
  $sheet = $tmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $tmp.Width, $tmp.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $tmp.Dispose()
  foreach ($it in $t.items) {
    $src = Get-Src $it.src
    $cx = [int]$it.ox + [int]$it.col * 33; $cy = [int]$it.oy + [int]$it.row * 41
    switch ($it.crop) {
      'full'  { $sx=$cx;      $sy=$cy;      $w=32; $h=40 }
      'floor' { $sx=$cx;      $sy=$cy + 24; $w=32; $h=16 }
      'wallW' { $sx=$cx;      $sy=$cy;      $w=16; $h=32 }
      'wallN' { $sx=$cx + 16; $sy=$cy;      $w=16; $h=32 }
      default { throw "unknown crop '$($it.crop)'" }
    }
    $dx = [int]$it.to * $w
    if ($dx + $w -gt $sheet.Width) { throw "$($t.name): index $($it.to) is outside the sheet" }
    for ($y = 0; $y -lt $h; $y++) { for ($x = 0; $x -lt $w; $x++) {
      $c = $src.GetPixel($sx + $x, $sy + $y)
      if (Is-Key $c) { $out = [System.Drawing.Color]::FromArgb(255, 0, 255) }
      else { $out = [GamePalette]::ToColor([GamePalette]::Nearest($c.R, $c.G, $c.B)) }
      $sheet.SetPixel($dx + $x, $y, $out)
    } }
  }
  $sheet.Save((Join-Path (Get-Location) ("assets_xcom\" + $t.name + ".BMP")), [System.Drawing.Imaging.ImageFormat]::Bmp)
  $sheet.Dispose()
  Write-Host ("{0}: {1} cells -> assets_xcom\{0}.BMP" -f $t.name, @($t.items).Count)
}
