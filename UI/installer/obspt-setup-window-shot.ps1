# Launch the installer, capture its main window with PrintWindow, then close it.
# AGENTS.md requires a real launch to the Setup main window after packaging.
param(
    [string]$Installer = 'K:/Projects/dev/OBS-PT/.target/.distribution/installer/OBS-PT-1.3.0-Installer.exe',
    [string]$Out = 'K:/Projects/dev/OBS-PT/.target/_tools/setup-window.png',
    [int]$WaitSec = 20
)
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public class S {
  public delegate bool EnumWindowsProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern int SendMessageTimeout(IntPtr h, uint m, IntPtr w, IntPtr l, uint f, uint t, out IntPtr res);
  public struct R { public int L, T, Rt, B; }
}
'@

$p = Start-Process -FilePath $Installer -PassThru
"launched pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($WaitSec)
$script:h = [IntPtr]::Zero
while ($script:h -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline) {
    [S]::EnumWindows({ param($hw, $l)
        $pid2 = 0
        [S]::GetWindowThreadProcessId($hw, [ref]$pid2) | Out-Null
        if ([int]$pid2 -eq $p.Id) {
            $sb = New-Object System.Text.StringBuilder 512
            [S]::GetWindowText($hw, $sb, 512) | Out-Null
            if ($sb.ToString() -match 'OBS-PT') { $script:h = $hw; return $false }
        }
        return $true
    }, [IntPtr]::Zero) | Out-Null
    if ($script:h -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 400 }
}
if ($script:h -eq [IntPtr]::Zero) { 'setup window not found'; exit 1 }

$sb2 = New-Object System.Text.StringBuilder 512
[S]::GetWindowText($script:h, $sb2, 512) | Out-Null
$r = New-Object S+R
[S]::GetWindowRect($script:h, [ref]$r) | Out-Null
$w = $r.Rt - $r.L; $ht = $r.B - $r.T
"title  : $($sb2.ToString())"
"window : ${w}x${ht}"

$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[S]::PrintWindow($script:h, $hdc, 2) | Out-Null
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"saved  : $Out"

$res = [IntPtr]::Zero
[S]::SendMessageTimeout($script:h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero, 2, 8000, [ref]$res) | Out-Null
Start-Sleep -Seconds 3
if (Get-Process -Id $p.Id -ErrorAction SilentlyContinue) {
    Stop-Process -Id $p.Id -Force
    'closed (forced)'
} else { 'closed' }