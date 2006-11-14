/*! -*- Mode: Java -*-
* Module: PalmDatabaseServlet.java
* Version: $Header$
*/
package palmthing.servlet;

import palmthing.*;
import palmthing.conduit.*;

import javax.servlet.*;
import javax.servlet.http.*;

import java.util.*;
import java.io.*;
import java.net.*;

/** Return a .PDB file directly by converting in the server. */
public class PalmDatabaseServlet extends HttpServlet {
  protected void doGet(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException {
    try {
      String userid;
      int sort = 5;
      boolean unicode = false;
    
      userid = request.getParameter("userid");
      if (userid == null) {
        Cookie[] cookies = request.getCookies();
        if (cookies != null) {
          for (int i = 0; i < cookies.length; i++) {
            Cookie cookie = cookies[i];
            if ("cookie_userid".equals(cookie.getName())) {
              userid = cookie.getValue();
              break;
            }
          }
        }
        if (userid == null) {
          // Need to log in.
          response.sendRedirect("http://www.librarything.com/");
          return;
        }
      }

      response.setContentType("application/vnd.palm");
      response.addHeader("Content-Disposition", 
                         "attachment; filename=PalmThing-Books.pdb");
      response.addHeader("Pragma", "no-cache");

      String param = request.getParameter("sort");
      if (param != null)
        sort = Integer.parseInt(param);

      if (request.getParameter("unicode") != null)
        unicode = true;
    
      LibraryThingImporter importer = new LibraryThingImporter();
      List books = importer.download(userid);

      if (sort != 0)
        Collections.sort(books, new BookRecordComparator(sort));

      PalmDatabaseDumper pdb = importer.getPalmDatabaseDumper(books, sort, unicode);
      pdb.dumpToStream(books, response.getOutputStream());
    }
    catch (PalmThingException ex) {
      throw new ServletException(ex);
    }
  }
}