Status: review

## What to build

Append a multitrack-audio manual-setup notice to the first-run Welcome
wizard's audio page (Page4, `OBSPT.Welcome.Page4` — the 音频与路由 page
written in issue 045).

The notice tells users: multitrack defaults are only a starting preset; every
machine differs (which process is the game, which plays music, which browser),
so to actually record the sounds you want you must open the 音频与路由 panel
and check/adjust each source's track assignment (game/music/mic etc.), adding
or removing sources as needed.

Five locales (en-US, zh-CN, zh-TW, ja-JP, ko-KR) get the appended paragraph,
keyed on a stable marker phrase so the smoke test can assert presence without
duplicating the whole 045 copy.

## Acceptance criteria

- [ ] All five locale files' `OBSPT.Welcome.Page4` values end with the new
      manual-setup paragraph (rich text, `<br>` separated, consistent style).
- [ ] The 045 copy above the appended paragraph is unchanged.
- [ ] No new wizard pages; OBSWelcome.ui untouched.
- [ ] Smoke/static check asserts the marker phrase in all five locales'
      Page4 value.
- [ ] Manual: fresh install (or About → reopen welcome), page 4 shows the
      notice in the selected UI language.

## Blocked by

None - can start immediately
