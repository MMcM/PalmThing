/*! -*- Mode: C -*-
* Module: BookDatabase.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants and types ***/

enum { DB_VER_0 = 0 };
#define DB_VER DB_VER_0

#define DB_NAME "PalmThing-Books"
#define DB_TYPE 'DATA'          // Main book database.

typedef struct {
  UInt32 bookID;
  UInt16 fieldMask;
  Char fields[0];
} BookRecordPacked;

// TODO: Could move to prefs.
#define SEEK_YIELD_QUANTUM 100

/*** Local storage ***/

static DmOpenRef g_BookDatabase = NULL;
static Char g_CategoryName[dmCategoryLength];

/*** Local routines ***/

static Int16 BookRecordCompare(void *p1, void *p2, 
                               Int16 sortKey,
                               SortRecordInfoType *sinfo1, SortRecordInfoType *sinfo2,
                               MemHandle appInfoH);

static UInt16 BookRecordPackedSize(BookRecord *record)
{
  UInt16 result;
  int i;
  Char *p;

  result = sizeof(BookRecordPacked);
  for (i = 0; i < BOOK_NFIELDS; i++) {
    if (NULL != (p = record->fields[i])) {
      result += StrLen(p) + 1;
    }
  }
  return result;
}

static void PackRecord(BookRecord *record, BookRecordPacked *packed)
{
  UInt16 mask;
  UInt32 offset;
  UInt16 len;
  int i;
  Char *p;

  DmWrite(packed, offsetof(BookRecordPacked,bookID), 
          &record->bookID, sizeof(packed->bookID));
  
  mask = 0;
  offset = offsetof(BookRecordPacked,fields);
  for (i = 0; i < BOOK_NFIELDS; i++) {
    if (NULL != (p = record->fields[i])) {
      mask |= (1 << i);
      len = StrLen(p) + 1;
      DmWrite(packed, offset, p, len);
      offset += len;
    }
  }
  DmWrite(packed, offsetof(BookRecordPacked,fieldMask), &mask, sizeof(packed->fieldMask));
}

static void UnpackRecord(BookRecordPacked *packed, BookRecord *record)
{
  int i;
  Char *p;
  
  record->bookID = packed->bookID;
  
  p = packed->fields;
  for (i = 0; i < BOOK_NFIELDS; i++) {
    if (packed->fieldMask & (1 << i)) {
      record->fields[i] = p;
      p += StrLen(p) + 1;
    }
    else {
      record->fields[i] = NULL;
    }
  }
}

Err BookDatabaseOpen()
{
  DmOpenRef db;
  UInt16 cardNo, attr, dbVersion;
  UInt8 fields;
  LocalID dbID, appInfoID;
  MemHandle appInfoH;
  BookAppInfo *appInfo;
  Err error;
  
  db = DmOpenDatabaseByTypeCreator(DB_TYPE, APP_CREATOR, dmModeReadWrite);
  if (NULL == db) {
    error = DmCreateDatabase(0, DB_NAME, APP_CREATOR, DB_TYPE, false);
    if (error) return error;

    db = DmOpenDatabaseByTypeCreator(DB_TYPE, APP_CREATOR, dmModeReadWrite);
    if (NULL == db) return DmGetLastErr();

    error = DmOpenDatabaseInfo(db, &dbID, NULL, NULL, &cardNo, NULL);
    if (error) return error;

    error = DmDatabaseInfo(cardNo, dbID, NULL, &attr, NULL, NULL,
                           NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (error) return error;

    attr |= dmHdrAttrBackup;

    dbVersion = DB_VER;

    appInfoH = DmNewHandle(db, sizeof(BookAppInfo));
    if (NULL == appInfoH) {
      DmCloseDatabase(db);
      DmDeleteDatabase(cardNo, dbID);
      return dmErrMemError;
    }
  
    appInfoID = MemHandleToLocalID(appInfoH);
    appInfo = (BookAppInfo *)MemHandleLock(appInfoH);
    DmSet(appInfo, 0, sizeof(BookAppInfo), 0);
    CategoryInitialize(&appInfo->categories, CategoryAppInfoStr);
    fields = KEY_TITLE_AUTHOR;
    DmWrite(appInfo, offsetof(BookAppInfo,sortFields), 
            &fields, sizeof(appInfo->sortFields));
    MemHandleUnlock(appInfoH);

    error = DmSetDatabaseInfo(cardNo, dbID, 
                              NULL, &attr, &dbVersion,
                              NULL, NULL, NULL, NULL, 
                              &appInfoID, NULL, NULL, NULL);
    if (error) return error;
  }
  g_BookDatabase = db;
  return errNone;
}

Err BookDatabaseClose()
{
  return DmCloseDatabase(g_BookDatabase);
}

BookAppInfo *BookDatabaseGetAppInfo()
{
  UInt16 cardNo;
  LocalID dbID, appInfoID;
  Err error;

  error = DmOpenDatabaseInfo(g_BookDatabase, &dbID, NULL, NULL, &cardNo, NULL);
  if (error) return NULL;

  error = DmDatabaseInfo(cardNo, dbID, NULL, NULL, NULL, NULL,
                         NULL, NULL, NULL, &appInfoID, NULL, NULL, NULL);
  if (error) return NULL;

  return MemLocalIDToLockedPtr(appInfoID, cardNo);
}

/** Returns with handle locked. **/
Err BookDatabaseGetRecord(UInt16 index, MemHandle *recordH, BookRecord *record)
{
  BookRecordPacked *packed;

  *recordH = DmQueryRecord(g_BookDatabase, index);
  if (NULL == *recordH)
    return DmGetLastErr();

  packed = (BookRecordPacked *)MemHandleLock(*recordH);
  UnpackRecord(packed, record);

  return errNone;
}

Boolean BookRecordHasField(UInt16 index, UInt16 fieldIndex)
{
  MemHandle recordH;
  BookRecordPacked *packed;
  Boolean result;

  recordH = DmQueryRecord(g_BookDatabase, index);
  if (NULL == recordH)
    return false;

  packed = (BookRecordPacked *)MemHandleLock(recordH);
  result = (packed->fieldMask & (1 << fieldIndex)) != 0;
  MemHandleUnlock(recordH);

  return result;
}

/** Get a single field without unpacking. **/
Err BookRecordGetField(UInt16 index, UInt16 fieldIndex, 
                       MemHandle *dataH, UInt16 *dataOffset, UInt16 *dataLen)
{
  BookRecordPacked *packed;
  Char *p;
  int i;
  UInt16 len;

  *dataH = DmQueryRecord(g_BookDatabase, index);
  if (NULL == *dataH)
    return DmGetLastErr();

  packed = (BookRecordPacked *)MemHandleLock(*dataH);
  if (!(packed->fieldMask & (1 << fieldIndex))) {
    MemHandleUnlock(*dataH);
    *dataOffset = MemHandleSize(*dataH);
    *dataLen = 0;
    return errNone;
  }
  
  p = packed->fields;
  for (i = 0; i < BOOK_NFIELDS; i++) {
    if (packed->fieldMask & (1 << i)) {
      len = StrLen(p) + 1;
      if (i == fieldIndex) {
        *dataOffset = p - (Char *)packed;
        *dataLen = len;
        break;
      }
      p += len;
    }
  }
  MemHandleUnlock(*dataH);
  return errNone;
}

static inline UInt16 BookRecordFindSortPosition(BookRecordPacked *packed)
{
  return DmFindSortPosition(g_BookDatabase, packed, NULL, BookRecordCompare, 0);
}

Err BookDatabaseNewRecord(UInt16 *index, BookRecord *record)
{
  MemHandle recordH;
  BookRecordPacked *packed;
  UInt16 nindex;
  Err error;

  if (NULL == record) {
    // No initial record, can do this more easily.
    BookRecord empty;
    MemSet(&empty, sizeof(empty), 0);
    record = &empty;

    nindex = dmMaxRecordIndex;  // Or would 0 be better?
    recordH = DmNewRecord(g_BookDatabase, &nindex, BookRecordPackedSize(record));
    if (NULL == recordH)
      return DmGetLastErr();
    
    packed = (BookRecordPacked *)MemHandleLock(recordH);
    PackRecord(record, packed);
    MemHandleUnlock(recordH);
    
    DmReleaseRecord(g_BookDatabase, nindex, true);
    *index = nindex;
    return errNone;
  }

  // Done the hard way because we can't find the sorted position
  // easily without the packed representation, for which we need
  // space.

  recordH = DmNewHandle(g_BookDatabase, BookRecordPackedSize(record));
  if (NULL == recordH)
    return DmGetLastErr();

  packed = (BookRecordPacked *)MemHandleLock(recordH);
  PackRecord(record, packed);

  nindex = BookRecordFindSortPosition(packed);
  MemHandleUnlock(recordH);

  // No outHP -> insert at index.
  error = DmAttachRecord(g_BookDatabase, &nindex, recordH, NULL);
 if (error) 
    MemHandleFree(recordH);
  else
    *index = nindex;

  return error;
}

Err BookDatabaseSaveRecord(UInt16 *index, MemHandle *recordH, BookRecord *record)
{
  BookAppInfo *appInfo;
  Int16 sortFields;
  MemHandle nrecordH, orecordH;
  BookRecordPacked *packed, *opacked;
  Boolean move;
  UInt16 nindex;
  Err error;

  // Again the hard part is getting the sorted position right, which requires
  // packing.

  appInfo = BookDatabaseGetAppInfo();
  sortFields = appInfo->sortFields;
  MemPtrUnlock(appInfo);

  nrecordH = DmNewHandle(g_BookDatabase, BookRecordPackedSize(record));
  if (NULL == nrecordH)
    return dmErrMemError;

  packed = (BookRecordPacked *)MemHandleLock(nrecordH);
  PackRecord(record, packed);
  if ((NULL != recordH) && (NULL != *recordH)) {
    // If caller got record from recordH via BookDatabaseGetRecord, we're done with that.
    MemHandleUnlock(*recordH);
    *recordH = NULL;
  }

  move = false;

  if (*index > 0) {
    orecordH = DmQueryRecord(g_BookDatabase, *index + 1);
    if (NULL == orecordH)
      move = true;
    else {
      opacked = (BookRecordPacked *)MemHandleLock(orecordH);
      if (BookRecordCompare(packed, opacked, sortFields,
                            NULL, NULL, NULL) > 0)
        move = true;
      MemHandleUnlock(orecordH);
    }
  }

  if (move) {
    nindex = BookRecordFindSortPosition(packed);
    DmMoveRecord(g_BookDatabase, *index, nindex);
    if (nindex > *index)
      nindex--;
    *index = nindex;
  }
    
  MemHandleUnlock(nrecordH);

  error = DmAttachRecord(g_BookDatabase, index, nrecordH, &orecordH);
  if (error) 
    MemHandleFree(nrecordH);
  else
    MemHandleFree(orecordH);

  return error;
}

Err BookDatabaseDirtyRecord(UInt16 index)
{
  UInt16  attr;
  Err error;
 
  error = DmRecordInfo(g_BookDatabase, index, &attr, NULL, NULL);
  if (error) return error;
  attr |= dmRecAttrDirty;
  error = DmSetRecordInfo(g_BookDatabase, index, &attr, NULL);
  return error;
}

Err BookDatabaseDeleteRecord(UInt16 *index, Boolean archive)
{
  UInt16 nindex;
  Err error;

  // Delete or archive the record.
  if (archive)
    error = DmArchiveRecord(g_BookDatabase, *index);
  else
    error = DmDeleteRecord(g_BookDatabase, *index);
  if (error) return error;
 
  // Deleted records are stored at the end of the database.
  nindex = DmNumRecords(g_BookDatabase);
  error = DmMoveRecord(g_BookDatabase, *index, nindex);
  if (error) return error;
  
  *index = nindex;
  return errNone;
}

Boolean BookDatabaseSeekRecord(UInt16 *index, Int16 offset, Int16 direction,
                               UInt16 category, BookSeekState *seekState)
{
  Boolean found;

  if (NULL == seekState)
    return (errNone == DmSeekRecordInCategory(g_BookDatabase, index, offset, direction,
                                              category));
  
  found = true;
  while (true) {
    if (errNone != DmSeekRecordInCategory(g_BookDatabase, index, 
                                          (0 == offset) ? 0 : 1, 
                                          direction, category)) {
      found = false;
      break;
    }

    if (!BookRecordFilterMatch(*index, &seekState->filter)) {
      if (offset == 0) offset = 1;
      continue;
    }

    if (--offset <= 0) 
      break;
  }

  return found;
}

static Boolean BookRecordPackedFilterMatch(BookRecordPacked *packed, BookFilter *filter)
{
  const Char *fldp, *keyp, *valp;
  Char *outp, ch;
  UInt16 len;
  UInt16 searchMask;
  int i;
  Boolean match;

  if (NULL == filter->keyPrep) {
    len = StrLen(filter->findKey);
    switch (filter->findType) {
    case FIND_NONE:
    case FIND_ISBN:
      break;
      
    case FIND_TAGS:
      // Prep'ed key is null-terminated strings between commas, followed by another nul.
      outp = filter->keyPrep = (Char *)MemPtrNew(len + 1);
      keyp = filter->findKey;
      while (true) {
        ch = *keyp++;
        if ('\0' == ch) break;
        if (',' == ch) {
          *outp++ = '\0';
          while (' ' == *keyp) keyp++;
        }
        *outp++ = ch;
      }
      *outp++ = '\0';
      *outp++ = '\0';
      break;

    default:
      filter->keyPrep = (Char *)MemPtrNew(len + 1);
      if (NULL == filter->keyPrep) return false;
      TxtGluePrepFindString(filter->findKey, filter->keyPrep, len + 1);
    }
  }

  switch (filter->findType) {
  case FIND_NONE:
  default:
    searchMask = 0;
    break;

  case FIND_ALL:
    searchMask = (1 << FIELD_TITLE) | (1 << FIELD_AUTHOR) | (1 << FIELD_PUBLICATION) |
      (1 << FIELD_SUMMARY) | (1 << FIELD_COMMENTS);
    break;

  case FIND_TITLE:
    searchMask = (1 << FIELD_TITLE);
    break;

  case FIND_AUTHOR:
    searchMask = (1 << FIELD_AUTHOR);
    break;

  case FIND_ISBN:
    searchMask = (1 << FIELD_ISBN);
    break;

  case FIND_TAGS:
    searchMask = (1 << FIELD_TAGS);
    break;

  case FIND_COMMENTS:
    searchMask = (1 << FIELD_COMMENTS);
    break;
  }

  searchMask &= packed->fieldMask;
  if (0 == searchMask)
    return false;

  match = false;
  fldp = packed->fields;
  for (i = 0; i < BOOK_NFIELDS; i++) {
    len = StrLen(fldp);
    if (searchMask & (1 << i)) {
      switch (filter->findType) {
      case FIND_NONE:
        break;

      case FIND_ISBN:
        // Caseless compare ignoring the - character.
        // Could just go digits with special case for 'X'.
        keyp = filter->findKey;
        valp = fldp;
        while (true) {
          while ('-' == *keyp) keyp++;
          while ('-' == *valp) valp++;
          if ('\0' == *keyp) {
            match = true;
            break;
          }
          if ('\0' == *valp)
            break;
          if (TxtGlueUpperChar(*keyp++) != TxtGlueUpperChar(*valp++))
            break;
        }
        break;
      
      case FIND_TAGS:
        // Must have all given keys as left substrings of some tag.
        match = true;
        keyp = filter->keyPrep;
        while ('\0' != *keyp) {
          valp = fldp;
          while (true) {
            valp = StrStr(valp, keyp);
            if (NULL == valp) {
              match = false;
              break;
            }
            if ((valp == fldp) ||
                (',' == *(valp-1)) ||
                ((' ' == *(valp-1)) &&
                 (',' == *(valp-2)))) {
              if (keyp == filter->keyPrep) {
                // Match indicator to first such.
                filter->matchPos = (valp - fldp);
                filter->matchLen = StrLen(keyp);
              }
              break;
            }
            valp++;             // False match within some tag.
          }
          if (!match) break;
          keyp += StrLen(keyp) + 1; // Next key tag.
        }
        break;

      default:
        match = TxtGlueFindString(fldp, filter->keyPrep,
                                  &filter->matchPos, &filter->matchLen);
      }
      if (match) {
        filter->matchField = i;
        break;
      }
      searchMask &= ~(1 << i);
      if (0 == searchMask)
        break;
    }
    if (packed->fieldMask & (1 << i))
      fldp += len + 1;
  }
  
  return match;
}

Boolean BookRecordFilterMatch(UInt16 index, BookFilter *filter)
{
  MemHandle recordH;
  BookRecordPacked *packed;
  Boolean match;

  recordH = DmQueryRecord(g_BookDatabase, index);
  if (NULL == recordH)
    return false;

  packed = (BookRecordPacked *)MemHandleLock(recordH);
  match = BookRecordPackedFilterMatch(packed, filter);
  MemHandleUnlock(recordH);

  return match;
}

Char *BookDatabaseGetCategoryName(UInt16 index)
{
  CategoryGetName(g_BookDatabase, index, g_CategoryName);
  return g_CategoryName;
}

void BookDatabaseSelectCategory(FormType *form, UInt16 ctlID, UInt16 lstID,
                                Boolean all, UInt16 *category)
{
  CategorySelect(g_BookDatabase, form, ctlID, lstID, all, category, 
                 g_CategoryName, 1, categoryHideEditCategory);
}

UInt16 BookRecordGetCategory(UInt16 index)
{
  UInt16 attr;
  Err error;

  error = DmRecordInfo(g_BookDatabase, index, &attr, NULL, NULL);
  if (errNone != error)
    return dmUnfiledCategory;

  return (attr & dmRecAttrCategoryMask);
}

Err BookRecordSetCategory(UInt16 index, UInt16 category)
{
  UInt16 attr;
  Err error;

  error = DmRecordInfo(g_BookDatabase, index, &attr, NULL, NULL);
  if (errNone != error)
    return error;

  attr &= ~dmRecAttrCategoryMask;
  attr |= ((category == dmAllCategories) ? dmUnfiledCategory : category)
    | dmRecAttrDirty;
  return DmSetRecordInfo(g_BookDatabase, index, &attr, NULL);
}

Char *BookRecordGetCategoryName(UInt16 index)
{
  return BookDatabaseGetCategoryName(BookRecordGetCategory(index));
}

Boolean BookRecordIsEmpty(BookRecord *record)
{
  int i;

  if (record->bookID)
    return false;

  for (i = 0; i < BOOK_NFIELDS; i++) {
    if (NULL != record->fields[i])
      return false;
  }

  return true;
}

static void BookRecordCompareGetField(BookRecordPacked *packed, Int16 fieldIndex,
                                      Char **fstr, UInt16 *flen)
{
  Char *p;
  int i;
  UInt16 len;

  if (!(packed->fieldMask & (1 << fieldIndex))) {
    *fstr = NULL;
    *flen = 0;
    return;
  }
    
  p = packed->fields;
  for (i = 0; i < BOOK_NFIELDS; i++) {
    if (packed->fieldMask & (1 << i)) {
      len = StrLen(p) + 1;
      if (i == fieldIndex) {
        *fstr = p;
        *flen = len;
        return;
      }
      p += len;
    }
  }
}

static Int16 BookRecordCompareField(BookRecordPacked *rec1, BookRecordPacked *rec2,
                                    Int16 fieldIndex)
{
  Char *str1, *str2;
  UInt16 len1, len2;

  BookRecordCompareGetField(rec1, fieldIndex, &str1, &len1);
  BookRecordCompareGetField(rec2, fieldIndex, &str2, &len2);
  if (NULL == str1) {
    if (NULL == str2)
      return 0;
    else
      return -1;
  }
  else if (NULL == str2) {
    return +1;
  }
  else {
    return TxtGlueCaselessCompare(str1, len1, NULL, str2, len2, NULL);
  }
}

static Int16 BookRecordCompare(void *p1, void *p2, 
                               Int16 sortKey,
                               SortRecordInfoType *sinfo1, SortRecordInfoType *sinfo2,
                               MemHandle appInfoH)
{
  BookAppInfo *appInfo;
  BookRecordPacked *rec1, *rec2;
  UInt16 f1, f2, comp;
  Boolean reverse;
  
  rec1 = (BookRecordPacked *)p1;
  rec2 = (BookRecordPacked *)p2;

  if (KEY_NONE == sortKey) {
    appInfo = (BookAppInfo *)MemHandleLock(appInfoH);
    sortKey = appInfo->sortFields;
    MemHandleUnlock(appInfoH);
  }

  reverse = false;
  if (sortKey < 0) {
    reverse = true;
    sortKey = -sortKey;
  }

  f1 = f2 = NO_FIELD;
  switch (sortKey) {
  case KEY_TITLE:
    f1 = FIELD_TITLE;
    break;
  case KEY_AUTHOR:
    f1 = FIELD_AUTHOR;
    break;
  case KEY_ISBN:
    f1 = FIELD_ISBN;
    break;
  case KEY_SUMMARY:
    f1 = FIELD_SUMMARY;
    break;
  case KEY_TITLE_AUTHOR:
    f1 = FIELD_TITLE;
    f2 = FIELD_AUTHOR;
    break;
  case KEY_AUTHOR_TITLE:
    f1 = FIELD_AUTHOR;
    f2 = FIELD_TITLE;
    break;
  }

  if (NO_FIELD != f1) {
    comp = BookRecordCompareField(rec1, rec2, f1);
    if (comp) 
      return (reverse) ? -comp : comp;
  }
  if (NO_FIELD != f2) {
    comp = BookRecordCompareField(rec1, rec2, f2);
    if (comp) 
      return (reverse) ? -comp : comp;
  }
  return 0;
}

UInt16 BookDatabaseGetSortFields()
{
  BookAppInfo *appInfo;
  UInt16 result;

  appInfo = BookDatabaseGetAppInfo();
  result = appInfo->sortFields;
  MemPtrUnlock(appInfo);

  return result;
}

void BookDatabaseSetSortFields(Int16 sortFields)
{
  BookAppInfo *appInfo;
  Int8 fields;

  appInfo = BookDatabaseGetAppInfo();
  fields = sortFields;
  DmWrite(appInfo, offsetof(BookAppInfo,sortFields), 
          &fields, sizeof(appInfo->sortFields));
  MemPtrUnlock(appInfo);
  
  DmQuickSort(g_BookDatabase, BookRecordCompare, sortFields);
}

Err BookDatabaseFind(FindParamsPtr params, UInt16 headerRsc,
                     void (*drawRecord)(BookRecord*, RectangleType*, UInt16))
{
  DmSearchStateType searchState;
  LocalID dbID;
  UInt16 cardNo;
  DmOpenRef db;
  MemHandle headerH;
  const Char *header;
  BookSeekState seekState;
  UInt16 listFields;
  MemHandle recordH;
  BookRecordPacked *packed;
  BookRecord record;
  RectangleType bounds;
  Err error;
  Boolean done;

  error = DmGetNextDatabaseByTypeCreator(true, &searchState, DB_TYPE, APP_CREATOR, 
                                         true, &cardNo, &dbID);
  if (error) {
    params->more = false;
    return error;
  }
  db = DmOpenDatabase(cardNo, dbID, params->dbAccesMode);
  if (NULL == db) {
    params->more = false;
    return DmGetLastErr();
  }

  headerH = DmGetResource(strRsc, headerRsc);
  header = MemHandleLock(headerH);
  done = FindDrawHeader(params, header);
  MemHandleUnlock(headerH);
  DmReleaseResource(headerH);
  
  listFields = KEY_TITLE_AUTHOR; // TODO: Move from prefs to db?

  seekState.filter.findType = FIND_ALL;
  seekState.filter.findKey = params->strAsTyped;
  seekState.filter.keyPrep = params->strToFind;
  seekState.currentRecord = params->recordNum;
  seekState.yieldCount = SEEK_YIELD_QUANTUM;
  while (!done) {
    if (--seekState.yieldCount == 0) {
      if (EvtSysEventAvail(true)) {
        params->more = true;
        break;
      }
      seekState.yieldCount = SEEK_YIELD_QUANTUM;
    }

    recordH = DmQueryNextInCategory(db, &seekState.currentRecord, dmAllCategories);
    if (NULL == recordH) {
      params->more = false;         
      break;
    }

    packed = (BookRecordPacked *)MemHandleLock(recordH);

    if (BookRecordPackedFilterMatch(packed, &seekState.filter)) {
      done = FindSaveMatch(params, seekState.currentRecord, 
                           seekState.filter.matchPos, seekState.filter.matchField, 
                           seekState.filter.matchLen, cardNo, dbID);
      if (!done) {
        UnpackRecord(packed, &record);
        FindGetLineBounds(params, &bounds);
        FntSetFont(stdFont);
        drawRecord(&record, &bounds, listFields);
        params->lineNumber++;
      }
    }

    MemHandleUnlock(recordH);
    seekState.currentRecord++;
  }

  DmCloseDatabase(db);
  return error;
}
