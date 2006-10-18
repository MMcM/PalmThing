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
  BookAppInfo *appInfo;
  Err error;

  // Get version of system ROM.
  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &g_ROMVersion);

  error = BookDatabaseOpen();
  if (error) return error;

  // TODO: Get prefs.

  appInfo = BookDatabaseGetAppInfo();
  ListFormSetup(appInfo);
  ViewFormSetup(appInfo);
  EditFormSetup(appInfo);
  NoteFormSetup(appInfo);
  MemPtrUnlock(appInfo);
  
  FrmGotoForm(ListForm);
  return errNone;
}

/** Final application cleanup. **/
static void AppStop(void)
{
  // TODO: Save prefs.

  // Send a frmSave event to all the open forms.
  FrmSaveAllForms();

  // Close all the open forms.
  FrmCloseAllForms();

  // Close the application's data file.
  BookDatabaseClose();
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
