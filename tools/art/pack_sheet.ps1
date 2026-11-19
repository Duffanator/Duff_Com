<#
.SYNOPSIS
  Converts a high-res source image (grid of sprites on a magenta background) into
  game-ready, palette-exact pixel sprites, and optionally pastes them into a game sheet.

.DESCRIPTION
  1. Slices -Source into -Cols x -Rows cells (reading order = cell index 0..N-1).
  2. Keys out the magenta (#FF00FF) background / transparency.
  3. Scales ALL cells by one common factor (so relative size is consistent), anchors the feet
     to the bottom of the cell and centers horizontally.
  4. Area-averages down, applies optional contrast/saturation boost, and quantizes to the
     game's real 16-ramp x 16-shade palette (mirrors src/vga.c vga_build_palette) with optional
     ordered dithering and a 1px outline.
  5. Writes a cell strip BMP (magenta = transparent) + an upscaled preview PNG.
  6. With -InjectSheet, pastes cell k into cell (InjectCol + k, InjectRow) of an existing sheet BMP.

.EXAMPLE
  powershell -File tools\art\pack_sheet.ps1 -Source art_src\units\soldier_stand_raw.png -Cols 4 -Rows 2 `
     -CellW 24 -CellH 32 -OutBmp art_src\build\soldier_stand.bmp -Outline `
     -InjectSheet assets\UNITS.BMP -InjectRow 0 -InjectCol 0
#>
param(
  [Parameter(Mandatory=$true)][string]$Source,
  [int]$Cols = 1,
  [int]$Rows = 1,
  [int]$CellW = 24,
  [int]$CellH = 32,
  [string]$OutBmp = "",
  [string]$OutPreview = "",
  [int]$PreviewScale = 6,
  [double]$Fit = 0.90,          # tallest character fills this fraction of CellH (before outline)
  [int]$BottomMargin = 2,       # empty rows below the feet
  [double]$Dither = 0.0,        # 0..1 ordered dither strength
  [double]$Contrast = 1.10,
  [double]$Saturation = 1.15,
  [switch]$Outline,
  [switch]$FlipX,
  [int]$RotateCells = 0,          # engine direction d shows source cell (d+RotateCells) mod n (the engine numbers dir 0 = screen up-right, my sheets start at "up")
  [int]$CellMargin = 0,           # ignore this many source px at every cell edge (removes grid lines drawn by the generator)
  [double]$ScaleOverride = 0,     # reuse the scale printed by an earlier run so poses stay the same size
  [string]$MirrorCells = "",      # "dest:src,dest:src" - replace a cell by the horizontal mirror of another
  [string]$InjectSheet = "",
  [int]$InjectRow = 0,
  [int]$InjectCol = 0,
  [string]$InjectColList = ""      # explicit destination cell column per source cell, e.g. "1,2,3,4,5,8,9,10" (object ids are not contiguous)
)

$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs'), (Join-Path $PSScriptRoot 'SpritePacker.cs')

$srcImg = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source).Path)
$src = New-Object System.Drawing.Bitmap $srcImg
$srcImg.Dispose()
$cells = [SpritePacker]::Pack($src, $Cols, $Rows, $CellW, $CellH, $Fit, $BottomMargin, $Dither, $Contrast, $Saturation, $Outline.IsPresent, $FlipX.IsPresent, $ScaleOverride, $CellMargin)
Write-Host ("Scale used: " + [SpritePacker]::LastScale)
if ($MirrorCells) { foreach ($pair in ($MirrorCells -split ",")) { $d,$s = $pair -split ":"; $m = New-Object int[] ($CellW * $CellH); for ($y=0;$y -lt $CellH;$y++){ for($x=0;$x -lt $CellW;$x++){ $m[$y*$CellW+$x] = $cells[[int]$s][$y*$CellW + ($CellW-1-$x)] } }; $cells[[int]$d] = $m } }
if ($RotateCells -ne 0) { $nn = $cells.Count; $old = @($cells); for ($d = 0; $d -lt $nn; $d++) { $cells[$d] = $old[(($d + $RotateCells) % $nn + $nn) % $nn] } }
$n = $Cols * $Rows

function Save-Strip($path, $cells, $cw, $ch) {
  $bmp = New-Object System.Drawing.Bitmap ($cw * $cells.Count), $ch, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  for ($k = 0; $k -lt $cells.Count; $k++) {
    for ($y = 0; $y -lt $ch; $y++) { for ($x = 0; $x -lt $cw; $x++) {
      $bmp.SetPixel($k * $cw + $x, $y, [GamePalette]::ToColor($cells[$k][$y * $cw + $x]))
    } }
  }
  $dir = Split-Path $path -Parent
  if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
  return $bmp
}

if ($OutBmp) { $strip = Save-Strip (Join-Path (Get-Location) $OutBmp) $cells $CellW $CellH; $strip.Dispose() }

if ($OutPreview) {
  # Preview: cells in the source grid layout, nearest-neighbour upscaled on a neutral dark grey.
  $pw = $Cols * $CellW * $PreviewScale; $ph = $Rows * $CellH * $PreviewScale
  $pv = New-Object System.Drawing.Bitmap $pw, $ph
  $g = [System.Drawing.Graphics]::FromImage($pv); $g.Clear([System.Drawing.Color]::FromArgb(70,90,70))
  for ($k = 0; $k -lt $n; $k++) {
    $bx = ($k % $Cols) * $CellW; $by = [math]::Floor($k / $Cols) * $CellH
    for ($y = 0; $y -lt $CellH; $y++) { for ($x = 0; $x -lt $CellW; $x++) {
      $idx = $cells[$k][$y * $CellW + $x]; if ($idx -ge 256) { continue }
      $br = New-Object System.Drawing.SolidBrush ([GamePalette]::ToColor($idx))
      $g.FillRectangle($br, ($bx + $x) * $PreviewScale, ($by + $y) * $PreviewScale, $PreviewScale, $PreviewScale)
      $br.Dispose()
    } }
  }
  $g.Dispose()
  $pdir = Split-Path (Join-Path (Get-Location) $OutPreview) -Parent
  if (-not (Test-Path $pdir)) { New-Item -ItemType Directory -Force $pdir | Out-Null }
  $pv.Save((Join-Path (Get-Location) $OutPreview), [System.Drawing.Imaging.ImageFormat]::Png)
  $pv.Dispose()
}

if ($InjectSheet) {
  $sp = (Resolve-Path $InjectSheet).Path
  $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($sp)); $tmp = [System.Drawing.Bitmap]::FromStream($ms)   # from memory so the file is not locked when saving
  $sheet = $tmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $tmp.Width, $tmp.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb); $tmp.Dispose()   # Clone = exact pixel copy (no resampling)
  for ($k = 0; $k -lt $n; $k++) {
    $colK = $InjectCol + $k; if ($InjectColList) { $colK = [int](($InjectColList -split ',')[$k]) }
    $dx = $colK * $CellW; $dy = $InjectRow * $CellH
    if ($dx + $CellW -gt $sheet.Width -or $dy + $CellH -gt $sheet.Height) { Write-Warning "cell $k outside sheet, skipped"; continue }
    for ($y = 0; $y -lt $CellH; $y++) { for ($x = 0; $x -lt $CellW; $x++) {
      $sheet.SetPixel($dx + $x, $dy + $y, [GamePalette]::ToColor($cells[$k][$y * $CellW + $x]))
    } }
  }
  $sheet.Save($sp, [System.Drawing.Imaging.ImageFormat]::Bmp)
  $sheet.Dispose()
  Write-Host "Injected $n cells into $InjectSheet at row $InjectRow, col $InjectCol"
}
Write-Host "Packed $n cells ($CellW x $CellH)."







