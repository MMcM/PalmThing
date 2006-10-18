/*! -*- Mode: C -*-
* Module: AppMain.c
* Version: $Header$
*/

#include <PalmOS.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Global storage ***/

UInt32 g_ROMVersion = 0;
UInt16 g_CurrentRecord = NO_RECORD;
UInt16 g_CurrentCategory = dmAllCategories;

/** About form. **/
void AboutFormDisplay()
{
  FormType *pForm = FrmInitForm(AboutForm);
  FrmDoDialog(pForm);
  FrmDeleteForm(pForm);
}

/** Initial application startup. **/
static Err AppStart(void)
{
  AppPreferences prefs, *pprefs;
  Int16 prefVer;
  UInt16 prefsSize;
  BookAppInfo *appInfo;
  Err error;

  // Get version of system ROM.
  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &g_ROMVersion);

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

  MemPtrUnlock(appInfo);
  
  FrmGotoForm(ListForm);
  return errNone;
}

/** Final application cleanup. **/
static void AppStop(void)
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
  PrefSetAppPreferences(APP_CREATOR, APP_PREF_ID, APP_PREF_VER, 
                        &prefs, sizeof(prefs), true);
}

/** Application event dispatching.  
 *
 * Only handle form load events.  From then on, form's own event
 * handler takes over.
**/
static Boolean AppHandleEvent(EventType* pEvent)
{
  Boolean handled = false;

  if (pEvent->eType == frmLoadEvent) {
    // Load the form resource.
    UInt16 formId = pEvent->data.frmLoad.formID;
    FormType* pForm = FrmInitForm(formId);
    FrmSetActiveForm(pForm);

    // Set the event handler for the form.  The handler of the currently
    // active form is called by FrmHandleEvent each time is receives an
    // event.
    switch (formId) {
    case ListForm:
      FrmSetEventHandler(pForm, ListFormHandleEvent);
      break;
    case ViewForm:
      FrmSetEventHandler(pForm, ViewFormHandleEvent);
      break;
    case EditForm:
      FrmSetEventHandler(pForm, EditFormHandleEvent);
      break;
    case NoteView:
    case NewNoteView:
      FrmSetEventHandler(pForm, NoteFormHandleEvent);
      break;
    }
    handled = true;
  }
	
  return handled;
}

/** Main event loop **/
static void AppEventLoop(void)
{
  EventType event;
  Err error;

  do {
    EvtGetEvent(&event, evtWaitForever);

    if (SysHandleEvent(&event))
      continue;
			
    if (MenuHandleEvent(0, &event, &error))
      continue;
			
    if (AppHandleEvent(&event))
      continue;

    FrmDispatchEvent(&event);

  } while (event.eType != appStopEvent);
}

/** Main entry point **/
UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
  Err error = errNone;

  switch (cmd) {
  case sysAppLaunchCmdNormalLaunch:
    error = AppStart();
    if (errNone == error) {
      AppEventLoop();
      AppStop();
    }
    break;
  }
	
  return error;
}
