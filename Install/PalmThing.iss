; PalmThing Inno Setup Script
; $Header$

[CustomMessages]
ExportCaption=Select Export Location
ExportDescription=Where is LibraryThing export located?
ExportSubCaption=Select where you download the LibraryThing tab-delimited export file, then click Next.
ExportCaption2=Location of export:
ExportFilters=Excel files|*.xls|Text files|*.txt|All files|*.*

[Setup]
AppName=PalmThing
AppVerName=PalmThing 1.0.0
AppVersion=1.0.0
; Consider moving to SourceForge.
AppPublisher=Mike McMahon
AppPublisherURL=http://home.comcast.net/~mmcm/palmthing/
AppSupportURL=http://home.comcast.net/~mmcm/palmthing/
AppUpdatesURL=http://home.comcast.net/~mmcm/palmthing/
DefaultDirName={pf}\PalmThing
DefaultGroupName=PalmThing
SourceDir=..
OutputBaseFilename=PalmThing
OutputDir=.

#define BUNDLE_JAVA_SUPPORT
#undef BUNDLE_COM_SUPPORT
#define CDK_PATH "..\..\CDK403"
#include "PalmConduit.iss"

[Files]
Source: "Java\PalmThingConduit.jar"; DestDir: "{app}"; Flags: nocompression
Source: "Release\PalmThing.prc"; DestDir: "{app}"
Source: "Docs\readme.html"; DestDir: "{app}"; Flags: isreadme
Source: "Docs\license.txt"; DestDir: "{app}"

[Icons]
; An icon to allow manual installation of your Palm app after setup
Name: "{group}\Install PalmThing on Palm"; Filename: "{code:PalmHotSyncDir}\Instapp.exe"; Parameters: "{app}\PalmThing.prc"
Name: "{group}\Uninstall PalmThing"; Filename: "{uninstallexe}"

[Code]
var
  LTExportPage: TInputFileWizardPage; { LibraryThing export file selection page }

function InitializeSetup(): Boolean;
begin
  Result := PalmInitializeSetup();
end;

procedure InitializeWizard;
begin
  { Put the Palm user selection wizard page after the directory selection page: }
  PalmInitializeWizard(wpSelectDir);
  { Put the LT export file selection wizard page before that: }
  LTExportPage := CreateInputFilePage(wpSelectDir, ExpandConstant('{cm:ExportCaption}'), ExpandConstant('{cm:ExportDescription}'), ExpandConstant('{cm:ExportSubCaption}'));
  LTExportPage.Add(ExpandConstant('{cm:ExportCaption2}'), ExpandConstant('{cm:ExportFilters}'), '.xls');
  LTExportPage.Values[0] := ExpandConstant('LibraryThing_TD.xls');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    { Java: Registers the conduit: }
    PalmInstallJavaConduit('plTN', CONDUIT_APPLICATION, 'palmthing.conduit.PalmThingConduit',
      ExpandConstant('{app}\PalmThingConduit.jar'), 'palmthing', LTExportPage.Values[0],
      'PalmThing-books', '{#SetupSetting("AppName")}', 2);
    { Queues the Palm application for installation on the selected users Palms: }
    PalmInstallApplication(ExpandConstant('{app}\PalmThing.prc'));
  end;
  PalmCurStepChanged(CurStep); { must be last }
end;

function InitializeUninstall(): Boolean;
begin
  Result := PalmInitializeUninstall();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    PalmUninstallConduit('plTN');
  end;
  PalmUninstallStepChanged(CurUninstallStep); { must be last }
end;
