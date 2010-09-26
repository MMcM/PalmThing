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

      // TODO: Read a .properties file from props path and initialize
      // field prefs, etc.
      LibraryThingImporter importer = new LibraryThingImporter();

      int syncType = props.syncType;
      List localBooks, backupBooks, remoteBooks;

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
        byte[] appInfo = null;
        Vector remoteCategories = null;
        if (remoteBooks != null) {
          try {
            appInfo = SyncManager.readDBAppInfoBlock(db, props.remoteNames[0]);
            // Must match remote.
            sortKey = appInfo[BookCategories.SIZE];
          }
          catch (SyncException ex) {
          }
          if (appInfo != null) {
            remoteCategories = Category.parseCategories(appInfo);
            BookCategories.setCategories(remoteBooks, remoteCategories);
          }
        }
        if (appInfo == null) {
          appInfo = new byte[BookCategories.SIZE + 1];
          appInfo[BookCategories.SIZE] = (byte)sortKey;
        }

        List changedBooks = merge(localBooks, backupBooks, remoteBooks,
                                  new BookRecordComparator(sortKey));
        if (remoteBooks == null)
          remoteBooks = changedBooks;

        Vector categories =  BookCategories.getCategories(remoteBooks);
        if (remoteCategories != null) {
          categories = BookCategories.mergeCategories(remoteCategories, categories);
        }
        byte[] catBytes = Category.toBytes(categories);
        System.arraycopy(catBytes, 0, appInfo, 0, catBytes.length);
        SyncManager.writeDBAppInfoBlock(db, props.remoteNames[0], appInfo);
        BookCategories.setCategoryIndices(changedBooks, categories);

        Iterator iter = changedBooks.iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
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

  protected List readAllRemote(int db) throws IOException {
    int nrecs = SyncManager.getDBRecordCount(db);
    List result = new ArrayList(nrecs);
    for (int i = 0; i < nrecs; i++) {
      BookRecord book = new BookRecord();
      book.setIndex(i);
      SyncManager.readRecordByIndex(db, book);
      result.add(book);
    }
    return result;
  }

  protected List readModifiedRemote(int db) throws IOException {
    List result = new ArrayList();
    while (true) {
      BookRecord book = new BookRecord();
      try {
        SyncManager.readNextModifiedRec(db, book);
      }
      catch (SyncException ex) {
        break;
      }
      result.add(book);
    }
    return result;
  }

  protected List merge(List localBooks, List backupBooks, List remoteBooks,
                       Comparator comp) 
      throws IOException {
    List changedBooks;
    if (remoteBooks == null) {
      // Everything is copied over and that is all remotely.
      changedBooks = remoteBooks = localBooks;
    }
    else {
      changedBooks = new ArrayList();
      Map remoteByBookID = hashByBookID(remoteBooks);
      {
        Map backupByBookID = hashByBookID(backupBooks);
        Iterator iter = localBooks.iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
          BookRecord backup = (BookRecord)
            backupByBookID.get(new Integer(book.getBookID()));
          if ((backup == null) || !book.equals(backup)) {
            // Local added / changed.
            BookRecord remote = (BookRecord)
              remoteByBookID.get(new Integer(book.getBookID()));
            if (remote != null)
              remoteBooks.remove(remote);
            changedBooks.add(book);
          }
          else {
            book.setId(backup.getId());
          }
          remoteBooks.add(book);
        }
      }
      {
        Map localByBookID = hashByBookID(localBooks);
        Iterator iter = ((backupBooks == null) ? remoteBooks : backupBooks).iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
          if (!localByBookID.containsKey(new Integer(book.getBookID()))) {
            // Local removed.
            book.setIsDeleted(true);
            BookRecord remote = (BookRecord)
              remoteByBookID.get(new Integer(book.getBookID()));
            if (remote != null)
              remoteBooks.remove(remote);
            changedBooks.add(book);
          }
        }
      }
    }
    
    Collections.sort(remoteBooks, comp);
    for (int i = 0; i < remoteBooks.size(); i++) {
      BookRecord book = (BookRecord)remoteBooks.get(i);
      // TODO: How does the index field affect an existing record?
      // Does not seem to get inserted at requested position, but
      // always at the end, which would necessitate a HH-side sort
      // after sync.
      if (book.getId() == 0)
        book.setIndex(i);
      /**
      else if (book.getIndex() != i) {
        book.setIndex(i);
        if ((changedBooks != remoteBooks) && !changedBooks.contains(book))
          changedBooks.add(book);
      }
      **/
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

  protected Map hashByBookID(Collection books) {
    Map result = new HashMap();
    if (books != null) {
      Iterator iter = books.iterator();
      while (iter.hasNext()) {
        BookRecord book = (BookRecord)iter.next();
        if (book.getBookID() != 0) {
          result.put(new Integer(book.getBookID()), book);
        }
      }
    }
    return result;
  }

  public int configure(ConfigureConduitInfo info) {
    ConduitConfigure config = new ConduitConfigure(info, NAME);
    config.createDialog();

    if (config.dataChanged) {
      info.syncNew = config.saveState;
      if (config.setDefault) {
        info.syncPermanent = config.saveState;
        info.syncPref = ConfigureConduitInfo.PREF_PERMANENT;
      }
    }
        
    return 0;
  }

  public String name() {
    return NAME;
  }
}
