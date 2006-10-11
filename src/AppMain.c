/*! -*- Mode: C -*-
* Module: AppMain.c
* Version: $Header$
*/

#include <PalmOS.h>

#include "AppResources.h"
#include "AppGlobals.h"

/***********************************************************************
 *
 * FUNCTION:    MainFormDoCommand
 *
 * DESCRIPTION: This routine performs the menu command specified.
 *
 * PARAMETERS:  command  - menu item id
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Boolean MainFormDoCommand(UInt16 command)
{
  Boolean handled = false;

  switch (command) {
  case MainOptionsAboutStarterApp:
    AboutFormDisplay();
    handled = true;
    break;
  }
	
  return handled;
}


/***********************************************************************
 *
 * FUNCTION:    MainFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the 
 *              "MainForm" of this application.
 *
 * PARAMETERS:  pEvent  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Boolean MainFormHandleEvent(EventType* pEvent)
{
  Boolean 	handled = false;
  FormType* 	pForm;

  switch (pEvent->eType) {
  case menuEvent:
    return MainFormDoCommand(pEvent->data.menu.itemID);

  case frmOpenEvent:
    pForm = FrmGetActiveForm();
    FrmDrawForm(pForm);
    handled = true;
    break;
			
  default:
    break;
  }
	
  return handled;
}

/** About form. **/
void AboutFormDisplay()
{
  FormType *pForm;

  pForm = FrmInitForm(AboutForm);
  FrmDoDialog(pForm);
  FrmDeleteForm(pForm);
}

/** Initial application startup. **/
static Err AppStart(void)
{
  FrmGotoForm(MainForm);
  return errNone;
}

/** Final application cleanup. **/
static void AppStop(void)
{
  // Close all the open forms.
  FrmCloseAllForms();
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
    UInt16 formId;
    FormType* pForm;

    // Load the form resource.
    formId = pEvent->data.frmLoad.formID;
		
    pForm = FrmInitForm(formId);
    FrmSetActiveForm(pForm);

    // Set the event handler for the form.  The handler of the currently
    // active form is called by FrmHandleEvent each time is receives an
    // event.
    switch (formId) {
    case MainForm:
      FrmSetEventHandler(pForm, MainFormHandleEvent);
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
    if ((error = AppStart()) == 0) {			
      AppEventLoop();
      AppStop();
    }
    break;
  }
	
  return error;
}
