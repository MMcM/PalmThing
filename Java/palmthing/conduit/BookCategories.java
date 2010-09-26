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
  public static int DEFAULT = 1;

  // TODO: I18N
  private static String g_Unfiled = "Unfiled";
  private static String g_Default = "Main";

  public static void setDefaultCategory(String defaultCategory) {
    g_Default = defaultCategory;
  }
  
  public static Vector getCategories(Collection books) {
    Vector result = new Vector(Category.MAX_CATEGORIES);

    result.add(new Category(g_Unfiled, UNFILED, UNFILED));

    Iterator iter = books.iterator();
    while (iter.hasNext()) {
      BookRecord book = (BookRecord)iter.next();
      if (book.getCategory() == null) {
        // Only add Main category if some book will need it.
        result.add(new Category(g_Default, DEFAULT, DEFAULT));
        break;
      }
    }
    
    Map byName = new HashMap();
    iter = books.iterator();
    while (iter.hasNext()) {
      BookRecord book = (BookRecord)iter.next();
      String cname = book.getCategory();
      if (cname == null) continue;
      Category category = (Category)byName.get(cname);
      if (category != null) continue;
      category = new Category(cname, result.size(), result.size());
      result.add(category);
      byName.put(cname, category);
    }

    while (result.size() < Category.MAX_CATEGORIES) {
      result.add(new Category(""));
    }

    return result;
  }

  public static Vector mergeCategories(Vector ocats, Vector ncats) {
    Vector result = new Vector(Category.MAX_CATEGORIES);

    Map byName = new HashMap();
    Stack empty = new Stack();

    Iterator iter = ocats.iterator();
    while (iter.hasNext()) {
      Category ocat = (Category)iter.next();
      Category cat = new Category(ocat.getName(), ocat.getId(), ocat.getIndex());
      result.add(cat);
      if (cat.getName().length() > 0)
        byName.put(cat.getName(), cat);
      else
        empty.push(cat);
    }

    iter = ncats.iterator();
    while (iter.hasNext()) {
      Category ncat = (Category)iter.next();
      if (ncat.getName().length() == 0) continue;
      Category ocat = (Category)byName.get(ncat.getName());
      if (ocat != null) continue;
      ocat = (Category)empty.pop();
      ocat.setName(ncat.getName());
      byName.put(ocat.getName(), ocat);
    }

    return result;
  }

  public static void setCategories(Collection books, Vector categories) {
    String[] cnames = new String[Category.MAX_CATEGORIES];

    int limit = UNFILED;        // Lowest that does not use named category.
    if ((categories.size() > DEFAULT) &&
        categories.get(DEFAULT).equals(g_Default))
      limit = DEFAULT;

    Iterator iter = books.iterator();
    while (iter.hasNext()) {
      BookRecord book = (BookRecord)iter.next();
      int index = book.getCategoryIndex();
      if (index <= limit) continue;
      String cname = cnames[index];
      if (cname == null) {
        Category category = (Category)categories.get(index);
        cname = category.getName();
        cnames[index] = cname;
      }
      book.setCategory(cname);
    }
  }

  public static void setCategoryIndices(Collection books, Vector categories) {
    Map byName = new HashMap();

    Iterator iter = categories.iterator();
    while (iter.hasNext()) {
      Category category = (Category)iter.next();
      if (category.getName().length() == 0) continue;
      String cname = category.getName();
      byName.put(cname, category);
    }

    iter = books.iterator();
    while (iter.hasNext()) {
      BookRecord book = (BookRecord)iter.next();
      String cname = book.getCategory();
      if (cname == null) continue;
      Category category = (Category)byName.get(cname);
      book.setCategoryIndex(category.getIndex());
    }
  }
}
