# Git hooks (this repo)

Hooks live here so they can be versioned. Git only runs them if you point `core.hooksPath` at this directory.

## One-time setup (per clone)

From the repository root:

```sh
git config core.hooksPath .githooks
```

On Windows (PowerShell), you can run:

```powershell
.\scripts\install-githooks.ps1
```

## `post-commit`

After each **local** commit, if the current branch name is **`pub`** (override with env `PUBLISH_BRANCH`), the hook runs **`./publish.sh`** from the repo root (override with env `PUBLISH_SCRIPT`).

Examples:

```sh
PUBLISH_BRANCH=release PUBLISH_SCRIPT=./scripts/ship.sh git commit -m "…"
```

## Notes

- This runs only on **commit**, not on **push**. To run something when GitHub receives a push, use GitHub Actions on `pub` instead.
- Keep `publish.sh` fast; commits block until the hook exits.
- Use `exit 0` in `publish.sh` unless you want to report hook failure (Git shows a warning if the hook exits non-zero).
