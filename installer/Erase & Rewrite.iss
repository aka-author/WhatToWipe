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
LicenseFile=INSTALL-LICENSE.txt
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
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.date"

[Icons]
Name: "{autoprograms}\Erase & Rewrite"; Filename: "{app}\EraseAndRewrite.exe"
Name: "{autodesktop}\Erase & Rewrite"; Filename: "{app}\EraseAndRewrite.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\EraseAndRewrite.exe"; Description: "Launch Erase & Rewrite"; Flags: nowait postinstall skipifsilent
