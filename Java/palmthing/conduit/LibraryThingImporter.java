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
        if (g_recordIDField.equals(col))
          recordIDFieldCol = i;
        else if (g_bookIDField.equals(col))
          bookIDFieldCol = i;
        else if (m_authorField.equals(col))
          stringFieldCols[BookRecord.FIELD_AUTHOR] = i;
        else {
          for (int j = 0; j < g_stringFields.length; j++) {
            if (g_stringFields[j].equals(col)) {
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
        if (fieldCol >= cols.length)
          System.err.println(line); // TODO: Specific exception?  continue?
        String col = cols[fieldCol];
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
      {
        int atsign = 0;
        while (true) {
          atsign = col.indexOf('@', atsign);
          if (atsign < 0) break;
          if ((atsign == 0) || (col.charAt(atsign-1) == ',')) {
            int comma = col.indexOf(',', atsign);
            if (comma < 0) comma = col.length();
            book.setCategoryTag(col.substring(atsign, comma));
            StringBuffer buf = new StringBuffer();
            if (atsign > 0)
              buf.append(col.substring(0, atsign-1));
            if (comma < col.length())
              buf.append(col.substring((buf.length() > 0) ? comma : comma+1));
            col = buf.toString();
            break;
          }
          // Don't be fooled by @ in the middle of a tag.
          atsign++;
        }
        col = stringReplaceAll(col, ",", ", ");
      }
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
      {
        col = stringReplaceAll(col, ", ", ",");
        String ctag = book.getCategoryTag();
        if (ctag != null)
          col = ctag + "," + col;
      }
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

  // Split the given line up into columns.
  protected String[] splitLine(String line) {
    String[] cols = stringSplit(line, m_fieldDelimiter);
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
            final Integer ZERO = new Integer(0);
            public String normalize(String str) {
              try {
                return (String)f_meth.invoke(null, new Object[] { 
                                               str, Boolean.FALSE, ZERO
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

  public static void main(String[] args) throws Exception {
    if (args.length == 0) {
      System.out.println("usage: LibraryThingImporter" +
                         " [-encoding enc] [-delimiter delim]" +
                         " [-author {last_first,first_last}] [-summary bool]" + 
                         " [-validate bool] [-unicode bool]" +
                         " [-read file] [-append file] [-write file]" +
                         " [-sort field] [-dump file]" + 
                         " [-dump-raw file] [-load-raw file]" +
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
        books = importer.importFile(args[i++]);
      }
      else if (arg.equals("-append")) {
        books.addAll(importer.importFile(args[i++]));
      }
      else if (arg.equals("-write")) {
        importer.exportFile(books, args[i++]);
      }
      else if (arg.equals("-sort")) {
        sort = Integer.parseInt(args[i++]);
        Collections.sort(books, new BookRecordComparator(sort));
      }
      else if (arg.equals("-dump")) {
        Vector categories = BookCategories.getCategories(books);
        BookCategories.setCategoryIndices(books, categories);
        PalmDatabaseDumper pdb = new PalmDatabaseDumper("PalmThing-Books", (short)0, 
                                                        "plTN", "DATA");
        byte[] catBytes = Category.toBytes(categories);
        byte[] appInfo = new byte[BookCategories.SIZE + 2];
        System.arraycopy(catBytes, 0, appInfo, 0, catBytes.length);
        appInfo[BookCategories.SIZE] = (byte)sort;
        pdb.setAppInfoBlock(appInfo);
        pdb.dumpToFile(books, args[i++]);
      }
      else if (arg.equals("-dump-raw")) {
        DataOutputStream ostr = new DataOutputStream(new FileOutputStream(args[i++]));
        Iterator iter = books.iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
          book.writeData(ostr);
        }
        ostr.close();
      }
      else if (arg.equals("-load-raw")) {
        books = new ArrayList();
        DataInputStream istr = new DataInputStream(new FileInputStream(args[i++]));
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
      }
      else if (arg.equals("-print")) {
        Iterator iter = books.iterator();
        while (iter.hasNext()) {
          BookRecord book = (BookRecord)iter.next();
          if (false) System.out.println(book.getISBN());
          System.out.println(book.toFormattedString());
        }
      }
      else {
        throw new PalmThingException("Unknown switch: " + arg);
      }
    }
  }
}
