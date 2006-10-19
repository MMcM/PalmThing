/*! -*- Mode: C -*-
* Module: EditForm.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants and types ***/

enum { COL_LABEL = 0, COL_DATA };

// Update codes.
#define UPDATE_FORCE_REDRAW 0x01
#define UPDATE_FONT_CHANGED 0x02
#define UPDATE_CATEGORY_CHANGED 0x04

#define EDIT_FIELD_SPACING 2
#define COL_LABEL_MAX_WIDTH 80

/*** Local storage ***/

static FontID g_EditLabelFont = stdFont;
static FontID g_EditBlankFont = stdFont;
static FontID g_EditDataFont = largeBoldFont;

static UInt16 g_ReturnToForm = ListForm;
static UInt16 g_TopFieldNumber = 0, g_CurrentFieldNumber = NO_FIELD;

static UInt8 g_EditFields[] = {
  FIELD_TITLE,
  FIELD_AUTHOR,
  FIELD_DATE,
  FIELD_ISBN,
  FIELD_PUBLICATION,
  FIELD_TAGS,
  FIELD_SUMMARY
};

#define EDIT_NFIELDS sizeof(g_EditFields)

static Char **g_EditLabels = NULL;

/*** Local routines ***/

static Err EditFormGetRecordField(MemPtr table, Int16 row, Int16 column, Boolean edit,
                                  MemHandle *dataH, Int16 *dataOffset, Int16 *dataSize, 
                                  FieldPtr field);
static Boolean EditFormSaveRecordField(MemPtr table, Int16 row, Int16 column);
static void EditFormLoadTable();
static Boolean EditFormUpdateDisplay(UInt16 updateCode);
static Boolean EditFormMenuCommand(UInt16 command);
static void EditFormSelectCategory();
static void EditFormScroll(WinDirectionType direction);
static void EditFormNextField(WinDirectionType direction);
static void EditFormSelectField(UInt16 row, UInt16 column);
static void EditFormResizeField(EventType *event);
static void EditFormSaveRecord();
static UInt16 EditFormComputeLabelWidth();
static void EditBeamRecord(Boolean send);
static void EditFontSelect();
static UInt16 EditFormComputeFieldHeight(TableType *table, UInt16 fieldNumber,
                                         UInt16 columnWidth, UInt16 maxHeight,
                                         BookRecord *record, FontID *fontID);
static void EditFormLoadTableRow(TableType *table, UInt16 row, UInt16 fieldNumber, 
                                 short rowHeight, FontID fontID);
static void EditFormUpdateScrollButtons(FormType *form, UInt16 bottomFieldNumber,
                                        Boolean lastItemClipped);
static void DeleteCurrentRecord(Boolean archive);

static inline void *FrmGetObjectPtrFromID(const FormType *formP, UInt16 objID)
{
  return FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, objID));
}

/*** Setup and event handling ***/

void EditFormSetup(AppPreferences *prefs, BookAppInfo *appInfo)
{
  g_EditLabelFont = FntGlueGetDefaultFontID(defaultSystemFont);
  g_EditBlankFont = FntGlueGetDefaultFontID(defaultSystemFont);
  if (NULL == prefs) {
    g_EditDataFont = FntGlueGetDefaultFontID(defaultLargeFont);
  }
  else {
    g_EditDataFont = prefs->editFont;
  }
}

void EditFormSetdown(AppPreferences *prefs)
{
  prefs->editFont = g_EditDataFont;
}

void EditFormActivate()
{
  g_ReturnToForm = FrmGetActiveFormID();
  FrmGotoForm(EditForm);
}

void EditFormNewRecord()
{
  Err error;
  
  error = BookDatabaseNewRecord(&g_CurrentRecord, NULL);
  if (error) {
    FrmAlert(DeviceFullAlert);
    return;
  }

  BookRecordSetCategory(g_CurrentRecord,
                        (g_CurrentCategory == dmAllCategories) ? 
                        dmUnfiledCategory : g_CurrentCategory);

  EditFormActivate();
}

static void EditFormOpen(FormType *form)
{
  MemHandle labelsH;
  Char *labels, *p, *np;
  UInt16 nlabels, lsize;
  int i;
  Int16 row, nrows;
  UInt16 labelColumnWidth, dataColumnWidth;
  TableType *table;
  RectangleType bounds;
  
  if (NULL == g_EditLabels) {
    labelsH = DmGetResource(strListRscType, EditLabels);
    labels = (Char *)MemHandleLock(labelsH);
    p = labels;
    p += StrLen(p) + 2;             // Skip over unused prefix and zero MSB.
    nlabels = *p++;
    lsize = MemHandleSize(labelsH) - (p - labels);
    g_EditLabels = (Char **)MemPtrNew(sizeof(Char *) * (nlabels + 1) + lsize);
    ErrFatalDisplayIf((NULL == g_EditLabels), "Out of memory");
    np = (Char *)(g_EditLabels + nlabels + 1);
    MemMove(np, p, lsize);
    MemHandleUnlock(labelsH);
    DmReleaseResource(labelsH);
    for (i = 0; i < nlabels; i++) {
      g_EditLabels[i] = np;
      np += StrLen(np) + 1;
    }
    g_EditLabels[nlabels] = NULL;
  }

  table = FrmGetObjectPtrFromID(form, EditTable);
  nrows = TblGetNumberOfRows(table);
  for (row = 0; row < nrows; row++) {
    TblSetItemStyle(table, row, COL_LABEL, labelTableItem);
    TblSetItemStyle(table, row, COL_DATA, textTableItem);
    TblSetRowUsable(table, row, false);
  }

  TblSetColumnUsable(table, COL_LABEL, true);
  TblSetColumnUsable(table, COL_DATA, true);
  TblSetColumnSpacing(table, COL_LABEL, EDIT_FIELD_SPACING);
  
  TblSetLoadDataProcedure(table, COL_DATA, EditFormGetRecordField);
  TblSetSaveDataProcedure(table, COL_DATA, EditFormSaveRecordField);

  TblGetBounds(table, &bounds);
  labelColumnWidth = EditFormComputeLabelWidth();
  dataColumnWidth = bounds.extent.x - EDIT_FIELD_SPACING - labelColumnWidth;
  TblSetColumnWidth(table, COL_LABEL, labelColumnWidth);
  TblSetColumnWidth(table, COL_DATA, dataColumnWidth);

  EditFormLoadTable();

  CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, EditCategorySelTrigger),
                          BookRecordGetCategoryName(g_CurrentRecord));
}

Boolean EditFormHandleEvent(EventType *event)
{
  Boolean handled;
  FormType *form;

  handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    {
      form = FrmGetActiveForm();
      EditFormOpen(form);
      FrmDrawForm(form);
    }
    handled = true;
    break;

  case frmUpdateEvent:
    return EditFormUpdateDisplay(event->data.frmUpdate.updateCode);

  case menuEvent:
    return EditFormMenuCommand(event->data.menu.itemID);
    
  case ctlSelectEvent:
    switch (event->data.ctlSelect.controlID) {
    case EditDoneButton:
      FrmGotoForm(g_ReturnToForm);
      handled = true;
      break;

    case EditNoteButton:
      NoteFormActivate();
      handled = true;
      break;

    case EditCategorySelTrigger:
      EditFormSelectCategory();
      handled = true;
      break;
    }
    break;

  case tblSelectEvent:
    EditFormSelectField(event->data.tblSelect.row, event->data.tblSelect.column);
    break;

  case ctlRepeatEvent:
    switch (event->data.ctlRepeat.controlID) {
    case EditScrollUpRepeating:
      EditFormScroll(winUp);
      break;
     
    case EditScrollDownRepeating:
      EditFormScroll(winDown);
      break;
    }
    break;

  case keyDownEvent:
    if (TxtCharIsHardKey(event->data.keyDown.modifiers,
                         event->data.keyDown.chr)) {
      TblReleaseFocus(FrmGetObjectPtrFromID(FrmGetActiveForm(), EditTable));
      g_TopFieldNumber = 0;
      g_CurrentFieldNumber = NO_FIELD;
      FrmGotoForm(ListForm);
      handled = true;
    }
    else {
      switch (event->data.keyDown.chr) {
      case nextFieldChr:
        EditFormNextField(winDown);
        handled = true;
        break;
     
      case prevFieldChr:
        EditFormNextField(winUp);
        handled = true;
        break;
     
      case pageUpChr:
        EditFormScroll(winUp);
        handled = true;
        break;
    
      case pageDownChr:
        EditFormScroll(winDown);
        handled = true;
        break;
      }
    }
    break;

  case fldHeightChangedEvent:
    EditFormResizeField(event);
    handled = true;
    break;
      
  case frmCloseEvent:
    EditFormSaveRecord();
    if (NULL != g_EditLabels) {
      MemPtrFree(g_EditLabels);
      g_EditLabels = NULL;
    }
    break;

  default:
    break;
  }

  return handled;
}

/*** Commands ***/

static FieldType *GetFocusField()
{
  FormType *form;
  UInt16 focus;
 
  form = FrmGetActiveForm();
  focus = FrmGetFocus(form);
  if (noFocus == focus)
    return NULL;
  
  switch (FrmGetObjectType(form, focus)) {
  case frmTableObj:
    return TblGetCurrentField(FrmGetObjectPtr(form, focus));
  case frmFieldObj:
    return FrmGetObjectPtr(form, focus);
  default:
    return NULL;
  }
}

static Boolean EditFormMenuCommand(UInt16 command)
{
  FieldType *field;
  Boolean handled;
 
  field = GetFocusField();
  handled = false;

  switch (command) {
  case EditUndo:
    if (field) {
      FldUndo(field);
      handled = true;
    }
    break;

  case EditCut:
    if (field) {
      FldCut(field);
      handled = true;
    }
    break;

  case EditCopy:
    if (field) {
      FldCopy(field);
      handled = true;
    }
    break;
   
  case EditPaste:
    if (field) {
      FldPaste(field);
      handled = true;
    }
    break;
   
  case EditSelectAll:
    if (field) {
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

  case RecordBeamBook:
    EditBeamRecord(false);
    handled = true;
    break;

  case RecordSendBook:
    EditBeamRecord(true);
    handled = true;
    break;

  case OptionsAbout:
    AboutFormDisplay();
    handled = true;
    break;

  case OptionsFont:
    EditFontSelect();
    handled = true;
    break;
  }

  return handled;
}

static void EditFontSelect()
{
  FontID newFont;

  if (!SYS_ROM_3_0) return;

  newFont = FontSelect(g_EditDataFont);
  if (newFont != g_EditDataFont) {
    g_EditDataFont = newFont;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FONT_CHANGED);
  }
}

static void EditBeamRecord(Boolean send)
{
  // TODO: Do something.
}

static void EditFormNextField(WinDirectionType direction)
{
  FormType *form;
  TableType *table;
  Int16 row, column;
  UInt16 nextFieldNumber;
  int i;
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
 
  if (!TblEditing(table))
    return;
   
  TblGetSelection(table, &row, &column);
  nextFieldNumber = TblGetRowID(table, row);
  if (winDown == direction) {
    if (nextFieldNumber >= BOOK_NFIELDS-1)
      nextFieldNumber = 0;
    else
      nextFieldNumber++;
    }
  else {
    if (nextFieldNumber == 0)
      nextFieldNumber = BOOK_NFIELDS-1;
    else
      nextFieldNumber--;
  }
  TblReleaseFocus(table);

  g_CurrentFieldNumber = nextFieldNumber;

  for (i = 1; i <= 2; i++) {
    if (TblFindRowID(table, nextFieldNumber, &row)) {
      EditFormSelectField(row, COL_DATA);
      break;
    }
    g_TopFieldNumber = nextFieldNumber;
    EditFormLoadTable();
    TblRedrawTable(table);
  }
}

static void EditFormSelectField(UInt16 row, UInt16 column)
{
  FormType *form;
  TableType *table;  
  FieldType *field;
  UInt16 editFieldNumber;
  Int16 curRow;
  BookRecord record;
  MemHandle recordH;
  FontID oldFont;
  Boolean redraw;

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
  
  // Tap the static label column and cursor moves into data column.
  if (COL_DATA != column) {
    TblReleaseFocus(table);
    TblUnhighlightSelection(table);
  }

  editFieldNumber = TblGetRowID(table, row);
  redraw = false;

  if (((editFieldNumber != g_CurrentFieldNumber) ||
       (NULL == TblGetCurrentField(table))) &&
      (!BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record))) {
    oldFont = FntGetFont();

    if ((NO_FIELD != g_CurrentFieldNumber) &&
        (NULL == record.fields[g_EditFields[g_CurrentFieldNumber]])) {
      // If the old current field is empty, set row height to blank height.
      if (TblFindRowID(table, g_CurrentFieldNumber, &curRow)) {
        FntSetFont(g_EditBlankFont);
        if (FntLineHeight() != TblGetRowHeight(table, curRow)) {
          TblMarkRowInvalid(table, curRow);
          redraw = true;
        }
      }
    }

    g_CurrentFieldNumber = editFieldNumber;

    if (NULL == record.fields[g_EditFields[g_CurrentFieldNumber]]) {
      // If the new current field is empty, set row height to edit height.
      FntSetFont(g_EditDataFont);
      if (FntLineHeight() != TblGetRowHeight(table, row)) {
        TblMarkRowInvalid(table, row);
        redraw = true;
      }
    }
    
    MemHandleUnlock(recordH);

    FntSetFont(oldFont);

    if (redraw) {
      TblReleaseFocus(table);
      EditFormLoadTable();
      TblFindRowID(table, editFieldNumber, &row);
      TblRedrawTable(table);
    }
  }

  if (TblGetCurrentField(table) == NULL) {
    TblGrabFocus(table, row, COL_DATA);
    field = TblGetCurrentField(table);
    FldGrabFocus(field);
    FldMakeFullyVisible(field);
  }
}

static void EditFormScroll(WinDirectionType direction)
{
  FormType *form;
  TableType *table;
  Int16 row;
  UInt16 fieldNumber;
  UInt16 height, columnWidth, tableHeight;
  RectangleType bounds;
  BookRecord record;
  MemHandle recordH;
  FontID fontID;

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
  TblReleaseFocus(table);
 
  TblGetBounds(table, &bounds);
  tableHeight = bounds.extent.y;
  height = 0;
  columnWidth = TblGetColumnWidth(table, COL_DATA);
 
  if (winDown == direction) {
    // Bottom row becomes top unless already top.
    row = TblGetLastUsableRow(table);
    fieldNumber = TblGetRowID(table, row);
  
    if ((row == 0) &&
        (fieldNumber < BOOK_NFIELDS-1))
      fieldNumber++;
  }
  else {
    fieldNumber = TblGetRowID(table, 0);
    if (fieldNumber == 0) 
      // Already at top.
      return;
         
    if (BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record))
      return;

    height = TblGetRowHeight(table, 0);
    if (height >= tableHeight)
      // One row fills whole screen, don't keep it.
      height = 0;                     

    while ((height < tableHeight) && (fieldNumber > 0)) {
      // So long as previous top still fits, add fields before.
      height += EditFormComputeFieldHeight(table, fieldNumber, columnWidth,
                                           tableHeight, &record, &fontID);
      if ((height <= tableHeight) ||
          (fieldNumber == TblGetRowID(table, 0)))
        fieldNumber--;
    }

    MemHandleUnlock(recordH);
  }

  TblMarkTableInvalid(table);
  g_CurrentFieldNumber = NO_FIELD;
  g_TopFieldNumber = fieldNumber;
 
  TblUnhighlightSelection(table);
  EditFormLoadTable();   
  TblRedrawTable(table);
}

static void EditFormSelectCategory()
{
  UInt16 category, ncategory;

  category = BookRecordGetCategory(g_CurrentRecord);
  ncategory = category;
  BookDatabaseSelectCategory(FrmGetActiveForm(), 
                             EditCategorySelTrigger, EditCategoryList, false,
                             &ncategory);
  
  // EditFormRestoreEditState();

  if (category != ncategory) {
    BookRecordSetCategory(g_CurrentRecord, ncategory);
    g_CurrentCategory = ncategory;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_CATEGORY_CHANGED);
  }
}

/*** Display ***/

static void EditFormLoadTable()
{
  FormType *form;
  TableType *table;  
  Int16 row, nrows;
  RectangleType bounds;
  UInt16 tableHeight, columnWidth, dataHeight, lineHeight, height, oldHeight;
  UInt16 pos, oldPos;
  UInt16 fieldNumber, lastFieldNumber;
  FontID fontID, oldFont;
  Boolean rowUsable, rowsInserted, lastItemClipped;
  BookRecord record;
  MemHandle recordH;

  if (BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record))
    return;

  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
  nrows = TblGetNumberOfRows(table);
  TblGetBounds(table, &bounds);
  tableHeight = bounds.extent.y;
  columnWidth = TblGetColumnWidth(table, COL_DATA);

  if (g_CurrentFieldNumber != NO_FIELD) {
    if (g_CurrentFieldNumber < g_TopFieldNumber)
      g_TopFieldNumber = g_CurrentFieldNumber;
  }

  row = 0;
  dataHeight = 0;
  oldPos = pos = 0;
  fieldNumber = g_TopFieldNumber;
  lastFieldNumber = fieldNumber;

  while (fieldNumber < EDIT_NFIELDS) {
    height = EditFormComputeFieldHeight(table, fieldNumber, columnWidth,
                                        tableHeight, &record, &fontID);
    
    oldFont = FntSetFont(fontID);
    lineHeight = FntLineHeight();
    FntSetFont(oldFont);
    
    if (dataHeight + lineHeight <= tableHeight) {
      rowUsable = TblRowUsable(table, row);
      
      if (rowUsable)
        oldHeight = TblGetRowHeight(table, row);
      else
        oldHeight = 0;
      
      if (!rowUsable ||
          (TblGetRowID(table, row) != fieldNumber) ||
          (TblGetItemFont(table, row, COL_DATA) != fontID)) {
        // Everything changed.
        EditFormLoadTableRow(table, row, fieldNumber, height, fontID);
      }
      else if (height != oldHeight) {
        TblSetRowHeight(table, row, height);
        TblMarkRowInvalid(table, row);
      }
      else if (pos != oldPos) {
        TblMarkRowInvalid(table, row);        
      }

      pos += height;
      oldPos += oldHeight;
      lastFieldNumber = fieldNumber;
      fieldNumber++;
      row++;
    }
    
    dataHeight += height;

    if (dataHeight >= tableHeight) {
      // Past the bottom.
      if (g_CurrentFieldNumber == NO_FIELD)
        break;                  // No field open, done.

      if (g_CurrentFieldNumber < fieldNumber)
        break;                  // Open field visible, done.

      if (fieldNumber == lastFieldNumber) {
        if ((fieldNumber == g_TopFieldNumber) ||
            (dataHeight == tableHeight))
          break;                // Only room for this one field, done.
      }
      
      // Start with next top field in hopes of getting current to show.
      g_TopFieldNumber++;
      fieldNumber = g_TopFieldNumber;

      row = 0;
      dataHeight = 0;
      oldPos = pos = 0;
    }
  }

  while (row < nrows) {  
    TblSetRowUsable(table, row, false);
    row++;
  }

  rowsInserted = false;

  // If the table is not full and there are additional fields before
  // the top one, then insert another one before.
  while (dataHeight < tableHeight) {
    fieldNumber = g_TopFieldNumber;
    if (fieldNumber == 0) break;
    fieldNumber--;
    
    height = EditFormComputeFieldHeight(table, fieldNumber, columnWidth, 
                                        tableHeight, &record, &fontID);
    if (dataHeight + height > tableHeight)
      break;                    // One more will not fit.
    
    TblInsertRow(table, 0);
    EditFormLoadTableRow(table, 0, fieldNumber, height, fontID);

    g_TopFieldNumber = fieldNumber;
    dataHeight += height;
    rowsInserted = true;
  }
  
  MemHandleUnlock(recordH);

  if (rowsInserted)
    TblMarkTableInvalid(table);

  lastItemClipped = (dataHeight > tableHeight);
  EditFormUpdateScrollButtons(form, lastFieldNumber, lastItemClipped);
}

static void EditFormLoadTableRow(TableType *table, UInt16 row, UInt16 fieldNumber, 
                                 short rowHeight, FontID fontID)
{
  TblSetRowUsable(table, row, true);
  TblSetRowID(table, row, fieldNumber); 
  TblSetRowHeight(table, row, rowHeight);
  TblSetItemFont(table, row, COL_LABEL, g_EditLabelFont);
  TblSetItemPtr(table, row, COL_LABEL, g_EditLabels[fieldNumber]);
  TblSetItemFont(table, row, COL_DATA, fontID);
  TblMarkRowInvalid(table, row);
}

static UInt16 EditFormComputeLabelWidth()
{
  FontID oldFont;
  UInt16 width, lwidth;
  int i;
  const Char *label;
  
  oldFont = FntSetFont(g_EditLabelFont);

  width = 0;
  for (i = 0; i < EDIT_NFIELDS; i++) {
    label = g_EditLabels[i];
    lwidth = FntCharsWidth(label, StrLen(label));
    if (lwidth > width)
      width = lwidth;
  }

  width += FntCharWidth(':');

  if (width > COL_LABEL_MAX_WIDTH)
    width = COL_LABEL_MAX_WIDTH;

  FntSetFont(oldFont);

  return width;
}

static UInt16 EditFormComputeFieldHeight(TableType *table, UInt16 fieldNumber,
                                         UInt16 columnWidth, UInt16 maxHeight,
                                         BookRecord *record, FontID *fontID)
{
  Int16 row, column;
  UInt16 lineHeight, height;
  FieldType *field;
  FontID oldFont;
  Char *text;
  Boolean unlock;

  text = record->fields[g_EditFields[fieldNumber]];
  unlock = false;
  if (TblEditing(table)) {
    // If field in question is open in editor, get that buffer instead.
    TblGetSelection(table, &row, &column);
    if (fieldNumber == TblGetRowID(table, row)) {
      field = TblGetCurrentField(table);
      text = FldGetTextPtr(field);
      unlock = true;
    }
  }

  if (((NULL != text) && ('\0' != *text)) ||
      (g_CurrentFieldNumber == fieldNumber) ||
      (g_EditDataFont == g_EditBlankFont)) {
    // Not blank, or current field, or fonts are the same.
    *fontID = g_EditDataFont;
    oldFont = FntSetFont(*fontID);
  }
  else {
    oldFont = FntSetFont(g_EditBlankFont);
    lineHeight = FntLineHeight();
  
    FntSetFont(g_EditDataFont);
    if (lineHeight == FntLineHeight())
      *fontID = g_EditDataFont;
    else {
      // Blank and different font height: use that font.
      *fontID = g_EditBlankFont;
      FntSetFont(g_EditBlankFont);
    }
  }
    
  lineHeight = FntLineHeight();
  if ((NULL == text) || ('\0' == *text))
    height = lineHeight;
  else {
    height = FldCalcFieldHeight(text, columnWidth);
    maxHeight /= lineHeight;
    if (height > maxHeight)
      height = maxHeight;
    height *= lineHeight;
  }
  
  FntSetFont(oldFont);
  
  return height;
}

static void EditFormResizeField(EventType *event)
{
  FormType *form;
  TableType *table;
  FieldType *field;  
  RectangleType bounds, ibounds;
  Int16 row, column;
  UInt16 lastRow, lastFieldNumber, rowID, pos;
  Boolean lastItemClipped, restoreFocus;

  // Get height before action.
  field = event->data.fldHeightChanged.pField;
  FldGetBounds(field, &bounds);

  // Have the table handle basic resize.
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
  TblHandleEvent(table, event);
  
  if (event->data.fldHeightChanged.newHeight >= bounds.extent.y) {
    // Expanding: all that's left to do is update scroll indicators in
    // case this is the first time.
    g_TopFieldNumber = TblGetRowID(table, 0);
    lastRow = TblGetLastUsableRow(table);
    lastFieldNumber = TblGetRowID(table, lastRow);
    TblGetBounds(table, &bounds);
    TblGetItemBounds(table, lastRow, COL_DATA, &ibounds);
    lastItemClipped = (ibounds.topLeft.y + ibounds.extent.y > 
                       bounds.topLeft.y + bounds.extent.y);
    EditFormUpdateScrollButtons(FrmGetActiveForm(), lastFieldNumber, lastItemClipped);
    return;
  }

  // Shrinking, may be able to expose more items or even undo a scroll.
  pos = rowID = 0;
  restoreFocus = false;
  
  if (TblGetRowID(table, 0) > 0) {
    // Removing focus will cause save which will cause unscroll if possible.
    TblGetSelection(table, &row, &column);
    rowID = TblGetRowID(table, row);
    field = TblGetCurrentField(table);
    pos = FldGetInsPtPosition(field);
    TblReleaseFocus(table);
    restoreFocus = true;
  }
  
  EditFormLoadTable();
  TblRedrawTable(table);

  if (restoreFocus) {
    TblFindRowID(table, rowID, &row);
    TblGrabFocus(table, row, column);
    FldSetInsPtPosition(field, pos);
    FldGrabFocus(field);
  }
}

static void EditFormUpdateScrollButtons(FormType *form, UInt16 bottomFieldNumber,
                                        Boolean lastItemClipped)
{
  Boolean scrollableUp, scrollableDown;
  UInt16 upIndex, downIndex;

  scrollableUp = (g_TopFieldNumber > 0);
  scrollableDown = (lastItemClipped || (bottomFieldNumber < EDIT_NFIELDS-1));
  upIndex = FrmGetObjectIndex(form, EditScrollUpRepeating);
  downIndex = FrmGetObjectIndex(form, EditScrollDownRepeating);
  FrmUpdateScrollers(form, upIndex, downIndex, scrollableUp, scrollableDown);
}

static Boolean EditFormUpdateDisplay(UInt16 updateCode)
{
  FormType *form;
  TableType *table;
  Boolean handled;
 
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
  handled = false;
 
  if (updateCode & frmRedrawUpdateCode) {
    FrmDrawForm(form);
    handled = true;
  }
 
  if (updateCode & UPDATE_FORCE_REDRAW) {
    EditFormLoadTable();
    TblRedrawTable(table);
    handled = true;
  }
 
  if (updateCode & UPDATE_FONT_CHANGED) {
    TblReleaseFocus(table);
    EditFormOpen(form);
    TblRedrawTable(table);
    //EditFormRestoreEditState();
    handled = true;
  }

  if (updateCode & UPDATE_CATEGORY_CHANGED) {
    CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, EditCategorySelTrigger),
                            BookRecordGetCategoryName(g_CurrentRecord));
    handled = true;
  }

  return handled;
}

static inline void FldSetAutoShift(FieldType *field)
{
  FieldAttrType attr;
  
  FldGetAttributes(field, &attr);
  attr.autoShift = true;
  FldSetAttributes(field, &attr);
}

static Err EditFormGetRecordField(MemPtr table, Int16 row, Int16 column, Boolean edit,
                                  MemHandle *dataH, Int16 *dataOffset, Int16 *dataSize, 
                                  FieldPtr field)
{
  UInt16 editFieldNumber, recordFieldIndex;
  MemHandle textH, ntextH;
  UInt16 textOffset, textLen;
  Char *text, *ntext;

  editFieldNumber = TblGetRowID(table, row);
  recordFieldIndex = g_EditFields[editFieldNumber];

  if (BookRecordGetField(g_CurrentRecord, recordFieldIndex, 
                         &textH, &textOffset, &textLen)) {
    textH = NULL;
    textOffset = textLen = 0;
  }

  if (edit) {
    switch (recordFieldIndex) {
    case FIELD_TITLE:
    case FIELD_AUTHOR:
    case FIELD_PUBLICATION:
    case FIELD_SUMMARY:
      // TODO: Think about this some more.
      FldSetAutoShift(field);
      break;
    }
    // Cannot edit in place in the database record because adjustments
    // would invalidate state fields' offsets into same handle.
    if (textLen > 1) {
      ntextH = MemHandleNew(textLen);
      if (NULL != ntextH) {
        ntext = MemHandleLock(ntextH);
        text = MemHandleLock(textH);
        MemMove(ntext, text + textOffset, textLen);
        MemHandleUnlock(textH);
        MemHandleUnlock(ntextH);
        *dataH = ntextH;
        *dataOffset = 0;
        *dataSize = textLen;
        return errNone;
      }
    }
    // Handle will be created as necessary.
    *dataH = NULL;
    *dataOffset = *dataSize = 0;
  }
  else {
    *dataH = textH;
    *dataOffset = textOffset;
    *dataSize = textLen;
  }
  return errNone;
}

static Boolean EditFormSaveRecordField(MemPtr table, Int16 row, Int16 column)
{
  FieldType *field;
  MemHandle textH;
  UInt16 editFieldNumber, recordFieldIndex;
  MemHandle recordH;
  BookRecord record;
  Char *text;
  UInt16 size;
  Int16 nrows;
  Boolean redraw;
  Err error;

  editFieldNumber = TblGetRowID(table, row);
  recordFieldIndex = g_EditFields[editFieldNumber];

  field = TblGetCurrentField(table);

  FldSetSelection(field, 0, 0);

  redraw = false;
  if (FldDirty(field)) {
    error = BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record);
    if (!error) {
      text = FldGetTextPtr(field);
      if ((NULL == text) || ('\0' == *text))
        record.fields[recordFieldIndex] = NULL;
      else
        record.fields[recordFieldIndex] = text;
      error = BookDatabaseSaveRecord(&g_CurrentRecord, &recordH, &record);
      if (NULL != text)
        MemPtrUnlock(text);
      if (NULL != recordH)
        MemHandleUnlock(recordH);

      if (error) {
        // Save did not happen.  Before telling the user, discard their edit.
        if (!BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record)) {
          textH = FldGetTextHandle(field);
          if (NULL != textH) {
            if (NULL == record.fields[recordFieldIndex])
              size = 1;
            else
              size = StrLen(record.fields[recordFieldIndex]) + 1;
            if (!MemHandleResize(textH, size)) {
              text = MemHandleLock(textH);
              if (size > 1)
                StrCopy(text, record.fields[recordFieldIndex]);
              else
                *text = '\0';
              MemHandleUnlock(textH);
            }
            // Force update of field.
            FldSetTextHandle(field, NULL);
            FldSetTextHandle(field, textH);
          }
          MemHandleUnlock(recordH);
        }
        FrmAlert(DeviceFullAlert);
        
        // Height of unsaved field may have changed, so reload below one that failed.
        nrows = TblGetNumberOfRows(table);
        while (row < nrows) {
          TblSetRowUsable(table, row, false);
          row++;
        }
        EditFormLoadTable();
        
        redraw = true;
      }
    }
  }

  FldFreeMemory(field);
  return redraw;
}

static void EditFormSaveRecord()
{
  FormType *form;
  TableType *table;
  MemHandle recordH;
  BookRecord record;
  Boolean empty;
  Err error;
  
  // This will invoke EditFormSaveRecordField as necessary.
  form = FrmGetActiveForm();
  table = FrmGetObjectPtrFromID(form, EditTable);
  TblReleaseFocus(table);

  error = BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record);
  if (error) return;

  empty = BookRecordIsEmpty(&record);
  MemHandleUnlock(recordH);
  if (empty) {
    DeleteCurrentRecord(false);
    return;
  }

  // Something may have changed that makes the current record not
  // satisfy category or filters.  Try to find one that does.
  if (!BookDatabaseSeekRecord(&g_CurrentRecord, 0, dmSeekBackward) &&
      !BookDatabaseSeekRecord(&g_CurrentRecord, 0, dmSeekForward))
    g_CurrentRecord = NO_RECORD;
}

static void DeleteCurrentRecord(Boolean archive)
{
  UInt16 newCurrentRecord;
  Boolean selectedForward;

  newCurrentRecord = g_CurrentRecord;
  selectedForward = false;
  if (!BookDatabaseSeekRecord(&newCurrentRecord, 0, dmSeekBackward)) {
    if (BookDatabaseSeekRecord(&newCurrentRecord, 0, dmSeekForward))
      selectedForward = true;
    else
      newCurrentRecord = NO_RECORD;
  }
  
  if (BookDatabaseDeleteRecord(&g_CurrentRecord, archive))
    return;

  if (selectedForward)
    newCurrentRecord--;
  g_CurrentRecord = newCurrentRecord;
}
