; Bytes File Juggler & Cleaner -- Inno Setup installer script
; Build the app first (scripts/build.bat + scripts/package.bat), which
; produces a self-contained dist/ folder with Bytes.exe and all Qt
; runtime DLLs (via windeployqt). This script simply packages that folder.
;
; Compile with: iscc installer\Bytes.iss   (from the repository root)

#define MyAppName "Bytes"
#define MyAppFullName "Bytes File Juggler & Cleaner"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Bytes Team"
#define MyAppExeName "Bytes.exe"
#define MyDistDir "..\dist"

[Setup]
AppId={{B17E5000-BEEF-4B17-9A11-BYTE50000001}
AppName={#MyAppFullName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Bytes
DefaultGroupName=Bytes
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=Bytes_Setup
SetupIconFile=..\assets\bytes.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "startupicon"; Description: "Start Bytes automatically when Windows starts"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#MyDistDir}\Bytes.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyDistDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist recursesubdirs
Source: "{#MyDistDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#MyDistDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#MyDistDir}\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "{#MyDistDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\assets\bytes.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Bytes"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\bytes.ico"
Name: "{group}\Uninstall Bytes"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Bytes"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\bytes.ico"; Tasks: desktopicon
Name: "{userstartup}\Bytes"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--tray"; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Bytes now"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
