<#
.SYNOPSIS
  Projects flat wall-panel textures (sheet of panels, reading order) into the game's isometric West and North
  wall sprites (16x32) and pastes them into assets\WALLS_W.BMP / assets\WALLS_N.BMP.

.EXAMPLE
  powershell -File tools\art\pack_walls.ps1 -Source art_src\walls\walls_A_raw.jpg -Cols 4 -Rows 2 `
     -WallIdList "1,2,3,4,5,6,7,8" -Inject -OutPreview art_src\build\walls_A.png
#>
param(
  [Parameter(Mandatory=$true)][string]$Source,
  [int]$Cols = 4,
  [int]$Rows = 2,
  [Parameter(Mandatory=$true)][string]$WallIdList,
  [switch]$Inject,
  [string]$OutPreview = "",
  [int]$PreviewScale = 8,
  [double]$Inset = 0.02,
  [double]$ShadeW = 0.86,      # west walls are drawn a little darker than north walls (depth cue)
  [double]$ShadeN = 1.0,
  [double]$Contrast = 1.15,
  [double]$Saturation = 1.10,
  [double]$Brightness = 8,
  [double]$Dither = 0.10
)
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs'), (Join-Path $PSScriptRoot 'WallProjector.cs')
function Load-Bmp([string]$path) { $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($path)); return [System.Drawing.Bitmap]::FromStream($ms) }

$src = Load-Bmp (Resolve-Path $Source).Path
$ids = @($WallIdList -split ',' | ForEach-Object { [int]$_ })
$n = $Cols * $Rows
if ($ids.Count -lt $n) { throw "Need $n wall ids, got $($ids.Count)" }
$sw = [int]($src.Width / $Cols); $sh = [int]($src.Height / $Rows)
$wArr = @(); $nArr = @()
for ($k = 0; $k -lt $n; $k++) {
  $x0 = ($k % $Cols) * $sw; $y0 = [math]::Floor($k / $Cols) * $sh
  $wArr += ,([WallProjector]::Project($src, $x0, $y0, $sw, $sh, $Inset, $false, $ShadeW, $Contrast, $Saturation, $Brightness, $Dither))
  $nArr += ,([WallProjector]::Project($src, $x0, $y0, $sw, $sh, $Inset, $true,  $ShadeN, $Contrast, $Saturation, $Brightness, $Dither))
}

if ($OutPreview) {
  # each wall shown as a corner: West piece next to North piece, over a dark floor-coloured block
  $ps = $PreviewScale; $cellW = 40; $cellH = 40
  $pv = New-Object System.Drawing.Bitmap ($Cols * $cellW * $ps), ($Rows * $cellH * $ps)
  $g = [System.Drawing.Graphics]::FromImage($pv); $g.Clear([System.Drawing.Color]::FromArgb(60,70,60))
  for ($k = 0; $k -lt $n; $k++) {
    $ox = ($k % $Cols) * $cellW + 4; $oy = [math]::Floor($k / $Cols) * $cellH + 6
    foreach ($pair in @(@($wArr, 0), @($nArr, 16))) {
      $arr = $pair[0]; $off = $pair[1]
      for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 16; $x++) {
        $idx = $arr[$k][$y * 16 + $x]; if ($idx -ge 256) { continue }
        $br = New-Object System.Drawing.SolidBrush ([GamePalette]::ToColor($idx))
        $g.FillRectangle($br, ($ox + $off + $x) * $ps, ($oy + $y) * $ps, $ps, $ps); $br.Dispose()
      } }
    }
  }
  $g.Dispose()
  $full = Join-Path (Get-Location) $OutPreview; $d = Split-Path $full -Parent; if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force $d | Out-Null }
  $pv.Save($full, [System.Drawing.Imaging.ImageFormat]::Png); $pv.Dispose()
}

if ($Inject) {
  foreach ($job in @(@('WALLS_W', $wArr), @('WALLS_N', $nArr))) {
    $sp = (Resolve-Path ("assets\" + $job[0] + ".BMP")).Path
    $tmp = Load-Bmp $sp
    $sheet = $tmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $tmp.Width, $tmp.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb); $tmp.Dispose()
    for ($k = 0; $k -lt $n; $k++) {
      $dx = $ids[$k] * 16
      if ($dx + 16 -gt $sheet.Width) { Write-Warning "wall id $($ids[$k]) outside the sheet, skipped"; continue }
      for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 16; $x++) { $sheet.SetPixel($dx + $x, $y, [GamePalette]::ToColor($job[1][$k][$y * 16 + $x])) } }
    }
    $sheet.Save($sp, [System.Drawing.Imaging.ImageFormat]::Bmp); $sheet.Dispose()
  }
  Write-Host "Injected $n walls (W+N) into assets\WALLS_W.BMP / WALLS_N.BMP"
}

