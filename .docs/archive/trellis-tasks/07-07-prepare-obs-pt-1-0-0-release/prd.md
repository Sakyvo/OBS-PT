# Prepare OBS-PT 1.0.0 release

## Goal

Prepare and publish OBS-PT 1.0.0 on GitHub:

- Replace the inherited upstream-style README with an OBS-PT README based on
  the official OBS README structure but focused on OBS-PT's product identity.
- Push a cleaned source tree to `https://github.com/Sakyvo/OBS-PT`.
- Publish a GitHub Release for version 1.0.0 with the installer and
  `Sourcecode.zip`.

## Requirements

### README

- Use `C:/Users/ASUS/Desktop/README.rst` as the structural reference.
- README must describe OBS-PT as "OBS For PotPvP".
- README must highlight core features:
  - PotPvP-focused defaults for Minecraft Java 1.7.10 / 1.8.9 capture.
  - High-FPS recording defaults for frame blending workflows.
  - Pre-optimized recording settings instead of requiring users to tune OBS
    from scratch.
  - Portable install model where user data lives under the install root.
  - Hybrid MP4 default for damage-resistant normal recordings.
- README must keep GPLv2-or-later licensing attribution via `COPYING`.

### Source Push

- Remote: `origin https://github.com/Sakyvo/OBS-PT`.
- Local branch: `master`.
- Remote default branch reported by `git remote show origin`: `master`.
- Push the release source to remote `master`.
- Include the OBS-PT source and required project files.
- Exclude development/session directories and generated artifacts, including at
  least `.agents/`, `.claude/`, `.codex/`, `.trellis/`, `docs/`, `build-v143/`,
  `build64/`, build outputs, temporary archives, and installer staging output.
- Do not commit unrelated local machine state or generated release artifacts
  into the source branch.

### Release Assets

- Version: `1.0.0`.
- Tag target: release source commit on `master`.
- Release title should identify OBS-PT 1.0.0.
- Attach:
  - `build-v143/_pkg/OBS-PT-1.0.0-Installer.exe`
  - `Sourcecode.zip`
- `Sourcecode.zip` must be generated from the cleaned source selection, not from
  the full working directory with development folders and build outputs.

### Confirmed Facts

- Current installer exists:
  `build-v143/_pkg/OBS-PT-1.0.0-Installer.exe`.
- Existing installer timestamp observed during planning:
  `2026-07-06 12:48:35`.
- `gh` CLI is installed at `C:/Program Files (x86)/GitHub CLI/gh.exe`.
- `gh auth status` reports login as GitHub account `Sakyvo` with `repo` scope.
- Existing Release list for `Sakyvo/OBS-PT` is empty.
- Remote has both `main` and `master`; remote HEAD/default is `master`.

## Acceptance Criteria

- [ ] `README.rst` is rewritten for OBS-PT and renders as valid RST.
- [ ] Source inclusion/exclusion is explicitly checked before commit.
- [ ] Development folders and build outputs are ignored or otherwise excluded
      from commit and `Sourcecode.zip`.
- [ ] Release source commit is pushed to `origin/master`.
- [ ] `Sourcecode.zip` exists and does not contain excluded development folders
      or build output.
- [ ] GitHub Release `1.0.0` exists with installer and `Sourcecode.zip`
      uploaded.
- [ ] Installer and source archive hashes are recorded in the final report.

## Out Of Scope

- Rebuilding feature code unless release validation discovers a blocking issue.
- Publishing to package managers or websites outside GitHub Releases.
- Migrating repository default branch from `master` to `main`.

## Open Questions

- None.
