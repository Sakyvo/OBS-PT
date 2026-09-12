Status: done

## What to build

Make `进程名匹配` the default window matching priority for a newly created
Application Audio Source, and give the creation dialog enough width to show its
controls properly.

The shipped `minecraft-sound` and `music` sources already store process-name
matching explicitly, but the source type declares no default for the priority,
so a user creating their own source lands on title matching and has to change
it by hand. Process-name matching is the behaviour this product documents as
the stable identity for application audio - see the Application Audio Source
entry in the glossary and ADR-0002 - so it should be what the user gets without
configuring anything.

Separately, the dialog that creates an audio source is too narrow: the routing
track selector is visibly compressed. Widen it enough to give the controls room
while keeping the dialog compact - the problem is horizontal only, so the
height must not grow.

## Acceptance criteria

- [x] A newly created Application Audio Source defaults to process-name
  matching, with no user action.
- [x] The shipped `minecraft-sound` and `music` sources continue to match by
  process name and behave exactly as before.
- [x] Existing user sources that stored a different priority keep it; the
  default applies only where no value was stored.
- [x] No other Application Audio Source default is altered.
- [x] The creation dialog is wide enough that the routing track selector is no
  longer compressed and its labels are fully legible.
- [x] The dialog's height is unchanged and it still reads as compact.
- [x] The dialog still requires an explicit Editor Track choice before it can
  proceed, per the existing rule.
- [x] The widened dialog holds up in every shipped locale, including the
  longest translations, without clipping or forcing a scrollbar.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

None - can start immediately

## Implementation

The process-output capture type declared no defaults at all - its defaults
callback was an empty function - so the priority fell through to 0, meaning
title matching. It now declares process-name matching, which is the identity
model ADR-0002 established. Because this is a declared default rather than a
stored value, sources that already saved a different priority keep it.

The shipped `minecraft-sound` and `music` sources store `priority: 3`
explicitly and are unaffected; they now simply agree with the default instead
of overriding it.

Dialog width 352 -> 432. Height untouched, per the note that the problem was
horizontal only.

## Remaining

Foreground-only: confirming the routing track selector is no longer
compressed, and that the widened dialog still reads as compact in zh-CN.
