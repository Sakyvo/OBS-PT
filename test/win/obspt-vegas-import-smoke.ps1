param(
    [Parameter(Mandatory = $true)]
    [string]$Recording,
    [string]$VegasPath = 'K:/VEGAS Pro/Vegas 23/vegas230.exe',
    [string]$WorkRoot =
        'K:/Projects/dev/OBS-PT/.target/_tdd/vegas-import',
    [string[]]$ExpectedTrackNames = @('Mix', 'Game', 'Music')
)

$ErrorActionPreference = 'Stop'
$recordingPath = (Resolve-Path $Recording).Path
$vegas = (Resolve-Path $VegasPath).Path
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$work = [IO.Path]::GetFullPath($WorkRoot)
if (-not $work.StartsWith($repo + [IO.Path]::DirectorySeparatorChar)) {
    throw "WorkRoot must stay inside the repository: $work"
}
New-Item -ItemType Directory -Path $work -Force | Out-Null

$probe = & ffprobe -v error `
    -show_entries `
        'stream=index,codec_type:stream_tags=name:stream_disposition=default' `
    -of json $recordingPath | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) {
    throw 'ffprobe could not inspect the VEGAS acceptance recording'
}
$audio = @($probe.streams | Where-Object codec_type -eq 'audio')
if ($audio.Count -ne $ExpectedTrackNames.Count) {
    throw "Expected $($ExpectedTrackNames.Count) audio streams, found $($audio.Count)"
}
for ($i = 0; $i -lt $audio.Count; ++$i) {
    if ($audio[$i].tags.name -ne $ExpectedTrackNames[$i]) {
        throw "Audio stream $i is '$($audio[$i].tags.name)', expected '$($ExpectedTrackNames[$i])'"
    }
    $expectedDefault = if ($i -eq 0) { 1 } else { 0 }
    if ($audio[$i].disposition.default -ne $expectedDefault) {
        throw "Audio stream $i has an invalid default disposition"
    }
}

$reportPath = "$work/vegas-report.json"
$scriptPath = "$work/vegas-import-probe.cs"
[IO.File]::Delete($reportPath)

function ConvertTo-CSharpLiteral {
    param([string]$Value)

    return '@"' + $Value.Replace('"', '""') + '"'
}

$probeSource = @'
using System;
using System.IO;
using System.Text;
using ScriptPortal.Vegas;

public class EntryPoint
{
    private const string MediaPath = __MEDIA_PATH__;
    private const string ReportPath = __REPORT_PATH__;

    private static string Json(string value)
    {
        if (value == null)
            return "null";
        return "\"" + value.Replace("\\", "\\\\")
            .Replace("\"", "\\\"")
            .Replace("\r", "\\r")
            .Replace("\n", "\\n") + "\"";
    }

    public void FromVegas(Vegas vegas)
    {
        try {
            vegas.NewProject(false, false);
            vegas.ImportFile(MediaPath, false, false);
            vegas.WaitForIdle();

            Media media = vegas.Project.MediaPool.Find(MediaPath);
            StringBuilder sourceIndices = new StringBuilder();
            StringBuilder takeIndices = new StringBuilder();
            StringBuilder trackNames = new StringBuilder();
            StringBuilder takeNames = new StringBuilder();
            Track firstAudioTrack = null;
            int audioTrackCount = 0;
            foreach (Track track in vegas.Project.Tracks) {
                if (!track.IsAudio())
                    continue;
                if (audioTrackCount > 0) {
                    sourceIndices.Append(',');
                    takeIndices.Append(',');
                    trackNames.Append(',');
                    takeNames.Append(',');
                }
                if (firstAudioTrack == null)
                    firstAudioTrack = track;
                TrackEvent trackEvent = track.Events.Count > 0
                    ? track.Events[0] : null;
                Take take = trackEvent == null ? null : trackEvent.ActiveTake;
                sourceIndices.Append(trackEvent == null
                    ? -1 : trackEvent.SourceStreamIndex);
                takeIndices.Append(take == null ? -1 : take.StreamIndex);
                trackNames.Append(Json(track.Name));
                takeNames.Append(Json(take == null ? null : take.Name));
                ++audioTrackCount;
            }

            bool muteApplied = false;
            bool removeApplied = false;
            if (firstAudioTrack != null) {
                firstAudioTrack.Mute = true;
                muteApplied = firstAudioTrack.Mute;
                vegas.Project.Tracks.Remove(firstAudioTrack);
                int remainingAudioTracks = 0;
                foreach (Track track in vegas.Project.Tracks) {
                    if (track.IsAudio())
                        ++remainingAudioTracks;
                }
                removeApplied = remainingAudioTracks == audioTrackCount - 1;
            }

            StringBuilder report = new StringBuilder();
            report.Append("{\"mediaAudioStreams\":")
                .Append(media == null ? -1 : media.StreamCount(MediaType.Audio))
                .Append(",\"audioTrackCount\":").Append(audioTrackCount)
                .Append(",\"sourceStreamIndices\":[")
                .Append(sourceIndices).Append(']')
                .Append(",\"takeStreamIndices\":[")
                .Append(takeIndices).Append(']')
                .Append(",\"trackNames\":[")
                .Append(trackNames).Append(']')
                .Append(",\"takeNames\":[")
                .Append(takeNames).Append(']')
                .Append(",\"firstAudioMuteApplied\":")
                .Append(muteApplied ? "true" : "false")
                .Append(",\"firstAudioRemoveApplied\":")
                .Append(removeApplied ? "true" : "false")
                .Append('}');
            File.WriteAllText(ReportPath, report.ToString(), Encoding.UTF8);
        } catch (Exception error) {
            File.WriteAllText(ReportPath,
                "{\"error\":" + Json(error.ToString()) + "}",
                Encoding.UTF8);
        } finally {
            vegas.NewProject(false, false);
            vegas.Exit();
        }
    }
}
'@
$probeSource = $probeSource.Replace(
    '__MEDIA_PATH__', (ConvertTo-CSharpLiteral $recordingPath)).Replace(
    '__REPORT_PATH__', (ConvertTo-CSharpLiteral $reportPath))
[IO.File]::WriteAllText(
    $scriptPath, $probeSource, [Text.UTF8Encoding]::new($false))

if (-not ('OBSPTIsolatedDesktop' -as [type])) {
    Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class OBSPTIsolatedDesktop
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct StartupInfo
    {
        public int Size;
        public string Reserved;
        public string Desktop;
        public string Title;
        public int X;
        public int Y;
        public int XSize;
        public int YSize;
        public int XChars;
        public int YChars;
        public int FillAttribute;
        public int Flags;
        public short ShowWindow;
        public short ReservedSize;
        public IntPtr ReservedPointer;
        public IntPtr StandardInput;
        public IntPtr StandardOutput;
        public IntPtr StandardError;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ProcessInformation
    {
        public IntPtr Process;
        public IntPtr Thread;
        public int ProcessId;
        public int ThreadId;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateDesktop(string name, IntPtr device,
        IntPtr mode, int flags, uint access, IntPtr attributes);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool CloseDesktop(IntPtr desktop);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateProcess(string application,
        StringBuilder commandLine, IntPtr processAttributes,
        IntPtr threadAttributes, bool inheritHandles, uint creationFlags,
        IntPtr environment, string currentDirectory,
        ref StartupInfo startupInfo,
        out ProcessInformation processInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint WaitForSingleObject(IntPtr handle,
        uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool TerminateProcess(IntPtr process, uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetExitCodeProcess(IntPtr process,
        out uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    public static string Run(string executable, string arguments,
        string workingDirectory, int timeoutMilliseconds)
    {
        string name = "OBSPT-QA-" + Guid.NewGuid().ToString("N");
        IntPtr desktop = CreateDesktop(name, IntPtr.Zero, IntPtr.Zero, 0,
            0x1C3, IntPtr.Zero);
        if (desktop == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(),
                "CreateDesktop");

        ProcessInformation process = new ProcessInformation();
        try {
            StartupInfo startup = new StartupInfo();
            startup.Size = Marshal.SizeOf(typeof(StartupInfo));
            startup.Desktop = "winsta0\\" + name;
            startup.Flags = 1;
            startup.ShowWindow = 7;
            StringBuilder command = new StringBuilder(
                "\"" + executable + "\" " + arguments);
            if (!CreateProcess(executable, command, IntPtr.Zero, IntPtr.Zero,
                    false, 0x600, IntPtr.Zero, workingDirectory,
                    ref startup, out process))
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "CreateProcess");

            bool timedOut = WaitForSingleObject(process.Process,
                (uint)timeoutMilliseconds) == 258;
            if (timedOut) {
                TerminateProcess(process.Process, 124);
                WaitForSingleObject(process.Process, 5000);
            }
            uint exitCode;
            GetExitCodeProcess(process.Process, out exitCode);
            return process.ProcessId + ":" + exitCode + ":" + timedOut;
        } finally {
            if (process.Thread != IntPtr.Zero)
                CloseHandle(process.Thread);
            if (process.Process != IntPtr.Zero)
                CloseHandle(process.Process);
            CloseDesktop(desktop);
        }
    }
}
'@
}

$processName = [IO.Path]::GetFileNameWithoutExtension($vegas)
if (Get-Process -Name $processName -ErrorAction SilentlyContinue) {
    throw "Close $processName before running the VEGAS import smoke test"
}
$processResult = [OBSPTIsolatedDesktop]::Run(
    $vegas, "-SCRIPT:`"$scriptPath`"", (Split-Path $vegas), 120000)
$processParts = $processResult.Split(':')
if ($processParts[2] -eq 'True') {
    throw 'VEGAS import smoke test timed out'
}
if ([int]$processParts[1] -ne 0) {
    throw "VEGAS exited with code $($processParts[1])"
}
if (-not (Test-Path $reportPath)) {
    throw 'VEGAS did not write an import report'
}
$vegasReport = Get-Content -Raw $reportPath | ConvertFrom-Json
if ($vegasReport.error) {
    throw "VEGAS import failed: $($vegasReport.error)"
}
if ($vegasReport.mediaAudioStreams -ne $audio.Count -or
    $vegasReport.audioTrackCount -ne $audio.Count) {
    throw 'VEGAS did not import every audio stream'
}

$sourceIndices = @($vegasReport.sourceStreamIndices | ForEach-Object {
    [int]$_
})
$takeIndices = @($vegasReport.takeStreamIndices | ForEach-Object {
    [int]$_
})
for ($i = 0; $i -lt $audio.Count; ++$i) {
    if ($sourceIndices[$i] -ne $audio[$i].index -or
        $takeIndices[$i] -ne $i) {
        throw "VEGAS changed the order of audio stream $i"
    }
}
if (-not $vegasReport.firstAudioMuteApplied -or
    -not $vegasReport.firstAudioRemoveApplied) {
    throw 'VEGAS could not mute and remove the first audio track'
}

$exposedNames = @($vegasReport.trackNames)
$containerNamesExposed = $exposedNames.Count -eq $ExpectedTrackNames.Count
for ($i = 0; $containerNamesExposed -and
     $i -lt $ExpectedTrackNames.Count; ++$i) {
    $containerNamesExposed =
        $exposedNames[$i] -eq $ExpectedTrackNames[$i]
}

[pscustomobject]@{
    Result = 'PASS'
    Recording = $recordingPath
    FileTrackNames = @($audio | ForEach-Object { $_.tags.name })
    DefaultTrack = $audio[0].tags.name
    Vegas = [pscustomobject]@{
        Path = $vegas
        Version = (Get-Item $vegas).VersionInfo.FileVersion
        ProcessId = [int]$processParts[0]
        ImportedAudioTracks = [int]$vegasReport.audioTrackCount
        SourceStreamIndices = $sourceIndices
        TakeStreamIndices = $takeIndices
        FirstTrackMuted = [bool]$vegasReport.firstAudioMuteApplied
        FirstTrackRemoved = [bool]$vegasReport.firstAudioRemoveApplied
        DisplaysContainerTrackNames = $containerNamesExposed
    }
    WorkRoot = $work
} | ConvertTo-Json -Depth 6
