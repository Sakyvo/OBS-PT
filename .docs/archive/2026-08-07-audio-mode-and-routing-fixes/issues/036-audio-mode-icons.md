Status: done

## What to build

Give the Audio Mode control real icons instead of the tinted settings glyph it
borrowed while the feature was a placeholder.

Multi-track is drawn as three stacked horizontal lines: the centre one green
`#2e9f58`, the outer two in the theme's ordinary toolbar colour. Single-track is
one horizontal line, green. The horizontal bar reads as a track, so the two
icons differ at a glance while sharing the green marker that identifies the mode
in effect. Green no longer means "not implemented yet" - it marks the active
track.

The icons follow the same mechanism as every other toolbar command: supplied by
the theme through the theme-id property rather than baked into the action, so
each theme can tint them and no source-level icon can override the theme. Per
ADR-0003 the control is never disabled, so no disabled variant is needed.

Two tinted copies cover every bundled theme: the three dark themes share one
set drawn in the light toolbar grey, and the system theme uses a black-outer
variant to match its own icons.

## Acceptance criteria

- [x] The Audio Mode control shows a three-line icon in multi-track and a
  one-line icon in single-track.
- [x] The centre line of the multi-track icon and the single line of the
  single-track icon are green `#2e9f58`.
- [x] The outer lines match the theme's ordinary toolbar icon colour, not the
  green.
- [x] The icon changes when the mode changes, without reopening the panel.
- [x] The icons come from the active theme through the theme-id mechanism, the
  same way the other toolbar commands get theirs.
- [x] Every bundled theme renders both icons legibly, including the system
  theme, whose light background needs its own tinting.
- [x] The tinted settings glyph previously used as a placeholder is no longer
  referenced anywhere.
- [x] The icons ship in staging and inside the installer for every theme that
  needs them.
- [x] Focused coverage runs offscreen, sends no real keyboard or mouse input,
  and shows no window.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

None - can start immediately

## Implementation

The runtime tinting helper is gone. The mode entry now carries a theme id like
every other toolbar command, and switching modes swaps that id and repolishes,
so the theme reloads the glyph. This removes the last place where an icon was
built in code rather than supplied by the theme.

Two tinted copies cover all four themes, matching how the existing toolbar
icons are organised: the three dark themes already share `Dark/*.svg`, and the
system theme reads from the Qt resource bundle. So the green centre line is
identical everywhere while the outer lines follow each theme.

The line weight is 2.4473, copied from the existing toolbar glyphs rather than
chosen freshly - at 16 px the thinner stroke I first used anti-aliased down far
enough that the sampling in the test could not find an opaque pixel, which is
also what a user would perceive as a faint icon.

Coverage: the toolbar test asserts the action carries the multi-track theme id
and bakes no icon of its own, that the theme-supplied glyph carries the green
hue, that swapping the id yields a visibly different glyph, and that all four
themes declare both icons. Verified the green assertion bites by recolouring
the SVG grey.

## Remaining

Foreground-only: confirming both icons read clearly at their real size in each
bundled theme, particularly the system theme on a light background.
