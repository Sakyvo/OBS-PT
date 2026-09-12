Status: done

## Parent

`.docs/prd/2026-07-25-executable-wide-application-audio-capture.md`

## What to build

Expand the executable tracer into the complete runtime contract: every independent same-name application instance is captured, parent/child audio sessions are de-duplicated, and the active capture set follows session creation, expiration, application exit, and restart. Identity lookup failures must fail closed while verified instances continue, with useful source state and logging.

## Acceptance criteria

- [x] Every non-expired audio session whose executable matches the configured identity contributes to the source unless it is already covered by a retained matching ancestor.
- [x] Unrelated same-name root instances are captured concurrently and mixed once each.
- [x] Parent and child sessions from one process tree never duplicate the same audio or increase gain through overlapping Process Loopback clients.
- [x] A newly created matching session joins an existing source without reopening properties or restarting OBS-PT.
- [x] Expiring or exiting one matching instance removes only that capture client and does not interrupt other matching roots.
- [x] Restarting the last matching application moves the source through pending and back to active without losing Source Identity, route, order, mute, monitoring, or volume state.
- [x] Session-set reconciliation is idempotent and releases every client, callback, and process handle when a source is removed or OBS-PT shuts down.
- [x] An audio session whose executable identity cannot be verified is excluded rather than guessed.
- [x] Permission failure is visible in source state and logs while other verified same-name instances continue to capture.
- [x] Deterministic model tests cover multiple roots, parent/child chains, repeated notifications, inaccessible identity, add/remove ordering, and empty-set transitions.
- [x] Real Hybrid MP4 tests use distinct tones to prove all independent roots are included, parent/child overlap is not doubled, and late start/exit/restart transitions preserve correct output.
- [x] Existing audio routing, topology locking, Compatibility Mix, streaming, Replay Buffer, and output lifecycle tests remain green.

## Evidence

- The executable-session model test covers independent roots, matching parent/child sessions, duplicate notifications, inaccessible identity, add/remove/retain, and empty transitions.
- `multi-root` recorded 440 Hz and 880 Hz from two unrelated instances of one executable into one `Game` source.
- `process-tree` recorded two real same-frequency parent/child sounds at about twice the single-tone magnitude, proving one tree capture rather than overlapping parent and child clients.
- `multi-root-lifecycle` retained 440 Hz while the 880 Hz root exited and rejoined; both measured about 127 at the recording tail.
- `restart` moved the last root through remove/add without recreating the source or losing Track 2 output.

## Blocked by

- `011-capture-browser-audio-session-by-executable.md`
