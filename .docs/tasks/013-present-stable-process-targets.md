Status: done

## Parent

`.docs/prd/2026-07-25-executable-wide-application-audio-capture.md`

## What to build

Make the Application Audio Source property experience express executable-wide capture directly. Process-name mode presents a stable, de-duplicated process selector sourced from visible applications and audio sessions, preserves absent targets as `not running`, and keeps process and window choices independent across mode switches.

## Acceptance criteria

- [x] Selecting process-name mode changes the target field from a window-oriented presentation to a localized Process selector.
- [x] Process candidates are the union of executable names from visible applications and observed audio sessions.
- [x] Candidate executable names are de-duplicated case-insensitively and do not expose one row per Chromium helper process or window.
- [x] The selected value displays a stable executable name and never displays a stale page or window title as its identity.
- [x] A configured executable absent from the current candidate set remains selected with a localized `not running` state and remains eligible for later automatic capture.
- [x] Candidate refreshes do not clear or rewrite the current target while the properties window is open.
- [x] Switching between process-name and window-oriented modes preserves both the executable target and the last window target.
- [x] The selector communicates that process-name mode covers all same-name instances and does not promise per-tab browser separation.
- [x] Permission and pending states have localized, accessible text and do not rely on color alone.
- [x] en-US, zh-CN, zh-TW, ja-JP, and ko-KR text is complete and fits the supported properties layout.
- [x] UI Automation verifies duplicate collapse, stable executable display, absent-target persistence, mode switching, accessible names, and restart persistence.
- [x] Game Capture and Window Capture keep their existing HWND-based property behavior.

## Blocked by

- `012-reconcile-and-deduplicate-executable-sessions.md`
