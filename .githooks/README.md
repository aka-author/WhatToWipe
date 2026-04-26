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

## Notes

- This repository intentionally does not use a `post-commit` hook for build/publish.
- If you need publish automation, use an explicit script/command or CI workflow.
