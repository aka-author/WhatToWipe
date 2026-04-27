# Settings Grid Wails App

This module provides a true grid settings editor using Wails + AG Grid.

## Build

From `settings-grid-wails`:

```powershell
wails build -platform windows/amd64
```

Output binary name: `EraseAndRewriteSettingsGrid.exe`.

Place this executable either:

- next to `EraseAndRewrite.exe`, or
- under `tools\EraseAndRewriteSettingsGrid.exe`.

The main app (`win-go`) launches this process from `Tools -> Settings...`.

## Frontend

From `settings-grid-wails/frontend`:

```powershell
npm install
npm run build
```

## Notes

- This app edits the same `%AppData%` config file (`Erase & Rewrite.config.txt`).
- Validation is enforced before save.
