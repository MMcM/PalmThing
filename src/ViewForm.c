/*! -*- Mode: C -*-
* Module: ViewForm.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants ***/

#define UPDATE_FONT_CHANGED 0x02

/*** Local storage ***/

static FontID g_ViewTitleFont = largeBoldFont;
static FontID g_ViewDetailFont = stdFont;
static FontID g_ViewFont = stdFont;

/*static*/ Boolean g_ViewSummary = false;

static UInt16 g_TopFieldNumber = 0, g_TopFieldOffset = 0;
static UInt16 g_HilightRecordFieldIndex = NO_FIELD, 
   g_HilightPosition = 0, g_HilightLength = 0;

static UInt8 g_ViewFields[] = {
  FIELD_TITLE,
  FIELD_AUTHOR,
  FIELD_ISBN,
  FIELD_PUBLICATION,
  FIELD_TAGS
};

#define VIEW_NFIELDS sizeof(g_ViewFields)

static UInt8 g_ViewFieldsSummary[] = {
  FIELD_SUMMARY,
  FIELD_ISBN,
  FIELD_PUBLICATION,
  FIELD_TAGS
};

#define VIEW_NFIELDS_SUMMARY sizeof(g_ViewFieldsSummary)

#define ViewNoTitle ListNoTitle

/*** Local routines ***/

static Boolean ViewFormMenuCommand(UInt16 command);
static void ViewFontSelect();
static void ViewBeamRecord(Boolean send);
static void ViewFormScroll(WinDirectionType direction);
static Boolean ViewFormGadgetHandler(FormGadgetTypeInCallback *gadgetP, 
                                     UInt16 cmd, void *paramP);
static Boolean ViewFormGadgetPen(EventType *event);
static Boolean ViewFormUpdateDisplay(UInt16 updateCode);
static void ViewFormGadgetDraw();

/*** Setup and event handling ***/

void ViewFormSetup(AppPreferences *prefs, BookAppInfo *appInfo)
{
  g_ViewTitleFont = FntGlueGetDefaultFontID(defaultLargeFont);
  g_ViewDetailFont = FntGlueGetDefaultFontID(defaultSmallFont);
  if (NULL == prefs) {
    g_ViewFont = FntGlueGetDefaultFontID(defaultSystemFont);
    g_ViewSummary = false;
  }
  else {
    g_ViewFont = prefs->viewFont;
    g_ViewSummary = prefs->viewSummary;
  }
}

void ViewFormSetdown(AppPreferences *prefs)
{
  prefs->viewFont = g_ViewFont;
  prefs->viewSummary = g_ViewSummary;
}

void ViewFormActivate(BookFilter *filter)
{
  g_TopFieldNumber = g_TopFieldOffset = 0;
  if ((NULL != filter) &&
      // Update match pos. for this record.
      BookRecordFilterMatch(g_CurrentRecord, filter)) {
    g_HilightRecordFieldIndex = filter->matchField;
    g_HilightPosition = filter->matchPos;
    g_HilightLength = filter->matchLen;
  }
  else {
    g_HilightRecordFieldIndex = NO_FIELD;
  }
  FrmGotoForm(ViewForm);
}

UInt16 ViewFormGoToPrepare(GoToParamsPtr params)
{
  // In case return to list.
  if (dmAllCategories != g_CurrentCategory) {
    g_CurrentCategory = BookRecordGetCategory(params->recordNum);
  }

  g_CurrentRecord = params->recordNum;
  g_TopFieldNumber = g_TopFieldOffset = 0;
  if (FIELD_COMMENTS == params->matchFieldNum) {
    g_HilightRecordFieldIndex = NO_FIELD;
    return NoteFormGoToPrepare(params);
  }
  else {
    g_HilightRecordFieldIndex = params->matchFieldNum;
    g_HilightPosition = params->matchPos;
    g_HilightLength = params->matchCustom;
    // TODO: May need to scroll into view by adjusting g_TopFieldNumber.
    return ViewForm;
  }
}

static void ViewFormOpen(FormType *form)
{
  if (SYS_ROM_3_5) {
    FrmSetGadgetHandler(form, FrmGetObjectIndex(form, ViewRecordGadget),
                        ViewFormGadgetHandler);
  }

  FrmSetCategoryLabel(form, FrmGetObjectIndex(form, ViewCategoryLabel),
                      BookRecordGetCategoryName(g_CurrentRecord));
}

Boolean ViewFormHandleEvent(EventType *event)
{
  FormType *form;
  Boolean handled = false;

  switch (event->eType) {
  case frmOpenEvent:
  case frmGotoEvent:            // All the special handling already done by Prepare.
    form = FrmGetActiveForm();
    ViewFormOpen(form);
    FrmDrawForm(form);
    if (!SYS_ROM_3_5)
      ViewFormGadgetDraw();
    handled = true;
    break;

  case frmUpdateEvent:
    return ViewFormUpdateDisplay(event->data.frmUpdate.updateCode);

  case menuEvent:
    return ViewFormMenuCommand(event->data.menu.itemID);
    
  case ctlSelectEvent:
    switch (event->data.ctlSelect.controlID) {
    case ViewDoneButton:
      FrmGotoForm(ListForm);
      handled = true;
      break;

    case ViewEditButton:
      g_TopFieldNumber = g_TopFieldOffset = 0;
      g_HilightRecordFieldIndex = NO_FIELD;
      EditFormActivate();
      handled = true;
      break;

    case ViewNoteButton:
      g_TopFieldNumber = g_TopFieldOffset = 0;
      g_HilightRecordFieldIndex = NO_FIELD;
      NoteFormActivate();
      handled = true;
      break;
    }
    break;

  case ctlRepeatEvent:
    switch (event->data.ctlRepeat.controlID) {
    case ViewScrollUpRepeating:
      ViewFormScroll(winUp);
      break;
     
    case ViewScrollDownRepeating:
      ViewFormScroll(winDown);
      break;
    }
    break;

  case penDownEvent:
    if (!SYS_ROM_3_5)
      handled = ViewFormGadgetPen(event);
    break;
  
  case keyDownEvent:
    if (TxtCharIsHardKey(event->data.keyDown.modifiers,
                         event->data.keyDown.chr)) {
      FrmGotoForm(ListForm);
      handled = true;
    }
    else {
      switch (event->data.keyDown.chr) {
      case pageUpChr:
        ViewFormScroll(winUp);
        handled = true;
        break;
    
      case pageDownChr:
        ViewFormScroll(winDown);
        handled = true;
        break;
      }
    }
    break;

  default:
    break;
  }

  return handled;
}

static Boolean ViewFormGadgetHandler(FormGadgetTypeInCallback *gadgetP, 
                                     UInt16 cmd, void *paramP)
{
  Boolean handled = false;

  switch (cmd) {
  case formGadgetDrawCmd:
    ViewFormGadgetDraw();
    handled = true;
    break;

  case formGadgetHandleEventCmd:
    if (frmGadgetEnterEvent == ((EventType *)paramP)->eType)
      handled = ViewFormGadgetPen(NULL);
    break;
  }

  return handled;
}

/*** Commands ***/

static Boolean ViewFormMenuCommand(UInt16 command)
{
  Boolean handled;
 
  handled = false;

  switch (command) {
  case RecordBeamBook:
    ViewBeamRecord(false);
    handled = true;
    break;

  case RecordSendBook:
    ViewBeamRecord(true);
    handled = true;
    break;

  case OptionsAbout:
    AboutFormDisplay();
    handled = true;
    break;

  case OptionsFont:
    ViewFontSelect();
    handled = true;
    break;
  }

  return handled;
}

static void ViewFontSelect()
{
  FontID newFont;

  if (!SYS_ROM_3_5) return;

  newFont = FontSelect(g_ViewFont);
  if (newFont != g_ViewFont) {
    g_ViewFont = newFont;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FONT_CHANGED);
  }
}

static void ViewBeamRecord(Boolean send)
{
  // TODO: Do something.
}

static void ViewFormScroll(WinDirectionType direction)
{
}

static Boolean ViewFormGadgetPen(EventType *event)
{
  FormType *form;
  RectangleType bounds;
  Int16 x, y;
  Boolean penDown;

  form = FrmGetActiveForm();
  FrmGetObjectBounds(form, FrmGetObjectIndex(form, ViewRecordGadget), &bounds);

  // Only in the (pre-3.5) case of a penDown event do we need to check
  // that it was within the gadget.
  if (NULL != event) {
    if (!RctPtInRectangle(event->screenX, event->screenY, &bounds))
      return false;
  }

  // Wait for release.
  do {
    PenGetPoint(&x, &y, &penDown);
  }
  while (penDown);
      
  if (RctPtInRectangle(x, y, &bounds)) {
    g_TopFieldNumber = g_TopFieldOffset = 0;
    EditFormActivate();
  }
  
  return true;
}

/*** Display ***/

static Boolean ViewFormUpdateDisplay(UInt16 updateCode)
{
  FormType *form;
  Boolean handled;
 
  form = FrmGetActiveForm();
  handled = false;
 
  if (updateCode & frmRedrawUpdateCode) {
    FrmDrawForm(form);
    handled = true;
  }
 
  if (updateCode & UPDATE_FONT_CHANGED) {
    FrmDrawForm(form);
    handled = true;
  }

  return handled;
}


static Boolean ViewFormGadgetDrawField(Char *str, Coord *y, RectangleType *bounds,
                                       UInt16 highlightPosition, UInt16 highlightLength)
{
  UInt16 lineHeight, bottom;
  UInt16 nchars, ndraw;
  UInt16 nhilight;
  RectangleType invert;
  
  lineHeight = FntLineHeight();
  bottom = bounds->topLeft.y + bounds->extent.y;

  while (true) {
    if (*y + lineHeight > bottom)
      return false;

    nchars = FntWordWrap(str, bounds->extent.x);
    ndraw = nchars;
    while ((ndraw > 0) && TxtGlueCharIsSpace(str[ndraw-1]))
      ndraw--;

    WinDrawChars(str, ndraw, bounds->topLeft.x, *y);
    if (highlightLength > 0) {
      if (highlightPosition < nchars) {
        invert.topLeft.y = *y;
        invert.extent.y = lineHeight;
        invert.topLeft.x = bounds->topLeft.x;
        if (highlightPosition > 0)
          invert.topLeft.x += FntCharsWidth(str, highlightPosition);
        nhilight = highlightLength;
        if (highlightPosition + nhilight > nchars)
          nhilight = nchars - highlightPosition;
        highlightLength -= nhilight;
        invert.extent.x = FntCharsWidth(str + highlightPosition, nhilight);
        highlightPosition = 0;
        if (invert.topLeft.x > 0) {
          // Aligned directly at the left of characters looks worse.
          invert.topLeft.x--;
          invert.extent.x++;
        }
        if (invert.topLeft.x + invert.extent.x > bounds->topLeft.x + bounds->extent.x) {
          // This is the case where part of what is to be hilit is in
          // the whitespace that we didn't draw.
          invert.extent.x = bounds->topLeft.x + bounds->extent.x - invert.topLeft.x;
        }
        WinInvertRectangle(&invert, 0);
      }
      else {
        highlightPosition -= nchars;
      }
    }
    *y += lineHeight;
    str += nchars;

    if ('\0' == *str) break;
  }

  return true;
}

static void ViewFormGadgetDraw()
{
  UInt8 *fields;
  UInt16 nfields, fieldNumber, recordFieldIndex;
  FormType *form;
  RectangleType bounds;
  BookRecord record;
  MemHandle recordH;
  FontID oldFont;
  Coord y;
  Char *str;
  UInt16 hilightPosition, hilightLength;
  MemHandle noneH;
  Boolean scrollableUp, scrollableDown;
  UInt16 upIndex, downIndex;

  if (BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record))
    return;

  if (g_ViewSummary) {
    fields = g_ViewFieldsSummary;
    nfields = VIEW_NFIELDS_SUMMARY;
  }
  else {
    fields = g_ViewFields;
    nfields = VIEW_NFIELDS;
  }

  form = FrmGetActiveForm();
  FrmGetObjectBounds(form, FrmGetObjectIndex(form, ViewRecordGadget), &bounds);

  oldFont = FntSetFont(g_ViewFont);
    
  noneH = NULL;

  y = bounds.topLeft.y;
  for (fieldNumber = g_TopFieldNumber; fieldNumber < nfields; fieldNumber++) {
    recordFieldIndex = fields[fieldNumber];

    str = record.fields[recordFieldIndex];
    if (NULL == str) {
      if (fieldNumber > 0) continue; // Top field always draws something.
      noneH = DmGetResource(strRsc, ViewNoTitle);
      str = (Char *)MemHandleLock(noneH);
    }
    else if (fieldNumber == 0)
      str += g_TopFieldOffset;

    if (recordFieldIndex == g_HilightRecordFieldIndex) {
      hilightPosition = g_HilightPosition;
      hilightLength = g_HilightLength;
      if (fieldNumber == 0)
        hilightPosition -= g_TopFieldOffset;
    }
    else {
      hilightPosition = hilightLength = 0;
    }

#ifdef UNICODE
    if (record.unicodeMask & (1 << recordFieldIndex)) {
      // TODO: hilightPosition, hilightLength.
      if (!UnicodeDrawField(str, StrLen(str), &y, &bounds))
        break;
      else
        continue;
    }
#endif

    switch (recordFieldIndex) {
    case FIELD_TITLE:
    case FIELD_SUMMARY:
      FntSetFont(g_ViewTitleFont);
      break;

    case FIELD_PUBLICATION:
      FntSetFont(g_ViewDetailFont);
      break;

    default:
      FntSetFont(g_ViewFont);
      break;
    }

    if (!ViewFormGadgetDrawField(str, &y, &bounds, hilightPosition, hilightLength))
      break;
  }

  if (NULL != noneH)
    MemHandleUnlock(noneH);

  FntSetFont(oldFont);

  MemHandleUnlock(recordH);

  scrollableUp = (g_TopFieldNumber > 0);
  scrollableDown = (fieldNumber < nfields);
  upIndex = FrmGetObjectIndex(form, ViewScrollUpRepeating);
  downIndex = FrmGetObjectIndex(form, ViewScrollDownRepeating);
  FrmUpdateScrollers(form, upIndex, downIndex, scrollableUp, scrollableDown);
}
