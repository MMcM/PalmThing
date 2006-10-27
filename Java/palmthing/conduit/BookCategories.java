/*! -*- Mode: Java -*-
* Module: BookCategories.java
* Version: $Header$
*/
package palmthing.conduit;

import palmthing.*;

import palm.conduit.*;

import java.util.*;

public class BookCategories {
  // All static methods.
  private BookCategories() {
  }

  // 276: renamed flags, labels[], uids[], lastid, pad.
  public static final int SIZE = 2 + (Category.CATEGORY_LENGTH * Category.MAX_CATEGORIES) + Category.MAX_CATEGORIES + 1 + 1;

  public static int UNFILED = 0;
  public static int MAIN = 1;

  // TODO: I18N
  private static String g_Unfiled = "Unfiled";
  private static String g_Main = "Main";

  public static Vector getCategories(Collection books) {
    Vector result = new Vector(Category.MAX_CATEGORIES);
    // TODO: do something
    return result;
  }

  public static void setCategoryIndices(Collection books, Vector getCategories) {
  }

  public static Vector mergeCategories(Vector cats1, Vector cats2) {
    Vector result = new Vector(Category.MAX_CATEGORIES);
    // TODO: do something
    return result;
  }
}
