<#
.SYNOPSIS
  Projects flat top-down seamless texture swatches into 32x16 isometric diamond floor tiles,
  quantized to the game palette, and pastes them into FLOORS.BMP.

.PARAMETER Source    Sheet of square swatches (-Cols x -Rows), reading order.
.PARAMETER TileIdList Comma list: FLOORS.BMP tile index for each swatch (same order), e.g. "1,2,3,4,5,6,7,8".
.PARAMETER Sheet     Target FLOORS.BMP (32 px per tile). Omit to only write the preview.
.PARAMETER Inset     Fraction (0..0.5) cropped from each swatch edge before projecting (removes AI border artefacts).
.PARAMETER Zoom      >1 samples a smaller part of the texture (bigger texture detail per tile), 1 = whole swatch.

.EXAMPLE
  powershell -File tools\art\pack_floors.ps1 -Source art_src\floors\floors_raw.jpg -Cols 4 -Rows 2 `
    -TileIdList "1,2,3,4,5,6,7,8" -Sheet assets\FLOORS.BMP -OutPreview art_src\build\floors_prev.png
#>
param(
  [Parameter(Mandatory=$true)][string]$Source,
  [int]$Cols = 4,
  [int]$Rows = 2,
  [Parameter(Mandatory=$true)][string]$TileIdList,
  [string]$Sheet = "",
  [string]$OutPreview = "",
  [int]$PreviewScale = 8,
  [double]$Inset = 0.03,
  [double]$Zoom = 1.0,
  [double]$Dither = 0.15,
  [double]$Contrast = 1.10,
  [double]$Saturation = 1.10,
  [double]$Brightness = 0
)
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs'), (Join-Path $PSScriptRoot 'FloorProjector.cs')


$srcImg = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source).Path)
$src = New-Object System.Drawing.Bitmap $srcImg; $srcImg.Dispose()
$sw = [int]($src.Width / $Cols); $sh = [int]($src.Height / $Rows)
$n = $Cols * $Rows
$TileIds = @($TileIdList -split "," | ForEach-Object { [int]$_ })
if ($TileIds.Count -lt $n) { throw "Need $n TileIds, got $($TileIds.Count)" }
$tiles = @()
for ($k = 0; $k -lt $n; $k++) {
  $tiles += ,([FloorProjector]::Project($src, ($k % $Cols) * $sw, [math]::Floor($k / $Cols) * $sh, $sw, $sh, $Inset, $Zoom, $Dither, $Contrast, $Saturation, $Brightness))
}

if ($OutPreview) {
  # Preview shows each tile 2x2 tiled (so seams are visible) on dark green.
  $ps = $PreviewScale
  $pv = New-Object System.Drawing.Bitmap ($Cols * 48 * $ps), ($Rows * 24 * $ps)
  $g = [System.Drawing.Graphics]::FromImage($pv); $g.Clear([System.Drawing.Color]::FromArgb(40,50,40))
  for ($k = 0; $k -lt $n; $k++) {
    $ox = ($k % $Cols) * 48; $oy = [math]::Floor($k / $Cols) * 24
    foreach ($o in @(@(8,0), @(0,4), @(16,4), @(8,8))) {   # four diamonds meeting edge-to-edge (iso 2x2 block)
      for ($y = 0; $y -lt 16; $y++) { for ($x = 0; $x -lt 32; $x++) {
        $idx = $tiles[$k][$y * 32 + $x]; if ($idx -ge 256) { continue }
        $br = New-Object System.Drawing.SolidBrush ([GamePalette]::ToColor($idx))
        $g.FillRectangle($br, ($ox + $o[0] + $x) * $ps, ($oy + $o[1] + $y) * $ps, $ps, $ps); $br.Dispose()
      } }
    }
  }
  $g.Dispose()
  $full = Join-Path (Get-Location) $OutPreview
  $d = Split-Path $full -Parent; if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force $d | Out-Null }
  $pv.Save($full, [System.Drawing.Imaging.ImageFormat]::Png); $pv.Dispose()
}

if ($Sheet) {
  $sp = (Resolve-Path $Sheet).Path
  $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($sp)); $tmp = [System.Drawing.Bitmap]::FromStream($ms)   # from memory so the file is not locked when saving
  $sh2 = $tmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $tmp.Width, $tmp.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb); $tmp.Dispose()   # Clone = exact pixel copy (no resampling)
  for ($k = 0; $k -lt $n; $k++) {
    $dx = $TileIds[$k] * 32
    if ($dx + 32 -gt $sh2.Width) { Write-Warning "tile id $($TileIds[$k]) outside sheet"; continue }
    for ($y = 0; $y -lt 16; $y++) { for ($x = 0; $x -lt 32; $x++) {
      $sh2.SetPixel($dx + $x, $y, [GamePalette]::ToColor($tiles[$k][$y * 32 + $x]))
    } }
  }
  $sh2.Save($sp, [System.Drawing.Imaging.ImageFormat]::Bmp); $sh2.Dispose()
  Write-Host "Pasted $n floor tiles into $Sheet"
}





