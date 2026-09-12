---
status: accepted
---

# Executable-Wide Application Audio Capture

`进程名匹配` treats the executable file name as the stable target and captures every matching application instance, independent of window title, class, lifetime, or startup order. OBS-PT discovers matching Windows audio sessions dynamically, de-duplicates parent/child sessions so one process tree is mixed once, and captures unrelated same-name roots separately. A missing target remains pending; new and expired sessions join or leave without reopening properties or restarting OBS-PT. Sessions whose executable identity cannot be verified fail closed and report insufficient permission while other verified instances continue.

The process-mode property selector lists de-duplicated executable names from visible applications and audio sessions, preserves an absent saved target as `not running`, and stores the executable separately from window-oriented matching data. Existing process-mode sources may derive that field once from their legacy window value. Title, class, and window executable priorities keep their existing single-window behavior.

## Considered Options

- Use the first window whose executable matches: rejected because a successful Process Loopback initialization can bind the wrong Chromium process tree and produce silence when the browser title or audio-service process changes.
- Enumerate all operating-system processes with the same name: rejected because process presence does not identify the processes that own audio sessions and would add noisy system candidates.
- Capture one arbitrary same-name instance: rejected because it contradicts executable-wide matching and remains unreliable for multi-process browsers.
- Require a virtual audio cable: rejected as mandatory user setup for a workflow OBS-PT should provide directly.
- Mark the EXE manifest `requireAdministrator`: rejected because users must be able to disable elevated startup.

## Elevated Launch

To reduce inaccessible game and audio sessions, `设置 > 高级 > 常规 > 以管理员身份运行 OBS-PT` is a Global Config preference enabled in shipped defaults. A normal launch requests elevation and restarts itself with its original arguments and working directory; the preference applies on the next launch. Refusing UAC silently continues the original non-elevated process. The EXE and installer keep their independent non-forced privilege policies.

## Consequences

One Application Audio Source may own several Process Loopback clients and must mix them without duplicate gain or discontinuity as sessions change. Executable-wide matching intentionally includes every independent instance with the same file name, so the UI must state that scope clearly. Regression coverage must include stale titles, browser-style audio child processes, multiple roots, parent/child de-duplication, late start, restart, permission failure, and process-selector persistence.
