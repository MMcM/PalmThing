/*! -*- Mode: C -*-
* Module: AppGlobals.h
* Version: $Header$
*/

/*** Application constants and types ***/

#define _SYS_ROM_3_0 sysMakeROMVersion(3,0,0,sysROMStageRelease,0)
#define _SYS_ROM_3_5 sysMakeROMVersion(3,5,0,sysROMStageRelease,0)
#define _SYS_ROM_4_0 sysMakeROMVersion(4,0,0,sysROMStageRelease,0)

#ifndef SYS_ROM_MIN
#define SYS_ROM_MIN _SYS_ROM_3_0
#endif

#if 1
#define SYS_ROM_3_0 true
#else
#define SYS_ROM_3_0 (g_ROMVersion >= _SYS_ROM_3_0)
#endif

#if 0
#define SYS_ROM_3_5 false
#else
#define SYS_ROM_3_5 (g_ROMVersion >= _SYS_ROM_3_5)
#endif

#if 0
#define SYS_ROM_4_0 false
#else
#define SYS_ROM_4_0 (g_ROMVersion >= _SYS_ROM_4_0)
#endif

#define APP_CREATOR 'plTN'      // Think SAMPA.
#define APP_PREF_ID 0x00
#define APP_PREF_VER 0x01

typedef struct {
  FontID listFont, viewFont, editFont, noteFont, isbnFont;
  UInt8 listFields;
  Boolean incrementalFind;
  Boolean viewSummary;
} AppPreferences;

#define countof(x) (sizeof(x)/sizeof(x[0]))
#define offsetof(t,f) ((UInt32)&(((t *)0)->f))
#define fsizeof(t,f) sizeof(((t *)0)->f)

#define NO_RECORD 0xFFFF

/** Fields of a book record, in the order in which they are stored. **/
enum {
  FIELD_TITLE,
  FIELD_AUTHOR,
  FIELD_DATE,
  FIELD_ISBN,
  FIELD_PUBLICATION,
  FIELD_TAGS,
  FIELD_SUMMARY,
  FIELD_COMMENTS,

  BOOK_NFIELDS
};

#define NO_FIELD 0xFFFF

/** Unpacked representation for easy manipulation. **/
typedef struct {
  UInt32 bookID;
#ifdef UNICODE
  UInt16 unicodeMask;
#endif
  Char *fields[BOOK_NFIELDS];
} BookRecord;

#ifdef UNICODE
#define BookRecordFieldIsUnicode(rec,i) (0 != ((rec)->unicodeMask & (1<<(i))))
#endif

/** Record key fields.
 * Can specify either database record sort order or list column
 * display.  Some values really only make sense for one or the other,
 * of course.
 **/
enum {
  KEY_NONE,
  KEY_TITLE,
  KEY_AUTHOR,
  KEY_ISBN,
  KEY_SUMMARY,
  KEY_TITLE_AUTHOR,
  KEY_AUTHOR_TITLE
};

typedef struct {
  AppInfoType categories;
  Int8 sortFields;
} BookAppInfo;

enum {
  FIND_NONE,
  FIND_ALL,
  FIND_TITLE,
  FIND_AUTHOR,
  FIND_ISBN,
  FIND_TAGS,
  FIND_COMMENTS
};

typedef struct {
  UInt8 findType;
  const Char *findKey;
  Char *keyPrep;
  UInt16 matchField;
  UInt32 matchPos;
  UInt16 matchLen;
} BookFilter;


typedef struct {
  BookFilter filter;
  UInt16 currentRecord;
  UInt16 amountRemaining;
  Int16 direction;
  UInt16 yieldCount;
} BookSeekState;

/*** Application globals ***/

extern UInt32 g_ROMVersion;
extern UInt16 g_EventInterval, g_CurrentCategory, g_CurrentRecord;
extern Boolean g_CurrentRecordEdited;

#include "Sections.h"

/*** Application routines ***/

extern void AboutFormDisplay();
extern FieldType *GetFocusField();

/*** List form routines ***/
extern void ListFormDowngrade(FormType *form) LIST_SECTION;
extern void ListFormSetup(AppPreferences *prefs, BookAppInfo *appInfo) LIST_SECTION;
extern void ListFormSetdown(AppPreferences *prefs) LIST_SECTION;
extern Boolean ListFormHandleEvent(EventType *event) LIST_SECTION;
extern void ListFormDrawTitle(BookRecord *record, RectangleType *bounds,
                              UInt16 listFields) LIST_SECTION;
extern Boolean PreferencesFormHandleEvent(EventType *event) LIST_SECTION;

/*** View form routines ***/
extern void ViewFormSetup(AppPreferences *prefs, BookAppInfo *appInfo) VIEW_SECTION;
extern void ViewFormSetdown(AppPreferences *prefs) VIEW_SECTION;
extern Boolean ViewFormHandleEvent(EventType *event) VIEW_SECTION;
extern void ViewFormActivate(BookFilter *filter) VIEW_SECTION;
extern UInt16 ViewFormGoToPrepare(GoToParamsPtr params) VIEW_SECTION;

/*** Edit form routines ***/
extern void EditFormSetup(AppPreferences *prefs, BookAppInfo *appInfo) EDIT_SECTION;
extern void EditFormSetdown(AppPreferences *prefs) EDIT_SECTION;
extern Boolean EditFormHandleEvent(EventType *event) EDIT_SECTION;
extern void EditFormActivate() EDIT_SECTION;
extern void EditFormNewRecord() EDIT_SECTION;

/*** Note form routines ***/
extern void NoteFormSetup(AppPreferences *prefs, BookAppInfo *appInfo) NOTE_SECTION;
extern void NoteFormSetdown(AppPreferences *prefs) NOTE_SECTION;
extern Boolean NoteFormHandleEvent(EventType *event) NOTE_SECTION;
extern void NoteFormActivate() NOTE_SECTION;
extern UInt16 NoteFormGoToPrepare(GoToParamsPtr params) NOTE_SECTION;

/*** ISBN form routines ***/
extern void ISBNFormSetup(AppPreferences *prefs, BookAppInfo *appInfo) ISBN_SECTION;
extern void ISBNFormSetdown(AppPreferences *prefs) ISBN_SECTION;
extern Boolean ISBNFormHandleEvent(EventType *event) ISBN_SECTION;
extern void ISBNFormActivate() ISBN_SECTION;

/*** Book database routines ***/
extern Err BookDatabaseOpen();
extern Err BookDatabaseClose();
extern BookAppInfo *BookDatabaseGetAppInfo();
extern Err BookDatabaseGetRecord(UInt16 index, MemHandle *recordH, BookRecord *record);
extern Boolean BookRecordHasField(UInt16 index, UInt16 fieldIndex);
extern Err BookRecordGetField(UInt16 index, UInt16 fieldIndex, 
                              MemHandle *dataH, UInt16 *dataOffset, UInt16 *dataLen);
extern Err BookDatabaseNewRecord(UInt16 *index, BookRecord *record);
extern Err BookDatabaseSaveRecord(UInt16 *index, MemHandle *recordH, BookRecord *record);
extern Err BookDatabaseDirtyRecord(UInt16 index);
extern Err BookDatabaseDeleteRecord(UInt16 *index, Boolean archive);
extern Boolean BookDatabaseSeekRecord(UInt16 *index, UInt16 offset, Int16 direction,
                                      UInt16 category);
extern Boolean BookDatabaseSeekRecordFiltered(BookSeekState *seekState, UInt16 category);
extern Boolean BookRecordFilterMatch(UInt16 index, BookFilter *filter);
extern Char *BookDatabaseGetCategoryName(UInt16 index);
extern Boolean BookRecordIsEmpty(BookRecord *record);
extern void BookDatabaseSelectCategory(FormType *form, UInt16 ctlID, UInt16 lstID,
                                       Boolean all, UInt16 *category);
extern UInt16 BookRecordGetCategory(UInt16 index);
extern Err BookRecordSetCategory(UInt16 index, UInt16 category);
extern Char *BookRecordGetCategoryName(UInt16 index);
extern UInt16 BookDatabaseGetSortFields();
extern void BookDatabaseSetSortFields(Int16 sortFields);
extern Err BookDatabaseFind(FindParamsPtr params, UInt16 headerRsc, 
                            void (*drawRecord)(BookRecord*, RectangleType*, UInt16));

/*** Exchange routines ***/

extern Boolean WebEnabled();
extern Err WebGotoCurrentBook();

#ifdef UNICODE
extern Boolean BookDatabaseIsUnicode();

/*** Unicode routines ***/
extern Err UnicodeInitialize(UInt16 launchFlags);
extern void UnicodeTerminate(UInt16 launchFlags);
extern void UnicodeSizeSingleLine(const Char *str, UInt16 len, 
                                  Int16 *width, Int16 *height, 
                                  FontID *font);
extern void UnicodeDrawSingleLine(const Char *str, UInt16 len, 
                                  Int16 x, Int16 y, 
                                  Int16 *width, Int16 *height);
extern Boolean UnicodeDrawField(const Char *str, UInt16 *offset, UInt16 len, 
                                UInt16 *ndrawn, Int16 *y, RectangleType *bounds);

#endif
