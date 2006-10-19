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

static Boolean g_ViewSummary = false;

static UInt16 g_TopFieldNumber = 0, g_TopFieldOffset = 0;

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

void ViewFormActivate()
{
  g_TopFieldNumber = g_TopFieldOffset = 0;
  FrmGotoForm(ViewForm);
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
  Boolean handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    {
      FormType  *form;

      form = FrmGetActiveForm();
      ViewFormOpen(form);
      FrmDrawForm(form);
      if (!SYS_ROM_3_5)
        ViewFormGadgetDraw();
    }
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
      EditFormActivate();
      handled = true;
      break;

    case ViewNoteButton:
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


static Boolean ViewFormGadgetDrawField(Char *str, Coord *y, RectangleType *bounds)
{
  UInt16 lineHeight, bottom;
  UInt16 nchars, ndraw;
  
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
  MemHandle noneH;
  Boolean scrollableUp, scrollableDown;
  UInt16 upIndex, downIndex;

  if (g_ViewSummary) {
    fields = g_ViewFieldsSummary;
    nfields = VIEW_NFIELDS_SUMMARY;
  }
  else {
    fields = g_ViewFields;
    nfields = VIEW_NFIELDS;
  }

  if (BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record))
    return;

  form = FrmGetActiveForm();
  FrmGetObjectBounds(form, FrmGetObjectIndex(form, ViewRecordGadget), &bounds);

  oldFont = FntSetFont(g_ViewFont);
    
  noneH = NULL;

  y = bounds.topLeft.y;
  fieldNumber = g_TopFieldNumber;
  while (fieldNumber < nfields) {
    recordFieldIndex = fields[fieldNumber];

    str = record.fields[recordFieldIndex];
    if (NULL == str) {
      if (fieldNumber > 0) continue; // Top field always draws something.
      noneH = DmGetResource(strRsc, ViewNoTitle);
      str = (Char *)MemHandleLock(noneH);
    }
    else if (fieldNumber == 0)
      str += g_TopFieldOffset;

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

    if (!ViewFormGadgetDrawField(str, &y, &bounds))
      break;
    fieldNumber++;
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
