<?xml version="1.0" encoding="UTF-8"?>
<!-- $Header$ -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:fo="http://www.w3.org/1999/XSL/Format">

  <xsl:output method="xml" encoding="utf-8" omit-xml-declaration="yes"/>

  <xsl:template match="/">
    <html>
      <head>
        <meta http-equiv="Content-Type" content="application/xhtml+xml; charset=utf-8"/>
        <meta http-equiv="Content-Language" content="en-us"/>
        <title>Book Links</title>
        <style>
table.books {
  border-width: 1px;
  border-spacing: 2px;
  border-style: outset;
  border-color: gray;
  border-collapse: separate;
  background-color: white;
}
table.books th {
  border-width: 1px;
  padding: 1px;
  border-style: inset;
  border-color: gray;
  background-color: #ddecfe;
}
table.books td {
  border-width: 1px;
  padding: 1px;
  border-style: inset;
  border-color: gray;
  background-color: #ddecfe;
}
       </style>
      </head>
      <body>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>
  <xsl:template match="books">
    <table class="books">
      <tr>
        <th>Author</th>
        <th>Title</th>
        <th>Tags</th>
        <th>Link</th>
      </tr>
      <xsl:apply-templates/>
    </table>
  </xsl:template>
  <xsl:template match="book">
    <tr>
      <td><xsl:apply-templates select="author"/></td>
      <td><xsl:apply-templates select="title"/></td>
      <td><xsl:apply-templates select="tags"/></td>
      <td><a href="{link/@url}">
        <xsl:choose>
          <xsl:when test="@book-id">Edit book</xsl:when>
          <xsl:otherwise>Add book</xsl:otherwise>
        </xsl:choose>
      </a></td>
    </tr>
  </xsl:template>
</xsl:stylesheet>
