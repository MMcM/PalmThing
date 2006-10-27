/*! -*- Mode: Java -*-
* Module: PalmDatabaseDumper.java
* Version: $Header$
*/
package palmthing.conduit;

import palmthing.*;

import palm.conduit.*;

import java.util.*;
import java.io.*;

/** Write the client format of a Palm database directly.
 * 
 * References: <a href="http://www.palmos.com/dev/support/docs/fileformats/PDB+PRCFormat.html">PDB and PRC Database Formats</a>
 * and <a href="http://membres.lycos.fr/microfirst/palm/pdb.html">The Pilot Record Database Format</a>.
 */
public class PalmDatabaseDumper {

  // ... creator, etc.

  public PalmDatabaseDumper() {
  }

  private byte[] m_appInfoBlock = null;

  public byte[] getAppInfoBlock() {
    return m_appInfoBlock;
  }

  public void setAppInfoBlock(byte[] appInfoBlock) {
    m_appInfoBlock = appInfoBlock;
  }

  public void dumpToFile(Collection books, String file)
      throws PalmThingException, IOException {
    FileOutputStream fstr = null;
    try {
      fstr = new FileOutputStream(file);
      dumpToStream(books, fstr);
    }
    finally {
      if (fstr != null)
        fstr.close();
    }
  }

  public void dumpToStream(Collection books, OutputStream ostr)
      throws PalmThingException, IOException {

    // TODO: ...

    ostr.flush();
  }
}
