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

static UniBucketPtr g_UniBucket;
static UInt8 g_FoundUniCharDB, g_FoundMappingDB, g_FoundKeyMapDB, g_FoundKeyboardDB;
static Boolean g_UnicodeInitialized = false;

#define NUM_FONT_PATHS 5
static const char *FONT_PATHS[NUM_FONT_PATHS] = {
  "/Palm/Launcher/",
  "/Palm/Programs/",
  "/Palm/Programs/UniLib/",
  "/Palm/Programs/PalmThing/",
  "/Palm/Programs/UniBible/"
};
static DmVFSPathType g_FontPaths[NUM_FONT_PATHS];

Err UnicodeInitialize(UInt16 launchFlags)
{
  int i;
  UInt32 vfsMgrVersion;
  UInt8 foundVFS;
  Err error;

  if (!BookDatabaseIsUnicode()) return errNone;

  g_UniBucket = (UniBucketPtr)MemPtrNew(sizeof(UniBucketType));
  if (NULL == g_UniBucket) return memErrNotEnoughSpace;

  MemSet(g_UniBucket, sizeof(g_UniBucket), 0);
  g_UniBucket->fontIDOffset = kFontIDOffset;
  g_UniBucket->maxActiveFontIDs = kMaxActiveFontIDs;

  error = FtrGet(sysFileCVFSMgr, vfsFtrIDVersion, &vfsMgrVersion);
  foundVFS = (!error && vfsMgrVersion);
  if (foundVFS) {
    g_UniBucket->uniCharDB.tryVFS = foundVFS;
    g_UniBucket->uniMapDB.tryVFS = foundVFS;
    for (i = 0; i < NUM_FONT_PATHS; i++) {
      StrCopy(g_FontPaths[i].path, FONT_PATHS[i]);
    }
  }

  // Don't need mappings database (already UTF-8) or any keyboard stuff (yet).
  g_FoundMappingDB = g_FoundKeyMapDB = g_FoundKeyboardDB = false;
  error = UniBucketOpenUnicode(g_UniBucket, foundVFS, g_FontPaths, NUM_FONT_PATHS, 
                               &g_FoundUniCharDB, &g_FoundMappingDB,
                               &g_FoundKeyMapDB, &g_FoundKeyboardDB);
  if (error) {
    MemPtrFree(g_UniBucket);
    return error;
  }

  if (!g_FoundUniCharDB) {
    SysFatalAlert("UnicodeCharDB (UniD/UniD) not found. Please install and try again. Downloads at www.unboundbible.org/unibible.");
    MemPtrFree(g_UniBucket);
    return dmErrCantFind;
  }
#if 0
  if (!g_FoundMappingDB) {
    SysFatalAlert("UnicodeMappingsDB (UniD/CMAP) not Found. Please install and try again. Downloads at www.unboundbible.org/unibible.");
    MemPtrFree(g_UniBucket);
    return dmErrCantFind;
  }
#endif

  g_UniBucket->flag[FLAG_RENDER_HEBREW_WITH_PRESENTATION_FORMS] = true;
  g_UniBucket->flag[FLAG_LINE_BREAK_STYLE] = LINE_BREAK_LF_CR_CRLF;
  
  g_UnicodeInitialized = true;

  return error;
}

void UnicodeTerminate(UInt16 launchFlags)
{
  if (g_UnicodeInitialized) {
    UniBucketCloseUnicode(g_UniBucket, g_FoundUniCharDB, g_FoundMappingDB, 
                          g_FoundKeyMapDB, g_FoundKeyboardDB);
    UniUtilReleaseCode(launchFlags);
    MemPtrFree(g_UniBucket);
  }
}

static UTF16 *UTF8toUTF16(const Char *str, UInt16 len)
{
  const UTF8 *utf8, *start8, *end8;
  UTF16 *utf16, *start16, *end16;
  ConversionResult cr;
  
  utf8 = (const UTF8 *)str;
  len++;                        // Include terminating nul.
  utf16 = (UTF16 *)MemPtrNew(len * 2);
  if (NULL == utf16) 
    return NULL;

  start8 = utf8;
  end8 = start8 + len;
  start16 = utf16;
  end16 = start16 + len;
  cr = ConvertUTF8toUTF16(&start8, end8, &start16, end16, lenientConversion);

  return utf16;
}

static void UnicodeFindFontToMatchHeight(UniBucketPtr uniBucketPtr, UTF16 *utf16,
                                         Int16* height, FontID* font)
{
  FontID oldFont, charFont;
  Int16 fontHeight, charFontHeight;
  UInt16 charset;
  UInt32 uch;
  
  oldFont = FntSetFont(*font);
  fontHeight = FntCharHeight();
  
  if (fontHeight != *height) {
    while (true) {
      uch = *utf16++;
      if (0 == uch) break;
      if (UniStrUniChar16PairIsExtendedChar(uch, *utf16))
        UniStrUniChar16PairToUniChar32(uch, *utf16++, &uch);
      charset = (UInt16)(uch / 0x100);
      charFont = UniFontGetFont(uniBucketPtr, charset);
      FntSetFont(charFont);
      charFontHeight = FntCharHeight();
      if (*height == charFontHeight) {
        *font = charFont;
        break;
      }
      if ((charFontHeight < *height) &&
          (charFontHeight > fontHeight)) {
        // Closest so far.
        *font = charFont;
        fontHeight = charFontHeight;
      }
      while ((0 != *utf16) &&
             (charset == (*utf16 / 0x100)))
        utf16++;
    }
    if (0 == uch)
      FntSetFont(*font);        // May be left-over from middle.
  }

  *height = FntLineHeight();    // Now we can include leading.
  FntSetFont(oldFont);
}

void UnicodeSizeSingleLine(const Char *str, UInt16 len, 
                           Int16 *width, Int16 *height, 
                           FontID *font)
{
  UTF16 *utf16;
  RectangleType bounds;
  UInt16 lw, lh;

  utf16 = UTF8toUTF16(str, len);
  if (NULL == utf16) {
    *width = 0;
    *height = g_UniBucket->lineHeight;
    return;
  }
  bounds.topLeft.x = bounds.topLeft.y = 0;
  bounds.extent.x = *width;
  bounds.extent.y = *height;
  UniStrUniCharPrintLine(g_UniBucket, 16, utf16,
                         bounds, UNI_TEXT_DIR_LR, UNI_VERTICAL_ALIGN_TOP,
                         NULL, NULL, &lw, &lh, NULL, false, true);
  *width = lw;
  *height = lh;
  UnicodeFindFontToMatchHeight(g_UniBucket, utf16, height, font);
  MemPtrFree(utf16);
}

void UnicodeDrawSingleLine(const Char *str, UInt16 len, 
                           Int16 x, Int16 y, 
                           Int16 *width, Int16 *height)
{
  UTF16 *utf16;
  RectangleType bounds;
  UInt16 lw, lh;

  utf16 = UTF8toUTF16(str, len);
  if (NULL == utf16) return;
  bounds.topLeft.x = x;
  bounds.topLeft.y = y;
  bounds.extent.x = *width;
  bounds.extent.y = *height;
  UniStrUniCharPrintLine(g_UniBucket, 16, utf16, 
                         bounds, UNI_TEXT_DIR_LR, UNI_VERTICAL_ALIGN_TOP,
                         NULL, NULL, &lw, &lh, NULL, true, true);
  MemPtrFree(utf16);

  *width = lw;
  *height = lh;
}

Boolean UnicodeDrawField(const Char *str, UInt16 *offset, UInt16 len, UInt16 *ndrawn,
                         Int16 *y, RectangleType *bounds)
{
  UTF16 *utf16, *up;
  RectangleType dbounds;
  UInt16 lw, lh;
  Int32 nchars;
  Boolean exhausted;

  utf16 = UTF8toUTF16(str, len);
  if (NULL == utf16) return false;

  dbounds = *bounds;
  dbounds.extent.y -= (*y - dbounds.topLeft.y);
  dbounds.topLeft.y = *y;

  *ndrawn = 0;

  up = utf16 + *offset;
  while (true) {
    nchars = UniStrUniCharPrintLine(g_UniBucket, 16, up, 
                                    dbounds, UNI_TEXT_DIR_LR, UNI_VERTICAL_ALIGN_TOP,
                                    NULL, NULL, &lw, &lh, NULL, true, false);
    dbounds.topLeft.y += lh;
    dbounds.extent.y -= lh;
    if (nchars <= 0) {
      exhausted = (nchars == -2);
      break;
    }
    *ndrawn += nchars;
    *offset = (up - utf16);     // Start, not end, of last piece drawn.
    up += nchars;
  }
  MemPtrFree(utf16);

  *y = dbounds.topLeft.y;
  return exhausted;
}

#endif
