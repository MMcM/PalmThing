/*! -*- Mode: Java -*-
* Module: PalmThingConduit.java
* Version: $Header$
*/

package palmthing.conduit;

import palm.conduit.*;

import java.util.*;
import java.io.*;

/** PalmThing conduit.

**/
public class PalmThingConduit implements Conduit {
  static final String NAME = "PalmThing Conduit";

  public void open(SyncProperties props) {
    Log.startSync();

    try {
      File local = new File(props.pathName, props.localName);
      LibraryThingImporter importer = new LibraryThingImporter();
      Vector books = importer.importFile(local.getPath());
      Log.out("Read " + books.size() + " books from " + local);
      
      int db = SyncManager.openDB(props.remoteNames[0], 0, 
                                  SyncManager.OPEN_READ | SyncManager.OPEN_WRITE | 
                                  SyncManager.OPEN_EXCLUSIVE);
      SyncManager.purgeAllRecs(db);
      
      Enumeration benum = books.elements();
      while (benum.hasMoreElements()) {
        BookRecord book = (BookRecord)benum.nextElement();
        SyncManager.writeRec(db, book);
      }
      
      SyncManager.closeDB(db);

      Log.endSync();
    } 
    catch (Exception ex) {
      ex.printStackTrace();
      Log.abortSync();
    }
  }

  public int configure(ConfigureConduitInfo info) {
    return 0;
  }

  public String name() {
    return NAME;
  }
}
