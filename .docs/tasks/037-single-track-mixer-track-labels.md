Status: review

## What to build

In `单音轨` Audio Mode the Mixer must stop showing stale multi-track badges.
The routing model deliberately preserves each source's multi-track Editor Track
assignment and mute flag across a mode switch, and the mixer currently renders
those preserved values as-is: a source that belonged to `2 Game` keeps its
`2 Game` badge, and a source muted for multi-track editing keeps its `M` badge,
even though neither has any meaning while only the Compatibility Mix exists.

While in `单音轨`, every volume control hides its track badge except sources
fixed to the Compatibility Mix (Desktop Audio), which keep showing `1 Mix`.
`多音轨` rendering is unchanged, and switching back restores every badge to its
true assignment because the model data is never touched — this is a derivation
rule at paint time, not a state rewrite, matching how the mode itself works.

Vertical and horizontal mixer layouts follow the same rule, since both render
the same badge.

## Acceptance criteria

- [x] In `单音轨`, no volume control shows an Editor Track badge (`2 Game`,
  `3 Music`, …) or an `M` muted badge; Desktop Audio controls keep showing
  `1 Mix`.
- [x] A non-Desktop source muted in `多音轨` (state preserved across the
  switch) shows no badge of any kind while in `单音轨`; a muted Desktop Audio
  still shows `1 Mix`, because the badge derives only from the source's fixed
  identity there and the preserved mute is ignored like everywhere else in
  that mode.
- [x] Switching back to `多音轨` restores every badge to its true assignment
  and mute state without any data having been rewritten.
- [ ] Vertical and horizontal mixer layouts behave identically.
- [ ] An offline test pins the badge derivation rule for both modes, and its
  discrimination is proven by temporarily reverting the rule and watching the
  test fail.

## Blocked by

None - can start immediately
