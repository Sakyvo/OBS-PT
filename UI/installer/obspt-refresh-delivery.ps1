# Refresh .target/app/ from the staging root and rewrite .target/delivery.json.
# app-prev is never overwritten (it guards the last version a user accepted).
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$target = Join-Path $repo '.target'
$staging = Join-Path $target '_pkg/OBS-PT'
$app = Join-Path $target 'app'
$installer = Join-Path $target 'installer'
$version = '1.2.0'
$installerExe = Join-Path $installer "OBS-PT-$version-Installer.exe"

if (-not (Test-Path $staging)) { throw "staging missing: $staging" }
if (-not (Test-Path $installerExe)) { throw "installer missing: $installerExe" }

# 1. Mirror staging -> app (replace contents, keep the directory itself).
Get-ChildItem -LiteralPath $app -Force | Remove-Item -Recurse -Force
foreach ($dir in Get-ChildItem -LiteralPath $staging) {
    Copy-Item -LiteralPath $dir.FullName -Destination $app -Recurse -Force
}

function Get-Manifest([string]$root) {
    $items = Get-ChildItem -LiteralPath $root -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($repo.Length + 1).Replace('\', '/')
        [pscustomobject]@{
            path   = $rel
            bytes  = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower()
        }
    }
    @($items | Sort-Object path)
}

$appFiles = Get-Manifest $app
$exe = Join-Path $app 'bin/OBS-PT.exe'
$exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLower()
$exeBytes = (Get-Item -LiteralPath $exe).Length

# Deterministic fingerprint of the declared build-input set: SHA-256 over the
# sorted "<repo-relative path>\n<file sha256>\n" stream. Reproducible by rerunning
# this script; the same set is declared in .docs/spec/build-output.md.
function Get-InputFingerprint([string[]]$sets) {
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($set in $sets) {
        $p = Join-Path $repo $set
        if (Test-Path -LiteralPath $p -PathType Leaf) {
            $rel = $set.Replace('\', '/')
            $h = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLower()
            $lines.Add("$rel`n$h")
        } elseif (Test-Path -LiteralPath $p -PathType Container) {
            foreach ($f in Get-ChildItem -LiteralPath $p -Recurse -File) {
                $rel = $f.FullName.Substring($repo.Length + 1).Replace('\', '/')
                $h = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLower()
                $lines.Add("$rel`n$h")
            }
        }
    }
    $payload = (($lines | Sort-Object) -join "`n") + "`n"
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $bytes = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($payload))
    $sha.Dispose()
    return ($bytes | ForEach-Object { $_.ToString('x2') }) -join ''
}

$inputSets = @('CMakeLists.txt', 'UI', 'libobs', 'plugins')
$fingerprint = Get-InputFingerprint $inputSets

$doc = [ordered]@{
    schemaVersion = 1
    project       = 'OBS-PT'
    app           = [ordered]@{
        status            = 'pending-manual-acceptance'
        source            = [ordered]@{
            kind   = 'input-fingerprint'
            value  = $fingerprint
            inputs = $inputSets
        }
        buildCommand      = 'cmake --build .target --config RelWithDebInfo --parallel (BUILD_TESTS=OFF) + cmake --install --prefix .target/_pkg/OBS-PT, mirrored into .target/app'
        acceptanceCommand = 'test/win/obspt-installer-config-reset-smoke.ps1 (PASS) + manual GUI acceptance'
        pendingSinceUtc   = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ss.fffZ')
        pendingManualItems = @(
            '主题后门: 设置 › 高级 页输入 debug → 通用 页主题下拉框出现 Acri/Rachni/System;关闭设置窗口再打开,隐藏主题消失',
            '低版本升级迁移(用旧版安装树跑本包后首次启动): 排布自动变为四块、录像编码器按本机硬件重探测(HEVC,无则回退)、Java 判别自动为宽松',
            '低版本升级迁移幂等: 迁移后手动调整排布,重启不再被覆盖(标记已置 1)',
            '录像默认编码器: 全新安装或「重置配置」后 设置 › 输出 › 录像 的编码器为 HEVC (NVIDIA NVENC HEVC / AMD HEVC)',
            '录像关键帧间隔: 设置 › 输出 › 录像 的「关键帧间隔(秒,0=自动)」为 0',
            '串流默认编码器仍为 H.264 (设置 › 输出 › 串流)',
            '串流速率控制: 设置 › 输出 › 串流 的「速率控制」为 CBR、码率 4500 Kbps',
            'Java 判别: 设置 › 高级 的「Java 判别」默认为 宽松(未被升级迁移覆盖时,手动改成严格后重启仍保持严格)',
            '默认布局: 全新安装或「停靠 → 重置界面」后为四块排布(上: 统计 + 预览;下: 音轨与路由 + 混音器 + 场景/来源 + 控件)',
            '主窗口启动时居中于主屏工作区(不再沿用上次位置)',
            '虚拟摄像机: 控件面板有「启动虚拟摄像机」,DirectShow 设备列表出现 OBS Virtual Camera 且自环出画面',
            '虚拟摄像机模块被浏览器占用时安装: 不弹窗、不杀进程,静默跳过该文件并完成安装',
            '安装器: 运行 OBS-PT-1.2.0-Installer.exe 到 Setup 主窗口,常规安装/升级流程无异常'
        )
        primaryEntry      = 'bin/OBS-PT.exe'
        primaryEntryBytes = $exeBytes
        primaryEntrySha256 = $exeHash
        files             = $appFiles
    }
    installer     = [ordered]@{
        status            = 'pending-manual-acceptance'
        source            = [ordered]@{
            kind   = 'input-fingerprint'
            value  = $fingerprint
            inputs = $inputSets
        }
        buildCommand      = 'makensis UI/installer/obspt-setup.nsi'
        acceptanceCommand = '7z t + manual Setup window launch'
        pendingSinceUtc   = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ss.fffZ')
        pendingManualItems = @(
            '安装器: 运行 OBS-PT-1.2.0-Installer.exe 到 Setup 主窗口,常规安装/升级流程无异常',
            '安装后 DirectShow 设备列表出现 OBS Virtual Camera,自环出画面;卸载后设备消失'
        )
        primaryEntry      = $null
        files             = @(
            [pscustomobject]@{
                path   = ".target/installer/OBS-PT-$version-Installer.exe"
                bytes  = (Get-Item -LiteralPath $installerExe).Length
                sha256 = (Get-FileHash -LiteralPath $installerExe -Algorithm SHA256).Hash.ToLower()
            }
        )
    }
}
$doc | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $target 'delivery.json') -Encoding UTF8

"app files      : $($appFiles.Count)"
"fingerprint    : $fingerprint"
"app exe sha256 : $exeHash"
"installer      : $((Get-Item $installerExe).Length) bytes"