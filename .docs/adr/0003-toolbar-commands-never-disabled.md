---
status: accepted
---

# Toolbar Commands Never Enter the Disabled State

The Visual Composition Panel toolbar never calls `setEnabled(false)` on its
commands: they stay enabled permanently, always render the theme icon at full
strength, always show the hover outline, and only decide whether to act when
clicked. The Audio and Routing Panel toolbar follows the same model, so the two
panels are visually and behaviourally identical. A command that does not apply
to the current selection performs no action and shows no message.

## Considered Options

- Mark unavailable commands `setEnabled(false)` and style the disabled state:
  rejected after implementing it. Qt's disabled rendering fights the toolbar
  contract in three separate ways, and every one of them needed a workaround.

  The theme supplies toolbar icons through QSS (`qproperty-icon`), so the
  QAction deliberately carries no icon of its own. Disabling a button makes
  QToolButton re-sync its icon from that empty action and drop the themed one,
  which collapses the button to a text label and pushes its neighbours into the
  overflow area where they disappear - the toolbar's size hint went from 135 px
  to 751 px. Qt also never sets `WA_UnderMouse` on a disabled widget and its
  `:hover` pseudo-state is mutually exclusive with `:disabled`, so a disabled
  command cannot show hover feedback through the stylesheet at all; force-setting
  the attribute paints nothing. Restoring both behaviours required repolishing on
  `QEvent::EnabledChange` plus a pointer-tracking event filter driving a custom
  `obsptHovered` property that every bundled theme had to mirror.

  All of that machinery exists only to make a disabled button look and feel like
  an enabled one. Not disabling it is simpler and matches upstream exactly.

- Keep the button enabled but grey the icon manually: rejected because it
  reintroduces the "is this broken?" ambiguity without any accessibility gain,
  and upstream shows no such state.

## Consequences

Availability is decided inside the slot, not by widget state, so every command
handler must tolerate being invoked with no selection, with Mix selected, or
with a Muted Sources Group row selected. Assistive technology reports these
commands as available even when they will do nothing; this is upstream OBS
behaviour and is accepted deliberately in exchange for a toolbar that never
shifts, dims, or loses its icons.

The rule covers only *context* unavailability. A command disabled because the
user switched the Audio Mode to single-track is a different case: that one is
hidden or reported with a message telling the user to switch back, because the
user cannot otherwise discover why it does nothing.
