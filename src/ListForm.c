/*! -*- Mode: C -*-
* Module: ListForm.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants and types ***/

enum { COL_TITLE };

// Update codes.
#define UPDATE_FORCE_REDRAW 0x01
#define UPDATE_FONT_CHANGED 0x02
#define UPDATE_CATEGORY_CHANGED 0x04

#define SPACE_BETWEEN_FIELDS 4
#define LEFT_FRACTION_NUM 3
#define LEFT_FRACTION_DEN 4

/*** Local storage ***/

static UInt16 g_TopVisibleRecord = 0;
static FontID g_ListFont = stdFont;
static UInt16 g_ListFields = KEY_TITLE_AUTHOR;

/*** Local routines ***/

static void ListFormDrawRecord(MemPtr table, Int16 row, Int16 column,
                               RectangleType *bounds);
static void ListFormLoadTable();
static Boolean ListFormMenuCommand(UInt16 command);
static void ListFormItemSelected(EventType *event);
static void ListFormSelectCategory();
static void ListFormScroll(WinDirectionType direction, UInt16 amount, Boolean byPage);
static Boolean ListFormUpdateDisplay(UInt16 updateCode);
static void ListBeamCategory(Boolean send);
static void ListFontSelect();
static void ListFormUpdateScrollButtons();
static void ListFormSelectRecord(UInt16 recordNum);
static void ListFormDrawTitle(BookRecord *record, RectangleType *bounds);

static inline void *FrmGetObjectPtrFromID(const FormType *formP, UInt16 objID)
{
  return FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, objID));
}

/*** Setup and event handling ***/

void ListFormSetup(BookAppInfo *appInfo)
{
  g_ListFont = FntGlueGetDefaultFontID(defaultSystemFont);
  g_ListFields = appInfo->listFields;
}

static void ListFormOpen(FormType *form)
{
  TableType *table;
  FontID oldFont;
  RectangleType tableBounds;
  Int16 row, nrows;
  Int16 extraWidth;

  oldFont = FntSetFont(g_ListFont);

  table = FrmGetObjectPtrFromID(form, ListTable);
  TblGetBounds(table, &tableBounds);
  nrows = TblGetNumberOfRows(table);

  for (row = 0; row < nrows; row++) {
    TblSetItemStyle(table, row, COL_TITLE, customTableItem);
    TblSetItemFont(table, row, COL_TITLE, g_ListFont);
    TblSetRowUsable(table, row, false);
  }

  TblSetColumnUsable(table, COL_TITLE, true);
  TblSetCustomDrawProcedure(table, COL_TITLE, ListFormDrawRecord);

  extraWidth = 0;

  // TODO: Any extra columns worth having?  Note for comment?  Quick browser link?

  TblSetColumnWidth(table, COL_TITLE, tableBounds.extent.x - extraWidth);

  ListFormLoadTable();

  CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ListCategorySelTrigger), 
                          BookDatabaseGetCategoryName(g_CurrentCategory));

  FntSetFont(oldFont);
}

Boolean ListFormHandleEvent(EventType *event)
{
  Boolean handled;

  handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    {
      FormType *form;

      form = FrmGetActiveForm();
      ListFormOpen(form);
      FrmDrawForm(form);
    }
    handled = true;
    break;

  case frmUpdateEvent:
    return ListFormUpdateDisplay(event->data.frmUpdate.updateCode);

  case menuEvent:
    return ListFormMenuCommand(event->data.menu.itemID);
    
  case tblSelectEvent:
    ListFormItemSelected(event);
    handled = true;
    break;

  case ctlSelectEvent:
    switch (event->data.ctlSelect.controlID) {
    case ListNewButton:
      EditFormNewRecord();
      handled = true;
      break;
        
    case ListCategorySelTrigger:
      ListFormSelectCategory();
      handled = true;
      break;
    }
    break;
  
  case ctlRepeatEvent:
    switch (event->data.ctlEnter.controlID) {
    case ListScrollUpRepeating:
      ListFormScroll(winUp, 1, true);
      // Leave unhandled so button can repeat.
      break;
    
    case ListScrollDownRepeating:
      ListFormScroll(winDown, 1, true);
      // Leave unhandled so button can repeat.
      break;
    }
    break;

  case keyDownEvent:
    if (TxtCharIsHardKey(event->data.keyDown.modifiers,
                         event->data.keyDown.chr)) {
      // TODO: Any hard key handling?
    }
    else {
      switch (event->data.keyDown.chr) {
      case pageUpChr:
        ListFormScroll(winUp, 1, true);
        handled = true;
        break;
     
      case pageDownChr:
        ListFormScroll(winDown, 1, true);
        handled = true;
        break;
     
      case linefeedChr:
        if (NO_RECORD != g_CurrentRecord)
          ViewFormActivate();
        handled = true;
        break;
     
      default:
        // TODO: Expose lookup control and begin typing.
        break;
      }
    }
    break;

  default:
    break;
  }

  return handled;
}

/*** Commands ***/

static Boolean ListFormMenuCommand(UInt16 command)
{
  Boolean handled;
 
  handled = false;

  switch (command) {
  case RecordBeamCategory:
    ListBeamCategory(false);
    handled = true;
    break;

  case RecordSendCategory:
    ListBeamCategory(true);
    handled = true;
    break;

  case OptionsAbout:
    AboutFormDisplay();
    handled = true;
    break;

  case OptionsFont:
    ListFontSelect();
    handled = true;
    break;
  }

  return handled;
}

static void ListFontSelect()
{
  FontID newFont;

  newFont = FontSelect(g_ListFont);
  if (newFont != g_ListFont) {
    g_ListFont = newFont;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FONT_CHANGED);
  }
}

static void ListBeamCategory(Boolean send)
{
  // TODO: Do something.
}

static void ListFormItemSelected(EventType *event)
{
  g_CurrentRecord = TblGetRowID(event->data.tblSelect.pTable,
                                event->data.tblSelect.row);
  
  switch (event->data.tblSelect.column) {
  case COL_TITLE:
    ViewFormActivate();
    break;
  }
}

static UInt16 ListFormNumberOfRows(TableType *table)
{
  Int16 rows, nrows;
  UInt16 tableHeight;
  FontID oldFont;
  RectangleType  bounds;

  nrows = TblGetNumberOfRows(table);

  TblGetBounds(table, &bounds);
  tableHeight = bounds.extent.y;

  oldFont = FntSetFont(g_ListFont);
  rows = tableHeight / FntLineHeight();
  FntSetFont(oldFont);

  if (rows <= nrows)
    return rows;
  else
    return nrows;
}

static void ListFormScroll(WinDirectionType direction, UInt16 amount, Boolean byPage)
{
  FormType *form;
  TableType *table;
  UInt16 rowsPerPage;
  UInt16 newTopVisibleRecord;
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ListTable);
  rowsPerPage = ListFormNumberOfRows(table) - 1;
  newTopVisibleRecord = g_TopVisibleRecord;

  if (byPage) {
    amount *= rowsPerPage;
  }

  if (direction == winDown) {
    // Forward n or last page.
    if (!BookDatabaseSeekRecord(&newTopVisibleRecord, amount, dmSeekForward)) {
      newTopVisibleRecord = dmMaxRecordIndex;        
      if (byPage) {
        if (!BookDatabaseSeekRecord(&newTopVisibleRecord, rowsPerPage, dmSeekBackward)) {
          newTopVisibleRecord = 0;
          BookDatabaseSeekRecord(&newTopVisibleRecord, 0, dmSeekForward);
        }
      }
      else {
        BookDatabaseSeekRecord(&newTopVisibleRecord, 1, dmSeekBackward);
      }
    }
  }
  else {
    // Backward n or top.
    if (!BookDatabaseSeekRecord(&newTopVisibleRecord, amount, dmSeekBackward)) {
      newTopVisibleRecord = 0;
      BookDatabaseSeekRecord(&newTopVisibleRecord, 0, dmSeekForward);
    }
  }

  if (g_TopVisibleRecord != newTopVisibleRecord) {
    g_TopVisibleRecord = newTopVisibleRecord;
    ListFormLoadTable();
    TblRedrawTable(table);
  }
}

static void ListFormSelectCategory()
{
  FormType *form;
  UInt16 category;

  form = FrmGetActiveForm();

  category = g_CurrentCategory;
  BookDatabaseSelectCategory(form, 
                             ListCategorySelTrigger, ListCategoryList, true,
                             &category);
  
  if (category != g_CurrentCategory) {
    g_CurrentCategory = category;
    g_CurrentRecord = NO_RECORD;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_CATEGORY_CHANGED);
  }
}

/*** Display ***/

static void ListFormLoadTable()
{
  FormType *form;
  TableType *table;
  FontID oldFont;
  UInt16 lineHeight;
  UInt16 recordNum;
  Int16 row, nrows, nvisible;

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ListTable);
  TblUnhighlightSelection(table);
  nrows = TblGetNumberOfRows(table);
  nvisible = ListFormNumberOfRows(table);

  oldFont = FntSetFont(g_ListFont);
  lineHeight = FntLineHeight();
  FntSetFont(oldFont);

  recordNum = g_TopVisibleRecord;
  for (row = 0; row < nvisible; row++) {
    if (!BookDatabaseSeekRecord(&recordNum, 
                                (row > 0) ? 1 : 0, 
                                dmSeekForward))
      break;

    TblSetRowUsable(table, row, true);
    TblMarkRowInvalid(table, row);
    TblSetRowID(table, row, recordNum);
    TblSetRowHeight(table, row, lineHeight);
  }

  while (row < nrows) {
    TblSetRowUsable(table, row, false);
    row++;
  }

  ListFormUpdateScrollButtons();
}

static void ListFormUpdateScrollButtons()
{
  FormType *form;
  TableType *table;
  Int16 row;
  UInt16 recordNum;
  Boolean scrollableUp, scrollableDown;
  UInt16 upIndex, downIndex;

  recordNum = g_TopVisibleRecord;
  // Up if not top first record.
  scrollableUp = BookDatabaseSeekRecord(&recordNum, 1, dmSeekBackward);
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ListTable);
  row = TblGetLastUsableRow(table);
  if (row != -1)
    recordNum = TblGetRowID(table, row);

  // Down if bottom not last record.
  scrollableDown = BookDatabaseSeekRecord(&recordNum, 1, dmSeekForward);

  upIndex = FrmGetObjectIndex(form, ListScrollUpRepeating);
  downIndex = FrmGetObjectIndex(form, ListScrollDownRepeating);
  FrmUpdateScrollers(form, upIndex, downIndex, scrollableUp, scrollableDown);
}

static Boolean ListFormUpdateDisplay(UInt16 updateCode)
{
  TableType *table;
  FormType *form;
  Boolean handled;
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ListTable);
  handled = false;
 
  if (updateCode & frmRedrawUpdateCode) {
    FrmDrawForm(form);
    handled = true;
  }
 
  if (updateCode & UPDATE_FORCE_REDRAW) {
    ListFormLoadTable();
    TblRedrawTable(table);
    handled = true;
  }
 
  if (updateCode & UPDATE_FONT_CHANGED) {
    ListFormOpen(form);
    TblRedrawTable(table);
    if (NO_RECORD != g_CurrentRecord)
      ListFormSelectRecord(g_CurrentRecord);
    handled = true;
  }

  if (updateCode & UPDATE_CATEGORY_CHANGED) {
    ListFormLoadTable();
    TblRedrawTable(table);
    CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ListCategorySelTrigger),
                            BookDatabaseGetCategoryName(g_CurrentCategory));
    handled = true;
  }

  return handled;
}

static void ListFormDrawRecord(MemPtr table, Int16 row, Int16 column,
                               RectangleType *bounds)
{
  FontID oldFont;
  UInt16 recordNum;
  MemHandle recordH;
  BookRecord record;
  Err error;

  oldFont = FntSetFont(g_ListFont);
  
  recordNum = TblGetRowID(table, row);
  error = BookDatabaseGetRecord(recordNum, &recordH, &record);
  if (error) {
    ErrDisplay("Record not found");
    MemHandleUnlock(recordH);
    return;
  }

  switch (column) {
  case COL_TITLE:
    ListFormDrawTitle(&record, bounds);
    break;
  }

  MemHandleUnlock(recordH);
  FntSetFont(oldFont);
}

static void DrawCharsInWidth(char *str, Int16 *width, Int16 *length,
                             Int16 x, Int16 y, Boolean rightJustify)
{ 
  Boolean fitInWidth;
  Int16 maxWidth, ellipsisWidth;

  maxWidth = *width;
  FntCharsInWidth(str, width, length, &fitInWidth);

  if (fitInWidth) {
    // Draw whole string (less trimmed spaces).
    if (rightJustify)
      x += maxWidth - *width;
    WinDrawChars(str, *length, x, y);
  }
  else {
    ellipsisWidth = (FntCharWidth('.') * 3);
    *width = maxWidth - ellipsisWidth;
    FntCharsInWidth(str, width, length, &fitInWidth);
  
    if (rightJustify)
      x += maxWidth - (*width + ellipsisWidth);
    WinDrawChars(str, *length, x, y);

    x += *width;
    WinDrawChars("...", 3, x, y);

    *width += ellipsisWidth;
  }
}

static void ListFormDrawTitle(BookRecord *record, RectangleType *bounds)
{
  Char *str1, *str2;
  UInt16 len1, len2;
  Int16 width, x, y;

  switch (g_ListFields) {
  case KEY_TITLE:
    str1 = record->fields[FIELD_TITLE];
    break;
  case KEY_AUTHOR:
    str1 = record->fields[FIELD_AUTHOR];
    break;
  case KEY_ISBN:
    str1 = record->fields[FIELD_ISBN];
    break;
  case KEY_SUMMARY:
    str1 = record->fields[FIELD_SUMMARY];
    break;
  case KEY_TITLE_AUTHOR:
    str1 = record->fields[FIELD_TITLE];
    str2 = record->fields[FIELD_AUTHOR];
    break;
  case KEY_AUTHOR_TITLE:
    str1 = record->fields[FIELD_AUTHOR];
    str2 = record->fields[FIELD_TITLE];
    break;
  }

  if (NULL != str1)
    len1 = StrLen(str1);
  if (NULL != str2)
    len2 = StrLen(str2);

  width = bounds->extent.x;
  x = bounds->topLeft.x;
  y = bounds->topLeft.y;

  if (NULL == str2) {
    if (NULL == str1) {
      str1 = "[None]";          // TODO: Resource.
      len1 = StrLen(str1);
    }
    DrawCharsInWidth(str1, &width, &len1, x, y, false);
  }
  else {
    if (NULL == str1) {
      DrawCharsInWidth(str2, &width, &len2, x, y, true);
    }
    else {
      width -= SPACE_BETWEEN_FIELDS;
      width = (width * LEFT_FRACTION_NUM) / LEFT_FRACTION_DEN;
      DrawCharsInWidth(str1, &width, &len1, x, y, false);
      x += width + SPACE_BETWEEN_FIELDS;
      width = bounds->extent.x - x;
      DrawCharsInWidth(str2, &width, &len2, x, y, true);
    }
  }
}

static void ListFormSelectRecord(UInt16 recordNum)
{
  FormType *form;
  TableType *table;
  Int16 row, column;
  int i;

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ListTable);

  if (TblGetSelection(table, &row, &column) &&
      (recordNum == TblGetRowID(table, row))) {
    // Currently selected.
    return;
  }

  for (i = 1; i <= 2; i++) {
    if (!TblFindRowID(table, recordNum, &row)) {
      TblSelectItem(table, row, COL_TITLE);
      g_CurrentRecord = recordNum;
      break;
    }
    g_TopVisibleRecord = recordNum;
    ListFormLoadTable();
    TblRedrawTable(table);
  }
}
