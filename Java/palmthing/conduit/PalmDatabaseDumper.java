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

  private byte[] m_name;
  private short m_version;
  private int m_creator, m_type;

  public PalmDatabaseDumper(String name, short version, String creator, String type) {
    m_name = new byte[32];
    try {
      byte[] nb = name.getBytes("ASCII");
      System.arraycopy(nb, 0, m_name, 0, Math.min(nb.length, 31));
    }
    catch (UnsupportedEncodingException ex) {
    }

    m_version = version;

    m_creator = char4(creator);
    m_type = char4(type);
  }

  private int char4(String str) {
    int result = 0;
    try {
      byte[] sb = str.getBytes("ASCII");
      for (int i = 0; i < 4; i++) {
        if (i < sb.length) {
          result |= (sb[i] << (24 - i * 8));
        }
      }
    }
    catch (UnsupportedEncodingException ex) {
    }
    return result;
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

  private static long g_timeOffset;
  static {
    Calendar cal = Calendar.getInstance();
    cal.set(1904, 0, 1, 0, 0, 0);
    g_timeOffset = cal.getTime().getTime();
  }

  public void dumpToStream(Collection records, OutputStream ostr)
      throws PalmThingException, IOException {
    int now = (int)((System.currentTimeMillis() - g_timeOffset) / 1000);

    int nrecs = records.size();
    byte[][] recs = new byte[nrecs][];
    byte[] attrs = new byte[nrecs];
    int i = 0;
    Iterator iter = records.iterator();
    while (iter.hasNext()) {
      Record record = (Record)iter.next();
      ByteArrayOutputStream bstr = new ByteArrayOutputStream();
      DataOutputStream rstr = new DataOutputStream(bstr);
      record.writeData(rstr);
      rstr.flush();
      recs[i] = bstr.toByteArray();
      attrs[i] = (byte)(record.getCategoryIndex() | (record.isPrivate() ? 0x10 : 0));
      i++;
    }
    
    DataOutputStream out = new DataOutputStream(ostr);

    int offset = (80 + 8 * recs.length);

    out.write(m_name);
    out.writeShort(0x0010);
    out.writeShort(m_version);
    out.writeInt(now);          // cdate
    out.writeInt(now);          // mdate
    out.writeInt(0);            // bdate
    out.writeInt(0);            // mod num
    out.writeInt((m_appInfoBlock == null) ? 0 : offset);
    out.writeInt(0);            // sort info
    out.writeInt(m_type);
    out.writeInt(m_creator);
    out.writeInt(0x10000 + recs.length); // seed
    out.writeInt(0);            // next rec
    out.writeShort((short)recs.length);

    if (m_appInfoBlock != null)
      offset += m_appInfoBlock.length;
    for (i = 0; i < recs.length; i++) {
      out.writeInt(offset);
      out.writeInt((attrs[i] << 24) | 0x10000 | i); // attrs and uid
      offset += recs[i].length;
    }

    out.writeShort(0);          // gap

    if (m_appInfoBlock != null)
      out.write(m_appInfoBlock);

    for (i = 0; i < recs.length; i++) {
      out.write(recs[i]);
    }
    
    out.flush();
  }
}
