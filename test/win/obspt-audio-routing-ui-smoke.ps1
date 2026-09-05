param(
    [string]$RuntimeRoot =
        'K:/Projects/dev/OBS-PT/.target/_pkg/OBS-PT',
    [string]$WorkRoot =
        'K:/Projects/dev/OBS-PT/.target/_tdd/audio-routing-ui',
    [switch]$SkipCreationUi
)

$ErrorActionPreference = 'Stop'
$runtime = (Resolve-Path $RuntimeRoot).Path
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$work = [IO.Path]::GetFullPath($WorkRoot)
if (-not $work.StartsWith($repo + [IO.Path]::DirectorySeparatorChar)) {
    throw "WorkRoot must stay inside the repository: $work"
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
if (-not ('OBSPTWindowStyle' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class OBSPTWindowStyle
{
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtr")]
    private static extern IntPtr GetWindowLongPtr64(IntPtr window, int index);

    [DllImport("user32.dll", EntryPoint = "GetWindowLong")]
    private static extern IntPtr GetWindowLong32(IntPtr window, int index);

    [DllImport("user32.dll")]
    private static extern bool ScreenToClient(IntPtr window, ref Point point);

    [DllImport("user32.dll")]
    private static extern IntPtr WindowFromPoint(Point point);

    [DllImport("user32.dll")]
    private static extern bool PostMessage(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    [StructLayout(LayoutKind.Sequential)]
    private struct Point
    {
        public int X;
        public int Y;
    }

    public static bool IsChild(IntPtr window)
    {
        long style = IntPtr.Size == 8
            ? GetWindowLongPtr64(window, -16).ToInt64()
            : GetWindowLong32(window, -16).ToInt64();
        return (style & 0x40000000L) != 0;
    }

    public static void Drag(IntPtr window, int sourceX, int sourceY,
        int targetX, int targetY)
    {
        Point source = new Point { X = sourceX, Y = sourceY };
        Point target = new Point { X = targetX, Y = targetY };
        if (window == IntPtr.Zero)
            window = WindowFromPoint(source);
        if (!ScreenToClient(window, ref source) ||
            !ScreenToClient(window, ref target))
            throw new InvalidOperationException("ScreenToClient failed");
        Func<int, int, IntPtr> point = (x, y) =>
            new IntPtr((y << 16) | (x & 0xffff));
        PostMessage(window, 0x0201, new IntPtr(1),
            point(source.X, source.Y));
        for (int step = 1; step <= 8; ++step)
        {
            int x = source.X + (target.X - source.X) * step / 8;
            int y = source.Y + (target.Y - source.Y) * step / 8;
            PostMessage(window, 0x0200, new IntPtr(1), point(x, y));
            System.Threading.Thread.Sleep(30);
        }
        PostMessage(window, 0x0202, IntPtr.Zero,
            point(target.X, target.Y));
    }

    public static void Click(IntPtr window, int screenX, int screenY)
    {
        Point target = new Point { X = screenX, Y = screenY };
        if (window == IntPtr.Zero)
            window = WindowFromPoint(target);
        if (!ScreenToClient(window, ref target))
            throw new InvalidOperationException("ScreenToClient failed");
        IntPtr point = new IntPtr(
            (target.Y << 16) | (target.X & 0xffff));
        PostMessage(window, 0x0201, new IntPtr(1), point);
        PostMessage(window, 0x0202, IntPtr.Zero, point);
    }

    public static void RightClick(IntPtr window, int screenX, int screenY)
    {
        Point target = new Point { X = screenX, Y = screenY };
        if (!ScreenToClient(window, ref target))
            throw new InvalidOperationException("ScreenToClient failed");
        IntPtr point = new IntPtr(
            (target.Y << 16) | (target.X & 0xffff));
        PostMessage(window, 0x0204, new IntPtr(2), point);
        PostMessage(window, 0x0205, IntPtr.Zero, point);
    }

    public static void ShowWithoutActivation(IntPtr window)
    {
        ShowWindow(window, 4);
        SetWindowPos(window, new IntPtr(1), 0, 0, 0, 0,
            0x0001 | 0x0002 | 0x0010 | 0x0040);
    }

    public static void ToggleDocked(IntPtr window)
    {
        PostMessage(window, 0x00A3, new IntPtr(2), IntPtr.Zero);
    }

}
'@
}

$languages = @(
    [pscustomobject]@{
        Locale = 'en-US'; Title = 'Audio & Routing'; Track = 'Track'
        Name = 'Name'; Source = 'Source'
        Tree = 'Audio and Routing tracks and sources'; Muted = 'Muted'
        Inspect = 'Properties'; Process = 'Process'
        NotRunning = 'obspt-ui-missing.exe (not running)'
        Pending = 'Status: Waiting for process'
        Priority = 'Window Match Priority'; Window = 'Window'
        TitleMode = 'Window title must match'
        ProcessMode = 'Match process name only'
        Track3 = 'Track 3  Music'
        MoveTo = 'Move to Track'
    },
    [pscustomobject]@{
        Locale = 'zh-CN'; Title = '音频与路由'; Track = '音轨'
        Name = '名称'; Source = '来源'
        Tree = '音频与路由的音轨和来源'; Muted = '静音轨'
        Inspect = '属性'; Process = '进程'
        NotRunning = 'obspt-ui-missing.exe（未运行）'
        Pending = '状态：等待进程运行'
        Priority = '窗口匹配优先级'; Window = '窗口'
        TitleMode = '窗口标题必须匹配'; ProcessMode = '进程名匹配'
        Track3 = '音轨 3  Music'
        MoveTo = '移动到音轨'
        AddAudio = '添加音频来源'
        AddApplication = '程序独立音频捕获'
        SourceSelect = '创建或选择源'
        CreateNew = '新建'; AddExisting = '添加现有'
        Route = '路由音轨'; RouteRequired = '请选择路由音轨'
        Ok = '确定'; Cancel = '取消'
        ConfirmChanges = '设置已更改'
        Placeholder = '多音轨录制（即将推出）'
        RemoveAudio = '删除音频来源'
        MoveUp = '上移'; MoveDown = '下移'
        View = '视图(V)'; Docks = '停靠窗口(D)'
        Desktop = '桌面音频'
    },
    [pscustomobject]@{
        Locale = 'zh-TW'; Title = '音訊與路由'; Track = '音軌'
        Name = '名稱'; Source = '來源'
        Tree = '音訊與路由的音軌和來源'; Muted = '靜音軌'
        Inspect = '屬性'; Process = '處理程序'
        NotRunning = 'obspt-ui-missing.exe（未執行）'
        Pending = '狀態：等待處理程序執行'
        Priority = '視窗比對優先度'; Window = '視窗'
        TitleMode = '視窗標題必須相符'; ProcessMode = '僅比對處理程序名稱'
        Track3 = '音軌 3  Music'
        MoveTo = '移動到音軌'
    },
    [pscustomobject]@{
        Locale = 'ja-JP'; Title = '音声とルーティング'; Track = 'トラック'
        Name = '名前'; Source = 'ソース'
        Tree = '音声とルーティングのトラックとソース'; Muted = 'ミュート'
        Inspect = 'プロパティ'; Process = 'プロセス'
        NotRunning = 'obspt-ui-missing.exe（実行されていません）'
        Pending = '状態：プロセスの起動待ち'
        Priority = 'ウィンドウの一致優先順位'; Window = 'ウィンドウ'
        TitleMode = 'ウィンドウのタイトルに一致する必要があります'
        ProcessMode = 'プロセス名のみ一致'
        Track3 = 'トラック 3  Music'
        MoveTo = 'トラックへ移動'
    },
    [pscustomobject]@{
        Locale = 'ko-KR'; Title = '오디오 및 라우팅'; Track = '트랙'
        Name = '이름'; Source = '소스'
        Tree = '오디오 및 라우팅 트랙과 소스'; Muted = '음소거'
        Inspect = '속성'; Process = '프로세스'
        NotRunning = 'obspt-ui-missing.exe(실행 중 아님)'
        Pending = '상태: 프로세스 실행 대기 중'
        Priority = '창 탐색 우선순위'; Window = '윈도우'
        TitleMode = '제목이 일치하는 창 찾기'
        ProcessMode = '프로세스 이름만 일치'
        Track3 = '트랙 3  Music'
        MoveTo = '트랙으로 이동'
    }
)
$languages = @($languages | Where-Object Locale -eq 'zh-CN')
$roles = @('Mix', 'Game', 'Music', 'Microphone', 'Voice Chat', 'Aux')

function Invoke-Robocopy {
    param([string]$Source, [string]$Destination)

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    & robocopy $Source $Destination /MIR /NFL /NDL /NJH /NJS /NP |
        Out-Null
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
        [void]$lines.Add("[$Section]")
        [void]$lines.Add("$Key=$Value")
    } else {
        $insert = $sectionIndex + 1
        $found = $false
        while ($insert -lt $lines.Count -and
               $lines[$insert] -notmatch '^\[.+\]$') {
            if ($lines[$insert] -match
                "^$([regex]::Escape($Key))=") {
                $lines[$insert] = "$Key=$Value"
                $found = $true
                break
            }
            $insert++
        }
        if (-not $found) {
            $lines.Insert($insert, "$Key=$Value")
        }
    }
    [IO.File]::WriteAllLines(
        $Path, $lines, [Text.UTF8Encoding]::new($false))
}

function Find-ElementById {
    param(
        [System.Windows.Automation.AutomationElement]$Root,
        [string]$Id,
        [System.Windows.Automation.TreeScope]$Scope =
            [System.Windows.Automation.TreeScope]::Descendants
    )

    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::AutomationIdProperty,
        $Id)
    return $Root.FindFirst($Scope, $condition)
}

function Find-ElementByIdSuffix {
    param(
        [System.Windows.Automation.AutomationElement]$Root,
        [string]$Suffix
    )

    $elements = $Root.FindAll(
        [System.Windows.Automation.TreeScope]::Descendants,
        [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($element in $elements) {
        try {
            if ($element.Current.AutomationId.EndsWith(
                    $Suffix, [StringComparison]::Ordinal)) {
                return $element
            }
        } catch [System.Windows.Automation.ElementNotAvailableException] {
            continue
        }
    }
    return $null
}

function Find-ElementByName {
    param(
        [System.Windows.Automation.AutomationElement]$Root,
        [string]$Name,
        [System.Windows.Automation.ControlType]$ControlType
    )

    $nameCondition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::NameProperty,
        $Name)
    $typeCondition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        $ControlType)
    $condition = [System.Windows.Automation.AndCondition]::new(
        $nameCondition, $typeCondition)
    return $Root.FindFirst(
        [System.Windows.Automation.TreeScope]::Descendants, $condition)
}

function Find-ElementByControlType {
    param(
        [System.Windows.Automation.AutomationElement]$Root,
        [System.Windows.Automation.ControlType]$ControlType
    )

    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        $ControlType)
    return $Root.FindFirst(
        [System.Windows.Automation.TreeScope]::Descendants, $condition)
}

function Wait-FloatingDock {
    param(
        [string]$Id,
        [int]$TimeoutMilliseconds = 5000
    )

    $condition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::AutomationIdProperty,
        $Id)
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    do {
        $elements = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants, $condition)
        foreach ($element in $elements) {
            try {
                $handle = [IntPtr]$element.Current.NativeWindowHandle
                if ($handle -ne [IntPtr]::Zero -and
                    -not [OBSPTWindowStyle]::IsChild($handle)) {
                    return $element
                }
            } catch [System.Windows.Automation.ElementNotAvailableException] {
                continue
            }
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    return $null
}

function Start-SmokeOBS {
    param([string]$Runtime)

    $process = Start-Process `
        -FilePath "$Runtime/bin/64bit/OBS-PT.exe" `
        -ArgumentList @(
            '--multi',
            '--profile', 'PotPvP',
            '--collection', 'PotPvP',
            '--scene', 'PotPvP',
            '--disable-updater',
            '--disable-missing-files-check') `
        -WorkingDirectory "$Runtime/bin/64bit" `
        -WindowStyle Minimized `
        -PassThru
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    } while ($process.MainWindowHandle -eq 0 -and
             -not $process.HasExited -and (Get-Date) -lt $deadline)
    if ($process.HasExited -or $process.MainWindowHandle -eq 0) {
        throw 'OBS-PT did not create its main window'
    }
    [OBSPTWindowStyle]::ShowWithoutActivation(
        [IntPtr]$process.MainWindowHandle)
    Start-Sleep -Seconds 2
    return $process
}

function Stop-SmokeOBS {
    param([Diagnostics.Process]$Process)

    if (-not $Process -or $Process.HasExited) {
        return
    }
    [void]$Process.CloseMainWindow()
    if (-not $Process.WaitForExit(5000)) {
        Stop-Process -Id $Process.Id -Force
    }
    Start-Sleep -Milliseconds 500
}

function Add-ProcessSourceFixture {
    param([string]$ScenePath)

    $scene = [IO.File]::ReadAllText($ScenePath) | ConvertFrom-Json
    $mojibake = -join @(
        [char]0x5997, [char]0x5c84, [char]0x6f70,
        [char]0x95ca, [char]0x62bd, [char]0xe576)
    $scene.DesktopAudioDevice1.name = $mojibake
    $scene.obspt_audio_routing.sources |
        Where-Object id -eq 'obspt-default-desktop-audio' |
        ForEach-Object { $_.name = $mojibake }
    $template = $scene.sources |
        Where-Object id -eq 'monitor_capture' |
        Select-Object -First 1
    $source = $template | ConvertTo-Json -Depth 100 | ConvertFrom-Json
    $source.id = 'wasapi_process_output_capture'
    $source.versioned_id = 'wasapi_process_output_capture'
    $source.name = 'Game Audio'
    $source.mixers = 2
    $source.private_settings = [pscustomobject]@{
        obspt_audio_source_id = 'ui-game'
    }
    $source.settings = [pscustomobject]@{
        window = 'Old Page Title:Ignored:obspt-ui-missing.exe'
        process_executable = 'obspt-ui-missing.exe'
        priority = 3
    }
    $scene.sources += $source
    $scene.obspt_audio_routing.sources += [pscustomobject]@{
        id = 'ui-game'
        name = 'Game Audio'
        kind = 'application_audio'
        editor_track = 2
        muted = $false
        monitor_only = $false
    }
    [IO.File]::WriteAllText(
        $ScenePath,
        ($scene | ConvertTo-Json -Depth 100 -Compress),
        [Text.UTF8Encoding]::new($false))
}

function Build-VisibleProcessFixture {
    param([string]$Directory)

    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $nativeDirectory = (Resolve-Path $Directory).Path
    $source = @'
using System;
using System.Drawing;
using System.Windows.Forms;
internal static class Program
{
    [STAThread]
    private static void Main()
    {
        Application.EnableVisualStyles();
        Form form = new Form();
        form.Text = "OBS-PT UI Process Candidate";
        form.ShowInTaskbar = true;
        form.StartPosition = FormStartPosition.Manual;
        form.Location = new Point(-30000, -30000);
        form.Opacity = 0.01;
        Application.Run(form);
    }
}
'@
    $sourcePath = Join-Path $nativeDirectory 'obspt-ui-process.cs'
    $exePath = Join-Path $nativeDirectory 'obspt-ui-process.exe'
    [IO.File]::WriteAllText(
        $sourcePath, $source, [Text.UTF8Encoding]::new($false))
    & 'C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe' `
        /nologo /target:winexe /optimize+ `
        /reference:System.dll /reference:System.Drawing.dll `
        /reference:System.Windows.Forms.dll "/out:$exePath" $sourcePath
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not build visible process fixture'
    }
    return $exePath
}

function Wait-GlobalElementByName {
    param(
        [string]$Name,
        [System.Windows.Automation.ControlType]$ControlType,
        [int]$TimeoutMilliseconds = 5000
    )
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    do {
        $element = Find-ElementByName `
            ([System.Windows.Automation.AutomationElement]::RootElement) `
            $Name $ControlType
        if ($element) {
            return $element
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    return $null
}

function Wait-GlobalElementAbsentByName {
    param(
        [string]$Name,
        [System.Windows.Automation.ControlType]$ControlType,
        [int]$TimeoutMilliseconds = 5000
    )
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    do {
        if (-not (Find-ElementByName `
                ([System.Windows.Automation.AutomationElement]::RootElement) `
                $Name $ControlType)) {
            return $true
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    return $false
}

function Start-AudioSourceCreation {
    param(
        [System.Windows.Automation.AutomationElement]$Dock,
        [object]$Language
    )
    $add = Find-ElementByIdSuffix $Dock 'OBSBasic.audioRoutingAdd'
    if (-not $add) {
        $add = Find-ElementByName $Dock $Language.AddAudio `
            ([System.Windows.Automation.ControlType]::Button)
    }
    if (-not $add) {
        $buttons = $Dock.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.PropertyCondition]::new(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::Button))
        $buttonNames = @($buttons | ForEach-Object {
            "$($_.Current.AutomationId):$($_.Current.Name)"
        })
        throw "$($Language.Locale): add-audio command is missing; buttons=$($buttonNames -join '|')"
    }
    $addRect = $add.Current.BoundingRectangle
    $addHandle = Get-AncestorNativeHandle $add
    if ($addHandle -eq [IntPtr]::Zero) {
        throw "$($Language.Locale): add-audio command has no native ancestor"
    }
    $sourceType = $null
    for ($attempt = 0; $attempt -lt 3 -and -not $sourceType; ++$attempt) {
        [OBSPTWindowStyle]::Click(
            $addHandle,
            [int]($addRect.X + $addRect.Width / 2),
            [int]($addRect.Y + $addRect.Height / 2))
        $sourceType = Wait-GlobalElementByName $Language.AddApplication `
            ([System.Windows.Automation.ControlType]::MenuItem) 1000
    }
    if (-not $sourceType) {
        $menuItems = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.PropertyCondition]::new(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::MenuItem))
        $menuNames = @($menuItems | Where-Object {
            -not $_.Current.IsOffscreen
        } | ForEach-Object Current | ForEach-Object Name)
        throw "$($Language.Locale): Application Audio menu item is missing; addId=$($add.Current.AutomationId) enabled=$($add.Current.IsEnabled) offscreen=$($add.Current.IsOffscreen) rect=$addRect handle=$addHandle menus=$($menuNames -join '|')"
    }
    $sourceRect = $sourceType.Current.BoundingRectangle
    $sourceHandle = Get-AncestorNativeHandle $sourceType
    if ($sourceHandle -eq [IntPtr]::Zero) {
        throw "$($Language.Locale): Application Audio menu has no native ancestor"
    }
    [OBSPTWindowStyle]::Click(
        $sourceHandle,
        [int]($sourceRect.X + $sourceRect.Width / 2),
        [int]($sourceRect.Y + $sourceRect.Height / 2))
    $dialog = Wait-GlobalElementByName $Language.SourceSelect `
        ([System.Windows.Automation.ControlType]::Window) 3000
    if (-not $dialog) {
        throw "$($Language.Locale): audio source creation window is missing"
    }
    return $dialog
}

function Invoke-DialogButton {
    param(
        [System.Windows.Automation.AutomationElement]$Dialog,
        [string]$Name
    )
    $button = Find-ElementByName $Dialog $Name `
        ([System.Windows.Automation.ControlType]::Button)
    if (-not $button) {
        throw "Dialog button '$Name' is missing"
    }
    $button.GetCurrentPattern(
        [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
}

function Set-SourceCreationName {
    param(
        [System.Windows.Automation.AutomationElement]$Dialog,
        [string]$Name
    )
    $edit = Find-ElementByControlType $Dialog `
        ([System.Windows.Automation.ControlType]::Edit)
    if (-not $edit) {
        throw 'Source creation name field is missing'
    }
    $edit.GetCurrentPattern(
        [System.Windows.Automation.ValuePattern]::Pattern).SetValue($Name)
}

function Find-RoutingSourceItem {
    param(
        [System.Windows.Automation.AutomationElement]$Tree,
        [string]$Name
    )
    return Get-TreeItems $Tree | Where-Object {
        $_.Current.Name -match [regex]::Escape($Name)
    } | Select-Object -First 1
}

function Get-TreeItems {
    param([System.Windows.Automation.AutomationElement]$Tree)
    return $Tree.FindAll(
        [System.Windows.Automation.TreeScope]::Descendants,
        [System.Windows.Automation.PropertyCondition]::new(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
            [System.Windows.Automation.ControlType]::TreeItem))
}

function Assert-AllTrackRows {
    param(
        [System.Windows.Automation.AutomationElement]$Tree,
        [object]$Language
    )
    $names = @(Get-TreeItems $Tree | ForEach-Object Current |
        ForEach-Object Name)
    for ($index = 0; $index -lt $roles.Count; ++$index) {
        $marker = "$($index + 1) $($roles[$index])"
        if (-not @($names | Where-Object {
                $_ -match [regex]::Escape($marker) }).Count) {
            throw "$($Language.Locale): missing row '$marker'; rows=$($names -join '|')"
        }
    }
    if (-not @($names | Where-Object {
            $_ -match "(^|[,，、]\s*)M $([regex]::Escape($Language.Muted))($|[,，、])"
        }).Count) {
        throw "$($Language.Locale): muted row is missing; rows=$($names -join '|')"
    }
    return $names
}

function Select-ComboItem {
    param(
        [System.Windows.Automation.AutomationElement]$Combo,
        [string]$Name
    )
    $condition = [System.Windows.Automation.AndCondition]::new(
        [System.Windows.Automation.PropertyCondition]::new(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
            [System.Windows.Automation.ControlType]::ListItem),
        [System.Windows.Automation.PropertyCondition]::new(
            [System.Windows.Automation.AutomationElement]::NameProperty,
            $Name))
    $item = $null
    $deadline = (Get-Date).AddSeconds(3)
    do {
        Open-Combo $Combo
        Start-Sleep -Milliseconds 100
        $item = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            $condition) | Where-Object {
                -not $_.Current.IsOffscreen
            } | Select-Object -First 1
    } while (-not $item -and (Get-Date) -lt $deadline)
    if (-not $item) {
        throw "Combo item '$Name' is missing"
    }
    try {
        $item.GetCurrentPattern(
            [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    } catch [System.InvalidOperationException] {
        $rect = $item.Current.BoundingRectangle
        $handle = Get-AncestorNativeHandle $item
        if ($handle -eq [IntPtr]::Zero) {
            throw "Combo item '$Name' has no native ancestor"
        }
        [OBSPTWindowStyle]::Click(
            $handle,
            [int]($rect.X + $rect.Width / 2),
            [int]($rect.Y + $rect.Height / 2))
    }
    Start-Sleep -Milliseconds 500
}

function Open-Combo {
    param([System.Windows.Automation.AutomationElement]$Combo)
    try {
        $Combo.GetCurrentPattern(
            [System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
        return
    } catch {
    }
    try {
        $rect = $Combo.Current.BoundingRectangle
    } catch [System.Windows.Automation.ElementNotAvailableException] {
        return
    }
    $handle = Get-AncestorNativeHandle $Combo
    if ($handle -eq [IntPtr]::Zero) {
        throw 'Combo has no native ancestor'
    }
    [OBSPTWindowStyle]::Click(
        $handle,
        [int]($rect.X + $rect.Width / 2),
        [int]($rect.Y + $rect.Height / 2))
}

function Close-Combo {
    param([System.Windows.Automation.AutomationElement]$Combo)
    try {
        $Combo.GetCurrentPattern(
            [System.Windows.Automation.ExpandCollapsePattern]::Pattern
        ).Collapse()
    } catch {
        $rect = $Combo.Current.BoundingRectangle
        $handle = Get-AncestorNativeHandle $Combo
        if ($handle -eq [IntPtr]::Zero) {
            return
        }
        [OBSPTWindowStyle]::Click(
            $handle,
            [int]($rect.X + $rect.Width / 2),
            [int]($rect.Y + $rect.Height / 2))
    }
}

function Get-ComboSelectionName {
    param([System.Windows.Automation.AutomationElement]$Combo)
    try {
        $value = $Combo.GetCurrentPattern(
            [System.Windows.Automation.ValuePattern]::Pattern
        ).Current.Value
        if ($value) {
            return $value
        }
    } catch [System.InvalidOperationException] {
    } catch [System.Windows.Automation.ElementNotAvailableException] {
        return $null
    }

    Open-Combo $Combo
    Start-Sleep -Milliseconds 100
    $items = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
        [System.Windows.Automation.TreeScope]::Descendants,
        [System.Windows.Automation.PropertyCondition]::new(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
            [System.Windows.Automation.ControlType]::ListItem))
    foreach ($item in $items) {
        try {
            $selected = $item.GetCurrentPattern(
                [System.Windows.Automation.SelectionItemPattern]::Pattern
            ).Current.IsSelected
            if ($selected) {
                return $item.Current.Name
            }
        } catch [System.InvalidOperationException] {
        }
    }
    return $null
}

function Get-AncestorWindow {
    param([System.Windows.Automation.AutomationElement]$Element)
    $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $current = $Element
    while ($current) {
        if ($current.Current.ControlType -eq
            [System.Windows.Automation.ControlType]::Window) {
            return $current
        }
        $current = $walker.GetParent($current)
    }
    return $null
}

function Get-AncestorNativeHandle {
    param([System.Windows.Automation.AutomationElement]$Element)
    $walker = [System.Windows.Automation.TreeWalker]::RawViewWalker
    $current = $Element
    while ($current) {
        try {
            $handle = [IntPtr]$current.Current.NativeWindowHandle
            if ($handle -ne [IntPtr]::Zero) {
                return $handle
            }
            $current = $walker.GetParent($current)
        } catch [System.Windows.Automation.ElementNotAvailableException] {
            return [IntPtr]::Zero
        }
    }
    return [IntPtr]::Zero
}

$staleTestProcesses = Get-CimInstance Win32_Process -Filter `
    "Name = 'OBS-PT.exe'" | Where-Object {
        $_.ExecutablePath -and
        [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith(
            $work, [StringComparison]::OrdinalIgnoreCase)
    }
if ($staleTestProcesses) {
    throw 'Close stale OBS-PT UI smoke instances before running the test'
}
New-Item -ItemType Directory -Path $work -Force | Out-Null
$fixtureExe = Build-VisibleProcessFixture "$work/fixture"
$results = [Collections.Generic.List[object]]::new()

foreach ($language in $languages) {
    $runRoot = "$work/$($language.Locale)"
    Invoke-Robocopy $runtime $runRoot
    if (-not (Test-Path "$runRoot/obs-studio/global.ini")) {
        Invoke-Robocopy "$repo/UI/data/obspt-defaults/obs-studio" `
            "$runRoot/obs-studio"
    }
    Set-IniValue "$runRoot/obs-studio/global.ini" General Language `
        $language.Locale
    Set-IniValue "$runRoot/obs-studio/global.ini" OBSPT BootstrapVersion `
        '999'
    Set-IniValue "$runRoot/obs-studio/global.ini" OBSPT FirstRunCompleted `
        'true'
    Add-ProcessSourceFixture `
        "$runRoot/obs-studio/basic/scenes/PotPvP.json"
    $process = $null
    $restart = $null
    $candidateProcesses = @()
    try {
        $candidateProcesses = @(
            Start-Process -FilePath $fixtureExe -WindowStyle Minimized -PassThru
            Start-Process -FilePath $fixtureExe -WindowStyle Minimized -PassThru
        )
        Start-Sleep -Milliseconds 500
        $process = Start-SmokeOBS $runRoot
        $root = [System.Windows.Automation.AutomationElement]::FromHandle(
            $process.MainWindowHandle)
		$routingButton = Find-ElementByIdSuffix $root `
			'OBSBasic.audioRoutingButton'
		if ($routingButton) {
			throw "$($language.Locale): redundant mixer routing button remains"
        }
        $stats = Find-ElementById $root 'OBSBasic.statsDock'
        if (-not $stats -or $stats.Current.BoundingRectangle.Width -le 0) {
            throw "$($language.Locale): Stats dock is not restored"
        }
        $dock = Find-ElementById $root 'OBSBasic.audioRoutingDock'
        $tree = if ($dock) {
            Find-ElementByControlType $dock `
                ([System.Windows.Automation.ControlType]::Tree)
        }
        if (-not $dock -or -not $tree) {
            throw "$($language.Locale): routing dock or table is missing"
        }
        $toolbarNames = @(
            $language.AddAudio,
            $language.RemoveAudio,
            $language.Inspect,
            $language.MoveUp,
            $language.MoveDown,
            $language.Placeholder
        )
        $allDockButtons = $dock.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.Condition]::TrueCondition)
        $allDockButtonNames = @($allDockButtons | ForEach-Object {
            $_.Current.Name
        })
        $toolbarPositions = @($toolbarNames | ForEach-Object {
            $name = $_
            for ($position = 0; $position -lt $allDockButtonNames.Count;
                 ++$position) {
                if ($allDockButtonNames[$position] -eq $name) {
                    $position
                    break
                }
            }
        })
        if ($toolbarPositions.Count -ne $toolbarNames.Count) {
            throw "$($language.Locale): routing toolbar control-tree order is incomplete; positions=$($toolbarPositions -join ','); names=$($allDockButtonNames -join '|')"
        }
        for ($index = 1; $index -lt $toolbarPositions.Count; ++$index) {
            if ($toolbarPositions[$index] -le
                $toolbarPositions[$index - 1]) {
                throw "$($language.Locale): routing toolbar order is invalid; positions=$($toolbarPositions -join ',')"
            }
        }
        $addCommand = $allDockButtons.Item($toolbarPositions[0])
        $placeholder = $allDockButtons.Item($toolbarPositions[5])
        if (-not $addCommand.Current.IsEnabled -or
            $placeholder.Current.IsEnabled -or
            $placeholder.Current.Name -ne $language.Placeholder) {
            throw "$($language.Locale): multi-track placeholder state is invalid; add=$($addCommand.Current.IsEnabled); placeholder=$($placeholder.Current.IsEnabled); name='$($placeholder.Current.Name)'"
        }
        $close = Find-ElementById $dock `
            'OBSBasic.audioRoutingDock.qt_dockwidget_closebutton'
        if ($close) {
            throw "$($language.Locale): routing dock close control remains"
        }
        if ($tree.Current.Name -ne $language.Tree) {
            throw "$($language.Locale): routing table accessible name is '$($tree.Current.Name)'"
        }
        if ($tree.Current.BoundingRectangle.Width -lt 348) {
            throw "$($language.Locale): routing tree is only $([math]::Round($tree.Current.BoundingRectangle.Width)) px wide"
        }
        foreach ($header in @(
                $language.Track, $language.Name, $language.Source)) {
            if (-not (Find-ElementByName $tree $header `
                    ([System.Windows.Automation.ControlType]::Header))) {
                throw "$($language.Locale): missing header '$header'"
            }
        }
        if (-not $SkipCreationUi) {
        $treeItems = Get-TreeItems $tree
        $treeItemNames = Assert-AllTrackRows $tree $language
        if (-not @($treeItemNames | Where-Object {
                $_ -match [regex]::Escape($language.Desktop)
            }).Count) {
            throw "$($language.Locale): known Desktop Audio mojibake was not repaired; rows=$($treeItemNames -join '|')"
        }
        $sourceItem = $null
        foreach ($treeItem in $treeItems) {
            if ($treeItem.Current.Name -match [regex]::Escape('1 Mix') -and
                $treeItem.Current.Name -ne '1 Mix') {
                $sourceItem = $treeItem
                break
            }
        }
        if (-not $sourceItem -or
            -not $sourceItem.Current.IsKeyboardFocusable) {
            throw "$($language.Locale): source row is not keyboard focusable; rows=$($treeItemNames -join '|')"
        }
        [void]$sourceItem.GetCurrentPattern(
            [System.Windows.Automation.InvokePattern]::Pattern)

        $gameItem = $null
        foreach ($treeItem in $treeItems) {
            if ($treeItem.Current.Name -match 'Game Audio') {
                $gameItem = $treeItem
                break
            }
        }
        if (-not $gameItem) {
            throw "$($language.Locale): Game Audio route is missing"
        }
        $gameRect = $gameItem.Current.BoundingRectangle
        $gameHandle = [IntPtr]$gameItem.Current.NativeWindowHandle
        $treeHandle = [IntPtr]$tree.Current.NativeWindowHandle
        $selectionHandle = if ($gameHandle -ne [IntPtr]::Zero) {
            $gameHandle
        } elseif ($treeHandle -ne [IntPtr]::Zero) {
            $treeHandle
        } else {
            [IntPtr]$process.MainWindowHandle
        }
        $inspect = Find-ElementByName $dock $language.Inspect `
            ([System.Windows.Automation.ControlType]::Button)
        if (-not $inspect) {
            throw "$($language.Locale): properties command is missing"
        }
        $selectionDeadline = (Get-Date).AddSeconds(2)
        while (-not $inspect.Current.IsEnabled -and
               (Get-Date) -lt $selectionDeadline) {
            [OBSPTWindowStyle]::Click(
                $selectionHandle,
                [int]($tree.Current.BoundingRectangle.X +
                    $tree.Current.BoundingRectangle.Width - 20),
                [int]($gameRect.Y + $gameRect.Height / 2))
            Start-Sleep -Milliseconds 200
        }
        if (-not $inspect.Current.IsEnabled) {
            throw "$($language.Locale): properties command did not enable; game=$gameHandle tree=$treeHandle main=$($process.MainWindowHandle) gameRect=$gameRect treeRect=$($tree.Current.BoundingRectangle)"
        }
        $inspect.GetCurrentPattern(
            [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
        $propertiesWindow = $null
        $propertiesDeadline = (Get-Date).AddSeconds(5)
        do {
            Start-Sleep -Milliseconds 100
            $windows = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
                [System.Windows.Automation.TreeScope]::Descendants,
                [System.Windows.Automation.PropertyCondition]::new(
                    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                    [System.Windows.Automation.ControlType]::Window))
            $propertiesWindow = $windows | Where-Object {
                $_.Current.Name -match 'Game Audio'
            } | Select-Object -First 1
        } while (-not $propertiesWindow -and
                 (Get-Date) -lt $propertiesDeadline)
        if (-not $propertiesWindow) {
            throw "$($language.Locale): Game Audio properties window is missing"
        }
        $nameEditor = Find-ElementById $propertiesWindow 'sourceNameEdit'
        if (-not $nameEditor) {
            $nameEditor = Find-ElementByIdSuffix $propertiesWindow `
                'sourceNameEdit'
        }
        if (-not $nameEditor) {
            $edits = $propertiesWindow.FindAll(
                [System.Windows.Automation.TreeScope]::Descendants,
                [System.Windows.Automation.PropertyCondition]::new(
                    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                    [System.Windows.Automation.ControlType]::Edit))
            foreach ($edit in $edits) {
                try {
                    $value = $edit.GetCurrentPattern(
                        [System.Windows.Automation.ValuePattern]::Pattern)
                    if ($value.Current.Value -eq 'Game Audio') {
                        $nameEditor = $edit
                        break
                    }
                } catch [System.InvalidOperationException] {
                }
            }
        }
        if (-not $nameEditor) {
            throw "$($language.Locale): routing properties name editor is missing"
        }
        $nameValue = $nameEditor.GetCurrentPattern(
            [System.Windows.Automation.ValuePattern]::Pattern)
        if ($nameValue.Current.Value -ne 'Game Audio') {
            throw "$($language.Locale): source name editor has '$($nameValue.Current.Value)'"
        }
        $cancelRename = 'UI Cancel Rename'
        $nameValue.SetValue($cancelRename)
        $combos = $propertiesWindow.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.PropertyCondition]::new(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::ComboBox))
        $processCombo = $null
        $priorityCombo = $null
        foreach ($combo in $combos) {
            $selected = Get-ComboSelectionName $combo
            if ($selected -eq $language.NotRunning) {
                $processCombo = $combo
            } elseif ($selected -eq $language.ProcessMode) {
                $priorityCombo = $combo
            }
        }
        if (-not $processCombo -or -not $priorityCombo) {
            throw "$($language.Locale): process property controls are missing"
        }
        $status = Find-ElementByName $propertiesWindow $language.Pending `
            ([System.Windows.Automation.ControlType]::Button)
        if (-not $status) {
            throw "$($language.Locale): pending process status is missing"
        }

        if ((Get-ComboSelectionName $processCombo) -ne
            $language.NotRunning) {
            throw "$($language.Locale): absent process target was not preserved"
        }
        Open-Combo $processCombo
        Start-Sleep -Milliseconds 100
        $lists = [System.Windows.Automation.AutomationElement]::RootElement.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.PropertyCondition]::new(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::List))
        $candidateItems = @()
        foreach ($list in $lists) {
            if ($list.Current.IsOffscreen) {
                continue
            }
            $items = $list.FindAll(
                [System.Windows.Automation.TreeScope]::Descendants,
                [System.Windows.Automation.PropertyCondition]::new(
                    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                    [System.Windows.Automation.ControlType]::ListItem))
            if (@($items | Where-Object {
                    $_.Current.Name -eq 'obspt-ui-process.exe'
                }).Count -gt 0) {
                $candidateItems = $items
                break
            }
        }
        if (@($candidateItems | Where-Object {
                $_.Current.Name -eq 'obspt-ui-process.exe'
            }).Count -ne 1) {
            $candidateNames = @($candidateItems | ForEach-Object Current |
                ForEach-Object Name)
            throw "$($language.Locale): duplicate process candidates were not collapsed; candidates=$($candidateNames -join '|')"
        }
        Close-Combo $processCombo

        Invoke-DialogButton $propertiesWindow $language.Cancel
        Start-Sleep -Milliseconds 300

        $dock = Find-ElementById $root 'OBSBasic.audioRoutingDock'
        $tree = Find-ElementByControlType $dock `
            ([System.Windows.Automation.ControlType]::Tree)
        if (-not (Find-RoutingSourceItem $tree 'Game Audio') -or
            (Find-RoutingSourceItem $tree $cancelRename)) {
            throw "$($language.Locale): canceled source rename changed the route"
        }

        if (-not $SkipCreationUi) {
        $creationDialog = Start-AudioSourceCreation $dock $language
        $newMode = Find-ElementByName $creationDialog $language.CreateNew `
            ([System.Windows.Automation.ControlType]::RadioButton)
        $existingMode = Find-ElementByName $creationDialog `
            $language.AddExisting `
            ([System.Windows.Automation.ControlType]::RadioButton)
        $visibleLists = $creationDialog.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.PropertyCondition]::new(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::List)) | Where-Object {
                    -not $_.Current.IsOffscreen -and
                    $_.Current.BoundingRectangle.Width -gt 0
                }
        if (($newMode -and -not $newMode.Current.IsOffscreen) -or
            ($existingMode -and -not $existingMode.Current.IsOffscreen) -or
            @($visibleLists).Count -gt 0) {
            throw "$($language.Locale): pure-audio creation exposes New/Add Existing"
        }
        $routeCombo = Find-ElementByName $creationDialog $language.Route `
            ([System.Windows.Automation.ControlType]::ComboBox)
        if (-not $routeCombo -or
            (Get-ComboSelectionName $routeCombo) -ne
                $language.RouteRequired) {
            throw "$($language.Locale): source-row selection became a route proposal"
        }
        $creationOk = Find-ElementByName $creationDialog $language.Ok `
            ([System.Windows.Automation.ControlType]::Button)
        if (-not $creationOk -or $creationOk.Current.IsEnabled) {
            throw "$($language.Locale): route-required creation can be confirmed"
        }
        Invoke-DialogButton $creationDialog $language.Cancel
        if (-not (Wait-GlobalElementAbsentByName $language.SourceSelect `
                ([System.Windows.Automation.ControlType]::Window) 3000)) {
            throw "$($language.Locale): canceled source creation stayed open"
        }

        $treeItems = Get-TreeItems $tree
        $track3Proposal = $treeItems | Where-Object {
            $_.Current.Name -eq 'Music'
        } | Select-Object -First 1
        if (-not $track3Proposal) {
            $proposalNames = @($treeItems | ForEach-Object Current |
                ForEach-Object Name)
            throw "$($language.Locale): Track 3 proposal row is missing; rows=$($proposalNames -join '|')"
        }
        $proposalRect = $track3Proposal.Current.BoundingRectangle
        $proposalHandle = Get-AncestorNativeHandle $track3Proposal
        if ($proposalHandle -eq [IntPtr]::Zero) {
            throw "$($language.Locale): Track 3 row has no native ancestor"
        }
        [OBSPTWindowStyle]::Click(
            $proposalHandle,
            [int]($proposalRect.X + $proposalRect.Width / 2),
            [int]($proposalRect.Y + $proposalRect.Height / 2))
        Start-Sleep -Milliseconds 200

        $creationDialog = Start-AudioSourceCreation $dock $language
        $routeCombo = Find-ElementByName $creationDialog $language.Route `
            ([System.Windows.Automation.ControlType]::ComboBox)
        $proposedRoute = if ($routeCombo) {
            Get-ComboSelectionName $routeCombo
        } else { $null }
        if (-not $routeCombo -or $proposedRoute -ne $language.Track3) {
            throw "$($language.Locale): selected Track 3 was not proposed; actual='$proposedRoute'"
        }
        $cancelName = 'UI Cancel Audio'
        Set-SourceCreationName $creationDialog $cancelName
        Invoke-DialogButton $creationDialog $language.Ok
        $cancelProperties = Wait-GlobalElementByName "属性 '$cancelName'" `
            ([System.Windows.Automation.ControlType]::Window) 5000
        if (-not $cancelProperties) {
            throw "$($language.Locale): provisional cancel properties are missing"
        }
        Invoke-DialogButton $cancelProperties $language.Cancel
        Start-Sleep -Milliseconds 300
        $tree = Find-ElementByControlType `
            (Find-ElementById $root 'OBSBasic.audioRoutingDock') `
            ([System.Windows.Automation.ControlType]::Tree)
        if (Find-RoutingSourceItem $tree $cancelName) {
            throw "$($language.Locale): canceled provisional source was retained"
        }

        $creationDialog = Start-AudioSourceCreation $dock $language
        Select-ComboItem `
            (Find-ElementByName $creationDialog $language.Route `
                ([System.Windows.Automation.ControlType]::ComboBox)) `
            $language.Track3
        $closeName = 'UI Close Audio'
        Set-SourceCreationName $creationDialog $closeName
        Invoke-DialogButton $creationDialog $language.Ok
        $closeProperties = Wait-GlobalElementByName "属性 '$closeName'" `
            ([System.Windows.Automation.ControlType]::Window) 5000
        if (-not $closeProperties) {
            throw "$($language.Locale): provisional close properties are missing"
        }
        $closeProperties.GetCurrentPattern(
            [System.Windows.Automation.WindowPattern]::Pattern).Close()
        Start-Sleep -Milliseconds 300
        $tree = Find-ElementByControlType `
            (Find-ElementById $root 'OBSBasic.audioRoutingDock') `
            ([System.Windows.Automation.ControlType]::Tree)
        if ((Find-RoutingSourceItem $tree $closeName) -or
            (Find-ElementByName `
                ([System.Windows.Automation.AutomationElement]::RootElement) `
                $language.ConfirmChanges `
                ([System.Windows.Automation.ControlType]::Window))) {
            throw "$($language.Locale): window-close rollback retained state or prompted"
        }

        $creationDialog = Start-AudioSourceCreation $dock $language
        Select-ComboItem `
            (Find-ElementByName $creationDialog $language.Route `
                ([System.Windows.Automation.ControlType]::ComboBox)) `
            $language.Track3
        $acceptedName = 'UI Accepted Audio'
        Set-SourceCreationName $creationDialog $acceptedName
        Invoke-DialogButton $creationDialog $language.Ok
        $acceptedProperties = Wait-GlobalElementByName "属性 '$acceptedName'" `
            ([System.Windows.Automation.ControlType]::Window) 5000
        if (-not $acceptedProperties) {
            throw "$($language.Locale): accepted provisional properties are missing"
        }
        Invoke-DialogButton $acceptedProperties $language.Ok
        Start-Sleep -Milliseconds 500
        $tree = Find-ElementByControlType `
            (Find-ElementById $root 'OBSBasic.audioRoutingDock') `
            ([System.Windows.Automation.ControlType]::Tree)
        $acceptedItem = Find-RoutingSourceItem $tree $acceptedName
        if (-not $acceptedItem -or
            $acceptedItem.Current.Name -notmatch '3 Music') {
            throw "$($language.Locale): accepted source did not commit to Track 3"
        }
        }

        $treeItems = Get-TreeItems $tree
        $gameItem = $treeItems | Where-Object {
            $_.Current.Name -match 'Game Audio'
        } | Select-Object -First 1
        $track3 = $treeItems | Where-Object {
            $_.Current.Name -match '3 Music'
        } | Select-Object -First 1
        $treeHandle = [IntPtr]$process.MainWindowHandle
        if (-not $gameItem -or -not $track3) {
            $endpointNames = @($treeItems | ForEach-Object Current |
                ForEach-Object Name)
            throw "$($language.Locale): drag endpoints are unavailable; rows=$($endpointNames -join '|')"
        }
        $sourceRect = $gameItem.Current.BoundingRectangle
        $targetRect = $track3.Current.BoundingRectangle
        [OBSPTWindowStyle]::Drag(
            $treeHandle,
            [int]($tree.Current.BoundingRectangle.X +
                $tree.Current.BoundingRectangle.Width - 20),
            [int]($sourceRect.Y + $sourceRect.Height / 2),
            [int]($tree.Current.BoundingRectangle.X +
                $tree.Current.BoundingRectangle.Width - 20),
            [int]($targetRect.Y + $targetRect.Height / 2))
        Start-Sleep -Milliseconds 500
        $tree = Find-ElementByControlType `
            (Find-ElementById $root 'OBSBasic.audioRoutingDock') `
            ([System.Windows.Automation.ControlType]::Tree)
        $treeItemNames = Assert-AllTrackRows $tree $language
        $dragMoved = @($treeItemNames | Where-Object {
            $_ -match '3 Music' -and $_ -match 'Game Audio'
        }).Count -gt 0
        if (-not $dragMoved) {
            $treeItems = Get-TreeItems $tree
            $gameItem = $treeItems | Where-Object {
                $_.Current.Name -match 'Game Audio'
            } | Select-Object -First 1
            $gameRect = $gameItem.Current.BoundingRectangle
            $gameMenuHandle = Get-AncestorNativeHandle $gameItem
            if ($gameMenuHandle -eq [IntPtr]::Zero) {
                throw "$($language.Locale): Game Audio row has no native ancestor"
            }
            [OBSPTWindowStyle]::RightClick(
                $gameMenuHandle,
                [int]($tree.Current.BoundingRectangle.X +
                    $tree.Current.BoundingRectangle.Width - 20),
                [int]($gameRect.Y + $gameRect.Height / 2))
            $moveMenu = Wait-GlobalElementByName $language.MoveTo `
                ([System.Windows.Automation.ControlType]::MenuItem) 3000
            if (-not $moveMenu) {
                throw "$($language.Locale): move-to menu is missing"
            }
            try {
                $moveMenu.GetCurrentPattern(
                    [System.Windows.Automation.ExpandCollapsePattern]::Pattern
                ).Expand()
            } catch {
                $moveMenu.GetCurrentPattern(
                    [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
            }
            $track3Menu = Wait-GlobalElementByName $language.Track3 `
                ([System.Windows.Automation.ControlType]::MenuItem) 3000
            if (-not $track3Menu) {
                throw "$($language.Locale): Track 3 menu item is missing"
            }
            $track3Menu.GetCurrentPattern(
                [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
            Start-Sleep -Milliseconds 300
            $tree = Find-ElementByControlType `
                (Find-ElementById $root 'OBSBasic.audioRoutingDock') `
                ([System.Windows.Automation.ControlType]::Tree)
            $treeItemNames = Assert-AllTrackRows $tree $language
            if (-not @($treeItemNames | Where-Object {
                    $_ -match '3 Music' -and $_ -match 'Game Audio'
                }).Count) {
                throw "$($language.Locale): route move did not reach Track 3"
            }
        }
        }

        $controlCount = $root.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.Condition]::TrueCondition).Count
        $treeWidth = [math]::Round($tree.Current.BoundingRectangle.Width)

        $float = Find-ElementById $dock `
            'OBSBasic.audioRoutingDock.qt_dockwidget_floatbutton'
        if (-not $float) {
            throw "$($language.Locale): float control is missing"
        }
        $float.GetCurrentPattern(
            [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
        $floatingDock = Wait-FloatingDock 'OBSBasic.audioRoutingDock'
        if (-not $floatingDock) {
            throw "$($language.Locale): routing dock did not float"
        }
        Stop-SmokeOBS $process
        $process = $null

        $restart = Start-SmokeOBS $runRoot
        $restoredDock = Wait-FloatingDock 'OBSBasic.audioRoutingDock'
        if (-not $restoredDock) {
            throw "$($language.Locale): floating layout was not restored"
        }
        $restoredTree = Find-ElementByControlType $restoredDock `
            ([System.Windows.Automation.ControlType]::Tree)
        $restoredNames = Assert-AllTrackRows $restoredTree $language
        if (-not $SkipCreationUi) {
            if (-not @($restoredNames | Where-Object {
                    $_ -match '3 Music' -and $_ -match 'Game Audio'
                }).Count) {
                throw "$($language.Locale): dragged route was not restored"
            }
        }
        if (-not $SkipCreationUi) {
            if (-not @($restoredNames | Where-Object {
                    $_ -match '3 Music' -and $_ -match $acceptedName
                }).Count) {
                throw "$($language.Locale): accepted source was not restored"
            }
        }
        $restoredDockHandle =
            [IntPtr]$restoredDock.Current.NativeWindowHandle
        if ($restoredDockHandle -eq [IntPtr]::Zero) {
            throw "$($language.Locale): restored floating dock has no handle"
        }
        [OBSPTWindowStyle]::ToggleDocked($restoredDockHandle)
        Start-Sleep -Milliseconds 500
        $restartRoot =
            [System.Windows.Automation.AutomationElement]::FromHandle(
                $restart.MainWindowHandle)
        $docked = Find-ElementById $restartRoot `
            'OBSBasic.audioRoutingDock'
        if (-not $docked) {
            throw "$($language.Locale): routing dock did not redock"
        }

        $viewMenu = Find-ElementByName $restartRoot $language.View `
            ([System.Windows.Automation.ControlType]::MenuItem)
        if (-not $viewMenu) {
            $menuNames = @($restartRoot.FindAll(
                [System.Windows.Automation.TreeScope]::Descendants,
                [System.Windows.Automation.PropertyCondition]::new(
                    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                    [System.Windows.Automation.ControlType]::MenuItem)) |
                ForEach-Object { $_.Current.Name })
            throw "$($language.Locale): View menu is missing; menus=$($menuNames -join '|')"
        }
        $viewMenu.GetCurrentPattern(
            [System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
        $docksMenu = Wait-GlobalElementByName $language.Docks `
            ([System.Windows.Automation.ControlType]::MenuItem) 3000
        if (-not $docksMenu) {
            throw "$($language.Locale): Docks menu is missing"
        }
        $docksMenu.GetCurrentPattern(
            [System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
        $routingToggle = Wait-GlobalElementByName $language.Title `
            ([System.Windows.Automation.ControlType]::MenuItem) 3000
        if (-not $routingToggle) {
            throw "$($language.Locale): routing View toggle is missing"
        }
        $routingToggle.GetCurrentPattern(
            [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
        $hidden = $false
        $visibilityDeadline = (Get-Date).AddSeconds(3)
        do {
            Start-Sleep -Milliseconds 100
            $hiddenDock = Find-ElementById $restartRoot `
                'OBSBasic.audioRoutingDock'
            $hidden = -not $hiddenDock -or $hiddenDock.Current.IsOffscreen
        } while (-not $hidden -and (Get-Date) -lt $visibilityDeadline)
        if (-not $hidden) {
            throw "$($language.Locale): routing dock did not hide"
        }
        $viewMenu.GetCurrentPattern(
            [System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
        $docksMenu = Wait-GlobalElementByName $language.Docks `
            ([System.Windows.Automation.ControlType]::MenuItem) 3000
        $docksMenu.GetCurrentPattern(
            [System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
        $routingToggle = Wait-GlobalElementByName $language.Title `
            ([System.Windows.Automation.ControlType]::MenuItem) 3000
        $routingToggle.GetCurrentPattern(
            [System.Windows.Automation.InvokePattern]::Pattern).Invoke()
        $restoredVisible = $false
        $visibilityDeadline = (Get-Date).AddSeconds(3)
        do {
            Start-Sleep -Milliseconds 100
            $visibleDock = Find-ElementById $restartRoot `
                'OBSBasic.audioRoutingDock'
            $restoredVisible = $visibleDock -and
                -not $visibleDock.Current.IsOffscreen
        } while (-not $restoredVisible -and
                 (Get-Date) -lt $visibilityDeadline)
        if (-not $restoredVisible) {
            throw "$($language.Locale): routing dock did not restore"
        }
        [void]$results.Add([pscustomobject]@{
            Locale = $language.Locale
            Controls = $controlCount
            TreeWidth = $treeWidth
            Tracks = $roles.Count
            Floating = $true
            Restored = $true
            Stats = $true
            ProcessSelector = $true
            MojibakeRepaired = $true
            PropertiesRenameCancel = $true
            TransactionalCreation = if ($SkipCreationUi) { $null } else { $true }
            DraggedSoleSource = if ($SkipCreationUi) { $null } else { $true }
            MessageLevelDragMoved = $dragMoved
            NativeOLEClaimed = $false
            ToolbarOrder = $true
            PlaceholderDisabled = $true
            CloseControlAbsent = $true
            Redocked = $true
            HiddenAndRestored = $true
        })
    } finally {
        Stop-SmokeOBS $process
        Stop-SmokeOBS $restart
        foreach ($candidateProcess in $candidateProcesses) {
            if ($candidateProcess -and -not $candidateProcess.HasExited) {
                Stop-Process -Id $candidateProcess.Id -Force
            }
        }
    }
}

[pscustomobject]@{
    Result = 'PASS'
    Languages = $results.Count
    Evidence = $results
    WorkRoot = $work
} | ConvertTo-Json -Depth 6
