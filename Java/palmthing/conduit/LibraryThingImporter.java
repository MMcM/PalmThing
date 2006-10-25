/*! -*- Mode: Java -*-
* Module: LibraryThingImporter.java
* Version: $Header$
*/
package palmthing.conduit;

import palmthing.*;

import palm.conduit.*;

import java.util.*;
import java.io.*;
import java.lang.reflect.*;

/** Import records from LibraryThing's tab-delimited export file.<p>
 * For now, read from a file, although with the right cookies direct
 * download should be possible.
 */
public class LibraryThingImporter {

  // Libraries that use MARC-8 / ANSEL will tend to have decomposed
  // accents, which can end up in the library.  Converting to
  // ISO-8859-1 works better with composed.  Every version of Java
  // knows how to do this internally, but the exact details are
  // different for each major release.
  interface Normalizer {
    public String normalize(String str);
  }

  private Normalizer m_normalizer;

  public LibraryThingImporter() {
    Class clazz = null;
    try {
      clazz = Class.forName("java.text.Normalizer");
    }
    catch (ClassNotFoundException ex) {
    }
    if (clazz != null) {
      Method meth = null;
      // 1.3
      try {
        meth = clazz.getMethod("compose", new Class[] { String.class });
      }
      catch (NoSuchMethodException ex) {
      }
      if (meth != null) {
        meth.setAccessible(true);
        final Method f_meth = meth;
        m_normalizer = new Normalizer() {
            public String normalize(String str) {
              try {
                return (String)f_meth.invoke(null, new Object[] { str });
              }
              catch (IllegalAccessException ex) {
                return str;
              }
              catch (InvocationTargetException ex) {
                return str;
              }
            }
          };
        return;
      }
      // 1.6
      Object form = null;
      try {
        Class nclass = Class.forName("java.text.Normalizer.Form");
        form = nclass.getMethod("valueOf", new Class[] { String.class })
          .invoke(null, new Object[] { "NFC" });
        meth = clazz.getMethod("normalize", new Class[] {
                                 Class.forName("java.lang.CharSequence"),
                                 nclass
                               });
      }
      catch (ClassNotFoundException ex) {
      }
      catch (NoSuchMethodException ex) {
      }
      catch (IllegalAccessException ex) {
      }
      catch (InvocationTargetException ex) {
      }
      if (meth != null) {
        final Method f_meth = meth;
        final Object f_form = form;
        m_normalizer = new Normalizer() {
            public String normalize(String str) {
              try {
                return (String)f_meth.invoke(null, new Object[] { str, f_form });
              }
              catch (IllegalAccessException ex) {
                return str;
              }
              catch (InvocationTargetException ex) {
                return str;
              }
            }
          };
        return;
      }
    }
    // 1.4
    try {
      clazz = Class.forName("sun.text.Normalizer");
    }
    catch (ClassNotFoundException ex) {
    }
    if (clazz != null) {
      Method meth = null;
      try {
        meth = clazz.getMethod("compose", new Class[] { 
                                 String.class, Boolean.TYPE, Integer.TYPE 
                               });
      }
      catch (NoSuchMethodException ex) {
      }
      if (meth != null) {
        final Method f_meth = meth;
        m_normalizer = new Normalizer() {
            public String normalize(String str) {
              try {
                return (String)f_meth.invoke(null, new Object[] { 
                                               str, Boolean.FALSE, Integer.valueOf(0) 
                                             });
              }
              catch (IllegalAccessException ex) {
                return str;
              }
              catch (InvocationTargetException ex) {
                return str;
              }
            }
          };
        return;
      }
    }
  }

  public static String g_recordIDField = "record id";
  public static String g_bookIDField = "book id";
  // Order must agree with BookRecord.
  public static String[] g_stringFields = {
    "title",
    "author (last, first)",
    "date",
    "ISBN",
    "publication",
    "tags",
    "summary",
    "comments",
  };

  public Vector importFile(String file) throws PalmThingException, IOException {
    FileInputStream fstr = null;
    try {
      fstr = new FileInputStream(file);
      return importStream(fstr);
    }
    finally {
      if (fstr != null)
        fstr.close();
    }
  }

  public Vector importStream(InputStream istr) throws PalmThingException, IOException {
    BufferedReader in = 
      new BufferedReader(new InputStreamReader(istr, "UTF-16"));
    
    int recordIDFieldCol = -1, bookIDFieldCol = -1;
    int[] stringFieldCols = new int[g_stringFields.length];
    Arrays.fill(stringFieldCols, -1);

    {
      String line = in.readLine();
      if (line == null) 
        throw new PalmThingException("Export is missing header line.");

      String[] cols = splitLine(line);

      // I think that there are two BOMs in the file sometimes.
      while (cols[0].charAt(0) == '\uFEFF')
        cols[0] = cols[0].substring(1);

      for (int i = 0; i < cols.length; i++) {
        String col = cols[i];
        if (g_recordIDField.equals(col))
          recordIDFieldCol = i;
        else if (g_bookIDField.equals(col))
          bookIDFieldCol = i;
        else {
          for (int j = 0; j < g_stringFields.length; j++) {
            if (g_stringFields[j].equals(col)) {
              stringFieldCols[j] = i;
              break;
            }
          }
        }
      }

      if (bookIDFieldCol < 0)
        throw new PalmThingException("Export is missing required book id column.");
    }

    Vector records = new Vector();

    while (true) {
      String line = in.readLine();
      if (line == null) break;
      String[] cols = splitLine(line);

      BookRecord book = new BookRecord();
      if (recordIDFieldCol >= 0) {
        String col = cols[recordIDFieldCol];
        if (col != null)
          book.setId(Integer.parseInt(col, 16));
      }
      if (bookIDFieldCol >= 0) {
        String col = cols[bookIDFieldCol];
        if (col != null)
          book.setBookID(Integer.parseInt(col));
      }
      for (int i = 0; i < stringFieldCols.length; i++) {
        int fieldCol = stringFieldCols[i];
        if (fieldCol < 0) continue;
        if (fieldCol >= cols.length)
          System.err.println(line);
        String col = cols[fieldCol];
        if (col == null) continue;
        if (m_normalizer != null)
          col = m_normalizer.normalize(col);
        col = convertFromExternal(col, i);
        book.setStringField(i, col);
      }

      records.add(book);
    }

    return records;
  }

  public void exportFile(Vector books, String file)
      throws PalmThingException, IOException {
    FileOutputStream fstr = null;
    try {
      fstr = new FileOutputStream(file);
      exportStream(books, fstr);
    }
    finally {
      if (fstr != null)
        fstr.close();
    }
  }

  public void exportStream(Vector books, OutputStream ostr)
      throws PalmThingException, IOException {
    BufferedWriter out = 
      new BufferedWriter(new OutputStreamWriter(ostr, "UTF-16"));
    
    {
      out.write(g_recordIDField);
      out.write('\t');
      out.write(g_bookIDField);
      for (int i = 0; i < g_stringFields.length; i++) {
        out.write('\t');
        out.write(g_stringFields[i]);
      }
      out.newLine();
    }

    Enumeration benum = books.elements();
    while (benum.hasMoreElements()) {
      BookRecord book = (BookRecord)benum.nextElement();
      if (book.getId() != BookRecord.RECORD_ID_NONE)
        out.write(Integer.toString(book.getId(), 16));
      out.write('\t');
      if (book.getBookID() != BookRecord.BOOK_ID_NONE)
        out.write(Integer.toString(book.getBookID()));
      for (int i = 0; i < g_stringFields.length; i++) {
        out.write('\t');
        String col = book.getStringField(i);
        if (col != null) {
          col = convertToExternal(col, i);
          out.write(col);
        }
      }
      out.newLine();
    }
    
    out.flush();
  }

  // Converts external (PC) format to internal (HH) format.
  protected String convertFromExternal(String col, int index) {
    if (col.indexOf("[return]") >= 0)
      col = stringReplaceAll(col, "[return]", "\n");
    switch (index) {
    case BookRecord.FIELD_ISBN:
      col = trimISBN(col);
      break;
    case BookRecord.FIELD_TAGS:
      col = stringReplaceAll(col, ",", ", ");
      break;
    }
    return col;
  }

  protected String trimISBN(String isbn) {
    int start = 0;
    if (isbn.charAt(start) == '[')
      start++;
    int end = isbn.length();
    if (isbn.charAt(end-1) == ']')
      end--;
    if (start == end)
      return null;
    return isbn.substring(start, end);
  }

  // Converts internal (HH) format to external (PC) format.
  protected String convertToExternal(String col, int i) {
    switch (i) {
    case BookRecord.FIELD_ISBN:
      col = "[" + col + "]";
      break;
    case BookRecord.FIELD_TAGS:
      col = stringReplaceAll(col, ", ", ",");
      break;
    }
    if (col.indexOf("\n") >= 0)
      col = stringReplaceAll(col, "\n", "[return]");
    return col;
  }

  protected String[] splitLine(String line) {
    String[] cols = stringSplit(line, '\t');
    for (int i = 0; i < cols.length; i++) {
      String col = cols[i];
      if (col.length() == 0)
        cols[i] = null;
    }
    return cols;
  }

  // This is all a bit harder to do in this ancient version of J2SE;
  // can't even use StringTokenizer.
  protected String[] stringSplit(String str, char delim) {
    Vector vec = new Vector();
    int index = 0;
    while (true) {
      int nindex = str.indexOf(delim, index);
      int toindex = (nindex < 0) ? str.length() : nindex;
      vec.addElement(str.substring(index, toindex));
      if (nindex < 0) break;
      index = nindex + 1;
    }
    String[] result = new String[vec.size()];
    vec.copyInto(result);
    return result;
  }

  protected String stringReplaceAll(String str, String key, String repl) {
    StringBuffer result = new StringBuffer();
    int index = 0;
    while (true) {
      int nindex = str.indexOf(key, index);
      int toindex = (nindex < 0) ? str.length() : nindex;
      if (toindex > index)
        result.append(str.substring(index, toindex));
      if (nindex < 0) break;
      result.append(repl);
      index = nindex + key.length();
    }
    return result.toString();
  }

  public static void main(String[] args) throws Exception {
    LibraryThingImporter importer = new LibraryThingImporter();

    Vector books = importer.importFile(args[0]);

    Collections.sort(books, 
                     new BookRecordComparator(BookRecordComparator.KEY_TITLE_AUTHOR));

    DataOutputStream ostr = new DataOutputStream(new FileOutputStream(args[1]));
    Enumeration benum = books.elements();
    while (benum.hasMoreElements()) {
      BookRecord book = (BookRecord)benum.nextElement();
      book.writeData(ostr);
    }
    ostr.close();

    books.clear();

    DataInputStream istr = new DataInputStream(new FileInputStream(args[1]));
    while (true) {
      try {
        BookRecord book = new BookRecord();
        book.readData(istr);
        books.addElement(book);
      }
      catch (EOFException ex) {
        break;
      }
    }
    istr.close();

    importer.exportFile(books, args[2]);

    books = importer.importFile(args[2]);

    benum = books.elements();
    while (benum.hasMoreElements()) {
      BookRecord book = (BookRecord)benum.nextElement();
      if (false) System.out.println(book.getISBN());
      System.out.println(book.toFormattedString());
    }
  }
}
