/*! -*- Mode: C -*-
* Module: AppMain.c
* Version: $Header$
*/

#include <PalmOS.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Global storage ***/

UInt32 g_ROMVersion = 0;
UInt16 g_EventInterval = evtWaitForever;
UInt16 g_CurrentCategory = dmAllCategories;
UInt16 g_CurrentRecord = NO_RECORD;
Boolean g_CurrentRecordEdited = false;

/** About form. **/
void AboutFormDisplay()
{
  FormType *form = FrmInitForm(AboutForm);
  FrmDoDialog(form);
  FrmDeleteForm(form);
}

/** Initial application startup. **/
static Err AppStart()
{
  AppPreferences prefs, *pprefs;
  Int16 prefVer;
  UInt16 prefsSize;
  BookAppInfo *appInfo;
  Err error;

  prefsSize = sizeof(prefs);
  prefVer = PrefGetAppPreferences(APP_CREATOR, APP_PREF_ID, &prefs, &prefsSize, true);
  if (prefVer != noPreferenceFound) {
    if (prefVer != APP_PREF_VER) {
      // Convert as necessary.
    }
    pprefs = &prefs;
  }
  else
    pprefs = NULL;

  error = BookDatabaseOpen();
  if (error) return error;

  appInfo = BookDatabaseGetAppInfo();

  ListFormSetup(pprefs, appInfo);
  ViewFormSetup(pprefs, appInfo);
  EditFormSetup(pprefs, appInfo);
  NoteFormSetup(pprefs, appInfo);
  ISBNFormSetup(pprefs, appInfo);

  MemPtrUnlock(appInfo);
  
  return errNone;
}

/** Final application cleanup. **/
static void AppStop()
{
  AppPreferences prefs;

  // Send a frmSave event to all the open forms.
  FrmSaveAllForms();

  // Close all the open forms.
  FrmCloseAllForms();

  // Close the application's data file.
  BookDatabaseClose();

  MemSet(&prefs, sizeof(prefs), 0);
  ListFormSetdown(&prefs);
  ViewFormSetdown(&prefs);
  EditFormSetdown(&prefs);
  NoteFormSetdown(&prefs);
  ISBNFormSetdown(&prefs);
  PrefSetAppPreferences(APP_CREATOR, APP_PREF_ID, APP_PREF_VER, 
                        &prefs, sizeof(prefs), true);
}

/** Application event dispatching.  
 *
 * Only handle form load events.  From then on, form's own event
 * handler takes over.
**/
static Boolean AppHandleEvent(EventType* event)
{
  Boolean handled = false;

  if (event->eType == frmLoadEvent) {
    // Load the form resource.
    UInt16 formId = event->data.frmLoad.formID;
    FormType* form = FrmInitForm(formId);
    FrmSetActiveForm(form);

    // Set the event handler for the form.  The handler of the currently
    // active form is called by FrmHandleEvent each time is receives an
    // event.
    switch (formId) {
    case ListForm:
      ListFormDowngrade(form);
      FrmSetEventHandler(form, ListFormHandleEvent);
      break;
    case ViewForm:
      FrmSetEventHandler(form, ViewFormHandleEvent);
      break;
    case EditForm:
      FrmSetEventHandler(form, EditFormHandleEvent);
      break;
    case NoteView:
    case NewNoteView:
      FrmSetEventHandler(form, NoteFormHandleEvent);
      break;
    case PreferencesForm:
      FrmSetEventHandler(form, PreferencesFormHandleEvent);
      break;
    case ISBNForm:
      FrmSetEventHandler(form, ISBNFormHandleEvent);
      break;
    }
    handled = true;
  }
	
  return handled;
}

/** Main event loop **/
static void AppEventLoop()
{
  EventType event;
  Err error;

  do {
    EvtGetEvent(&event, g_EventInterval);

    if (SysHandleEvent(&event))
      continue;
			
    if (MenuHandleEvent(0, &event, &error))
      continue;
			
    if (AppHandleEvent(&event))
      continue;

    FrmDispatchEvent(&event);

  } while (event.eType != appStopEvent);
}

static void GoToPrepare(GoToParamsPtr params)
{
  EventType event;
  UInt16 formID;
 
  MemSet(&event, sizeof(EventType), 0);

  formID = ViewFormGoToPrepare(params);

  event.eType = frmLoadEvent;
  event.data.frmLoad.formID = formID;
  EvtAddEventToQueue(&event);
 
  // Send frmGotoEvent in place of frmOpenEvent that FrmGotoForm would send.
  event.eType = frmGotoEvent;
  event.data.frmGoto.formID = formID;
  event.data.frmGoto.recordNum = params->recordNum;
  event.data.frmGoto.matchPos = params->matchPos;
  event.data.frmGoto.matchLen = params->matchCustom;
  event.data.frmGoto.matchFieldNum = params->matchFieldNum;
  EvtAddEventToQueue(&event);
}

/** Main entry point **/
UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
  UInt32 romVersion;
  Boolean launched;
  Err error;

  // Get version of system ROM (into local since some launch codes don't init globals).
  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
  if (romVersion < SYS_ROM_MIN) {
    if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
                       (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) {
      FrmAlert(RomIncompatibleAlert);
  
      // Pilot 1.0 will continuously relaunch this app unless we
      // switch to another safe one.
      if (romVersion < sysMakeROMVersion(2,0,0,sysROMStageRelease,0))
        AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
    }
    return sysErrRomIncompatible;
  }

  error = errNone;
  switch (cmd) {
  case sysAppLaunchCmdNormalLaunch:
    g_ROMVersion = romVersion;
    error = AppStart();
    if (error) return error;
    FrmGotoForm(ListForm);
    AppEventLoop();
    AppStop();
    break;

  case sysAppLaunchCmdFind:
    error = BookDatabaseFind((FindParamsType *)cmdPBP, FindHeader, ListFormDrawTitle);
    break;

  case sysAppLaunchCmdGoTo:
    launched = (0 != (launchFlags & sysAppLaunchFlagNewGlobals));
    if (launched) {
      g_ROMVersion = romVersion;
      error = AppStart();
      if (error) return error;
    }
    else {
      // TODO: Is there a case where this affects GoToParamsType.recordNum?
      // New records are at the end, but an old one might get completely cleared out.
      FrmCloseAllForms();
    }
    GoToPrepare((GoToParamsType *)cmdPBP);
    if (launched) {
      AppEventLoop();
      AppStop();   
    }      
    break;

  case sysAppLaunchCmdSyncNotify:
    // TODO: register for Exg.
    break;

  case sysAppLaunchCmdExgAskUser:
    // TODO: Exg dialog.
    break;

  case sysAppLaunchCmdExgReceiveData:
    if (launchFlags & sysAppLaunchFlagSubCall) {
      FrmSaveAllForms();
    }
    else {
      error = BookDatabaseOpen();
      if (error) return error;
    }
    // TODO: Exg receive.
    if (!(launchFlags & sysAppLaunchFlagSubCall)) {
      BookDatabaseClose();
    }
    break;
  }
	
  return error;
}
