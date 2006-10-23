/*! -*- Mode: Java -*-
* Module: BookRecordComparator.java
* Version: $Header$
*/
package palmthing.conduit;

import java.util.*;

public class BookRecordComparator implements Comparator {

  // Must agree with enum in AppGlobals.h.

  public static final int KEY_NONE = 0;
  public static final int KEY_TITLE = 1;
  public static final int KEY_AUTHOR = 2;
  public static final int KEY_ISBN = 3;
  public static final int KEY_SUMMARY = 4;
  public static final int KEY_TITLE_AUTHOR = 5;
  public static final int KEY_AUTHOR_TITLE = 6;

  private int m_sortKey, m_field1, m_field2;
  private boolean m_reverse;

  public BookRecordComparator(int sortKey) {
    m_sortKey = sortKey;

    m_reverse = false;
    if (sortKey < 0) {
      m_reverse = true;
      sortKey = -sortKey;
    }
    
    m_field1 = m_field2 = -1;
    switch (sortKey) {
    case KEY_TITLE:
      m_field1 = BookRecord.FIELD_TITLE;
      break;
    case KEY_AUTHOR:
      m_field1 = BookRecord.FIELD_AUTHOR;
      break;
    case KEY_ISBN:
      m_field1 = BookRecord.FIELD_ISBN;
      break;
    case KEY_SUMMARY:
      m_field1 = BookRecord.FIELD_SUMMARY;
      break;
    case KEY_TITLE_AUTHOR:
      m_field1 = BookRecord.FIELD_TITLE;
      m_field2 = BookRecord.FIELD_AUTHOR;
      break;
    case KEY_AUTHOR_TITLE:
      m_field1 = BookRecord.FIELD_AUTHOR;
      m_field2 = BookRecord.FIELD_TITLE;
      break;
    }
  }

  public int compare(Object o1, Object o2) {
    BookRecord book1 = (BookRecord)o1;
    BookRecord book2 = (BookRecord)o2;

    if (m_field1 >= 0) {
      int comp = compareFields(book1, book2, m_field1);
      if (comp != 0) 
        return (m_reverse) ? -comp : comp;
    }
    if (m_field2 >= 0) {
      int comp = compareFields(book1, book2, m_field2);
      if (comp != 0) 
        return (m_reverse) ? -comp : comp;
    }
    return 0;
  }

  public static String[] g_noise = { "a ", "the " };

  private static String trimTitle(String str) {
    for (int i = 0; i < g_noise.length; i++) {
      String noise = g_noise[i];
      if ((str.length() > noise.length()) &&
          (str.substring(0, noise.length()).equalsIgnoreCase(noise))) {
        return str.substring(noise.length());
      }
    }
    return str;
  }

  private static int compareFields(BookRecord book1, BookRecord book2, int field) {
    String str1 = book1.getStringField(field);
    String str2 = book2.getStringField(field);
    if (str1 == null) {
      if (str2 == null)
        return 0;
      else
        return -1;
    }
    else if (str2 == null) {
      return +1;
    }
    else {
      switch (field) {
      case BookRecord.FIELD_TITLE:
      case BookRecord.FIELD_SUMMARY:
        str1 = trimTitle(str1);
        str2 = trimTitle(str2);
        return str1.compareToIgnoreCase(str2);
      case BookRecord.FIELD_ISBN:
        // TODO: Special ISBN comparison ignoring punctuation.
      default:
        return str1.compareToIgnoreCase(str2);
      }
    }
  }
}
