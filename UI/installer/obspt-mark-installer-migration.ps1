param(
    [Parameter(Mandatory = $true)]
    [string]$IniPath,

    [Parameter(Mandatory = $true)]
    [ValidateSet('0', '1')]
    [string]$Value
)

# Issue 075 (ADR 0009): record whether the application must run the
# installer-driven default replacement on its next launch.
#
# The edit is surgical on purpose: only the single
# [OBSPT] InstallerMigrationVersion line changes. Everything else - key
# order, comments, blank lines, the UTF-8 BOM, and the file's own line
# ending - is preserved byte for byte. NSIS cannot do this safely (the
# 528-character [BasicWindow] DockState plus mixed CRLF/LF across trees),
# which is why the installer delegates to this helper.
#
# Failure is never fatal: a missing or unwritable file exits 0 silently.
# The installer must not be blocked by a configuration marker (ADR-0006
# posture: no prompt, no abort).

$ErrorActionPreference = 'Stop'

function Exit-WithoutFailing {
    exit 0
}

if (-not (Test-Path -LiteralPath $IniPath -PathType Leaf)) {
    Exit-WithoutFailing
}

try {
    $bytes = [System.IO.File]::ReadAllBytes($IniPath)
} catch {
    Exit-WithoutFailing
}

$hasBom = $bytes.Length -ge 3 -and
    $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF

try {
    $text = [System.IO.File]::ReadAllText($IniPath)
} catch {
    Exit-WithoutFailing
}

# Detect the dominant line ending so the rewritten file keeps it.
$crlf = ([regex]::Matches($text, "`r`n")).Count
$lf = ([regex]::Matches($text, "`n")).Count
$eol = if ($crlf -gt 0 -and $crlf * 2 -ge $lf) { "`r`n" } else { "`n" }

# Work on the raw line list, keeping a sentinel so a file without a trailing
# newline is restored without one.
$endsWithNewline = $text.EndsWith("`n")
$body = $text
if ($endsWithNewline) {
    $body = $text.Substring(0, $text.Length - 1)
    # A CRLF file leaves its final CR behind; strip it too, or the join
    # below re-adds a second one and corrupts the last line.
    if ($body.EndsWith("`r")) {
        $body = $body.Substring(0, $body.Length - 1)
    }
}
$lines = [System.Collections.Generic.List[string]]::new()
foreach ($line in ($body -split "`r`n|`n")) {
    $lines.Add($line)
}
if ($endsWithNewline) {
    # The split consumed the final newline; drop the empty trailing element
    # it produced so the join below re-adds exactly one.
    if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') {
        $lines.RemoveAt($lines.Count - 1)
    }
}

$section = ''
$sectionIndex = -1
$insertAt = -1
$markerIndex = -1

for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]

    if ($line -match '^\s*\[([^\]]+)\]\s*$') {
        $section = $matches[1]
        if ($section -ieq 'OBSPT') {
            $sectionIndex = $i
            $insertAt = $i + 1
        }
        continue
    }

    if ($section -ieq 'OBSPT') {
        if ($line -match '^\s*InstallerMigrationVersion\s*=') {
            $markerIndex = $i
            break
        }
        $insertAt = $i + 1
    }
}

$newLine = "InstallerMigrationVersion=$Value"

if ($markerIndex -ge 0) {
    $lines[$markerIndex] = $newLine
} elseif ($sectionIndex -ge 0) {
    $lines.Insert($insertAt, $newLine)
} else {
    # No [OBSPT] section at all: append one, preserving what came before.
    if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ne '') {
        $lines.Add('')
    }
    $lines.Add('[OBSPT]')
    $lines.Add($newLine)
}

$result = [string]::Join($eol, $lines)
if ($endsWithNewline) {
    $result += $eol
}

try {
    $encoding = if ($hasBom) {
        [System.Text.UTF8Encoding]::new($true)
    } else {
        [System.Text.UTF8Encoding]::new($false)
    }
    [System.IO.File]::WriteAllText($IniPath, $result, $encoding)
} catch {
    Exit-WithoutFailing
}

exit 0
