Status: review

## What to build

Give the user control over whether multi-track recordings embed the
Compatibility Mix, via a new `多音轨行为` group box on the Settings → Audio
page, placed above the existing `通用` group box (the page stays a plain
column of group boxes — no tab widget is introduced).

The group box contains exactly one option, `Mix音轨行为`, laid out as a
QFormLayout row mirroring the `采样率` row in the `通用` group box below:
the label cell carries the help icon to the LEFT of the `Mix音轨行为` text
(same properties-view glyph, inlined into the label's rich text before the
text, tooltip on the label), and the field cell carries a combo box with
`生成` (default) and `不生成`. Radio buttons were shipped in the first
iteration and rejected at acceptance: the control must match the page's
existing form rows. The icon reuses the properties-view
pattern: the existing `help.svg` / `help_light.svg` resources picked by theme
lightness, inlined into the label's rich text, with the tooltip on the label.
The explanation text (five locales, `Basic.Settings.Audio.*` naming) covers:
`生成` lets ordinary players hear everything but editing software imports the
Mix alongside the Editor Tracks, doubling volume until the Mix is disabled;
`不生成` writes only the Editor Tracks so ordinary players hear just the first
one (`Game` by default) while editors import without overlap; the option
affects only multi-track-capable local recordings and is irrelevant to
streaming and `单音轨` mode. The explanation also states the consequence the
domain model forces: Desktop Audio lives only on the Compatibility Mix, so
with `不生成` any sound captured solely through Desktop Audio — not covered by
an Application Audio source — is not recorded at all.

Semantics of `不生成`: the Compatibility Mix is omitted from the local
recording topology whenever the container can carry multiple tracks — standard
recording, custom FFmpeg recording with a multi-track container
(mkv/mp4/mov/ts), and Replay Buffer. The file's first audio stream then is the
first non-empty Editor Track (`2 Game` by default), which is what ordinary
players will play; if every Editor Track is empty the file has no audio
stream, consistent with the existing all-muted rule. Streaming always sends
the Compatibility Mix, the VOD Track is unaffected, single-track-only
containers (flv/hls/m3u8) keep their Mix-only topology, and `单音轨` Audio
Mode ignores the option entirely (the Mix is always generated there). The
control is not disabled merely because the current Scene Collection is in
`单音轨` — a Profile setting is not coupled to Scene Collection state.

Guard while outputs run, same rule as the Audio Mode switch
(`outputHandler->Active()`): opening the Settings dialog during an active
output shows the radios disabled with an inline hint that recording or
streaming is in progress (a visible label — disabled widgets do not show
tooltips); if an output starts while the dialog is open and the user still
flips the value, saving refuses with a warning box, mirroring
`AudioRouting.AudioMode.Busy`. A running session's file topology never
changes mid-write either way; the new value takes effect from the next
session.

Persistence: a new key in the Profile's `[Audio]` section, absent meaning
`生成` so existing installs keep today's behavior. The shipped PotPvP
`basic.ini` carries the explicit default. `CONTEXT.md` already documents the
term `Mix Track Behavior / Mix音轨行为`.

## Acceptance criteria

- [ ] Settings → Audio shows the `多音轨行为` group box above `通用`; the
  `Mix音轨行为` row uses the same QFormLayout label/field structure as the
  `采样率` row, with the help icon left of the label text and a combo box
  offering `生成` (default on fresh installs and on installs lacking the key)
  and `不生成`.
- [ ] The help icon matches the properties-view help glyph in both light and
  dark themes, and its tooltip explains both choices, the player/editor
  trade-off, and the streaming / `单音轨` exemption in all five locales.
- [x] With `不生成`, a standard recording contains no Mix stream: its first
  audio stream is the first non-empty Editor Track and players default to it;
  with `生成`, today's topology is unchanged.
- [x] Replay Buffer and custom FFmpeg recordings with multi-track containers
  follow the same option; flv/hls/m3u8 custom outputs stay Mix-only;
  streaming and VOD output are bit-identical either way.
- [x] In `单音轨` mode recordings contain the Mix regardless of the option.
- [ ] With any output active, the Settings dialog disables the combo and
  shows the inline hint; flipping the value after an output started mid-dialog
  is refused at save with the busy warning and the stored value is untouched.
- [x] The shipped PotPvP `basic.ini` contains the explicit default key, and
  staged/installer packaging carries it.
- [x] Offline tests pin the topology derivation for both values (including
  the empty-editor-tracks case, the Desktop Audio exclusion, and the
  single-track and single-track-container exemptions), with discrimination
  proven by temporarily forcing the gate open and watching the omit cases
  fail. The settings-page guard has no background seam (it hangs off the
  Settings dialog and the live output handler) and is verified by human
  acceptance instead.

## Blocked by

None - can start immediately
