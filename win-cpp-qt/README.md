# WhatToWipe (Windows, Qt 6)

C++/Qt 6 implementation of the WhatToWipe functional specification: folder-size treemap with FS-compliant scanning, projection, settings grid, and shell integration.

## Requirements

- Windows 10+
- CMake 3.21+
- Qt 6.5+ (Widgets)
- C++17 compiler (MinGW-w64 or MSVC matching your Qt build)

## Build

From this directory:

```powershell
.\build.ps1
```

Output executable:

`../../bin/win/current/WhatToWipe.exe`

Version metadata is taken from `versioninfo.json` (default `1.0.0.0000`).

## Configuration

On first run the app creates:

`%LocalAppData%\WhatToWipe\WhatToWipe.config.txt`

Legacy settings may be imported once from `%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt` after defaults are written.

## Layout

- `src/app` — application shell, session, main window
- `src/scan` — background scanner, archive classifier, subtree merge
- `src/treemap` — projection, squarified layout, widget
- `src/platform` — volume validation and shell open helpers
- `src/config` — typed treemap settings and atomic save
- `src/ui` — alerts, about, settings grid

Behavior follows `codebase/docs/specs/funcspec.md` and dispute resolutions (reject `DRIVE_REMOTE`, design A reparse handling, subtree merge on update, no cancel confirmation in settings).
