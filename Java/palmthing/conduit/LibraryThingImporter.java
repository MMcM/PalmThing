/*! -*- Mode: C -*-
* Module: LibraryThingImporter.java
* Version: $Header$
*/

package palmthing.conduit;

import palmthing.*;

import palm.conduit.*;

import java.util.*;
import java.io.*;

/** Import records from LibraryThing's tab-delimited export file.<p>
 * For now, read from a file, although with the right cookies direct
 * download should be possible.
 */
public class LibraryThingImporter {

  public static String g_bookField = "book id";
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

  private int m_bookFieldCol = -1;
  private int[] m_stringFieldCols = new int[g_stringFields.length];

  private Vector m_records;

  public Vector getRecords() {
    return m_records;
  }

  public void importFile(String file) throws PalmThingException, IOException {
    FileInputStream fstr = null;
    try {
      fstr = new FileInputStream(file);
      importStream(fstr);
    }
    finally {
      if (fstr != null)
        fstr.close();
    }
  }

  public void importStream(InputStream istr) throws PalmThingException, IOException {
    BufferedReader in = 
      new BufferedReader(new InputStreamReader(istr, "UTF-16"));
    
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
        if (g_bookField.equals(col))
          m_bookFieldCol = i;
        else {
          for (int j = 0; j < g_stringFields.length; j++) {
            if (g_stringFields[j].equals(col)) {
              m_stringFieldCols[j] = i;
              break;
            }
          }
        }
      }

      if (m_bookFieldCol < 0)
        throw new PalmThingException("Export is missing required book id column.");
    }

    m_records = new Vector();

    while (true) {
      String line = in.readLine();
      if (line == null) break;
      String[] cols = splitLine(line);

      BookRecord book = new BookRecord();
      book.setBookID(Integer.parseInt(cols[m_bookFieldCol]));
      for (int i = 0; i < m_stringFieldCols.length; i++) {
        int j = m_stringFieldCols[i];
        if (j < 0) continue;
        String col = cols[j];
        if (col == null) continue;
        switch (i) {
        case BookRecord.FIELD_ISBN:
          col = trimISBN(col);
          break;
        }
        book.setStringField(i, col);
      }

      m_records.add(book);
    }
  }

  protected String[] splitLine(String line) {
    String[] cols = stringSplit(line, '\t');
    for (int i = 0; i < cols.length; i++) {
      String col = cols[i];
      if (col.length() == 0)
        cols[i] = null;
      else if (col.indexOf("[return]") >= 0)
        cols[i] = stringReplaceAll(col, "[return]", "\n");
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
      index = nindex + key.length();
    }
    return result.toString();
  }

  protected String trimISBN(String isbn) {
    if (isbn == null)
      return isbn;
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

  public static void main(String[] args) throws Exception {
    LibraryThingImporter importer = new LibraryThingImporter();
    importer.importFile(args[0]);

    Vector books = importer.getRecords();

    DataOutputStream ostr = new DataOutputStream(new FileOutputStream(args[1]));
    Enumeration benum = books.elements();
    while (benum.hasMoreElements()) {
      BookRecord book = (BookRecord)benum.nextElement();
      book.writeData(ostr);
    }
    ostr.close();

    DataInputStream istr = new DataInputStream(new FileInputStream(args[1]));
    while (true) {
      try {
        BookRecord book = new BookRecord();
        book.readData(istr);
        if (false) System.out.println(book.getISBN());
        System.out.println(book.toFormattedString());
      }
      catch (EOFException ex) {
        break;
      }
    }
    istr.close();
  }
}
