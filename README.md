# WhatToWipe / Shitwiper codebase

Git repo = **`codebase`** (specs + stubs only). Full rules: [docs/folder-structure.md](docs/folder-structure.md).

The Windows Go **module** lives next to this repo: **`../samples/`** at the **project root**.

Open [Shitwiper.code-workspace](Shitwiper.code-workspace) for **`codebase`** + **`samples`**. Build: [`../samples/build.bat`](../samples/build.bat) writes **`DiskTreemap.exe`** next to the module (gitignored there); **no `bin/` output** for now.

- [docs/specs/funcspec.md](docs/specs/funcspec.md)
- [docs/specs/techspec-win-go.md](docs/specs/techspec-win-go.md), [docs/specs/arch-win-go.md](docs/specs/arch-win-go.md)
- [win-go/](win-go/)
