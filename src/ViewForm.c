/*! -*- Mode: C -*-
* Module: ViewForm.c
* Version: $Header$
*/

#include <PalmOS.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants ***/


/*** Local storage ***/


/*** Local routines ***/


/*** Setup and event handling ***/

void ViewFormSetup(AppPreferences *prefs, BookAppInfo *appInfo)
{
}

void ViewFormSetdown(AppPreferences *prefs)
{
}

void ViewFormActivate()
{
  // Until finished doing this form.
  EditFormActivate();
}

static void ViewFormOpen(FormType *form)
{
}

Boolean ViewFormHandleEvent(EventType *event)
{
  Boolean handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    {
      FormType  *form;

      form = FrmGetActiveForm();
      ViewFormOpen(form);
      FrmDrawForm(form);
    }
    handled = true;
    break;

  default:
    break;
  }

  return handled;
}
