; Inno Setup script of the Polish translation of Legacy of Kain: Defiance Remastered.
; The installer does not contain game data: it carries PolishPatcher.exe and polish.pak, finds the
; game, and lets the patcher change the user's own bigfile.x64.dat (see docs/patching.md).
;
; Build:  ISCC.exe /DAppVersion=1.0.0 /DPatcherExe=..\dist\PolishPatcher.exe /DPackageFile=..\polish.pak lokdp.iss

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef PatcherExe
  #define PatcherExe "..\dist\PolishPatcher.exe"
#endif
#ifndef PackageFile
  #define PackageFile "..\polish.pak"
#endif

[Setup]
AppId={{4FB1E66D-D226-40BE-A53C-8C7010B15BEF}
AppName=Legacy of Kain: Defiance Remastered PL
AppVersion={#AppVersion}
AppPublisher=Grupa Polonizacyjna Nosgothica
AppPublisherURL=https://github.com/Grupa-Polonizacyjna-Nosgothica/lok-defiance-remaster-pl
AppSupportURL=https://discord.gg/bJKCtJcaDg
; The installer works in the game's folder
DefaultDirName={code:GetDefaultGameDir}
AppendDefaultDirName=no
DirExistsWarning=no
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
; Games in "Program Files" are not writable without administrator rights
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
UninstallFilesDir={app}\LokDP-PL
UninstallDisplayName=Legacy of Kain: Defiance Remastered PL
UninstallDisplayIcon={app}\LokDP-PL\lokdp.ico
CloseApplications=no
WizardStyle=modern
SetupIconFile=lokdp.ico
WizardImageFile=wizardimg.bmp
WizardSmallImageFile=wizardsmall.bmp
InfoBeforeFile=INSTALL.txt
OutputDir=..\dist
OutputBaseFilename=LoKDR-PL-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Used by the wizard before anything is installed
Source: "{#PatcherExe}"; DestDir: "{tmp}"; Flags: dontcopy
Source: "{#PatcherExe}"; DestDir: "{app}\LokDP-PL"; Flags: ignoreversion
Source: "{#PackageFile}"; DestDir: "{app}\LokDP-PL"; Flags: ignoreversion
Source: "reapply.cmd"; DestDir: "{app}\LokDP-PL"; Flags: ignoreversion
Source: "lokdp.ico"; DestDir: "{app}\LokDP-PL"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Legacy of Kain Defiance Remastered PL\{cm:ReapplyName}"; Filename: "{app}\LokDP-PL\reapply.cmd"; WorkingDir: "{app}\LokDP-PL"; IconFilename: "{app}\LokDP-PL\lokdp.ico"
Name: "{autoprograms}\Legacy of Kain Defiance Remastered PL\{cm:UninstallName}"; Filename: "{uninstallexe}"

[CustomMessages]
polish.ReapplyName=Zastosuj spolszczenie ponownie
polish.UninstallName=Odinstaluj spolszczenie
polish.NotAGameDir=Ten folder nie wygląda na folder gry Legacy of Kain: Defiance Remastered (brak plików bigfile.x64.dat i defiance.exe).%n%nWskaż główny folder gry (w Steam: prawy przycisk na grze, Zarządzaj, Przeglądaj pliki lokalne).
polish.ErrVersion=Ta wersja gry nie jest obsługiwana. Spolszczenie przygotowano dla wersji ze Steam.%n%nSprawdź integralność plików gry w kliencie Steam lub GOG i spróbuj ponownie.
polish.ErrRunning=Gra jest uruchomiona. Zamknij ją (oraz launcher) i spróbuj ponownie.
polish.ErrWrite=Nie można zapisać pliku bigfile.x64.dat. Zamknij grę i launcher, upewnij się, że plik nie jest tylko do odczytu, i spróbuj ponownie.
polish.ErrSpace=Brak miejsca na dysku z grą.
polish.ErrChanged=Plik gry został zmieniony po poprzednim spolszczeniu (np. przez aktualizację lub sprawdzenie plików). Sprawdź integralność plików gry w kliencie Steam lub GOG i zainstaluj spolszczenie ponownie.
polish.ErrGeneric=Nie udało się zmodyfikować plików gry.
polish.ErrLog=Szczegóły zapisano w pliku:
polish.Patching=Modyfikowanie plików gry...
polish.PatchFailedHint=Spolszczenie nie zostało zastosowane. Po usunięciu problemu uruchom skrót "Zastosuj spolszczenie ponownie" z menu Start.
polish.Done=Spolszczenie zostało zainstalowane.%n%nW launcherze gry wybierz język rosyjski - w jego miejsce gra wyświetli teksty polskie.
polish.UninstallRunning=Gra jest uruchomiona. Zamknij ją (oraz launcher) i uruchom dezinstalator ponownie.
polish.UninstallChanged=Plik gry został zmieniony po zainstalowaniu spolszczenia, więc nie można go automatycznie przywrócić. Sprawdź integralność plików gry w kliencie Steam lub GOG, aby przywrócić oryginał. Dezinstalacja będzie kontynuowana.
polish.UninstallFailed=Nie udało się przywrócić oryginalnego pliku gry. Dezinstalacja została przerwana.
english.ReapplyName=Apply the translation again
english.UninstallName=Uninstall the translation
english.NotAGameDir=This folder does not look like the Legacy of Kain: Defiance Remastered game folder (bigfile.x64.dat and defiance.exe are missing).%n%nSelect the main game folder (in Steam: right-click the game, Manage, Browse local files).
english.ErrVersion=This version of the game is not supported. The translation is made for the Steam build.%n%nVerify the game files in your Steam or GOG client and try again.
english.ErrRunning=The game is running. Close it (and its launcher) and try again.
english.ErrWrite=Cannot write bigfile.x64.dat. Close the game and its launcher, make sure the file is not read-only, and try again.
english.ErrSpace=Not enough free disk space on the drive of the game.
english.ErrChanged=The game file was changed after the previous patch (for example by an update or a file verification). Verify the game files in your Steam or GOG client and install the translation again.
english.ErrGeneric=The game files could not be modified.
english.ErrLog=Details were saved to:
english.Patching=Modifying the game files...
english.PatchFailedHint=The translation was not applied. After fixing the problem run "Apply the translation again" from the Start menu.
english.Done=The translation is installed.%n%nSelect Russian in the game launcher; the game will show the Polish texts in its place.
english.UninstallRunning=The game is running. Close it (and its launcher) and run the uninstaller again.
english.UninstallChanged=The game file was changed after the translation was installed, so it cannot be restored automatically. Verify the game files in your Steam or GOG client to get the original back. Uninstalling continues.
english.UninstallFailed=The original game file could not be restored. Uninstalling was cancelled.

[Code]
const
  SteamAppId = '3747730';

var
  DetectedGameDir: String;
  DetectionDone: Boolean;

// ---------------------------------------------------------------- finding the game

function IsGameDir(const Dir: String): Boolean;
begin
  Result := (Dir <> '') and FileExists(AddBackslash(Dir) + 'bigfile.x64.dat')
            and FileExists(AddBackslash(Dir) + 'defiance.exe');
end;

// Value of a line like:  "key"   "value"  (Valve's text format); backslashes are doubled in the file
function VdfValue(const Line, Key: String): String;
var
  S: String;
  P: Integer;
begin
  Result := '';
  S := Trim(Line);
  if Pos('"' + Key + '"', S) <> 1 then Exit;
  S := Copy(S, Length(Key) + 3, MaxInt);
  P := Pos('"', S);
  if P = 0 then Exit;
  S := Copy(S, P + 1, MaxInt);
  P := Pos('"', S);
  if P = 0 then Exit;
  Result := Copy(S, 1, P - 1);
  StringChangeEx(Result, '\\', '\', True);
end;

function FindInSteamLibrary(const Library_: String): String;
var
  Lines: TArrayOfString;
  I: Integer;
  InstallDir, Candidate: String;
begin
  Result := '';
  if not LoadStringsFromFile(AddBackslash(Library_) + 'steamapps\appmanifest_' + SteamAppId + '.acf', Lines) then Exit;
  for I := 0 to GetArrayLength(Lines) - 1 do
  begin
    InstallDir := VdfValue(Lines[I], 'installdir');
    if InstallDir <> '' then
    begin
      Candidate := AddBackslash(Library_) + 'steamapps\common\' + InstallDir;
      if IsGameDir(Candidate) then Result := Candidate;
      Exit;
    end;
  end;
end;

function FindSteamGame: String;
var
  SteamDir, Value: String;
  Lines: TArrayOfString;
  I: Integer;
begin
  Result := '';
  if not RegQueryStringValue(HKLM32, 'SOFTWARE\Valve\Steam', 'InstallPath', SteamDir) then
    if not RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamDir) then Exit;
  StringChangeEx(SteamDir, '/', '\', True);

  Result := FindInSteamLibrary(SteamDir);
  if Result <> '' then Exit;
  // Other library folders are listed in libraryfolders.vdf
  if LoadStringsFromFile(AddBackslash(SteamDir) + 'steamapps\libraryfolders.vdf', Lines) then
    for I := 0 to GetArrayLength(Lines) - 1 do
    begin
      Value := VdfValue(Lines[I], 'path');
      if Value <> '' then
      begin
        Result := FindInSteamLibrary(Value);
        if Result <> '' then Exit;
      end;
    end;
end;

function FindGogGame: String;
var
  Names: TArrayOfString;
  I: Integer;
  GameName, GamePath: String;
begin
  Result := '';
  if not RegGetSubkeyNames(HKLM32, 'SOFTWARE\GOG.com\Games', Names) then Exit;
  for I := 0 to GetArrayLength(Names) - 1 do
    if RegQueryStringValue(HKLM32, 'SOFTWARE\GOG.com\Games\' + Names[I], 'gameName', GameName)
       and (Pos('Defiance', GameName) > 0)
       and RegQueryStringValue(HKLM32, 'SOFTWARE\GOG.com\Games\' + Names[I], 'path', GamePath)
       and IsGameDir(GamePath) then
    begin
      Result := GamePath;
      Exit;
    end;
end;

function GetDefaultGameDir(Param: String): String;
begin
  if not DetectionDone then
  begin
    DetectionDone := True;
    DetectedGameDir := FindSteamGame;
    if DetectedGameDir = '' then DetectedGameDir := FindGogGame;
  end;
  if DetectedGameDir <> '' then
    Result := DetectedGameDir
  else
    Result := ExpandConstant('{autopf}') + '\Legacy of Kain Defiance Remastered';
end;

// ---------------------------------------------------------------- running the patcher

// Runs PolishPatcher through cmd so that its messages end up in a log file. Returns the exit code, or -1.
function RunPatcher(const Exe, Arguments, LogFile: String): Integer;
var
  ResultCode: Integer;
begin
  if Exec(ExpandConstant('{cmd}'), '/C ""' + Exe + '" ' + Arguments + ' > "' + LogFile + '" 2>&1"', '',
          SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    Result := ResultCode
  else
    Result := -1;
end;

function MessageForExitCode(const Code: Integer): String;
begin
  case Code of
    3: Result := CustomMessage('ErrVersion');
    4: Result := CustomMessage('ErrRunning');
    5: Result := CustomMessage('ErrSpace');
    6: Result := CustomMessage('ErrChanged');
    7: Result := CustomMessage('ErrWrite');
  else
    Result := CustomMessage('ErrGeneric');
  end;
end;

// ---------------------------------------------------------------- wizard

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Dir, LogFile: String;
  Code: Integer;
begin
  Result := True;
  if CurPageID <> wpSelectDir then Exit;

  Dir := WizardDirValue;
  if not IsGameDir(Dir) then
  begin
    MsgBox(CustomMessage('NotAGameDir'), mbError, MB_OK);
    Result := False;
    Exit;
  end;

  // Game build, running game and write access, before anything is copied
  ExtractTemporaryFile('PolishPatcher.exe');
  LogFile := ExpandConstant('{tmp}\check.log');
  Code := RunPatcher(ExpandConstant('{tmp}\PolishPatcher.exe'), 'check "' + AddBackslash(Dir) + 'bigfile.x64.dat"', LogFile);
  if Code <> 0 then
  begin
    MsgBox(MessageForExitCode(Code), mbError, MB_OK);
    Result := False;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Dir, LogFile: String;
  Code: Integer;
begin
  if CurStep <> ssPostInstall then Exit;

  Dir := ExpandConstant('{app}');
  LogFile := Dir + '\LokDP-PL\patch.log';
  WizardForm.StatusLabel.Caption := CustomMessage('Patching');
  Code := RunPatcher(Dir + '\LokDP-PL\PolishPatcher.exe',
                     'patch "' + Dir + '\bigfile.x64.dat" "' + Dir + '\LokDP-PL\polish.pak"', LogFile);
  if Code <> 0 then
    MsgBox(MessageForExitCode(Code) + #13#10#13#10 + CustomMessage('ErrLog') + #13#10 + LogFile + #13#10#13#10
           + CustomMessage('PatchFailedHint'), mbError, MB_OK);
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption := CustomMessage('Done') + #13#10#13#10 + WizardForm.FinishedLabel.Caption;
end;

// ---------------------------------------------------------------- uninstall

function InitializeUninstall: Boolean;
var
  Dir: String;
  Code: Integer;
begin
  Result := True;
  Dir := ExpandConstant('{app}');
  if not FileExists(Dir + '\bigfile.x64.dat') then Exit; // the game is gone; nothing to restore

  Code := RunPatcher(Dir + '\LokDP-PL\PolishPatcher.exe', 'restore "' + Dir + '\bigfile.x64.dat"',
                     Dir + '\LokDP-PL\restore.log');
  case Code of
    0: Result := True;
    4: begin MsgBox(CustomMessage('UninstallRunning'), mbError, MB_OK); Result := False; end;
    6: MsgBox(CustomMessage('UninstallChanged'), mbInformation, MB_OK);
  else
    begin MsgBox(CustomMessage('UninstallFailed'), mbError, MB_OK); Result := False; end;
  end;
end;
