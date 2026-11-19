<#
.SYNOPSIS
  Splits whole-vehicle sprites into the engine's two-tile object sprites (back + front tile, +X and +Y orientation)
  and pastes them into assets\OBJECTS.BMP.

.DESCRIPTION
  Input strip (from pack_sheet.ps1 -Cols 2 -Rows 2 -CellW 48 -CellH 48 -OutBmp ...): one 48x48 canvas per vehicle,
  the vehicle facing the LOWER-RIGHT, feet centred at x=24, magenta = transparent.
  Geometry (verified against the original art / isomap.c):
    +X pair: back tile at (x,y), front tile at (x+1,y)  => front sprite is drawn at back + (16, 8).
    +Y pair: back tile at (x,y), front tile at (x,y+1)  => front sprite is drawn at back + (-16, 8).
    The two halves meet on the vertical line x = 24 of the canvas.
  +X : back = canvas x<24 at (x,y) ; front = canvas x>=24 at (x-16, y-8)
  +Y : the canvas is mirrored (car faces lower-LEFT): back = x>=24 at (x-16, y) ; front = x<24 at (x, y-8)
#>
param(
  [Parameter(Mandatory=$true)][string]$Strip,
  [Parameter(Mandatory=$true)][string]$KindsFile,    # JSON file: [{"name":"cruiser","backX":6,"frontX":14,"backY":15,"frontY":16}, ...] in strip order
  [switch]$Inject,
  [string]$OutPreview = "",
  [int]$PreviewScale = 6
)
$ErrorActionPreference = 'Stop'
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs')
function Load-Bmp([string]$path) { $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($path)); return [System.Drawing.Bitmap]::FromStream($ms) }
function Is-Key($c) { return ($c.R -ge 245 -and $c.G -le 15 -and $c.B -ge 245) }

$stripBmp = Load-Bmp (Resolve-Path $Strip).Path
$kinds = Get-Content (Resolve-Path $KindsFile).Path -Raw | ConvertFrom-Json
$cells = @{}     # object id -> int[32*40] ARGB (0 = transparent)
$canv = @{}
for ($k = 0; $k -lt @($kinds).Count; $k++) {
  $kd = @($kinds)[$k]
  $cv = New-Object 'int[]' (48 * 48)
  for ($y = 0; $y -lt 48; $y++) { for ($x = 0; $x -lt 48; $x++) {
    $px = $stripBmp.GetPixel($k * 48 + $x, $y); if (-not (Is-Key $px)) { $cv[$y * 48 + $x] = $px.ToArgb() }
  } }
  $canv[$kd.name] = $cv
  $bx = New-Object 'int[]' (32 * 40); $fx = New-Object 'int[]' (32 * 40); $by = New-Object 'int[]' (32 * 40); $fy = New-Object 'int[]' (32 * 40)
  for ($y = 0; $y -lt 48; $y++) { for ($x = 0; $x -lt 48; $x++) {
    $v = $cv[$y * 48 + $x]; $m = $cv[$y * 48 + (47 - $x)]     # $m = mirrored canvas pixel (car facing lower-left)
    if ($x -lt 24) {
      if ($v -ne 0 -and $y -lt 40) { $bx[$y * 32 + $x] = $v }                 # +X back
      if ($m -ne 0 -and $y -ge 8)  { $fy[($y - 8) * 32 + $x] = $m }           # +Y front
    } else {
      if ($v -ne 0 -and $y -ge 8)  { $fx[($y - 8) * 32 + ($x - 16)] = $v }    # +X front
      if ($m -ne 0 -and $y -lt 40) { $by[$y * 32 + ($x - 16)] = $m }          # +Y back
    }
  } }
  $cells[[int]$kd.backX] = $bx; $cells[[int]$kd.frontX] = $fx; $cells[[int]$kd.backY] = $by; $cells[[int]$kd.frontY] = $fy
}

if ($OutPreview) {
  $ps = $PreviewScale; $kn = @($kinds).Count
  $pv = New-Object System.Drawing.Bitmap ($kn * 2 * 80 * $ps), (60 * $ps)
  $g = [System.Drawing.Graphics]::FromImage($pv); $g.Clear([System.Drawing.Color]::FromArgb(70,90,70))
  function Draw-Cell($arr, $ox, $oy) {
    for ($y = 0; $y -lt 40; $y++) { for ($x = 0; $x -lt 32; $x++) {
      $v = $arr[$y * 32 + $x]; if ($v -eq 0) { continue }
      $c = [System.Drawing.Color]::FromArgb($v); $br = New-Object System.Drawing.SolidBrush $c
      $g.FillRectangle($br, ($ox + $x) * $ps, ($oy + $y) * $ps, $ps, $ps); $br.Dispose()
    } }
  }
  for ($k = 0; $k -lt $kn; $k++) {
    $kd = @($kinds)[$k]
    $ax = ($k * 2) * 80 + 18; Draw-Cell $cells[[int]$kd.backX] $ax 4; Draw-Cell $cells[[int]$kd.frontX] ($ax + 16) 12
    $bx0 = ($k * 2 + 1) * 80 + 34; Draw-Cell $cells[[int]$kd.backY] $bx0 4; Draw-Cell $cells[[int]$kd.frontY] ($bx0 - 16) 12
  }
  $g.Dispose()
  $full = Join-Path (Get-Location) $OutPreview; $d = Split-Path $full -Parent; if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force $d | Out-Null }
  $pv.Save($full, [System.Drawing.Imaging.ImageFormat]::Png); $pv.Dispose()
}

if ($Inject) {
  $sp = (Resolve-Path 'assets\OBJECTS.BMP').Path
  $sheet = (Load-Bmp $sp); $sheet = $sheet.Clone((New-Object System.Drawing.Rectangle 0, 0, $sheet.Width, $sheet.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  foreach ($id in $cells.Keys) {
    $arr = $cells[$id]
    for ($y = 0; $y -lt 40; $y++) { for ($x = 0; $x -lt 32; $x++) {
      $v = $arr[$y * 32 + $x]
      $col = if ($v -eq 0) { [System.Drawing.Color]::FromArgb(255, 0, 255) } else { [System.Drawing.Color]::FromArgb($v) }
      $sheet.SetPixel($id * 32 + $x, $y, $col)
    } }
  }
  $sheet.Save($sp, [System.Drawing.Imaging.ImageFormat]::Bmp); $sheet.Dispose()
  Write-Host ("Injected vehicle cells for object ids: " + (($cells.Keys | Sort-Object) -join ','))
}



