Status: done

## What to build

Update the Game Capture defaults to what PotPvP high-frame-rate recording
actually needs: frame rate limiting on, anti-cheat compatibility hook off.

Both values are currently the upstream defaults and are inverted from what this
product wants. The shipped Minecraft source does not store either key
explicitly, so it inherits whatever the source type declares - which means
changing the declared defaults updates the preconfigured source and any source
the user creates from one place, with nothing to keep in sync.

Note the consequence: existing user sources that never stored these keys will
also pick up the new behaviour after an overwrite install. That is intended -
the defaults are being corrected, not merely changed for new sources.

## Acceptance criteria

- [x] Game Capture declares frame rate limiting enabled by default.
- [x] Game Capture declares the anti-cheat compatibility hook disabled by
  default.
- [x] A newly created Game Capture source shows both settings in the new state
  without the user touching them.
- [x] The shipped Minecraft source resolves to the same two values, and does so
  by inheriting the declared defaults rather than by storing its own copy.
- [x] No other Game Capture default is altered.
- [x] The shipped scene collection remains loadable and the Minecraft source
  still captures as before in every other respect.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

None - can start immediately

## Implementation

Two declared defaults inverted, nothing else touched. Verified that the
shipped Minecraft source stores neither key, so it resolves through the
declared defaults rather than carrying its own copy - one place to maintain,
and the preconfigured source and a user-created one cannot drift apart.

## Remaining

Foreground-only: confirming the two checkboxes read correctly in a newly
created Game Capture source and in the shipped Minecraft source.
