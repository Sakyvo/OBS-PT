param(
    [ValidateSet('blank', 'game', 'display')]
    [string]$Mode = 'blank',
    [ValidateSet('all', 'pre_amf', 'amd_era', 'current')]
    [string]$Build = 'all',
    [ValidateSet('Normal', 'AboveNormal', 'High')]
    [string]$Priority = 'Normal',
    [ValidateRange(10, 300)]
    [int]$DurationSeconds = 30,
    [ValidateRange(1, 5)]
    [int]$Repeats = 2
)

$ErrorActionPreference = 'Stop'
$repo = 'K:/Projects/dev/OBS-PT'
$task = "$repo/.trellis/tasks/07-13-high-fps-render-delay-regression"
$installedConfig = 'D:/OBS-PT/obs-studio'
$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$runRoot = "$repo/build-v143/_perf-ab/$runId-$Mode"

$builds = [ordered]@{
    pre_amf = "$repo/build-v143/_hybrid-smoke-fixed-default"
    amd_era = "$repo/build-v143/_amd-smoke-20260712-113151"
    current = "$repo/build-v143/_pkg/OBS-PT"
}
if ($Build -ne 'all') {
    $selectedBuild = [ordered]@{}
    $selectedBuild.Add($Build, $builds[$Build])
    $builds = $selectedBuild
}

function Invoke-Robocopy {
    param([string]$Source, [string]$Destination, [string[]]$ExcludedDirectories)

    $args = @($Source, $Destination, '/E', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    if ($ExcludedDirectories.Count -gt 0) {
        $args += '/XD'
        $args += $ExcludedDirectories
    }
    & robocopy @args | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy failed ($LASTEXITCODE): $Source -> $Destination"
    }
}

function Set-IniValue {
    param(
        [string]$Path,
        [string]$Section,
        [string]$Key,
        [string]$Value
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($line in [System.IO.File]::ReadAllLines($Path)) {
        [void]$lines.Add($line)
    }

    $sectionLine = "[$Section]"
    $sectionIndex = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq $sectionLine) {
            $sectionIndex = $i
            break
        }
    }

    if ($sectionIndex -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ne '') {
            [void]$lines.Add('')
        }
        [void]$lines.Add($sectionLine)
        [void]$lines.Add("$Key=$Value")
    } else {
        $nextSection = $lines.Count
        $keyIndex = -1
        for ($i = $sectionIndex + 1; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^\[.+\]$') {
                $nextSection = $i
                break
            }
            if ($lines[$i] -match "^$([regex]::Escape($Key))=") {
                $keyIndex = $i
                break
            }
        }
        if ($keyIndex -ge 0) {
            $lines[$keyIndex] = "$Key=$Value"
        } else {
            $lines.Insert($nextSection, "$Key=$Value")
        }
    }

    [System.IO.File]::WriteAllLines(
        $Path,
        $lines,
        [System.Text.UTF8Encoding]::new($false))
}

function Set-SceneMode {
    param([string]$Path, [string]$SelectedMode)

    $sceneData = [System.IO.File]::ReadAllText($Path) | ConvertFrom-Json
    $scene = $sceneData.sources | Where-Object { $_.id -eq 'scene' -and $_.name -eq 'PotPvP' }
    if (-not $scene) {
        throw "PotPvP scene missing from $Path"
    }

    foreach ($item in $scene.settings.items) {
        if ($item.name -eq 'Minecraft') {
            $item.visible = $SelectedMode -eq 'game'
        } elseif ($item.name -eq 'Display') {
            $item.visible = $SelectedMode -eq 'display'
        }
    }

    $timer = $sceneData.modules.'output-timer'
    if (-not $timer) {
        throw "Output Timer settings missing from $Path"
    }
    $timer.recordTimerHours = 0
    $timer.recordTimerMinutes = 0
    $timer.recordTimerSeconds = $DurationSeconds
    $timer.autoStartRecordTimer = $true
    $timer.pauseRecordTimer = $false

    $json = $sceneData | ConvertTo-Json -Depth 100 -Compress
    [System.IO.File]::WriteAllText(
        $Path,
        $json,
        [System.Text.UTF8Encoding]::new($false))
}

function Prepare-BuildCopy {
    param([string]$Name, [string]$Source)

    $destination = "$runRoot/builds/$Name"
    $sourceNative = (Resolve-Path $Source).Path
    $configNative = (Resolve-Path $installedConfig).Path
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Invoke-Robocopy $Source $destination @(
        "$sourceNative\obs-studio",
        "$sourceNative\recordings")
    Invoke-Robocopy $installedConfig "$destination/obs-studio" @(
        "$configNative\logs",
        "$configNative\profiler_data",
        "$configNative\crashes")

    if (-not (Test-Path "$destination/bin/64bit/OBS-PT.exe") -or
        -not (Test-Path "$destination/data/obs-studio/locale/en-US.ini")) {
        throw "Incomplete application copy: $destination"
    }

    $output = "$destination/recordings"
    New-Item -ItemType Directory -Path $output -Force | Out-Null

    $global = "$destination/obs-studio/global.ini"
    $basic = "$destination/obs-studio/basic/profiles/PotPvP/basic.ini"
    $scene = "$destination/obs-studio/basic/scenes/PotPvP.json"
    Set-IniValue $global 'General' 'EnableAutoUpdates' 'false'
    Set-IniValue $global 'General' 'ProcessPriority' $Priority
    Set-IniValue $global 'General' 'ConfirmOnExit' 'false'
    Set-IniValue $global 'General' 'FirstRun' 'true'
    Set-IniValue $global 'OBSPT' 'BootstrapVersion' '3'
    Set-IniValue $global 'OBSPT' 'FirstRunCompleted' 'true'
    Set-IniValue $basic 'General' 'OpenStatsOnStartup' 'false'
    Set-IniValue $basic 'AdvOut' 'RecFilePath' $output
    Set-SceneMode $scene $Mode

    return $destination
}

function Wait-ForRecordingStart {
    param([string]$LogDirectory, [System.Diagnostics.Process]$Process)

    $deadline = (Get-Date).AddSeconds(60)
    do {
        Start-Sleep -Milliseconds 250
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "OBS-PT exited before recording started: $($Process.ExitCode)"
        }
        $log = Get-ChildItem -LiteralPath $LogDirectory -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($log) {
            $content = Get-Content -Raw -LiteralPath $log.FullName -ErrorAction SilentlyContinue
            if ($content -match '==== Recording Start') {
                return $log.FullName
            }
        }
    } while ((Get-Date) -lt $deadline)

    throw 'Timed out waiting for recording start'
}

function Run-One {
    param([string]$BuildName, [int]$Iteration, [string]$Root)

    $exe = "$Root/bin/64bit/OBS-PT.exe"
    $workDir = "$Root/bin/64bit"
    $logDir = "$Root/obs-studio/logs"
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null

    $started = Get-Date
    $arguments = @(
        '--startrecording',
        '--profile', 'PotPvP',
        '--collection', 'PotPvP',
        '--scene', 'PotPvP',
        '--minimize-to-tray',
        '--disable-updater',
        '--disable-missing-files-check')
    $process = Start-Process -FilePath $exe -ArgumentList $arguments `
        -WorkingDirectory $workDir -WindowStyle Hidden -PassThru

    try {
        $logPath = Wait-ForRecordingStart $logDir $process
        $recordingDeadline = (Get-Date).AddSeconds($DurationSeconds + 90)
        $recordingCompleteAt = $null
        do {
            Start-Sleep -Milliseconds 250
            $content = Get-Content -Raw -LiteralPath $logPath -ErrorAction SilentlyContinue
            $recordingComplete = $content -match
                '\[mp4 output: .+\] MP4 file output complete\.'
            $encodingMetricsComplete = $content -match
                'number of skipped frames due to encoding lag:'
            if ($recordingComplete -and -not $recordingCompleteAt) {
                $recordingCompleteAt = Get-Date
            }
            $process.Refresh()
            if ($process.HasExited -and -not $recordingComplete) {
                throw 'OBS-PT exited before completing the recording'
            }
        } while ((Get-Date) -lt $recordingDeadline -and
            (-not $recordingComplete -or
                (-not $encodingMetricsComplete -and
                    (Get-Date) -lt $recordingCompleteAt.AddSeconds(1))))
        if (-not $recordingComplete) {
            throw 'Timed out waiting for the recording to complete'
        }
    } catch {
        if (-not $process.HasExited) {
            $process.Kill()
            [void]$process.WaitForExit(10000)
        }
        throw
    }

    $terminatedAfterMetrics = $false
    $process.Refresh()
    if (-not $process.HasExited) {
        $terminatedAfterMetrics = $true
        $process.Kill()
        [void]$process.WaitForExit(10000)
    }

    $content = Get-Content -Raw -LiteralPath $logPath
    $render = [regex]::Match(
        $content,
        'Number of lagged frames due to rendering lag/stalls: (\d+) \(([0-9.]+)%\)')
    $encode = [regex]::Match(
        $content,
        'number of skipped frames due to encoding lag: (\d+)/(\d+) \(([0-9.]+)%\)')
    $gameCaptureHooked = $content -match
        "\[game-capture: 'Minecraft'\] (?:gl )?shared texture capture successful"

    $recording = Get-ChildItem "$Root/recordings" -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    [pscustomobject]@{
        Build = $BuildName
        Iteration = $Iteration
        Mode = $Mode
        Priority = $Priority
        Started = $started.ToString('o')
        DurationSeconds = $DurationSeconds
        RenderingLagPercent = if ($render.Success) {
            [double]$render.Groups[2].Value
        } else {
            0.0
        }
        EncodingLagPercent = if ($encode.Success) {
            [double]$encode.Groups[3].Value
        } else {
            0.0
        }
        EncodingSkipped = if ($encode.Success) {
            [int]$encode.Groups[1].Value
        } else {
            0
        }
        EncodingTotal = if ($encode.Success) {
            [int]$encode.Groups[2].Value
        } else {
            $null
        }
        GameCaptureHooked = if ($Mode -eq 'game') {
            $gameCaptureHooked
        } else {
            $null
        }
        TerminatedAfterMetrics = $terminatedAfterMetrics
        RecordingBytes = if ($recording) { $recording.Length } else { 0 }
        Log = $logPath
        Error = $null
    }
}

if (Get-Process -Name OBS-PT -ErrorAction SilentlyContinue) {
    throw 'Close every OBS-PT instance before running the A/B test'
}

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$prepared = @{}
foreach ($entry in $builds.GetEnumerator()) {
    $prepared[$entry.Key] = Prepare-BuildCopy $entry.Key $entry.Value
}

$baseOrder = @($builds.Keys)
$results = @()
for ($iteration = 1; $iteration -le $Repeats; $iteration++) {
    $order = if ($iteration % 2 -eq 1) {
        $baseOrder
    } else {
        $baseOrder[($baseOrder.Count - 1)..0]
    }
    foreach ($name in $order) {
        Write-Host "Running $name iteration $iteration ($Mode)..."
        try {
            $results += Run-One $name $iteration $prepared[$name]
        } catch {
            Write-Warning "$name iteration $iteration failed: $($_.Exception.Message)"
            $failureLog = Get-ChildItem "$($prepared[$name])/obs-studio/logs" -File `
                -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
            $failureContent = if ($failureLog) {
                Get-Content -Raw -LiteralPath $failureLog.FullName `
                    -ErrorAction SilentlyContinue
            } else {
                ''
            }
            $results += [pscustomobject]@{
                Build = $name
                Iteration = $iteration
                Mode = $Mode
                Priority = $Priority
                Started = (Get-Date).ToString('o')
                DurationSeconds = $DurationSeconds
                RenderingLagPercent = $null
                EncodingLagPercent = $null
                EncodingSkipped = $null
                EncodingTotal = $null
                GameCaptureHooked = if ($Mode -eq 'game') {
                    $failureContent -match
                        "\[game-capture: 'Minecraft'\] (?:gl )?shared texture capture successful"
                } else {
                    $null
                }
                TerminatedAfterMetrics = $null
                RecordingBytes = $null
                Log = if ($failureLog) { $failureLog.FullName } else { $null }
                Error = $_.Exception.Message
            }
        }
        $results | Export-Csv "$runRoot/results.csv" -NoTypeInformation
    }
}

$results | Format-Table Build, Iteration, GameCaptureHooked,
    RenderingLagPercent, EncodingLagPercent, EncodingSkipped, EncodingTotal,
    TerminatedAfterMetrics, RecordingBytes, Error -AutoSize
Write-Host "Results: $runRoot/results.csv"
