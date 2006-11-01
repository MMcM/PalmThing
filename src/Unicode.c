/*! -*- Mode: C -*-
* Module: Unicode.c
* Version: $Header$
*/

#include <PalmOS.h>
#include <PalmOSGlue.h>

#include "AppResources.h"
#include "AppGlobals.h"

/**** Whole file is conditional ****/
#ifdef UNICODE

#include <VFSMgr.h>
#include <UniLibDefines.h>
#include <UniLibTypes.h>
#include <UniLibFunctionsExtern.h>

static UniBucketType g_UniBucket;
static UInt8 g_FoundUniCharDB, g_FoundMappingDB;
static Boolean g_UnicodeInitialized = false;

Err UnicodeInitialize()
{
  Err error;

  if (!BookDatabaseIsUnicode()) return errNone;

  MemSet(&g_UniBucket, sizeof(g_UniBucket), 0);
  g_UniBucket.fontIDOffset = kFontIDOffset;
  g_UniBucket.maxActiveFontIDs = kMaxActiveFontIDs;
  error = UniBucketOpenUnicode(&g_UniBucket, false, NULL, 0, 
                               &g_FoundUniCharDB, true, &g_FoundMappingDB);
  if (error) return error;

  if (!g_FoundUniCharDB) {
    SysFatalAlert("UnicodeCharDB (UniD/UniD) not found. Please install and try again. Downloads at www.unboundbible.org/unibible.");
    return dmErrCantFind;
  }
  if (!g_FoundMappingDB) {
    SysFatalAlert("UnicodeMappingsDB (UniD/CMAP) not Found. Please install and try again. Downloads at www.unboundbible.org/unibible.");
    return dmErrCantFind;
  }

  g_UniBucket.flag[FLAG_RENDER_HEBREW_WITH_PRESENTATION_FORMS] = true;
  
  return error;
}

void UnicodeTerminate()
{
  if (g_UnicodeInitialized)
    UniBucketCloseUnicode(&g_UniBucket, g_FoundUniCharDB, g_FoundMappingDB);
}

#endif
