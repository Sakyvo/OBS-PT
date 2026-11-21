# Refresh product files only; preserve portable user data and the accepted predecessor.
param([string]$Version = '1.3.0')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$target = Join-Path $repo '.target'
$staging = [IO.Path]::GetFullPath((Join-Path $target '_pkg/OBS-PT'))
$app = [IO.Path]::GetFullPath((Join-Path $target '.distribution/app'))
$previous = [IO.Path]::GetFullPath((Join-Path $target '.distribution/app-prev'))
$installerExe = Join-Path $target ".distribution/installer/OBS-PT-$Version-Installer.exe"
$receipt = Join-Path $target 'delivery.json'
$rollback = Join-Path $target 'Testing/delivery-refresh-rollback'
$old = if (Test-Path $receipt) { Get-Content $receipt -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
if (Test-Path $rollback) { throw "Unfinished delivery transaction: $rollback" }
foreach ($file in @("$staging/bin/OBS-PT.exe", $installerExe)) {
    if (-not (Test-Path $file)) { throw "Missing delivery input: $file" }
    $v = (Get-Item $file).VersionInfo
    if ("$($v.ProductMajorPart).$($v.ProductMinorPart).$($v.ProductBuildPart)" -ne $Version) { throw "Unexpected product version: $file" }
}
$cache = Get-Content "$target/CMakeCache.txt" -Raw
if ($cache -notmatch 'BUILD_TESTS:BOOL=OFF' -or $cache -notmatch 'CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022' -or
    $cache -notmatch "OBS_VERSION_OVERRIDE:STRING=$([regex]::Escape($Version))") { throw 'Release configuration mismatch' }
$running = Get-CimInstance Win32_Process | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($app + '\', [StringComparison]::OrdinalIgnoreCase) }
if ($running) { throw 'Close the running OBS-PT before refreshing its delivery directory' }
if ($old.app.status -eq 'accepted' -and (Test-Path $previous)) { throw 'An accepted predecessor already exists; inspect before refresh' }

function Hash([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Manifest([string]$root) {
    @(Get-ChildItem $root -Recurse -File | Where-Object { $_.FullName -notlike "$root\obs-studio\*" -and $_.FullName -notlike "$root\recordings\*" } | ForEach-Object {
        @{ path = $_.FullName.Substring($repo.Length + 1).Replace('\', '/'); bytes = $_.Length; sha256 = Hash $_.FullName }
    } | Sort-Object { $_.path })
}
$inputs = @('CMakeLists.txt', 'UI', 'libobs', 'plugins', 'cmake', 'deps2019', 'C:/Qt/5.15.2/msvc2019_64')
$inputs += @(Get-ChildItem $repo -Directory -Filter 'libobs-*' | Select-Object -ExpandProperty Name)
$lines = [Collections.Generic.List[string]]::new()
foreach ($set in $inputs) {
    $path = if ([IO.Path]::IsPathRooted($set)) { $set } else { Join-Path $repo $set }
    $files = if (Test-Path $path -PathType Leaf) { @(Get-Item $path) } else { @(Get-ChildItem $path -Recurse -File | Where-Object { $_.FullName -notmatch '[\\/]\.git([\\/]|$)' }) }
    foreach ($f in $files) {
        $relative = if ($f.FullName.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) { $f.FullName.Substring($repo.Length + 1) } else { $f.FullName }
        $lines.Add($relative.Replace('\', '/') + [char]0 + (Hash $f.FullName) + "`n")
    }
}
$lines.Sort([StringComparer]::Ordinal)
$fingerprint = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes(($lines -join '')))).ToLowerInvariant()
$source = @{ kind = 'input-fingerprint'; value = $fingerprint; inputs = $inputs; algorithm = 'SHA256 ordinal UTF8 path + NUL + file SHA256 + LF' }
$stamp = (Get-Date).ToUniversalTime().ToString('o')
$manual = @($old.app.pendingManualItems | ForEach-Object { $_.Replace('OBS-PT-1.2.0-Installer.exe', "OBS-PT-$Version-Installer.exe") })
$manual += @(
    '080/081: Select the CBZ window in Game Capture; verify active cooperative status, counters, preview, recording, resize/fullscreen and restart recovery.',
    '081: Force ordinary capture, then disable it; verify actual path/reason and complete recordings. Unsupported clients retain ordinary capture.',
    '082: Gameplay A/B/C at unchanged 480 FPS/resolution/quality, 30 seconds, >=4 GiB free RAM, reversed order twice; collect real frame times, lag and repeat/missing-image evidence. No performance improvement claimed until accepted.',
    '082: Upgrade from 1.2.0 preserves existing layout, encoder, Java choice, scene target and portable settings. Public release requires approval.'
)
$productDirs = @('bin', 'data', 'obs-plugins', 'include', 'cmake')
$movedAccepted = $false
$createdConfig = $false
$changedDirs = [Collections.Generic.List[string]]::new()
$restored = $false
New-Item -ItemType Directory $rollback | Out-Null
try {
    if ($old.app.status -eq 'accepted' -and (Test-Path $app)) {
        Move-Item $app $previous
        $movedAccepted = $true
    }
    New-Item -ItemType Directory -Force $app | Out-Null
    if (Test-Path $receipt) { Copy-Item $receipt "$rollback/delivery.json" }
    foreach ($name in $productDirs) {
        $dest = Join-Path $app $name
        if (Test-Path $dest) { Move-Item $dest (Join-Path $rollback $name) }
        $changedDirs.Add($name)
        if (Test-Path (Join-Path $staging $name)) { Copy-Item (Join-Path $staging $name) $dest -Recurse }
    }
    # Fresh install only; an existing User Data Root is user-owned.
    if (-not (Test-Path "$app/obs-studio")) {
        $config = if ($movedAccepted) { "$previous/obs-studio" } else { "$staging/obs-studio" }
        if (Test-Path $config) {
            $createdConfig = $true
            Copy-Item $config "$app/obs-studio" -Recurse
        }
    }
    if ($movedAccepted -and (Test-Path "$previous/recordings")) { Move-Item "$previous/recordings" "$app/recordings" }
    foreach ($name in $productDirs) {
        $src = Join-Path $staging $name
        if (Test-Path $src) { foreach ($f in Get-ChildItem $src -Recurse -File) {
            $dest = Join-Path (Join-Path $app $name) $f.FullName.Substring($src.Length + 1)
            if ((Hash $dest) -ne (Hash $f.FullName)) { throw "Delivery hash mismatch: $dest" }
        } }
    }
    $doc = $old
    $doc.schemaVersion = 1; $doc.project = 'OBS-PT'
    $doc.app = @{
        status = 'pending-manual-acceptance'; version = $Version; directory = '.target/.distribution/app'; source = $source
        releaseConfiguration = 'VS2022 v143 x64 RelWithDebInfo; BUILD_TESTS=OFF'
        buildCommand = 'cmake --build .target --config RelWithDebInfo; cmake --install --prefix .target/_pkg/OBS-PT; obspt-refresh-delivery.ps1'
        pendingSinceUtc = $stamp; pendingManualItems = @($manual | Select-Object -Unique)
        primaryEntry = 'bin/OBS-PT.exe'; primaryEntryBytes = (Get-Item "$app/bin/OBS-PT.exe").Length
        primaryEntrySha256 = Hash "$app/bin/OBS-PT.exe"; files = Manifest $app
        performance = @{ status = 'pending-manual-acceptance'; improvementClaimed = $false }
    }
    $doc.installer = @{
        status = 'pending-manual-acceptance'; version = $Version; source = $source
        buildCommand = 'makensis UI/installer/obspt-setup.nsi'; pendingSinceUtc = $stamp
        pendingManualItems = @('Complete install/upgrade on target system; publication requires explicit approval')
        files = @(@{path = ".target/.distribution/installer/OBS-PT-$Version-Installer.exe"; bytes = (Get-Item $installerExe).Length; sha256 = Hash $installerExe})
    }
    $doc | ConvertTo-Json -Depth 12 | Set-Content "$rollback/new-receipt.json" -Encoding utf8
    Move-Item "$rollback/new-receipt.json" $receipt -Force
    $restored = $true
} catch {
    foreach ($name in $changedDirs) {
        if (Test-Path (Join-Path $app $name)) { Remove-Item (Join-Path $app $name) -Recurse -Force }
        if (Test-Path (Join-Path $rollback $name)) {
            Move-Item (Join-Path $rollback $name) (Join-Path $app $name)
        }
    }
    if ($createdConfig -and (Test-Path "$app/obs-studio")) { Remove-Item "$app/obs-studio" -Recurse -Force }
    if (Test-Path "$rollback/delivery.json") { Copy-Item "$rollback/delivery.json" $receipt -Force }
    if ($movedAccepted) {
        if (Test-Path "$app/recordings") { Move-Item "$app/recordings" "$previous/recordings" }
        Remove-Item $app -Recurse -Force
        Move-Item $previous $app
    }
    $restored = $true
    throw
} finally {
    if ($restored) { Remove-Item $rollback -Recurse -Force }
}
"App SHA256: $($doc.app.primaryEntrySha256)"
"Installer SHA256: $($doc.installer.files[0].sha256)"
"Input fingerprint: $fingerprint"
