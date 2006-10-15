/*! -*- Mode: C -*-
* Module: AppGlobals.h
* Version: $Header$
*/

/*** Application constants and types ***/

#define countof(x) (sizeof(x)/sizeof(x[0]))
#define offsetof(t,f) ((UInt32)&(((t *)0)->f))

#define SYS_ROM_3_0 sysMakeROMVersion(3,0,0,sysROMStageRelease,0)
#define SYS_ROM_3_5 sysMakeROMVersion(3,5,0,sysROMStageRelease,0)
#define SYS_ROM_4_0 sysMakeROMVersion(4,0,0,sysROMStageRelease,0)

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
  UInt8 listFields;
  UInt8 sortFields;
} BookAppInfo;

/*** Application globals ***/

extern UInt32 g_ROMVersion;
extern UInt16 g_CurrentRecord, g_CurrentCategory;

/*** Application routines ***/

extern void AboutFormDisplay();

/*** List form routines ***/
extern void ListFormSetup(BookAppInfo *appInfo);
extern Boolean ListFormHandleEvent(EventType *event);

/*** View form routines ***/
extern void ViewFormSetup(BookAppInfo *appInfo);
extern Boolean ViewFormHandleEvent(EventType *event);
extern void ViewFormActivate();

/*** Edit form routines ***/
extern void EditFormSetup(BookAppInfo *appInfo);
extern Boolean EditFormHandleEvent(EventType *event);
extern void EditFormActivate();
extern void EditFormNewRecord();

/*** Note form routines ***/
extern void NoteFormSetup(BookAppInfo *appInfo);
extern Boolean NoteFormHandleEvent(EventType *event);

/*** Book database routines ***/
extern Err BookDatabaseOpen();
extern Err BookDatabaseClose();
extern BookAppInfo *BookDatabaseGetAppInfo();
extern Err BookDatabaseGetRecord(UInt16 index, MemHandle *recordH, BookRecord *record);
extern Err BookRecordGetField(UInt16 index, UInt16 fieldIndex, 
                              MemHandle *dataH, UInt16 *dataOffset, UInt16 *dataLen);
extern Err BookDatabaseNewRecord(UInt16 *index, BookRecord *record);
extern Err BookDatabaseSaveRecord(UInt16 *index, MemHandle *recordH, BookRecord *record);
extern Err BookDatabaseDeleteRecord(UInt16 *index, Boolean archive);
extern Boolean BookDatabaseSeekRecord(UInt16 *index, Int16 offset, Int16 direction);
extern Char *BookDatabaseGetCategoryName(UInt16 index);
extern Boolean BookRecordIsEmpty(BookRecord *record);
extern void BookDatabaseSelectCategory(FormType *form, UInt16 ctlID, UInt16 lstID,
                                       Boolean all, UInt16 *category);
extern UInt16 BookRecordGetCategory(UInt16 index);
extern Err BookRecordSetCategory(UInt16 index, UInt16 category);
extern Char *BookRecordGetCategoryName(UInt16 index);
