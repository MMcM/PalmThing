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

#if 0
#define SYS_ROM_3_0 false
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
  Char *fields[BOOK_NFIELDS];
} BookRecord;

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
} BookFindState;

/*** Application globals ***/

extern UInt32 g_ROMVersion;
extern UInt16 g_CurrentRecord, g_CurrentCategory;

/*** Application routines ***/

extern void AboutFormDisplay();
extern FieldType *GetFocusField();

/*** List form routines ***/
extern void ListFormSetup(AppPreferences *prefs, BookAppInfo *appInfo);
extern void ListFormSetdown(AppPreferences *prefs);
extern Boolean ListFormHandleEvent(EventType *event);
extern Boolean PreferencesFormHandleEvent(EventType *event);

/*** View form routines ***/
extern void ViewFormSetup(AppPreferences *prefs, BookAppInfo *appInfo);
extern void ViewFormSetdown(AppPreferences *prefs);
extern Boolean ViewFormHandleEvent(EventType *event);
extern void ViewFormActivate();

/*** Edit form routines ***/
extern void EditFormSetup(AppPreferences *prefs, BookAppInfo *appInfo);
extern void EditFormSetdown(AppPreferences *prefs);
extern Boolean EditFormHandleEvent(EventType *event);
extern void EditFormActivate();
extern void EditFormNewRecord();

/*** Note form routines ***/
extern void NoteFormSetup(AppPreferences *prefs, BookAppInfo *appInfo);
extern void NoteFormSetdown(AppPreferences *prefs);
extern void NoteFormActivate();
extern Boolean NoteFormHandleEvent(EventType *event);

/*** ISBN form routines ***/
extern void ISBNFormSetup(AppPreferences *prefs, BookAppInfo *appInfo);
extern void ISBNFormSetdown(AppPreferences *prefs);
extern Boolean ISBNFormHandleEvent(EventType *event);
extern void ISBNFormActivate();

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
extern Boolean BookDatabaseSeekRecord(UInt16 *index, Int16 offset, Int16 direction,
                                      UInt16 category, BookFindState *findState);
extern Boolean BookRecordFindMatch(UInt16 index, BookFindState *findState);
extern Char *BookDatabaseGetCategoryName(UInt16 index);
extern Boolean BookRecordIsEmpty(BookRecord *record);
extern void BookDatabaseSelectCategory(FormType *form, UInt16 ctlID, UInt16 lstID,
                                       Boolean all, UInt16 *category);
extern UInt16 BookRecordGetCategory(UInt16 index);
extern Err BookRecordSetCategory(UInt16 index, UInt16 category);
extern Char *BookRecordGetCategoryName(UInt16 index);
extern UInt16 BookDatabaseGetSortFields();
extern void BookDatabaseSetSortFields(Int16 sortFields);
