#ifndef StageDir
  #error StageDir must be provided by the installer build script
#endif
#ifndef DistDir
  #error DistDir must be provided by the installer build script
#endif
#ifndef AppVersion
  #define AppVersion "3.0.0"
#endif

#define AppName "Within the Latent Walls"
#define AppNameFrench "Entre les Murs Latents"
#define AppExeName "WithinTheLatentWalls.exe"
[Setup]
AppId={{97D24A49-F2CF-40C6-84CC-8F240078BAC8}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Astrofra
AppPublisherURL=https://github.com/astrofra/game-liminal-raytraced-llm-world
AppSupportURL=https://github.com/astrofra/game-liminal-raytraced-llm-world/issues
AppUpdatesURL=https://github.com/astrofra/game-liminal-raytraced-llm-world
DefaultDirName={localappdata}\Programs\Within the Latent Walls
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
WizardStyle=modern dynamic
Compression=none
SolidCompression=no
DiskSpanning=yes
DiskSliceSize=2000000000
SlicesPerDisk=1
OutputDir={#DistDir}
OutputBaseFilename=Within-the-Latent-Walls-Setup-{#AppVersion}
LicenseFile={#StageDir}\LICENSE.txt
InfoBeforeFile={#StageDir}\INSTALLATION_README.txt
UninstallDisplayIcon={app}\{#AppExeName}
SetupLogging=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Dirs]
Name: "{localappdata}\WithinTheLatentWalls"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
  WorkingDir: "{app}"; Comment: "{#AppName} / {#AppNameFrench}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
  WorkingDir: "{app}"; Tasks: desktopicon; Comment: "{#AppName} / {#AppNameFrench}"
Name: "{group}\Documentation"; Filename: "{app}\README.md"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#AppExeName}"; \
  WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; \
  Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\models"
