#ifndef SourceDir
  #error "SourceDir is required. Pass /DSourceDir=..."
#endif

#ifndef OutputDir
  #define OutputDir SourceDir
#endif

#define ProductVersion GetStringFileInfo(SourceDir + "\EraseAndRewrite.exe", "ProductVersion")
#if ProductVersion == ""
  #define ProductVersion "0.0.0.0"
#endif

[Setup]
AppId={{A53E0BC6-12D1-4FA5-A83E-E65D2911F9D2}
AppName=Erase & Rewrite
AppVersion={#ProductVersion}
AppPublisher=Erase & Rewrite
InfoBeforeFile=INSTALL-LICENSE.txt
DefaultDirName={autopf}\Erase & Rewrite
DefaultGroupName=Erase & Rewrite
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=EraseAndRewrite-Setup-{#ProductVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\EraseAndRewrite.exe

[Files]
; Explicit file list only — no wildcards. SourceDir may contain engineering metadata
; (versioninfo.json, build-meta.json, *.date) beside the exe; those must never be installed.
Source: "{#SourceDir}\EraseAndRewrite.exe"; DestDir: "{app}"; Flags: ignoreversion
; Legal texts from installer/ (paths relative to this .iss file).
Source: "COPYING"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE-NOTICE"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Erase & Rewrite"; Filename: "{app}\EraseAndRewrite.exe"; AppUserModelID: "MikhailOstrogorskiy.EraseAndRewrite"
Name: "{autodesktop}\Erase & Rewrite"; Filename: "{app}\EraseAndRewrite.exe"; Tasks: desktopicon; AppUserModelID: "MikhailOstrogorskiy.EraseAndRewrite"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\EraseAndRewrite.exe"; Description: "Launch Erase & Rewrite"; Flags: nowait postinstall skipifsilent
