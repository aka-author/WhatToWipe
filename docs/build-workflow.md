# Windows exe build and installer workflow

This document describes how the Erase & Rewrite Windows binary is produced, how the Inno Setup installer is produced, and the **arguments / interfaces** of each script involved.

## Repository layout (paths the scripts assume)

- **`codebase/`** РІР‚вЂќ Git repository root (contains `win-go/`, `scripts/`, `installer/`, `docs/`).
- **`win-go/`** — Go module; legacy `build.ps1` lives here.
- **`win-cpp-qt/`** — C++ Qt module; **shipping** `build.ps1 -StaticQt` lives here.
- **Project root** РІР‚вЂќ parent of `codebase/` (the script calls it `$ShitwiperRoot`; equivalent to `codebase\..`). Build output goes under **`<ProjectRoot>\bin\win\`**, not under `codebase\` only.
- **`<ProjectRoot>\tools\`** РІР‚вЂќ location for helper tool executables. Tool `.exe` files must not stay in `win-go\`.

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
| `$Exe` | `{OutDir}\Erase & Rewrite.exe` |

### High-level steps (in order)

1. Walk parents of `win-go/` to find **`.git`** (`$GitRoot`). If missing, git commits and real branch/commit in history are skipped (warning only).
2. **Bump build** in `win-go/versioninfo.json`: increment numeric `FixedFileInfo.*.Build` only; string `FileVersion` / `ProductVersion` use **four uppercase hex digits** for the fourth segment (e.g. `0.1.0.000A`). Fails if build would exceed `0xFFFF`.
3. Copy or generate **About** PNG into `win-go/assets/art/`; run **`go run ./tools/genicons`**; run **`go generate .`** (includes Windows resource / `goversioninfo`).
4. Move any root-level `win-go\*.exe` helper artifacts to **`<ProjectRoot>\tools\`** (enforced by the build script).
5. If `$GitRoot` exists: **`git add -A`** then **`git commit -m "build: version {productVer}"`** (full-repo snapshot; throws if nothing staged).
6. **`Prepare-CurrentBuildFolder`**: if `bin\win\current` is non-empty: if a **`*.date`** marker exists and `{WinBinRoot}\{markerStem}` does not exist yet, move all items into that archive folder; otherwise clear `current` (details in script comments).
7. **`go build`** РІвЂ вЂ™ `Erase & Rewrite.exe` under `current` (`CGO_ENABLED=0`, `-H windowsgui`, `-trimpath`, `-buildvcs=false`).
8. Optional **Authenticode** if `ERASE_REWRITE_SIGNTOOL` is set (see below).
9. Copy `versioninfo.json` beside the exe; write **`.date`** marker (name + body), **`build-meta.json`**, append **`docs/history/builds.txt`**.
10. If `$GitRoot` exists: **`git add -A`** and **`git commit -m "docs: append build {productVer} to history"`**.

### Environment variables (exe build only)

| Variable | Purpose |
|----------|---------|
| `ERASE_REWRITE_SIGNTOOL` | If set, full path to **`signtool.exe`**; after `go build`, the script runs signing on `Erase & Rewrite.exe`. |
| `ERASE_REWRITE_SIGN_ARGS` | Optional; split on whitespace and passed to `signtool` between `sign` and the exe path. |

---

## 1b. Exe build: `win-cpp-qt/build.ps1` (shipping static Qt)

The active Windows C++ Qt implementation uses **`win-cpp-qt/build.ps1`**. Shipping builds pass **`-StaticQt`**.

### How you run it

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "path\to\codebase\win-cpp-qt\build.ps1" -StaticQt
```

### Parameters

| Switch | Meaning |
|--------|---------|
| `-StaticQt` | Link Qt and required plugins statically; output one `EraseAndRewrite.exe` (no `windeployqt`). **Required for release.** |

Without `-StaticQt`, the same script builds against the shared Qt kit and runs `deploy-standalone.ps1` for development.

### Output layout

Same as Go: **`<ProjectRoot>\bin\win\current\EraseAndRewrite.exe`** plus `versioninfo.json`, `build-meta.json`, and a `.date` marker. Static mode ships **no** `Qt6*.dll` or plugin folders beside the exe.

### High-level steps (static / `-StaticQt`)

Same git and folder discipline as `win-go/build.ps1` (version bump, snapshot commit, archive prior `current` via `.date`, history commit). Additionally:

1. **`Wipe-BinQtDeployArtifacts`** — remove legacy Qt DLL/plugin trees from all `bin/win/*` folders.
2. **`Sync-ArtAssets`** — copy `art/broombunny*.png` from project root into `codebase/assets/art/`.
3. **`tools/build_app_ico.cpp`** (MinGW, same toolchain as the app) builds `app.ico`; **`app.rc`** picks up version info and embeds the icon.
4. CMake configure/build in **`build-static/`** with `-DWTW_STATIC_QT=ON` against `mingw_64_static`.
5. Copy **`EraseAndRewrite.exe`** to `bin/win/current/`.
6. **`Strip-MingwStaticExe`** — `strip --strip-all` via the Qt MinGW toolchain (see below).
7. **`test-run.ps1 -StaticQt`** — import-table checks and smoke launch (optional verification).

Full detail: [specs/arch-win-cpp-qt.md](specs/arch-win-cpp-qt.md) §9, [specs/impl-win-cpp-qt.md](specs/impl-win-cpp-qt.md) §13.

### Static executable size and debug overlay stripping

Static MinGW links produce a **~47 MB** file immediately after link, but only **~29 MB** is the functional PE image. The remaining **~20 MB** is a **debug overlay** (DWARF data from static `.a` archives) appended after the last PE section. It is not needed to run the program.

**Shipping policy:** step 6 above runs **`strip --strip-all`** on the copied exe. Typical result: **~28 MB** shipping binary (build **1.0.0.001A** onward).

| Stage | Typical size |
|-------|----------------|
| Link output | ~47 MB |
| After `strip --strip-all` | ~28 MB |

This satisfies techspec **WR-03**: standard PE metadata and `VERSIONINFO` (`.rsrc`) are preserved; only debug overlay is removed. `strip --strip-debug` alone is **not** sufficient — it leaves the large tail intact.

Environment overrides: `CMAKE_PREFIX_PATH` (shared kit hint), `QT_STATIC_PREFIX` (explicit static prefix).

Optional signing: same `ERASE_REWRITE_SIGNTOOL` / `ERASE_REWRITE_SIGN_ARGS` variables as the Go build (applied after tests).

---

## 2. Installer build: `scripts/build-installer.bat`

Run **after** a successful exe build so `SourceDir` contains `Erase & Rewrite.exe`.

### Arguments (required)

```bat
scripts\build-installer.bat <SourceDir> <OutputRootDir>
```

| Arg | Position | Description |
|-----|----------|-------------|
| `SourceDir` | `%1` | Folder that **must** contain **`Erase & Rewrite.exe`** (typically `<ProjectRoot>\bin\win\current`). Expanded with `%~f1`. |
| `OutputRootDir` | `%2` | Root directory under which the batch creates a **timestamped** subfolder and passes that to Inno as output. Expanded with `%~f2`. |

Default project convention: set `OutputRootDir` to **`<ProjectRoot>\delivery\win`** unless explicitly overridden.

The batch resolves **`installer\Erase & Rewrite.iss`** as `"%SCRIPT_DIR%..\installer\Erase & Rewrite.iss"` (i.e. relative to `scripts/`, inside **codebase**).

### Behavior

1. Validates `SourceDir` and `Erase & Rewrite.exe`; creates `OutputRootDir` if missing.
2. Sets `STAMP` to `yyyy-MM-dd_HH-mm` (via PowerShell), then `OUTPUT_DIR = OutputRootDir\STAMP`.
3. Locates **Inno Setup 6** `ISCC.exe`: `%LOCALAPPDATA%\Programs\Inno Setup 6\`, `ProgramFiles(x86)`, `ProgramFiles`, then **`PATH`**.
4. Invokes:

   ```text
   ISCC.exe "/DSourceDir=<SourceDir>" "/DOutputDir=<OUTPUT_DIR>" "<path\to>\installer\Erase & Rewrite.iss"
   ```

### Exit codes (batch)

| Code | Meaning |
|------|---------|
| 0 | Success |
| 2 | Missing `SourceDir` or `OutputRootDir` |
| 3 | `SourceDir` does not exist |
| 4 | `Erase & Rewrite.exe` missing under `SourceDir` |
| 5РІР‚вЂњ8 | mkdir / stamp failures |
| 6 | `ISCC.exe` not found |
| 9 | Inno script path not found |
| Other | Inno compiler return code |

---

## 3. Inno script: `installer/Erase & Rewrite.iss`

This file is **not** run directly; **`ISCC.exe`** compiles it. The batch always passes the defines below.

### Preprocessor defines (compiler `/DРІР‚В¦`)

| Define | Required | Default if omitted | Meaning |
|--------|----------|--------------------|---------|
| `SourceDir` | **Yes** (script `#error`s if missing) | РІР‚вЂќ | Directory tree packaged into `{app}`; must contain `Erase & Rewrite.exe`. |
| `OutputDir` | No | Falls back to `SourceDir` in the `.iss` | Where the generated **setup .exe** is written. The batch always sets it to `OutputRootDir\yyyy-MM-dd_HH-mm`. |

### Version string

`ProductVersion` is read at compile time from **`GetStringFileInfo(SourceDir + "\Erase & Rewrite.exe", "ProductVersion")`**. If empty, it falls back to `0.0.0.0`. **`OutputBaseFilename`** becomes `Erase & Rewrite-Setup-{#ProductVersion}`.

### Packaging rules (summary)

- **`[Files]`** installs only:
  - `EraseAndRewrite.exe` from `SourceDir` (the build output folder)
  - `COPYING`, `LICENSE-NOTICE`, and `THIRD-PARTY-NOTICES` from `installer/` (LEGALSPEC LS-50)
- Build metadata (`versioninfo.json`, `build-meta.json`, `*.date`) stays in `bin/win/current` for engineering traceability and is **not** shipped to `{app}`.
- **Legal notice page**: `[Setup]` uses `InfoBeforeFile=INSTALL-LICENSE.txt`, so the installer shows GPL and third-party license notices before file installation with **Next** only — no acceptance gate (GPLv3 section 9; LEGALSPEC LS-120).

---

## 4. End-to-end example (typical machine)

**Qt shipping build (current Windows line):**

```powershell
Set-Location ...\codebase\win-cpp-qt
.\build.ps1 -StaticQt
```

**Go build (legacy reference line):**

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

**`scripts/install-githooks.ps1`** РІР‚вЂќ no arguments; walks up to `.git`, runs **`git config core.hooksPath .githooks`**. Unrelated to compiling the exe or the installer.

Git hooks (including `post-commit`) are not the source of truth for this build flow and must not redefine it into commit-triggered builds.
