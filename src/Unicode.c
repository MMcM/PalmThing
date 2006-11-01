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

void UnicodeSizeSingleLine(const Char *str, UInt16 len, 
                           Int16 *width, Int16 *height)
{
  const UTF8 *utf8, *start8, *end8;
  UTF16 *utf16, *start16, *end16;
  UInt16 lw, lh;
  ConversionResult cr;

  utf8 = (const UTF8 *)str;
  len++;                        // Include terminating nul.
  utf16 = (UTF16 *)MemPtrNew(len * 2);
  if (NULL == utf16) {
    *height = g_UniBucket.lineHeight;
  }
  start8 = utf8;
  end8 = start8 + len;
  start16 = utf16;
  end16 = start16 + len;
  cr = ConvertUTF8toUTF16(&start8, end8, &start16, end16, lenientConversion);
  UniStrUniCharPrintLine(&g_UniBucket, 16, utf16, 
                         0, 0, *height, *width,
                         UNI_TEXT_DIR_LR, 0, 0, 
                         &lw, &lh, false);
  MemPtrFree(utf16);

  *width = lw;
  *height = lh;
}

void UnicodeDrawSingleLine(const Char *str, UInt16 len, 
                           Int16 x, Int16 y, 
                           Int16 *width, Int16 *height)
{
  const UTF8 *utf8, *start8, *end8;
  UTF16 *utf16, *start16, *end16;
  UInt16 lw, lh;
  ConversionResult cr;

  utf8 = (const UTF8 *)str;
  len++;                        // Include terminating nul.
  utf16 = (UTF16 *)MemPtrNew(len * sizeof(UTF16));
  if (NULL == utf16) return;
  start8 = utf8;
  end8 = start8 + len;
  start16 = utf16;
  end16 = start16 + len;
  cr = ConvertUTF8toUTF16(&start8, end8, &start16, end16, lenientConversion);
  UniStrUniCharPrintLine(&g_UniBucket, 16, utf16, 
                         y, x, *height, *width,
                         UNI_TEXT_DIR_LR, winPaint, winOverlay, 
                         &lw, &lh, true);
  MemPtrFree(utf16);

  *width = lw;
  *height = lh;
}

#endif
