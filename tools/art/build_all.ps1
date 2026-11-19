<#
.SYNOPSIS
  Rebuilds ALL new art from art_src/manifest.json into assets/*.BMP, then verifies that only the
  cells/tiles named in the manifest differ from the original (git HEAD) sheets.

.EXAMPLE
  powershell -File tools\art\build_all.ps1                 # everything
  powershell -File tools\art\build_all.ps1 -Only op_fem_red # one unit
  powershell -File tools\art\build_all.ps1 -VerifyOnly      # just run the safety check
#>
param(
  [string]$Manifest = "art_src\manifest.json",
  [string]$Only = "",
  [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$pack   = Join-Path $PSScriptRoot 'pack_sheet.ps1'
$floors = Join-Path $PSScriptRoot 'pack_floors.ps1'
$m = Get-Content $Manifest -Raw | ConvertFrom-Json
$unitsBmp = 'assets\UNITS.BMP'; $floorsBmp = 'assets\FLOORS.BMP'

function Run-Pack([string[]]$argList) {
  $ErrorActionPreference = 'Continue'   # a child process writing to stderr (e.g. profile noise) is not a failure
  $out = & powershell -ExecutionPolicy Bypass -NoProfile -File $pack @argList 2>&1 | Out-String
  if ($LASTEXITCODE -ne 0 -or $out -match 'Exception calling|Cannot |is not recognized') { throw "pack_sheet failed:`n$out" }
  return $out
}

if (-not $VerifyOnly) {
  foreach ($u in $m.units) {
    if ($Only -and $u.id -ne $Only) { continue }
    Write-Host "== unit $($u.id)  (sprite type $($u.type))"
    $fit = if ($u.fit) { [double]$u.fit } else { 0.84 }
    $dir = $u.dir
    $standSrc = Join-Path $dir $u.stand
    $base = $u.type * 4
    # pose 0: stand; its scale is reused for every other pose so they stay the same size
    $extra = @(); if ($m.rotateCells) { $extra += @('-RotateCells', [int]$m.rotateCells) }; if ($u.cellMargin) { $extra += @('-CellMargin', [int]$u.cellMargin) }; if ($u.bottomMargin) { $extra += @('-BottomMargin', [int]$u.bottomMargin) }
    $standMir = $null; if ($u.mirror) { $standMir = $u.mirror.stand }
    $sargs = @('-Source', $standSrc, '-Cols', 4, '-Rows', 2, '-Outline', '-Fit', $fit, '-InjectSheet', $unitsBmp, '-InjectRow', $base) + $extra
    if ($standMir) { $sargs += @('-MirrorCells', $standMir) }
    $o = Run-Pack $sargs
    if ($o -notmatch 'Scale used: ([0-9.]+)') { throw "no scale reported for $($u.id)" }
    $scale = $Matches[1]
    $poses = @(@('kneel', 1), @('walkA', 2), @('walkB', 3))
    foreach ($p in $poses) {
      $name = $p[0]; $row = $base + $p[1]
      $src = $u.$name
      $usesStand = (-not $src -or $src -eq '@stand'); if ($usesStand) { $src = $u.stand }
      $args2 = @('-Source', (Join-Path $dir $src), '-Cols', 4, '-Rows', 2, '-Outline', '-ScaleOverride', $scale, '-InjectSheet', $unitsBmp, '-InjectRow', $row) + $extra
      $mir = $null; if ($u.mirror) { if ($usesStand) { $mir = $u.mirror.stand } else { $mir = $u.mirror.$name } }
      if ($mir) { $args2 += @('-MirrorCells', $mir) }
      Run-Pack $args2 | Out-Null
    }
  }
  foreach ($pd in $m.procDeaths) {
    Write-Host "== procedural deaths  types $($pd.types)  ($($pd.mode))"
    $ErrorActionPreference = 'Continue'
    & powershell -ExecutionPolicy Bypass -NoProfile -File (Join-Path $PSScriptRoot 'gen_deaths_proc.ps1') -Types $pd.types -Mode $pd.mode
    if ($LASTEXITCODE -ne 0) { throw "gen_deaths_proc failed" }
  }  foreach ($dth in $m.deaths) {
    if ($Only -and $dth.id -ne $Only) { continue }
    Write-Host "== death $($dth.id)  (sprite type $($dth.type))"
    $dargs = @('-Source', (Join-Path $dth.dir $dth.src), '-Cols', 4, '-Rows', 1, '-CellW', 32, '-CellH', 32, '-Outline', '-Fit', $(if ($dth.fit) { [double]$dth.fit } else { 0.80 }), '-InjectSheet', 'assets\DEATH.BMP', '-InjectRow', $dth.type)
    if ($dth.cellMargin) { $dargs += @('-CellMargin', [int]$dth.cellMargin) }
    Run-Pack $dargs | Out-Null
  }
  foreach ($w in $m.walls) {
    Write-Host "== walls $($w.src)  ids $($w.ids)"
    $ErrorActionPreference = 'Continue'
    & powershell -ExecutionPolicy Bypass -NoProfile -File (Join-Path $PSScriptRoot 'pack_walls.ps1') -Source $w.src -Cols $w.cols -Rows $w.rows -WallIdList $w.ids -Inset $w.inset -Inject
    if ($LASTEXITCODE -ne 0) { throw "pack_walls failed" }
  }
  foreach ($o in $m.objects) {
    Write-Host "== objects $($o.src)  cols $($o.injectCols)"
    $oargs = @('-Source', $o.src, '-Cols', $o.cols, '-Rows', $o.rows, '-CellW', 32, '-CellH', 40, '-Outline', '-Fit', $(if ($o.fit) { [double]$o.fit } else { 0.92 }), '-BottomMargin', $(if ($o.bottomMargin) { [int]$o.bottomMargin } else { 2 }), '-InjectSheet', 'assets\OBJECTS.BMP', '-InjectRow', 0, '-InjectColList', $o.injectCols)
    if ($o.cellMargin) { $oargs += @('-CellMargin', [int]$o.cellMargin) }
    Run-Pack $oargs | Out-Null
  }
  foreach ($v in $m.vehicles) {
    Write-Host "== vehicles $($v.src)"
    $strip = 'art_src\build\vehicles_strip.bmp'
    Run-Pack @('-Source', $v.src, '-Cols', $v.cols, '-Rows', $v.rows, '-CellW', 48, '-CellH', 48, '-Outline', '-Fit', 0.95, '-BottomMargin', 2, '-OutBmp', $strip) | Out-Null
    $ErrorActionPreference = 'Continue'
    & powershell -ExecutionPolicy Bypass -NoProfile -File (Join-Path $PSScriptRoot 'pack_vehicles.ps1') -Strip $strip -KindsFile $v.kindsFile -Inject
    if ($LASTEXITCODE -ne 0) { throw "pack_vehicles failed" }
  }  foreach ($f in $m.floors) {
    Write-Host "== floors $($f.src)  ids $($f.ids)"
    $ErrorActionPreference = 'Continue'
    & powershell -ExecutionPolicy Bypass -NoProfile -File $floors -Source $f.src -Cols $f.cols -Rows $f.rows -TileIdList $f.ids `
       -Zoom $f.zoom -Contrast $f.contrast -Saturation $f.saturation -Brightness $f.brightness -Sheet $floorsBmp
    if ($LASTEXITCODE -ne 0) { throw "pack_floors failed" }
  }
}

# ---------------- verification against git HEAD ----------------
Write-Host "== verifying only manifest-listed cells changed (vs git HEAD)"
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'BmpDiff.cs')
$tmpDir = Join-Path $root 'art_src\build'; New-Item -ItemType Directory -Force $tmpDir | Out-Null
$allOk = $true
foreach ($job in @(@('assets/UNITS.BMP', 24, 32), @('assets/FLOORS.BMP', 32, 16), @('assets/DEATH.BMP', 32, 32), @('assets/WALLS_W.BMP', 16, 32), @('assets/WALLS_N.BMP', 16, 32), @('assets/OBJECTS.BMP', 32, 40))) {
  $name = Split-Path $job[0] -Leaf; $orig = Join-Path $tmpDir ($name + '.orig')
  cmd /c "git show HEAD:$($job[0]) > ""$orig"""
  $changed = [BmpDiff]::ChangedCells((Resolve-Path $job[0]).Path, $orig, $job[1], $job[2])
  $expected = New-Object System.Collections.Generic.HashSet[string]
  if ($name -eq 'UNITS.BMP') {
    foreach ($u in $m.units) { for ($p = 0; $p -lt 4; $p++) { for ($d = 0; $d -lt 8; $d++) { [void]$expected.Add("$d,$($u.type * 4 + $p)") } } }
  } elseif ($name -eq 'DEATH.BMP') {
    foreach ($dth in $m.deaths) { for ($s = 0; $s -lt 4; $s++) { [void]$expected.Add("$s,$($dth.type)") } }
    foreach ($pd in $m.procDeaths) { foreach ($pt in ($pd.types -split ',')) { for ($s = 0; $s -lt 4; $s++) { [void]$expected.Add("$s,$([int]$pt)") } } }
  } elseif ($name -like 'WALLS_*') {
    foreach ($w in $m.walls) { foreach ($id in ($w.ids -split ',')) { [void]$expected.Add("$([int]$id),0") } }
  } elseif ($name -eq 'OBJECTS.BMP') {
    foreach ($o in $m.objects) { foreach ($id in ($o.injectCols -split ',')) { [void]$expected.Add("$([int]$id),0") } }
    foreach ($v in $m.vehicles) { foreach ($kd in (Get-Content $v.kindsFile -Raw | ConvertFrom-Json | ForEach-Object { $_ })) { foreach ($pn in 'backX','frontX','backY','frontY') { [void]$expected.Add("$([int]$kd.$pn),0") } } }
  } else {
    foreach ($f in $m.floors) { foreach ($id in ($f.ids -split ',')) { [void]$expected.Add("$([int]$id),0") } }
  }
  $unexpected = @($changed | Where-Object { -not $expected.Contains($_) })
  Write-Host ("   {0}: {1} cells changed, {2} unexpected" -f $name, $changed.Count, $unexpected.Count)
  if ($unexpected.Count -gt 0) { $allOk = $false; Write-Host ("   UNEXPECTED: " + ($unexpected -join ' ')) -ForegroundColor Red }
}
if ($allOk) { Write-Host "OK: nothing outside the manifest was modified." -ForegroundColor Green } else { exit 2 }








