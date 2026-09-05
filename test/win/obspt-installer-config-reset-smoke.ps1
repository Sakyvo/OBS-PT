param(
    [string]$WorkRoot =
        'K:/Projects/dev/OBS-PT/.target/_tdd/installer-config-reset'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$work = [IO.Path]::GetFullPath($WorkRoot)
if (-not $work.StartsWith($repo + [IO.Path]::DirectorySeparatorChar)) {
    throw "WorkRoot must stay inside the repository: $work"
}
if (Test-Path -LiteralPath $work) {
    Remove-Item -LiteralPath $work -Recurse -Force
}

$oldRoot = "$work/old"
$newRoot = "$work/new"
$oldProfile = "$oldRoot/basic/profiles/Legacy"
$newProfile = "$newRoot/basic/profiles/PotPvP"
$oldScenes = "$oldRoot/basic/scenes"
$newScenes = "$newRoot/basic/scenes"
New-Item -ItemType Directory -Force -Path @(
    $oldProfile, $newProfile, $oldScenes, $newScenes) | Out-Null

function Write-Utf8 {
    param([string]$Path, [string]$Content)

    [IO.File]::WriteAllText(
        $Path, $Content.TrimStart(), [Text.UTF8Encoding]::new($false))
}

Write-Utf8 "$oldRoot/global.ini" @'
[General]
Language=ja-JP
HotkeyFocusType=NeverDisableHotkeys
EnableCustomServerVodTrack=true
RunAsAdministrator=false

[Basic]
ProfileDir=Legacy
SceneCollectionFile=Legacy

[BasicWindow]
DockState=OLD_LAYOUT
ReplayBufferWhileStreaming=true
KeepReplayBufferStreamStops=true
'@
Write-Utf8 "$newRoot/global.ini" @'
[General]
Language=zh-CN
HotkeyFocusType=DisableHotkeysInFocus
EnableCustomServerVodTrack=false
RunAsAdministrator=true

[Basic]
ProfileDir=PotPvP
SceneCollectionFile=PotPvP

[BasicWindow]
DockState=NEW_LAYOUT
ReplayBufferWhileStreaming=false
KeepReplayBufferStreamStops=false
'@

Write-Utf8 "$oldProfile/basic.ini" @'
[Hotkeys]
OBSBasic.StartRecording={"bindings":[{"key":"OBS_KEY_F9"}]}

[Output]
Mode=Simple
AudioBitrate=64
DelayEnable=true

[SimpleOutput]
StreamEncoder=old_stream_encoder
VBitrate=7777
FilePath=F:/LegacySimple
RecRB=true
RecRBTime=45
RecQuality=Lossless

[AdvOut]
Encoder=old_stream_encoder
RecEncoder=old_record_encoder
RecFilePath=F:/LegacyAdvanced
FFFilePath=F:/LegacyFFmpeg
RecRB=true
RecRBTime=60

[Video]
BaseCX=1280
BaseCY=720
'@
Write-Utf8 "$newProfile/basic.ini" @'
[Hotkeys]
OBSBasic.StartRecording={"bindings":[]}

[Output]
Mode=Advanced
AudioBitrate=160
DelayEnable=false

[SimpleOutput]
StreamEncoder=new_stream_encoder
VBitrate=6000
FilePath=C:/DefaultSimple
RecRB=false
RecRBTime=20
RecQuality=Small

[AdvOut]
Encoder=new_stream_encoder
RecEncoder=new_record_encoder
RecFilePath=C:/DefaultAdvanced
FFFilePath=C:/DefaultFFmpeg
RecRB=false
RecRBTime=20

[Video]
BaseCX=1920
BaseCY=1080
'@

Write-Utf8 "$oldProfile/service.json" '{"settings":{"server":"legacy"}}'
Write-Utf8 "$newProfile/service.json" '{"settings":{"server":"default"}}'
Write-Utf8 "$oldProfile/streamEncoder.json" '{"bitrate":7777}'
Write-Utf8 "$newProfile/streamEncoder.json" '{"bitrate":6000}'
Write-Utf8 "$oldProfile/recordEncoder.json" '{"rate_control":"OLD"}'
Write-Utf8 "$newProfile/recordEncoder.json" '{"rate_control":"CQP"}'

$oldScene = [ordered]@{
    name = 'Legacy'
    current_scene = 'PotPvP'
    sources = @(
        [ordered]@{
            id = 'monitor_capture'
            name = 'Display'
            settings = [ordered]@{ method = 99 }
            hotkeys = [ordered]@{
                'libobs.mute' = @([ordered]@{ key = 'OBS_KEY_F7' })
            }
        }
    )
    obspt_audio_routing = [ordered]@{
        version = 0
        sources = @([ordered]@{ id = 'legacy-route' })
    }
}
$newScene = [ordered]@{
    name = 'PotPvP'
    current_scene = 'PotPvP'
    sources = @(
        [ordered]@{
            id = 'monitor_capture'
            name = 'Display'
            settings = [ordered]@{ method = 0 }
            hotkeys = [ordered]@{ 'libobs.mute' = @() }
        }
    )
    obspt_audio_routing = [ordered]@{
        version = 1
        sources = @([ordered]@{ id = 'new-route' })
    }
}
Write-Utf8 "$oldScenes/Legacy.json" `
    ($oldScene | ConvertTo-Json -Depth 20 -Compress)
Write-Utf8 "$newScenes/PotPvP.json" `
    ($newScene | ConvertTo-Json -Depth 20 -Compress)

& "$repo/UI/installer/obspt-preserve-config.ps1" $oldRoot $newRoot
if (-not $?) {
	throw 'Configuration merge failed'
}

# Failure path: a merge that dies mid-flight must leave an error log file
# next to the target root so the installer's warning is diagnosable.
$failOld = "$work/fail-old"
$failNew = "$work/fail-new"
New-Item -ItemType Directory -Force -Path @(
    "$failOld/basic/profiles/Legacy",
    "$failNew/basic/profiles/PotPvP") | Out-Null
Write-Utf8 "$failOld/global.ini" @'
[Basic]
ProfileDir=Legacy

[BasicWindow]
DockState=FAIL_OLD
'@
Write-Utf8 "$failNew/global.ini" @'
[Basic]
ProfileDir=PotPvP

[BasicWindow]
DockState=FAIL_NEW
'@
Write-Utf8 "$failOld/basic/profiles/Legacy/basic.ini" @'
[Hotkeys]
OBSBasic.StartRecording={"bindings":[{"key":"OBS_KEY_F9"}]}
'@
# Make the target basic.ini a directory so Write-Ini fails mid-merge.
New-Item -ItemType Directory -Force -Path "$failNew/basic/profiles/PotPvP/basic.ini" | Out-Null
$expectedLog = "$failNew/../obspt-preserve-error.log"
$mergeFailed = $false
try {
    & "$repo/UI/installer/obspt-preserve-config.ps1" $failOld $failNew 2>$null
} catch {
    # Expected: the helper dies on the broken target and rethrows.
    $mergeFailed = $true
}
if (-not $mergeFailed) {
    throw 'Failure-path merge unexpectedly succeeded'
}
if (-not (Test-Path -LiteralPath $expectedLog)) {
    throw 'Failure-path merge did not leave a diagnosable error log'
}
if ((Get-Item -LiteralPath $expectedLog).Length -eq 0) {
    throw 'Failure-path error log is empty'
}

$global = [IO.File]::ReadAllText("$newRoot/global.ini")
$basic = [IO.File]::ReadAllText("$newProfile/basic.ini")
$service = [IO.File]::ReadAllText("$newProfile/service.json")
$streamEncoder = [IO.File]::ReadAllText("$newProfile/streamEncoder.json")
$recordEncoder = [IO.File]::ReadAllText("$newProfile/recordEncoder.json")
$scene = [IO.File]::ReadAllText("$newScenes/PotPvP.json") |
    ConvertFrom-Json

foreach ($pattern in @(
        'HotkeyFocusType=NeverDisableHotkeys',
        'EnableCustomServerVodTrack=true',
        'ReplayBufferWhileStreaming=true',
        'KeepReplayBufferStreamStops=true')) {
    if ($global -notmatch [regex]::Escape($pattern)) {
        throw "Global preserved value is missing: $pattern"
    }
}
foreach ($pattern in @(
        'Language=zh-CN',
        'RunAsAdministrator=true',
        'DockState=NEW_LAYOUT')) {
    if ($global -notmatch [regex]::Escape($pattern)) {
        throw "Global packaged default was overwritten: $pattern"
    }
}
foreach ($pattern in @(
        'OBS_KEY_F9',
        'Mode=Advanced',
        'AudioBitrate=160',
        'VBitrate=7777',
        'FilePath=F:/LegacySimple',
        'RecFilePath=F:/LegacyAdvanced',
        'FFFilePath=F:/LegacyFFmpeg',
        'RecRB=true',
        'RecRBTime=60')) {
    if ($basic -notmatch [regex]::Escape($pattern)) {
        throw "Profile preserved value is missing: $pattern"
    }
}
foreach ($pattern in @(
        'RecEncoder=new_record_encoder',
        'RecQuality=Small',
        'BaseCX=1920',
        'BaseCY=1080')) {
    if ($basic -notmatch [regex]::Escape($pattern)) {
        throw "Profile packaged default was overwritten: $pattern"
    }
}
if ($service -notmatch 'legacy' -or $streamEncoder -notmatch '7777') {
    throw 'Stream JSON files were not preserved'
}
if ($recordEncoder -notmatch 'CQP' -or $recordEncoder -match 'OLD') {
    throw 'Recording encoder defaults were overwritten'
}
if ($scene.sources[0].settings.method -ne 0 -or
    $scene.obspt_audio_routing.sources[0].id -ne 'new-route') {
    throw 'Scene layout or routing was restored from the old configuration'
}
if ($scene.sources[0].hotkeys.'libobs.mute'[0].key -ne 'OBS_KEY_F7') {
    throw 'Scene source hotkeys were not preserved'
}

$packagedScenePath =
    "$repo/UI/data/obspt-defaults/obs-studio/basic/scenes/PotPvP.json"
$packagedScene = [IO.File]::ReadAllText($packagedScenePath) |
    ConvertFrom-Json
$expectedSources = @(

[pscustomobject]@{
        Name = 'minecraft-sound'; Executable = 'javaw.exe'
        Identity = 'obspt-default-minecraft-sound'; Track = 2
    },
    [pscustomobject]@{
        Name = 'music'; Executable = 'vivaldi.exe'
        Identity = 'obspt-default-music'; Track = 3
    }
)

# Issue 052: Reset UI restores the packaged default layout
# (data/obs-studio/obspt-defaults/obs-studio/global.ini [BasicWindow]
# DockState), not the session snapshot, and no longer applies the upstream
# five-dock proportional resize.
$mainCpp = [IO.File]::ReadAllText("$repo/UI/window-basic-main.cpp")
$resetMatch = [regex]::Match($mainCpp, '(?s)void OBSBasic::on_resetUI_triggered\(\).*?
\}')
if (-not $resetMatch.Success) {
    throw "on_resetUI_triggered not found in window-basic-main.cpp"
}
$resetBody = $resetMatch.Value
if (-not $resetBody.Contains('obspt-defaults/obs-studio/global.ini') -or
    -not $resetBody.Contains('QByteArray::fromBase64') -or
    -not $resetBody.Contains('restoreState(')) {
    throw "Reset UI must load and restore the packaged DockState"
}
if ($resetBody.Contains('startingDockLayout') -or
    $resetBody.Contains('resizeDocks(')) {
    throw "Reset UI must not use the session snapshot or fixed proportions"
}
if ($resetBody.Contains('statsDock')) {
    throw "Reset UI must not override any dock after restoreState (packaged DockState governs statsDock too)"
}

# Issue 053: hide the Acri theme from the combo; if a user already has it
# selected, fall back to Dark at startup and persist the change.
$settingsCpp = [IO.File]::ReadAllText("$repo/UI/window-basic-settings.cpp")
if (-not $settingsCpp.Contains('if (name == "Acri")')) {
    throw "Settings theme list must skip Acri"
}
$obsAppCpp = [IO.File]::ReadAllText("$repo/UI/obs-app.cpp")
if (-not $obsAppCpp.Contains('strcmp(themeName, "Acri")') -or
    -not $obsAppCpp.Contains('themeName = "Dark"') -or
    -not $obsAppCpp.Contains('config_set_string(globalConfig, "General", "CurrentTheme2", "Dark")')) {
    throw "InitTheme must map Acri to Dark and persist the change"
}

# Issue 054: Welcome Page4 in every shipped locale ends with the multitrack
# manual-setup notice (per-locale bold marker).
$noticeMarkers = @{
    'en-US' = '<b>Note</b>'
    'zh-CN' = '<b>注</b>'
    'zh-TW' = '<b>注</b>'
    'ja-JP' = '<b>注意</b>'
    'ko-KR' = '<b>참고</b>'
}
foreach ($loc in $noticeMarkers.Keys) {
    $ini = [IO.File]::ReadAllText("$repo/UI/data/locale/$loc.ini")
    $idx = $ini.IndexOf('OBSPT.Welcome.Page4=')
    if ($idx -lt 0) { throw "Welcome Page4 missing in locale $loc" }
    $page4 = $ini.Substring($idx)
    $nl = $page4.IndexOf("`n")
    if ($nl -gt 0) { $page4 = $page4.Substring(0, $nl) }
    if (-not $page4.Contains($noticeMarkers[$loc])) {
        throw "Welcome Page4 must carry the manual multitrack note in locale $loc"
    }
}

foreach ($expected in $expectedSources) {
    $source = @($packagedScene.sources | Where-Object name -eq $expected.Name)
    if ($source.Count -ne 1) {
        throw "Packaged source count is invalid: $($expected.Name)"
    }
    $source = $source[0]
    if ($source.id -ne 'wasapi_process_output_capture' -or
        $source.settings.priority -ne 3 -or
        $source.settings.process_executable -ne $expected.Executable -or
        $source.private_settings.obspt_audio_source_id -ne $expected.Identity -or
        $null -ne $source.settings.PSObject.Properties['window']) {
        throw "Packaged process target is invalid: $($expected.Name)"
    }
    $route = @($packagedScene.obspt_audio_routing.sources |
        Where-Object id -eq $expected.Identity)
    if ($route.Count -ne 1 -or
        $route[0].editor_track -ne $expected.Track -or
        $route[0].name -ne $expected.Name) {
        throw "Packaged route is invalid: $($expected.Name)"
    }
}
# Issue 046: the installer loads English + SimpChinese; NSIS
# built-in auto-detection picks the system UI language with no dialog.
$nsi = [IO.File]::ReadAllText("$repo/UI/installer/obspt-setup.nsi")
if ($nsi -notmatch [regex]::Escape('MUI_LANGUAGE "English"')) {
    throw 'Installer does not load English'
}
if ($nsi -notmatch [regex]::Escape('MUI_LANGUAGE "SimpChinese"')) {
    throw 'Installer does not load SimpChinese'
}
if ($nsi -match 'MUI_LANGDLL_DISPLAY') {
    throw 'Installer must not show a language selection dialog'
}
if ($nsi -match 'SetLang') {
    throw 'Installer must not use invalid SetLang command'
}
if ($nsi -notmatch [regex]::Escape('LangString OBSPT_ResetTitle')) {
    throw 'Installer user-facing strings are not localized'
}

# Issue 044: the shipped scene collection must default to single-track.
if ($packagedScene.obspt_audio_routing.audioMode -ne 'single') {
    throw 'Packaged scene audioMode is not single-track'
}

$packagedProfile = [IO.File]::ReadAllText(
    "$repo/UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/basic.ini")
foreach ($pattern in @('RecFilePath=recordings', 'FilePath=recordings')) {
    if ($packagedProfile -notmatch [regex]::Escape($pattern)) {
        throw "Packaged portable recording path is missing: $pattern"
    }
}
if ($packagedProfile -match '(?im)^(RecFilePath|FilePath)=[A-Z]:[/\\]') {
    throw 'Packaged defaults contain a machine-specific recording path'
}
# Issue 042: the shipped profile presets replay buffer parameters so users
# only flip the switch.
foreach ($pattern in @('RecRB=false', 'RecRBSize=4096', 'RecRBTime=180')) {
    if ($packagedProfile -notmatch [regex]::Escape($pattern)) {
        throw "Packaged replay-buffer preset key is missing: $pattern"
    }
# Issue 043: first-run bootstrap must write the probed encoder to BOTH the
# recording and the streaming slots of the active profile.
$bootstrap = [IO.File]::ReadAllText("$repo/UI/obspt-bootstrap.cpp")
$recIdx = $bootstrap.IndexOf('"AdvOut", "RecEncoder"')
$strIdx = $bootstrap.IndexOf('"AdvOut", "Encoder"')
if ($recIdx -lt 0 -or $strIdx -lt 0 -or $strIdx -le $recIdx) {
    throw 'Bootstrap must write [AdvOut] Encoder right after RecEncoder'
}

}


[xml]$settingsUi = [IO.File]::ReadAllText(
    "$repo/UI/forms/OBSBasicSettings.ui")
$bitrateLabel = $settingsUi.SelectSingleNode(
    "//widget[@name='label_20']")
$bitrateWidth = $bitrateLabel.SelectSingleNode(
    "property[@name='minimumSize']/size/width")
$bitrateAlignment = $bitrateLabel.SelectSingleNode(
    "property[@name='alignment']/set")
if ($null -eq $bitrateWidth -or $bitrateWidth.InnerText -ne '170' -or
    $null -eq $bitrateAlignment -or
    $bitrateAlignment.InnerText -notmatch 'AlignRight') {
    throw 'Unified audio bitrate label is not aligned with Output Mode'
}

$nsi = [IO.File]::ReadAllText("$repo/UI/installer/obspt-setup.nsi")
foreach ($pattern in @(
        'Page custom ConfigPageCreate ConfigPageLeave',
        'Call NeedsConfigReset',
        'MessageBox MB_YESNO|MB_ICONEXCLAMATION',
        'obspt-preserve-config.ps1',
        '!define CONFIG_GENERATION "3"',
        'ReadINIStr $0 "$INSTDIR\obs-studio\global.ini" "OBSPT" "BootstrapVersion"',
        '$0 < ${CONFIG_GENERATION}',
        'Sleep 2000')) {
    if (-not $nsi.Contains($pattern)) {
        throw "Installer reset contract is missing: $pattern"
    }
}
$retryCount = ([regex]::Matches($nsi, [regex]::Escape('nsExec::ExecToLog'))).Count
if ($retryCount -lt 2) {
    throw 'Installer merge must retry once before declaring failure'
}
if ($nsi.Contains('IfFileExists "$INSTDIR\obs-studio\*.*" show_config_reset 0')) {
    throw 'Reset page must be gated by generation, not by directory existence'
}
# Issue 047: program File /r must not blanket-copy obs-studio (the staged
# User Data Root templates) over an existing config. Program dirs are copied
# explicitly; obs-studio templates deploy only when the dir is absent (fresh /
# legacy-after-rename / post-force-reset).
if ($nsi.Contains('File /r "${SOURCE_DIR}\*.*"')) {
    throw 'Upgrade must not blanket-copy obs-studio over user config'
}
foreach ($pattern in @('File /r "${SOURCE_DIR}\bin"',
                      'File /r "${SOURCE_DIR}\data"',
                      'File /r "${SOURCE_DIR}\obs-plugins"',
                      'IfFileExists "$INSTDIR\obs-studio" 0 deploy_obs_studio_templates',
                      'deploy_obs_studio_templates:',
                      'File /r "${SOURCE_DIR}\obs-studio"')) {
    if (-not $nsi.Contains($pattern)) {
        throw "Installer obs-studio deploy contract is missing: $pattern"
    }
}
# control: only on an existing current-gen User Data Root, default unchecked,
# confirm dialog on check, backup to obs-studio.__obspt_backup.
foreach ($pattern in @('Var ForceResetChecked',
                      'ForceResetPageCreate',
                      'ForceResetPageLeave',
                      'Page custom ForceResetPageCreate ForceResetPageLeave',
                      'NSD_CreateCheckBox',
                      'OBSPT_FRCheckbox',
                      'OBSPT_FRConfirmMsg')) {
    if (-not $nsi.Contains($pattern)) {
        throw "Installer force-reset contract is missing: $pattern"
    }
}
# Issue 050 safety: the CleanupLegacyResidue function body must never
# remove the User Data Root (obs-studio) or the output folder (recordings).
$cleanupStart = $nsi.IndexOf('Function CleanupLegacyResidue')
$cleanupEnd = $nsi.IndexOf('FunctionEnd', $cleanupStart)
if ($cleanupStart -lt 0 -or $cleanupEnd -lt 0) {
    throw 'CleanupLegacyResidue function not found'
}
$cleanupBody = $nsi.Substring($cleanupStart, $cleanupEnd - $cleanupStart)
if ($cleanupBody -match 'RMDir.*obs-studio|Delete.*obs-studio|RMDir.*recordings|Delete.*recordings') {
    throw 'Residue cleanup must never touch obs-studio or recordings'
}
if ($nsi -notmatch '(?i)__obspt_backup') {
    throw 'Force-reset must back up to obs-studio.__obspt_backup'
}

# Issue 050: installer launch paths point at the flat exe and legacy
# residue is cleaned by an allow-list routine that never touches the User
# Data Root (obs-studio) or the output folder (recordings).
if ($nsi -notmatch [regex]::Escape('!define EXE_PATH "bin\OBS-PT.exe"')) {
    throw 'Installer EXE_PATH must be the flat bin\OBS-PT.exe'
}
foreach ($pattern in @('Function CleanupLegacyResidue',
                      'Call CleanupLegacyResidue',
                      'RMDir /r "$INSTDIR\obs-plugins\64bit"',
                      'RMDir /r "$INSTDIR\64bit"',
                      'RMDir /r "$INSTDIR\bin\64bit"')) {
    if (-not $nsi.Contains($pattern)) {
        throw "Installer residue-cleanup contract is missing: $pattern"
    }
}
# Issue 049: flat layout - no bin/64bit or obs-plugins/64bit anywhere in
# the runtime/install chain (OBS-PT is permanently x64).
$obsWindows = [IO.File]::ReadAllText("$repo/libobs/obs-windows.c")
if ($obsWindows -notmatch [regex]::Escape('"../obs-plugins"')) {
    throw 'libobs module path must be flat (../obs-plugins)'
}
if ($obsWindows -match '64bit') {
    throw 'libobs must not reference a 64bit folder'
}
$mainCpp = [IO.File]::ReadAllText("$repo/UI/window-basic-main.cpp")
if ($mainCpp -match 'bin/64bit|bin/32bit') {
    throw 'Main window module path must be flat (bin)'
}
if ($mainCpp -notmatch [regex]::Escape('path + "/bin"')) {
    throw 'Main window must add module path /bin'
}

# Issue 048: default-setting policy audit. RunAsAdministrator is the sole
# force-override member, expressed as a config default (never written by
# the bootstrap), so a user-set value survives every launch and upgrade and
# is reset only by the force-reset install. Record paths are preset
# (absent/placeholder-only via OBSPTPlanRecordPaths). No bootstrap write may
# clobber an existing user value on a normal launch.
$bootstrap = [IO.File]::ReadAllText("$repo/UI/obspt-bootstrap.cpp")
if ($bootstrap -match 'config_set_(bool|int|uint|string)\([^;]*\"RunAsAdministrator\"') {
    throw 'Bootstrap must not write RunAsAdministrator (it is preset-only)'
}
if ($bootstrap -notmatch [regex]::Escape('OBSPTPlanRecordPaths')) {
    throw 'Record-path default fills must be preset (OBSPTPlanRecordPaths)'
}
$obsapp = [IO.File]::ReadAllText("$repo/UI/obs-app.cpp")
if ($obsapp -notmatch 'config_set_default_bool[^;]*\"RunAsAdministrator\"') {
    throw 'RunAsAdministrator must be a config default (force-override member)'
}

[pscustomobject]@{
    Result = 'PASS'
    Preserved = @('stream', 'recording paths', 'hotkeys', 'replay buffer')
    Reset = @('scene layout', 'audio routing', 'video', 'record encoder')
    WorkRoot = $work
} | ConvertTo-Json -Depth 4
