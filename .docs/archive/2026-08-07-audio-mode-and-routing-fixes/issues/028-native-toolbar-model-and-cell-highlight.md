Status: done

## What to build

Bring the Audio and Routing Panel toolbar back to the native Visual
Composition Panel model: commands are never put into the disabled state. They
stay permanently enabled, always render the theme icon at full strength, always
show the hover outline, and decide whether to act only when clicked. A command
that does not apply to the current selection performs no action and shows no
message.

This reverses the approach taken in issue 026. The machinery introduced there
to make a disabled button look and behave like an enabled one is removed
entirely: the pointer-tracking hover property, the mirrored hover rules added to
each bundled theme, and the repolish-on-availability-change workaround. The
availability decision moves out of widget state and into the command handlers,
which must now tolerate being invoked with no selection, with Mix selected, or
with a Muted Sources Group row selected. See ADR-0003 for why.

Separately, the table's selection highlight must fill the entire cell rather
than only the text it contains, matching the Visual Composition Panel. Cell
selection semantics stay as they are: the source column and the name column
remain independently selectable, because F2 renames a different thing in each.
Do not change the panel to row selection.

## Acceptance criteria

- [x] No toolbar command in the Audio and Routing Panel is ever placed in the
  disabled state, regardless of selection or source count.
- [x] Every command renders its theme icon at full strength at all times, in
  every bundled theme.
- [x] Every command shows the hover outline at all times, using the theme's
  own hover rule with no additional property or selector.
- [x] Invoking a command that does not apply to the current selection performs
  no action and produces no message.
- [x] Add, Remove, Properties, Move Up and Move Down keep their existing
  eligibility behaviour for no selection, Mix, an Editor Track, Muted Sources
  Group, and single- or multi-source tracks.
- [x] Command handlers are safe to invoke in every one of those states.
- [x] The hover-tracking property, the per-theme mirrored hover rules, and the
  availability-change repolish added by issue 026 are all removed, and no
  bundled theme retains a rule that references them.
- [x] Toolbar command slots keep identical geometry and visibility as the
  selection changes.
- [x] The selection highlight fills the whole cell, not just the text.
- [x] The source column and the name column remain independently selectable.
- [x] Focused Qt coverage runs offscreen, sends no real keyboard or mouse
  input, and shows no window.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

None - can start immediately

## Implementation

The disabled state is gone, and with it everything issue 026 built to
compensate for it: the pointer-tracking watcher, the `obsptHovered` marker
property, the mirrored hover rules in Dark, Acri and Rachni, and the repolish
on availability change. Net deletion.

Eligibility moved into the command handlers, which is where it has to live
once widget state no longer gates anything. Remove now checks whether the
source may be removed, and Move checks the model's move availability for the
requested direction, instead of trusting that the button would have been
disabled. The keyboard reorder shortcut no longer consults widget state
either. `UpdateCommands()` only ever flipped enabled flags, so it is gone.

The right-click "move to track" submenu still disables individual entries.
ADR-0003 covers toolbar commands only; a transient menu does not suffer from
the icon and layout problems that motivated the rule.

The cell highlight turned out to be about the **focus outline**, not the
selection background. Qt collapses `SE_ItemViewItemFocusRect` onto the text
rect unless the item reports `showDecorationSelected`, which is why the
outline hugged short labels. The table's delegate now sets it. Measured: the
outline was 40 px inside a 240 px cell, and spans the full cell after the fix.

The delegate moved out of the dock and into the table itself, where it
belongs and where the existing table test can reach it. The table also gained
a small query for the rect the style will outline, so the behaviour is
assertable without rendering.

Coverage: the toolbar test now asserts commands are built enabled, that no
hover machinery or theme marker rule survives, and that slots keep their
geometry and icons. The table test asserts the focus outline spans the cell.
Both run offscreen with no real input.

## Remaining

Foreground-only, deferred to the acceptance package: visual confirmation in
Dark that unavailable commands look identical to available ones and that the
cell outline reads correctly, plus the zh-CN runtime smoke.
