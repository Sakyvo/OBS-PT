Status: review

## What to build

A global `Java Detection / Java 判别` mode that makes the `javaw.exe` ↔
`java.exe` pair match each other in BOTH capture paths, so Minecraft launched
via `java.exe` (instead of the usual launcher `javaw.exe`) is still captured
when loose mode is on.

**Setting**: new group box at the VERY BOTTOM of Settings → Advanced (after
the Network group), one label `Java 判别` with a combo `严格` / `宽松`
- persisted in Global Config `[General] JavaDetection` (`Strict` default,
  `Lenient`), Global scope, five locales wired
- locked while any output is active: the combo is disabled with the inline
  busy hint at load; a change after an output started mid-dialog is refused
  at save with the standard pattern and the stored value untouched
- never overwritten: it is a Global Config user value; same-generation
  installs (issue 039) leave the entire User Data Root untouched, so it
  survives by construction; no merge-list entry needed

**Propagation**: plugins cannot read Global Config. The UI (app startup after
scene load, and Settings save when the value changed) writes a per-source
`java_detection_lenient` bool (default false = strict) into the settings of
every `game_capture` and `wasapi_process_output_capture` source. Each plugin
reads only its own source setting — no cross-process config plumbing.

**Matching rule** (one semantic rule, minimal abstraction — no dynamic
registry): the alias group `{javaw.exe, java.exe}` matches bidirectionally
when leniency is on.

- Audio (`plugins/win-wasapi/executable-audio-session-model.cpp`):
  `SelectExecutableSessions` gains a leniency parameter; when on, the exe
  equality check (`EqualExecutable`) additionally treats the two names in the
  alias group as equal (both directions). Existing parent-chain/roothood
  logic is unchanged. Model test (`executable-audio-session-model-test.cpp`)
  covers: strict = java-only sessions do NOT match a javaw target; lenient =
  javaw target matches java.exe sessions AND java target matches javaw.exe
  sessions; non-java targets unaffected in both modes.
- Game capture (`libobs/util/windows/window-helpers.c`): the exe comparison
  inside `window_rating()` (`exe_matches = dstr_cmpi(&cur_exe, exe) == 0`)
  gains a leniency flag threaded through `ms_find_window` (new parameter, all
  in-repo call sites updated: game-capture passes its source setting,
  window-capture passes strict, the win-wasapi helper call passes strict).
  The C-side pair check mirrors the C++ rule with a cross-reference comment —
  the two implementations are the same 2-entry table, kept side-by-side in
  the issue and code comments so they stay in sync.

**Out of scope**: window capture and display capture sources keep strict
exclusive matching; no other executable alias groups (e.g. browser
families); no change to the shipped scene's `window:` string.

## Acceptance criteria

- [x] `EquivalentExecutables`-style rule in ONE place per side (audio model +
      window-helpers), each documented as the mirror of the other.
- [x] Audio model test: strict mode — `java.exe` sessions do not match a
      `javaw.exe` target; `javaw.exe` sessions do not match a `java.exe`
      target.
- [x] Audio model test: lenient mode — both cross-directions match; ordering,
      parent-chain coverage, and non-java targets are byte-identical to
      strict results.
- [ ] Game capture: lenient javaw target window-rates a `java.exe` window as
      hitting its exe clause (static-code-level check + manual GUI verify).
- [ ] Settings → Advanced bottom: `Java 判别` combo 严格(default)/宽松,
      disabled while any output is active with inline hint; save-flip during
      active output is refused and the stored value is untouched.
- [x] Propagation: on startup and on settings save, every game_capture and
      wasapi_process_output_capture source carries `java_detection_lenient`
      matching the global value; brand-new sources get the current global
      value at creation.
- [x] Values round-trip: Strict/Lenient persist across restart via Global
      Config; reinstall over same-generation does not touch them (039 skip).
- [x] All five locales carry the new strings (title, option, strict, lenient,
      busy hint + busy dialog).

## Acceptance round 1 fixes

- The first .ui edit corrupted the Advanced page tail (the combo rendered
  full-width with no label, the group title landed on the Hotkeys box). The
  whole tail region was rebuilt cleanly: the Hotkeys box is restored
  byte-identical to upstream, and the Java Detection box now mirrors the
  Network box structure (right-aligned 170px label column, topMargin 2) so
  the combo's left edge aligns with the Bind-to-IP combo above.

## Blocked by

None - can start immediately
