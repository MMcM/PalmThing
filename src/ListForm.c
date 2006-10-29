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

enum {
  REDISPLAY_NONE, REDISPLAY_BEGIN,
  REDISPLAY_SCROLL_FORWARD, REDISPLAY_SCROLL_BACKWARD, 
  REDISPLAY_FIRST_PAGE, REDISPLAY_LAST_PAGE, 
  REDISPLAY_FILL_TOP, REDISPLAY_FILL_NEXT, 
  REDISPLAY_UP_ARROW
};

typedef struct {
  BookSeekState seekState;
  UInt16 action;
  MemHandle keyHandle;
  UInt16 cacheFillPointer;
  UInt16 recordCache[0];
} BookFindState;

/*** Local storage ***/

static UInt16 g_TopVisibleRecord = 0;
static FontID g_ListFont = stdFont;
static UInt16 g_ListFields = KEY_TITLE_AUTHOR;
static Boolean g_IncrementalFind = true; // TODO: Turn back off.
static BookFindState *g_FindState = NULL;
static Boolean g_ScrollCurrentIntoView = false;

/*** Local routines ***/

static void ListFormDrawRecord(MemPtr table, Int16 row, Int16 column,
                               RectangleType *bounds);
static void ListFormRedisplay(UInt16 action, Boolean checkCache);
static Boolean ListFormMenuCommand(UInt16 command);
static void ListFormItemSelected(EventType *event);
static void ListFormSelectCategory();
static void ListFormFindTypeSelected(EventType *event);
static void ListFormScroll(WinDirectionType direction);
static Boolean ListFormUpdateDisplay(UInt16 updateCode);
static void ListBeamCategory(Boolean send);
static void ListFontSelect();
static void ListFormFieldChanged();
static void ListFormFind();
static void ListFormUpdateFindState();
static void ListFormCloseFindState();
static void ListFormDeleteFindState();

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

  ListFormDeleteFindState();
}

static void ReplaceGraphicalButton(FormType *form, UInt16 newID, UInt16 oldID)
{
  UInt16 index;

  index = FrmGetObjectIndex(form, newID);

  FrmHideObject(form, index);

  index = FrmGetObjectIndex(form, oldID);

  FrmShowObject(form, index-1);
  FrmShowObject(form, index);
}

void ListFormDowngrade(FormType *form)
{
  if (!SYS_ROM_3_5) {
    ReplaceGraphicalButton(form, ListFindClearButton, ListFindClear30Button);
    ReplaceGraphicalButton(form, ListFindButton, ListFind30Button);
  }
}

static void ListFormOpen(FormType *form)
{
  TableType *table;
  FieldType *field;
  ListType *list;
  ControlType *ctl;
  RectangleType tableBounds;
  Int16 row, nrows;
  Int16 extraWidth;
  Char *label, *selection;
  UInt16 len;
  Boolean checkCache;

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

  if (NULL != g_FindState) {
    // Restore previous Find filter.
    ctl = FrmGetObjectPtrFromID(form, ListFindTypePopTrigger);
    list = FrmGetObjectPtrFromID(form, ListFindTypeList);
    LstSetSelection(list, g_FindState->seekState.filter.findType - 1);
    label = (Char *)CtlGetLabel(ctl);
    selection = LstGetSelectionText(list, LstGetSelection(list));
    len = StrChr(selection, ':') + 1 - selection;
    MemMove(label, selection, len);
    label[len] = '\0';
    CtlSetLabel(ctl, label);
    field = FrmGetObjectPtrFromID(form, ListFindTextField);
    FldSetTextHandle(field, g_FindState->keyHandle);
    g_FindState->seekState.filter.findKey = FldGetTextPtr(field);
  }

  checkCache = true;

  if (g_CurrentRecordEdited) {
    g_CurrentRecordEdited = false;
    checkCache = false;
    if (NO_RECORD != g_CurrentRecord) {
      if ((dmAllCategories != g_CurrentCategory) &&
          (BookRecordGetCategory(g_CurrentRecord) != g_CurrentCategory)) {
        // The current record was edited to no longer match the current category.
        // (The category is usually changed in this case, but be safe.)
        g_CurrentRecord = NO_RECORD;
      }
      else if (NULL != g_FindState) {
        if (!BookRecordFilterMatch(g_CurrentRecord, &g_FindState->seekState.filter)) {
          // The current record was edited to no longer match the filter.
          g_CurrentRecord = NO_RECORD;
        }
      }
    }
  }

  CategorySetTriggerLabel(FrmGetObjectPtrFromID(form, ListCategoryPopTrigger), 
                          BookDatabaseGetCategoryName(g_CurrentCategory));

  FrmDrawForm(form);

  if (NO_RECORD != g_CurrentRecord)
    g_ScrollCurrentIntoView = true;
  ListFormRedisplay(REDISPLAY_BEGIN, checkCache);
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
    case ListFindClear30Button:
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
    case ListFind30Button:
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
      ListFormScroll(winUp);
      // Leave unhandled so button can repeat.
      break;
    
    case ListScrollDownRepeating:
      ListFormScroll(winDown);
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
        ListFormScroll(winUp);
        handled = true;
        break;
     
      case pageDownChr:
        ListFormScroll(winDown);
        handled = true;
        break;
     
      case linefeedChr:
        form = FrmGetActiveForm();
        if (noFocus != FrmGetFocus(form)) {
          FrmSetFocus(form, noFocus);
          ListFormFind();
        }
        else if (NO_RECORD != g_CurrentRecord) {
          ViewFormActivate((NULL != g_FindState) ? &g_FindState->seekState.filter : NULL);
        }
        handled = true;
        break;
     
      default:
        if (!TxtGlueCharIsPrint(event->data.keyDown.chr)) 
          break;
        /* else falls through */
      case backspaceChr:
        form = FrmGetActiveForm();
        index = FrmGetObjectIndex(form, ListFindTextField);
        field = FrmGetObjectPtr(form, index);
        if (noFocus == FrmGetFocus(form)) {
          FrmSetFocus(form, index);
        }
        FldHandleEvent(field, event);
        ListFormFieldChanged();
        handled = true;
        break;
      }
    }
    break;

  case fldChangedEvent:
    ListFormFieldChanged();
    break;

  case frmCloseEvent:
    ListFormCloseFindState();
    break;

  case nilEvent:
    ListFormRedisplay(REDISPLAY_NONE, false);
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
    ViewFormActivate((NULL != g_FindState) ? &g_FindState->seekState.filter : NULL);
    break;
  }
}

static UInt16 ListFormNumberOfRows(TableType *table)
{
  Int16 rows, nrows;
  UInt16 tableHeight;
  FontID oldFont;
  RectangleType bounds;

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

static void ListFormScroll(WinDirectionType direction)
{
  UInt16 action, pendingAction;
  FormType *form;
  TableType *table;
  UInt16 rowsPerPage;

  if (direction == winDown)
    action = REDISPLAY_SCROLL_FORWARD;
  else
    action = REDISPLAY_SCROLL_BACKWARD;

  if (NULL != g_FindState) {
    pendingAction = g_FindState->action;
    if ((pendingAction == REDISPLAY_SCROLL_FORWARD) ||
        (pendingAction == REDISPLAY_SCROLL_BACKWARD)) {
      // If we already have a scroll in progress, then we attempt to
      // augment it rather than starting over.
      form = FrmGetActiveForm();
      table = FrmGetObjectPtrFromID(form, ListTable);
      rowsPerPage = ListFormNumberOfRows(table) - 1;
      if (action == pendingAction) {
        // Same direction: easy, just add more work.
        g_FindState->seekState.amountRemaining += rowsPerPage;
      }
      else if (g_FindState->seekState.amountRemaining >= rowsPerPage) {
        // Opposite direction, but several pages pending, cancel one.
        // Even if that goes to zero, still need to move to a valid record.
        g_FindState->seekState.amountRemaining -= rowsPerPage;
      }
      else {
        // Actually reversing direction.
        g_FindState->action = action;
        g_FindState->seekState.amountRemaining =
          rowsPerPage - g_FindState->seekState.amountRemaining;
      }
      return;
    }
  }

  ListFormRedisplay(action, false);
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
    ListFormFieldChanged();
  }
}

static void ListFormFieldChanged()
{
  FormType *form;
  ControlType *ctl;

  ListFormUpdateFindState();
  g_TopVisibleRecord = 0;
  if (g_IncrementalFind) {
    if (SYS_ROM_3_5) {
      form = FrmGetActiveForm();
      ctl = FrmGetObjectPtrFromID(form, ListFindButton);
      CtlSetGraphics(ctl, FindActiveBitmap, NULL);
    }
    ListFormRedisplay(REDISPLAY_BEGIN, false);
  }
}

static void ListFormFind()
{
  ListFormUpdateFindState();
  g_TopVisibleRecord = 0;
  ListFormRedisplay(REDISPLAY_BEGIN, false);
}

static void ListFormUpdateFindState()
{
  Char *key;
  FormType *form;
  FieldType *field;
  ListType *list;
  TableType *table;
  UInt16 size;

  ListFormDeleteFindState();

  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, ListFindTextField);
  key = FldGetTextPtr(field);
  if ((NULL == key) || ('\0' == *key))
    return;

  table = FrmGetObjectPtrFromID(form, ListTable);

  // Two extra to remember outsides for scroll arrows.
  size = sizeof(BookFindState) + sizeof(UInt16) * (TblGetNumberOfRows(table) + 2);
  g_FindState = MemPtrNew(size);
  if (NULL == g_FindState)
    return;
  MemSet(g_FindState, size, 0);

  g_FindState->keyHandle = FldGetTextHandle(field);

  list = FrmGetObjectPtrFromID(form, ListFindTypeList);
  g_FindState->seekState.filter.findType = LstGetSelection(list) + 1;
  g_FindState->seekState.filter.findKey = key;
  g_FindState->seekState.filter.keyPrep = NULL;
}

static void ListFormCloseFindState()
{
  FormType *form;
  FieldType *field;
  Char *key;

  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, ListFindTextField);

  if (NULL != g_FindState) {
    key = FldGetTextPtr(field);
    if ((NULL == key) || ('\0' == *key)) {
      ListFormDeleteFindState();
    }
    else {
      FldCompactText(field);
      g_FindState->seekState.filter.findKey = NULL; // About to be unlocked.
      FldSetTextHandle(field, NULL);
    }
  }
}

static void ListFormDeleteFindState()
{
  if (NULL != g_FindState) {
    if (NULL != g_FindState->seekState.filter.keyPrep)
      MemPtrFree(g_FindState->seekState.filter.keyPrep);
    if (NULL == g_FindState->seekState.filter.findKey)
      // This is the case where the state is left over with form not open.
      MemHandleFree(g_FindState->keyHandle);
    MemPtrFree(g_FindState);
    g_FindState = NULL;
  }
}

/*** Display ***/

// The time consuming part of display is seeking around in the
// database.  So allow that to be preempted via a state machine around
// the various seeks.
static void ListFormRedisplay(UInt16 action, Boolean checkCache)
{
  FormType *form;
  TableType *table;
  ControlType *ctl;
  FontID oldFont;
  RectangleType bounds, ibounds;
  UInt16 currentRecord, amount, ndraw, upIndex, downIndex;
  Int16 direction, row, nrows, selectRow, column, lineHeight, cacheIndex;
  Coord y;
  Boolean incremental, haveTableVars, preempted, found, fromCache, 
    updateScrollable, scrollableUp, scrollableDown;

  incremental = (REDISPLAY_NONE == action);
  if (incremental) {
    if ((g_FindState == NULL) ||
        (REDISPLAY_NONE == g_FindState->action))
      // Nothing to do.
      return;
    // The cache is not used in incremental mode; all its processing
    // is done before any preemption.
    checkCache = false;
  }
  else if (!checkCache && (NULL != g_FindState)) {
    g_FindState->cacheFillPointer = 0;
  }

  haveTableVars = updateScrollable = false;
  ndraw = 0;
  while (true) {
    if ((g_FindState == NULL) ||
        (((REDISPLAY_NONE != action) &&
          (action != g_FindState->action)) ||
         (g_FindState->seekState.amountRemaining == 0))) {
      // Unless resuming a previously preempted seek, figure out what to do.
      switch (action) {
      case REDISPLAY_BEGIN:
        action = REDISPLAY_UP_ARROW;
        continue;

      case REDISPLAY_SCROLL_FORWARD:
      case REDISPLAY_SCROLL_BACKWARD:
        form = FrmGetActiveForm();
        table = FrmGetObjectPtrFromID(form, ListTable);
        currentRecord = g_TopVisibleRecord;
        amount = ListFormNumberOfRows(table) - 1;
        direction = (REDISPLAY_SCROLL_FORWARD == action) ?
          dmSeekForward : dmSeekBackward;
        break;
      case REDISPLAY_FIRST_PAGE:
        currentRecord = 0;
        amount = 0;
        direction = dmSeekForward;
        break;
      case REDISPLAY_LAST_PAGE:
        form = FrmGetActiveForm();
        table = FrmGetObjectPtrFromID(form, ListTable);
        // NB: dmMaxRecordIndex and NO_RECORD are the same.
        currentRecord = dmMaxRecordIndex;
        amount = ListFormNumberOfRows(table) - 1;
        direction = dmSeekBackward;
        break;
      case REDISPLAY_FILL_TOP:
        currentRecord = g_TopVisibleRecord;
        amount = 0;
        direction = dmSeekForward;
        break;
      case REDISPLAY_FILL_NEXT:
        // currentRecord is where we left off.
        amount = 1;
        direction = dmSeekForward;
        break;
      case REDISPLAY_UP_ARROW:
        currentRecord = g_TopVisibleRecord;
        amount = 1;
        direction = dmSeekBackward;
        break;
      }
      
      if (NULL != g_FindState) {
        g_FindState->seekState.currentRecord = currentRecord;
        g_FindState->seekState.amountRemaining = amount;
        g_FindState->seekState.direction = direction;
      }
    }

    if (g_FindState == NULL) {
      found = BookDatabaseSeekRecord(&currentRecord, amount, direction, 
                                     g_CurrentCategory);
    }
    else {
      fromCache = false;
      cacheIndex = -1;
      if (checkCache) {
        switch (action) {
        case REDISPLAY_UP_ARROW:
          cacheIndex = 0;
          break;
        case REDISPLAY_FILL_TOP:
          cacheIndex = 1;
          break;
        case REDISPLAY_FILL_NEXT:
          // This use of row is okay because we never enter get to
          // REDISPLAY_FILL_NEXT except through REDISPLAY_FILL_TOP,
          // except in incremental mode, when we don't check the
          // cache at all.
          cacheIndex = row + 1;
          break;
        }
      }
      if ((cacheIndex >= 0) &&
          (cacheIndex < g_FindState->cacheFillPointer)) {
        fromCache = true;
        currentRecord = g_FindState->recordCache[cacheIndex];
        found = (NO_RECORD != currentRecord);
      }
      else {
        preempted = false;
        while (true) {
          found = BookDatabaseSeekRecordFiltered(&g_FindState->seekState, 
                                                 g_CurrentCategory);
          if (found) {
            currentRecord = g_FindState->seekState.currentRecord;
            break;
          }
          else if (g_FindState->seekState.amountRemaining > 0) {
            // Preempted: return to main event loop, which will call
            // us back if nothing changes.
            // TODO: Right now, the same flag enables preemption and
            // incremental find.  Could be separate, so that explicit
            // finds were still preemptable.
            if (g_IncrementalFind && EvtSysEventAvail(true)) {
              preempted = true;
              break;
            }
          }
          else {
            currentRecord = NO_RECORD;
            break;
          }
        }
        if (preempted) break;
      }
    }

    switch (action) {
    case REDISPLAY_SCROLL_FORWARD:
      if (found) {
        g_TopVisibleRecord = currentRecord;
        action = REDISPLAY_UP_ARROW;
      }
      else {
        action = REDISPLAY_LAST_PAGE;
      }
      continue;
    case REDISPLAY_SCROLL_BACKWARD:
    case REDISPLAY_LAST_PAGE:
      if (found) {
        g_TopVisibleRecord = currentRecord;
        action = REDISPLAY_UP_ARROW;
      }
      else {
        action = REDISPLAY_FIRST_PAGE;
      }
      continue;
    case REDISPLAY_FIRST_PAGE:
      // First page has nothing above by definition.
      if (g_FindState == NULL) {
        scrollableUp = false;
      }
      else if (!fromCache) {
        g_FindState->recordCache[0] = NO_RECORD;
        g_FindState->cacheFillPointer = 1;
      }
      action = REDISPLAY_FILL_TOP;
      if (found) {
        g_TopVisibleRecord = currentRecord;
        continue;
      }
      // If failed to find first page, must be entirely empty, so skip
      // redundant seeks.
      break;
    case REDISPLAY_UP_ARROW:
      if (g_FindState == NULL) {
        scrollableUp = found;
      }
      else if (!fromCache) {
        g_FindState->recordCache[0] = currentRecord;
        g_FindState->cacheFillPointer = 1;
      }
      action = REDISPLAY_FILL_TOP;
      continue;
    default:
      break;
    }
    
    // Have processed one of the fill actions.
    if ((NULL != g_FindState) && !fromCache) {
      // Remember success or failure.
      g_FindState->recordCache[g_FindState->cacheFillPointer++] = currentRecord;
    }

    if (!found) {
      scrollableDown = false;
      updateScrollable = true;
      action = REDISPLAY_NONE;
      break;
    }

    if (!haveTableVars) {
      form = FrmGetActiveForm();
      table = FrmGetObjectPtrFromID(form, ListTable);
      TblGetBounds(table, &bounds);
      nrows = TblGetNumberOfRows(table);
      if (REDISPLAY_FILL_TOP == action) {
        y = bounds.topLeft.y;
        row = 0;
        TblUnhighlightSelection(table);
      }
      else {
        row = TblGetLastUsableRow(table);
        TblGetItemBounds(table, row, COL_TITLE, &ibounds);
        row++;
        y = ibounds.topLeft.y + lineHeight;
      }
      selectRow = -1;
      oldFont = FntSetFont(g_ListFont);
      lineHeight = FntLineHeight();
      FntSetFont(oldFont);
      haveTableVars = true;
    }

    if ((row >= nrows) ||
        (y + lineHeight > bounds.topLeft.y + bounds.extent.y)) {
      // Row does not fit.
      scrollableDown = true;
      updateScrollable = true;
      action = REDISPLAY_NONE;
      break;
    }

    TblSetRowUsable(table, row, true);
    TblMarkRowInvalid(table, row);
    TblSetRowID(table, row, currentRecord);
    TblSetRowHeight(table, row, lineHeight);
    if (currentRecord == g_CurrentRecord)
      selectRow = row;
    row++;
    y += lineHeight;
    ndraw++;

    action = REDISPLAY_FILL_NEXT;
  }

  if (!incremental) {
    if (!haveTableVars) {
      // Only need a subset.
      form = FrmGetActiveForm();
      table = FrmGetObjectPtrFromID(form, ListTable);
      nrows = TblGetNumberOfRows(table);
      haveTableVars = true;
      // This is the empty table case.
      row = 0;
      selectRow = -1;
    }
    // Mark remainder of screen empty.
    while (row < nrows) {
      TblSetRowUsable(table, row, false);
      row++;
      ndraw++;
    }
  }

  if (ndraw > 0) {
    TblRedrawTable(table);
    // Doc isn't clear on this point, but evidently you can't select an invalid row.
    // Have to draw it first or SelectItem just doesn't take.
    if (selectRow >= 0) {
      TblSelectItem(table, selectRow, COL_TITLE);
      g_ScrollCurrentIntoView = false; // Done.
    }
  }

  if (updateScrollable) {
    if (NULL != g_FindState) {
      scrollableUp = ((g_FindState->cacheFillPointer > 0) &&
                      (NO_RECORD != g_FindState->recordCache[0]));
    }
    if (!haveTableVars)
      // Only need this.
      form = FrmGetActiveForm();
    upIndex = FrmGetObjectIndex(form, ListScrollUpRepeating);
    downIndex = FrmGetObjectIndex(form, ListScrollDownRepeating);
    FrmUpdateScrollers(form, upIndex, downIndex, scrollableUp, scrollableDown);
  }

  if (REDISPLAY_NONE == action) {
    // Final actions when redisplay nominally finished.
    if (g_ScrollCurrentIntoView) {
      g_ScrollCurrentIntoView = false; // Only try once.
      if (!TblGetSelection(table, &row, &column)) {
        g_TopVisibleRecord = g_CurrentRecord;
        // Could use goto, I suppose.
        ListFormRedisplay(REDISPLAY_BEGIN, false);
        return;
      }
    }
  }

  if (SYS_ROM_3_5) {
    ctl = FrmGetObjectPtrFromID(form, ListFindButton);
    CtlSetGraphics(ctl,
                   (REDISPLAY_NONE == action) ? FindBitmap : FindActiveBitmap,
                   NULL);
  }

  if (NULL != g_FindState) {
    g_FindState->action = action;
    if (REDISPLAY_NONE == action)
      g_EventInterval = evtWaitForever;
    else
      g_EventInterval = SysTicksPerSecond() / 10;
  }
}

static Boolean ListFormUpdateDisplay(UInt16 updateCode)
{
  FormType *form;
  Boolean handled;
 
  handled = false;
 
  if (updateCode & frmRedrawUpdateCode) {
    form = FrmGetActiveForm();
    FrmDrawForm(form);
    handled = true;
  }
 
  if (updateCode & UPDATE_FORCE_REDRAW) {
    ListFormRedisplay(REDISPLAY_BEGIN, false);
    handled = true;
  }
 
  if (updateCode & UPDATE_FONT_CHANGED) {
    if (NO_RECORD != g_CurrentRecord)
      g_ScrollCurrentIntoView = true;
    ListFormRedisplay(REDISPLAY_BEGIN, true);
    handled = true;
  }

  if (updateCode & UPDATE_CATEGORY_CHANGED) {
    g_TopVisibleRecord = 0;
    ListFormRedisplay(REDISPLAY_BEGIN, false);
    form = FrmGetActiveForm();
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
    ListFormDrawTitle(&record, bounds, g_ListFields);
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

void ListFormDrawTitle(BookRecord *record, RectangleType *bounds, UInt16 listFields)
{
  Char *str1, *str2;
  UInt16 len1, len2;
  Int16 width, x, y;
  DmResID noneID;
  MemHandle noneH;

  str1 = str2 = NULL;
  len1 = len2 = 0;
  switch (listFields) {
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
      switch (listFields) {
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
