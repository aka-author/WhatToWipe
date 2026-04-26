# Windows exe build and installer workflow

This document describes how the WhatToWipe Windows binary is produced, how the Inno Setup installer is produced, and the **arguments / interfaces** of each script involved.

## Repository layout (paths the scripts assume)

- **`codebase/`** — Git repository root (contains `win-go/`, `scripts/`, `installer/`, `docs/`).
- **`win-go/`** — Go module; `build.ps1` lives here.
- **Project root** — parent of `codebase/` (the script calls it `$ShitwiperRoot`; equivalent to `codebase\..`). Build output goes under **`<ProjectRoot>\bin\win\`**, not under `codebase\` only.

So with a clone at `<ProjectRoot>\codebase\`, the exe lands in **`<ProjectRoot>\bin\win\current\`**.

---

## 1. Exe build: `win-go/build.ps1`

### Flow direction (important)

- This workflow is **build-driven**: you run the build script, and that build process may create commits.
- It is **not commit-driven**: normal commits must not be treated as a trigger to start builds.
- In short: **build => commit(s)**, not **commit => build**.

### How you run it

```powershell
# From any working directory; paths are derived from the script location.
powershell -NoProfile -ExecutionPolicy Bypass -File "path\to\codebase\win-go\build.ps1"
```

Or `cd` into `win-go` and run `.\build.ps1`.

### Parameters / CLI

**None.** The script does not accept command-line arguments. It always uses:

| Symbol | Meaning |
|--------|---------|
| `$ModuleRoot` | Directory containing `build.ps1` (`win-go/`) |
| `$CodebaseRoot` | Parent of `$ModuleRoot` (`codebase/`) |
| `$ShitwiperRoot` | Parent of `$CodebaseRoot` |
| `$WinBinRoot` | `<ProjectRoot>\bin\win` |
| `$OutDir` | `{WinBinRoot}\current` |
| `$Exe` | `{OutDir}\WhatToWipe.exe` |

### High-level steps (in order)

1. Walk parents of `win-go/` to find **`.git`** (`$GitRoot`). If missing, git commits and real branch/commit in history are skipped (warning only).
2. **Bump build** in `win-go/versioninfo.json`: increment numeric `FixedFileInfo.*.Build` only; string `FileVersion` / `ProductVersion` use **four uppercase hex digits** for the fourth segment (e.g. `0.1.0.000A`). Fails if build would exceed `0xFFFF`.
3. Copy or generate **About** PNG into `win-go/assets/art/`; run **`go run ./tools/genicons`**; run **`go generate .`** (includes Windows resource / `goversioninfo`).
4. If `$GitRoot` exists: **`git add -A`** then **`git commit -m "build: version {productVer}"`** (full-repo snapshot; throws if nothing staged).
5. **`Prepare-CurrentBuildFolder`**: if `bin\win\current` is non-empty: if a **`*.date`** marker exists and `{WinBinRoot}\{markerStem}` does not exist yet, move all items into that archive folder; otherwise clear `current` (details in script comments).
6. **`go build`** → `WhatToWipe.exe` under `current` (`CGO_ENABLED=0`, `-H windowsgui`, `-trimpath`, `-buildvcs=false`).
7. Optional **Authenticode** if `WHATTOWIPE_SIGNTOOL` is set (see below).
8. Copy `versioninfo.json` beside the exe; write **`.date`** marker (name + body), **`build-meta.json`**, append **`docs/history/builds.txt`**.
9. If `$GitRoot` exists: **`git add -A`** and **`git commit -m "docs: append build {productVer} to history"`**.

### Environment variables (exe build only)

| Variable | Purpose |
|----------|---------|
| `WHATTOWIPE_SIGNTOOL` | If set, full path to **`signtool.exe`**; after `go build`, the script runs signing on `WhatToWipe.exe`. |
| `WHATTOWIPE_SIGN_ARGS` | Optional; split on whitespace and passed to `signtool` between `sign` and the exe path. |

---

## 2. Installer build: `scripts/build-installer.bat`

Run **after** a successful exe build so `SourceDir` contains `WhatToWipe.exe`.

### Arguments (required)

```bat
scripts\build-installer.bat <SourceDir> <OutputRootDir>
```

| Arg | Position | Description |
|-----|----------|-------------|
| `SourceDir` | `%1` | Folder that **must** contain **`WhatToWipe.exe`** (typically `<ProjectRoot>\bin\win\current`). Expanded with `%~f1`. |
| `OutputRootDir` | `%2` | Root directory under which the batch creates a **timestamped** subfolder and passes that to Inno as output. Expanded with `%~f2`. |

Default project convention: set `OutputRootDir` to **`<ProjectRoot>\delivery\win`** unless explicitly overridden.

The batch resolves **`installer\WhatToWipe.iss`** as `"%SCRIPT_DIR%..\installer\WhatToWipe.iss"` (i.e. relative to `scripts/`, inside **codebase**).

### Behavior

1. Validates `SourceDir` and `WhatToWipe.exe`; creates `OutputRootDir` if missing.
2. Sets `STAMP` to `yyyy-MM-dd_HH-mm` (via PowerShell), then `OUTPUT_DIR = OutputRootDir\STAMP`.
3. Locates **Inno Setup 6** `ISCC.exe`: `%LOCALAPPDATA%\Programs\Inno Setup 6\`, `ProgramFiles(x86)`, `ProgramFiles`, then **`PATH`**.
4. Invokes:

   ```text
   ISCC.exe "/DSourceDir=<SourceDir>" "/DOutputDir=<OUTPUT_DIR>" "<path\to>\installer\WhatToWipe.iss"
   ```

### Exit codes (batch)

| Code | Meaning |
|------|---------|
| 0 | Success |
| 2 | Missing `SourceDir` or `OutputRootDir` |
| 3 | `SourceDir` does not exist |
| 4 | `WhatToWipe.exe` missing under `SourceDir` |
| 5–8 | mkdir / stamp failures |
| 6 | `ISCC.exe` not found |
| 9 | Inno script path not found |
| Other | Inno compiler return code |

---

## 3. Inno script: `installer/WhatToWipe.iss`

This file is **not** run directly; **`ISCC.exe`** compiles it. The batch always passes the defines below.

### Preprocessor defines (compiler `/D…`)

| Define | Required | Default if omitted | Meaning |
|--------|----------|--------------------|---------|
| `SourceDir` | **Yes** (script `#error`s if missing) | — | Directory tree packaged into `{app}`; must contain `WhatToWipe.exe`. |
| `OutputDir` | No | Falls back to `SourceDir` in the `.iss` | Where the generated **setup .exe** is written. The batch always sets it to `OutputRootDir\yyyy-MM-dd_HH-mm`. |

### Version string

`ProductVersion` is read at compile time from **`GetStringFileInfo(SourceDir + "\WhatToWipe.exe", "ProductVersion")`**. If empty, it falls back to `0.0.0.0`. **`OutputBaseFilename`** becomes `WhatToWipe-Setup-{#ProductVersion}`.

### Packaging rules (summary)

- **`[Files]`**: `Source: "{#SourceDir}\*"` into `{app}`, recursive; **`Excludes: "*.date"`** so marker files are not shipped.
- **EULA page**: `[Setup]` uses `LicenseFile=EULA.txt`, so the installer shows the license acceptance step.

---

## 4. End-to-end example (typical machine)

After adjusting paths to your project root:

```powershell
Set-Location ...\codebase\win-go
.\build.ps1
```

```bat
<ProjectRoot>\codebase\scripts\build-installer.bat "<ProjectRoot>\bin\win\current" "<ProjectRoot>\delivery\win"
```

The installer executable appears under **`<ProjectRoot>\delivery\win\<yyyy-MM-dd_HH-mm>\`**.

---

## 5. Related script (not part of exe/installer pipeline)

**`scripts/install-githooks.ps1`** — no arguments; walks up to `.git`, runs **`git config core.hooksPath .githooks`**. Unrelated to compiling the exe or the installer.

Git hooks (including `post-commit`) are not the source of truth for this build flow and must not redefine it into commit-triggered builds.
