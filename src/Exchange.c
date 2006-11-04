/*! -*- Mode: C -*-
* Module: Exchange.c
* Version: $Header$
*/

#include <PalmOS.h>

#include "AppResources.h"
#include "AppGlobals.h"

/*** Local constants ***/

#define MAX_URL 256
#define BASE_URL "http://www.librarything.com"

/*** Local storage ***/

static Boolean g_WebChecked = false, g_WebEnabled = false;

/*** Global routines ***/

Boolean WebEnabled()
{
  // TODO: Don't bother with older sysFileCClipper technique for now.
  UInt32 numapps;
  Err error;

  if (!SYS_ROM_4_0) return false;

  if (!g_WebChecked) {
    error = ExgGetRegisteredApplications(NULL, &numapps, NULL, NULL, 
                                         exgRegSchemeID, "http");
    g_WebEnabled = ((errNone == error) && (numapps > 0));
    g_WebChecked = true;
  }

  return g_WebEnabled;
}

Err WebGotoCurrentBook()
{
  BookRecord record;
  MemHandle recordH;
  ExgSocketType socket;
  Char *url;
  Err error;

  error = BookDatabaseGetRecord(g_CurrentRecord, &recordH, &record);
  if (error) return error;

  url = MemPtrNew(MAX_URL);
  if (0 != record.bookID) {
    StrPrintF(url, "%s/%s?%s=%ld", BASE_URL, "work.php", "book", record.bookID);
  }
  else if (NULL != record.fields[FIELD_ISBN]) {
    StrPrintF(url, "%s/%s/%s", BASE_URL, "isbn", record.fields[FIELD_ISBN]);
  }
  else {
    StrCopy(url, BASE_URL);
  }
  MemSet(&socket, sizeof(socket), 0);
  socket.name = url;
  error = ExgRequest(&socket);
  MemPtrFree(url);
  MemHandleUnlock(recordH);
  return error;
}
