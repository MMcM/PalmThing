/*! -*- Mode: Java -*-
* Module: LibraryThingImporter.java
* Version: $Header$
*/
package palmthing.conduit;

import palmthing.*;

import palm.conduit.*;

import java.util.*;
import java.io.*;
import java.net.*;

/** Import records from LibraryThing's tab-delimited export file.<p>
 * For now, read from a file, although with the right cookies direct
 * download should be possible.
 */
public class LibraryThingImporter {
  // Require BOM on input; BE with BOM on output.
  private String m_encoding = "UTF-16";

  /** Get the character encoding to use when reading / writing. */
  public String getEncoding() {
    return m_encoding;
  }

  public void setEncoding(String encoding) {
    m_encoding = encoding;
  }

  public static final String g_recordIDField = "record id";
  public static final String g_bookIDField = "book id";
  // Order must agree with BookRecord.
  public static final String[] g_stringFields = {
    "title",
    "author",
    "date",
    "ISBN",
    "publication",
    "tags",
    "summary",
    "comments",
  };

  public static final String g_authorLastFirstField = "author (last, first)";
  public static final String g_authorFirstLastField = "author (first, last)";
  private String m_authorField = g_authorLastFirstField;

  public static final int AUTHOR_LAST_FIRST = 0;
  public static final int AUTHOR_FIRST_LAST = 1;

  /** Get which of the two author fields is tranferred to the HH. */
  public int getAuthorField() {
    if (m_authorField.equals(g_authorLastFirstField))
      return AUTHOR_LAST_FIRST;
    else
      return AUTHOR_FIRST_LAST;
  }

  public void setAuthorField(int authorField) {
    switch (authorField) {
    case AUTHOR_LAST_FIRST:
      m_authorField = g_authorLastFirstField;
      break;
    case AUTHOR_FIRST_LAST:
      m_authorField = g_authorFirstLastField;
      break;
    }
  }

  private boolean m_includeSummaryField = true;

  /** Get whether the summary field (which mostly duplicates the title
   * and author and date fields) is transferred to the HH. */
  public boolean getIncludeSummary() {
    return m_includeSummaryField;
  }

  public void setIncludeSummary(boolean includeSummaryField) {
    m_includeSummaryField = includeSummaryField;
  }

  private boolean m_validate = true;

  /** Get whether there are any mandatory fields in the input file. 
   * Can be set off when inputting non-LT flat files.
   */
  public boolean getValidate() {
    return m_validate;
  }

  public void setValidate(boolean validate) {
    m_validate = validate;
  }

  private String m_addTags = null;

  /** Get additional tags to be added to each record read from the file.
   * Mainly useful when appending several files to add tags related to the whole file.
   */
  public String getAddTags() {
    return m_addTags;
  }

  public void setAddTags(String addTags) {
    m_addTags = addTags;
  }

  /** Import from the given file.
   * @return {@link List} of {@link BookRecord}.
   */
  public List importFile(String file) throws PalmThingException, IOException {
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

  public static String EXPORT_TAB = "http://www.librarything.com/export-tab.php";

  public List download(String user) throws PalmThingException, IOException {
    URL url = new URL(EXPORT_TAB);
    HttpURLConnection conn = (HttpURLConnection)url.openConnection();
    conn.setRequestMethod("GET");
    conn.setRequestProperty("Cookie", "cookie_userid=" + user);
    InputStream istr = conn.getInputStream();
    List result = importStream(istr);
    istr.close();
    return result;
  }

  private UnicodeUtils.Normalizer m_normalizer = UnicodeUtils.getNormalizer();

  /** Import from the given stream. */
  public List importStream(InputStream istr) throws PalmThingException, IOException {
    BufferedReader in = 
      new BufferedReader(new InputStreamReader(istr, m_encoding));
    
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
        if (g_recordIDField.equalsIgnoreCase(col))
          recordIDFieldCol = i;
        else if (g_bookIDField.equalsIgnoreCase(col))
          bookIDFieldCol = i;
        else if (m_authorField.equalsIgnoreCase(col))
          stringFieldCols[BookRecord.FIELD_AUTHOR] = i;
        else {
          for (int j = 0; j < g_stringFields.length; j++) {
            if (g_stringFields[j].equalsIgnoreCase(col)) {
              if ((j == BookRecord.FIELD_SUMMARY) && !m_includeSummaryField)
                break;
              stringFieldCols[j] = i;
              break;
            }
          }
        }
      }

      if (m_validate && (bookIDFieldCol < 0))
        throw new PalmThingException("Export is missing required book id column.");
    }

    List records = new ArrayList();

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
        if (fieldCol < 0) {
          if ((i == BookRecord.FIELD_TAGS) &&
              (m_addTags != null)) {
            setField(book, i, m_addTags);
          }
          continue;
        }
        String col = null;
        if (fieldCol < cols.length)
          col = cols[fieldCol];
        if ((i == BookRecord.FIELD_TAGS) &&
            (m_addTags != null)) {
          if (col == null)
            col = m_addTags;
          else
            col = m_addTags + "," + col;
        }
        if (col == null) continue;
        if (m_normalizer != null)
          col = m_normalizer.normalize(col);
        if (UnicodeUtils.hasHangulSyllables(col))
          // UniLib only does Hangul as an alphabet, not pre-composed.
          col = UnicodeUtils.decomposeHangulSyllables(col);
        setField(book, i, col);
      }

      book.setCategoryIndex(BookCategories.MAIN);

      records.add(book);
    }

    return records;
  }

  /** Export to the given file. */
  public void exportFile(Collection books, String file)
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

  /** Export to the given stream. */
  public void exportStream(Collection books, OutputStream ostr)
      throws PalmThingException, IOException {
    BufferedWriter out = 
      new BufferedWriter(new OutputStreamWriter(ostr, m_encoding));
    
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

    Iterator iter = books.iterator();
    while (iter.hasNext()) {
      BookRecord book = (BookRecord)iter.next();
      if (book.getId() != BookRecord.RECORD_ID_NONE)
        out.write(Integer.toString(book.getId(), 16));
      out.write('\t');
      if (book.getBookID() != BookRecord.BOOK_ID_NONE)
        out.write(Integer.toString(book.getBookID()));
      for (int i = 0; i < g_stringFields.length; i++) {
        out.write('\t');
        String col = getField(book, i);
        if (col != null)
          out.write(col);
      }
      out.newLine();
    }
    
    out.flush();
  }

  // Converts external (PC) format to internal (HH) format.
  protected void setField(BookRecord book, int index, String col) {
    switch (index) {
    case BookRecord.FIELD_ISBN:
      col = trimISBN(col);
      break;
    case BookRecord.FIELD_TAGS:
      col = BookUtils.extractCategoryTag(col, book);
      if (col != null)
        col = stringReplaceAll(col, ",", ", ");
      break;
    default:
      if (col.indexOf("[return]") >= 0)
        col = stringReplaceAll(col, "[return]", "\n");
    }
    book.setStringField(index, col);
  }

  // LT writes ISBNs with brackets to avoid Excel interpreting them as numbers.
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
  protected String getField(BookRecord book, int i) {
    String col = book.getStringField(i);
    if (col == null) {
      return ((i == BookRecord.FIELD_TAGS) ? book.getCategoryTag() : null);
    }
    switch (i) {
    case BookRecord.FIELD_ISBN:
      col = "[" + col + "]";
      break;
    case BookRecord.FIELD_TAGS:
      col = BookUtils.mergeCategoryTag(col, book);
      col = stringReplaceAll(col, ", ", ",");
      break;
    default:
      if (col.indexOf("\n") >= 0)
        col = stringReplaceAll(col, "\n", "[return]");
    }
    return col;
  }

  private char m_fieldDelimiter = '\t';

  public char getFieldDelimiter() {
    return m_fieldDelimiter;
  }

  public void setFieldDelimiter(char fieldDelimiter) {
    m_fieldDelimiter = fieldDelimiter;
  }

  private char m_quoteCharacter = '"';

  // Split the given line up into columns.
  protected String[] splitLine(String line) {
    String[] cols = stringSplit(line, m_fieldDelimiter);
    for (int i = 0; i < cols.length; i++) {
      String col = cols[i];
      if (col.length() == 0)
        cols[i] = null;
      else if ((col.charAt(0) == m_quoteCharacter) &&
               (col.charAt(col.length()-1) == m_quoteCharacter))
        cols[i] = col.substring(1, col.length()-1);
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

  // Must agree with AppGlobals.h and BookDatabase.c.
  public static final String APP_CREATOR = "plTN";
  public static final String DB_TYPE = "DATA";
  public static final String DB_TYPE_UNICODE = "DATU";
  public static final short DB_VER = (short)0;
  public static final String DB_NAME = "PalmThing-Books";

  public PalmDatabaseDumper getPalmDatabaseDumper(List books, 
                                                  int sort, boolean unicode) 
      throws PalmThingException, IOException {
    Vector categories = BookCategories.getCategories(books);
    BookCategories.setCategoryIndices(books, categories);
    PalmDatabaseDumper result = 
      new PalmDatabaseDumper(DB_NAME, DB_VER, APP_CREATOR, 
                             (unicode) ? DB_TYPE_UNICODE : DB_TYPE);
    byte[] catBytes = Category.toBytes(categories);
    byte[] appInfo = new byte[BookCategories.SIZE + 2];
    System.arraycopy(catBytes, 0, appInfo, 0, catBytes.length);
    appInfo[BookCategories.SIZE] = (byte)sort;
    result.setAppInfoBlock(appInfo);
    return result;
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0) {
      System.out.println("usage: LibraryThingImporter" +
                         " [-encoding enc] [-delimiter delim]" +
                         " [-author {last_first,first_last}] [-summary bool]" + 
                         " [-validate bool] [-unicode bool]" +
                         " [-read file] [-append file] [-write file]" +
                         " [-sort field] [-dump file]" + 
                         " [-dump-raw file] [-load-raw file]" +
                         " [-read-xml file] [-write-xml file] [-write-html file xsl]" +
                         " [-print]");
      return;
    }
    LibraryThingImporter importer = new LibraryThingImporter();
    List books = null;
    int sort = 0;
    boolean unicode = false;
    int i = 0;
    while (i < args.length) {
      String arg = args[i++];
      if (!arg.startsWith("-"))
        throw new PalmThingException("Not a switch: " + arg);
      if (arg.equals("-encoding")) {
        importer.setEncoding(args[i++]);
      }
      else if (arg.equals("-delimiter")) {
        importer.setFieldDelimiter(args[i++].charAt(0));
      }
      else if (arg.equals("-author")) {
        String author = args[i++];
        if ("last_first".equals(author))
          importer.setAuthorField(AUTHOR_LAST_FIRST);
        else if ("first_last".equals(author))
          importer.setAuthorField(AUTHOR_FIRST_LAST);
        else
          throw new PalmThingException("Unknown author mode: " + author);
      }
      else if (arg.equals("-summary")) {
        importer.setIncludeSummary(Boolean.valueOf(args[i++]).booleanValue());
      }
      else if (arg.equals("-validate")) {
        importer.setValidate(Boolean.valueOf(args[i++]).booleanValue());
      }
      else if (arg.equals("-add-tags")) {
        String tags = args[i++];
        if (tags.length() == 0)
          tags = null;
        importer.setAddTags(tags);
      }
      else if (arg.equals("-unicode")) {
        unicode = Boolean.valueOf(args[i++]).booleanValue();
        BookRecord.setUnicode(unicode);
      }
      else if (arg.equals("-read")) {
        String file = args[i++];
        books = importer.importFile(file);
        System.out.println(books.size() + " books read from " + file);
      }
      else if (arg.equals("-append")) {
        String file = args[i++];
        List nbooks = importer.importFile(file);
        books.addAll(nbooks);
        System.out.println(nbooks.size() + " books added from " + file);
      }
      else if (arg.equals("-write")) {
        String file = args[i++];
        importer.exportFile(books, file);
        System.out.println(books.size() + " books written to " + file);
      }
      else if (arg.equals("-sort")) {
        sort = Integer.parseInt(args[i++]);
        Collections.sort(books, new BookRecordComparator(sort));
      }
      else if (arg.equals("-dump")) {
        String file = args[i++];
        PalmDatabaseDumper pdb = importer.getPalmDatabaseDumper(books, sort, unicode);
        pdb.dumpToFile(books, file);
        System.out.println(books.size() + " books written to " + file);
      }
      else if (arg.equals("-dump-raw")) {
        String file = args[i++];
        DataOutputStream ostr = new DataOutputStream(new FileOutputStream(file));
        Iterator iter = books.iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
          book.writeData(ostr);
        }
        ostr.close();
        System.out.println(books.size() + " books written to " + file);
      }
      else if (arg.equals("-load-raw")) {
        String file = args[i++];
        books = new ArrayList();
        DataInputStream istr = new DataInputStream(new FileInputStream(file));
        while (true) {
          try {
            BookRecord book = new BookRecord();
            book.readData(istr);
            books.add(book);
          }
          catch (EOFException ex) {
            break;
          }
        }
        istr.close();
        System.out.println(books.size() + " books read from " + file);
      }
      else if (arg.equals("-read-xml")) {
        String file = args[i++];
        books = new XMLImporter().importFile(file);
        System.out.println(books.size() + " books read from " + file);
      }
      else if (arg.equals("-write-xml")) {
        String file = args[i++];
        new XMLImporter().exportFile(books, file, null);
        System.out.println(books.size() + " books written to " + file);
      }
      else if (arg.equals("-write-html")) {
        String file = args[i++];
        String xsl = args[i++];
        new XMLImporter().exportFile(books, file, xsl);
        System.out.println(books.size() + " books written to " + file + " using " + xsl);
      }
      else if (arg.equals("-print")) {
        System.out.println(books.size() + " books:");
        Iterator iter = books.iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
          if (false) System.out.println(book.getISBN());
          System.out.println(book.toFormattedString());
        }
      }
      else if (arg.equals("-download")) {
        String user = args[i++];
        books = importer.download(user);
        System.out.println(books.size() + " books downloaded for " + user);
      }
      else {
        throw new PalmThingException("Unknown switch: " + arg);
      }
    }
  }
}
