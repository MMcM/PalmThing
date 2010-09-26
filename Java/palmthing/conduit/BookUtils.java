/*! -*- Mode: Java -*-
* Module: BooksUtils.java
* Version: $Header$
*/
package palmthing.conduit;

import java.io.*;

public class BookUtils {
  // All static methods.
  private BookUtils() {
  }

  public static String[] g_noise = { "a ", "the " };

  public static String trimTitle(String str) {
    for (int i = 0; i < g_noise.length; i++) {
      String noise = g_noise[i];
      if ((str.length() > noise.length()) &&
          (str.substring(0, noise.length()).equalsIgnoreCase(noise))) {
        return str.substring(noise.length());
      }
    }
    return str;
  }

  public static String mergeCategoryTag(String col, BookRecord book) {
    String cname = book.getCategory();
    if (cname != null)
      cname = "@" + cname;
    if (col == null)
      return cname;
    if (cname != null)
      col = cname + ", " + col;
    return col;
  }

  public static String extractCategoryTag(String col, BookRecord book) {
    int atsign = 0;
    while (true) {
      atsign = col.indexOf('@', atsign);
      if (atsign < 0) break;
      if ((atsign == 0) || (col.charAt(atsign-1) == ',')) {
        int comma = col.indexOf(',', atsign);
        if (comma < 0) comma = col.length();
        book.setCategory(col.substring(atsign+1, comma));
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
    if (col.length() == 0)
      col = null;
    return col;
  }

  public static String getLinkURL(BookRecord book) {
    StringBuffer buf = new StringBuffer("http://www.librarything.com/");
    if (book.getBookID() != 0) {
      buf.append("work.php?book=");
      buf.append(book.getBookID());
    }
    else {
      buf.append("addbooks.php?search=");
      if (book.getISBN() != null) {
        buf.append(book.getISBN());
      }
      else {
        if (book.getAuthor() != null) {
          buf.append(toURL(book.getAuthor()));
          if (book.getTitle() != null)
            buf.append(",+");
        }
        if (book.getTitle() != null) {
          buf.append(toURL(BookUtils.trimTitle(book.getTitle())));
        }
      }
    }
    return buf.toString();
  }

  private static final char[] g_hexChars = "0123456789ABCDEF".toCharArray();
  /** Encode string for inclusion in URL. */
  public static String toURL(String value) {
    StringBuffer sb = new StringBuffer();
    for (int i = 0; i < value.length(); i++) {
      char c = value.charAt(i);
      if (c > 128) {
        // Actually, it depends on the headers and server defaults
        // what encoding gets used.
        try {
          byte[] bytes = new String(new char[] { c }).getBytes("UTF-8");
          for (int j = 0; j < bytes.length; j++) {
            int b = (int)bytes[j] & 0xFF; // Do not sign extend.
            sb.append('%');
            sb.append(g_hexChars[b / 16]);
            sb.append(g_hexChars[b % 16]);
          }
        }
        catch (UnsupportedEncodingException ex) {
          throw new RuntimeException(ex);
        }
      }
      else if (!Character.isLetterOrDigit(c)) {
        sb.append('%');
        sb.append(g_hexChars[c / 16]);
        sb.append(g_hexChars[c % 16]);
      }
      else {
        sb.append(c);
      }
    }
    return sb.toString();
  }
}
