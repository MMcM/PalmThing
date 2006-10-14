/*! -*- Mode: C -*-
* Module: PalmThingConduit.java
* Version: $Header$
*/

package palmthing.conduit;

import palm.conduit.*;
import java.io.*;

/** PalmThing conduit.

**/
public class PalmThingConduit implements Conduit {
  static final String NAME = "PalmThing Conduit";

  public void open(SyncProperties props) {
    Log.startSync();

    try {
      //...

      Log.endSync();
    } 
    catch (Exception ex) {
      ex.printStackTrace();
      Log.abortSync();
    }
  }

  public int configure(ConfigureConduitInfo info) {
    return 0;
  }

  public String name() {
    return NAME;
  }
}
