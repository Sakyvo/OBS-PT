# Implement: OBS-PT 1.0.0 Release

## Checklist

1. Refresh release context:
   - Read applicable project/spec context.
   - Inspect current git status and remote status.
2. README:
   - Rewrite `README.rst` for OBS-PT using the official README reference.
   - Validate RST syntax if a local tool is available; otherwise run a focused
     text review.
3. Source hygiene:
   - Update `.gitignore` for local development folders and generated outputs.
   - Inspect ignored/untracked files.
   - Stage intended source files only.
   - Verify no `.agents/`, `.claude/`, `.codex/`, `.trellis/`, `docs/`,
     build output, installer staging, archives, logs, or local IDE state are
     staged.
4. Release validation:
   - Verify installer exists.
   - Run `7z t build-v143/_pkg/OBS-PT-1.0.0-Installer.exe`.
   - Compute installer SHA256.
5. Commit and push:
   - Commit release source on `master`.
   - Push to `origin/master`.
6. Source archive:
   - Generate `build-v143/_pkg/Sourcecode.zip` from the pushed release commit
     with `git archive`.
   - Inspect archive contents for excluded directories.
   - Compute SHA256.
7. GitHub Release:
   - Create/update release `1.0.0`.
   - Upload installer and `Sourcecode.zip`.
   - Verify assets are present.

## Validation Commands

```powershell
git status --short
git diff --cached --stat
git diff --cached --name-only
7z t build-v143/_pkg/OBS-PT-1.0.0-Installer.exe
Get-FileHash build-v143/_pkg/OBS-PT-1.0.0-Installer.exe -Algorithm SHA256
7z l build-v143/_pkg/Sourcecode.zip
Get-FileHash build-v143/_pkg/Sourcecode.zip -Algorithm SHA256
git ls-remote --heads origin
```

## Risk Points

- The repository has many currently untracked source files, so staging must be
  deliberate.
- `gh` CLI is missing and no token env var is available; Release upload may
  require user-provided authentication or manual publishing.
- `.gitignore` currently ignores `*.zip`; this is fine for release assets but
  means `Sourcecode.zip` should not be committed.
