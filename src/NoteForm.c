/*! -*- Mode: C -*-
* Module: NoteForm.c
* Version: $Header$
*/

#include <PalmOS.h>

#include "AppResources.h"
#include "AppGlobals.h"

/***
 * Since comments can be a bit too long for just a field on the Edit
 * page, we edit them using the standard application's NoteView.  This
 * includes things like emulating its non-standard title.  For this,
 * code is taken from palmosbible.com.
 ***/

/*** Local storage ***/

static UInt16 g_returnToForm = ListForm;

/*** Local routines ***/


/*** Setup and event handling ***/

void NoteFormSetup(BookAppInfo *appInfo)
{
}

void NoteFormActivate()
{
  // TODO: Make sure there is a note to edit.

  g_returnToForm = FrmGetActiveFormID();
  FrmGotoForm(NewNoteView);
}

static void NoteFormOpen(FormType *form)
{
}

Boolean NoteFormHandleEvent(EventType *event)
{
  Boolean handled;

  handled = false;

  switch (event->eType) {
  default:
    break;
  }

  return handled;
}

/*** Commands ***/

static Boolean NoteFormMenuCommand(UInt16 command)
{
  Boolean handled;

  handled = false;

  return handled;
}

static void NoteFormPageScroll(WinDirectionType direction)
{
}

static void NoteFormScroll(Int16 linesToScroll)
{
}

static void NoteFormSave(void)
{
}

static Boolean NoteFormDeleteNote()
{
  return false;
}

/*** Display ***/

static void NoteFormLoadRecord(void)
{
}

static void NoteFormDrawTitle(FormType *form)
{
}

static void NoteFormUpdateScrollBar(void)
{
}
