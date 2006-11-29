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
#define UPDATE_TOP_CHANGED 0x08

/*** Local storage ***/

static FontID g_ViewTitleFont = largeBoldFont;
static FontID g_ViewDetailFont = stdFont;
static FontID g_ViewFont = stdFont;

/*static*/ Boolean g_ViewSummary = false;

static UInt16 g_TopFieldNumber = 0, g_TopFieldOffset = 0;
static UInt16 g_BottomFieldNumber = 0, g_BottomFieldOffset = 0;
static UInt16 g_HilightRecordFieldIndex = NO_FIELD, 
   g_HilightPosition = 0, g_HilightLength = 0;

static UInt8 g_ViewFields[] = {
  FIELD_TITLE,
  FIELD_AUTHOR,
  FIELD_ISBN,
  FIELD_PUBLICATION,
  FIELD_TAGS
#ifdef UNICODE
  ,FIELD_COMMENTS
#endif
};

#define VIEW_NFIELDS sizeof(g_ViewFields)

static UInt8 g_ViewFieldsSummary[] = {
  FIELD_SUMMARY,
  FIELD_ISBN,
  FIELD_PUBLICATION,
  FIELD_TAGS
#ifdef UNICODE
  ,FIELD_COMMENTS
#endif
};

#define VIEW_NFIELDS_SUMMARY sizeof(g_ViewFieldsSummary)

#define ViewNoTitle ListNoTitle

/*** Local routines ***/

static void ViewFormOpen(FormType *form) VIEW_SECTION;
static Boolean ViewFormMenuCommand(UInt16 command) VIEW_SECTION;
static void ViewFontSelect() VIEW_SECTION;
static void ViewBeamRecord(Boolean send) VIEW_SECTION;
static void ViewFormScroll(WinDirectionType direction) VIEW_SECTION;
static Boolean ViewFormGadgetHandler(FormGadgetTypeInCallback *gadgetP, 
                                     UInt16 cmd, void *paramP) VIEW_SECTION;
static Boolean ViewFormGadgetPen(EventType *event) VIEW_SECTION;
static Boolean ViewFormUpdateDisplay(UInt16 updateCode) VIEW_SECTION;
static void ViewFormGadgetDraw() VIEW_SECTION;
static Boolean ViewFormGadgetDrawField(Char *str, UInt16 *offset, UInt16 *ndrawn,
                                       Coord *y, RectangleType *bounds,
                                       UInt16 highlightPosition, UInt16 highlightLength) VIEW_SECTION;

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
  UInt16 index;

  if (SYS_ROM_3_5) {
    FrmSetGadgetHandler(form, FrmGetObjectIndex(form, ViewRecordGadget),
                        ViewFormGadgetHandler);
  }

  if (WebEnabled()) {
    index = FrmGetObjectIndex(form, ViewWebButton);
    FrmShowObject(form, index);
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

    case ViewWebButton:
      WebGotoCurrentBook();
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
  if (winDown == direction) {
    // Prefer not to start in the middle of a field, but must if it
    // takes more than a screen all by itself.
    if (g_TopFieldNumber == g_BottomFieldNumber)
      g_TopFieldOffset = g_BottomFieldOffset;
    else {
      g_TopFieldNumber = g_BottomFieldNumber;
      g_TopFieldOffset = 0;
    }
  }
  else {
    // TODO: Can do better than this.
    g_TopFieldNumber = g_TopFieldOffset = 0;
  }
  FrmUpdateForm(FrmGetActiveFormID(), UPDATE_TOP_CHANGED);
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
 
  if (updateCode & (frmRedrawUpdateCode | UPDATE_FONT_CHANGED | UPDATE_TOP_CHANGED)) {
    FrmDrawForm(form);
    handled = true;
  }

  return handled;
}


static Boolean ViewFormGadgetDrawField(Char *str, UInt16 *offset, UInt16 *ndrawn,
                                       Coord *y, RectangleType *bounds,
                                       UInt16 highlightPosition, UInt16 highlightLength)
{
  Char *sp;
  UInt16 lineHeight, bottom;
  UInt16 nchars, ndraw;
  UInt16 nhilight;
  RectangleType invert;
  Boolean exhausted;
  
  lineHeight = FntLineHeight();
  bottom = bounds->topLeft.y + bounds->extent.y;

  if (highlightLength > 0) {
    if (highlightPosition < *offset)
      highlightPosition = highlightLength = 0;
    else
      highlightPosition -= *offset;
  }

  *ndrawn = 0;

  sp = str + *offset;
  while (true) {
    if (*y + lineHeight > bottom) {
      exhausted = false;
      break;
    }

    nchars = FntWordWrap(sp, bounds->extent.x);
    ndraw = nchars;
    while ((ndraw > 0) && TxtGlueCharIsSpace(sp[ndraw-1]))
      ndraw--;

    WinDrawChars(sp, ndraw, bounds->topLeft.x, *y);
    if (highlightLength > 0) {
      if (highlightPosition < nchars) {
        invert.topLeft.y = *y;
        invert.extent.y = lineHeight;
        invert.topLeft.x = bounds->topLeft.x;
        if (highlightPosition > 0)
          invert.topLeft.x += FntCharsWidth(sp, highlightPosition);
        nhilight = highlightLength;
        if (highlightPosition + nhilight > nchars)
          nhilight = nchars - highlightPosition;
        highlightLength -= nhilight;
        invert.extent.x = FntCharsWidth(sp + highlightPosition, nhilight);
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
    *ndrawn += nchars;
    *offset = (sp - str);     // Start, not end, of last piece drawn.
    sp += nchars;

    if ('\0' == *sp) {
      exhausted = true;
      break;
    }
  }
  
  return exhausted;
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
  UInt16 offset, ndrawn;
  UInt16 hilightPosition, hilightLength;
  MemHandle noneH;
  Boolean exhausted, scrollableUp, scrollableDown;
  UInt16 upIndex, downIndex;

  if (BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record))
    return;

  if (g_ViewSummary && (NULL != record.fields[FIELD_SUMMARY])) {
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
    offset = 0;
    if (NULL == str) {
      if (fieldNumber > 0) continue; // Top field always draws something.
      noneH = DmGetResource(strRsc, ViewNoTitle);
      str = (Char *)MemHandleLock(noneH);
    }
    else if (fieldNumber == g_TopFieldNumber)
      offset = g_TopFieldOffset;

#ifdef UNICODE
    if ((FIELD_COMMENTS == recordFieldIndex) &&
        !BookRecordFieldIsUnicode(&record, recordFieldIndex))
      // Since we can't edit Unicode yet, we have to display this here
      // if it's Unicode.
      continue;
#endif

    if (recordFieldIndex == g_HilightRecordFieldIndex) {
      hilightPosition = g_HilightPosition;
      hilightLength = g_HilightLength;
    }
    else {
      hilightPosition = hilightLength = 0;
    }

#ifdef UNICODE
    if (BookRecordFieldIsUnicode(&record, recordFieldIndex)) {
      // TODO: hilightPosition, hilightLength.
      exhausted = UnicodeDrawField(str, &offset, StrLen(str), &ndrawn, &y, &bounds);
    }
    else
#endif
    {
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
      exhausted = ViewFormGadgetDrawField(str, &offset, &ndrawn, &y, &bounds, 
                                          hilightPosition, hilightLength);
    }
    if (ndrawn > 0) {
      // We actually drew something.
      g_BottomFieldNumber = fieldNumber;
      g_BottomFieldOffset = offset;
    }
    if (!exhausted) 
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
