# Regression for the complete overwrite-install sequence for the three
# independent recording directories: old Profiles -> installer merge ->
# first-run bootstrap. Stopping after the merge is what let the bootstrap
# overwrite slip through, so every case asserts the value a user would see
# after first launch.
#
# Runs entirely in a temp directory. Touches no installed copy of OBS-PT.

param(
    [string]$PolicyExe = ""
)

$ErrorActionPreference = 'Stop'
$script:Failures = 0

function Check {
    param([bool]$Condition, [string]$What)
    if ($Condition) { return }
    $script:Failures++
    Write-Host "FAIL: $What"
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$preserve = Join-Path $here 'obspt-preserve-config.ps1'
if (-not (Test-Path -LiteralPath $preserve)) {
    throw "obspt-preserve-config.ps1 not found next to this script"
}

$packagedDefault = 'recordings'
$absoluteDefault = 'K:/Install/OBS-PT/recordings'

# A [string] parameter silently coerces $null to '', which would make "the key
# is absent" indistinguishable from "the key is empty". Use an explicit
# sentinel for absence instead.
$omit = '<omit-key>'

function New-Profile {
    param(
        [string]$Root,
        [string]$Name,
        [string]$Simple,
        [string]$Advanced,
        [string]$FFmpeg
    )
    $dir = Join-Path $Root $Name
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $lines = @('[AdvOut]', 'RecType=Standard')
    if ($Advanced -ne $omit) { $lines += "RecFilePath=$Advanced" }
    if ($FFmpeg -ne $omit) { $lines += "FFFilePath=$FFmpeg" }
    $lines += @('', '[SimpleOutput]')
    if ($Simple -ne $omit) { $lines += "FilePath=$Simple" }
    Set-Content -LiteralPath (Join-Path $dir 'basic.ini') -Value $lines -Encoding UTF8
    return $dir
}

function Get-IniPath {
    param([string]$IniPath, [string]$Section, [string]$Key)
    $current = ''
    foreach ($line in Get-Content -LiteralPath $IniPath) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(.+)\]$') { $current = $Matches[1]; continue }
        if ($current -ne $Section) { continue }
        if ($trimmed -match "^$([regex]::Escape($Key))=(.*)$") {
            return $Matches[1]
        }
    }
    return $null
}

# Mirrors OBSPTRecordPathNeedsDefault: bootstrap initialises a directory only
# when it is absent, empty, or still the packaged placeholder.
function Invoke-Bootstrap {
    param([string]$IniPath)
    $result = @{}
    foreach ($slot in @(
        @{ Name = 'simple';   Section = 'SimpleOutput'; Key = 'FilePath' },
        @{ Name = 'advanced'; Section = 'AdvOut';       Key = 'RecFilePath' },
        @{ Name = 'ffmpeg';   Section = 'AdvOut';       Key = 'FFFilePath' }
    )) {
        $value = Get-IniPath $IniPath $slot.Section $slot.Key
        if ([string]::IsNullOrEmpty($value) -or $value -eq $packagedDefault) {
            $result[$slot.Name] = $absoluteDefault
        } else {
            $result[$slot.Name] = $value
        }
    }
    return $result
}

function Invoke-Sequence {
    param(
        [scriptblock]$BuildOldProfiles,
        [string]$OldActiveProfile
    )

    $work = Join-Path ([System.IO.Path]::GetTempPath()) ("obspt-rec-" + [guid]::NewGuid().ToString('N'))
    $backup = Join-Path $work 'backup'
    $target = Join-Path $work 'obs-studio'
    $backupProfiles = Join-Path $backup 'basic\profiles'
    $targetProfiles = Join-Path $target 'basic\profiles'
    New-Item -ItemType Directory -Path $backupProfiles -Force | Out-Null
    New-Item -ItemType Directory -Path $targetProfiles -Force | Out-Null

    & $BuildOldProfiles $backupProfiles

    Set-Content -LiteralPath (Join-Path $backup 'global.ini') `
        -Value @('[Basic]', "ProfileDir=$OldActiveProfile") -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $target 'global.ini') `
        -Value @('[Basic]', 'ProfileDir=PotPvP') -Encoding UTF8

    # The freshly installed PotPvP Profile ships the relative placeholder.
    New-Profile $targetProfiles 'PotPvP' $packagedDefault $packagedDefault $packagedDefault | Out-Null

    & $preserve $backup $target | Out-Null

    $merged = Join-Path $targetProfiles 'PotPvP\basic.ini'
    $final = Invoke-Bootstrap $merged
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    return $final
}

# --- three distinct paths survive the whole sequence ---
$r = Invoke-Sequence {
    param($root)
    New-Profile $root 'PotPvP' 'D:/simple' 'E:/advanced' 'F:/ffmpeg' | Out-Null
} 'PotPvP'
Check ($r.simple -eq 'D:/simple') "Simple Output path survives to first launch (got '$($r.simple)')"
Check ($r.advanced -eq 'E:/advanced') "Advanced recording path survives (got '$($r.advanced)')"
Check ($r.ffmpeg -eq 'F:/ffmpeg') "Custom FFmpeg path survives (got '$($r.ffmpeg)')"

# --- the reported regression: one path, restored, must not be reset ---
$r = Invoke-Sequence {
    param($root)
    New-Profile $root 'PotPvP' 'D:/OBS-PT/recordings' 'D:/OBS-PT/recordings' 'D:/OBS-PT/recordings' | Out-Null
} 'PotPvP'
Check ($r.simple -eq 'D:/OBS-PT/recordings') "the reported reset no longer happens (got '$($r.simple)')"

# --- partial values: only the empty slot is initialised ---
$r = Invoke-Sequence {
    param($root)
    New-Profile $root 'PotPvP' 'D:/simple' '' 'F:/ffmpeg' | Out-Null
} 'PotPvP'
Check ($r.simple -eq 'D:/simple') "a filled slot is untouched beside an empty one"
Check ($r.advanced -eq $absoluteDefault) "the empty slot alone gets the absolute default"
Check ($r.ffmpeg -eq 'F:/ffmpeg') "the third slot is unaffected by the empty one"

# --- a missing or offline drive is still a user choice ---
$r = Invoke-Sequence {
    param($root)
    New-Profile $root 'PotPvP' 'Z:/offline/rec' 'Z:/offline/rec' 'Z:/offline/rec' | Out-Null
} 'PotPvP'
Check ($r.simple -eq 'Z:/offline/rec') "an offline drive path is preserved (got '$($r.simple)')"

# --- multiple old Profiles: only the ACTIVE one may supply the paths ---
$r = Invoke-Sequence {
    param($root)
    # Same name as the new Profile, but NOT the active one.
    New-Profile $root 'PotPvP' 'X:/inactive' 'X:/inactive' 'X:/inactive' | Out-Null
    New-Profile $root 'MyProfile' 'D:/active' 'E:/active' 'F:/active' | Out-Null
} 'MyProfile'
Check ($r.simple -eq 'D:/active') "a same-named inactive Profile cannot supply Simple Output (got '$($r.simple)')"
Check ($r.advanced -eq 'E:/active') "a same-named inactive Profile cannot supply Advanced (got '$($r.advanced)')"
Check ($r.ffmpeg -eq 'F:/active') "a same-named inactive Profile cannot supply Custom FFmpeg (got '$($r.ffmpeg)')"

# --- an inactive Profile must not fill a gap the active one left ---
# The active Profile omits the key entirely (not an empty value), so a
# same-name merge would leave the inactive Profile's value standing.
$r = Invoke-Sequence {
    param($root)
    New-Profile $root 'PotPvP' 'X:/inactive' 'X:/inactive' 'X:/inactive' | Out-Null
    New-Profile $root 'MyProfile' $omit 'E:/active' 'F:/active' | Out-Null
} 'MyProfile'
Check ($r.simple -eq $absoluteDefault) "an inactive Profile cannot fill the active Profile's missing key (got '$($r.simple)')"

# --- and it must not fill a gap left as an empty value either ---
$r = Invoke-Sequence {
    param($root)
    New-Profile $root 'PotPvP' 'X:/inactive' 'X:/inactive' 'X:/inactive' | Out-Null
    New-Profile $root 'MyProfile' '' 'E:/active' 'F:/active' | Out-Null
} 'MyProfile'
Check ($r.simple -eq $absoluteDefault) "an inactive Profile cannot fill the active Profile's empty slot (got '$($r.simple)')"

# --- a fresh install expands the packaged placeholder ---
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("obspt-rec-fresh-" + [guid]::NewGuid().ToString('N'))
$freshProfiles = Join-Path $work 'basic\profiles'
New-Item -ItemType Directory -Path $freshProfiles -Force | Out-Null
New-Profile $freshProfiles 'PotPvP' $packagedDefault $packagedDefault $packagedDefault | Out-Null
$fresh = Invoke-Bootstrap (Join-Path $freshProfiles 'PotPvP\basic.ini')
Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
Check ($fresh.simple -eq $absoluteDefault) "a fresh install expands the placeholder for Simple Output"
Check ($fresh.advanced -eq $absoluteDefault) "a fresh install expands the placeholder for Advanced"
Check ($fresh.ffmpeg -eq $absoluteDefault) "a fresh install expands the placeholder for Custom FFmpeg"

# --- a later bootstrap version bump preserves what the first run settled ---
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("obspt-rec-bump-" + [guid]::NewGuid().ToString('N'))
$bumpProfiles = Join-Path $work 'basic\profiles'
New-Item -ItemType Directory -Path $bumpProfiles -Force | Out-Null
New-Profile $bumpProfiles 'PotPvP' 'D:/simple' $absoluteDefault 'F:/ffmpeg' | Out-Null
$bump = Invoke-Bootstrap (Join-Path $bumpProfiles 'PotPvP\basic.ini')
Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
Check ($bump.simple -eq 'D:/simple') "a version bump preserves a user path"
Check ($bump.advanced -eq $absoluteDefault) "a version bump preserves an already-expanded default"
Check ($bump.ffmpeg -eq 'F:/ffmpeg') "a version bump preserves the Custom FFmpeg path"

# --- the native policy binary agrees with the model used above ---
if ($PolicyExe -and (Test-Path -LiteralPath $PolicyExe)) {
    & $PolicyExe | Out-Null
    Check ($LASTEXITCODE -eq 0) "the shipped record path policy binary passes"
}

if ($script:Failures -eq 0) {
    Write-Host "All OBS-PT recording path sequence tests passed"
    exit 0
}
Write-Host "$($script:Failures) recording path sequence assertion(s) failed"
exit 1
