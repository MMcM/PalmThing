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

  // Must agree with BookRecordPacked in BookDatabase.c.
  public void writeData(DataOutputStream out) throws IOException {
    out.writeInt(m_bookID);

    short mask = 0;
    for (int i = 0; i < BOOK_NFIELDS; i++) {
      if (m_stringFields[i] != null) {
        mask |= (1 << i);
      }
    }
    out.writeShort(mask);

    for (int i = 0; i < BOOK_NFIELDS; i++) {
      if (m_stringFields[i] != null) {
        // TODO: Unicode support.
        writeCString(out, m_stringFields[i]);
      }
    }
  }

  public void readData(DataInputStream in) throws IOException {
    m_bookID = in.readInt();
    
    short mask = in.readShort();
    
    for (int i = 0; i < BOOK_NFIELDS; i++) {
      if ((mask & (1 << i)) != 0) {
        m_stringFields[i] = readCString(in);
      }
    }
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
            "  summary: " + getSummary() + "\r\n" +
            "}\r\n" +
            super.toFormattedString());
  }
}
