/*! -*- Mode: C -*-
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

  private int m_bookID;
  private String m_stringFields[] = new String[BOOK_NFIELDS];

  protected void setBookID(int bookID) {
    m_bookID = bookID;
  }

  protected void setStringField(int index, String field) {
    m_stringFields[index] = field;
  }

  public int getBookID() {
    return m_bookID;
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
        // TODO: I'm not sure that
        // AbstractRecord.writeCString/readCString work outside ASCII.
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
