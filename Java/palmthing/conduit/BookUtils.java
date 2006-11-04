/*! -*- Mode: Java -*-
* Module: BooksUtils.java
* Version: $Header$
*/
package palmthing.conduit;

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
    if (col == null)
      return book.getCategoryTag();
    String ctag = book.getCategoryTag();
    if (ctag != null)
      col = ctag + ", " + col;
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
    if (col.length() == 0)
      col = null;
    return col;
  }

}
