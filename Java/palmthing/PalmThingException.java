/*! -*- Mode: Java -*-
* Module: PalmThingException.java
* Version: $Header$
*/

package palmthing;

/** Base class for exceptions within the subsystem. */
public class PalmThingException extends Exception {
  public PalmThingException() {
  }
  public PalmThingException(String msg) {
    super(msg);
  }
  public PalmThingException(Throwable cause) {
    super(cause);
  }
  public PalmThingException(String msg, Throwable cause) {
    super(msg, cause);
  }
}
