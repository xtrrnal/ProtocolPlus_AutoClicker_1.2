[Setup]
AppName=Protocol+ Auto Clicker
AppVersion=1.2.0
DefaultDirName={autopf}\ProtocolPlus Auto Clicker
DefaultGroupName=Protocol+ Auto Clicker
OutputDir=release
OutputBaseFilename=ProtocolPlus_Auto_Clicker_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=resources\protocolplus.ico
UninstallDisplayIcon={app}\ProtocolPlus_Auto_Clicker.exe
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "build\ProtocolPlus_Auto_Clicker.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Protocol+ Auto Clicker"; Filename: "{app}\ProtocolPlus_Auto_Clicker.exe"
Name: "{autodesktop}\Protocol+ Auto Clicker"; Filename: "{app}\ProtocolPlus_Auto_Clicker.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\ProtocolPlus_Auto_Clicker.exe"; Description: "Launch Protocol+ Auto Clicker"; Flags: nowait postinstall skipifsilent
