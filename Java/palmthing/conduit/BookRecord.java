/*! -*- Mode: Java -*-
* Module: BookRecord.java
* Version: $Header$
*/
package palmthing.conduit;

import palmthing.*;

import palm.conduit.*;

import java.io.*;

public class BookRecord extends AbstractRecord {
  // Must agree with enum in AppGlobals.h.

  public static final int FIELD_TITLE = 0;
  public static final int FIELD_AUTHOR = 1;
  public static final int FIELD_DATE = 2;
  public static final int FIELD_ISBN = 3;
  public static final int FIELD_PUBLICATION = 4;
  public static final int FIELD_TAGS = 5;
  public static final int FIELD_SUMMARY = 6;
  public static final int FIELD_COMMENTS = 7;

  public static final int BOOK_NFIELDS = 8;

  public static final int RECORD_ID_NONE = 0;

  public static final int BOOK_ID_NONE = 0;

  private int m_bookID;
  private String m_categoryTag;
  private String m_stringFields[] = new String[BOOK_NFIELDS];

  protected String getStringField(int index) {
    return m_stringFields[index];
  }
  protected void setStringField(int index, String field) {
    m_stringFields[index] = field;
  }

  public int getBookID() {
    return m_bookID;
  }
  protected void setBookID(int bookID) {
    m_bookID = bookID;
  }

  public String getCategoryTag() {
    return m_categoryTag;
  }
  public void setCategoryTag(String categoryTag) {
    m_categoryTag = categoryTag;
  }

  public String getTitle() {
    return m_stringFields[FIELD_TITLE];
  }
  public String getAuthor() {
    return m_stringFields[FIELD_AUTHOR];
  }
  public String getDate() {
    return m_stringFields[FIELD_DATE];
  }
  public String getISBN() {
    return m_stringFields[FIELD_ISBN];
  }
  public String getPublication() {
    return m_stringFields[FIELD_PUBLICATION];
  }
  public String getTags() {
    return m_stringFields[FIELD_TAGS];
  }
  public String getSummary() {
    return m_stringFields[FIELD_SUMMARY];
  }
  public String getComments() {
    return m_stringFields[FIELD_COMMENTS];
  }

  private static boolean g_unicode = false;

  public static boolean getUnicode() {
    return g_unicode;
  }

  public static void setUnicode(boolean unicode) {
    g_unicode = unicode;
  }

  private static String g_CStringEncoding = null;
  
  public static String getCStringEncoding() {
    return g_CStringEncoding;
  }

  public static void setCStringEncoding(String encoding) {
    g_CStringEncoding = encoding;
  }
  
  // Must agree with BookRecordPacked in BookDatabase.c.
  public void writeData(DataOutputStream out) throws IOException {
    out.writeInt(m_bookID);

    short mask = 0, unicodeMask = 0;
    for (int i = 0; i < BOOK_NFIELDS; i++) {
      if (m_stringFields[i] != null) {
        mask |= (1 << i);
        if (g_unicode && requiresUnicode(m_stringFields[i]))
          unicodeMask |= (1 << i);
      }
    }
    out.writeShort(mask);

    if (g_unicode)
      out.writeShort(unicodeMask);

    for (int i = 0; i < BOOK_NFIELDS; i++) {
      if (m_stringFields[i] != null) {
        if (g_unicode && ((unicodeMask & (1 << i)) != 0))
          writeUTFString(out, m_stringFields[i]);
        else
          writeCString(out, m_stringFields[i]);
      }
    }
  }

  public void readData(DataInputStream in) throws IOException {
    m_bookID = in.readInt();
    
    short mask = in.readShort();
    short unicodeMask = 0;

    if (g_unicode)
      unicodeMask = in.readShort();
    
    for (int i = 0; i < BOOK_NFIELDS; i++) {
      if ((mask & (1 << i)) != 0) {
        if (g_unicode && ((unicodeMask & (1 << i)) != 0))
          m_stringFields[i] = readUTFString(in);
        else
          m_stringFields[i] = readCString(in);
      }
    }
  }

  protected static String readUTFString(DataInputStream in) throws IOException {
    ByteArrayOutputStream bstr = new ByteArrayOutputStream();
    while (true) {
      int c = in.read();
      if (c == 0) break;
      bstr.write((byte)c);
    }
    return bstr.toString("UTF-8");
  }

  static public void writeUTFString(DataOutputStream out, String str) 
      throws IOException {
    out.write(str.getBytes("UTF-8"));
    out.write(0);
  }

  static public String readCString(DataInputStream in) throws IOException {
    ByteArrayOutputStream bstr = new ByteArrayOutputStream();
    while (true) {
      int b = in.read();
      if (b <= 0) break;
      bstr.write(b);
    }
    if (g_CStringEncoding == null)
      return bstr.toString();
    else
      return bstr.toString(g_CStringEncoding);
  }

  static public void writeCString(DataOutputStream out, String string) 
      throws IOException {
    if (g_CStringEncoding == null)
      out.write(string.getBytes());
    else
      out.write(string.getBytes(g_CStringEncoding));
    out.write(0);
  }

  protected static boolean requiresUnicode(String str) {
    for (int i = 0; i < str.length(); i++) {
      if (str.charAt(i) >= 0x100)
        return true;
    }
    return false;
  }

  public int hashCode() {
    return m_bookID;
  }

  public boolean equals(Object obj) {
    if (!(obj instanceof BookRecord)) return false;
    BookRecord other = (BookRecord)obj;
    if (m_bookID != other.m_bookID) return false;
    if (!super.equals(obj)) return false;
    if (m_categoryTag == null)
      return (other.m_categoryTag == null);
    else
      return m_categoryTag.equals(other.m_categoryTag);
  }

  public String toString() {
    return "Book record: " + getBookID() + " " + super.toString();
  }

  public String toFormattedString() {
    return ("Book record: {\r\n" +
            "  id: " + getBookID() + "\r\n" +
            "  category: " + getCategoryTag() + "\r\n" +
            "  summary: " + getSummary() + "\r\n" +
            "}\r\n" +
            super.toFormattedString());
  }
}
