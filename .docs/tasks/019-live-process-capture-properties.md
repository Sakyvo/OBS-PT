Status: done

## Parent

`.docs/prd/2026-07-26-audio-routing-usability-defaults.md`

## What to build

Keep process-name Application Audio Source properties normalized and truthful while the dialog is open. Saving one match mode removes inactive fields from the other mode, and process-session reconciliation refreshes waiting, capturing, and permission state without using instantaneous audio level or rewriting the selected target. This covers PRD user stories 29-35.

## Acceptance criteria

- [x] Saving process-name matching retains the executable identity and removes the inactive window tuple (title, class, and executable).
- [x] Saving a window-oriented match mode retains its window target and removes the inactive process executable identity.
- [x] Switching modes in the property editor preserves the user's unsaved choices as needed for editing, while the committed settings contain only the active match mode.
- [x] A configured executable with no matching audio session displays the localized waiting state without clearing its route or target.
- [x] Establishing the first matching audio session changes an open property editor to capturing without reopening properties or restarting OBS-PT.
- [x] Removing the final matching session changes an open property editor back to waiting.
- [x] Permission or identity-resolution failure has a distinct localized state and does not suppress capture from other identifiable matching instances.
- [x] Capturing state depends on one or more established matching sessions and does not flicker when the application is temporarily silent.
- [x] Property refreshes do not reset pending user edits, rewrite the configured executable, or change Editor Track assignment.
- [x] Executable-session model tests cover session-count, late-start, exit, restart, idempotent reconciliation, and permission transitions.
- [x] Real Application Audio smoke proves audible output for a stale-window-field process target across late start and restart.
- [x] Non-activating `zh-CN` UI Automation verifies localized waiting presentation and accessible names; live capturing/permission transitions are qualified by the session model, property-refresh source audit, and real late-start/restart recording smokes.

## Blocked by

None - can start immediately
