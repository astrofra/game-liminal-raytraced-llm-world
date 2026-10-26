#ifndef StageDir
  #error StageDir must be provided by the installer build script
#endif
#ifndef DistDir
  #error DistDir must be provided by the installer build script
#endif
#ifndef AppVersion
  #define AppVersion "3.0.1"
#endif
#ifndef ModelDir
  #error ModelDir must come from scripts/models.json via build_installer.py
#endif
#ifndef ModelName
  #error ModelName must come from scripts/models.json via build_installer.py
#endif
#ifndef ModelUrl
  #error ModelUrl must come from scripts/models.json via build_installer.py
#endif
#ifndef ModelHash
  #error ModelHash must come from scripts/models.json via build_installer.py
#endif
#ifndef ModelSize
  #error ModelSize must come from scripts/models.json via build_installer.py
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
Compression=lzma2/normal
SolidCompression=yes
DiskSpanning=no
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
Name: "{app}\{#ModelDir}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Excludes: "*.gguf"; Flags: ignoreversion recursesubdirs createallsubdirs
; Keep this LAST: game/assets/DLLs first, then download to a temporary file in
; the destination directory. Inno verifies Hash before renaming it to ModelName.
Source: "{#ModelUrl}"; DestName: "{#ModelName}"; DestDir: "{app}\{#ModelDir}"; \
  Hash: "{#ModelHash}"; ExternalSize: {#ModelSize}; \
  Flags: external download ignoreversion; Check: ModelDownloadRequired; AfterInstall: ModelDownloaded
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
  Flags: nowait postinstall skipifsilent; Check: IsModelReady

[UninstallDelete]
Type: filesandordirs; Name: "{app}\models"

[CustomMessages]
english.ModelIncompleteTitle=Installation incomplete
french.ModelIncompleteTitle=Installation incomplète
english.ModelIncomplete=The required Gemma model was not installed and verified. Run Setup again to finish the download. The game has not been launched.
french.ModelIncomplete=Le modèle Gemma requis n'a pas été installé et vérifié. Relancez l'installeur pour terminer le téléchargement. Le jeu n'a pas été lancé.

[Code]
var
  CheckedModelPath: String;
  ModelReady: Boolean;

function ModelPath: String;
begin
  Result := ExpandConstant('{app}\{#ModelDir}\{#ModelName}');
end;

function ModelDownloadRequired: Boolean;
var
  Path: String;
  Size: Int64;
begin
  Path := ModelPath;
  { Check functions can run repeatedly. Recheck only if the destination changed. }
  if CheckedModelPath <> Path then begin
    CheckedModelPath := Path;
    ModelReady := False;
    if FileSize64(Path, Size) and (Size = {#ModelSize}) then begin
      Log('Verifying existing Gemma model before reuse: ' + Path);
      try
        ModelReady := CompareText(GetSHA256OfFile(Path), '{#ModelHash}') = 0;
      except
        Log('Existing model cannot be verified: ' + GetExceptionMessage);
      end;
    end;
    if ModelReady then
      Log('Existing Gemma model verified; download skipped.')
    else
      Log('Gemma download required; missing or invalid existing model.');
  end;
  Result := not ModelReady;
end;

procedure ModelDownloaded;
var
  Size: Int64;
begin
  { The native Hash check has already succeeded before this callback. }
  ModelReady := FileSize64(ModelPath, Size) and (Size = {#ModelSize});
  if not ModelReady then
    RaiseException(CustomMessage('ModelIncomplete'));
  Log('Gemma download installed after SHA-256 verification.');
end;

function IsModelReady: Boolean;
begin
  Result := ModelReady;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and not ModelReady then begin
    Log(CustomMessage('ModelIncomplete'));
    SuppressibleMsgBox(CustomMessage('ModelIncomplete'), mbError, MB_OK, IDOK);
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  { Native file-error dialogs allow Skip. Do not display success in that case. }
  if (CurPageID = wpFinished) and not ModelReady then begin
    WizardForm.FinishedHeadingLabel.Caption := CustomMessage('ModelIncompleteTitle');
    WizardForm.FinishedLabel.Caption := CustomMessage('ModelIncomplete');
  end;
end;

function GetCustomSetupExitCode: Integer;
begin
  Result := 0;
  if not ModelReady then
    Result := 1;
end;
