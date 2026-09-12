Status: done

## Parent

`.docs/prd/2026-07-27-audio-routing-interaction-lifetime.md`

## What to build

Make Audio and Routing context-menu commands safe across creation, action
dispatch, destruction, persistence, and subsequent main-menu use. Replace the
borrowed-style ownership violation and express reassignment, optional unmute,
and destination ordering as one atomic Source Identity transition. A command
must commit completely, save and refresh once after nested menu dispatch, or
leave the prior state untouched.

## Acceptance criteria

- [x] Opening and dismissing the source context menu leaves the shared application style and main menu bar valid.
- [x] Triggering `Move to Track` repeatedly does not hang or crash OBS-PT, including after the temporary menu has been destroyed.
- [x] The destructive removal presentation remains visibly distinct without transferring ownership of a borrowed widget or application style.
- [x] Moving an active source to another Editor Track preserves its Source Identity, source instance, name, properties, monitoring state, and collection ownership.
- [x] A context-menu move appends the source to the destination Editor Track.
- [x] Moving an active source to its current Editor Track is a no-op with no save or presentation rebuild.
- [x] The active source's current track is shown as selected and disabled in the destination menu.
- [x] Moving a source from the Muted Sources Group atomically changes its Editor Track, appends it, and unmutes it.
- [x] A muted source's prior track is shown as selected but remains actionable; choosing it restores the source to its prior track and position.
- [x] Plain mute and unmute preserve the prior Editor Track, order, and Monitor Only state.
- [x] A failed or topology-locked transition performs no partial assignment, unmute, ordering, save, or refresh.
- [x] One successful command produces one effective routing transition, one save, one mixer update, and one Audio and Routing rebuild.
- [x] Focused model tests cover commit, rollback, append, same-track no-op, muted restoration, muted reassignment, and topology locking.
- [x] A Qt lifetime test creates, triggers, and destroys the temporary menu, then exercises the main menu bar and proves process survival.

## Blocked by

None - can start immediately
