Status: review

## What to build

In Settings → Output → Streaming, the OBS-PT VOD Track row must align with
the page's standard form rows. The row is added programmatically when the
service supports a VOD track; the advanced-mode variant currently parks a
RightToLeft checkbox in the label column, so the row's selector sits
misaligned against the field column shared by the other combos (`速率控制`
etc.). Rework the row so the selector shares the field column's left edge and
width with those combos, and the label side reads like a standard form row.

Apply the same check to the simple-output variant of the row
(`simpleStreamingLayout`) so both output modes present the same alignment.

Behavior is unchanged: the checkbox still enables the selector, both still
persist `VodTrackEnabled` / `VodTrackIndex`, and the row still appears only
for services with VOD support. This is a GUI-only slice — verify by human
acceptance rather than by manufacturing a background seam.

## Acceptance criteria

- [ ] Advanced output mode: the VOD Track row's selector aligns with the
  field column used by the surrounding combos (left edge and width), and the
  label side no longer uses the RightToLeft checkbox trick.
- [ ] Simple output mode: the VOD Track row follows the same alignment.
- [x] Enable/disable, persistence, and service-dependent visibility behave
  exactly as before.

## Blocked by

None - can start immediately
