Status: done

## Parent

`.docs/prd/2026-07-27-audio-routing-interaction-lifetime.md`

## What to build

Finish the Audio and Routing workspace controls without changing Recording
Topology. Group all source commands on the left, reserve the right edge for a
disabled future multi-track recording control, and make the dock permanently
non-closable from its title bar while retaining floating, docking, movement,
layout persistence, and View-menu recovery.

## Acceptance criteria

- [x] The toolbar order is Add, Delete, Properties, Move Up, and Move Down on the left.
- [x] Move Up and Move Down appear immediately to the right of Properties rather than at the far edge.
- [x] A spacer separates source commands from the right-aligned multi-track recording placeholder.
- [x] The placeholder uses the approved green settings icon, accessible name, and tooltip.
- [x] The placeholder is visibly disabled, cannot be toggled, and never reads, writes, or changes configuration.
- [x] The placeholder does not alter Compatibility Mix, Editor Tracks, Recording Topology, output selection, or persistence.
- [x] The Audio and Routing title bar has no close button.
- [x] The float/dock control remains available and can switch between docked and small-window states.
- [x] The dock remains movable and its placement is restored by the existing layout system.
- [x] The View menu can still hide and restore Audio and Routing.
- [x] Locking and unlocking the main UI never restores the Audio and Routing close button.
- [x] Other docks retain their existing close, float, move, lock, and visibility behavior.
- [x] `zh-CN` background UI Automation verifies toolbar order and state, placeholder state, title-bar controls, floating, docking, hiding, restoration, and restart persistence without foreground activation.

## Blocked by

- `023-enforce-mix-and-source-cell-command-boundaries.md`
