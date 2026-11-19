<#
.SYNOPSIS
  Procedural death animation: articulates a unit's front-facing sprite (UNITS.BMP, direction 3) into a 4-frame collapse
  using a 3-segment kinematic chain (Upper Body [y=0..18], Thighs [y=18..24], Shins [y=24..30]) for humans, or a 4-stage
  sparking crash-and-wreck for drones, then applies a crisp 1px black outline to each 32x32 frame in assets\DEATH.BMP.

.PARAMETER Types   comma list of sprite types, e.g. "9,10,11,12,13,14"
.PARAMETER Mode    human = articulated 3-segment collapse (stagger -> drop to knees -> fall -> sprawled corpse); drone = crash wreck
#>
param(
  [Parameter(Mandatory=$true)][string]$Types,
  [string]$Mode = 'human',
  [int]$Dir = 3
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'GamePalette.cs')

function Load-Bmp([string]$path) {
  $ms = New-Object System.IO.MemoryStream (,[System.IO.File]::ReadAllBytes($path))
  return [System.Drawing.Bitmap]::FromStream($ms)
}
function Is-Key([System.Drawing.Color]$c) {
  return ($c.R -ge 245 -and $c.G -le 15 -and $c.B -ge 245)
}
function Is-BlackOutline([System.Drawing.Color]$c) {
  return ($c.R -le 12 -and $c.G -le 12 -and $c.B -le 12)
}

$units = Load-Bmp (Resolve-Path 'assets\UNITS.BMP').Path
$tmp = Load-Bmp (Resolve-Path 'assets\DEATH.BMP').Path
$death = $tmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $tmp.Width, $tmp.Height), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$tmp.Dispose()
$keyCol = [System.Drawing.Color]::FromArgb(255, 0, 255)
$blkCol = [System.Drawing.Color]::FromArgb(0, 0, 0)

foreach ($t in ($Types -split ',' | ForEach-Object { [int]$_ })) {
  $sx0 = $Dir * 24; $sy0 = ($t * 4) * 32
  $raw = New-Object 'System.Drawing.Color[]' (24 * 32)
  for ($y = 0; $y -lt 32; $y++) {
    for ($x = 0; $x -lt 24; $x++) {
      $raw[$y * 24 + $x] = $units.GetPixel($sx0 + $x, $sy0 + $y)
    }
  }
  # Strip the 1px outer black outline so rotated outline pixels don't clutter the interior; we re-outline cleanly at the end.
  $src = New-Object 'System.Drawing.Color[]' (24 * 32)
  for ($y = 0; $y -lt 32; $y++) {
    for ($x = 0; $x -lt 24; $x++) {
      $c = $raw[$y * 24 + $x]
      if (Is-Key $c) { $src[$y * 24 + $x] = $keyCol; continue }
      if (Is-BlackOutline $c) {
        $adjBg = ($x -eq 0) -or ($x -eq 23) -or ($y -eq 0) -or ($y -eq 31) -or
                 (Is-Key $raw[$y * 24 + ($x - 1)]) -or (Is-Key $raw[$y * 24 + ($x + 1)]) -or
                 (Is-Key $raw[($y - 1) * 24 + $x]) -or (Is-Key $raw[($y + 1) * 24 + $x])
        if ($adjBg) { $src[$y * 24 + $x] = $keyCol; continue }
      }
      $src[$y * 24 + $x] = $c
    }
  }

  for ($s = 0; $s -lt 4; $s++) {
    $buf = New-Object 'System.Drawing.Color[]' (32 * 32)
    for ($i = 0; $i -lt 1024; $i++) { $buf[$i] = $keyCol }

    if ($Mode -eq 'drone') {
      $stages = @(
        @{a=14;  sy=1.00; drop=1;  tx=0;  shade=1.00},
        @{a=-24; sy=0.92; drop=5;  tx=1;  shade=0.94},
        @{a=36;  sy=0.80; drop=10; tx=-1; shade=0.86},
        @{a=16;  sy=0.64; drop=12; tx=0;  shade=0.76}
      )
      $st = $stages[$s]
      $th = $st.a * [math]::PI / 180.0; $ca = [math]::Cos($th); $sa = [math]::Sin($th)
      $pivSrcX = 12.0; $pivSrcY = 16.0
      $outPivX = $pivSrcX + 4.0 + $st.tx; $outPivY = $pivSrcY + 2.0 + $st.drop
      for ($y = 1; $y -lt 31; $y++) {
        for ($x = 1; $x -lt 31; $x++) {
          $dx = ($x + 0.5) - $outPivX; $dy = (($y + 0.5) - $outPivY) / $st.sy
          $u = $dx * $ca - $dy * $sa; $v = $dx * $sa + $dy * $ca
          $ix = [int][math]::Floor($u + $pivSrcX); $iy = [int][math]::Floor($v + $pivSrcY)
          if ($ix -ge 0 -and $ix -lt 24 -and $iy -ge 0 -and $iy -lt 32) {
            $c = $src[$iy * 24 + $ix]
            if (-not (Is-Key $c)) {
              $pi = [GamePalette]::Nearest($c.R * $st.shade, $c.G * $st.shade, $c.B * $st.shade)
              $rgb = [GamePalette]::Pal[$pi]
              $buf[$y * 32 + $x] = [System.Drawing.Color]::FromArgb($rgb[0], $rgb[1], $rgb[2])
            }
          }
        }
      }
      $colSparkY = [System.Drawing.Color]::FromArgb(250, 224, 58)
      $colSparkO = [System.Drawing.Color]::FromArgb(235, 125, 24)
      if ($s -eq 0) {
        $buf[14 * 32 + 15] = $colSparkY; $buf[13 * 32 + 16] = $colSparkO; $buf[16 * 32 + 18] = $colSparkY
      } elseif ($s -eq 1) {
        $buf[17 * 32 + 14] = $colSparkO; $buf[18 * 32 + 15] = $colSparkY; $buf[20 * 32 + 19] = $colSparkO
      } elseif ($s -eq 2) {
        $buf[23 * 32 + 12] = $colSparkY; $buf[24 * 32 + 19] = $colSparkO
      } else {
        $buf[25 * 32 + 15] = $colSparkO
      }
    } else {
      # 3-segment human kinematic chain:
      #   Upper body (head+torso+arms): src y = 0..18.5, pivoted at Hip (src 12, 18.5)
      #   Thighs:                       src y = 18.0..24.5, pivoted at Hip (src 12, 18.5) -> reaches Knee (5.5px down thigh)
      #   Shins+Feet:                   src y = 24.0..30.5, pivoted at Knee (src 12, 24.0)
      # Notice how in Stage 1 (drop to knees), the torso stays nearly upright (angU=14 deg) while thighs angle forward (-28 deg)
      # and shins fold horizontally backward (+82 deg) along the floor!
      # In Stage 2 (falling onto side/back), torso falls left (56 deg), thighs angle down-right (22 deg), shins fold back (86 deg).
      # In Stage 3 (sprawled corpse), torso lies flat left (82 deg), thighs angle right (64 deg), shins bend down-right (102 deg).
      $hStages = @(
        @{hipX=16.5; hipY=21.0; angU=18; angT=-22; angS=28;  sxU=1.00; syU=1.00; sxT=1.00; syT=0.95; sxS=1.00; syS=0.95},
        @{hipX=15.0; hipY=23.5; angU=12; angT=-34; angS=84;  sxU=0.98; syU=0.92; sxT=0.95; syT=0.85; sxS=0.90; syS=0.90},
        @{hipX=16.5; hipY=23.5; angU=48; angT=-12; angS=86;  sxU=0.90; syU=0.94; sxT=0.92; syT=0.85; sxS=0.88; syS=0.90},
        @{hipX=17.5; hipY=24.5; angU=80; angT=44;  angS=108; sxU=0.78; syU=0.92; sxT=0.82; syT=0.88; sxS=0.80; syS=0.88}
      )
      $hs = $hStages[$s]
      $hipSrcX = 12.0; $hipSrcY = 18.5
      $kneeSrcX = 12.0; $kneeSrcY = 24.0
      $thighLen = ($kneeSrcY - $hipSrcY) * $hs.syT

      $thU = $hs.angU * [math]::PI / 180.0; $caU = [math]::Cos($thU); $saU = [math]::Sin($thU)
      $thT = $hs.angT * [math]::PI / 180.0; $caT = [math]::Cos($thT); $saT = [math]::Sin($thT)
      $thS = $hs.angS * [math]::PI / 180.0; $caS = [math]::Cos($thS); $saS = [math]::Sin($thS)

      # Forward kinematics: screen position of Knee = Hip + R(angT) * (0, thighLen)
      # Since inverse mapping uses: u = dx*cos - dy*sin, v = dx*sin + dy*cos,
      # a point (u=0, v=thighLen) in source maps to screen offset: dx = v*sin(angT), dy = v*cos(angT)
      $kneeX = $hs.hipX + $thighLen * $saT
      $kneeY = $hs.hipY + $thighLen * $caT

      for ($y = 1; $y -lt 30; $y++) {
        for ($x = 1; $x -lt 31; $x++) {
          $picked = $keyCol
          # 1. Check Upper Body (head + torso + arms, src y <= 19.0) around Hip
          $dxH = ($x + 0.5) - $hs.hipX; $dyH = ($y + 0.5) - $hs.hipY
          $uU = ($dxH * $caU - $dyH * $saU) / $hs.sxU
          $vU = ($dxH * $saU + $dyH * $caU) / $hs.syU
          $ixU = [int][math]::Floor($uU + $hipSrcX); $iyU = [int][math]::Floor($vU + $hipSrcY)
          if ($vU -le 0.8 -and $ixU -ge 0 -and $ixU -lt 24 -and $iyU -ge 0 -and $iyU -le 19) {
            $cU = $src[$iyU * 24 + $ixU]
            if (-not (Is-Key $cU)) { $picked = $cU }
          }
          # 2. Check Thighs (src y in 18..24.5) around Hip
          if (Is-Key $picked) {
            $uT = ($dxH * $caT - $dyH * $saT) / $hs.sxT
            $vT = ($dxH * $saT + $dyH * $caT) / $hs.syT
            $ixT = [int][math]::Floor($uT + $hipSrcX); $iyT = [int][math]::Floor($vT + $hipSrcY)
            if ($vT -ge -0.8 -and $vT -le 6.2 -and $ixT -ge 0 -and $ixT -lt 24 -and $iyT -ge 18 -and $iyT -le 25) {
              $cT = $src[$iyT * 24 + $ixT]
              if (-not (Is-Key $cT)) { $picked = $cT }
            }
          }
          # 3. Check Shins + Feet (src y in 23.5..31) around Knee
          if (Is-Key $picked) {
            $dxK = ($x + 0.5) - $kneeX; $dyK = ($y + 0.5) - $kneeY
            $uS = ($dxK * $caS - $dyK * $saS) / $hs.sxS
            $vS = ($dxK * $saS + $dyK * $caS) / $hs.syS
            $ixS = [int][math]::Floor($uS + $kneeSrcX); $iyS = [int][math]::Floor($vS + $kneeSrcY)
            if ($vS -ge -0.8 -and $ixS -ge 0 -and $ixS -lt 24 -and $iyS -ge 23 -and $iyS -lt 32) {
              $cS = $src[$iyS * 24 + $ixS]
              if (-not (Is-Key $cS)) { $picked = $cS }
            }
          }
          if (-not (Is-Key $picked)) {
            $shade = if ($s -ge 2 -and $y -ge 26) { 0.80 } else { 1.0 }
            if ($shade -lt 1.0) {
              $pi = [GamePalette]::Nearest($picked.R * $shade, $picked.G * $shade, $picked.B * $shade)
              $rgb = [GamePalette]::Pal[$pi]
              $picked = [System.Drawing.Color]::FromArgb($rgb[0], $rgb[1], $rgb[2])
            }
            $buf[$y * 32 + $x] = $picked
          }
        }
      }
    }

    # Add crisp 1px black outline around all foreground pixels
    $out = $buf.Clone()
    for ($y = 0; $y -lt 32; $y++) {
      for ($x = 0; $x -lt 32; $x++) {
        if (-not (Is-Key $buf[$y * 32 + $x])) { continue }
        $adj = ($x -gt 0 -and -not (Is-Key $buf[$y * 32 + ($x - 1)])) -or
               ($x -lt 31 -and -not (Is-Key $buf[$y * 32 + ($x + 1)])) -or
               ($y -gt 0 -and -not (Is-Key $buf[($y - 1) * 32 + $x])) -or
               ($y -lt 31 -and -not (Is-Key $buf[($y + 1) * 32 + $x]))
        if ($adj) { $out[$y * 32 + $x] = $blkCol }
      }
    }

    for ($y = 0; $y -lt 32; $y++) {
      for ($x = 0; $x -lt 32; $x++) {
        $death.SetPixel($s * 32 + $x, $t * 32 + $y, $out[$y * 32 + $x])
      }
    }
  }
}

$death.Save((Resolve-Path 'assets\DEATH.BMP').Path, [System.Drawing.Imaging.ImageFormat]::Bmp)
$death.Dispose(); $units.Dispose()
Write-Host "Procedural death frames written for types: $Types ($Mode)"
