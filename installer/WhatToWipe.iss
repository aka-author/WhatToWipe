#ifndef SourceDir
  #error "SourceDir is required. Pass /DSourceDir=..."
#endif

#ifndef OutputDir
  #define OutputDir SourceDir
#endif

#define ProductVersion GetStringFileInfo(SourceDir + "\WhatToWipe.exe", "ProductVersion")
#if ProductVersion == ""
  #define ProductVersion "0.0.0.0"
#endif

[Setup]
AppId={{A53E0BC6-12D1-4FA5-A83E-E65D2911F9D2}
AppName=WhatToWipe
AppVersion={#ProductVersion}
AppPublisher=WhatToWipe
LicenseFile=EULA.txt
DefaultDirName={autopf}\WhatToWipe
DefaultGroupName=WhatToWipe
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=WhatToWipe-Setup-{#ProductVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\WhatToWipe.exe

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.date"

[Icons]
Name: "{autoprograms}\WhatToWipe"; Filename: "{app}\WhatToWipe.exe"
Name: "{autodesktop}\WhatToWipe"; Filename: "{app}\WhatToWipe.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\WhatToWipe.exe"; Description: "Launch WhatToWipe"; Flags: nowait postinstall skipifsilent
