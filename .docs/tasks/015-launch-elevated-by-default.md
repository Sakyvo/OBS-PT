Status: done

Validation note: implementation, deterministic model and execution tests,
one-process elevated startup, complete argument preservation, and installer
preference preservation pass. This workstation has `EnableLUA=0` and
`ConsentPromptBehaviorAdmin=0`, so Windows cannot present a rejectable UAC
prompt. The real accepted relaunch was verified; an injected production
executor verifies that refusal/failure continues the original process and is
attempted exactly once.

## Parent

`.docs/prd/2026-07-25-executable-wide-application-audio-capture.md`

## What to build

Add the optional Elevated Launch policy. The shipped Global Config enables it, Advanced > General lets the user disable it, and a non-elevated launch requests UAC before loading the main application. Successful elevation preserves portable startup context; declining silently continues the original non-elevated process.

## Acceptance criteria

- [x] Advanced > General exposes a localized `Run OBS-PT as administrator` checkbox backed by Global Config.
- [x] Shipped defaults enable the preference, while a user-disabled value persists across restarts and configuration-preservation flows according to the existing Global Config contract.
- [x] Changing the checkbox does not interrupt the current process or any active output and is clearly marked as taking effect on the next launch.
- [x] An enabled, non-elevated launch requests administrator privileges before the main window, welcome flow, updater, sources, or outputs initialize.
- [x] A successful elevated relaunch preserves the complete command-line argument sequence and original working directory, then exits the original process.
- [x] An already elevated launch proceeds exactly once and cannot enter a relaunch loop.
- [x] Refusing or failing UAC silently continues the original non-elevated launch without an extra warning.
- [x] A disabled preference starts without requesting UAC.
- [x] The executable manifest remains non-forcing and the installer retains its independent user-level execution policy.
- [x] Start Menu, desktop, installer finish-page, direct executable, updater relaunch, profile/collection selection, tray-minimized, and output-start arguments retain their existing behavior.
- [x] en-US, zh-CN, zh-TW, ja-JP, and ko-KR labels and restart-effect text are complete and fit the Advanced settings layout.
- [x] A deterministic startup-decision test covers disabled, enabled/non-elevated, already elevated, accepted relaunch, refused relaunch, argument preservation, and working-directory preservation.
- [x] Windows acceptance verifies an accepted elevated launch; deterministic execution-layer refusal verifies the normal process continues without another dialog or retry loop on this UAC-disabled workstation.

## Blocked by

None - can start immediately
