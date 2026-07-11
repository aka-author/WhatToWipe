# Erase & Rewrite (Windows, Qt 6)

C++/Qt 6 implementation of the Erase & Rewrite functional specification: folder-size treemap with FS-compliant scanning, projection, settings grid, and shell integration.

## Build

From this directory:

```powershell
.\build.ps1
```

Output:

`../../bin/win/current/EraseAndRewrite.exe`

## Config

New installs use:

`%LocalAppData%\Erase & Rewrite\Erase & Rewrite.config.txt`

Legacy Qt builds may have used `%LocalAppData%\WhatToWipe\WhatToWipe.config.txt`; that path is imported on first run when the FS path is missing.
