param(
    [Parameter(Mandatory = $true)]
    [string]$BackupRoot,

    [Parameter(Mandatory = $true)]
    [string]$TargetRoot
)

$ErrorActionPreference = 'Stop'

function New-OrderedMap {
    return New-Object System.Collections.Specialized.OrderedDictionary
}

function Read-Ini {
    param([Parameter(Mandatory = $true)][string]$Path)

    $data = New-OrderedMap
    $section = ''
    $data[$section] = New-OrderedMap

    if (-not (Test-Path -LiteralPath $Path)) {
        return $data
    }

    foreach ($line in [System.IO.File]::ReadAllLines($Path)) {
        if ($line -match '^\s*\[([^\]]+)\]\s*$') {
            $section = $matches[1]
            if (-not $data.Contains($section)) {
                $data[$section] = New-OrderedMap
            }
            continue
        }

        if ($line -match '^\s*([^=;#][^=]*?)=(.*)$') {
            $key = $matches[1].Trim()
            $value = $matches[2]
            $data[$section][$key] = $value
        }
    }

    return $data
}

function Write-Ini {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Data
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $firstSection = $true

    foreach ($section in $Data.Keys) {
        $values = $Data[$section]

        if ($section -eq '') {
            foreach ($key in $values.Keys) {
                $lines.Add("$key=$($values[$key])")
            }
            continue
        }

        if (-not $firstSection -or $lines.Count -gt 0) {
            $lines.Add('')
        }
        $firstSection = $false

        $lines.Add("[$section]")
        foreach ($key in $values.Keys) {
            $lines.Add("$key=$($values[$key])")
        }
    }

    $encoding = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllLines($Path, [string[]]$lines, $encoding)
}

function Copy-IniSection {
    param($Source, $Target, [string]$Section)

    if (-not $Source.Contains($Section)) {
        return
    }

    $copy = New-OrderedMap
    foreach ($key in $Source[$Section].Keys) {
        $copy[$key] = $Source[$Section][$key]
    }
    $Target[$Section] = $copy
}

function Copy-IniKeys {
    param($Source, $Target, [string]$Section, [string[]]$Keys)

    if (-not $Source.Contains($Section)) {
        return
    }
    if (-not $Target.Contains($Section)) {
        $Target[$Section] = New-OrderedMap
    }

    foreach ($key in $Keys) {
        if ($Source[$Section].Contains($key)) {
            $Target[$Section][$key] = $Source[$Section][$key]
        }
    }
}

function Get-IniValue {
    param($Data, [string]$Section, [string]$Key)

    if ($Data.Contains($Section) -and $Data[$Section].Contains($Key)) {
        return [string]$Data[$Section][$Key]
    }
    return $null
}

function Copy-IfExists {
    param([string]$Source, [string]$Destination)

    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
}

function Merge-GlobalConfig {
    $sourcePath = Join-Path $BackupRoot 'global.ini'
    $targetPath = Join-Path $TargetRoot 'global.ini'
    if (-not (Test-Path -LiteralPath $sourcePath) -or
        -not (Test-Path -LiteralPath $targetPath)) {
        return
    }

    $source = Read-Ini $sourcePath
    $target = Read-Ini $targetPath

    Copy-IniKeys $source $target 'General' @(
        'HotkeyFocusType',
        'EnableCustomServerVodTrack'
    )
    Copy-IniKeys $source $target 'BasicWindow' @(
        'ReplayBufferWhileStreaming',
        'KeepReplayBufferStreamStops'
    )

    Write-Ini $targetPath $target
}

function Merge-ProfileConfig {
    param([string]$SourceProfile, [string]$TargetProfile)

    Copy-IfExists (Join-Path $SourceProfile 'service.json') `
        (Join-Path $TargetProfile 'service.json')
    Copy-IfExists (Join-Path $SourceProfile 'streamEncoder.json') `
        (Join-Path $TargetProfile 'streamEncoder.json')

    $sourceIniPath = Join-Path $SourceProfile 'basic.ini'
    $targetIniPath = Join-Path $TargetProfile 'basic.ini'
    if (-not (Test-Path -LiteralPath $sourceIniPath) -or
        -not (Test-Path -LiteralPath $targetIniPath)) {
        return
    }

    $source = Read-Ini $sourceIniPath
    $target = Read-Ini $targetIniPath

    Copy-IniSection $source $target 'Hotkeys'

    Copy-IniKeys $source $target 'Output' @(
        'Mode',
        'DelayEnable',
        'DelaySec',
        'DelayPreserve',
        'Reconnect',
        'RetryDelay',
        'MaxRetries',
        'BindIP',
        'NewSocketLoopEnable',
        'LowLatencyEnable',
        'DynamicBitrate'
    )
    Copy-IniKeys $source $target 'Stream1' @(
        'IgnoreRecommended'
    )
    Copy-IniKeys $source $target 'SimpleOutput' @(
        'StreamEncoder',
        'VBitrate',
        'ABitrate',
        'UseAdvanced',
        'Preset',
        'NVENCPreset',
        'AMDPreset',
        'QSVPreset',
        'x264Settings',
        'VodTrackEnabled',
        'FilePath',
        'RecRB',
        'RecRBTime',
        'RecRBSize',
        'RecRBPrefix',
        'RecRBSuffix'
    )
    Copy-IniKeys $source $target 'AdvOut' @(
        'ApplyServiceSettings',
        'Encoder',
        'Rescale',
        'RescaleRes',
        'TrackIndex',
        'VodTrackEnabled',
        'VodTrackIndex',
        'Track1Bitrate',
        'Track2Bitrate',
        'Track3Bitrate',
        'Track4Bitrate',
        'Track5Bitrate',
        'Track6Bitrate',
        'Track1Name',
        'Track2Name',
        'Track3Name',
        'Track4Name',
        'Track5Name',
        'Track6Name',
        'RecFilePath',
        'FFFilePath',
        'RecRB',
        'RecRBTime',
        'RecRBSize',
        'RecRBPrefix',
        'RecRBSuffix'
    )

    Write-Ini $targetIniPath $target
}

function Get-JsonProperty {
    param($Object, [string]$Name)

    if ($null -eq $Object -or $Object -is [string]) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }
    return $property.Value
}

function Set-JsonProperty {
    param($Object, [string]$Name, $Value)

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
        return
    }

    $property.Value = $Value
}

function Test-JsonObject {
    param($Value)

    return $null -ne $Value -and
        $Value -is [psobject] -and
        $Value -isnot [string] -and
        $Value -isnot [System.Array]
}

function Find-MatchingSourceItem {
    param($SourceArray, $TargetItem, [int]$Index)

    $targetName = Get-JsonProperty $TargetItem 'name'
    $targetId = Get-JsonProperty $TargetItem 'id'

    if ($null -ne $targetName) {
        foreach ($candidate in $SourceArray) {
            if ((Get-JsonProperty $candidate 'name') -eq $targetName -and
                (($null -eq $targetId) -or (Get-JsonProperty $candidate 'id') -eq $targetId)) {
                return $candidate
            }
        }

        foreach ($candidate in $SourceArray) {
            if ((Get-JsonProperty $candidate 'name') -eq $targetName) {
                return $candidate
            }
        }
    }

    if ($Index -lt $SourceArray.Count) {
        return $SourceArray[$Index]
    }

    return $null
}

function Merge-Hotkeys {
    param($Source, $Target)

    if ($null -eq $Source -or $null -eq $Target) {
        return
    }

    if ($Target -is [System.Array] -and $Source -is [System.Array]) {
        for ($i = 0; $i -lt $Target.Count; $i++) {
            $sourceItem = Find-MatchingSourceItem $Source $Target[$i] $i
            Merge-Hotkeys $sourceItem $Target[$i]
        }
        return
    }

    if (-not (Test-JsonObject $Source) -or -not (Test-JsonObject $Target)) {
        return
    }

    $sourceHotkeys = Get-JsonProperty $Source 'hotkeys'
    if ($null -ne $sourceHotkeys) {
        Set-JsonProperty $Target 'hotkeys' $sourceHotkeys
    }

    foreach ($targetProperty in @($Target.PSObject.Properties)) {
        if ($targetProperty.Name -eq 'hotkeys') {
            continue
        }

        $sourceProperty = $Source.PSObject.Properties[$targetProperty.Name]
        if ($null -ne $sourceProperty) {
            Merge-Hotkeys $sourceProperty.Value $targetProperty.Value
        }
    }
}

function Merge-SceneHotkeys {
    param([string]$SourceScenePath, [string]$TargetScenePath)

    if (-not (Test-Path -LiteralPath $SourceScenePath) -or
        -not (Test-Path -LiteralPath $TargetScenePath)) {
        return
    }

    try {
        $source = Get-Content -Raw -LiteralPath $SourceScenePath | ConvertFrom-Json
        $target = Get-Content -Raw -LiteralPath $TargetScenePath | ConvertFrom-Json
        Merge-Hotkeys $source $target
        $json = $target | ConvertTo-Json -Depth 100 -Compress
        $encoding = New-Object System.Text.UTF8Encoding $false
        [System.IO.File]::WriteAllText($TargetScenePath, $json, $encoding)
    } catch {
        Write-Warning "Could not merge hotkeys for scene file '$TargetScenePath': $($_.Exception.Message)"
    }
}

if (-not (Test-Path -LiteralPath $BackupRoot) -or
    -not (Test-Path -LiteralPath $TargetRoot)) {
    exit 0
}

Merge-GlobalConfig

$backupGlobal = Read-Ini (Join-Path $BackupRoot 'global.ini')
$targetGlobal = Read-Ini (Join-Path $TargetRoot 'global.ini')

$backupProfilesRoot = Join-Path $BackupRoot 'basic\profiles'
$targetProfilesRoot = Join-Path $TargetRoot 'basic\profiles'
$script:ProfilePairs = New-Object System.Collections.ArrayList
$script:ProfilePairSeen = @{}

function Add-ProfilePair {
    param([string]$Source, [string]$Target)

    if (-not (Test-Path -LiteralPath $Source -PathType Container) -or
        -not (Test-Path -LiteralPath $Target -PathType Container)) {
        return
    }

    $key = "$Source|$Target"
    if ($script:ProfilePairSeen.ContainsKey($key)) {
        return
    }

    $script:ProfilePairSeen[$key] = $true
    [void]$script:ProfilePairs.Add([pscustomobject]@{
        Source = $Source
        Target = $Target
    })
}

if ((Test-Path -LiteralPath $backupProfilesRoot) -and
    (Test-Path -LiteralPath $targetProfilesRoot)) {
    Get-ChildItem -LiteralPath $backupProfilesRoot -Directory | ForEach-Object {
        Add-ProfilePair $_.FullName (Join-Path $targetProfilesRoot $_.Name)
    }

    $oldActiveProfile = Get-IniValue $backupGlobal 'Basic' 'ProfileDir'
    $newActiveProfile = Get-IniValue $targetGlobal 'Basic' 'ProfileDir'
    if ($oldActiveProfile -and $newActiveProfile) {
        Add-ProfilePair (Join-Path $backupProfilesRoot $oldActiveProfile) `
            (Join-Path $targetProfilesRoot $newActiveProfile)
    }
}

foreach ($pair in $script:ProfilePairs) {
    Merge-ProfileConfig $pair.Source $pair.Target
}

$backupScenesRoot = Join-Path $BackupRoot 'basic\scenes'
$targetScenesRoot = Join-Path $TargetRoot 'basic\scenes'
$script:ScenePairs = New-Object System.Collections.ArrayList
$script:ScenePairSeen = @{}

function Add-ScenePair {
    param([string]$Source, [string]$Target)

    if (-not (Test-Path -LiteralPath $Source) -or
        -not (Test-Path -LiteralPath $Target)) {
        return
    }

    $key = "$Source|$Target"
    if ($script:ScenePairSeen.ContainsKey($key)) {
        return
    }

    $script:ScenePairSeen[$key] = $true
    [void]$script:ScenePairs.Add([pscustomobject]@{
        Source = $Source
        Target = $Target
    })
}

if ((Test-Path -LiteralPath $backupScenesRoot) -and
    (Test-Path -LiteralPath $targetScenesRoot)) {
    Get-ChildItem -LiteralPath $backupScenesRoot -Filter '*.json' -File | ForEach-Object {
        Add-ScenePair $_.FullName (Join-Path $targetScenesRoot $_.Name)
    }

    $oldActiveScene = Get-IniValue $backupGlobal 'Basic' 'SceneCollectionFile'
    $newActiveScene = Get-IniValue $targetGlobal 'Basic' 'SceneCollectionFile'
    if ($oldActiveScene -and $newActiveScene) {
        Add-ScenePair (Join-Path $backupScenesRoot "$oldActiveScene.json") `
            (Join-Path $targetScenesRoot "$newActiveScene.json")
    }
}

foreach ($pair in $script:ScenePairs) {
    Merge-SceneHotkeys $pair.Source $pair.Target
}
