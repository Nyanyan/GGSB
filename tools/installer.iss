#ifndef AppVersion
  #error Build this script through tools/installer.ps1, or define AppVersion.
#endif
#ifndef PayloadDir
  #error Define PayloadDir as the extracted distribution ZIP directory.
#endif

[Setup]
AppId={{A4FE299F-5BF8-432F-90CB-3D5C933F9260}
AppName=GGSB
AppVersion={#AppVersion}
AppPublisher=Nyanyan
AppPublisherURL=https://github.com/Nyanyan/GGSB
AppSupportURL=https://github.com/Nyanyan/GGSB/issues
AppUpdatesURL=https://github.com/Nyanyan/GGSB/releases
DefaultDirName={localappdata}\Programs\GGSB
DefaultGroupName=GGSB
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
SetupArchitecture=x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SourceDir=..
OutputDir=build
OutputBaseFilename=GGSB-{#AppVersion}-windows-x64-setup
SetupIconFile=assets\icon.ico
UninstallDisplayIcon={app}\GGSB.exe
VersionInfoVersion={#AppVersion}
VersionInfoDescription=GGSB Installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
Uninstallable=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

; Only distribution files are installed. Runtime-created settings, accounts,
; logs and records are never overwritten or removed by the uninstaller.
[Files]
Source: "{#PayloadDir}\GGSB.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\THIRD_PARTY_LICENSES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\dll\*"; DestDir: "{app}\dll"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\engine\*"; DestDir: "{app}\engine"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\languages\*.json"; DestDir: "{app}\languages"; Flags: ignoreversion
Source: "{#PayloadDir}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\GGSB"; Filename: "{app}\GGSB.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\GGSB"; Filename: "{app}\GGSB.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\GGSB.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,GGSB}"; Flags: nowait postinstall skipifsilent
