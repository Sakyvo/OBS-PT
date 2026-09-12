## Problem Statement

OBS-PT currently exposes a `进程名匹配` mode for an Application Audio Source, but the runtime still chooses one matching window and starts Process Loopback from that window's PID. The property selector also persists and displays the complete old window title/class/executable tuple. A Chromium-based browser such as Vivaldi can therefore show a stale selected title, initialize Process Loopback successfully, and still produce silence because the actual audio session belongs to a different process. Reopening properties or selecting another live window should not be required whenever a title, tab, process, or startup order changes.

The Audio and Routing panel also has a drag-and-drop presentation defect. Moving the only source out of Track 2 updates the routing model correctly, but Qt's internal move cleanup removes the newly rebuilt empty Track 2 row a second time. The row remains missing until another event forces a refresh, creating the appearance that the track was deleted.

Finally, a non-elevated OBS-PT process cannot always identify or capture audio sessions and games running with higher privileges. OBS-PT has no user-facing elevated-launch preference, so users must discover and apply Windows compatibility settings themselves.

## Solution

Make `进程名匹配` an executable-wide Application Audio Source mode. The selected executable name becomes the stable identity. OBS-PT observes matching Windows audio sessions, captures every independent same-name application instance, de-duplicates sessions in the same parent/child process tree, and reconciles additions and removals without user intervention. Window titles, classes, visible windows, and startup order no longer determine capture eligibility in this mode.

Present process mode as a process selector rather than a stale window selector. Candidate executable names come from visible applications and audio sessions, are de-duplicated, and retain a configured but absent target as `not running`. Store the process target separately from window-oriented matching data and lazily derive it for existing process-mode sources.

Keep the routing model authoritative during drag and drop. A completed custom move must not trigger Qt's second model-row removal, and every Track 1-6 and Muted Sources Group row must remain visible even when empty.

Add a user-controlled Elevated Launch preference under Advanced > General. It is enabled in shipped defaults and requests administrator privileges before the main application loads. Users can disable it, changes take effect on the next launch, and refusing UAC silently continues the original non-elevated launch.

## User Stories

1. As a Vivaldi user, I want `进程名匹配` to keep capturing after the active tab title changes, so that background music does not disappear from my recording.
2. As a browser user, I want capture to follow the executable rather than one window title, so that navigating to another page does not require editing the source.
3. As a browser user, I want the browser audio-service process to be discovered even when it is not the selected window PID, so that a successfully initialized source is not silently empty.
4. As a user, I want all independent instances with the configured executable name to be captured, so that executable-wide matching behaves literally.
5. As a user with two Vivaldi instances, I want audio from both instances in the same Application Audio Source, so that neither is omitted arbitrarily.
6. As a user, I want parent and child audio sessions from the same application tree to be mixed only once, so that the result is not louder or distorted.
7. As a user, I want an application started after OBS-PT to join capture automatically, so that startup order does not matter.
8. As a user, I want a restarted application to rejoin capture automatically, so that I do not need to reopen source properties.
9. As a user, I want one closing same-name instance to leave capture without interrupting the others, so that independent instances remain reliable.
10. As a user, I want a source with no current matching session to remain configured and waiting, so that temporary absence does not erase its target.
11. As a user, I want an application without a visible window to remain capturable when it owns a matching audio session, so that background audio helpers are supported.
12. As a user, I want actual silence to remain distinct from source eligibility, so that capture membership is not inferred from momentary volume.
13. As a privacy-conscious user, I want sessions whose executable identity cannot be verified to be excluded, so that OBS-PT never guesses and captures unrelated audio.
14. As a user with several matching sessions, I want inaccessible sessions to be skipped without stopping accessible instances, so that partial permission differences do not disable the whole source.
15. As a user, I want the routing panel to expose an insufficient-permission state for an affected source, so that I can diagnose why one application is absent.
16. As a support technician, I want permission failures and selected executable identities in the log, so that a silent source can be diagnosed from evidence.
17. As a user editing a process-mode source, I want the selector label to say `Process`, so that its scope is not confused with a particular window.
18. As a user editing a process-mode source, I want to see stable executable names such as `vivaldi.exe`, so that old page titles do not look selected.
19. As a user, I want duplicate executable candidates collapsed into one entry, so that Chromium's process count does not make the selector unusable.
20. As a user, I want candidate processes to include visible applications and applications with audio sessions, so that both normal and windowless audio applications can be selected.
21. As a user, I want a configured executable that is not running to remain selected and display `not running`, so that the source is ready for a later launch.
22. As a user, I want the process list to update while properties are refreshed, so that newly available applications can be selected without restarting OBS-PT.
23. As a user switching matching modes, I want the process target and window target stored independently, so that switching modes does not destroy either choice.
24. As an existing internal-test user, I want a process-mode source with only a legacy window tuple to derive its executable automatically, so that my current source keeps working.
25. As a user, I want executable comparison to ignore case but otherwise be exact, so that similarly named programs are not captured accidentally.
26. As a browser user, I accept that process-name mode captures the complete browser executable rather than one tab, so that the scope is predictable.
27. As a user dragging the only Track 2 source to another Editor Track, I want Track 2 to remain visible, so that I know it is empty rather than deleted.
28. As a user moving a source between any tracks, I want all Track 1-6 and M rows to remain visible, so that the routing table has a stable structure.
29. As a user, I want one drag to cause one routing mutation, so that presentation code cannot remove a second row after the model updates.
30. As a user, I want a moved source to retain its Source Identity and properties, so that drag and drop changes only its route and position.
31. As a user, I want drag and context-menu moves to produce the same persistent result, so that both interaction paths remain equivalent.
32. As a user recording or streaming, I want existing topology-lock rules to remain enforced during drag, so that this UI fix does not allow unsafe route changes.
33. As a keyboard or screen-reader user, I want empty track rows and process states to retain accessible labels, so that the repaired UI remains operable without a mouse.
34. As a new OBS-PT user, I want administrator launch enabled by default, so that elevated games and audio sessions are less likely to be inaccessible.
35. As a user who does not want UAC on every start, I want to disable administrator launch in Advanced > General, so that elevation remains optional.
36. As a user changing the administrator preference, I want it to apply next launch, so that the current recording session is never interrupted.
37. As a user accepting UAC, I want OBS-PT to restart with the same command-line arguments and working directory, so that profiles, scenes, startup actions, and portable paths are preserved.
38. As a user refusing UAC, I want OBS-PT to continue normally without another warning, so that declining elevation does not block use.
39. As a portable user, I want the EXE manifest to remain non-forcing, so that the Global Config preference remains authoritative.
40. As a user running the installer, I want installer elevation policy to remain independent of application elevation, so that installing and launching are not conflated.
41. As an OBS-PT user, I want the application to detect that it is already elevated, so that startup cannot enter a relaunch loop.
42. As a user of any supported UI language, I want process-mode, permission, and administrator-launch text localized consistently, so that behavior is understandable.
43. As a maintainer, I want a real-output regression proving stale-title executable matching contains audible data, so that initialization logs alone cannot produce a false pass.
44. As a maintainer, I want a real-output regression with multiple same-name roots, so that executable-wide scope is verified by distinct tones.
45. As a maintainer, I want a real-output regression proving parent/child session de-duplication, so that double gain cannot return unnoticed.
46. As a maintainer, I want late-start, exit, and restart regressions, so that dynamic reconciliation is verified over time.
47. As a maintainer, I want a UI automation regression that performs the failing drag, so that Track 2 cannot disappear again.
48. As a maintainer, I want UI automation to verify stable process display and absent-target persistence, so that stale titles cannot return.
49. As a maintainer, I want deterministic model tests for session reconciliation and elevated-launch decisions, so that OS timing and secure-desktop UAC do not make the suite flaky.
50. As a release tester, I want one manual Vivaldi restart check and one UAC accept/decline check, so that the real third-party and Windows privilege boundaries are covered.
51. As a release engineer, I want the RelWithDebInfo build, staging tree, and installer refreshed after the change, so that the tested executable and plugins are the packaged ones.
52. As a PotPvP user, I want the 480 FPS, resolution, quality, Hybrid MP4, and existing routing topology contracts unchanged, so that this reliability fix does not reduce recording performance or quality.

## Implementation Decisions

- Process-name mode uses the executable base file name as a case-insensitive exact identity. It does not use wildcards or a full-path identity.
- A shared Windows audio-session observer discovers non-expired sessions and their owning executable names. Application Audio Sources subscribe to changes instead of polling one selected window.
- Each process-mode source reconciles a desired set of matching session PIDs with its active Process Loopback clients.
- Parent relationships are resolved before capture. Matching PIDs already covered by another matching ancestor are omitted; unrelated root processes remain separate captures.
- Process Loopback continues to include the target process tree for every retained root.
- Several loopback clients owned by one Source feed one output path without duplicate gain. Client addition or removal must not recreate the Source Identity or its Audio Routing Configuration entry.
- An unreadable executable identity is never treated as a match. The source and log surface insufficient permission while verified clients continue.
- Process-mode UI uses a de-duplicated executable selector. Its candidates are the union of visible application executables and observed audio-session executables.
- A saved executable absent from current candidates remains selected with a localized `not running` suffix.
- Process target persistence is independent from the legacy serialized window tuple. Existing process-mode settings derive the executable from the tuple when the new value is absent.
- Title, class, and window-executable match modes retain the existing HWND-based behavior. Game Capture and Window Capture still require a concrete window and are not converted to audio-session aggregation by this PRD.
- The Application Audio Source remains collection-owned, outside every Scene Item, and continues to enter output through the existing audio-mix-root and Audio Routing Configuration contracts.
- Existing Compatibility Mix, Editor Track, Muted Sources Group, topology locking, streaming, Replay Buffer, and Hybrid MP4 behavior remain unchanged.
- The routing table's custom drag handler is the sole authority for a completed route move. It must prevent Qt's internal move path from removing a model row after the table has been rebuilt.
- Empty Track 1-6 and M groups remain real selectable/drop-target rows and preserve their accessible track labels.
- Elevated Launch is a Global Config boolean exposed in Advanced > General and enabled in shipped defaults.
- Elevation is requested before the main UI and capture subsystems load. A successful relaunch preserves the complete argument vector and working directory and exits the original process.
- The application detects an already elevated token and never relaunches again.
- Refusing or failing the UAC request continues the original non-elevated startup silently.
- Changing Elevated Launch never restarts the current process and takes effect on the next application start.
- The application manifest remains `asInvoker`; the installer retains its independent user-level execution contract.
- New UI and state text is complete for en-US, zh-CN, zh-TW, ja-JP, and ko-KR.
- Changes affecting the executable, win-wasapi plugin, shared process/session support, shipped Global Config, or UI resources require a complete build, refreshed staging, and a new verified installer.

## Testing Decisions

- The primary seam is the existing real Windows Application Audio smoke test. It launches the actual OBS-PT binary hidden, creates deterministic audio applications, records through the actual output stack, and verifies Hybrid MP4 streams with FFprobe and frequency analysis.
- The primary seam must prove audible output, not merely count successful Process Loopback initialization messages.
- Extend deterministic tone fixtures to represent two unrelated same-name roots and a browser-style parent/child audio topology. Distinct frequencies and measured magnitudes verify inclusion and de-duplication externally.
- Reuse existing late-start, restart, process-tree, and real-output scenarios where possible instead of creating parallel harnesses.
- Extend the existing Windows UI Automation smoke to execute a real drag from a sole-source track and assert that Track 1-6 and M remain present immediately and after restart.
- The UI Automation smoke also verifies that process mode shows stable executable names, collapses duplicates, preserves a missing target, and exposes localized accessible state.
- Use a narrow deterministic model test for desired-session reconciliation, parent/child de-duplication, inaccessible identity handling, and idempotent add/remove behavior.
- Use a narrow startup-decision model test for disabled, enabled/non-elevated, already elevated, elevation accepted, and elevation refused outcomes. Do not attempt to automate the Windows secure desktop.
- Manually verify one Vivaldi scenario: save a target, change the active title or restart Vivaldi, start OBS-PT without opening properties, and confirm the Music meter and recorded track contain sound.
- Manually verify Elevated Launch once with UAC accepted and once refused. Acceptance preserves arguments and portable root; refusal reaches the ordinary non-elevated UI without a warning.
- Run UI smoke in en-US, zh-CN, zh-TW, ja-JP, and ko-KR.
- Run the complete existing audio routing model, update model, player compatibility, real-output, and packaging checks to catch shared-topology regressions.
- Keep OBS-PT out of the foreground during automated runtime tests, reject unexpected existing instances by default, and clean up every tone/helper process.
- Final qualification keeps the product's 480 FPS and quality defaults. No test may lower production defaults to hide performance or capture defects.
- Refresh staging with tests disabled, verify no test executable enters staging, build one installer process, run archive integrity and hash checks, and launch the Setup window.

## Out of Scope

- Making Compatibility Mix optional or disabling it by default.
- Adding the single-track/multi-track quick switch to Audio and Routing.
- Changing the default output so only Desktop Audio is recorded while Editor Track assignments remain dormant.
- Redesigning the complete shipped Scene Collection, Dock layout, personal paths, hotkeys, or current machine-specific default sources.
- Capturing one browser tab separately from other tabs in the same executable.
- Converting Game Capture or Window Capture process-name matching into audio-session aggregation; video capture still requires an HWND.
- Changing existing Compatibility Mix authority, Editor Track numbering, track metadata, muted behavior, output topology, streaming, VOD, Replay Buffer, or Hybrid MP4 contracts.
- Lowering 480 FPS, resolution, quality, or encoder requirements.
- Publishing or replacing a GitHub Release.

## Further Notes

- The current deterministic process-name smoke already passes with a deliberately stale stored title and proves the generic single-process path can record audible `Mix` and `Game` tracks. The reported Vivaldi failure is therefore a multi-process/session-discovery gap, not a general string-comparison failure.
- Current upstream OBS Application Audio Capture still starts Process Loopback from one matched window PID. The public `bozbez/win-capture-audio` project demonstrates the stronger pattern of observing audio sessions, selecting PIDs by executable, and de-duplicating process trees. Its GPL-2.0 code is implementation evidence; local adaptation must respect license obligations and should reuse the pattern rather than copy unnecessary plugin architecture.
- ADR 0002 records the executable-wide privacy boundary, rejected single-window design, process-tree de-duplication, fail-closed permission policy, and optional Elevated Launch decision.
- The future optional Mix and single/multi-track design contradicts parts of the current routing contract and must receive its own grill and documentation update rather than being partially introduced here.
