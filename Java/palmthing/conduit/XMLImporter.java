/*! -*- Mode: Java -*-
* Module: XMLImporter.java
* Version: $Header$
*/
package palmthing.conduit;

import palmthing.*;

import javax.xml.parsers.*;
import javax.xml.transform.*;
import javax.xml.transform.sax.*;
import javax.xml.transform.stream.*;
import org.xml.sax.*;
import org.xml.sax.helpers.*;
//import org.apache.xml.serializer.OutputPropertiesFactory;

import java.util.*;
import java.io.*;

/** Import (and export) records to simple XML format for use with XSLT and other tools.
 */
public class XMLImporter extends DefaultHandler {

  public static final String g_rootElement = "books";
  public static final String g_bookElement = "book";
  public static final String g_categoryTagAttribute = "category";
  public static final String g_recordIDAttribute = "record-id";
  public static final String g_bookIDAttribute = "book-id";
  // Order must agree with BookRecord.
  public static final String[] g_stringFieldElements = {
    "title",
    "author",
    "date",
    "ISBN",
    "publication",
    "tags",
    "summary",
    "comments",
  };

  private List m_books;
  private BookRecord m_book;
  private int m_field;
  private StringBuffer m_fieldBuffer;

  public List importFile(String file) throws PalmThingException, IOException {
    return importSource(new InputSource(file));
  }

  public List importStream(InputStream str) throws PalmThingException, IOException {
    return importSource(new InputSource(str));
  }

  public List importSource(InputSource src) throws PalmThingException, IOException {
    try {
      SAXParserFactory fact = SAXParserFactory.newInstance();
      // This is so that a localName will always be passed, even if
      // there are no namespace prefixes used.
      fact.setNamespaceAware(true);
      SAXParser sax = fact.newSAXParser();
      sax.parse(src, this);
    }
    catch (ParserConfigurationException ex) {
      throw new PalmThingException(ex);
    }
    catch (SAXException ex) {
      throw new PalmThingException(ex);
    }
    return m_books;
  }

  public void startElement(String namespaceURI, String localName, String qualifiedName, 
                           Attributes attrs) {
    if (g_rootElement.equals(localName)) {
      m_books = new ArrayList();
      m_book = null;
    }
    else if (g_bookElement.equals(localName)) {
      m_book = new BookRecord();
      String attr = attrs.getValue(g_categoryTagAttribute);
      if (attr != null)
        m_book.setCategoryTag(attr);
      attr = attrs.getValue(g_recordIDAttribute);
      if (attr != null)
        m_book.setId(Integer.parseInt(attr));
      attr = attrs.getValue(g_bookIDAttribute);
      if (attr != null)
        m_book.setBookID(Integer.parseInt(attr));
    }
    else {
      m_fieldBuffer = null;
      for (int i = 0; i < g_stringFieldElements.length; i++) {
        if (g_stringFieldElements[i].equals(localName)) {
          m_field = i;
          m_fieldBuffer = new StringBuffer();
          break;
        }
      }
    }
  }

  public void endElement(String namespaceURI, String localName, String qualifiedName) {
    if (g_bookElement.equals(localName)) {
      m_books.add(m_book);
      m_book = null;
    }
    else {
      if (m_fieldBuffer != null) {
        m_book.setStringField(m_field, m_fieldBuffer.toString());
        m_fieldBuffer = null;
      }
    }
  }

  public void characters(char[] ch, int start, int length) {
    if (m_fieldBuffer != null)
      m_fieldBuffer.append(ch, start, length);
  }

  public void exportFile(Collection books, String file, String xsl)
      throws PalmThingException, IOException {
    exportResult(books, new StreamResult(file), xsl);
  }

  public void exportStream(Collection books, OutputStream str, String xsl)
      throws PalmThingException, IOException {
    exportResult(books, new StreamResult(str), xsl);
  }

  public void exportResult(Collection books, Result rs, String xsl)
      throws PalmThingException, IOException {
    try {
      SAXTransformerFactory fact = 
        (SAXTransformerFactory)TransformerFactory.newInstance();

      TransformerHandler hand;
      if (xsl == null) {
        hand = fact.newTransformerHandler();
        Properties props = new Properties();
        props.put(OutputKeys.ENCODING, "UTF-8");
        props.put(OutputKeys.METHOD, "xml");
        props.put(OutputKeys.INDENT, "yes");
        //props.put(OutputPropertiesFactory.S_KEY_INDENT_AMOUNT, "2");
        hand.getTransformer().setOutputProperties(props);
      }
      else {
        hand = fact.newTransformerHandler(new StreamSource(xsl));
      }
      hand.setResult(rs);

      exportHandler(books, hand);
    }
    catch (TransformerConfigurationException ex) {
      throw new PalmThingException(ex);
    }
    catch (SAXException ex) {
      throw new PalmThingException(ex);
    }
  }

  private static final Attributes NO_ATTRS = new AttributesImpl();

  protected void exportHandler(Collection books, ContentHandler hand) 
      throws SAXException {
    hand.startDocument();
    hand.startElement(null, g_rootElement, g_rootElement, NO_ATTRS);
    
    Iterator iter = books.iterator();
    while (iter.hasNext()) {
      BookRecord book = (BookRecord)iter.next();
      AttributesImpl attrs = new AttributesImpl();
      if (book.getCategoryTag() != null) {
        attrs.addAttribute(null, g_categoryTagAttribute, g_categoryTagAttribute, null,
                           book.getCategoryTag());
      }
      if (book.getId() != BookRecord.RECORD_ID_NONE) {
        attrs.addAttribute(null, g_recordIDAttribute, g_recordIDAttribute, null,
                           Integer.toString(book.getId()));
      }
      if (book.getBookID() != 0) {
        attrs.addAttribute(null, g_bookIDAttribute, g_bookIDAttribute, null,
                           Integer.toString(book.getBookID()));
      }
      hand.startElement(null, g_bookElement, g_bookElement, attrs);
      for (int i = 0; i < g_stringFieldElements.length; i++) {
        String field = book.getStringField(i);
        if (field == null) continue;
        String elem = g_stringFieldElements[i];
        hand.startElement(null, elem, elem, NO_ATTRS);
        hand.characters(field.toCharArray(), 0, field.length());
        hand.endElement(null, elem, elem);
      }      
      hand.endElement(null, g_bookElement, g_bookElement);
    }
    hand.endElement(null, g_rootElement, g_rootElement);
    hand.endDocument();
  }  
}
