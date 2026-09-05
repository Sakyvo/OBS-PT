param(
    [Parameter(Mandatory = $true)]
    [string]$Recording,
    [string]$VlcPath =
        'K:/Projects/dev/OBS-PT/.target/_tools/vlc/extracted/vlc-3.0.23/vlc.exe'
)

$ErrorActionPreference = 'Stop'
$recordingPath = (Resolve-Path $Recording).Path
$vlc = (Resolve-Path $VlcPath).Path
$work = [IO.Path]::GetFullPath(
    'K:/Projects/dev/OBS-PT/.target/_tdd/player-compatibility')
New-Item -ItemType Directory -Path $work -Force | Out-Null

$probe = & ffprobe -v error `
    -show_entries `
        'stream=index,codec_type,codec_name,sample_rate,channels:stream_tags=name:stream_disposition=default:format=duration' `
    -of json $recordingPath | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) {
    throw 'ffprobe could not inspect the compatibility recording'
}
$audio = @($probe.streams | Where-Object codec_type -eq 'audio')
if ($audio.Count -ne 3) {
    throw "Expected three audio streams, found $($audio.Count)"
}
$expectedNames = @('Mix', 'Game', 'Music')
for ($i = 0; $i -lt $audio.Count; ++$i) {
    if ($audio[$i].tags.name -ne $expectedNames[$i]) {
        throw "Audio stream $i is '$($audio[$i].tags.name)', expected '$($expectedNames[$i])'"
    }
    $expectedDefault = if ($i -eq 0) { 1 } else { 0 }
    if ($audio[$i].disposition.default -ne $expectedDefault) {
        throw "Audio stream $i has an invalid default disposition"
    }
}

Add-Type -AssemblyName System.Windows.Forms
$player = New-Object -ComObject WMPlayer.OCX
try {
    $player.settings.volume = 0
    $player.settings.mute = $true
    $player.URL = $recordingPath
    $player.controls.play()
    $deadline = (Get-Date).AddSeconds(20)
    do {
        [Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 50
        $windowsState = $player.playState
        $windowsDuration = $player.currentMedia.duration
    } while ($windowsState -notin @(3, 8) -and
             (Get-Date) -lt $deadline)
    if ($windowsState -notin @(3, 8) -or $windowsDuration -le 0 -or
        $player.error.errorCount -ne 0) {
        throw 'Windows Media Player did not decode the recording'
    }
} finally {
    $player.controls.stop()
    $player.close()
    [void][Runtime.InteropServices.Marshal]::ReleaseComObject($player)
}

$stdout = "$work/vlc.stdout.log"
$stderr = "$work/vlc.stderr.log"
$arguments = @(
    '--intf=dummy',
    '--no-video',
    '--aout=dummy',
    '--play-and-exit',
    '--no-repeat',
    '--no-loop',
    '--no-one-instance',
    '--no-qt-privacy-ask',
    '--no-media-library',
    '--ignore-config',
    '--verbose=2',
    "`"$recordingPath`"") -join ' '
$vlcProcess = Start-Process `
    -FilePath $vlc `
    -ArgumentList $arguments `
    -WorkingDirectory (Split-Path $vlc) `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -PassThru
$vlcTimeoutMilliseconds = [Math]::Max(
    30000,
    [int](([double]$probe.format.duration + 30) * 1000))
if (-not $vlcProcess.WaitForExit($vlcTimeoutMilliseconds)) {
    Stop-Process -Id $vlcProcess.Id -Force
    throw 'VLC did not exit after decoding the recording'
}
$vlcProcess.Refresh()
if ($vlcProcess.ExitCode -ne 0) {
    throw "VLC exited with code $($vlcProcess.ExitCode)"
}
$vlcLog = (Get-Content -Raw $stdout -ErrorAction SilentlyContinue) +
    (Get-Content -Raw $stderr -ErrorAction SilentlyContinue)
$enabledAudio = [regex]::Matches(
    $vlcLog, 'adding track\[Id 0x[0-9a-f]+\] audio \(enable\)',
    [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
$disabledAudio = [regex]::Matches(
    $vlcLog, 'adding track\[Id 0x[0-9a-f]+\] audio \(disable\)',
    [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
if ($enabledAudio -ne 1 -or $disabledAudio -ne 2 -or
    $vlcLog -notmatch 'using audio decoder module' -or
    $vlcLog -notmatch 'EOF reached') {
    throw 'VLC did not select exactly Mix and decode it to EOF'
}

[pscustomobject]@{
    Result = 'PASS'
    Recording = $recordingPath
    DurationSeconds = [double]$probe.format.duration
    Tracks = @($audio | ForEach-Object tags | ForEach-Object name)
    DefaultTrack = $audio[0].tags.name
    WindowsMediaPlayer = [pscustomobject]@{
        PlayState = $windowsState
        DurationSeconds = $windowsDuration
        Muted = $true
    }
    VLC = [pscustomobject]@{
        Path = $vlc
        ExitCode = $vlcProcess.ExitCode
        EnabledAudioTracks = $enabledAudio
        DisabledAudioTracks = $disabledAudio
        DecodedToEOF = $true
    }
} | ConvertTo-Json -Depth 6
