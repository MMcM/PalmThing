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
      // Get the pathnames to work with.
      File local = new File(props.localName);
      if (!local.isAbsolute())
        local = new File(props.pathName, props.localName);
      String name = local.getName();
      if (name.endsWith(".xls"))
        name = name.substring(0, name.length() - 4);
      File backup = new File(props.pathName, name + ".bak");

      LibraryThingImporter importer = new LibraryThingImporter();

      int syncType = props.syncType;
      Vector localBooks, backupBooks, remoteBooks;

      if (syncType != SyncProperties.SYNC_HH_TO_PC) {
        localBooks = importer.importFile(local.getPath());
        Log.out("Read " + localBooks.size() + " books from " + local);
      }
      else {
        localBooks = null;
      }
      
      if ((syncType != SyncProperties.SYNC_PC_TO_HH) &&
          backup.exists()) {
        backupBooks = importer.importFile(backup.getPath());
        Log.out("Read " + backupBooks.size() + " books from " + backup);
      }
      else {
        backupBooks = null;
      }
      int db = SyncManager.openDB(props.remoteNames[0], 0, 
                                  SyncManager.OPEN_READ | SyncManager.OPEN_WRITE | 
                                  SyncManager.OPEN_EXCLUSIVE);

      if (syncType == SyncProperties.SYNC_HH_TO_PC) {
        remoteBooks = readAllRemote(db);
        Log.out("Read " + remoteBooks.size() + " books from " + props.userName);
      }
      else {
        if (syncType == SyncProperties.SYNC_PC_TO_HH) {
          SyncManager.purgeAllRecs(db);
          remoteBooks = null;
        }
        else {
          if (backupBooks == null) {
            remoteBooks = readAllRemote(db);
          }
          else {
            remoteBooks = readModifiedRemote(db);
          }
          Log.out("Read " + remoteBooks.size() + " books from " + props.userName);
          SyncManager.purgeDeletedRecs(db);
        }

        int sortKey = BookRecordComparator.KEY_TITLE_AUTHOR;
        try {
          byte[] appInfo = SyncManager.readDBAppInfoBlock(db, props.remoteNames[0]);
          sortKey = appInfo[276]; // Must match remote.
        }
        catch (SyncException ex) {
        }

        Vector changedBooks = merge(localBooks, backupBooks, remoteBooks,
                                    new BookRecordComparator(sortKey));
        if (remoteBooks == null)
          remoteBooks = changedBooks;

        Enumeration benum = changedBooks.elements();
        while (benum.hasMoreElements()) {
          BookRecord book = (BookRecord)benum.nextElement();
          if (book.isDeleted())
            SyncManager.deleteRecord(db, book);
          else
            SyncManager.writeRec(db, book);
        }
        Log.out("Wrote " + changedBooks.size() + " books to " + props.userName);
      }

      SyncManager.closeDB(db);

      importer.exportFile(remoteBooks, backup.getPath());
      Log.out("Wrote " + remoteBooks.size() + " books to " + backup);

      Log.endSync();
    } 
    catch (Exception ex) {
      ex.printStackTrace();
      Log.abortSync();
    }
  }

  protected Vector readAllRemote(int db) throws IOException {
    int nrecs = SyncManager.getDBRecordCount(db);
    Vector result = new Vector(nrecs);
    for (int i = 0; i < nrecs; i++) {
      BookRecord book = new BookRecord();
      book.setIndex(i);
      SyncManager.readRecordByIndex(db, book);
      result.addElement(book);
    }
    return result;
  }

  protected Vector readModifiedRemote(int db) throws IOException {
    Vector result = new Vector();
    while (true) {
      BookRecord book = new BookRecord();
      try {
        SyncManager.readNextModifiedRec(db, book);
      }
      catch (SyncException ex) {
        break;
      }
      result.addElement(book);
    }
    return result;
  }

  protected Vector merge(Vector localBooks, Vector backupBooks, Vector remoteBooks,
                         Comparator comp) 
      throws IOException {
    Vector changedBooks;
    if (remoteBooks == null) {
      remoteBooks = changedBooks = localBooks;
    }
    else {
      changedBooks = new Vector();
      Hashtable remoteByBookID = hashByBookID(remoteBooks);
      {
        Hashtable backupByBookID = hashByBookID(backupBooks);
        Enumeration benum = localBooks.elements();
        while (benum.hasMoreElements()) {
          BookRecord book = (BookRecord)benum.nextElement();
          BookRecord backup = (BookRecord)
            backupByBookID.get(new Integer(book.getBookID()));
          if ((backup == null) || !book.equals(backup)) {
            // Local added / changed.
            BookRecord remote = (BookRecord)
              remoteByBookID.get(new Integer(book.getBookID()));
            if (remote != null)
              remoteBooks.remove(remote);
            remoteBooks.addElement(book);
            changedBooks.addElement(book);
          }
        }
      }
      {
        Hashtable localByBookID = hashByBookID(localBooks);
        Enumeration benum = ((backupBooks == null) ? remoteBooks : backupBooks)
          .elements();
        while (benum.hasMoreElements()) {
          BookRecord book = (BookRecord)benum.nextElement();
          if (!localByBookID.containsKey(new Integer(book.getBookID()))) {
            // Local removed.
            book.setIsDeleted(true);
            BookRecord remote = (BookRecord)
              remoteByBookID.get(new Integer(book.getBookID()));
            if (remote != null)
              remoteBooks.remove(remote);
            changedBooks.addElement(book);
          }
        }
      }
    }
    
    Collections.sort(remoteBooks, comp);
    for (int i = 0; i < remoteBooks.size(); i++) {
      BookRecord book = (BookRecord)remoteBooks.elementAt(i);
      if (book.getId() == 0)
        book.setIndex(i);
      else if (book.getIndex() != i) {
        book.setIndex(i);
        if ((changedBooks != remoteBooks) && !changedBooks.contains(book))
          changedBooks.addElement(book);
      }
    }

    if (changedBooks != remoteBooks) {
      Collections.sort(changedBooks, 
                       new Comparator() {
                         public int compare(Object o1, Object o2) {
                           BookRecord book1 = (BookRecord)o1;
                           BookRecord book2 = (BookRecord)o2;
                           return (book1.getIndex() - book2.getIndex());
                         }
                       });
    }

    return changedBooks;
  }

  protected Hashtable hashByBookID(Vector books) {
    Hashtable result = new Hashtable();
    if (books != null) {
      Enumeration benum = books.elements();
      while (benum.hasMoreElements()) {
        BookRecord book = (BookRecord)benum.nextElement();
        if (book.getBookID() != 0) {
          result.put(new Integer(book.getBookID()), book);
        }
      }
    }
    return result;
  }

  public int configure(ConfigureConduitInfo info) {
    return 0;
  }

  public String name() {
    return NAME;
  }
}
