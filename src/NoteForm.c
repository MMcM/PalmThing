/*! -*- Mode: C -*-
* Module: NoteForm.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/***
 * Since comments can be a bit too long for just a field on the Edit
 * page, we edit them using the standard application's NoteView.  This
 * includes things like emulating its non-standard title.  For this,
 * code is taken from palmosbible.com.
 ***/

/*** Local constants and types ***/

// Update codes.
#define UPDATE_FONT_CHANGED 0x02

/*** Local storage ***/

static UInt16 g_NoteRecordField = FIELD_COMMENTS;

static FontID g_NoteFont = stdFont;

static UInt16 g_ReturnToForm = ListForm;

/*** Local routines ***/

static Boolean NoteFormMenuCommand(UInt16 command);
static void NoteFontSelect();
static void NoteFormPageScroll(WinDirectionType direction);
static void NoteFormScroll(Int16 linesToScroll);
static void NoteFormSave();
static Boolean NoteFormDeleteNote();
static void NoteFormLoadRecord();
static void NoteFormDrawTitle(FormType *form);
static void NoteFormUpdateScrollBar();

static inline void *FrmGetObjectPtrFromID(const FormType *formP, UInt16 objID)
{
  return FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, objID));
}

/*** Setup and event handling ***/

void NoteFormSetup(AppPreferences *prefs, BookAppInfo *appInfo)
{
  if (NULL == prefs) {
    g_NoteFont = FntGlueGetDefaultFontID(defaultSmallFont);
  }
  else {
    g_NoteFont = prefs->noteFont;
  }
}

void NoteFormSetdown(AppPreferences *prefs)
{
  prefs->noteFont = g_NoteFont;
}

void NoteFormActivate()
{
  MemHandle recordH;
  BookRecord record;

  if (!BookRecordHasField(g_CurrentRecord, g_NoteRecordField)) {
    // Make sure there is a note to edit.
    if (!BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record)) {
      record.fields[g_NoteRecordField] = "";
      BookDatabaseSaveRecord(&g_CurrentRecord, &recordH, &record);
      if (NULL != recordH)
        MemHandleUnlock(recordH);
    }
  }

  g_ReturnToForm = FrmGetActiveFormID();
  FrmGotoForm(SYS_ROM_3_5 ? NewNoteView : NoteView);
}

static void NoteFormOpen(FormType *form)
{
  FieldType *field;
  FieldAttrType attr;

  NoteFormLoadRecord();

  field = FrmGetObjectPtrFromID(form, NoteField);
  FldGetAttributes(field, &attr);
  attr.hasScrollBar = true;
  FldSetAttributes(field, &attr);
}

Boolean NoteFormHandleEvent(EventType *event)
{
  FormType *form;
  FieldType *field;
  Boolean handled;

  handled = false;

  switch (event->eType) {
  case frmOpenEvent:
    form = FrmGetActiveForm();
    NoteFormOpen(form);
    // Draw the note view's title bar.  NoteFormDrawTitle
    // includes a call to FrmDrawForm, so it's not needed here.
    NoteFormDrawTitle(form);
    NoteFormUpdateScrollBar();
    FrmSetFocus(form, FrmGetObjectIndex(form, NoteField));
    handled = true;
    break;

  case frmGotoEvent:
    form = FrmGetActiveForm();
    g_CurrentRecord = event->data.frmGoto.recordNum;
    NoteFormOpen(form);
    field = FrmGetObjectPtrFromID(form, NoteField);
    FldSetScrollPosition(field, event->data.frmGoto.matchPos);
    FldSetSelection(field, event->data.frmGoto.matchPos, 
                    event->data.frmGoto.matchPos +
                    event->data.frmGoto.matchLen);
    NoteFormDrawTitle(form);
    NoteFormUpdateScrollBar();
    FrmSetFocus(form, FrmGetObjectIndex(form, NoteField));
    handled = true;
    break;

  case frmUpdateEvent:
    form = FrmGetActiveForm();
    if (event->data.frmUpdate.updateCode & UPDATE_FONT_CHANGED) {
      field = FrmGetObjectPtrFromID(form, NoteField);
      FldSetFont(field, g_NoteFont);
      NoteFormUpdateScrollBar();
      handled = true;
    }
    else {
      NoteFormDrawTitle(form);
      handled = true;
    }
    break;

  case menuEvent:
    return NoteFormMenuCommand(event->data.menu.itemID);

  case ctlSelectEvent:
    switch (event->data.ctlSelect.controlID) {
      case NoteDoneButton:
        NoteFormSave();
        FrmGotoForm(g_ReturnToForm);
        handled = true;
        break;

      case NoteDeleteButton:
        if (NoteFormDeleteNote())
          FrmGotoForm(g_ReturnToForm);
        handled = true;
        break;
               
      default:
        break;
      }
    break;

  case sclRepeatEvent:
    NoteFormScroll(event->data.sclRepeat.newValue - 
                   event->data.sclRepeat.value);
    break;

  case keyDownEvent:
    if (TxtCharIsHardKey(event->data.keyDown.modifiers,
                         event->data.keyDown.chr)) {
      NoteFormSave();
      FrmGotoForm(ListForm);
      handled = true;
    } 
    else {
      switch (event->data.keyDown.chr) {
      case pageUpChr:
        NoteFormPageScroll(winUp);
        handled = true;
        break;
     
      case pageDownChr:
        NoteFormPageScroll(winDown);
        handled = true;
        break;
     
      default:
        break;
      }
    }
    break;

  case fldChangedEvent:
    NoteFormUpdateScrollBar();
    handled = true;
    break;
      
  case frmCloseEvent:
    NoteFormSave();
    break;

  default:
    break;
  }

  return handled;
}

/*** Commands ***/

static Boolean NoteFormMenuCommand(UInt16 command)
{
  FormType *form;
  FieldType *field;
  Boolean handled;

  handled = false;

  switch (command) {
  case noteFontCmd:
  case newNoteFontCmd:
    NoteFontSelect();
    handled = true;
    break;

  // TODO: Perhaps it would be better to just handle menuOpenEvent and
  // hide these items.
  case notePhoneLookupCmd:
  case newNotePhoneLookupCmd:
    form = FrmGetActiveForm();
    field = FrmGetObjectPtrFromID(form, NoteField);
    PhoneNumberLookup(field);
    break;
  }

  return handled;
}

static void NoteFontSelect()
{
  FontID newFont;

  if (!SYS_ROM_3_0) return;

  newFont = FontSelect(g_NoteFont);
  if (newFont != g_NoteFont) {
    g_NoteFont = newFont;
    FrmUpdateForm(FrmGetActiveFormID(), UPDATE_FONT_CHANGED);
  }
}

static void NoteFormPageScroll(WinDirectionType direction)
{
  FormType *form;
  FieldType *field;
  ScrollBarType *bar;
  Int16 min, max, value, pageSize;
  UInt16 linesToScroll;
 
  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, NoteField);
 
  if (FldScrollable(field, direction)) {
    linesToScroll = FldGetVisibleLines(field) - 1;
    FldScrollField(field, linesToScroll, direction);
  
    bar = FrmGetObjectPtrFromID(form, NoteScrollBar);
    SclGetScrollBar(bar, &value, &min, &max, &pageSize);
  
    if (direction == winUp)
      value -= linesToScroll;
    else
      value += linesToScroll;
      
    SclSetScrollBar(bar, value, min, max, pageSize);
  }
}

static void NoteFormScroll(Int16 linesToScroll)
{
  FormType *form;
  FieldType *field;
  ScrollBarType *bar;
  Int16 min, max, value, pageSize;
  UInt16 blankLines;
 
  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, NoteField);
 
  if (linesToScroll < 0) {
    blankLines = FldGetNumberOfBlankLines(field);
    FldScrollField(field, -linesToScroll, winUp);
  
    // If there were blank lines visible at the end of the field
    // then we need to update the scroll bar.
    if (blankLines > 0) {
      bar = FrmGetObjectPtrFromID(form, NoteScrollBar);
      SclGetScrollBar(bar, &value, &min, &max, &pageSize);
      if (blankLines > -linesToScroll)
        max += linesToScroll;
      else
        max -= blankLines;
      SclSetScrollBar(bar, value, min, max, pageSize);
    }
  }
  else if (linesToScroll > 0)
    FldScrollField(field, linesToScroll, winDown);
}

static void NoteFormSave()
{
  FormType *form;
  FieldType *field;

  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, NoteField);

  if (FldDirty(field)) {      
    FldCompactText(field);
    BookDatabaseDirtyRecord(g_CurrentRecord);
  }

  FldSetTextHandle(field, NULL);
}

static Boolean NoteFormDeleteNote()
{
  FormType *form;
  FieldType *field;
  MemHandle recordH;
  BookRecord record;

  if (FrmAlert(DeleteNoteAlert) != DeleteNoteYes)
    return false;

  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, NoteField);
  FldSetTextHandle(field, NULL);

  if (!BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record)) {
    record.fields[g_NoteRecordField] = NULL;
    BookDatabaseSaveRecord(&g_CurrentRecord, &recordH, &record);
    if (NULL != recordH)
      MemHandleUnlock(recordH);
  }

  return true;
}

/*** Display ***/

static void NoteFormLoadRecord()
{
  FormType *form;
  FieldType *field;
  MemHandle recordH;
  UInt16 offset, length;

  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, NoteField);

  FldSetFont(field, g_NoteFont);

  if (BookRecordGetField(g_CurrentRecord, g_NoteRecordField, 
                         &recordH, &offset, &length)) {
    recordH = NULL;
    offset = length = 0;
  }
  FldSetText(field, recordH, offset, length);
}

// This routine is taken more or less straight from the example.
static void NoteFormDrawTitle(FormType *form)
{
  Coord x, y, length, width, nameExtent, formWidth;
  MemHandle nameH;
  UInt16 nameOffset, nameLength;
  FontID oldFont;
  IndexedColorType oldForeColor, oldBackColor, oldTextColor;
  RectangleType bounds, eraseRect, drawRect;
  Boolean noName, ignored;
  Char *name;
  UInt8 *lockedWindow;
 
  // The distinctive look of the note view in the ROM applications is
  // not a property of the NoteForm or NewNoteForm form resources
  // stored in ROM.  Rather, it is achieved by application code,
  // drawing an inverted rounded rectangular title bar over the top
  // of the regular-shaped title bar.

  // Different action is required depending on the version of the Palm
  // OS Librarian is running on.  Prior to 3.5, drawing the note
  // view's title bar is very simple, since color is only one bit.
  // With the introduction of up to 8-bit color in Palm OS 3.5, this
  // routine must make sure to draw the title so it matches the
  // current Form title colors, whatever they may be.
  lockedWindow = NULL;
  if (SYS_ROM_3_5) {
    // Lock the screen during drawing to avoid the mess that occurs
    // when the system draws the form's title, then Librarian draws
    // the special note view title over that.
    lockedWindow = WinScreenLock(winLockCopy);
  }
 
  FrmDrawForm(form);
 
  FrmGetFormBounds(form, &bounds);
  formWidth = bounds.extent.x;
  x = 2;
  y = 1;
  nameExtent = formWidth - 4;
   
  RctSetRectangle(&eraseRect, 0, 0, formWidth, (FntLineHeight() + 4) );
  RctSetRectangle(&drawRect, 0, 0, formWidth, (FntLineHeight() + 2) );
 
  // Save the current font and set the font to bold for drawing the
  // title.
  oldFont = FntSetFont(boldFont);
 
  // On 3.5, Save and set the window colors.  This is necessary
  // because the FrmDrawForm call above sets the foreground and
  // background colors to the colors used for drawing the form's UI,
  // but NoteFormDrawTitle needs to draw in the colors used for the
  // form's title bar.
  if (SYS_ROM_3_5) {
    oldForeColor = WinSetForeColor(UIColorGetTableEntryIndex(UIFormFrame));
    oldBackColor = WinSetBackColor(UIColorGetTableEntryIndex(UIFormFill));
    oldTextColor = WinSetTextColor(UIColorGetTableEntryIndex(UIFormFrame));
  }

  // Clear the title area and draw the note view title bar.
  WinEraseRectangle(&eraseRect, 0);
  WinDrawRectangle(&drawRect, 3);

  if (BookRecordGetField(g_CurrentRecord, FIELD_TITLE, 
                         &nameH, &nameOffset, &nameLength)) {
    nameH = NULL;
    nameOffset = nameLength = 0;
  }
  else if (nameLength > 0)
    nameLength--;               // Was alloc length, want string length.

  if (nameLength > 0) {
    name = (Char *)MemHandleLock(nameH);
    name += nameOffset;
    noName = false;
  }
  else {
    nameH = DmGetResource(strRsc, NoteNoName);
    name = (Char *)MemHandleLock(nameH);
    nameLength = StrLen(name);
    noName = true;
  }

  // Find out how much of the title string will fit.  If all of the
  // string fits, center it before drawing.  Since FntCharsInWidthName
  // changes the width value it receives to the actual width allowed
  // in a given space, pass a copy of nameExtent instead of the real
  // thing, since we're interested in using nameExtent later.
  length = nameLength;
  width = nameExtent;
  FntCharsInWidth(name, &width, &length, &ignored);
  if (nameExtent > width)
    x += (nameExtent - width) / 2;
 
  // Draw the portion of the title string that fits.
  WinDrawInvertedChars(name, length, x, y);

  // Unlock the record that LibGetRecord locked.
  if (NULL != nameH) {
    MemHandleUnlock(nameH);
    if (noName)
      DmReleaseResource(nameH);
  }
 
  // Everything is drawn, so on OS 3.5, it is now time to unlock the
  // form and toss the whole mess back onto the screen.
  if (lockedWindow)
    WinScreenUnlock();
 
  // Restore the colors to their original settings.
  if (SYS_ROM_3_5) {
    WinSetForeColor(oldForeColor);
    WinSetBackColor(oldBackColor);
    WinSetTextColor(oldTextColor);
  }
 
  // Restore the font.
  FntSetFont(oldFont);
}

static void NoteFormUpdateScrollBar()
{
  FormType *form;
  FieldType *field;
  ScrollBarType *bar;
  Int16 maxValue;
  UInt16 scrollPos, textHeight, fieldHeight;
 
  form = FrmGetActiveForm();
  field = FrmGetObjectPtrFromID(form, NoteField);

  FldGetScrollValues(field, &scrollPos, &textHeight, &fieldHeight);
 
  if (textHeight > fieldHeight)
    maxValue = textHeight - fieldHeight;
  else if (scrollPos)
    maxValue = scrollPos;
  else
    maxValue = 0;
 
  bar = FrmGetObjectPtrFromID(form, NoteScrollBar);
  SclSetScrollBar(bar, scrollPos, 0, maxValue, fieldHeight - 1);
}
