; Palm Support Library for Inno Setup
; by Andrew Gregory
; Version 1.8
; 23 November 2005
; http://www.scss.com.au/family/andrew/pdas/palm/innosetup/
;
; Refer to the Palm Conduit Development Kit for API documentation.

[CustomMessages]
PalmNoHotSync=Could not find Palm HotSync software. Cannot install {#SetupSetting("AppName")}.
PalmSelectUsers=Select Palm Users
PalmWhichUsers=Which Palm users should use {#SetupSetting("AppName")}?
PalmSelectUsersNext=Select the users who are to have {#SetupSetting("AppName")} installed on their Palm computer, then click Next.
PalmInstJSync=Installing JSync (may take a few minutes) ...
PalmInstCOM=Installing COM support (may take a few minutes) ...

#ifdef BUNDLE_JAVA_SUPPORT
[Setup]
ExtraDiskSpaceRequired=21677631
#endif
#ifdef BUNDLE_COM_SUPPORT
[Setup]
ExtraDiskSpaceRequired=1239028
#endif

; Required DLLs from the CDK
[Files]
Source: "{#CDK_PATH}\Common\Bin\condmgr.dll"; DestDir: "{app}"
Source: "{#CDK_PATH}\Common\Bin\HSAPI.dll"; DestDir: "{app}"
Source: "{#CDK_PATH}\Common\Bin\Instaide.dll"; DestDir: "{app}"
#ifdef BUNDLE_JAVA_SUPPORT
Source: "{#CDK_PATH}\Java\JSync Installer\JSyncInstaller.exe"; DestDir: "{tmp}"; Flags: ignoreversion nocompression
; Updated v6 DLL to fix Java conduit support in v6 desktop software
; Download file from <https://www.developerpavilion.com/palmos/page.asp?page_id=365&tool_id=14>
; and copy to the given source location
; Comment out if you don't need this
Source: "{#CDK_PATH}\Java\Bin\sync20.dll"; DestDir: "{code:PalmHotSyncDir}"; Flags: uninsneveruninstall; Check: FileIsVersion(ExpandConstant('{code:PalmHotSyncDir}\sync20.dll'), 6)
#endif
#ifdef BUNDLE_COM_SUPPORT
Source: "{#CDK_PATH}\Com\Installer\PalmCOMInstaller.exe"; DestDir: "{app}"; Flags: nocompression
#endif

#ifdef BUNDLE_JAVA_SUPPORT
[Run]
Filename: "{tmp}\JSyncInstaller.exe"; Parameters: "/s -a /s"; StatusMsg: "{cm:PalmInstJSync}"
#endif
#ifdef BUNDLE_COM_SUPPORT
[Run]
Filename: "{app}\PalmCOMInstaller.exe"; Parameters: "-s -a -s"; StatusMsg: "{cm:PalmInstCOM}"
[UninstallRun]
Filename: "{app}\PalmCOMInstaller.exe"; Parameters: "-s -a -uninstall -s"
#endif

[Code]
var
  PalmUserPage: TInputOptionWizardPage; { Palm user selection wizard page }

{ * * * * * * * * *
  *               *
  * Miscellaneous *
  *               *
  * * * * * * * * * }

{ Used to stop the v6 sync20.dll update from overwriting a v4 file }
function FileIsVersion(Filename: String; majver: Integer): Boolean;
var
  ms: Cardinal;
  ls: Cardinal;
begin
  Result := False;
  if GetVersionNumbers(Filename, ms, ls) then
    if (ms / 65536 ) = majver then
      Result := True;
end;

{ * * * * * * * * * * * *
  *                     *
  * Conduit Manager API *
  *                     *
  * * * * * * * * * * * * }
const
  CONDUIT_COMPONENT = 0;
  CONDUIT_APPLICATION = 1;
  ERR_BUFFER_TOO_SMALL = -1010;
function _CmGetHotSyncExecPath(Path: String; var Size: Integer): Integer; external 'CmGetHotSyncExecPath@files:condmgr.dll stdcall setuponly';
function CmGetHotSyncExecPath(var Path: String): Integer;
var
  size: Integer;
  idx: Integer;
begin
  Path := ' ';
  size := Length(Path);
  Result := _CmGetHotSyncExecPath(Path, size);
  if Result = ERR_BUFFER_TOO_SMALL then
  begin
    Path := StringOfChar(' ', size);
    Result := _CmGetHotSyncExecPath(Path, size);
  end;
  if Result = 0 then
  begin
    idx := Pos(StringOfChar(Chr(0), 1), Path);
    if idx > 0 then SetLength(Path, idx - 1);
  end
  else
  begin
    Path := '';
  end;
end;
function CmInstallCreator(CreatorID: String; CondType: Integer): Integer; external 'CmInstallCreator@files:condmgr.dll stdcall setuponly';
function CmRemoveConduitByCreatorID(CreatorID: String): Integer; external 'CmRemoveConduitByCreatorID@{app}\condmgr.dll stdcall uninstallonly';
function CmSetCreatorName(CreatorID: String; Name: String): Integer; external 'CmSetCreatorName@files:condmgr.dll stdcall setuponly';
function CmSetCreatorDirectory(CreatorID: String; Directory: String): Integer; external 'CmSetCreatorDirectory@files:condmgr.dll stdcall setuponly';
function CmSetCreatorFile(CreatorID: String; Filename: String): Integer; external 'CmSetCreatorFile@files:condmgr.dll stdcall setuponly';
function CmSetCreatorRemote(CreatorID: String; Remote: String): Integer; external 'CmSetCreatorRemote@files:condmgr.dll stdcall setuponly';
function CmSetCreatorTitle(CreatorID: String; Title: String): Integer; external 'CmSetCreatorTitle@files:condmgr.dll stdcall setuponly';
function CmSetCreatorPriority(CreatorID: String; Priority: Longint): Integer; external 'CmSetCreatorPriority@files:condmgr.dll stdcall setuponly';
function CmSetCreatorInfo(CreatorID: String; Info: String): Integer; external 'CmSetCreatorInfo@files:condmgr.dll stdcall setuponly';
function CmSetCreatorValueString(CreatorID: String; Value: String; Setting: String): Integer; external 'CmSetCreatorValueString@files:condmgr.dll stdcall setuponly';

{ * * * * * * * * * * * *
  *                     *
  * HotSync Manager API *
  *                     *
  * * * * * * * * * * * * }
const
  HsCloseApp = 0;
  HsStartApp = 1;
  HsRestart = 2;
  HsReserved = 3;
  HSFLAG_NONE = 0;
function   Installer_HsSetAppStatus(StatusType: Integer; StartFlags: Longint): Longint; external 'HsSetAppStatus@files:hsapi.dll stdcall setuponly';
function Uninstaller_HsSetAppStatus(StatusType: Integer; StartFlags: Longint): Longint; external 'HsSetAppStatus@{app}\hsapi.dll stdcall uninstallonly';
function HsSetAppStatus(StatusType: Integer; StartFlags: Longint): Longint;
begin
  if IsUninstaller() then
    Result := Uninstaller_HsSetAppStatus(StatusType, StartFlags)
  else
    Result := Installer_HsSetAppStatus(StatusType, StartFlags);
end;
function   Installer_HsRefreshConduitInfo(): Longint; external 'HsRefreshConduitInfo@files:hsapi.dll stdcall setuponly';
function Uninstaller_HsRefreshConduitInfo(): Longint; external 'HsRefreshConduitInfo@{app}\hsapi.dll stdcall uninstallonly';
function HsRefreshConduitInfo(): Longint;
begin
  if IsUninstaller() then
    Result := Uninstaller_HsRefreshConduitInfo()
  else
    Result := Installer_HsRefreshConduitInfo();
end;

{ * * * * * * * * * * *
  *                   *
  * Install Aide  API *
  *                   *
  * * * * * * * * * * * }
const
  ERR_PILOT_BUFSIZE_TOO_SMALL = -502;
function _PltGetUser(Index: Integer; Name: String; var Size: Shortint): Integer; external 'PltGetUser@files:instaide.dll stdcall setuponly';
function PltGetUser(Index: Integer; var Name: String): Integer;
var
  size: Shortint;
begin
  Name := StringOfChar(' ', 255);
  size := 0;
  Result := _PltGetUser(Index, Name, size);
  if Result = ERR_PILOT_BUFSIZE_TOO_SMALL then
  begin
    Name := StringOfChar(' ', size);
    Result := _PltGetUser(Index, Name, size);
  end;
  if Result >= 0 then
  begin
    SetLength(Name, Result);
  end
  else
  begin
    Name := '';
  end;
end;
function PltGetUserCount(): Integer; external 'PltGetUserCount@files:instaide.dll stdcall setuponly';
function PltInstallFile(User: String; Filename: String): Integer; external 'PltInstallFile@files:instaide.dll stdcall setuponly';
function PltGetUsers(): TArrayOfString;
var
  num: Integer;
  name: String;
  users: TArrayOfString;
  idx: Integer;
  err: Integer;
begin
  num := PltGetUserCount;
  SetArrayLength(users, num);
  for idx := 0 to num - 1 do
  begin
    err := PltGetUser(idx, name);
    users[idx] := name;
  end;
  Result := users;
end;

{ * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                       *
  * Conduit Installation/Uninstallation Helper Functions  *
  *                                                       *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * }
function PalmInstallConduit(CreatorID: String; cType: Integer; Name, Directory, Filename, Remote, Title: String; Priority: Integer): Integer;
var
  err: Integer;
begin
  err := CmInstallCreator(CreatorID, cType);
  if (err = 0) then
    err := CmSetCreatorName(CreatorID, Name);
  if (err = 0) and (Directory <> '') then
    err := CmSetCreatorDirectory(CreatorID, Directory);
  if (err = 0) and (Filename <> '') then
    err := CmSetCreatorFile(CreatorID, Filename);
  if (err = 0) and (Remote <> '') then
    err := CmSetCreatorRemote(CreatorID, Remote);
  if (err = 0) and (Title <> '') then
    err := CmSetCreatorTitle(CreatorID, Title);
  if (err = 0) then
    err := CmSetCreatorPriority(CreatorID, Priority);
  Result := err;
end;
function PalmInstallJavaConduit(CreatorID: String; cType: Integer; ClassName, ClassPath, Directory, Filename, Remote, Title: String; Priority: Integer): Integer;
var
  err: Integer;
begin
  err := PalmInstallConduit(CreatorID, cType, 'jsync13.dll', Directory, Filename, Remote, Title, Priority);
  if err = 0 then
    err := CmSetCreatorValueString(CreatorID, 'ClassName', ClassName);
  if err = 0 then
    err := CmSetCreatorValueString(CreatorID, 'ClassPath13', ClassPath);
  Result := err;
end;
function PalmInstallCOMConduit(CreatorID: String; cType: Integer; COMClient, Directory, Filename, Remote, Title: String; Priority: Integer): Integer;
var
  err: Integer;
begin
  err := PalmInstallConduit(CreatorID, cType, 'COMConduit.dll', Directory, Filename, Remote, Title, Priority);
  if err = 0 then
    err := CmSetCreatorValueString(CreatorID, 'COMClient', COMClient);
  Result := err;
end;
procedure PalmUninstallConduit(CreatorID: String);
begin
  { Uninstall the conduit }
  CmRemoveConduitByCreatorID(CreatorID);
end;

{ * * * * * * * * * * * * * * * * * * * * *
  *                                       *
  * Palm HotSync Dir for "code:" variable *
  *                                       *
  * * * * * * * * * * * * * * * * * * * * *}
function PalmHotSyncDir(Param: String): String;
var
  path: String;
begin
  CmGetHotSyncExecPath(path);
  Result := ExtractFileDir(path);
end;

{ * * * * * * * * * * * * * *
  *                         *
  * Wizard Helper Functions *
  *                         *
  * * * * * * * * * * * * * * }

{ Checks for HotSync installation. Call from InitializeSetup. }
function PalmInitializeSetup(): Boolean;
var
  path: String;
  err: Integer;
begin
  PalmUserPage := nil;
  err := CmGetHotSyncExecPath(path);
  Result := (err = 0) and (Length(path) > 0);
  if not Result then MsgBox(ExpandConstant('{cm:PalmNoHotSync}'), mbError, MB_OK);
end;

{ Creates the Palm user selection wizard page. Call from InitializeWizard. }
procedure PalmInitializeWizard(const AfterID: Integer);
var
  PalmUsers: TArrayOfString;
  idx: Integer;
begin
  PalmUsers := PltGetUsers();
  if GetArrayLength(PalmUsers) > 0 then
  begin
    PalmUserPage := CreateInputOptionPage(AfterID, ExpandConstant('{cm:PalmSelectUsers}'), ExpandConstant('{cm:PalmWhichUsers}'), ExpandConstant('{cm:PalmSelectUsersNext}'), False, True);
    for idx := 0 to GetArrayLength(PalmUsers) - 1 do
    begin
      PalmUserPage.Add(PalmUsers[idx]);
      PalmUserPage.Values[idx] := True;
    end;
  end;
end;

procedure PalmCurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    HsSetAppStatus(HsCloseApp, HSFLAG_NONE);
  if CurStep = ssPostInstall then
    HsSetAppStatus(HsStartApp, HSFLAG_NONE);
end;


{ Calls PltInstallFile for each selected Palm user. }
procedure PalmInstallApplication(Filename: String);
var
  idx: Integer;
begin
  if PalmUserPage <> nil then
  begin
    { Install the given Palm application for selected users }
    for idx := 0 to PalmUserPage.CheckListBox.ITEMS.Count - 1 do
      if PalmUserPage.Values[idx] then
        PltInstallFile(PalmUserPage.CheckListBox.ITEMS.Strings[idx], Filename);
  end;
end;

function PalmInitializeUninstall(): Boolean;
begin
  Result := True;
end;

procedure PalmUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    HsRefreshConduitInfo();
    { Release DLLs so they can be deleted }
    UnloadDLL(ExpandConstant('{app}\instaide.dll'));
    UnloadDLL(ExpandConstant('{app}\hsapi.dll'));
    UnloadDLL(ExpandConstant('{app}\condmgr.dll'));
  end;
end;

