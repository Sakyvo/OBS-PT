# Design: OBS-PT 1.0.0 Release

## Boundaries

- Repository source is prepared in the current working tree on `master`.
- Release assets are generated under `build-v143/_pkg/`.
- GitHub remote is `origin` at `https://github.com/Sakyvo/OBS-PT`.
- Development workflow directories are local-only and must not be pushed or
  included in `Sourcecode.zip`.

## README Shape

Keep the official OBS README structure:

- Title and short project summary.
- "What is OBS-PT?" section.
- Quick links.
- License and upstream attribution.

Adapt the content to OBS-PT:

- Position as an independent OBS fork for PotPvP recording.
- Emphasize 480 FPS high-FPS capture defaults and frame blending workflows.
- Mention Hybrid MP4 normal recording default.
- Mention portable install/User Data Root behavior.
- Keep wording concise and suitable for a public GitHub repository.

## Source Packaging

Use Git as the source of truth after staging the intended release files:

1. Ensure `.gitignore` excludes local development directories and generated
   build/release outputs.
2. Stage only intended source files and project metadata.
3. Commit release source.
4. Generate `Sourcecode.zip` from the release commit with `git archive`, so the
   archive matches the pushed source and does not include untracked local files.

## Release Publication

Preferred path:

- Use `C:/Program Files (x86)/GitHub CLI/gh.exe`; it is installed and
  authenticated as `Sakyvo`.

Fallback path:

- Use GitHub REST API if a token is provided via `GH_TOKEN` or `GITHUB_TOKEN`.

Blocked path:

- If GitHub CLI auth expires or upload fails, prepare the commit and assets and
  stop before remote Release creation with exact manual upload instructions.

## Compatibility Notes

- Remote default branch is `master`; do not switch to `main`.
- Existing remote `main` is unrelated for this release unless the user changes
  direction.
- GitHub auto-generated source archives are not a replacement for the requested
  `Sourcecode.zip`; create and upload the explicit archive.

## Rollback

- Before pushing, inspect `git diff --cached --stat` and staged file list.
- If staged files include excluded directories or build output, unstage and
  correct `.gitignore` before commit.
- If release upload fails after push, leave the pushed source intact and retry
  only the Release API/upload step.
