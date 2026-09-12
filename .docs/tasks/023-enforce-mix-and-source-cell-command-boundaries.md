Status: done

## Parent

`.docs/prd/2026-07-27-audio-routing-interaction-lifetime.md`

## What to build

Make Compatibility Mix ownership and table-cell intent explicit throughout the
Audio and Routing Panel. Track and name cells select a track; only an eligible
source cell selects and manipulates a source. Keep Desktop Audio manageable for
properties and mute while preventing any operation that violates its fixed Mix
assignment, including while it resides in the Muted Sources Group.

## Acceptance criteria

- [x] Clicking columns 0-1 of an occupied anchor row selects its track and does not select the first source.
- [x] Clicking the source column selects that Source Identity and restores all applicable source commands.
- [x] A source context menu opens only from an eligible source cell.
- [x] Drag initiation is possible only from an eligible source cell.
- [x] With no source selected, Delete, Properties, Move Up, and Move Down are disabled while Add remains available.
- [x] A one-source Editor Track or Muted Sources Group disables both ordering commands.
- [x] First, middle, and last source selection enables only the ordering directions that can change state.
- [x] Selecting Desktop Audio in Mix enables Properties and mute control but disables dragging, reassignment, ordering, and deletion.
- [x] Mix and its name remain fixed and cannot be selected as an Editor Track destination.
- [x] Muted Desktop Audio cannot be assigned to an Editor Track or deleted.
- [x] Muted Desktop Audio can be reordered within the Muted Sources Group.
- [x] Unmuting Desktop Audio always returns it to Compatibility Mix.
- [x] Active sources show their current destination selected and disabled; muted sources show their prior destination selected and actionable.
- [x] Track-cell selection still proposes eligible occupied Editor Tracks during new pure-audio source creation.
- [x] Focused Qt tests cover occupied anchor cells, empty track cells, ordinary sources, Desktop Audio in Mix, and Desktop Audio in the Muted Sources Group.

## Blocked by

- `021-crash-free-atomic-context-menu-routing.md`
- `022-crash-free-native-audio-route-drag.md`
