#ifndef StageDir
  #error StageDir must be provided by the installer build script
#endif
#ifndef DistDir
  #error DistDir must be provided by the installer build script
#endif
#ifndef AppVersion
  #define AppVersion "2.2.0"
#endif

#define AppName "Within the Latent Walls"
#define AppNameFrench "Entre les Murs Latents"
#define AppExeName "WithinTheLatentWalls.exe"
#ifndef ModelDir
  #error ModelDir must come from the model manifest via build_installer.py
#endif
#ifndef ModelName
  #error ModelName must come from the model manifest via build_installer.py
#endif
#ifndef ModelUrl
  #error ModelUrl must come from the model manifest via build_installer.py
#endif
#ifndef ModelHash
  #error ModelHash must come from the model manifest via build_installer.py
#endif
#ifndef ModelSize
  #error ModelSize must come from the model manifest via build_installer.py
#endif

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
Compression=lzma2/ultra64
SolidCompression=yes
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
Name: "{app}\{#ModelDir}"
Name: "{localappdata}\WithinTheLatentWalls"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; The model is deliberately external: it is downloaded and SHA-256 verified by Setup.
Source: "{#ModelUrl}"; DestName: "{#ModelName}"; DestDir: "{app}\{#ModelDir}"; \
  Hash: "{#ModelHash}"; ExternalSize: {#ModelSize}; \
  Flags: external download ignoreversion onlyifdoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
  Parameters: "--sdl --model ""{app}\{#ModelDir}\{#ModelName}"" --location quarry_threshold --save-state ""{localappdata}\WithinTheLatentWalls\sdl_session_state.json"""; \
  WorkingDir: "{app}"; Comment: "{#AppName} / {#AppNameFrench}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
  Parameters: "--sdl --model ""{app}\{#ModelDir}\{#ModelName}"" --location quarry_threshold --save-state ""{localappdata}\WithinTheLatentWalls\sdl_session_state.json"""; \
  WorkingDir: "{app}"; Tasks: desktopicon; Comment: "{#AppName} / {#AppNameFrench}"
Name: "{group}\Documentation"; Filename: "{app}\README.md"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#AppExeName}"; \
  Parameters: "--sdl --model ""{app}\{#ModelDir}\{#ModelName}"" --location quarry_threshold --save-state ""{localappdata}\WithinTheLatentWalls\sdl_session_state.json"""; \
  WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; \
  Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\models"
