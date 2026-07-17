Status: done

## What to build

Deliver one complete GitHub Issue intake path for OBS-PT. Contributors must
choose one of four bilingual forms for a bug, enhancement, usage help request,
or open discussion. Each form must prefill its matching lowercase title prefix,
apply exactly one category label plus the initial `triage` state, collect the
agreed category-specific information, and prevent ordinary contributors from
bypassing the forms with a blank Issue.

Establish the matching eight-label repository taxonomy. Category labels are
`bug`, `enhancement`, `help`, and `discussion`; maintainer-managed state labels
are `triage`, `wip`, `duplicate`, and `wontfix`. Remove the unused GitHub
defaults so `help` cannot be confused with `help wanted` and `discussion`
cannot overlap `question`. Labels use the agreed red, cyan, blue, purple,
yellow, deep-blue, light-gray, and dark-gray visual roles and concise bilingual
descriptions.

All four forms require separate OBS-PT version, operating system, CPU, GPU and
driver, memory, and recording-disk fields. Bug reports additionally require
actual behavior, reproduction steps, expected behavior, and a response for
each diagnostic artifact: an OBS Statistics screenshot, a Task Manager CPU/GPU
screenshot, and the original log from the same run. Each artifact response may
instead explain why the artifact is unavailable or inapplicable; logs are
located under `<Install Root>/obs-studio/logs/` and remain encouraged rather
than unconditionally required.

Enhancement requests collect the current limitation, expected improvement,
PotPvP recording use case, optional workaround, and optional supporting
material. Help requests collect the question, desired result, attempted
solutions, and optional configuration or diagnostics. Discussions collect the
topic, OBS-PT or PotPvP background, current position, desired conclusion, and
optional alternatives or references.

## Acceptance criteria

- [x] The template chooser presents exactly four ordered Chinese/English forms:
  bug, enhancement, help, and discussion; ordinary contributors cannot choose
  a blank Issue.
- [x] Each form prefills `[bug] `, `[enhancement] `, `[help] `, or
  `[discussion] ` and applies its matching category label plus `triage` at
  creation time, without continuous title-rewriting automation.
- [x] Every form exposes six separate required system-information fields for
  OBS-PT version, operating system, CPU, GPU and driver, memory, and recording
  disk capacity and SSD/HDD type.
- [x] The bug form separately requires actual behavior, reproduction steps,
  expected behavior, and a non-empty screenshot-or-reason response for both
  OBS Statistics and Task Manager CPU/GPU usage.
- [x] The bug form requires either an original same-run log attachment or a
  reason it cannot be provided, and describes both the portable log path and
  the Help-menu discovery route.
- [x] The enhancement, help, and discussion forms contain their agreed required
  category-specific fields while keeping screenshots, logs, workarounds,
  alternatives, and supporting material optional where specified.
- [x] Every form requires confirmation that the reporter searched for
  duplicates, is reporting an OBS-PT matter, supplied accurate information,
  and checked public attachments for sensitive data; the bug form also asks
  the reporter to reproduce on the current version where possible.
- [x] The live repository contains exactly `bug`, `enhancement`, `help`,
  `discussion`, `triage`, `wip`, `duplicate`, and `wontfix`, with the agreed
  semantic colors and bilingual descriptions; `documentation`,
  `good first issue`, `help wanted`, `invalid`, and `question` are absent.
- [x] The documented status semantics are `triage` for pending review, no state
  label for accepted backlog work, `wip` for active work, and a closed Issue
  for completion; `duplicate` and `wontfix` are mutually exclusive terminal
  states managed by collaborators with Triage permission or higher.
- [x] No assignee, Project, Milestone, external contact, private vulnerability
  reporting, security policy, or GitHub Action is added as part of this slice.
- [x] Automated contract checks validate the form schema and agreed intake
  behavior, and live GitHub inspection confirms the published chooser and
  repository label state.

## Verification

- Published commits: `69c498c` and `5508838` on `origin/master`.
- `python -X utf8 test/github/test_issue_intake.py -v` passes all local checks.
- With `OBS_PT_VERIFY_REMOTE=1`, all ten local and live GitHub checks pass.
- The live Labels API returns exactly the eight agreed labels, colors, and
  bilingual descriptions.

## Blocked by

None - can start immediately
