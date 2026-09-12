Status: done

## Parent

`../prd/2026-07-16-application-audio-multitrack-recording.md`

## User stories

1-12, 16-17, 32-35

## What to build

Deliver the user-facing game/music separation workflow on a fresh portable installation. Keep only Recording Track 1 enabled, make Desktop Audio the Track 1 compatibility mix, define stable `Mix`, `Game`, `Music`, `Voice Chat`, `Microphone`, and `Aux` metadata, and leave the default Scene Collection free of guessed Application Audio Sources.

Using two real Application Audio Sources, prove that a user can retain a complete first track while routing game and background music to independent tracks for editing. Document the setup, Windows requirement, BETA status, and application-process boundaries.

## Acceptance criteria

- [x] A fresh shipped Profile enables only Recording Track 1.
- [x] Shipped Desktop Audio is routed only to mixer 1 and no Application Audio Source is pre-created or guessed.
- [x] Recording Tracks 1-6 have stable metadata names `Mix`, `Game`, `Music`, `Voice Chat`, `Microphone`, and `Aux`.
- [x] Existing 160 Kbps AAC defaults remain unchanged.
- [x] Adding the game Source leaves it on mixer 2; the user can route a second music Source to mixer 3 through Advanced Audio Properties and manually enable Tracks 2 and 3.
- [x] A Hybrid MP4 recording contains three AAC streams with the expected metadata names.
- [x] Frequency or content analysis proves that Mix contains both deterministic signals, Game contains only the game signal, and Music contains only the music signal.
- [x] A normal player that selects only the first audio stream plays the complete Mix.
- [x] MP4, MKV, MOV, TS, and M3U8 retain their existing recording behavior in smoke tests; FLV continues to expose and write one selected track.
- [x] Existing Profiles and Scene Collections are not migrated or rewritten by normal startup.
- [x] User documentation explains game/music separation, manual track enabling, the Build 19041 requirement, BETA labeling, browser process-tree granularity, and unsupported windowless helper processes.

## Blocked by

- `003-capture-one-application-audio-source.md`
