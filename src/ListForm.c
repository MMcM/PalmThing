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
#define UPDATE_FIND_TYPE_CHANGED 0x08

#define SPACE_BETWEEN_FIELDS 4
#define LEFT_FRACTION_NUM 3
#define LEFT_FRACTION_DEN 4

/*** Local storage ***/

static UInt16 g_TopVisibleRecord = 0;
static FontID g_ListFont = stdFont;
static UInt16 g_ListFields = KEY_TITLE_AUTHOR;
static Boolean g_IncrementalFind = true;
static BookFindState *g_FindState = NULL;

/*** Local routines ***/

static void ListFormDrawRecord(MemPtr table, Int16 row, Int16 column,
                               RectangleType *bounds);
static void ListFormLoadTable();
static Boolean ListFormMenuCommand(UInt16 command);
static void ListFormItemSelected(EventType *event);
static void ListFormSelectCategory();
static void ListFormFindTypeSelected(EventType *event);
static void ListFormScroll(WinDirectionType direction, UInt16 amount, Boolean byPage);
static Boolean ListFormUpdateDisplay(UInt16 updateCode);
static void ListBeamCategory(Boolean send);
static void ListFontSelect();
static void ListFormSelectRecord(UInt16 recordNum);
static void ListFormFieldChanged();
static void ListFormFindStart();
static void ListFormFindCleanup();
static void ListFormFind();
static void ListFormDrawTitle(BookRecord *record, RectangleType *bounds);

static inline void *FrmGetObjectPtrFromID(const FormType *formP, UInt16 objID)
{
  return FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, objID));
}

/*** Setup and event handling ***/

void ListFormSetup(AppPreferences *prefs, BookAppInfo *appInfo)
{
  if (NULL == prefs) {
    g_ListFont = FntGlueGetDefaultFontID(defaultSystemFont);
    g_ListFields = (appInfo->sortFields < 0) ? 
      -appInfo->sortFields : appInfo->sortFields;
  }
  else {
    g_ListFont = prefs->listFont;
    g_ListFields = prefs->listFields;
    g_IncrementalFind = prefs->incrementalFind;
  }
}

void ListFormSetdown(AppPreferences *prefs)
{
  prefs->listFont = g_ListFont;
  prefs->listFields = g_ListFields;
  prefs->incrementalFind = g_IncrementalFind;
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

  CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ListCategoryPopTrigger), 
                          BookDatabaseGetCategoryName(g_CurrentCategory));

  FntSetFont(oldFont);
}

Boolean ListFormHandleEvent(EventType *event)
{
  FormType *form;
  FieldType *field;
  UInt16 index;
  Boolean handled;

  handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    form = FrmGetActiveForm();
    ListFormOpen(form);
    FrmDrawForm(form);
      
    if (NO_RECORD != g_CurrentRecord)
      ListFormSelectRecord(g_CurrentRecord);
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
    case ListCategoryPopTrigger:
      ListFormSelectCategory();
      handled = true;
      break;

    case ListNewButton:
      EditFormNewRecord();
      handled = true;
      break;

    case ListFindClearButton:
      form = FrmGetActiveForm();
      index = FrmGetObjectIndex(form, ListFindTextField);
      field = FrmGetObjectPtr(form, index);
      if (noFocus == FrmGetFocus(form)) {
        FrmSetFocus(form, index);
      }
      FldDelete(field, 0, FldGetTextLength(field));
      // Sends fldChangedEvent, so no need for ListFormFieldChanged() here.
      handled = true;
      break;

    case ListFindButton:
      ListFormFind();
      handled = true;
      break;
    }
    break;
  
  case popSelectEvent:
    switch (event->data.popSelect.controlID) {
    case ListFindTypePopTrigger:
      ListFormFindTypeSelected(event);
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
      // This is the main form, so no handling.
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
        form = FrmGetActiveForm();
        if (noFocus != FrmGetFocus(form)) {
          FrmSetFocus(form, noFocus);
          ListFormFind();
        }
        else if (NO_RECORD != g_CurrentRecord) {
          ViewFormActivate();
        }
        handled = true;
        break;
     
      default:
        if (TxtGlueCharIsPrint(event->data.keyDown.chr)) {
          form = FrmGetActiveForm();
          index = FrmGetObjectIndex(form, ListFindTextField);
          field = FrmGetObjectPtr(form, index);
          if (noFocus == FrmGetFocus(form)) {
            FrmSetFocus(form, index);
          }
          FldHandleEvent(field, event);
          ListFormFieldChanged();
          handled = true;
        }
        break;
      }
    }
    break;

  case fldChangedEvent:
    ListFormFieldChanged();
    break;

  case nilEvent:
    break;

  default:
    break;
  }

  return handled;
}

/*** Commands ***/

static Boolean ListFormMenuCommand(UInt16 command)
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

  case RecordBeamCategory:
    ListBeamCategory(false);
    handled = true;
    break;

  case RecordSendCategory:
    ListBeamCategory(true);
    handled = true;
    break;
        
  case RecordAddISBNs:
    ISBNFormActivate();
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

  case OptionsPreferences:
    FrmPopupForm(PreferencesForm);
    handled = true;
    break;
  }

  return handled;
}

static void ListFontSelect()
{
  FontID newFont;

  if (!SYS_ROM_3_0) return;

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
 
  ListFormFindStart();

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, ListTable);
  rowsPerPage = ListFormNumberOfRows(table) - 1;
  newTopVisibleRecord = g_TopVisibleRecord;

  if (byPage) {
    amount *= rowsPerPage;
  }

  if (direction == winDown) {
    // Forward n or last page.
    if (!BookDatabaseSeekRecord(&newTopVisibleRecord, amount, dmSeekForward,
                                g_CurrentCategory, g_FindState)) {
      newTopVisibleRecord = dmMaxRecordIndex;        
      if (byPage) {
        if (!BookDatabaseSeekRecord(&newTopVisibleRecord, rowsPerPage, dmSeekBackward,
                                    g_CurrentCategory, g_FindState)) {
          newTopVisibleRecord = 0;
          BookDatabaseSeekRecord(&newTopVisibleRecord, 0, dmSeekForward,
                                 g_CurrentCategory, g_FindState);
        }
      }
      else {
        BookDatabaseSeekRecord(&newTopVisibleRecord, 1, dmSeekBackward,
                               g_CurrentCategory, g_FindState);
      }
    }
  }
  else {
    // Backward n or top.
    if (!BookDatabaseSeekRecord(&newTopVisibleRecord, amount, dmSeekBackward,
                                g_CurrentCategory, g_FindState)) {
      newTopVisibleRecord = 0;
      BookDatabaseSeekRecord(&newTopVisibleRecord, 0, dmSeekForward,
                             g_CurrentCategory, g_FindState);
    }
  }

  if (g_TopVisibleRecord != newTopVisibleRecord) {
    g_TopVisibleRecord = newTopVisibleRecord;
    ListFormLoadTable();
    TblRedrawTable(table);
  }

  ListFormFindCleanup();
}

static void ListFormSelectCategory()
{
  FormType *form;
  UInt16 category;

  form = FrmGetActiveForm();

  category = g_CurrentCategory;
  BookDatabaseSelectCategory(form, 
                             ListCategoryPopTrigger, ListCategoryList, true,
                             &category);
  
  if (category != g_CurrentCategory) {
    g_CurrentCategory = category;
    g_CurrentRecord = NO_RECORD;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_CATEGORY_CHANGED);
  }
}

static void ListFormFindTypeSelected(EventType *event)
{
  Char *label, *selection;
  UInt16 len;

  label = (Char *)CtlGetLabel(event->data.popSelect.controlP);
  selection = LstGetSelectionText(event->data.popSelect.listP,
                                  event->data.popSelect.selection);
  len = StrChr(selection, ':') + 1 - selection;
  MemMove(label, selection, len);
  label[len] = '\0';
  CtlSetLabel(event->data.popSelect.controlP, label);

  if (event->data.popSelect.selection != event->data.popSelect.priorSelection) {
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FIND_TYPE_CHANGED);
  }
}

static void ListFormFieldChanged()
{

}

static void ListFormFind()
{
  FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FORCE_REDRAW);
}

static void ListFormFindStart()
{
  Char *key;
  FormType *form;
  FieldType *field;
  ListType *list;

  if (NULL != g_FindState)
    return;

  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, ListFindTextField);
  key = FldGetTextPtr(field);
  if ((NULL == key) || ('\0' == *key))
    return;

  g_FindState = MemPtrNew(sizeof(BookFindState));
  if (NULL == g_FindState)
    return;

  list = FrmGetObjectPtrFromID(form, ListFindTypeList);
  g_FindState->findType = LstGetSelection(list) + 1;
  g_FindState->findKey = key;
  g_FindState->keyPrep = NULL;
}

static void ListFormFindCleanup()
{
  if (NULL != g_FindState) {
    if (NULL != g_FindState->keyPrep)
      MemPtrFree(g_FindState->keyPrep);
    MemPtrFree(g_FindState);
    g_FindState = NULL;
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
  Boolean scrollableUp, scrollableDown;
  UInt16 upIndex, downIndex;

  ListFormFindStart();

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
    if (!BookDatabaseSeekRecord(&recordNum, (row > 0) ? 1 : 0, dmSeekForward,
                                g_CurrentCategory, g_FindState))
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

  recordNum = g_TopVisibleRecord;
  // Up if top not first record.
  scrollableUp = BookDatabaseSeekRecord(&recordNum, 1, dmSeekBackward,
                                        g_CurrentCategory, g_FindState);
 
  row = TblGetLastUsableRow(table);
  if (row != -1)
    recordNum = TblGetRowID(table, row);

  // Down if bottom not last record.
  scrollableDown = BookDatabaseSeekRecord(&recordNum, 1, dmSeekForward,
                                          g_CurrentCategory, g_FindState);

  upIndex = FrmGetObjectIndex(form, ListScrollUpRepeating);
  downIndex = FrmGetObjectIndex(form, ListScrollDownRepeating);
  FrmUpdateScrollers(form, upIndex, downIndex, scrollableUp, scrollableDown);

  ListFormFindCleanup();
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
    CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ListCategoryPopTrigger),
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

/** Like WinDrawTruncChars, but with support for right justification
 * and returning width consumed so that any left over can be used for
 * another field. */
static void DrawTruncChars(char *str, Int16 *length, Int16 x, Int16 y, Int16 *width, 
                           Boolean rightJustify)
{ 
  Int16 fullLength, maxWidth;
  Boolean fitInWidth;

  fullLength = *length;
  maxWidth = *width;
  FntCharsInWidth(str, width, length, &fitInWidth);

  if (fitInWidth) {
    // Draw whole string (less trimmed spaces).
    if (rightJustify)
      x += maxWidth - *width;
    WinDrawChars(str, *length, x, y);
  }
  else {
    // Claim whole space and draw with ellipsis.
    *width = maxWidth;
    WinGlueDrawTruncChars(str, fullLength, x, y, maxWidth);
  }
}

static void ListFormDrawTitle(BookRecord *record, RectangleType *bounds)
{
  Char *str1, *str2;
  UInt16 len1, len2;
  Int16 width, x, y;
  DmResID noneID;
  MemHandle noneH;

  str1 = str2 = NULL;
  len1 = len2 = 0;
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
    noneH = NULL;
    if (NULL == str1) {
      switch (g_ListFields) {
      case KEY_TITLE:
      case KEY_TITLE_AUTHOR:
      case KEY_AUTHOR_TITLE:
        noneID = ListNoTitle;
        break;
      default:
        noneID = ListNone;
        break;
      }
      noneH = DmGetResource(strRsc, noneID);
      str1 = (Char *)MemHandleLock(noneH);
      len1 = StrLen(str1);
    }
    WinGlueDrawTruncChars(str1, len1, x, y, width);
    if (NULL != noneH) {
      MemHandleUnlock(noneH);
      DmReleaseResource(noneH);
    }
  }
  else {
    if (NULL == str1) {
      DrawTruncChars(str2, &len2, x, y, &width, true);
    }
    else {
      width -= SPACE_BETWEEN_FIELDS;
      width = (width * LEFT_FRACTION_NUM) / LEFT_FRACTION_DEN;
      DrawTruncChars(str1, &len1, x, y, &width, false);
      x += width + SPACE_BETWEEN_FIELDS;
      width = bounds->extent.x - x;
      DrawTruncChars(str2, &len2, x, y, &width, true);
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
    if (TblFindRowID(table, recordNum, &row)) {
      TblSelectItem(table, row, COL_TITLE);
      g_CurrentRecord = recordNum;
      break;
    }
    g_TopVisibleRecord = recordNum;
    ListFormLoadTable();
    TblRedrawTable(table);
  }
}

/*** Preferences ***/

extern Boolean g_ViewSummary;

static void PreferencesFormOpen(FormType *form)
{
  ListType *list;
  ControlType *ctl;
  Char *label;
  Int16 sortFields;
  Boolean reverse;

  sortFields = BookDatabaseGetSortFields();
  if (sortFields < 0) {
    sortFields = -sortFields;
    reverse = true;
  }
  else
    reverse = false;

  // We arrange for the initial label to be the longest possible, then
  // cast off the const and overwrite to current selection.  I think
  // this avoids trouble should the list texts get relocated.
  list = FrmGetObjectPtrFromID(form, PreferencesSortList);
  LstSetSelection(list, sortFields-1);
  ctl = FrmGetObjectPtrFromID(form, PreferencesSortPopTrigger);
  label = (Char *)CtlGetLabel(ctl);
  StrCopy(label, LstGetSelectionText(list, LstGetSelection(list)));
  CtlSetLabel(ctl, label);
  ctl = FrmGetObjectPtrFromID(form, PreferencesSortReverse);
  CtlSetValue(ctl, reverse);
  
  list = FrmGetObjectPtrFromID(form, PreferencesListList);
  LstSetSelection(list, g_ListFields-1);
  ctl = FrmGetObjectPtrFromID(form, PreferencesListPopTrigger);
  label = (Char *)CtlGetLabel(ctl);
  StrCopy(label, LstGetSelectionText(list, LstGetSelection(list)));
  CtlSetLabel(ctl, label);

  ctl = FrmGetObjectPtrFromID(form, PreferencesIncrementalCheckbox);
  CtlSetValue(ctl, g_IncrementalFind);
  
  FrmSetControlGroupSelection(form, PreferencesViewGroup,
                              (g_ViewSummary) ? PrefsViewSummaryPushButton :
                                                PrefsViewTitleAuthorPushButton);
}

static void PreferencesFormSave()
{
  FormType *form, *sortForm;
  ListType *list;
  ControlType *ctl;
  Int16 sortFields, newSortFields;
  UInt16 newListFields;
  Boolean forceRedraw;

  sortFields = BookDatabaseGetSortFields();
  forceRedraw = false;

  form = FrmGetActiveForm();

  list = FrmGetObjectPtrFromID(form, PreferencesSortList);
  ctl = FrmGetObjectPtrFromID(form, PreferencesSortReverse);
  newSortFields = LstGetSelection(list) + 1;
  if (CtlGetValue(ctl))
    newSortFields = -newSortFields;
  if (sortFields != newSortFields) {
    g_TopVisibleRecord = 0;
    g_CurrentRecord = NO_RECORD;

    sortForm = FrmInitForm(SortForm);
    FrmSetActiveForm(sortForm);
    FrmDrawForm(sortForm);
    
    BookDatabaseSetSortFields(newSortFields);
    
    FrmEraseForm(sortForm);
    FrmDeleteForm(sortForm);
    FrmSetActiveForm(form);

    forceRedraw = true;
  }

  list = FrmGetObjectPtrFromID(form, PreferencesListList);
  newListFields = LstGetSelection(list) + 1;
  if (g_ListFields != newListFields) {
    g_ListFields = newListFields;
    forceRedraw = true;
  }

  ctl = FrmGetObjectPtrFromID(form, PreferencesIncrementalCheckbox);
  g_IncrementalFind = CtlGetValue(ctl);
  
  g_ViewSummary = 
    (FrmGetObjectId(form, FrmGetControlGroupSelection(form, PreferencesViewGroup)) == 
     PrefsViewSummaryPushButton);

  if (forceRedraw)
    FrmUpdateForm(ListForm, UPDATE_FORCE_REDRAW);
}

Boolean PreferencesFormHandleEvent(EventType *event)
{
  FormType *form;
  Boolean handled;

  handled = false;
  switch (event->eType) {
    case frmOpenEvent:
      form = FrmGetActiveForm();
      PreferencesFormOpen(form);
      FrmDrawForm(form);
      handled = true;
      break;

    case ctlSelectEvent:
      switch (event->data.ctlSelect.controlID) {
      case PreferencesOKButton:
      case PreferencesCancelButton:
        form = FrmGetActiveForm();
        if (PreferencesOKButton == event->data.ctlSelect.controlID)
          PreferencesFormSave(form);
        FrmEraseForm(form);
        FrmDeleteForm(form);
#if 0
        FrmReturnToForm(0);
#else
        FrmSetActiveForm(FrmGetFirstForm());
#endif
        handled = true;
        break;
      }
      break;
  
    default:
      break;
    }
 
  return handled;
}

