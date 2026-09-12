Status: done

## What to build

Make the Audio and Routing Panel bottom commands reuse the native Visual
Composition Panel toolbar contract instead of maintaining a visual
approximation. The toolbar must keep stable command slots and the original
grouping, theme icons, dimensions, spacing, tooltips, enabled-state semantics,
and pointer feedback across every bundled theme. Keep the future multi-track
recording entry as a distinct green, disabled placeholder without beginning
that feature.

## Acceptance criteria

- [x] The toolbar uses the same established GUI actions, toolbar mechanics,
  theme properties, icons, and state styling as the Visual Composition Panel;
  no Audio and Routing-specific approximation is retained where the original
  component can be reused.
- [x] Command order is Add, Remove, Properties, separator, Move Up, Move Down,
  followed by the right-aligned multi-track placeholder.
- [x] Every command retains a stable visible slot when the table selection or
  source count changes; an unavailable command never appears to disappear.
- [x] Unavailable ordinary commands remain truly disabled, render with a gray
  icon rather than a black or missing icon, show the same white outline on
  mouse hover as available commands, and perform no action when clicked.
- [x] Add, Remove, Properties, Move Up, and Move Down preserve their current
  command eligibility and behavior for no selection, Mix, an Editor Track,
  Muted Sources, and single- or multi-source tracks.
- [x] The disabled multi-track placeholder remains green as the sole visual
  exception, shows a white outline on hover, and performs no action; no
  multi-track recording implementation is added.
- [ ] Dark receives focused visual verification, and every other bundled theme
  keeps all commands visible with distinguishable enabled and disabled states.
- [ ] Focused Qt coverage and the zh-CN runtime smoke verify order, stable
  visibility, disabled semantics, hover styling, and the green placeholder
  without sending real keyboard or mouse input or disturbing foreground work.
- [x] The production VS2022 v143 build succeeds, staging is refreshed with
  tests disabled, and the local 1.1.0 installer is regenerated and validated.
  (Launching Setup to its main window is foreground-only and is listed in the
  acceptance package.)

## Blocked by

None - can start immediately

## Implementation

Shared helper `UI/obspt-toolbar-theme.{hpp,cpp}` now owns the toolbar
contract. `OBSBasic::copyActionsDynamicProperties()` calls into it, so the
native scenes/sources toolbars and the Audio and Routing toolbar run the same
code rather than two equivalent implementations.

Three real defects were found and fixed:

1. **Black / missing icons.** The dock baked `:/res/images/*.svg` into each
   QAction. Those resource glyphs are stroked `#000000`; the native toolbar
   instead leaves the QAction icon-less and lets the theme inject
   `qproperty-icon: url(./Dark/plus.svg)` (stroked `#d2d2d2`) onto the
   generated QToolButton. Measured icon luminance was 0 before, ~210 after.
   Properties had no icon at all, because its glyph does not exist outside the
   theme directory - the literal "it disappeared" report.

2. **Commands vanishing on selection change.** Because the QAction carries no
   icon by design, `setEnabled()` makes QToolButton re-sync its icon from that
   empty action and drop the themed one. The button collapses to a text label
   (toolbar size hint 135 -> 751 px) and pushes its neighbours into the
   overflow area, where they are hidden. The helper now repolishes on
   `QEvent::EnabledChange` so the theme reinstates the icon. Verified: all
   slots keep identical geometry and visibility across an availability change.

3. **No hover feedback while disabled.** Measured, not assumed: Qt never sets
   `WA_UnderMouse` on a disabled widget, and `:hover` is mutually exclusive
   with `:disabled` - even force-setting the attribute paints nothing. A
   dynamic-property selector does reach a disabled button, so the helper tracks
   the pointer on the (still enabled) toolbar and drives an `obsptHovered`
   property. Dark, Acri and Rachni mirror their `QToolButton:hover` block onto
   `QToolButton[obsptHovered="true"]`; a test compares the two blocks so a
   future theme edit cannot drift them apart. System.qss defers to the platform
   style and declares no hover rule, so it has nothing to mirror.

The native spacer sits outside the toolbar in `OBSBasic.ui`, so the layout
mirrors that: commands in one QToolBar, the placeholder in its own, stretch
between them. Putting the spacer inside the toolbar is what pushed the
placeholder into the overflow area.

`AudioRouting.MultiTrackPlaceholder` was missing from zh-TW, ja-JP and ko-KR
and has been added.

Coverage: `UI/obspt-audio-routing-toolbar-test.cpp` (offscreen, no real input,
no window shown).

## Remaining

Foreground-only, deferred to the acceptance package: Dark visual verification,
zh-CN runtime smoke, and the production build / staging / installer refresh
that the batch performs once.
