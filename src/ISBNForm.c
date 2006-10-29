/*! -*- Mode: C -*-
* Module: ISBNForm.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants and types ***/

enum { COL_ISBN = 0, COL_STATUS };

// Update codes.
#define UPDATE_FONT_CHANGED 0x02
#define UPDATE_CATEGORY_CHANGED 0x04

typedef struct {
  MemHandle isbn;
  UInt16 record;
  UInt8 status;
} ISBNEntry;

static ISBNEntry *g_ISBNs = NULL;

/*** Local storage ***/

static FontID g_ISBNFont = largeBoldFont;

/*** Local routines ***/

static void ISBNFormOpen(FormType *form) ISBN_SECTION;
static void ISBNFormClose() ISBN_SECTION;
static Boolean ISBNFormMenuCommand(UInt16 command) ISBN_SECTION;
static void ISBNFontSelect() ISBN_SECTION;
static void ISBNFormNextField(WinDirectionType direction) ISBN_SECTION;
static void ISBNFormSelectCategory() ISBN_SECTION;
static Boolean ISBNFormUpdateDisplay(UInt16 updateCode) ISBN_SECTION;
static void ISBNFormSave() ISBN_SECTION;
static void ISBNFormClear() ISBN_SECTION;
static Err ISBNFormGetISBN(MemPtr table, Int16 row, Int16 column, 
                           Boolean editing, MemHandle *textH, Int16 *textOffset,
                           Int16 *textAllocSize, FieldType *fldText) ISBN_SECTION;
static Boolean ISBNFormSaveISBN(MemPtr table, Int16 row, Int16 column) ISBN_SECTION;
static void ISBNFormDrawStatus(MemPtr table, Int16 row, Int16 column,
                               RectangleType *bounds) ISBN_SECTION;

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
  TableType *table;
  FieldType *field;
  RectangleType bounds;
  Int16 row, nrows;
  UInt16 lineHeight, tableIndex, statusWidth;
  FontID oldFont;

  tableIndex = FrmGetObjectIndex(form, ISBNTable);
  table = FrmGetObjectPtr(form, tableIndex);
  nrows = TblGetNumberOfRows(table);

  g_ISBNs = (ISBNEntry *)MemPtrNew(nrows * sizeof(ISBNEntry));
  if (NULL == g_ISBNs) return;
  MemSet(g_ISBNs, nrows * sizeof(ISBNEntry), 0);

  oldFont = FntSetFont(g_ISBNFont);
  statusWidth = FntLineHeight();
  FntSetFont(oldFont);
  
  for (row = 0; row < nrows; row++) {
    TblSetItemStyle(table, row, COL_ISBN, textTableItem);
    TblSetItemFont(table, row, COL_ISBN, g_ISBNFont);
    TblSetRowHeight(table, row, lineHeight);
    TblSetItemStyle(table, row, COL_STATUS, customTableItem);
    TblSetRowUsable(table, row, true);
    g_ISBNs[row].record = NO_RECORD;
  }
  
  TblSetColumnUsable(table, COL_ISBN, true);
  TblSetColumnUsable(table, COL_STATUS, true);
  TblSetLoadDataProcedure(table, COL_ISBN, ISBNFormGetISBN);
  TblSetSaveDataProcedure(table, COL_ISBN, ISBNFormSaveISBN);
  TblSetCustomDrawProcedure(table, COL_STATUS, ISBNFormDrawStatus);

  oldFont = FntSetFont(symbolFont);
  statusWidth = FntCharWidth('X');
  FntSetFont(oldFont);

  TblGetBounds(table, &bounds);
  TblSetColumnWidth(table, COL_ISBN, bounds.extent.x - statusWidth - 1);
  TblSetColumnSpacing(table, COL_ISBN, 1);
  TblSetColumnWidth(table, COL_STATUS, statusWidth);

  if (dmAllCategories == g_CurrentCategory)
    g_CurrentCategory = dmUnfiledCategory;
  CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ISBNCategorySelTrigger),
                          BookDatabaseGetCategoryName(g_CurrentCategory));

  FrmDrawForm(form);

  FrmSetFocus(form, tableIndex);
  TblGrabFocus(table, 0, COL_ISBN);
  field = TblGetCurrentField(table);
  FldGrabFocus(field);
}

static void ISBNFormClose()
{
  FormType *form;
  TableType *table;
  Int16 row, nrows;

  if (NULL == g_ISBNs) return;

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ISBNTable);
  nrows = TblGetNumberOfRows(table);

  for (row = 0; row < nrows; row++) {
    if (NULL != g_ISBNs[row].isbn) {
      MemHandleFree(g_ISBNs[row].isbn);
    }
  }

  MemPtrFree(g_ISBNs);
  g_ISBNs = NULL;
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
      ISBNFormClear();
      handled = true;
      break;
    }
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
    ISBNFormClose();
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
    // TODO: Set font.
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
  // This does the actual save via SaveISBN.
  TblReleaseFocus(table);
}

static void ISBNFormClear()
{
  FormType *form;
  TableType *table;
  
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ISBNTable);
}

static Err ISBNFormGetISBN(MemPtr table, Int16 row, Int16 column, 
                           Boolean editing, MemHandle *textH, Int16 *textOffset,
                           Int16 *textAllocSize, FieldType *fldText)
{
  MemHandle isbn;
  Char *p;

  if (column != COL_ISBN) return errNone;

  isbn = g_ISBNs[row].isbn;

  if (editing && (NULL == isbn)) {
    isbn = MemHandleNew(11);
    if (NULL == isbn) return DmGetLastErr();
    p = (Char *)MemHandleLock(isbn);
    *p = '\0';
    MemHandleUnlock(isbn);
    g_ISBNs[row].isbn = isbn;
  }

  *textH = isbn;
  *textOffset = 0;
  *textAllocSize = (NULL == isbn) ? 0 : MemHandleSize(isbn);
  
  return errNone;
}

static Boolean ISBNFormSaveISBN(MemPtr table, Int16 row, Int16 column)
{
  ISBNEntry *entry;

  if (column != COL_ISBN) return false;

  entry = g_ISBNs + row;

  return true;
}

static void ISBNFormDrawStatus(MemPtr table, Int16 row, Int16 column,
                               RectangleType *bounds)
{
  FontID oldFont;
  char statusChar;
  Coord x;

  if (column != COL_STATUS) return;

  oldFont = FntSetFont(symbolFont);

  statusChar = 0x16;

  x = bounds->topLeft.x + (bounds->extent.x - FntCharWidth(statusChar)) / 2;
  WinDrawChars(&statusChar, 1, x, bounds->topLeft.y);

  FntSetFont(oldFont);
}

