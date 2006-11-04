/*! -*- Mode: Java -*-
* Module: UnicodeUtils.java
* Version: $Header$
*/
package palmthing;

import java.lang.reflect.*;

public class UnicodeUtils {
  // All static methods.
  private UnicodeUtils() {
  }

  private static final int HANGUL_SYLLABLE_BASE = 0xAC00;
  private static final int HANGUL_SYLLABLE_COUNT = 11172;
  private static final int HANGUL_CHOSEONG_BASE = 0x1100;
  private static final int HANGUL_CHOSEONG_COUNT = 19;
  private static final int HANGUL_JUNGSEONG_BASE = 0x1161;
  private static final int HANGUL_JUNGSEONG_COUNT = 21;
  private static final int HANGUL_JONGSEONG_BASE = 0x11a7;
  private static final int HANGUL_JONGSEONG_COUNT = 28;
  
  public static boolean hasHangulSyllables(String str) {
    for (int i = 0; i < str.length(); i++) {
      char ch = str.charAt(i);
      if ((ch >= HANGUL_SYLLABLE_BASE) && 
          (ch < HANGUL_SYLLABLE_BASE + HANGUL_SYLLABLE_COUNT))
        return true;
    }
    return false;
  }

  public static String decomposeHangulSyllables(String str) {
    StringBuffer buf = new StringBuffer(str);
    for (int i = 0; i < buf.length(); i++) {
      int offset = (int)buf.charAt(i) - HANGUL_SYLLABLE_BASE;
      if ((offset >= 0) && (offset < HANGUL_SYLLABLE_COUNT)) {
        buf.deleteCharAt(i);
        int choseong = (offset / (HANGUL_JUNGSEONG_COUNT * HANGUL_JONGSEONG_COUNT));
        int jungseong = (offset % (HANGUL_JUNGSEONG_COUNT * HANGUL_JONGSEONG_COUNT)) / 
          HANGUL_JONGSEONG_COUNT;
        int jongseong = (offset % HANGUL_JONGSEONG_COUNT);
        if (jongseong == 0) {   //no trailing consonant
          buf.insert(i, new char[] {
                       (char)(HANGUL_CHOSEONG_BASE + choseong),
                       (char)(HANGUL_JUNGSEONG_BASE + jungseong)
                     });
          i++;
        }
        else {
          buf.insert(i, new char[] {
                       (char)(HANGUL_CHOSEONG_BASE + choseong),
                       (char)(HANGUL_JUNGSEONG_BASE + jungseong),
                       (char)(HANGUL_JONGSEONG_BASE + jongseong)
                     });
          i+=2;
        }
      }
    }
    return buf.toString();
  }
  
  // Libraries that use MARC-8 / ANSEL will tend to have decomposed
  // accents, which can end up in the library.  Converting to
  // ISO-8859-1 works better with composed.  Every version of Java
  // knows how to do this internally, but the exact details are
  // different for each major release.
  public static interface Normalizer {
    public String normalize(String str);
  }

  public static Normalizer getNormalizer() {
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
        return new Normalizer() {
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
        return new Normalizer() {
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
        return new Normalizer() {
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
      }
    }
    return null;
  }

}
