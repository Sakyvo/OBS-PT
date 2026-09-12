Status: done

## Parent

`.docs/prd/2026-07-26-audio-routing-usability-defaults.md`

## What to build

Make Audio and Routing the complete and unambiguous surface for reassigning and ordering existing manageable audio sources. Dragging and `Move to Track` move the existing Source Identity rather than creating a copy, while command availability, destructive styling, toolbar theming, and navigation reflect the real operation. This covers PRD user stories 4-8 and 20-27.

## Acceptance criteria

- [x] Every manageable audio source appears exactly once in Audio and Routing.
- [x] Dragging an existing source to another Editor Track changes its single assignment and removes it from the previous track.
- [x] `Move to Track` produces the same persisted route as drag-and-drop and never creates another OBS Source or Source Identity.
- [x] A moved source retains its name, capture properties, mute state where applicable, monitoring state, collection ownership, and Source Identity.
- [x] A one-source Editor Track or Muted Sources Group disables both reorder commands.
- [x] The first source in a multi-source group disables Move Up only, the last disables Move Down only, and a middle source enables both.
- [x] Toolbar buttons, keyboard commands, and context menus expose the same ordering availability and invoke the same routing mutations.
- [x] The audio-source removal command uses the established destructive color while movement and properties remain neutral.
- [x] Audio and Routing toolbar icons use the existing theme mechanism and remain legible in supported dark and light themes.
- [x] The redundant Audio and Routing button is absent from the mixer footer; the dock and menu entry remain available and focus the same panel.
- [x] Native Qt/model tests cover exact boundary command states and single-owner drag behavior.
- [x] Non-activating `zh-CN` UI Automation verifies direct cross-track movement and restart persistence; source/style/theme audits and native tests cover command parity, removal presentation, theme assets, and duplicate-entry removal without foreground interaction.
- [x] Moving remains blocked during topology-bearing output with the existing stop-and-restart guidance, while safe mute behavior is unchanged.

## Blocked by

None - can start immediately
