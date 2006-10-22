/*! -*- Mode: C -*-
* Module: ISBNForm.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants and types ***/

enum { COL_ISBN = 0, COL_VALID };

// Update codes.
#define UPDATE_FONT_CHANGED 0x02
#define UPDATE_CATEGORY_CHANGED 0x04

/*** Local storage ***/

static FontID g_ISBNFont = largeBoldFont;

/*** Local routines ***/

static void ISBNFormOpen(FormType *form) ISBN_SECTION;
static Boolean ISBNFormMenuCommand(UInt16 command) ISBN_SECTION;
static void ISBNFontSelect() ISBN_SECTION;
static void ISBNFormNextField(WinDirectionType direction) ISBN_SECTION;
static void ISBNFormSelectField(UInt16 row, UInt16 column) ISBN_SECTION;
static void ISBNFormSelectCategory() ISBN_SECTION;
static Boolean ISBNFormUpdateDisplay(UInt16 updateCode) ISBN_SECTION;
static void ISBNFormSave() ISBN_SECTION;

static inline void *FrmGetObjectPtrFromID(const FormType *formP, UInt16 objID) ISBN_SECTION;

static inline void *FrmGetObjectPtrFromID(const FormType *formP, UInt16 objID)
{
  return FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, objID));
}

/*** Setup and event handling ***/

void ISBNFormSetup(AppPreferences *prefs, BookAppInfo *appInfo)
{
  if (NULL == prefs) {
    g_ISBNFont = FntGlueGetDefaultFontID(defaultLargeFont);
  }
  else {
    g_ISBNFont = prefs->isbnFont;
  }
}

void ISBNFormSetdown(AppPreferences *prefs)
{
  prefs->isbnFont = g_ISBNFont;
}

void ISBNFormActivate()
{
  FrmGotoForm(ISBNForm);
}

static void ISBNFormOpen(FormType *form)
{
  // TODO: stuff.

  CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ISBNCategorySelTrigger),
                          BookDatabaseGetCategoryName(g_CurrentCategory));
}

Boolean ISBNFormHandleEvent(EventType *event)
{
  FormType *form;
  Boolean handled;

  handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    form = FrmGetActiveForm();
    ISBNFormOpen(form);
    FrmDrawForm(form);
    handled = true;
    break;

  case frmUpdateEvent:
    return ISBNFormUpdateDisplay(event->data.frmUpdate.updateCode);

  case menuEvent:
    return ISBNFormMenuCommand(event->data.menu.itemID);
    
  case ctlSelectEvent:
    switch (event->data.ctlSelect.controlID) {
    case ISBNCategorySelTrigger:
      ISBNFormSelectCategory();
      handled = true;
      break;

    case ISBNDoneButton:
      ISBNFormSave();
      FrmGotoForm(ListForm);
      handled = true;
      break;

    case ISBNSaveButton:
      ISBNFormSave();
      handled = true;
      break;
    }
    break;

  case tblSelectEvent:
    ISBNFormSelectField(event->data.tblSelect.row, event->data.tblSelect.column);
    break;

  case keyDownEvent:
    if (TxtCharIsHardKey(event->data.keyDown.modifiers,
                         event->data.keyDown.chr)) {
      ISBNFormSave();
      FrmGotoForm(ListForm);
      handled = true;
    }
    else {
      switch (event->data.keyDown.chr) {
      case nextFieldChr:
        ISBNFormNextField(winDown);
        handled = true;
        break;
     
      case prevFieldChr:
        ISBNFormNextField(winUp);
        handled = true;
        break;
      }
    }
    break;
      
  case frmCloseEvent:
    // TODO: Anything?
    break;

  default:
    break;
  }

  return handled;
}

/*** Commands ***/

static Boolean ISBNFormMenuCommand(UInt16 command)
{
  FieldType *field;
  Boolean handled;
 
  handled = false;

  switch (command) {
  case EditUndo:
    field = GetFocusField();
    if (NULL != field) {
      FldUndo(field);
      handled = true;
    }
    break;

  case EditCut:
    field = GetFocusField();
    if (NULL != field) {
      FldCut(field);
      handled = true;
    }
    break;

  case EditCopy:
    field = GetFocusField();
    if (NULL != field) {
      FldCopy(field);
      handled = true;
    }
    break;
   
  case EditPaste:
    field = GetFocusField();
    if (NULL != field) {
      FldPaste(field);
      handled = true;
    }
    break;
   
  case EditSelectAll:
    field = GetFocusField();
    if (NULL != field) {
      FldSetSelection(field, 0, FldGetTextLength(field));
      handled = true;
    }
    break;
   
  case EditKeyboard:
    SysKeyboardDialog(kbdDefault);
    handled = true;
    break;
   
  case EditGraffitiHelp:
    SysGraffitiReferenceDialog(referenceDefault);
    handled = true;
    break;

  case OptionsAbout:
    AboutFormDisplay();
    handled = true;
    break;

  case OptionsFont:
    ISBNFontSelect();
    handled = true;
    break;
  }

  return handled;
}

static void ISBNFontSelect()
{
  FontID newFont;

  if (!SYS_ROM_3_0) return;

  newFont = FontSelect(g_ISBNFont);
  if (newFont != g_ISBNFont) {
    g_ISBNFont = newFont;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FONT_CHANGED);
  }
}

static void ISBNFormSelectField(UInt16 row, UInt16 column)
{
}

static void ISBNFormNextField(WinDirectionType direction)
{
  FormType *form;
  TableType *table;
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ISBNTable);
 
  if (!TblEditing(table))
    return;
   
  // TODO: ...
}

static void ISBNFormSelectCategory()
{
  FormType *form;
  UInt16 category;

  form = FrmGetActiveForm();

  category = g_CurrentCategory;
  BookDatabaseSelectCategory(form, 
                             ISBNCategorySelTrigger, ISBNCategoryList, true,
                             &category);
  
  if (category != g_CurrentCategory) {
    g_CurrentCategory = category;
    g_CurrentRecord = NO_RECORD;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_CATEGORY_CHANGED);
  }
}

/*** Display ***/

static Boolean ISBNFormUpdateDisplay(UInt16 updateCode)
{
  FormType *form;
  TableType *table;
  Boolean handled;
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ISBNTable);
  handled = false;
 
  if (updateCode & frmRedrawUpdateCode) {
    FrmDrawForm(form);
    handled = true;
  }
 
  if (updateCode & UPDATE_FONT_CHANGED) {
    TblReleaseFocus(table);
    ISBNFormOpen(form);
    TblRedrawTable(table);
    handled = true;
  }

  if (updateCode & UPDATE_CATEGORY_CHANGED) {
    CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ISBNCategorySelTrigger),
                            BookDatabaseGetCategoryName(g_CurrentCategory));
    handled = true;
  }

  return handled;
}

static void ISBNFormSave()
{
  FormType *form;
  TableType *table;
  
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ISBNTable);
  TblReleaseFocus(table);

  // TODO: ...
}
