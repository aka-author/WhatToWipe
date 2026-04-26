#ifndef SourceDir
  #error "SourceDir is required. Pass /DSourceDir=..."
#endif

#ifndef OutputDir
  #define OutputDir SourceDir
#endif

#define ProductVersion GetStringFileInfo(SourceDir + "\Trash Advisor.exe", "ProductVersion")
#if ProductVersion == ""
  #define ProductVersion "0.0.0.0"
#endif

[Setup]
AppId={{A53E0BC6-12D1-4FA5-A83E-E65D2911F9D2}
AppName=Trash Advisor
AppVersion={#ProductVersion}
AppPublisher=Trash Advisor
LicenseFile=EULA.txt
DefaultDirName={autopf}\Trash Advisor
DefaultGroupName=Trash Advisor
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Trash Advisor-Setup-{#ProductVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\Trash Advisor.exe

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.date"

[Icons]
Name: "{autoprograms}\Trash Advisor"; Filename: "{app}\Trash Advisor.exe"
Name: "{autodesktop}\Trash Advisor"; Filename: "{app}\Trash Advisor.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\Trash Advisor.exe"; Description: "Launch Trash Advisor"; Flags: nowait postinstall skipifsilent
