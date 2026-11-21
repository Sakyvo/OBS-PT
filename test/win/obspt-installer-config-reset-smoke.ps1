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

# Issue 053: hide the legacy themes (Acri, Rachni, System) from the combo
# behind the Advanced-page 'debug' backdoor; if a user already has one
# selected, fall back to Dark at startup and persist the change.
$settingsCpp = [IO.File]::ReadAllText("$repo/UI/window-basic-settings.cpp")
foreach ($hidden in @('"Acri"', '"Rachni"', '"System"')) {
    if (-not $settingsCpp.Contains($hidden)) {
        throw "Settings theme list must include $hidden in the hidden set"
    }
}
if (-not $settingsCpp.Contains('revealHiddenThemes')) {
    throw "Settings theme list skip must be gated by revealHiddenThemes"
}
if ($settingsCpp.Contains("if (name == `"Acri`")`r`n`t`t`tcontinue;") -or
    $settingsCpp.Contains("if (name == `"Acri`")`n`t`t`tcontinue;")) {
    throw "Settings theme list must not use the old ungated Acri-only skip"
}
if (-not $settingsCpp.Contains('debugKeyBuffer == "debug"') -or
    -not $settingsCpp.Contains('ui->settingsPages->currentWidget() == ui->advancedPage')) {
    throw "Settings must gate reveal on lowercase 'debug' typed on the Advanced page"
}
$settingsHpp = [IO.File]::ReadAllText("$repo/UI/window-basic-settings.hpp")
if (-not $settingsHpp.Contains('bool revealHiddenThemes = false;')) {
    throw "revealHiddenThemes must default to false (per-dialog lifetime)"
}
$obsAppCpp = [IO.File]::ReadAllText("$repo/UI/obs-app.cpp")
foreach ($legacy in @('Acri', 'Rachni', 'System')) {
    if (-not $obsAppCpp.Contains("strcmp(themeName, `"$legacy`")")) {
        throw "InitTheme must map $legacy to Dark"
    }
}
if (-not $obsAppCpp.Contains('themeName = "Dark"') -or
    -not $obsAppCpp.Contains('config_set_string(globalConfig, "General", "CurrentTheme2", "Dark")')) {
    throw "InitTheme must map the hidden themes to Dark and persist the change"
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


# Issue 066: the shipped version and the shipped default layout.
$nsi = [IO.File]::ReadAllText("$repo/UI/installer/obspt-setup.nsi")
if ($nsi -match 'OBS-PT 1\.[0-9]+\.[0-9]+') {
    throw 'Installer user-facing strings must use ${APP_VERSION}, not a literal version'
}
if (-not $nsi.Contains('${APP_VERSION} requires the new unified audio configuration') -and
    -not $nsi.Contains('${APP_VERSION} 需要新的统一音频配置')) {
    throw 'Installer reset page must interpolate ${APP_VERSION}'
}

$agents = [IO.File]::ReadAllText("$repo/AGENTS.md")
if ($agents -notmatch 'OBS_VERSION_OVERRIDE:STRING=1\.3\.0') {
    throw 'Documented configure command must build version 1.3.0'
}

# The packaged DockState is the maintainer layout: stats + preview on top,
# audio routing / mixer / scenes+sources / controls below. It must decode and
# carry those docks, and no longer be a hand-authored placeholder.
$packagedDefaultIni = "$repo/UI/data/obspt-defaults/obs-studio/global.ini"
$defaultsIni = [IO.File]::ReadAllText($packagedDefaultIni)
$dockMatch = [regex]::Match($defaultsIni, '(?m)^DockState=(.+)$')
if (-not $dockMatch.Success) {
    throw 'Packaged defaults have no [BasicWindow] DockState'
}
$dockBytes = [Convert]::FromBase64String($dockMatch.Groups[1].Value.Trim())
# Qt stores dock names as UTF-16 at arbitrary offsets in the blob, and byte
# order can shift between Qt builds; compare hex so neither alignment nor
# endianness can produce a false failure.
$dockHex = ($dockBytes | ForEach-Object { $_.ToString('x2') }) -join ''
foreach ($dock in @('statsDoc', 'audioRoutingDoc', 'mixerDoc', 'scenesDock',
                    'sourcesDoc', 'controlsDoc', 'transitionsDock')) {
    $le = -join ($dock.ToCharArray() | ForEach-Object {
        '{0:x2}00' -f [int][char]$_ })
    $be = -join ($dock.ToCharArray() | ForEach-Object {
        '00{0:x2}' -f [int][char]$_ })
    if (-not $dockHex.Contains($le) -and -not $dockHex.Contains($be)) {
        throw "Packaged default DockState is missing dock: $dock"
    }
}
# A shipped default must also carry window geometry so the regions land at
# their intended sizes on first run.
if (-not [regex]::IsMatch($defaultsIni, '(?m)^geometry=.+$')) {
    throw 'Packaged defaults have no window geometry'
}

# Issue 064 / ADR-0006: Setup never terminates or blocks on user
# applications. The virtual camera filter DLLs can be mapped inside a
# browser for its whole lifetime; the payload copy runs with
# `SetOverwrite try` so such a file is skipped silently instead of raising
# NSIS's overwrite dialog, and an in-use probe only records what was kept.
$nsi = [IO.File]::ReadAllText("$repo/UI/installer/obspt-setup.nsi")
foreach ($pattern in @('SetOverwrite try', 'Function ReportLockedVirtualCam',
                      'Call ReportLockedVirtualCam')) {
    if (-not $nsi.Contains($pattern)) {
        throw "Locked-file skip contract is missing: $pattern"
    }
}
foreach ($forbidden in @('taskkill', 'Stop-Process', 'terminate-vcam',
                        'TerminateVirtualCamHolders', 'SetErrorLevel 2',
                        'CheckVirtualCamInUse')) {
    if ($nsi.Contains($forbidden)) {
        throw "Setup must not terminate or block on user applications: $forbidden"
    }
}
$vcamProbe = [regex]::Match($nsi, '(?s)Function ReportLockedVirtualCam.*?FunctionEnd').Value
if ($vcamProbe -match 'MessageBox') {
    throw 'The in-use probe must report via the install log, never a prompt'
}

# Virtual-camera registration contract: both architectures are declared and
# the two regsvr32 calls must be independent (`&`), never chained (`&&`) -
# a chained command aborts the 64-bit registration whenever the 32-bit
# module is absent, which silently ships an unregistered filter.
$register = [regex]::Match($nsi,'(?s)Function RegisterVirtualCam.*?FunctionEnd').Value
if ($register -notmatch 'obs-virtualcam-module32\.dll' -or
    $register -notmatch 'obs-virtualcam-module64\.dll') {
    throw 'RegisterVirtualCam must register both module architectures'
}
if ($register -match '&&') {
    throw 'Registration must not chain regsvr32 calls with && (a missing module would skip the other)'
}
if ($nsi -notmatch 'obs-virtualcam-module32\.dll is missing from staging') {
    throw 'Packaging must fail loudly when the 32-bit virtual-camera module is not staged'
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

# Issue 066: recording defaults to a hardware HEVC encoder; streaming stays
# H.264 (HEVC push compatibility is a separate, codec-flagged path).
if ($bootstrap -notmatch 'probe_record_encoder\(\)' -or
    $bootstrap -notmatch 'probe_h264_record_encoder\(\)') {
    throw 'Bootstrap must probe recording and streaming encoders separately'
}
# The streaming slot must be fed from the H.264 probe, not the recording one.
$streamRegion = $bootstrap.Substring([Math]::Max(0, $strIdx - 600), 700)
if ($streamRegion.IndexOf('probe_h264_record_encoder()') -lt 0 -or
    $streamRegion.IndexOf('stream_enc.encoder_id') -lt 0) {
    throw 'The streaming encoder must come from the H.264 probe'
}
if ($bootstrap -notmatch 'obspt_recording_hevc_preference') {
    throw 'The recording HEVC preference list must drive the recording probe'
}

$modelCpp = [IO.File]::ReadAllText("$repo/UI/obspt-bootstrap-model.cpp")
$modelHeader = [IO.File]::ReadAllText("$repo/UI/obspt-bootstrap-model.h")
$bootstrapHeader = [IO.File]::ReadAllText("$repo/UI/obspt-bootstrap.h")
if ($modelHeader -notmatch 'OBSPT_BOOTSTRAP_VERSION 6') {
    throw 'The keyint/streaming default change is a v6 repair'
}
# Exactly one definition: a second copy silently drifts and makes a repair
# that claims to have bumped the version never run.
if ($bootstrapHeader -match '#define\s+OBSPT_BOOTSTRAP_VERSION') {
    throw 'OBSPT_BOOTSTRAP_VERSION must be defined once (bootstrap-model.h), not duplicated'
}
if ($modelCpp -match '#define\s+OBSPT_BOOTSTRAP_VERSION') {
    throw 'OBSPT_BOOTSTRAP_VERSION must be defined in the header, not the .cpp'
}
foreach ($id in @('jim_hevc_nvenc', 'h265_texture_amf')) {
    if ($modelCpp -notmatch [regex]::Escape($id)) {
        throw "Recording HEVC preference is missing encoder: $id"
    }
}

# Issue 073: recording keyframe interval is 0 (encoder-chosen) in every
# encoder template, and streaming is CBR at a fixed target bitrate.
$bootstrapSrc = [IO.File]::ReadAllText("$repo/UI/obspt-bootstrap.cpp")
if ($bootstrapSrc -match 'obs_data_set_int\(root, "keyint_sec", 2\)') {
    throw 'A recording template still hardcodes keyint_sec 2'
}
if ($bootstrapSrc -notmatch 'obspt_recording_keyint_sec\(\)') {
    throw 'Recording templates must take the keyframe interval from the model'
}
if ($bootstrapSrc -notmatch 'obspt_streaming_bitrate_kbps\(\)') {
    throw 'The streaming seed must take its bitrate from the model'
}
if ($bootstrapSrc -notmatch 'apply_stream_encoder_to_profile') {
    throw 'Bootstrap must write streamEncoder.json under the repair gate'
}
if ($modelCpp -notmatch 'obspt_recording_keyint_sec[\s\S]{0,80}return 0;') {
    throw 'obspt_recording_keyint_sec must return 0'
}
if ($modelCpp -notmatch 'obspt_streaming_bitrate_kbps[\s\S]{0,80}return 4500;') {
    throw 'obspt_streaming_bitrate_kbps must return 4500'
}

# Java Detection ships lenient, as a preset default (never overwrites a
# user's explicit 严格).
$obsApp = [IO.File]::ReadAllText("$repo/UI/obs-app.cpp")
if ($obsApp -notmatch 'config_set_default_bool\(globalConfig, "General",\s*"JavaDetectionLenient",\s*true\)') {
    throw 'Java Detection must default to lenient via a preset default'
}
if ($obsApp -match 'config_set_bool\([^;]*"JavaDetectionLenient"') {
    throw 'Java Detection must never be force-written over a user value'
}

# The shipped seed must agree with the bootstrap template, or fresh installs
# and repaired installs would disagree.
$packagedRecordEncoder = Get-Content -Raw `
    "$repo/UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/recordEncoder.json" |
    ConvertFrom-Json
if ($packagedRecordEncoder.encoder -ne 'jim_hevc_nvenc') {
    throw 'Packaged recordEncoder.json must seed the HEVC recording encoder'
}
if ($packagedRecordEncoder.profile -ne 'main') {
    throw 'HEVC recording seed must use the main profile'
}
if ($packagedProfile -notmatch '(?m)^RecEncoder=jim_hevc_nvenc$') {
    throw 'Packaged profile must select the HEVC recording encoder'
}
if ($packagedProfile -match '(?m)^Encoder=') {
    throw 'Packaged profile must not pin a streaming encoder'
}
if ($packagedRecordEncoder.keyint_sec -ne 0) {
    throw 'Packaged recordEncoder.json must seed keyint_sec 0'
}

# The shipped streaming seed must be CBR at the model's target bitrate.
$packagedStreamEncoder = Get-Content -Raw `
    "$repo/UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/streamEncoder.json" |
    ConvertFrom-Json
if ($packagedStreamEncoder.rate_control -ne 'CBR') {
    throw 'Packaged streamEncoder.json must seed CBR'
}
if ($packagedStreamEncoder.bitrate -ne 4500) {
    throw 'Packaged streamEncoder.json must seed 4500 Kbps'
}

# The shipped global.ini must carry the lenient Java Detection default so a
# fresh portable tree matches the compiled-in default.
$packagedGlobal = [IO.File]::ReadAllText(
    "$repo/UI/data/obspt-defaults/obs-studio/global.ini")
if ($packagedGlobal -notmatch '(?m)^JavaDetectionLenient=true\r?$') {
    throw 'Packaged global.ini must seed lenient Java Detection'
}

# --- Issue 075 / ADR 0009: installer-driven default replacement ---

# The marker axis must be its own constant, defined exactly once, and must
# not be BootstrapVersion (whose repair cargo forces PotPvP and FirstRun).
if ($modelHeader -notmatch 'OBSPT_INSTALLER_MIGRATION_VERSION 1') {
    throw 'The installer-driven replacement must declare its own version constant'
}
if ($bootstrapHeader -match '#define\s+OBSPT_INSTALLER_MIGRATION_VERSION' -or
    $modelCpp -match '#define\s+OBSPT_INSTALLER_MIGRATION_VERSION') {
    throw 'OBSPT_INSTALLER_MIGRATION_VERSION must be defined once, in the model header'
}
if ($modelCpp -notmatch 'obspt_installer_migration_needed') {
    throw 'The installer-migration decision must live in the testable model'
}

# Application side: the replacement runs under its own flag, replaces the
# dock layout from the packaged defaults, and leaves BootstrapVersion alone.
if ($bootstrapSrc -notmatch 'g_installer_migration_needed') {
    throw 'The bootstrap must track the installer migration flag'
}
if ($bootstrapSrc -notmatch 'apply_packaged_default_layout') {
    throw 'The migration must replace the dock layout from the packaged defaults'
}
if ($bootstrapSrc -notmatch 'GetDataFilePath\("obspt-defaults/obs-studio/global\.ini"') {
    throw 'The migration must read the packaged default layout, not a hardcoded blob'
}
# DockState only - the user's window size is theirs.
$layoutRegion = $bootstrapSrc.Substring(
    [Math]::Max(0, $bootstrapSrc.IndexOf('apply_packaged_default_layout')))
if ($layoutRegion -notmatch '"BasicWindow", "DockState"') {
    throw 'The layout replacement must target [BasicWindow] DockState'
}
if ($layoutRegion -match '"BasicWindow", "geometry"') {
    throw 'The layout replacement must not touch [BasicWindow] geometry'
}
# The migration must never write BootstrapVersion - that axis drags in the
# v5 collateral (forcing PotPvP and FirstRun).
$migrationIdx = $bootstrapSrc.IndexOf('g_installer_migration_needed) {')
$migrationEnd = $bootstrapSrc.IndexOf('g_late_bootstrap_needed = false;', $migrationIdx)
if ($migrationIdx -lt 0 -or $migrationEnd -lt 0) {
    throw 'Could not isolate the installer-migration block'
}
$migrationBlock = $bootstrapSrc.Substring($migrationIdx, $migrationEnd - $migrationIdx)
if ($migrationBlock -match 'BootstrapVersion') {
    throw 'The installer migration must not write BootstrapVersion'
}
if ($migrationBlock -match '"Basic",\s*"Profile') {
    throw 'The installer migration must not switch the active Profile'
}
if ($migrationBlock -notmatch 'config_set_bool\(global_config, "General",\s*"JavaDetectionLenient", true\)') {
    throw 'The migration must force lenient Java Detection'
}
if ($migrationBlock -notmatch 'InstallerMigrationVersion') {
    throw 'The migration must record its own completion marker'
}

# Installer side: the trigger reads the previously installed product version
# and delegates the ini edit to the helper.
foreach ($pattern in @(
        '!include "WordFunc.nsh"',
        '!insertmacro VersionCompare',
        'Function MarkInstallerMigration',
        'Call MarkInstallerMigration',
        'ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "DisplayVersion"',
        '!define MIGRATION_RELEASE "1.2.0"',
        '${VersionCompare} $0 "${MIGRATION_RELEASE}" $1',
        'obspt-mark-installer-migration.ps1',
        'InstallerMigrationVersion',
        '${MIGRATION_PENDING}',
        '${MIGRATION_DONE}')) {
    if (-not $nsi.Contains($pattern)) {
        throw "Installer migration contract is missing: $pattern"
    }
}
# The marker must be decided before DisplayVersion is overwritten.
$markCall = $nsi.IndexOf('Call MarkInstallerMigration')
$regWrite = $nsi.IndexOf('WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion"')
if ($markCall -lt 0 -or $regWrite -lt 0 -or $regWrite -lt $markCall) {
    throw 'The migration marker must be decided before DisplayVersion is rewritten'
}
# The installer must never write BootstrapVersion.
if ($nsi -match 'WriteINIStr[^\n]*BootstrapVersion' -or
    $nsi -match 'BootstrapVersion[^\n]*InstallerMigrationVersion') {
    throw 'The installer must not write BootstrapVersion'
}
# Silent by contract: no prompt, and never fatal.
$markFnStart = $nsi.IndexOf('Function MarkInstallerMigration')
$markFnEnd = $nsi.IndexOf('FunctionEnd', $markFnStart)
$markFn = $nsi.Substring($markFnStart, $markFnEnd - $markFnStart)
if ($markFn -match 'MessageBox') {
    throw 'The installer migration must not prompt'
}
if ($markFn -match 'Abort') {
    throw 'The installer migration must never abort the installation'
}

# The helper must exist and preserve the rest of the file.
$markHelper = "$repo/UI/installer/obspt-mark-installer-migration.ps1"
if (-not (Test-Path -LiteralPath $markHelper)) {
    throw 'The installer migration helper is missing'
}
$markSrc = [IO.File]::ReadAllText($markHelper)
foreach ($pattern in @('InstallerMigrationVersion', 'ValidateSet', 'exit 0')) {
    if ($markSrc -notmatch [regex]::Escape($pattern)) {
        throw "Installer migration helper is missing: $pattern"
    }
}

# Behavioural: a marker write inserts exactly one line and changes nothing
# else, keeping the UTF-8 BOM and the file's own line ending.
$markFixture = Join-Path $work 'marker'
New-Item -ItemType Directory -Force -Path $markFixture | Out-Null
foreach ($flavour in @('crlf', 'lf')) {
    $src = "$repo/UI/data/obspt-defaults/obs-studio/global.ini"
    $text = [IO.File]::ReadAllText($src)
    if ($flavour -eq 'lf') { $text = $text.Replace("`r`n", "`n") }
    $fixture = Join-Path $markFixture "$flavour.ini"
    [IO.File]::WriteAllText($fixture, $text, (New-Object System.Text.UTF8Encoding $true))
    $before = [IO.File]::ReadAllText($fixture)
    & $markHelper -IniPath $fixture -Value 0 | Out-Null
    $after = [IO.File]::ReadAllText($fixture)

    # Line-ending blind: a CRLF file leaves a trailing CR on every split
    # element, which must not decide whether the marker is present.
    $added = @($after -split "`n" | Where-Object {
            $_.TrimEnd("`r") -match '^InstallerMigrationVersion=' })
    if ($added.Count -ne 1 -or $added[0].TrimEnd("`r") -ne 'InstallerMigrationVersion=0') {
        throw "The marker helper must add exactly one marker line ($flavour)"
    }
    $stripped = @($after -split "`n" | Where-Object {
            $_.TrimEnd("`r") -notmatch '^InstallerMigrationVersion=' })
    $expected = @($before -split "`n")
    if (@(Compare-Object $stripped $expected -SyncWindow 0).Count -ne 0) {
        throw "The marker helper must change nothing but the marker ($flavour)"
    }
    $bytes = [IO.File]::ReadAllBytes($fixture)
    if (-not ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)) {
        throw "The marker helper must preserve the UTF-8 BOM ($flavour)"
    }
    $crlfAfter = ([regex]::Matches($after, "`r`n")).Count
    if ($flavour -eq 'crlf' -and $crlfAfter -ne 55) {
        throw 'The marker helper must keep CRLF endings'
    }
    if ($flavour -eq 'lf' -and $crlfAfter -ne 0) {
        throw 'The marker helper must keep LF endings'
    }
    if (-not $after.EndsWith("`n")) {
        throw "The marker helper must keep a trailing newline ($flavour)"
    }
    if ($after -notmatch 'InstallerMigrationVersion=0\r?\n') {
        throw "The marker line must be newline-terminated ($flavour)"
    }
}
# A missing file must be a silent no-op, never a failure.
& $markHelper -IniPath (Join-Path $markFixture 'absent.ini') -Value 1
if (-not $?) {
    throw 'The marker helper must tolerate a missing configuration file'
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

# Build-output contract must describe what the pipeline actually does: the
# installer lands in .target/.distribution/installer/ (NSIS OutFile), the tree is flat, and
# the packaging guard for the Win32 virtual-camera module is documented.
$contract = [IO.File]::ReadAllText("$repo/.docs/spec/build-output.md")
if ($contract -notmatch [regex]::Escape('.target/.distribution/installer/OBS-PT-<version>-Installer.exe')) {
    throw 'Build-output contract must name the real installer output path'
}
if ($contract -notmatch [regex]::Escape('_vcam32')) {
    throw 'Build-output contract must document the Win32 virtual-camera lane'
}
if ($contract -match 'runnable form is\s+`\.target/_pkg/OBS-PT/bin/64bit') {
    throw 'Build-output contract still claims the removed bin/64bit layout'
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
