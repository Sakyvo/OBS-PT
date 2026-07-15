# Implementation Plan

## Ordered Work

1. Validate the task-local runner parses and refuses an active formal OBS-PT
   process before creating any disposable copies.
2. Verify hidden startup, isolated config/output paths, fixed 30-second timer,
   snapshot selection, explicit priority, and consistent CSV fields.
3. Re-read all retained run logs and classify game samples by shared-texture
   hook success before comparing lag counters.
4. Confirm the low-pressure pre-AMF/current comparison and the cross-version
   high-pressure reproduction recorded in `research/initial-evidence.md`.
5. Confirm binary attribution: identical active NVENC, capture, and Hybrid MP4
   plugins; identical core code sections; inactive AMF path on the RTX machine.
6. Apply the attribution gate. With current evidence, make no production source,
   default configuration, binary, or installer change.
7. Run the full task check, then capture the benchmark/sample-validity rules in
   the relevant Trellis spec during Phase 3.3.

## Validation

```powershell
$errors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
  (Resolve-Path '<task>/research/run-ab.ps1'), [ref]$null, [ref]$errors)
if ($errors.Count) { throw $errors }

Import-Csv 'build-v143/_perf-ab/20260713-134450-game/results.csv'
rg -i 'shared texture capture successful' '<accepted sample log>'
Get-Process -Name OBS-PT -ErrorAction SilentlyContinue
git status --short
```

Review gates:

- No accepted game sample lacks the success hook line.
- Current and pre-AMF valid samples are both at or below 0.5%.
- Production source/binary paths have no task changes.
- No OBS-PT test process remains.
- No NSIS build is required unless the attribution gate later authorizes a
  binary or plugin change.

## Risk And Rollback

- Rapid OBS restarts can leave game capture unhooked; classify rather than
  silently average those samples.
- Background workloads can change between samples; record memory/process state
  and avoid claiming small cross-run differences as source causality.
- Generated recordings are large. They remain under `_perf-ab` and can be
  discarded after review without touching formal recordings.
