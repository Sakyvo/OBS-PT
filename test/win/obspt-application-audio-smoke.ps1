param(
    [string]$RuntimeRoot = 'K:/Projects/dev/OBS-PT/.target/rundir/RelWithDebInfo',
    [ValidateSet(
        'capture',
        'dual',
        'topology-change',
        'muted',
        'monitor-only',
        'private-audio',
        'creation-track',
        'shipped-defaults',
        'video-only',
        'process-tree',
        'multi-root',
        'multi-root-lifecycle',
        'vivaldi',
        'start-later',
        'restart',
        'missing-stop',
        'reconnecting-stop',
        'inactive',
        'preserve-config')]
    [string]$Scenario = 'capture',
    [ValidateSet('title', 'class', 'exe', 'process-name')]
    [string]$MatchPriority = 'exe',
    [ValidateSet('hybrid_mp4', 'mp4', 'mkv', 'mov', 'flv', 'ts', 'm3u8')]
    [string]$RecordingFormat = 'hybrid_mp4',
    [ValidateSet('Advanced', 'Simple')]
    [string]$OutputMode = 'Advanced',
    [ValidateSet('Standard', 'Custom')]
    [string]$RecordingType = 'Standard',
    [ValidateSet('matroska', 'mp4', 'flv', 'mpegts', 'mystery-container')]
    [string]$CustomFormat = 'matroska',
    [ValidateSet('None', 'UnknownContainer', 'MissingVideoEncoder',
        'MissingAudioEncoder')]
    [string]$ExpectedStartFailure = 'None',
    [switch]$ReplayBuffer,
    [switch]$Streaming,
    [switch]$VodTrack,
    [switch]$BilibiliLocal,
    [switch]$ParallelVodFailure,
    [ValidateRange(32, 1024)]
    [int]$AudioBitrate = 160,
    [switch]$TargetMinimized,
    [ValidateRange(5, 3600)]
    [int]$DurationSeconds = 10,
    [ValidateRange(1, 1000)]
    [int]$Fps = 60,
    [ValidateRange(8, 16384)]
    [int]$CanvasWidth = 640,
    [ValidateRange(8, 16384)]
    [int]$CanvasHeight = 360,
    [switch]$Qualification,
    [switch]$UseRuntimeDefaults,
    [switch]$AllowExistingInstances,
    [string]$VivaldiPath = 'K:/Vivaldi/vivaldi.exe'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$runtime = (Resolve-Path $RuntimeRoot).Path
$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$runRoot = "$repo/.target/_tdd/application-audio/$runId"
$streamPort = Get-Random -Minimum 20000 -Maximum 40000
$toneProcess = $null
$musicProcess = $null
$privateProcess = $null
$vivaldiProfile = $null
$obsProcess = $null
$streamServer = $null
$streamCapture = $null
$parallelFailureDialogTitle = $null
$resourceSamples = [Collections.Generic.List[object]]::new()
$lastResourceSample = [datetime]::MinValue
$lastProcessCpuSeconds = $null

if (-not ('OBSPTSystemMetrics' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

public static class OBSPTSystemMetrics
{
    [StructLayout(LayoutKind.Sequential)]
    private struct MemoryStatus
    {
        public uint Length;
        public uint MemoryLoad;
        public ulong TotalPhysical;
        public ulong AvailablePhysical;
        public ulong TotalPageFile;
        public ulong AvailablePageFile;
        public ulong TotalVirtual;
        public ulong AvailableVirtual;
        public ulong AvailableExtendedVirtual;
    }

    [DllImport("kernel32.dll")]
    private static extern bool GetSystemTimes(
        out FILETIME idle, out FILETIME kernel, out FILETIME user);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx(ref MemoryStatus status);

    private static ulong lastIdle;
    private static ulong lastKernel;
    private static ulong lastUser;
    private static bool hasCpuSample;

    private static ulong ToUInt64(FILETIME value)
    {
        return ((ulong)(uint)value.dwHighDateTime << 32) |
            (uint)value.dwLowDateTime;
    }

    public static void Reset()
    {
        hasCpuSample = false;
    }

    public static double SampleCpuPercent()
    {
        FILETIME idleTime;
        FILETIME kernelTime;
        FILETIME userTime;
        if (!GetSystemTimes(out idleTime, out kernelTime, out userTime))
            return -1;

        ulong idle = ToUInt64(idleTime);
        ulong kernel = ToUInt64(kernelTime);
        ulong user = ToUInt64(userTime);
        if (!hasCpuSample)
        {
            lastIdle = idle;
            lastKernel = kernel;
            lastUser = user;
            hasCpuSample = true;
            return -1;
        }

        ulong idleDelta = idle - lastIdle;
        ulong totalDelta = (kernel - lastKernel) + (user - lastUser);
        lastIdle = idle;
        lastKernel = kernel;
        lastUser = user;
        return totalDelta == 0
            ? 0
            : 100.0 * (totalDelta - idleDelta) / totalDelta;
    }

    public static double AvailableMemoryMB()
    {
        MemoryStatus status = new MemoryStatus();
        status.Length = (uint)Marshal.SizeOf(status);
        return GlobalMemoryStatusEx(ref status)
            ? status.AvailablePhysical / 1048576.0
            : -1;
    }
}
'@
}
[OBSPTSystemMetrics]::Reset()

if (-not ('OBSPTTestKeyboard' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class OBSPTTestKeyboard
{
    [DllImport("user32.dll")]
    private static extern void keybd_event(
        byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);

    public static void Press(byte virtualKey)
    {
        keybd_event(virtualKey, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(100);
        keybd_event(virtualKey, 0, 2, UIntPtr.Zero);
    }
}
'@
}

if (-not ('OBSPTTestWindows' -as [type])) {
    Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class OBSPTTestWindows
{
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(
        EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(
        IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(
        IntPtr window, StringBuilder text, int maximum);

    [DllImport("user32.dll")]
    private static extern bool PostMessage(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    public static string CloseFailureDialog(int processId)
    {
        string title = null;
        IntPtr match = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != (uint)processId)
                return true;
            StringBuilder text = new StringBuilder(512);
            GetWindowText(window, text, text.Capacity);
            string candidate = text.ToString();
            if (candidate != "Failed to start recording" &&
                candidate != "启动录像失败" &&
                candidate != "無法開始錄影" &&
                candidate != "録画開始に失敗しました" &&
                candidate != "녹화를 시작하지 못했습니다" &&
                candidate != "Failed to start streaming" &&
                candidate != "启动串流失败" &&
                candidate != "無法開始串流" &&
                candidate != "配信開始に失敗しました" &&
                candidate != "방송을 시작하지 못했습니다")
                return true;
            title = candidate;
            match = window;
            return false;
        }, IntPtr.Zero);
        if (match != IntPtr.Zero)
            PostMessage(match, 0x0010, IntPtr.Zero, IntPtr.Zero);
        return title;
    }
}
'@
}

function Read-SharedText {
    param([string]$Path)

    $share = [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
    $stream = [IO.File]::Open(
        $Path,
        [IO.FileMode]::Open,
        [IO.FileAccess]::Read,
        $share)
    try {
        $reader = [IO.StreamReader]::new(
            $stream,
            [Text.Encoding]::UTF8,
            $true)
        try {
            return $reader.ReadToEnd()
        } finally {
            $reader.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Invoke-Robocopy {
    param([string]$Source, [string]$Destination)

    & robocopy $Source $Destination /E /NFL /NDL /NJH /NJS /NP | Out-Null
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

    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        [void]$lines.Add($line)
    }

    $sectionIndex = $lines.IndexOf("[$Section]")
    if ($sectionIndex -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ne '') {
            [void]$lines.Add('')
        }
        [void]$lines.Add("[$Section]")
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

    [IO.File]::WriteAllLines(
        $Path,
        $lines,
        [Text.UTF8Encoding]::new($false))
}

function Build-ToneApplication {
    param([string]$Directory)

    $nativeDirectory = (Resolve-Path $Directory).Path
    $sourcePath = Join-Path $nativeDirectory 'obspt-tone.cs'
    $exePath = Join-Path $nativeDirectory 'obspt-tone-game.exe'
    $source = @'
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Media;
using System.Threading;
using System.Windows.Forms;

internal sealed class ToneForm : Form
{
    private const int WS_EX_NOACTIVATE = 0x08000000;
    private MemoryStream wave;
    private SoundPlayer player;
    private Process child;

    protected override bool ShowWithoutActivation { get { return true; } }

    protected override CreateParams CreateParams
    {
        get
        {
            CreateParams value = base.CreateParams;
            value.ExStyle |= WS_EX_NOACTIVATE;
            return value;
        }
    }

    internal ToneForm(
        double frequency,
        string title,
        bool minimized,
        bool spawnChild)
    {
        Text = title;
        ShowInTaskbar = false;
        StartPosition = FormStartPosition.Manual;
        Location = new Point(-32000, -32000);
        ClientSize = new Size(320, 180);
        Opacity = 0.01;
        if (minimized)
            WindowState = FormWindowState.Minimized;

        if (spawnChild)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo();
            startInfo.FileName = Application.ExecutablePath;
            startInfo.Arguments = frequency.ToString() +
                " OBSPT_Tone_Child normal child-audio";
            startInfo.UseShellExecute = false;
            startInfo.CreateNoWindow = true;
            startInfo.WindowStyle = ProcessWindowStyle.Hidden;
            child = Process.Start(startInfo);
            StartPlayer(frequency);
        }
        else
        {
            StartPlayer(frequency);
        }
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        if (player != null)
        {
            player.Stop();
            player.Dispose();
            wave.Dispose();
        }
        if (child != null && !child.HasExited)
        {
            child.Kill();
            child.WaitForExit();
        }
        base.OnFormClosed(e);
    }

    private void StartPlayer(double frequency)
    {
        wave = CreateWave(frequency);
        player = new SoundPlayer(wave);
        player.Load();
        player.PlayLooping();
    }

    private static MemoryStream CreateWave(double frequency)
    {
        const int sampleRate = 48000;
        const short channels = 2;
        const short bitsPerSample = 16;
        const int frames = sampleRate;
        const short amplitude = 128;
        int blockAlign = channels * bitsPerSample / 8;
        int dataSize = frames * blockAlign;
        MemoryStream stream = new MemoryStream(44 + dataSize);
        BinaryWriter writer = new BinaryWriter(stream);

        writer.Write(new char[] {'R', 'I', 'F', 'F'});
        writer.Write(36 + dataSize);
        writer.Write(new char[] {'W', 'A', 'V', 'E'});
        writer.Write(new char[] {'f', 'm', 't', ' '});
        writer.Write(16);
        writer.Write((short)1);
        writer.Write(channels);
        writer.Write(sampleRate);
        writer.Write(sampleRate * blockAlign);
        writer.Write((short)blockAlign);
        writer.Write(bitsPerSample);
        writer.Write(new char[] {'d', 'a', 't', 'a'});
        writer.Write(dataSize);

        for (int frame = 0; frame < frames; frame++)
        {
            short sample = (short)(Math.Sin(
                2.0 * Math.PI * frequency * frame / sampleRate) * amplitude);
            writer.Write(sample);
            writer.Write(sample);
        }

        writer.Flush();
        stream.Position = 0;
        return stream;
    }

    [STAThread]
    private static void Main(string[] args)
    {
        double frequency = args.Length > 0 ? double.Parse(args[0]) : 440.0;
        string title = args.Length > 1 ? args[1] : "OBSPT_Tone_Game";
        bool minimized = args.Length > 2 && args[2] == "minimized";
        string mode = args.Length > 3 ? args[3] : "direct";
        if (mode == "child-audio")
        {
            using (MemoryStream wave = CreateWave(frequency))
            using (SoundPlayer player = new SoundPlayer(wave))
            {
                player.Load();
                player.PlayLooping();
                Thread.Sleep(Timeout.Infinite);
            }
            return;
        }
        Application.EnableVisualStyles();
        Application.Run(new ToneForm(
            frequency,
            title,
            minimized,
            mode == "spawn-child"));
    }
}
'@

    [IO.File]::WriteAllText(
        $sourcePath,
        $source,
        [Text.UTF8Encoding]::new($false))

    $csc = 'C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    if (-not (Test-Path $csc)) {
        throw "C# compiler not found: $csc"
    }

    & $csc /nologo /target:winexe /optimize+ `
        /reference:System.dll `
        /reference:System.Drawing.dll `
        /reference:System.Windows.Forms.dll `
        "/out:$exePath" $sourcePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exePath)) {
        throw 'Failed to compile deterministic tone application'
    }

    return $exePath
}

function Prepare-PortableRuntime {
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    try {
        $attributes = [IO.File]::GetAttributes($runRoot)
        [IO.File]::SetAttributes(
            $runRoot,
            $attributes -bor [IO.FileAttributes]::NotContentIndexed)
    } catch {
        Write-Warning "Could not disable content indexing for $runRoot"
    }
    Invoke-Robocopy $runtime $runRoot
    if (-not $UseRuntimeDefaults) {
        $portableConfig = "$runRoot/obs-studio"
        if (Test-Path $portableConfig) {
            Remove-Item -LiteralPath $portableConfig -Recurse -Force
        }
        Copy-Item -Recurse -Force `
            "$repo/UI/data/obspt-defaults/obs-studio" `
            $portableConfig
    } elseif (-not (Test-Path "$runRoot/obs-studio/global.ini")) {
        throw 'RuntimeRoot does not contain packaged OBS-PT defaults'
    }
    New-Item -ItemType Directory -Path "$runRoot/recordings" -Force |
        Out-Null

    $global = "$runRoot/obs-studio/global.ini"
    Set-IniValue $global 'General' 'FirstRun' 'false'
    Set-IniValue $global 'General' 'EnableAutoUpdates' 'false'
    Set-IniValue $global 'General' 'ConfirmOnExit' 'false'
    Set-IniValue $global 'OBSPT' 'BootstrapVersion' '3'
    Set-IniValue $global 'OBSPT' 'FirstRunCompleted' 'true'
    if ($VodTrack) {
        Set-IniValue $global 'General' 'EnableCustomServerVodTrack' 'true'
    }

    if ($Scenario -eq 'preserve-config') {
        return $runRoot
    }

    $basic = "$runRoot/obs-studio/basic/profiles/PotPvP/basic.ini"
    Set-IniValue $basic 'Video' 'BaseCX' "$CanvasWidth"
    Set-IniValue $basic 'Video' 'BaseCY' "$CanvasHeight"
    Set-IniValue $basic 'Video' 'OutputCX' "$CanvasWidth"
    Set-IniValue $basic 'Video' 'OutputCY' "$CanvasHeight"
    Set-IniValue $basic 'Video' 'FPSNum' "$Fps"
    Set-IniValue $basic 'Output' 'Mode' $OutputMode
    Set-IniValue $basic 'Output' 'AudioBitrate' "$AudioBitrate"
    Set-IniValue $basic 'AdvOut' 'RecFormat' $RecordingFormat
	Set-IniValue $basic 'AdvOut' 'RecType' $(
		if ($RecordingType -eq 'Custom') { 'FFmpeg' } else { 'Standard' })
    Set-IniValue $basic 'AdvOut' 'RecFilePath' "$runRoot/recordings"
    Set-IniValue $basic 'SimpleOutput' 'FilePath' "$runRoot/recordings"
    Set-IniValue $basic 'SimpleOutput' 'RecFormat' $RecordingFormat
	if ($RecordingType -eq 'Custom') {
		$customExtension = switch ($CustomFormat) {
			'matroska' { 'mkv' }
			'mpegts' { 'ts' }
			default { $CustomFormat }
		}
		Set-IniValue $basic 'AdvOut' 'FFOutputToFile' 'true'
		Set-IniValue $basic 'AdvOut' 'FFFilePath' "$runRoot/recordings"
		Set-IniValue $basic 'AdvOut' 'FFFileNameWithoutSpace' 'false'
		Set-IniValue $basic 'AdvOut' 'FFFormat' $CustomFormat
		Set-IniValue $basic 'AdvOut' 'FFFormatMimeType' ''
		Set-IniValue $basic 'AdvOut' 'FFExtension' $customExtension
		Set-IniValue $basic 'AdvOut' 'FFVBitrate' '2500'
		Set-IniValue $basic 'AdvOut' 'FFVGOPSize' '120'
		Set-IniValue $basic 'AdvOut' 'FFVEncoderId' '0'
		Set-IniValue $basic 'AdvOut' 'FFVEncoder' 'libx264'
		Set-IniValue $basic 'AdvOut' 'FFAEncoderId' '0'
		Set-IniValue $basic 'AdvOut' 'FFAEncoder' 'aac'
		if ($ExpectedStartFailure -eq 'MissingVideoEncoder') {
			Set-IniValue $basic 'AdvOut' 'FFVEncoder' ''
		} elseif ($ExpectedStartFailure -eq 'MissingAudioEncoder') {
			Set-IniValue $basic 'AdvOut' 'FFAEncoder' ''
		}
	}
	if ($ReplayBuffer) {
		if ($RecordingType -ne 'Standard') {
			throw 'Replay Buffer smoke requires Standard recording output'
		}
		Set-IniValue $basic 'AdvOut' 'RecRB' 'true'
		Set-IniValue $basic 'AdvOut' 'RecRBTime' '30'
		Set-IniValue $basic 'AdvOut' 'RecRBSize' '512'
		Set-IniValue $basic 'SimpleOutput' 'RecRB' 'true'
		Set-IniValue $basic 'SimpleOutput' 'RecRBTime' '30'
		Set-IniValue $basic 'SimpleOutput' 'RecRBSize' '512'
		Set-IniValue $basic 'Hotkeys' 'ReplayBuffer' `
			'{"ReplayBuffer.Save":[{"key":"OBS_KEY_F8"}]}'
		Set-IniValue $basic 'Hotkeys' 'OBSBasic.StopReplayBuffer' `
			'{"bindings":[{"key":"OBS_KEY_F9"}]}'
	}
	if ($Streaming) {
		if ($ParallelVodFailure) {
			Set-IniValue $basic 'Hotkeys' 'OBSBasic.StartStreaming' `
				'{"bindings":[{"key":"OBS_KEY_F10"}]}'
		}
		Set-IniValue $basic 'Hotkeys' 'OBSBasic.ForceStopStreaming' `
			'{"bindings":[{"key":"OBS_KEY_F7"}]}'
		Set-IniValue $basic 'Stream1' 'IgnoreRecommended' 'true'
		$localServer = "rtmp://127.0.0.1:$streamPort/live"
		$service = if ($BilibiliLocal) {
			$servicesPath = "$runRoot/data/obs-plugins/rtmp-services/services.json"
			$services = Get-Content $servicesPath -Raw | ConvertFrom-Json
			$bilibili = $services.services |
				Where-Object { $_.name -eq 'Bilibili Live' } |
				Select-Object -First 1
			if (-not $bilibili) {
				throw 'Bilibili Live service is missing from the packaged service list'
			}
			foreach ($server in $bilibili.servers) {
				$server.url = $localServer
			}
			[IO.File]::WriteAllText(
				$servicesPath,
				($services | ConvertTo-Json -Depth 12),
				[Text.UTF8Encoding]::new($false))
			[ordered]@{
				type = 'rtmp_common'
				settings = [ordered]@{
					bwtest = $false
					key = 'test'
					server = $localServer
					service = 'Bilibili Live'
				}
				hotkeys = [ordered]@{}
			}
		} else {
			[ordered]@{
				type = 'rtmp_custom'
				settings = [ordered]@{
					server = $localServer
					key = 'test'
					use_auth = $false
				}
				hotkeys = [ordered]@{}
			}
		}
		[IO.File]::WriteAllText(
			"$runRoot/obs-studio/basic/profiles/PotPvP/service.json",
			($service | ConvertTo-Json -Depth 8 -Compress),
			[Text.UTF8Encoding]::new($false))
	}
	if ($VodTrack) {
		Set-IniValue $basic 'SimpleOutput' 'VodTrackEnabled' 'true'
		Set-IniValue $basic 'AdvOut' 'VodTrackEnabled' 'true'
		Set-IniValue $basic 'AdvOut' 'VodTrackIndex' $(
			if ($ParallelVodFailure) { '6' } else { '3' })
	}
    return $runRoot
}

function Add-ApplicationAudioFixture {
    param(
        [string]$ToneExecutable,
        [bool]$Active,
        [string]$WindowClass,
        [string]$PriorityMode
    )

    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $sceneData.DesktopAudioDevice1.mixers = 1
	$sceneData.sources = @($sceneData.sources | Where-Object {
		$_.id -ne 'wasapi_process_output_capture'
	})

    $template = $sceneData.sources |
        Where-Object id -eq 'monitor_capture' |
        Select-Object -First 1
    $source = $template | ConvertTo-Json -Depth 100 | ConvertFrom-Json
    $source.id = 'wasapi_process_output_capture'
    $source.versioned_id = 'wasapi_process_output_capture'
	$source.name = if ($vivaldiScenario) { 'Music Audio' } else { 'Game Audio' }
	$source.mixers = if ($vivaldiScenario) { 4 } else { 2 }
	$source.enabled = $Active
    if ($Streaming) {
        $source.hotkeys = [pscustomobject]@{
            'libobs.mute' = @([pscustomobject]@{ key = 'OBS_KEY_F3' })
            'libobs.unmute' = @([pscustomobject]@{ key = 'OBS_KEY_F4' })
            'libobs.push-to-mute' = @()
            'libobs.push-to-talk' = @()
        }
        $sceneData.DesktopAudioDevice1.hotkeys.'libobs.mute' = @(
            [pscustomobject]@{ key = 'OBS_KEY_F1' })
        $sceneData.DesktopAudioDevice1.hotkeys.'libobs.unmute' = @(
            [pscustomobject]@{ key = 'OBS_KEY_F2' })
    }
	$source.private_settings = [pscustomobject]@{
		obspt_audio_source_id = if ($vivaldiScenario) {
			'smoke-music'
		} else {
			'smoke-game'
		}
	}
    $priority = switch ($PriorityMode) {
        'class' { 0 }
        'title' { 1 }
        'process-name' { 3 }
        default { 2 }
    }
    $storedTitle = if ($PriorityMode -in @('class', 'process-name')) {
        'OBSPT_Tone_Previous_Title'
    } else {
        'OBSPT_Tone_Game'
    }
    $storedClass = if ($PriorityMode -in @('exe', 'process-name')) {
        'Ignored'
    } else {
        $WindowClass
    }
    $source.settings = [pscustomobject]@{
        window = "$storedTitle`:$storedClass`:$([IO.Path]::GetFileName($ToneExecutable))"
        priority = $priority
        process_executable = $([IO.Path]::GetFileName($ToneExecutable))
    }
    $sceneData.sources += $source

	$sceneData.DesktopAudioDevice1.private_settings = [pscustomobject]@{
		obspt_audio_source_id = 'smoke-desktop'
	}
	$routing = [pscustomobject]@{
		version = 1
		tracks = @(
			[pscustomobject]@{ number = 2; name = 'Game' }
			[pscustomobject]@{ number = 3; name = 'Music' }
			[pscustomobject]@{ number = 4; name = 'Microphone' }
			[pscustomobject]@{ number = 5; name = 'Voice Chat' }
			[pscustomobject]@{ number = 6; name = 'Aux' }
		)
		sources = @(
			[pscustomobject]@{
				id = 'smoke-desktop'
				name = $sceneData.DesktopAudioDevice1.name
				kind = 'desktop_audio'
				editor_track = 0
			}
			[pscustomobject]@{
				id = if ($vivaldiScenario) { 'smoke-music' } else { 'smoke-game' }
				name = if ($vivaldiScenario) { 'Music Audio' } else { 'Game Audio' }
				kind = 'application_audio'
				editor_track = if ($vivaldiScenario) { 3 } else { 2 }
			}
		)
	}
	$sceneData | Add-Member -Force `
		-NotePropertyName 'obspt_audio_routing' `
		-NotePropertyValue $routing

    $timer = $sceneData.modules.'output-timer'
    $timer.recordTimerHours = [Math]::Floor($DurationSeconds / 3600)
    $timer.recordTimerMinutes = [Math]::Floor(
        ($DurationSeconds % 3600) / 60)
    $timer.recordTimerSeconds = $DurationSeconds % 60
    $timer.autoStartRecordTimer = $true
    $timer.pauseRecordTimer = $false

    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Set-ShippedDefaultsFixture {
    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $expected = @(
        [pscustomobject]@{
            Name = 'minecraft-sound'; Executable = 'javaw.exe'
            Identity = 'obspt-default-minecraft-sound'; Track = 2
        },
        [pscustomobject]@{
            Name = 'music'; Executable = 'vivaldi.exe'
            Identity = 'obspt-default-music'; Track = 3
        }
    )
    foreach ($item in $expected) {
        $source = @($sceneData.sources | Where-Object name -eq $item.Name)
        $route = @($sceneData.obspt_audio_routing.sources |
            Where-Object id -eq $item.Identity)
        if ($source.Count -ne 1 -or $route.Count -ne 1 -or
            $source[0].settings.priority -ne 3 -or
            $source[0].settings.process_executable -ne $item.Executable -or
            $source[0].private_settings.obspt_audio_source_id -ne
                $item.Identity -or
            $null -ne $source[0].settings.PSObject.Properties['window'] -or
            $route[0].editor_track -ne $item.Track) {
            throw "Shipped source is invalid: $($item.Name)"
        }
    }

    $timer = $sceneData.modules.'output-timer'
    $timer.recordTimerHours = [Math]::Floor($DurationSeconds / 3600)
    $timer.recordTimerMinutes = [Math]::Floor(
        ($DurationSeconds % 3600) / 60)
    $timer.recordTimerSeconds = $DurationSeconds % 60
    $timer.autoStartRecordTimer = $true
    $timer.pauseRecordTimer = $false
    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Get-VivaldiFixtureProcesses {
    if (-not $vivaldiProfile) {
        return @()
    }

    $profileArgument = "--user-data-dir=$vivaldiProfile"
    return @(Get-CimInstance Win32_Process -Filter "Name='vivaldi.exe'" |
        Where-Object {
            $_.CommandLine -and $_.CommandLine.Contains($profileArgument)
        })
}

function Start-VivaldiFixture {
    if (-not (Test-Path $VivaldiPath)) {
        throw "Vivaldi executable not found: $VivaldiPath"
    }

    $htmlPath = "$runRoot/vivaldi-tone.html"
    if (-not (Test-Path $htmlPath)) {
        $html = @'
<!doctype html>
<meta charset="utf-8">
<title>OBS-PT Vivaldi Audio Initial</title>
<script>
const audio = new AudioContext();
const oscillator = audio.createOscillator();
const gain = audio.createGain();
oscillator.frequency.value = 440;
gain.gain.value = 0.01;
oscillator.connect(gain).connect(audio.destination);
oscillator.start();
setTimeout(() => document.title = "OBS-PT Vivaldi Audio Changed", 2000);
</script>
'@
        [IO.File]::WriteAllText(
            $htmlPath,
            $html,
            [Text.UTF8Encoding]::new($false))
    }

    $page = [Uri]::new($htmlPath).AbsoluteUri
    Start-Process -FilePath $VivaldiPath -ArgumentList @(
        "--user-data-dir=$vivaldiProfile",
        '--no-first-run',
        '--disable-default-apps',
        '--autoplay-policy=no-user-gesture-required',
        '--window-position=-32000,-32000',
        "--app=$page") -WindowStyle Minimized | Out-Null

    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 250
        $processes = Get-VivaldiFixtureProcesses
    } while ($processes.Count -eq 0 -and (Get-Date) -lt $deadline)
    if ($processes.Count -eq 0) {
        throw 'The isolated Vivaldi profile did not start'
    }
}

function Stop-VivaldiFixture {
    foreach ($process in Get-VivaldiFixtureProcesses) {
        Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
    }

    $deadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
    } while ((Get-VivaldiFixtureProcesses).Count -gt 0 -and
        (Get-Date) -lt $deadline)
}

function Add-MusicAudioFixture {
    param([string]$ToneExecutable)

    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $gameSource = $sceneData.sources |
        Where-Object { $_.id -eq 'wasapi_process_output_capture' } |
        Select-Object -First 1
    $musicSource = $gameSource |
        ConvertTo-Json -Depth 100 |
        ConvertFrom-Json
    $musicSource.name = 'Music Audio'
    $musicSource.mixers = 4
    if ($Streaming) {
        $musicSource.hotkeys = [pscustomobject]@{
            'libobs.mute' = @([pscustomobject]@{ key = 'OBS_KEY_F5' })
            'libobs.unmute' = @([pscustomobject]@{ key = 'OBS_KEY_F6' })
            'libobs.push-to-mute' = @()
            'libobs.push-to-talk' = @()
        }
    }
    $musicSource.private_settings = [pscustomobject]@{
        obspt_audio_source_id = 'smoke-music'
    }
    $musicSource.settings = [pscustomobject]@{
        window = "OBSPT_Tone_Music:Ignored:$([IO.Path]::GetFileName($ToneExecutable))"
        priority = 2
    }
    $sceneData.sources += $musicSource
    $sceneData.obspt_audio_routing.sources += [pscustomobject]@{
        id = 'smoke-music'
        name = 'Music Audio'
        kind = 'application_audio'
        editor_track = 3
    }

	[IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Add-PrivateAudioFixture {
    param([string]$ToneExecutable)

    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $scriptPath = "$runRoot/obspt-private-audio.lua"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $sceneData.DesktopAudioDevice1.muted = $true
    $desktopRoute = $sceneData.obspt_audio_routing.sources |
        Where-Object kind -eq 'desktop_audio' |
        Select-Object -First 1
    $desktopRoute | Add-Member -Force -NotePropertyName muted `
        -NotePropertyValue $true

    $target = "OBSPT_Tone_Private:Ignored:$([IO.Path]::GetFileName($ToneExecutable))"
    $script = @'
obs = obslua
private_source = nil
parent_source = nil
private_item = nil
target_window = "__TARGET_WINDOW__"

function activate_private_source()
    if private_source == nil then
        return
    end
    parent_source = obs.obs_frontend_get_current_scene()
    if parent_source == nil then
        return
    end
    local scene = obs.obs_scene_from_source(parent_source)
    if scene == nil then
        obs.obs_source_release(parent_source)
        parent_source = nil
        return
    end
    private_item = obs.obs_scene_add(scene, private_source)
    obs.obs_scene_release(scene)
    if private_item == nil then
        obs.obs_source_release(parent_source)
        parent_source = nil
        return
    end
    obs.obs_sceneitem_set_locked(private_item, true)
    obs.timer_remove(activate_private_source)
    obs.script_log(obs.LOG_INFO, "OBSPT private audio source active")
end

function script_load(settings)
    local source_settings = obs.obs_data_create()
    obs.obs_data_set_string(source_settings, "window", target_window)
    obs.obs_data_set_int(source_settings, "priority", 2)
    private_source = obs.obs_source_create_private(
        "wasapi_process_output_capture", "Private Audio", source_settings)
    obs.obs_data_release(source_settings)
    if private_source == nil then
        obs.script_log(obs.LOG_ERROR, "OBSPT private audio source creation failed")
        return
    end
    obs.timer_add(activate_private_source, 50)
end

function script_unload()
    obs.timer_remove(activate_private_source)
    if private_item ~= nil then
        obs.obs_sceneitem_remove(private_item)
        private_item = nil
    end
    if private_source ~= nil then
        obs.obs_source_release(private_source)
        private_source = nil
    end
    if parent_source ~= nil then
        obs.obs_source_release(parent_source)
        parent_source = nil
    end
end
'@
    $script = $script.Replace('__TARGET_WINDOW__', $target)
    [IO.File]::WriteAllText(
        $scriptPath, $script, [Text.UTF8Encoding]::new($false))
    $sceneData.modules.'scripts-tool' = @(
        [pscustomobject]@{
            path = $scriptPath.Replace('\', '/')
            settings = [pscustomobject]@{}
        }
    )
    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Add-CreationTrackFixture {
    param([string]$ToneExecutable)

    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $scriptPath = "$runRoot/obspt-creation-track.lua"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $target = "OBSPT_Tone_Creation:Ignored:$([IO.Path]::GetFileName($ToneExecutable))"
    $script = @'
obs = obslua
created_source = nil
target_window = "__TARGET_WINDOW__"

function script_load(settings)
    local source_settings = obs.obs_data_create()
    obs.obs_data_set_string(source_settings, "window", target_window)
    obs.obs_data_set_int(source_settings, "priority", 2)
    obs.obs_data_set_int(
        source_settings, "obspt_initial_editor_track", 3)
    created_source = obs.obs_source_create(
        "wasapi_process_output_capture", "Creation Music",
        source_settings, nil)
    obs.obs_data_release(source_settings)
    if created_source == nil then
		obs.script_log(
			obs.LOG_ERROR, "OBSPT creation-track source creation failed")
		return
	end
	obs.script_log(obs.LOG_INFO, "OBSPT creation-track source created")
end

function script_unload()
	if created_source ~= nil then
		obs.obs_source_release(created_source)
		created_source = nil
	end
end
'@
    $script = $script.Replace('__TARGET_WINDOW__', $target)
    [IO.File]::WriteAllText(
        $scriptPath, $script, [Text.UTF8Encoding]::new($false))
    $sceneData.modules.'scripts-tool' = @(
        [pscustomobject]@{
            path = $scriptPath.Replace('\', '/')
            settings = [pscustomobject]@{}
        }
    )
    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Set-ExcludedMusicFixture {
    param([ValidateSet('muted', 'monitor-only')][string]$Mode)

    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $sceneData.DesktopAudioDevice1.muted = $true
    $desktopRoute = $sceneData.obspt_audio_routing.sources |
        Where-Object kind -eq 'desktop_audio' |
        Select-Object -First 1
    $desktopRoute | Add-Member -Force -NotePropertyName muted `
        -NotePropertyValue $true

    $musicSource = $sceneData.sources |
        Where-Object name -eq 'Music Audio' |
        Select-Object -First 1
    $musicRoute = $sceneData.obspt_audio_routing.sources |
        Where-Object id -eq 'smoke-music' |
        Select-Object -First 1
    if ($Mode -eq 'muted') {
        $musicSource.muted = $true
        $musicRoute | Add-Member -Force -NotePropertyName muted `
            -NotePropertyValue $true
    } else {
        $musicSource.monitoring_type = 1
        $musicRoute | Add-Member -Force -NotePropertyName monitor_only `
            -NotePropertyValue $true
    }

    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Set-VideoOnlyFixture {
    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $sceneData.DesktopAudioDevice1.muted = $true
    $desktopRoute = $sceneData.obspt_audio_routing.sources |
        Where-Object kind -eq 'desktop_audio' |
        Select-Object -First 1
    $desktopRoute | Add-Member -Force -NotePropertyName muted `
        -NotePropertyValue $true

    $timer = $sceneData.modules.'output-timer'
    $timer.recordTimerHours = [Math]::Floor($DurationSeconds / 3600)
    $timer.recordTimerMinutes = [Math]::Floor(
        ($DurationSeconds % 3600) / 60)
    $timer.recordTimerSeconds = $DurationSeconds % 60
    $timer.autoStartRecordTimer = $true
    $timer.pauseRecordTimer = $false
    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Add-TopologyMutationFixture {
    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $scriptPath = "$runRoot/obspt-topology-mutation.lua"
    $tonePath = "$runRoot/late-source.wav"

    & ffmpeg -v error -y -f lavfi `
        -i 'sine=frequency=1320:sample_rate=48000' `
        -t $DurationSeconds -ac 2 -ar 48000 $tonePath
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the late-source tone fixture'
    }

    $script = @'
obs = obslua
tone_file = ""
mutation_step = 0

function mutate_topology()
    mutation_step = mutation_step + 1
    if mutation_step == 1 then
        local settings = obs.obs_data_create()
        obs.obs_data_set_bool(settings, "is_local_file", true)
        obs.obs_data_set_string(settings, "local_file", tone_file)
        obs.obs_data_set_bool(settings, "looping", true)
        local source = obs.obs_source_create(
            "ffmpeg_source", "Late Audio", settings, nil)
        obs.obs_data_release(settings)
        if source ~= nil then
            local scene_source = obs.obs_frontend_get_current_scene()
            local scene = obs.obs_scene_from_source(scene_source)
            if scene ~= nil then
                obs.obs_scene_add(scene, source)
            end
            obs.obs_source_release(scene_source)
            obs.obs_source_release(source)
            obs.script_log(obs.LOG_INFO, "OBSPT topology source created")
        end
    elseif mutation_step == 2 then
        local source = obs.obs_get_source_by_name("Music Audio")
        if source ~= nil then
            obs.obs_source_remove(source)
            obs.obs_source_release(source)
            obs.script_log(obs.LOG_INFO, "OBSPT topology source removed")
        end
        obs.timer_remove(mutate_topology)
    end
end

function on_frontend_event(event)
    if event == obs.OBS_FRONTEND_EVENT_RECORDING_STARTED then
        obs.timer_add(mutate_topology, 2500)
    end
end

function script_update(settings)
    tone_file = obs.obs_data_get_string(settings, "tone_file")
end

function script_load(settings)
    script_update(settings)
    obs.obs_frontend_add_event_callback(on_frontend_event)
end

function script_unload()
    obs.timer_remove(mutate_topology)
    obs.obs_frontend_remove_event_callback(on_frontend_event)
end
'@
    [IO.File]::WriteAllText(
        $scriptPath,
        $script,
        [Text.UTF8Encoding]::new($false))

    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $sceneData.DesktopAudioDevice1.muted = $true
    $desktopRoute = $sceneData.obspt_audio_routing.sources |
        Where-Object kind -eq 'desktop_audio' |
        Select-Object -First 1
    $desktopRoute | Add-Member -Force -NotePropertyName muted `
        -NotePropertyValue $true
    $sceneData.modules.'scripts-tool' = @(
        [pscustomobject]@{
            path = $scriptPath.Replace('\', '/')
            settings = [pscustomobject]@{
                tone_file = $tonePath.Replace('\', '/')
            }
        }
    )
    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Get-ToneWindowClass {
    param([Diagnostics.Process]$Process)

    if (-not ('OBSPTWindowProbe' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class OBSPTWindowProbe
{
    private delegate bool EnumProc(IntPtr hwnd, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hwnd, StringBuilder value, int count);

    public static string FindVisibleWindowClass(uint targetProcessId)
    {
        string result = null;
        EnumWindows(delegate(IntPtr hwnd, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(hwnd, out processId);
            if (processId == targetProcessId && IsWindowVisible(hwnd)) {
                StringBuilder value = new StringBuilder(256);
                GetClassName(hwnd, value, value.Capacity);
                result = value.ToString();
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
    }

    $deadline = (Get-Date).AddSeconds(10)
    do {
        $windowClass = [OBSPTWindowProbe]::FindVisibleWindowClass(
            [uint32]$Process.Id)
        if ($windowClass) {
            return $windowClass
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    throw 'Tone application did not create a visible target window'
}

function Start-ToneApplication {
    param(
        [string]$Executable,
        [string]$Title,
        [double]$Frequency = 440
    )

    $process = Start-Process -FilePath $Executable `
        -ArgumentList @(
            "$Frequency",
            $Title,
            $(if ($TargetMinimized) { 'minimized' } else { 'normal' }),
            $(if ($Scenario -eq 'process-tree') { 'spawn-child' } else { 'direct' })) `
        -WorkingDirectory "$runRoot/bin/64bit" `
        -WindowStyle Hidden `
        -PassThru
    Start-Sleep -Seconds 2
    $process.Refresh()
    if ($process.HasExited) {
        throw "Tone application exited during startup: $Title"
    }
    return $process
}

function Export-VodTrackAudio {
    param([string]$StreamCapture)

    $dataPath = "$runRoot/vod-track-data.bin"
    $audioPath = "$runRoot/vod-track.aac"
    & ffmpeg -v error -y -i $StreamCapture -map '0:d:0' -c copy `
        -f data $dataPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not extract the RTMP additional-media stream'
    }

    $data = [IO.File]::ReadAllBytes($dataPath)
    $signature = [byte[]](@(0x02, 0x00, 0x0f) +
        [Text.Encoding]::ASCII.GetBytes('additionalMedia'))
    $mediaKey = [byte[]](@(0x00, 0x05) +
        [Text.Encoding]::ASCII.GetBytes('media') + @(0x11, 0x0c))
    $config = $null
    $frames = [Collections.Generic.List[byte[]]]::new()

    for ($offset = 0; $offset -le $data.Length - $signature.Length;) {
        $matched = $true
        for ($index = 0; $index -lt $signature.Length; $index++) {
            if ($data[$offset + $index] -ne $signature[$index]) {
                $matched = $false
                break
            }
        }
        if (-not $matched) {
            $offset++
            continue
        }

        $keyOffset = -1
        $searchEnd = [Math]::Min(
            $data.Length - $mediaKey.Length,
            $offset + $signature.Length + 64)
        for ($candidate = $offset + $signature.Length;
             $candidate -le $searchEnd; $candidate++) {
            $keyMatched = $true
            for ($index = 0; $index -lt $mediaKey.Length; $index++) {
                if ($data[$candidate + $index] -ne $mediaKey[$index]) {
                    $keyMatched = $false
                    break
                }
            }
            if ($keyMatched) {
                $keyOffset = $candidate
                break
            }
        }
        if ($keyOffset -lt 0) {
            $offset += $signature.Length
            continue
        }

        $cursor = $keyOffset + $mediaKey.Length
        $u29 = 0
        for ($part = 0; $part -lt 4; $part++) {
            if ($cursor -ge $data.Length) {
                throw 'Truncated AMF3 byte-array length in VOD stream'
            }
            $value = $data[$cursor++]
            if ($part -lt 3) {
                $u29 = ($u29 -shl 7) -bor ($value -band 0x7f)
                if (($value -band 0x80) -eq 0) {
                    break
                }
            } else {
                $u29 = ($u29 -shl 8) -bor $value
            }
        }
        if (($u29 -band 1) -eq 0) {
            throw 'Referenced AMF3 byte arrays are unsupported in VOD smoke'
        }
        $length = $u29 -shr 1
        if ($length -lt 3 -or $cursor + $length -gt $data.Length -or
            $data[$cursor] -ne 0xaf) {
            throw 'Invalid wrapped AAC packet in VOD stream'
        }

        $packetType = $data[$cursor + 1]
        $payload = [byte[]]$data[($cursor + 2)..($cursor + $length - 1)]
        if ($packetType -eq 0) {
            $config = $payload
        } elseif ($packetType -eq 1) {
            $frames.Add($payload)
        }
        $offset = $cursor + $length
    }

    if (-not $config -or $config.Length -lt 2 -or $frames.Count -eq 0) {
        throw 'VOD stream did not contain AAC configuration and media frames'
    }

    $audioObjectType = ($config[0] -shr 3) -band 0x1f
    $frequencyIndex = (($config[0] -band 0x07) -shl 1) -bor
        (($config[1] -shr 7) -band 0x01)
    $channelConfig = ($config[1] -shr 3) -band 0x0f
    if ($audioObjectType -lt 1 -or $audioObjectType -gt 4 -or
        $frequencyIndex -gt 12 -or $channelConfig -eq 0) {
        throw 'Unsupported AAC AudioSpecificConfig in VOD stream'
    }

    $output = [IO.MemoryStream]::new()
    try {
        foreach ($frame in $frames) {
            $frameLength = $frame.Length + 7
            $header = [byte[]]@(
                0xff,
                0xf1,
                ((($audioObjectType - 1) -shl 6) -bor
                    ($frequencyIndex -shl 2) -bor
                    ($channelConfig -shr 2)),
                ((($channelConfig -band 3) -shl 6) -bor
                    ($frameLength -shr 11)),
                (($frameLength -shr 3) -band 0xff),
                ((($frameLength -band 7) -shl 5) -bor 0x1f),
                0xfc)
            $output.Write($header, 0, $header.Length)
            $output.Write($frame, 0, $frame.Length)
        }
        [IO.File]::WriteAllBytes($audioPath, $output.ToArray())
    } finally {
        $output.Dispose()
    }

    [pscustomobject]@{
        File = $audioPath
        Frames = $frames.Count
        AudioSpecificConfig = [BitConverter]::ToString($config)
    }
}

function Measure-ToneMagnitude {
    param(
        [string]$Recording,
        [int]$AudioIndex,
        [double]$Frequency,
        [double]$StartSeconds = 0
    )

    $rawPath = "$runRoot/audio-$AudioIndex-$Frequency.raw"
    & ffmpeg -v error -y -i $Recording -ss $StartSeconds -t 5 `
        -map "0:a:$AudioIndex" -ac 1 -ar 48000 -f s16le $rawPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to decode audio stream $AudioIndex"
    }

    $bytes = [IO.File]::ReadAllBytes($rawPath)
    $sampleCount = [Math]::Min([int]($bytes.Length / 2), 48000 * 5)
    $startSample = [Math]::Min(48000, [Math]::Max(0, $sampleCount / 4))
    $usable = $sampleCount - $startSample
    if ($usable -lt 48000) {
        throw "Audio stream $AudioIndex is too short for frequency analysis"
    }

    $real = 0.0
    $imaginary = 0.0
    $step = 2.0 * [Math]::PI * $Frequency / 48000.0
    for ($i = 0; $i -lt $usable; $i++) {
        $offset = ($startSample + $i) * 2
        $sample = [BitConverter]::ToInt16($bytes, $offset)
        $angle = $step * $i
        $real += $sample * [Math]::Cos($angle)
        $imaginary -= $sample * [Math]::Sin($angle)
    }

    return 2.0 * [Math]::Sqrt(
        $real * $real + $imaginary * $imaginary) / $usable
}

function Stop-TestProcess {
    param([Diagnostics.Process]$Process)

    if (-not $Process) {
        return
    }

    $Process.Refresh()
    if (-not $Process.HasExited) {
        $Process.Kill()
        [void]$Process.WaitForExit(10000)
    }
}

function Add-ResourceSample {
    param([Diagnostics.Process]$Process)

    if (-not $Qualification) {
        return
    }

    $now = Get-Date
    if (($now - $script:lastResourceSample).TotalSeconds -lt 30) {
        return
    }

    $Process.Refresh()
    $processCpuSeconds = $Process.TotalProcessorTime.TotalSeconds
    $processCpuPercent = $null
    if ($null -ne $script:lastProcessCpuSeconds -and
        $script:lastResourceSample -ne [datetime]::MinValue) {
        $elapsed = ($now - $script:lastResourceSample).TotalSeconds
        $processCpuPercent = 100.0 *
            ($processCpuSeconds - $script:lastProcessCpuSeconds) /
            ($elapsed * [Environment]::ProcessorCount)
    }

    $systemCpuPercent = [OBSPTSystemMetrics]::SampleCpuPercent()
    if ($systemCpuPercent -lt 0) {
        $systemCpuPercent = $null
    }
    $freeMemoryMB = [OBSPTSystemMetrics]::AvailableMemoryMB()
    if ($freeMemoryMB -lt 0) {
        $freeMemoryMB = $null
    }

    $gpuPercent = $null
    $encoderPercent = $null
    $gpuMemoryUsedMB = $null
    $gpuMemoryTotalMB = $null
    if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
        $gpuLine = & nvidia-smi `
            --query-gpu=utilization.gpu,utilization.encoder,memory.used,memory.total `
            --format=csv,noheader,nounits 2>$null | Select-Object -First 1
        if ($LASTEXITCODE -eq 0 -and $gpuLine) {
            $gpu = $gpuLine.Trim() -split ',\s*'
            if ($gpu.Count -eq 4) {
                $gpuPercent = [double]$gpu[0]
                $encoderPercent = [double]$gpu[1]
                $gpuMemoryUsedMB = [double]$gpu[2]
                $gpuMemoryTotalMB = [double]$gpu[3]
            }
        }
    }

    [void]$script:resourceSamples.Add([pscustomobject]@{
        Time = $now.ToString('o')
        ProcessCpuPercent = $processCpuPercent
        SystemCpuPercent = $systemCpuPercent
        WorkingSetMB = $Process.WorkingSet64 / 1MB
        FreeMemoryMB = $freeMemoryMB
        GpuPercent = $gpuPercent
        EncoderPercent = $encoderPercent
        GpuMemoryUsedMB = $gpuMemoryUsedMB
        GpuMemoryTotalMB = $gpuMemoryTotalMB
    })
    [IO.File]::WriteAllText(
        "$runRoot/resource-samples.json",
        ($script:resourceSamples | ConvertTo-Json -Depth 4),
        [Text.UTF8Encoding]::new($false))
    $script:lastResourceSample = $now
    $script:lastProcessCpuSeconds = $processCpuSeconds
}

function Get-Percentile {
    param([object[]]$Values, [double]$Percentile)

    $sorted = @($Values | Where-Object { $null -ne $_ } | Sort-Object)
    if ($sorted.Count -eq 0) {
        return $null
    }
    $index = [Math]::Max(
        0,
        [Math]::Ceiling($sorted.Count * $Percentile) - 1)
    return [double]$sorted[$index]
}

function Wait-ForQualificationHeadroom {
    if (-not $Qualification) {
        return
    }

    $longQualification = $DurationSeconds -ge 1800
    $requiredStableSamples = if ($longQualification) { 60 } else { 3 }
    $deadline = (Get-Date).AddMinutes(
        $(if ($longQualification) { 30 } else { 10 }))
    $stableSamples = 0
    [OBSPTSystemMetrics]::Reset()
    [void][OBSPTSystemMetrics]::SampleCpuPercent()
    do {
        Start-Sleep -Seconds 5
        $cpu = [OBSPTSystemMetrics]::SampleCpuPercent()
        $freeMemory = [OBSPTSystemMetrics]::AvailableMemoryMB()
        $gpu = $null
        if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
            $gpuLine = & nvidia-smi --query-gpu=utilization.gpu `
                --format=csv,noheader,nounits 2>$null |
                Select-Object -First 1
            if ($LASTEXITCODE -eq 0 -and $gpuLine) {
                $gpu = [double]$gpuLine.Trim()
            }
        }

        if ($cpu -ge 0 -and $cpu -lt 20 -and
            $freeMemory -gt 4096 -and
            ($null -eq $gpu -or $gpu -lt 20)) {
            $stableSamples++
        } else {
            $stableSamples = 0
        }
    } while ($stableSamples -lt $requiredStableSamples -and
             (Get-Date) -lt $deadline)

    if ($stableSamples -lt $requiredStableSamples) {
        throw "System headroom did not stabilize before qualification: CPU=$cpu%, free=$freeMemory MB, GPU=$gpu%"
    }
    [OBSPTSystemMetrics]::Reset()
}

function Test-QualificationRecording {
    param([string]$Recording, [string]$LogContent)

    $failures = [Collections.Generic.List[string]]::new()
    $renderMatch = [regex]::Match(
        $LogContent,
        'Number of lagged frames due to rendering lag/stalls: \d+ \(([0-9.]+)%\)')
    $encodeMatch = [regex]::Match(
        $LogContent,
        'skipped frames due to encoding lag:\s+\d+/\d+ \(([0-9.]+)%\)')
    $renderLagPercent = if ($renderMatch.Success) {
        [double]$renderMatch.Groups[1].Value
    } else { 0.0 }
    $encodeLagPercent = if ($encodeMatch.Success) {
        [double]$encodeMatch.Groups[1].Value
    } else { 0.0 }
    if ($renderLagPercent -gt 0.5 -or $encodeLagPercent -gt 0.5) {
        [void]$failures.Add(
            "Lag limit exceeded: render=$renderLagPercent%, encode=$encodeLagPercent%")
    }

    $frameMatch = [regex]::Match(
        $LogContent,
        "Output 'adv_file_output': Total frames output: (\d+)")
    if (-not $frameMatch.Success) {
        throw 'Recording output frame count was not logged'
    }

    $timingProbe = & ffprobe -v error -show_entries `
        'stream=index,codec_type,start_time,duration:stream_tags=name' `
        -of json $Recording | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) {
        throw 'FFprobe could not read qualification stream timing'
    }
    $video = $timingProbe.streams |
        Where-Object codec_type -eq 'video' |
        Select-Object -First 1
    $audio = @($timingProbe.streams | Where-Object codec_type -eq 'audio')
    if (-not $video -or $audio.Count -ne 3) {
        throw 'Qualification recording does not contain one video and three audio streams'
    }

    $culture = [Globalization.CultureInfo]::InvariantCulture
    $videoStart = [double]::Parse($video.start_time, $culture)
    $videoEnd = $videoStart + [double]::Parse($video.duration, $culture)
    $streamDrift = foreach ($stream in $audio) {
        $audioStart = [double]::Parse($stream.start_time, $culture)
        $audioEnd = $audioStart + [double]::Parse($stream.duration, $culture)
        [pscustomobject]@{
            Name = $stream.tags.name
            StartDriftMs = 1000.0 * [Math]::Abs($audioStart - $videoStart)
            EndDriftMs = 1000.0 * [Math]::Abs($audioEnd - $videoEnd)
        }
    }
    $maxDriftMs = ($streamDrift | ForEach-Object {
        [Math]::Max($_.StartDriftMs, $_.EndDriftMs)
    } | Measure-Object -Maximum).Maximum
    if ($maxDriftMs -gt 50) {
        [void]$failures.Add(
            "Audio/video drift exceeded 50 ms: $maxDriftMs ms")
    }

    $decodeOutput = & ffmpeg -v error -i $Recording -map '0:a' `
        -f null - 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $decodeOutput.Trim()) {
        [void]$failures.Add(
            "Full audio decode reported corruption: $decodeOutput")
    }
    foreach ($audioIndex in 1, 2) {
        $silenceOutput = & ffmpeg -hide_banner -nostats -i $Recording `
            -map "0:a:$audioIndex" `
            -af 'silencedetect=noise=-80dB:d=0.05' `
            -f null - 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $silenceOutput -match 'silence_start') {
            [void]$failures.Add(
                "Track $audioIndex contains a decode failure or silence gap")
        }
    }

    $endOffset = [Math]::Max(0, $DurationSeconds - 8)
    $endTone = [ordered]@{
        Mix440 = Measure-ToneMagnitude $Recording 0 440 $endOffset
        Mix880 = Measure-ToneMagnitude $Recording 0 880 $endOffset
        Game440 = Measure-ToneMagnitude $Recording 1 440 $endOffset
        Game880 = Measure-ToneMagnitude $Recording 1 880 $endOffset
        Music440 = Measure-ToneMagnitude $Recording 2 440 $endOffset
        Music880 = Measure-ToneMagnitude $Recording 2 880 $endOffset
    }
    if ($endTone.Mix440 -lt 10 -or $endTone.Mix880 -lt 10 -or
        $endTone.Game440 -lt 10 -or
        $endTone.Game440 -lt ($endTone.Game880 * 5) -or
        $endTone.Music880 -lt 10 -or
        $endTone.Music880 -lt ($endTone.Music440 * 5)) {
        [void]$failures.Add(
            "End-of-recording audio tracks are not isolated: $($endTone | ConvertTo-Json -Compress)")
    }

    $processCpu95 = Get-Percentile `
        @($resourceSamples | ForEach-Object ProcessCpuPercent) 0.95
    $systemCpu95 = Get-Percentile `
        @($resourceSamples | ForEach-Object SystemCpuPercent) 0.95
    $gpu95 = Get-Percentile `
        @($resourceSamples | ForEach-Object GpuPercent) 0.95
    $encoder95 = Get-Percentile `
        @($resourceSamples | ForEach-Object EncoderPercent) 0.95
    $minFreeMemory = ($resourceSamples | ForEach-Object FreeMemoryMB |
        Where-Object { $null -ne $_ } | Measure-Object -Minimum).Minimum
    $maxWorkingSet = ($resourceSamples | ForEach-Object WorkingSetMB |
        Measure-Object -Maximum).Maximum
    $minGpuFreeMemory = ($resourceSamples | Where-Object {
        $null -ne $_.GpuMemoryTotalMB
    } | ForEach-Object {
        $_.GpuMemoryTotalMB - $_.GpuMemoryUsedMB
    } | Measure-Object -Minimum).Minimum
    $driveName = [IO.Path]::GetPathRoot($runRoot).Substring(0, 1)
    $diskFreeMB = (Get-PSDrive $driveName).Free / 1MB

    $headroom = [ordered]@{
        Samples = $resourceSamples.Count
        ProcessCpu95Percent = $processCpu95
        SystemCpu95Percent = $systemCpu95
        Gpu95Percent = $gpu95
        Encoder95Percent = $encoder95
        MaxWorkingSetMB = $maxWorkingSet
        MinFreeMemoryMB = $minFreeMemory
        MinGpuFreeMemoryMB = $minGpuFreeMemory
        DiskFreeAfterMB = $diskFreeMB
    }
    if ($resourceSamples.Count -lt 2 -or
        $systemCpu95 -ge 95 -or $gpu95 -ge 98 -or
        $encoder95 -ge 98 -or $minFreeMemory -lt 1024 -or
        $minGpuFreeMemory -lt 512 -or $diskFreeMB -lt 2048) {
        [void]$failures.Add(
            "Qualification lacked resource headroom: $($headroom | ConvertTo-Json -Compress)")
    }

    return [pscustomobject]@{
        Passed = $failures.Count -eq 0
        Failures = $failures
        RenderingLagPercent = $renderLagPercent
        EncodingLagPercent = $encodeLagPercent
        OutputFrames = [int]$frameMatch.Groups[1].Value
        MaxAVDriftMs = $maxDriftMs
        StreamDrift = $streamDrift
        EndToneAnalysis = $endTone
        ResourceHeadroom = $headroom
    }
}

function Wait-ForLogPattern {
    param(
        [Diagnostics.Process]$Process,
        [string]$Pattern,
        [int]$MinimumCount = 1,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 250
        $log = Get-ChildItem "$runRoot/obs-studio/logs" -File `
            -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($log) {
            $content = Read-SharedText $log.FullName
            if ([regex]::Matches($content, $Pattern).Count -ge $MinimumCount) {
                return [pscustomobject]@{ Log = $log.FullName; Content = $content }
            }
        }

        $Process.Refresh()
        if ($Process.HasExited) {
            throw "OBS-PT exited while waiting for log pattern: $Pattern"
        }
    } while ((Get-Date) -lt $deadline)

    throw "Timed out waiting for log pattern: $Pattern"
}

function Wait-ForResult {
    param([Diagnostics.Process]$Process)

    $deadline = (Get-Date).AddSeconds($DurationSeconds + 120)
    $completionPattern = if ($RecordingType -eq 'Custom') {
		'==== Recording Stop'
	} else { switch ($RecordingFormat) {
        'hybrid_mp4' { '\[mp4 output: .+\] MP4 file output complete\.' }
        default { "Output of file '.+' stopped" }
	} }
    $log = $null
    $content = ''
    do {
        Add-ResourceSample $Process
        Start-Sleep -Milliseconds 250
        $log = Get-ChildItem "$runRoot/obs-studio/logs" -File `
            -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($log) {
            $content = Read-SharedText $log.FullName
            if ($content -match $completionPattern) {
                return [pscustomobject]@{ Log = $log.FullName; Content = $content }
            }
        }

        $Process.Refresh()
        if ($Process.HasExited) {
            throw "OBS-PT exited before recording completed: $($Process.ExitCode)"
        }
    } while ((Get-Date) -lt $deadline)

    throw "Timed out waiting for $RecordingFormat recording completion"
}

function Test-ExistingConfigPreservation {
    $basic = "$runRoot/obs-studio/basic/profiles/PotPvP/basic.ini"
    $scenePath = "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    Set-IniValue $basic 'Video' 'BaseCX' '1280'
    Set-IniValue $basic 'Video' 'BaseCY' '720'
    Set-IniValue $basic 'Video' 'OutputCX' '1280'
    Set-IniValue $basic 'Video' 'OutputCY' '720'
    Set-IniValue $basic 'Video' 'FPSNum' '333'
    Set-IniValue $basic 'AdvOut' 'RecFormat' 'mkv'
    Set-IniValue $basic 'AdvOut' 'RecTracks' '5'
    Set-IniValue $basic 'AdvOut' 'Track2Name' 'Existing Game'

    $sceneData = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    $sceneData.DesktopAudioDevice1.mixers = 5
    $sceneData.DesktopAudioDevice1.name = 'Existing Desktop Audio'
    $sceneData.DesktopAudioDevice1.private_settings | Add-Member `
        -NotePropertyName obspt_preservation_sentinel `
        -NotePropertyValue 'keep-existing-scene'
    [IO.File]::WriteAllText(
        $scenePath,
        ($sceneData | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))

    $basicHash = (Get-FileHash $basic -Algorithm SHA256).Hash
    $sceneHash = (Get-FileHash $scenePath -Algorithm SHA256).Hash
    $script:obsProcess = Start-Process `
        -FilePath "$runRoot/bin/64bit/OBS-PT.exe" `
        -ArgumentList ($multiArgument + @(
            '--profile', 'PotPvP',
            '--collection', 'PotPvP',
            '--scene', 'PotPvP',
            '--minimize-to-tray',
            '--disable-updater',
            '--disable-missing-files-check')) `
        -WorkingDirectory "$runRoot/bin/64bit" `
        -WindowStyle Hidden `
        -PassThru

    $loaded = Wait-ForLogPattern $script:obsProcess 'Loaded scenes:'
    Start-Sleep -Seconds 2
    if ((Get-FileHash $basic -Algorithm SHA256).Hash -ne $basicHash) {
        throw 'Normal startup rewrote the existing Profile'
    }
    if ((Get-FileHash $scenePath -Algorithm SHA256).Hash -ne $sceneHash) {
        throw 'Normal startup rewrote the existing Scene Collection'
    }

    $loadedScene = [IO.File]::ReadAllText($scenePath) | ConvertFrom-Json
    if ($loadedScene.DesktopAudioDevice1.mixers -ne 5 -or
        $loadedScene.DesktopAudioDevice1.name -ne 'Existing Desktop Audio' -or
        $loadedScene.DesktopAudioDevice1.private_settings.obspt_preservation_sentinel -ne
            'keep-existing-scene') {
        throw 'Normal startup changed existing Scene Collection values'
    }

    [pscustomobject]@{
        Result = 'PASS'
        Scenario = $Scenario
        Runtime = $runRoot
        Log = $loaded.Log
        ProfileSHA256 = $basicHash
        SceneCollectionSHA256 = $sceneHash
    }
}

if (-not $AllowExistingInstances -and
    (Get-Process -Name OBS-PT -ErrorAction SilentlyContinue)) {
    throw 'Close every OBS-PT instance before running the smoke test'
}
$multiArgument = if ($AllowExistingInstances) { @('--multi') } else { @() }

$creationTrackScenario = $Scenario -eq 'creation-track'
$shippedDefaultsScenario = $Scenario -eq 'shipped-defaults'
$dualSourceScenario = $Scenario -in @(
    'dual', 'topology-change', 'creation-track', 'shipped-defaults')
$privateAudioScenario = $Scenario -eq 'private-audio'
$multiRootScenario = $Scenario -in @('multi-root', 'multi-root-lifecycle')
$vivaldiScenario = $Scenario -eq 'vivaldi'
$hasMusicFixture = $Scenario -in @(
    'dual', 'topology-change', 'muted', 'monitor-only')

try {
    if ($Scenario -in @(
        'start-later', 'restart', 'reconnecting-stop',
        'multi-root-lifecycle', 'vivaldi') -and
        $DurationSeconds -lt 15) {
        throw "$Scenario requires DurationSeconds >= 15"
    }
    if ($Scenario -eq 'topology-change' -and $DurationSeconds -lt 12) {
        throw 'topology-change requires DurationSeconds >= 12'
    }
    if ($Scenario -notin @(
        'capture', 'dual', 'topology-change', 'muted', 'monitor-only',
        'video-only', 'process-tree') -and
        $MatchPriority -notin @('exe', 'process-name')) {
        throw 'Non-capture lifecycle scenarios require an executable match mode'
    }
    if ($Qualification -and
        ($Scenario -ne 'dual' -or $RecordingFormat -ne 'hybrid_mp4' -or
         $DurationSeconds -lt 60)) {
        throw 'Qualification requires a dual Hybrid MP4 run of at least 60 seconds'
    }
	if ($Streaming -and ($Scenario -ne 'dual' -or
		$DurationSeconds -lt 12)) {
		throw 'Streaming smoke requires Scenario=dual and at least 12 seconds'
	}
	if ($VodTrack -and -not $Streaming) {
		throw 'VOD Track smoke requires -Streaming'
	}
	if ($ParallelVodFailure -and
		(-not $Streaming -or -not $VodTrack -or $Scenario -ne 'dual')) {
		throw 'Parallel VOD failure smoke requires Scenario=dual, Streaming, and VodTrack'
	}
	if ($BilibiliLocal -and -not $Streaming) {
		throw 'BilibiliLocal smoke requires -Streaming'
	}
	if ($BilibiliLocal -and $ParallelVodFailure) {
		throw 'BilibiliLocal smoke cannot combine with ParallelVodFailure'
	}
	if ($RecordingType -eq 'Custom' -and $OutputMode -ne 'Advanced') {
		throw 'Custom FFmpeg smoke requires Advanced output mode'
	}
	if ($ExpectedStartFailure -ne 'None' -and
		($RecordingType -ne 'Custom' -or $Scenario -ne 'dual')) {
		throw 'Expected start failure smoke requires Scenario=dual and RecordingType=Custom'
	}
	if ($ExpectedStartFailure -eq 'UnknownContainer' -and
		$CustomFormat -ne 'mystery-container') {
		throw 'UnknownContainer failure smoke requires CustomFormat=mystery-container'
	}
	if ($ExpectedStartFailure -in @('MissingVideoEncoder', 'MissingAudioEncoder') -and
		$CustomFormat -eq 'mystery-container') {
		throw 'Missing encoder failure smoke requires a supported custom container'
	}

    Prepare-PortableRuntime | Out-Null
    if ($Scenario -eq 'preserve-config') {
        Test-ExistingConfigPreservation
        return
    }
    $toneExe = Build-ToneApplication "$runRoot/bin/64bit"
    $musicExe = "$runRoot/bin/64bit/obspt-tone-music.exe"
    $privateExe = "$runRoot/bin/64bit/obspt-tone-private.exe"
	$defaultGameExe = "$runRoot/bin/64bit/javaw.exe"
	$defaultMusicExe = "$runRoot/bin/64bit/vivaldi.exe"
    if ($vivaldiScenario) {
        $vivaldiProfile = "$runRoot/vivaldi-profile"
        Start-VivaldiFixture
    }
    if ($hasMusicFixture -or $creationTrackScenario) {
        Copy-Item -Force $toneExe $musicExe
    }
    if ($privateAudioScenario) {
        Copy-Item -Force $toneExe $privateExe
    }
	if ($shippedDefaultsScenario) {
		Copy-Item -Force $toneExe $defaultGameExe
		Copy-Item -Force $toneExe $defaultMusicExe
		Set-ShippedDefaultsFixture
		$toneProcess = Start-ToneApplication `
			$defaultGameExe 'OBSPT_Default_Game' 440
		$musicProcess = Start-ToneApplication `
			$defaultMusicExe 'OBSPT_Default_Music' 880
	}

    if ($Scenario -in @(
        'capture',
        'dual',
        'topology-change',
        'muted',
        'monitor-only',
        'private-audio',
        'creation-track',
        'process-tree',
        'multi-root',
        'multi-root-lifecycle',
        'vivaldi',
        'restart',
        'reconnecting-stop')) {
        if (-not $vivaldiScenario) {
            $toneProcess = Start-ToneApplication $toneExe 'OBSPT_Tone_Game'
        }
    }
    $windowClass = if ($toneProcess -and
        $MatchPriority -notin @('exe', 'process-name')) {
        Get-ToneWindowClass $toneProcess
    } else {
        'Ignored'
    }
    if ($Scenario -eq 'video-only') {
        Set-VideoOnlyFixture
    } elseif (-not $shippedDefaultsScenario) {
        Add-ApplicationAudioFixture `
            $(if ($vivaldiScenario) { $VivaldiPath } else { $toneExe }) `
            ($Scenario -ne 'inactive') `
            $windowClass `
            $MatchPriority
    }
    if ($multiRootScenario) {
        $musicProcess = Start-ToneApplication `
            $toneExe `
            'OBSPT_Tone_Second_Root' `
            880
    }
    if ($hasMusicFixture) {
        Add-MusicAudioFixture $musicExe
        $musicProcess = Start-ToneApplication `
            $musicExe `
            'OBSPT_Tone_Music' `
            880
    }
    if ($creationTrackScenario) {
        $musicProcess = Start-ToneApplication `
            $musicExe `
            'OBSPT_Tone_Creation' `
            880
        Add-CreationTrackFixture $musicExe
    }
    if ($privateAudioScenario) {
        $privateProcess = Start-ToneApplication `
            $privateExe `
            'OBSPT_Tone_Private' `
            880
        Add-PrivateAudioFixture $privateExe
    }
    if ($Scenario -in @('muted', 'monitor-only')) {
        Set-ExcludedMusicFixture $Scenario
    }
    if ($Scenario -eq 'topology-change') {
        Add-TopologyMutationFixture
    }

    Wait-ForQualificationHeadroom
	if ($Streaming -and -not $ParallelVodFailure) {
		$streamCapture = "$runRoot/stream.flv"
		$streamUrl = "rtmp://127.0.0.1:$streamPort/live/test"
		$streamServer = Start-Process ffmpeg `
			-ArgumentList @(
				'-hide_banner', '-loglevel', 'error', '-listen', '1',
				'-i', $streamUrl, '-map', '0', '-c', 'copy', $streamCapture) `
			-WindowStyle Hidden `
			-PassThru
		Start-Sleep -Seconds 1
	}

    $startArguments = [Collections.Generic.List[string]]::new()
    if ($AllowExistingInstances) {
        [void]$startArguments.Add('--multi')
    }
    [void]$startArguments.Add('--startrecording')
    if ($ReplayBuffer) {
        [void]$startArguments.Add('--startreplaybuffer')
    }
	if ($Streaming -and -not $ParallelVodFailure) {
		[void]$startArguments.Add('--startstreaming')
	}
    foreach ($argument in @(
        '--profile', 'PotPvP',
        '--collection', 'PotPvP',
        '--scene', 'PotPvP',
        '--minimize-to-tray',
        '--disable-updater',
        '--disable-missing-files-check')) {
        [void]$startArguments.Add($argument)
    }

    $obsProcess = Start-Process `
        -FilePath "$runRoot/bin/64bit/OBS-PT.exe" `
        -ArgumentList $startArguments `
        -WorkingDirectory "$runRoot/bin/64bit" `
        -WindowStyle Hidden `
        -PassThru

	if ($ExpectedStartFailure -ne 'None') {
		$dialogTitle = $null
		$deadline = (Get-Date).AddSeconds(30)
		do {
			Start-Sleep -Milliseconds 250
			$dialogTitle = [OBSPTTestWindows]::CloseFailureDialog(
				$obsProcess.Id)
		} while (-not $dialogTitle -and (Get-Date) -lt $deadline -and
			 -not $obsProcess.HasExited)
		if (-not $dialogTitle) {
			throw "Expected recording start failure dialog was not shown ($ExpectedStartFailure)"
		}
		Start-Sleep -Seconds 1
		$artifacts = @(Get-ChildItem "$runRoot/recordings" -File |
			Where-Object Length -gt 0)
		if ($artifacts.Count -ne 0) {
			throw "Rejected recording start left $($artifacts.Count) non-empty artifact(s)"
		}
		[pscustomobject]@{
			Result = 'PASS'
			Scenario = $Scenario
			ExpectedStartFailure = $ExpectedStartFailure
			CustomFormat = $CustomFormat
			DialogTitle = $dialogTitle
			RecordingArtifacts = $artifacts.Count
			Runtime = $runRoot
		} | Format-List
		return
	}

    $initializedPattern =
        "WASAPI: Device '\[VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK\]' \[\d+ Hz\] initialized"
    $rootAddedPattern =
        "\[win-wasapi: '[^']+'\] Added process audio session root"
    $missingPattern = if ($MatchPriority -eq 'process-name') {
        '\[WASAPISource::TryInitialize\].*No matching active audio session'
    } else {
        '\[WASAPISource::TryInitialize\].*Failed to find window'
    }

    switch ($Scenario) {
    'capture' {
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
    }
    'dual' {
        Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
    }
    'topology-change' {
        Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
        Wait-ForLogPattern $obsProcess 'OBSPT topology source created' |
            Out-Null
        Wait-ForLogPattern $obsProcess 'OBSPT topology source removed' |
            Out-Null
    }
    'muted' {
        Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
    }
    'monitor-only' {
        Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
    }
    'private-audio' {
        Wait-ForLogPattern $obsProcess 'OBSPT private audio source active' |
            Out-Null
        Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
    }
    'creation-track' {
        Wait-ForLogPattern $obsProcess `
		'OBSPT creation-track source created' | Out-Null
        Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
    }
    'video-only' {
        # OutputTimer supplies the observation window.
    }
    'process-tree' {
        $child = Get-CimInstance Win32_Process `
            -Filter "ParentProcessId=$($toneProcess.Id)" |
            Where-Object Name -eq 'obspt-tone-game.exe' |
            Select-Object -First 1
        if (-not $child) {
            throw 'Tone parent did not create its audio-only child process'
        }
        $childToneProcessId = $child.ProcessId
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
    }
    { $_ -in @('multi-root', 'multi-root-lifecycle') } {
        Wait-ForLogPattern $obsProcess $rootAddedPattern 2 | Out-Null
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
        if ($Scenario -eq 'multi-root-lifecycle') {
            Start-Sleep -Seconds 2
            Stop-TestProcess $musicProcess
            $musicProcess = $null
            Wait-ForLogPattern $obsProcess 'Removed process audio session root' |
                Out-Null
            $musicProcess = Start-ToneApplication `
                $toneExe `
                'OBSPT_Tone_Second_Root_Restarted' `
                880
            Wait-ForLogPattern $obsProcess $rootAddedPattern 3 | Out-Null
        }
    }
    'start-later' {
        Wait-ForLogPattern $obsProcess $missingPattern | Out-Null
        $toneProcess = Start-ToneApplication $toneExe 'OBSPT_Tone_Game'
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
    }
    'vivaldi' {
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
        Start-Sleep -Seconds 3
        Stop-VivaldiFixture
        Wait-ForLogPattern $obsProcess 'Removed process audio session root' |
            Out-Null
        Start-VivaldiFixture
        Wait-ForLogPattern $obsProcess $rootAddedPattern 2 | Out-Null
    }
    'restart' {
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
        Start-Sleep -Seconds 2
        Stop-TestProcess $toneProcess
        $toneProcess = $null
        if ($MatchPriority -eq 'process-name') {
            Wait-ForLogPattern $obsProcess 'Removed process audio session root' |
                Out-Null
        } else {
            Wait-ForLogPattern $obsProcess 'window disappeared' | Out-Null
        }
        $toneProcess = Start-ToneApplication `
            $toneExe `
            'OBSPT_Tone_Game_Restarted'
        if ($MatchPriority -eq 'process-name') {
            Wait-ForLogPattern $obsProcess $rootAddedPattern 2 | Out-Null
        } else {
            Wait-ForLogPattern $obsProcess $initializedPattern 2 | Out-Null
        }
    }
    'missing-stop' {
        Wait-ForLogPattern $obsProcess $missingPattern | Out-Null
    }
    'reconnecting-stop' {
        Wait-ForLogPattern $obsProcess $initializedPattern | Out-Null
        Start-Sleep -Seconds 2
        Stop-TestProcess $toneProcess
        $toneProcess = $null
        if ($MatchPriority -eq 'process-name') {
            Wait-ForLogPattern $obsProcess 'Removed process audio session root' |
                Out-Null
        } else {
            Wait-ForLogPattern $obsProcess 'window disappeared' | Out-Null
        }
    }
    'inactive' {
        # The output timer provides the observation window. The assertions below
        # verify that an inactive Source never attempts initialization.
    }
    }
	if ($ParallelVodFailure) {
		Wait-ForLogPattern $obsProcess '==== Recording Start' | Out-Null
		$activeRecording = Get-ChildItem "$runRoot/recordings" -File |
			Sort-Object LastWriteTime -Descending | Select-Object -First 1
		if (-not $activeRecording) {
			throw 'Parallel failure smoke did not create the first recording'
		}
		[OBSPTTestKeyboard]::Press(0x79)
		$deadline = (Get-Date).AddSeconds(30)
		do {
			Start-Sleep -Milliseconds 250
			$parallelFailureDialogTitle =
				[OBSPTTestWindows]::CloseFailureDialog($obsProcess.Id)
		} while (-not $parallelFailureDialogTitle -and
			 (Get-Date) -lt $deadline -and -not $obsProcess.HasExited)
		if (-not $parallelFailureDialogTitle) {
			throw 'Incompatible VOD output did not show a start failure dialog'
		}
		Start-Sleep -Seconds 2
		$liveLog = Get-ChildItem "$runRoot/obs-studio/logs" -File |
			Sort-Object LastWriteTime -Descending | Select-Object -First 1
		$liveContent = Read-SharedText $liveLog.FullName
		$obsProcess.Refresh()
		if ($obsProcess.HasExited -or
			$liveContent -match '==== Recording Stop') {
			throw 'First recording stopped after the VOD start failure'
		}
	}

    if ($ReplayBuffer) {
        Wait-ForLogPattern $obsProcess '==== Replay Buffer Start' |
            Out-Null
        Start-Sleep -Seconds ([Math]::Max(5, [Math]::Floor(
            $DurationSeconds / 2)))
        [OBSPTTestKeyboard]::Press(0x77)
        Wait-ForLogPattern $obsProcess 'Wrote replay buffer to' |
            Out-Null
        [OBSPTTestKeyboard]::Press(0x78)
        Wait-ForLogPattern $obsProcess '==== Replay Buffer Stop' |
            Out-Null
    }
	if ($Streaming -and -not $ParallelVodFailure) {
		Wait-ForLogPattern $obsProcess '==== Streaming Start' | Out-Null
		Start-Sleep -Seconds 3
		foreach ($key in 0x70, 0x72, 0x74) {
			[OBSPTTestKeyboard]::Press($key)
		}
		Start-Sleep -Seconds 3
		foreach ($key in 0x71, 0x73, 0x75) {
			[OBSPTTestKeyboard]::Press($key)
		}
		Start-Sleep -Seconds ([Math]::Max(3, $DurationSeconds - 6))
		[OBSPTTestKeyboard]::Press(0x76)
		Wait-ForLogPattern $obsProcess '==== Streaming Stop' | Out-Null
		if (-not $streamServer.WaitForExit(15000)) {
			throw 'Local RTMP endpoint did not close after streaming stopped'
		}
		if ($streamServer.ExitCode -ne 0 -or
		    -not (Test-Path $streamCapture)) {
			throw 'Local RTMP endpoint did not produce a valid capture'
		}
	}

    $result = Wait-ForResult $obsProcess
	if ($ParallelVodFailure) {
		$recordStarts = [regex]::Matches(
			$result.Content, '==== Recording Start').Count
		$recordStops = [regex]::Matches(
			$result.Content, '==== Recording Stop').Count
		$streamStarts = [regex]::Matches(
			$result.Content, '==== Streaming Start').Count
		if ($recordStarts -ne 1 -or $recordStops -ne 1 -or
			$streamStarts -ne 0) {
			throw 'Second-output failure changed the first recording lifecycle'
		}
	}
	if ($Streaming -and -not $ParallelVodFailure) {
		$streamStarts = [regex]::Matches(
			$result.Content, '==== Streaming Start').Count
		if ($streamStarts -ne 1 -or
		    $result.Content -match 'reconnecting|Reconnect successful') {
			throw 'Streaming connection was recreated during mute/unmute'
		}
		$streamProbe = & ffprobe -v error -show_entries `
			'stream=codec_type,codec_name:format=duration' `
			-of json $streamCapture | ConvertFrom-Json
		$streamAudio = @($streamProbe.streams |
			Where-Object codec_type -eq 'audio')
		$streamData = @($streamProbe.streams |
			Where-Object codec_type -eq 'data')
		if ($streamAudio.Count -ne 1 -or
		    @($streamAudio | Where-Object codec_name -ne 'aac').Count -ne 0) {
			throw 'Streaming output does not contain one AAC Compatibility Mix'
		}
		$streamToneAnalysis = $null
		if ($BilibiliLocal) {
			if ($streamData.Count -ne 0) {
				throw 'Bilibili Live unexpectedly emitted a VOD data stream'
			}
		} elseif ($VodTrack) {
			if ($streamData.Count -ne 1) {
				throw 'VOD streaming output does not contain additional media'
			}
			$vodExport = Export-VodTrackAudio $streamCapture
			$streamToneAnalysis = [ordered]@{
				Mix440 = Measure-ToneMagnitude $streamCapture 0 440 0
				Mix880 = Measure-ToneMagnitude $streamCapture 0 880 0
				Vod440 = Measure-ToneMagnitude $vodExport.File 0 440 0
				Vod880 = Measure-ToneMagnitude $vodExport.File 0 880 0
				Vod880AfterUnmute = Measure-ToneMagnitude $vodExport.File 0 880 8
				VodFrames = $vodExport.Frames
			}
			if ($streamToneAnalysis.Mix440 -lt 10 -or
			    $streamToneAnalysis.Mix880 -lt 10 -or
			    $streamToneAnalysis.Vod880 -lt 10 -or
			    $streamToneAnalysis.Vod880 -lt
			        ($streamToneAnalysis.Vod440 * 5) -or
			    $streamToneAnalysis.Vod880AfterUnmute -lt 10) {
				throw "VOD Track routing is invalid: $($streamToneAnalysis | ConvertTo-Json -Compress)"
			}
			$vodVolumes = [ordered]@{}
			foreach ($window in @(
				@('BeforeMute', 1), @('Muted', 4), @('AfterUnmute', 8))) {
				$volumeOutput = & ffmpeg -hide_banner -ss $window[1] -t 1 `
					-i $vodExport.File -map '0:a:0' -af volumedetect `
					-f null - 2>&1 | Out-String
				$match = [regex]::Match(
					$volumeOutput,
					'mean_volume:\s+(-?inf|-?[0-9.]+) dB')
				if (-not $match.Success) {
					throw "Could not measure VOD volume for $($window[0])"
				}
				$vodVolumes[$window[0]] = if (
					$match.Groups[1].Value -eq '-inf') {
					-200.0
				} else { [double]$match.Groups[1].Value }
			}
			if ($vodVolumes.BeforeMute -lt -80 -or
			    $vodVolumes.Muted -gt -80 -or
			    $vodVolumes.AfterUnmute -lt -80) {
				throw "VOD mute timeline is invalid: $($vodVolumes | ConvertTo-Json -Compress)"
			}
			$streamToneAnalysis['VodVolumes'] = $vodVolumes
		}
		$streamVolumes = [ordered]@{}
		foreach ($window in @(
			@('BeforeMute', 1), @('Muted', 4), @('AfterUnmute', 8))) {
			$volumeOutput = & ffmpeg -hide_banner -ss $window[1] -t 1 `
				-i $streamCapture -map '0:a:0' -af volumedetect `
				-f null - 2>&1 | Out-String
			$match = [regex]::Match(
				$volumeOutput, 'mean_volume:\s+(-?inf|-?[0-9.]+) dB')
			if (-not $match.Success) {
				throw "Could not measure stream volume for $($window[0])"
			}
			$streamVolumes[$window[0]] = if (
				$match.Groups[1].Value -eq '-inf') {
				-200.0
			} else { [double]$match.Groups[1].Value }
		}
		if ($streamVolumes.BeforeMute -lt -80 -or
		    $streamVolumes.Muted -gt -80 -or
		    $streamVolumes.AfterUnmute -lt -80) {
			throw "Stream mute timeline is invalid: $($streamVolumes | ConvertTo-Json -Compress)"
		}
	}
    if ($result.Content -match "Source ID 'wasapi_process_output_capture' not found") {
        throw 'Application Audio Source was not registered'
    }
    $initializationCount =
        [regex]::Matches($result.Content, $initializedPattern).Count
    $missingCount = [regex]::Matches($result.Content, $missingPattern).Count

    if ($Scenario -in @(
        'capture',
        'dual',
        'topology-change',
        'muted',
        'monitor-only',
        'private-audio',
        'creation-track',
        'shipped-defaults',
        'process-tree',
        'multi-root',
        'multi-root-lifecycle',
        'start-later') -and
        $initializationCount -lt 1) {
        throw 'Process-loopback device did not initialize'
    }
    if ($hasMusicFixture -and $initializationCount -lt 2) {
        throw 'Both Application Audio Sources did not initialize'
    }
	if ($shippedDefaultsScenario -and $initializationCount -lt 2) {
		throw 'Both shipped Application Audio Sources did not initialize'
	}
    if ($privateAudioScenario -and $initializationCount -lt 2) {
        throw 'Managed and private Application Audio Sources did not initialize'
    }
    if ($Scenario -eq 'restart' -and
        $MatchPriority -ne 'process-name' -and $initializationCount -lt 2) {
        throw 'Process-loopback device did not reinitialize after restart'
    }
    if ($Scenario -in @('start-later', 'missing-stop') -and
        $missingCount -ne 1) {
        throw "Expected one missing-target warning, found $missingCount"
    }
    if ($Scenario -eq 'inactive' -and
        ($initializationCount -ne 0 -or $missingCount -gt 1)) {
        throw 'Inactive Application Audio Source continued reconnecting'
    }

    $savedScene = [IO.File]::ReadAllText(
        "$runRoot/obs-studio/basic/scenes/PotPvP.json") |
        ConvertFrom-Json
    $sceneItemNames = @($savedScene.sources |
        Where-Object id -eq 'scene' |
        ForEach-Object { $_.settings.items } |
        ForEach-Object { $_.name })
    foreach ($audioSourceName in @(
            'Game Audio', 'Music Audio', 'Creation Music')) {
        $savedAudioSource = @($savedScene.sources |
            Where-Object name -eq $audioSourceName)
        if ($savedAudioSource.Count -gt 0 -and
            $sceneItemNames -contains $audioSourceName) {
            throw "$audioSourceName was persisted as a Scene Item"
        }
    }
    if ($Scenario -ne 'video-only' -and -not $shippedDefaultsScenario) {
		$primarySourceName = if ($vivaldiScenario) {
			'Music Audio'
		} else {
			'Game Audio'
		}
		$primaryTrack = if ($vivaldiScenario) { 3 } else { 2 }
        $savedGameSource = @($savedScene.sources |
            Where-Object name -eq $primarySourceName)
        $savedGameRoute = @($savedScene.obspt_audio_routing.sources |
            Where-Object name -eq $primarySourceName)
        if ($savedGameSource.Count -ne 1 -or
            $savedGameRoute.Count -ne 1 -or
            $savedGameRoute[0].editor_track -ne $primaryTrack) {
            throw "Collection-owned $primarySourceName did not survive save with its route"
        }
    }
	if ($shippedDefaultsScenario) {
		foreach ($item in @(
			@('minecraft-sound', 'obspt-default-minecraft-sound', 2),
			@('music', 'obspt-default-music', 3))) {
			$source = @($savedScene.sources | Where-Object name -eq $item[0])
			$route = @($savedScene.obspt_audio_routing.sources |
				Where-Object id -eq $item[1])
			if ($source.Count -ne 1 -or $route.Count -ne 1 -or
				$route[0].editor_track -ne $item[2]) {
				throw "Shipped source did not survive save: $($item[0])"
			}
		}
	}

    if ($privateAudioScenario) {
        if (@($savedScene.obspt_audio_routing.sources |
                Where-Object name -eq 'Private Audio').Count -ne 0) {
            throw 'Private audio appeared in the manageable routing model'
        }
    }
    if ($creationTrackScenario) {
        $createdRoute = @($savedScene.obspt_audio_routing.sources |
            Where-Object name -eq 'Creation Music')
        if ($createdRoute.Count -ne 1 -or
            $createdRoute[0].editor_track -ne 3) {
            throw 'Creation-time Track 3 selection was not persisted'
        }
        $createdSource = @($savedScene.sources |
            Where-Object name -eq 'Creation Music')
        if ($createdSource.Count -ne 1 -or
            ($createdSource[0].settings.PSObject.Properties.Name) -contains
                'obspt_initial_editor_track') {
            throw 'Creation-time routing setting leaked into source persistence'
        }
    }
    if ($Scenario -eq 'dual') {
        $savedGame = $savedScene.sources |
            Where-Object name -eq 'Game Audio' |
            Select-Object -First 1
        $savedMusic = $savedScene.sources |
            Where-Object name -eq 'Music Audio' |
            Select-Object -First 1
        if ($savedScene.DesktopAudioDevice1.mixers -ne 1 -or
            $savedGame.mixers -ne 2 -or $savedMusic.mixers -ne 4) {
            throw 'Desktop authority did not de-duplicate Application Audio mixer bits'
        }
    }

    $effectiveFormat = if ($RecordingType -eq 'Custom') {
		switch ($CustomFormat) {
			'matroska' { 'mkv' }
			'mpegts' { 'ts' }
			default { $CustomFormat }
		}
	} else { $RecordingFormat }
    $recordingExtension = if ($effectiveFormat -eq 'hybrid_mp4') {
        '.mp4'
    } else {
        ".$effectiveFormat"
    }
    $recording = $null
    $recordingDeadline = (Get-Date).AddSeconds(10)
    do {
        $recording = Get-ChildItem "$runRoot/recordings" -File |
            Where-Object Extension -eq $recordingExtension |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($recording -and $recording.Length -gt 0) {
            break
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $recordingDeadline)
    if (-not $recording -or $recording.Length -eq 0) {
        throw "$effectiveFormat recording was not created"
    }
	$replayEvidence = @()
	if ($ReplayBuffer) {
		$recordingFiles = @(Get-ChildItem "$runRoot/recordings" -File |
			Where-Object Extension -eq $recordingExtension)
		if ($recordingFiles.Count -lt 2) {
			throw 'Recording and Replay Buffer did not produce separate files'
		}
		$normalRecording = $recordingFiles |
			Where-Object Name -NotLike 'Replay *' |
			Sort-Object LastWriteTime -Descending |
			Select-Object -First 1
		if ($normalRecording) {
			$recording = $normalRecording
		}
	}

    $probe = & ffprobe -v error -show_entries `
        'stream=index,codec_type,codec_name,bit_rate:stream_disposition=default:stream_tags=name,title:format=duration' `
        -of json $recording.FullName | ConvertFrom-Json
    $videoStreams = @($probe.streams | Where-Object codec_type -eq 'video')
    $audioStreams = @($probe.streams | Where-Object codec_type -eq 'audio')
    if ($videoStreams.Count -ne 1) {
        throw "Expected one video stream, found $($videoStreams.Count)"
    }
    $multiTrackFormat = $effectiveFormat -in @(
        'hybrid_mp4', 'mp4', 'mov', 'mkv', 'ts')
    $supportsAudioMetadata = $effectiveFormat -in @(
        'hybrid_mp4', 'mp4', 'mov', 'mkv')
    $expectedAudioStreams = if ($Scenario -eq 'video-only') {
        0
    } elseif (-not $multiTrackFormat) {
        1
    } elseif ($dualSourceScenario) {
        3
    } else {
        2
    }
    if ($audioStreams.Count -ne $expectedAudioStreams -or
        @($audioStreams | Where-Object codec_name -ne 'aac').Count -ne 0) {
        throw "Expected $expectedAudioStreams AAC streams, found $($audioStreams.Count)"
    }
    if ($RecordingType -eq 'Standard' -and $expectedAudioStreams -gt 0) {
        $configuredEncoderCount = [regex]::Matches(
            $result.Content,
            "(?i)\[[^\]]*aac encoder: '[^']+'\] bitrate:\s+$AudioBitrate,").Count
        if ($configuredEncoderCount -lt $expectedAudioStreams) {
            throw "Only $configuredEncoderCount of $expectedAudioStreams audio encoders used $AudioBitrate Kbps"
        }
    }
    foreach ($audioStream in $audioStreams) {
        if ($audioStream.bit_rate -and [int64]$audioStream.bit_rate -le 0) {
            throw 'Audio stream bitrate metadata is invalid'
        }
    }
    $trackNames = @($audioStreams | ForEach-Object {
        if ($_.tags.name) { $_.tags.name } else { $_.tags.title }
    })
    $expectedTrackNames = if ($Scenario -eq 'video-only') {
        ''
    } elseif (-not $multiTrackFormat) {
        'Mix'
    } elseif ($dualSourceScenario) {
        'Mix,Game,Music'
    } elseif ($vivaldiScenario) {
        'Mix,Music'
    } else {
        'Mix,Game'
    }
    if ($supportsAudioMetadata -and
        ($trackNames -join ',') -ne $expectedTrackNames) {
        throw "Unexpected audio track names: $($trackNames -join ',')"
    }
    if ($supportsAudioMetadata -and $audioStreams.Count -gt 0 -and
        ($audioStreams[0].disposition.default -ne 1 -or
        @($audioStreams | Select-Object -Skip 1 | Where-Object {
            $_.disposition.default -ne 0
        }).Count -ne 0)) {
        throw 'Only Mix may be the default playback audio stream'
    }
    $recordedDuration = [double]::Parse(
        $probe.format.duration,
        [Globalization.CultureInfo]::InvariantCulture)
    if ($recordedDuration -lt ($DurationSeconds - 2)) {
        throw "Recording is too short: $recordedDuration of $DurationSeconds seconds"
    }

    $toneAnalysis = $null
    if ($privateAudioScenario) {
        $toneAnalysis = [ordered]@{
            Mix440 = Measure-ToneMagnitude $recording.FullName 0 440
            Mix880 = Measure-ToneMagnitude $recording.FullName 0 880
        }
        if ($multiTrackFormat) {
            $toneAnalysis['Game440'] =
                Measure-ToneMagnitude $recording.FullName 1 440
            $toneAnalysis['Game880'] =
                Measure-ToneMagnitude $recording.FullName 1 880
        }
        if ($toneAnalysis.Mix440 -lt 10 -or
            $toneAnalysis.Mix880 -lt 10 -or
            ($multiTrackFormat -and
             ($toneAnalysis.Game440 -lt 10 -or
              $toneAnalysis.Game440 -lt ($toneAnalysis.Game880 * 5)))) {
            throw "Private audio routing is invalid: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    } elseif ($dualSourceScenario -and $multiTrackFormat) {
        $toneAnalysis = [ordered]@{
            Mix440 = Measure-ToneMagnitude $recording.FullName 0 440
            Mix880 = Measure-ToneMagnitude $recording.FullName 0 880
            Game440 = Measure-ToneMagnitude $recording.FullName 1 440
            Game880 = Measure-ToneMagnitude $recording.FullName 1 880
            Music440 = Measure-ToneMagnitude $recording.FullName 2 440
            Music880 = Measure-ToneMagnitude $recording.FullName 2 880
        }
        if ($toneAnalysis.Mix440 -lt 10 -or $toneAnalysis.Mix880 -lt 10 -or
            $toneAnalysis.Game440 -lt 10 -or
            $toneAnalysis.Game440 -lt ($toneAnalysis.Game880 * 5) -or
            $toneAnalysis.Music880 -lt 10 -or
            $toneAnalysis.Music880 -lt ($toneAnalysis.Music440 * 5)) {
            throw "Audio tracks are not isolated: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    } elseif ($dualSourceScenario) {
        $toneAnalysis = [ordered]@{
            Mix440 = Measure-ToneMagnitude $recording.FullName 0 440
            Mix880 = Measure-ToneMagnitude $recording.FullName 0 880
        }
        if ($toneAnalysis.Mix440 -lt 10 -or
            $toneAnalysis.Mix880 -lt 10) {
            throw "Mix-only output is incomplete: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    } elseif ($multiRootScenario) {
        $toneAnalysis = [ordered]@{
            Mix440 = Measure-ToneMagnitude $recording.FullName 0 440
            Mix880 = Measure-ToneMagnitude $recording.FullName 0 880
            Game440 = Measure-ToneMagnitude $recording.FullName 1 440
            Game880 = Measure-ToneMagnitude $recording.FullName 1 880
        }
        if ($toneAnalysis.Mix440 -lt 10 -or
            $toneAnalysis.Mix880 -lt 10 -or
            $toneAnalysis.Game440 -lt 10 -or
            $toneAnalysis.Game880 -lt 10) {
            throw "Executable-wide roots are incomplete: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
        if ($Scenario -eq 'multi-root-lifecycle') {
            $toneAnalysis['Game440End'] =
                Measure-ToneMagnitude $recording.FullName 1 440 ($DurationSeconds - 2)
            $toneAnalysis['Game880End'] =
                Measure-ToneMagnitude $recording.FullName 1 880 ($DurationSeconds - 2)
            if ($toneAnalysis.Game440End -lt 10 -or
                $toneAnalysis.Game880End -lt 10) {
                throw "Executable roots did not survive exit/restart: $($toneAnalysis | ConvertTo-Json -Compress)"
            }
        }
    } elseif ($vivaldiScenario) {
        $toneAnalysis = [ordered]@{
            Mix440 = Measure-ToneMagnitude $recording.FullName 0 440
            Music440 = Measure-ToneMagnitude $recording.FullName 1 440
            Mix440End = Measure-ToneMagnitude `
                $recording.FullName 0 440 ($DurationSeconds - 6)
            Music440End = Measure-ToneMagnitude `
                $recording.FullName 1 440 ($DurationSeconds - 6)
        }
        if ($toneAnalysis.Mix440 -lt 10 -or
            $toneAnalysis.Music440 -lt 10 -or
            $toneAnalysis.Mix440End -lt 10 -or
            $toneAnalysis.Music440End -lt 10) {
            throw "Vivaldi Music track did not survive restart: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    } elseif ($Scenario -notin @('missing-stop', 'inactive', 'video-only')) {
        $toneAnalysis = [ordered]@{
            Mix440 = Measure-ToneMagnitude $recording.FullName 0 440
        }
        if ($multiTrackFormat) {
            $toneAnalysis['Game440'] =
                Measure-ToneMagnitude $recording.FullName 1 440
        }
        if ($toneAnalysis.Mix440 -lt 10 -or
            ($multiTrackFormat -and $toneAnalysis.Game440 -lt 10)) {
            throw "Game tone is missing from required output tracks: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    }

    if ($Scenario -eq 'process-tree' -and
        ($toneAnalysis.Game440 -lt 180 -or $toneAnalysis.Game440 -gt 330)) {
        throw "Process-tree capture was missing or duplicated: $($toneAnalysis | ConvertTo-Json -Compress)"
    }

    if ($Scenario -eq 'topology-change' -and $multiTrackFormat) {
        $toneAnalysis['LateMix1320'] =
            Measure-ToneMagnitude $recording.FullName 0 1320
        $toneAnalysis['LateGame1320'] =
            Measure-ToneMagnitude $recording.FullName 1 1320
        $toneAnalysis['LateMusic1320'] =
            Measure-ToneMagnitude $recording.FullName 2 1320
        $toneAnalysis['Game440AfterDelete'] =
            Measure-ToneMagnitude $recording.FullName 1 440 7
        $toneAnalysis['Music880AfterDelete'] =
            Measure-ToneMagnitude $recording.FullName 2 880 7
        if ($toneAnalysis.LateMix1320 -gt 5 -or
            $toneAnalysis.LateGame1320 -gt 5 -or
            $toneAnalysis.LateMusic1320 -gt 5 -or
            $toneAnalysis.Game440AfterDelete -lt 10 -or
            $toneAnalysis.Music880AfterDelete -gt 5) {
            throw "Frozen topology changed during source mutation: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    }

    if ($Scenario -in @('muted', 'monitor-only')) {
        $toneAnalysis['Mix880'] =
            Measure-ToneMagnitude $recording.FullName 0 880
        if ($multiTrackFormat) {
            $toneAnalysis['Game880'] =
                Measure-ToneMagnitude $recording.FullName 1 880
        }
        if ($toneAnalysis.Mix880 -gt 5 -or
            ($multiTrackFormat -and $toneAnalysis.Game880 -gt 5)) {
            throw "Excluded Music source leaked into output: $($toneAnalysis | ConvertTo-Json -Compress)"
        }
    }

	if ($ReplayBuffer) {
		foreach ($replayFile in $recordingFiles |
			Where-Object FullName -NE $recording.FullName) {
			$replayProbe = & ffprobe -v error -show_entries `
				'stream=index,codec_type,codec_name:stream_disposition=default:stream_tags=name,title:format=duration' `
				-of json $replayFile.FullName | ConvertFrom-Json
			$replayAudio = @($replayProbe.streams |
				Where-Object codec_type -eq 'audio')
			if ($replayAudio.Count -ne $expectedAudioStreams -or
			    @($replayAudio | Where-Object codec_name -ne 'aac').Count -ne 0) {
				throw "Replay file '$($replayFile.Name)' has an invalid audio topology"
			}
			$replayNames = @($replayAudio | ForEach-Object {
				if ($_.tags.name) { $_.tags.name } else { $_.tags.title }
			})
			if ($supportsAudioMetadata -and
			    ($replayNames -join ',') -ne $expectedTrackNames) {
				throw "Replay file '$($replayFile.Name)' has unexpected track names"
			}
			if ($supportsAudioMetadata -and
			    ($replayAudio[0].disposition.default -ne 1 -or
			    @($replayAudio | Select-Object -Skip 1 | Where-Object {
				$_.disposition.default -ne 0
			    }).Count -ne 0)) {
				throw "Replay file '$($replayFile.Name)' has an invalid default track"
			}
			$replayDuration = [double]::Parse(
				$replayProbe.format.duration,
				[Globalization.CultureInfo]::InvariantCulture)
			if ($replayDuration -lt 3) {
				throw "Replay file '$($replayFile.Name)' is too short"
			}
			$replayTones = $null
			if ($dualSourceScenario -and $multiTrackFormat) {
				$replayTones = [ordered]@{
					Mix440 = Measure-ToneMagnitude $replayFile.FullName 0 440
					Mix880 = Measure-ToneMagnitude $replayFile.FullName 0 880
					Game440 = Measure-ToneMagnitude $replayFile.FullName 1 440
					Game880 = Measure-ToneMagnitude $replayFile.FullName 1 880
					Music440 = Measure-ToneMagnitude $replayFile.FullName 2 440
					Music880 = Measure-ToneMagnitude $replayFile.FullName 2 880
				}
				if ($replayTones.Mix440 -lt 10 -or
				    $replayTones.Mix880 -lt 10 -or
				    $replayTones.Game440 -lt 10 -or
				    $replayTones.Game440 -lt ($replayTones.Game880 * 5) -or
				    $replayTones.Music880 -lt 10 -or
				    $replayTones.Music880 -lt ($replayTones.Music440 * 5)) {
					throw "Replay file '$($replayFile.Name)' has invalid tone isolation"
				}
			}
			$replayEvidence += [pscustomobject]@{
				File = $replayFile.FullName
				AudioStreams = $replayAudio.Count
				Duration = $replayDuration
				Tones = $replayTones
			}
		}
	}

    $meanVolumeDb = $null
    if ($Scenario -notin @('missing-stop', 'inactive', 'video-only')) {
        $volumeOutput = & ffmpeg -hide_banner -i $recording.FullName `
            -map '0:a:0' -af volumedetect -f null - 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            throw 'FFmpeg could not decode the isolated audio stream'
        }
        $meanVolume = [regex]::Match(
            $volumeOutput,
            'mean_volume:\s+(-?[0-9.]+) dB')
        if (-not $meanVolume.Success -or
            [double]$meanVolume.Groups[1].Value -lt -90) {
            throw 'The isolated audio stream is silent'
        }
        $meanVolumeDb = [double]$meanVolume.Groups[1].Value
    }

    $qualificationResult = $null
    $qualificationReport = $null
    if ($Qualification) {
        $qualificationResult = Test-QualificationRecording `
            $recording.FullName `
            $result.Content
        $qualificationReport = "$runRoot/qualification.json"
        $report = [ordered]@{
            Result = 'PASS'
            Scenario = $Scenario
            RecordingFormat = $RecordingFormat
            DurationSeconds = $DurationSeconds
            FPS = $Fps
            CanvasWidth = $CanvasWidth
            CanvasHeight = $CanvasHeight
            Recording = $recording.FullName
            RecordingBytes = $recording.Length
            Metrics = $qualificationResult
            ResourceSamples = $resourceSamples
        }
        [IO.File]::WriteAllText(
            $qualificationReport,
            ($report | ConvertTo-Json -Depth 10),
            [Text.UTF8Encoding]::new($false))
        if (-not $qualificationResult.Passed) {
            throw "Qualification failed; see $qualificationReport"
        }
    }

    [pscustomobject]@{
        Result = 'PASS'
        Scenario = $Scenario
        MatchPriority = $MatchPriority
        RecordingFormat = $RecordingFormat
        OutputMode = $OutputMode
		RecordingType = $RecordingType
		CustomFormat = if ($RecordingType -eq 'Custom') {
			$CustomFormat
		} else { $null }
		ReplayBuffer = [bool]$ReplayBuffer
		ReplayEvidence = if ($ReplayBuffer) {
			$replayEvidence | ConvertTo-Json -Depth 6 -Compress
		} else { $null }
		Streaming = [bool]$Streaming
		VodTrack = [bool]$VodTrack
		BilibiliLocal = [bool]$BilibiliLocal
		ParallelVodFailure = [bool]$ParallelVodFailure
		ParallelFailureDialog = $parallelFailureDialogTitle
		StreamVolumes = if ($Streaming -and -not $ParallelVodFailure) {
			$streamVolumes | ConvertTo-Json -Compress
		} else { $null }
		StreamTones = if ($VodTrack -and -not $BilibiliLocal -and
			-not $ParallelVodFailure) {
			$streamToneAnalysis | ConvertTo-Json -Compress
		} else { $null }
        AudioBitrateKbps = $AudioBitrate
        Canvas = "$CanvasWidth`x$CanvasHeight@$Fps"
        TargetMinimized = [bool]$TargetMinimized
        Runtime = $runRoot
        Log = $result.Log
        Recording = $recording.FullName
        RecordingBytes = $recording.Length
        AudioStreams = $audioStreams.Count
        MeanVolumeDb = $meanVolumeDb
        Initializations = $initializationCount
        MissingTargetWarnings = $missingCount
        ChildToneProcessId = $childToneProcessId
        QualificationReport = $qualificationReport
        RenderingLagPercent = $qualificationResult.RenderingLagPercent
        EncodingLagPercent = $qualificationResult.EncodingLagPercent
        MaxAVDriftMs = $qualificationResult.MaxAVDriftMs
        ResourceHeadroom = if ($qualificationResult) {
            $qualificationResult.ResourceHeadroom | ConvertTo-Json -Compress
        } else {
            $null
        }
        ToneAnalysis = if ($toneAnalysis) {
            $toneAnalysis | ConvertTo-Json -Compress
        } else {
            $null
        }
    } | Format-List
} finally {
    Stop-TestProcess $obsProcess
	Stop-TestProcess $streamServer
    Stop-TestProcess $toneProcess
    Stop-TestProcess $musicProcess
    Stop-TestProcess $privateProcess
    Stop-VivaldiFixture
    Get-Process -Name 'obspt-tone-game', 'obspt-tone-music', `
        'obspt-tone-private' `
        -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
}
