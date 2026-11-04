/*
 * 065-xml.h (issue 702) - XML read one piece at a time: the pull-parser
 *
 * For a general audience: the game's interface, and every addon's, is
 * written partly in XML: nested tags such as <Frame name="Foo"> ...
 * </Frame> saying which frames exist and how they look. This reads such a
 * file from its bytes and hands its pieces over one at a time, in order:
 * a tag opening (with its name, its attributes and the line it is on), a
 * run of text, a tag closing, the end. Whoever asks builds what it wants
 * from them (066-framexml builds the interface's element tree). It is the
 * client's own, with no XML library (docs/006, "XML parsing").
 *
 * What it reads: the part of XML 1.0 the interface's files use. Elements,
 * attributes in single or double quotes, text, comments, CDATA sections,
 * the five named entities (&amp; &lt; &gt; &quot; &apos;) and numbered
 * ones (&#65; &#x41;, written out as UTF-8), a byte-order mark and the
 * <?xml ...?> declaration at the start (skipped, as are other <? ?>), a
 * <!DOCTYPE ...> with no declarations of its own (skipped). Names may
 * hold a colon (xmlns:xsi, xsi:schemaLocation are ordinary attributes:
 * no namespaces). Line ends (\r\n, \r) become \n in text and values, and
 * a value's tabs and line ends become spaces, as XML says.
 *
 * What it refuses, with W_ERR_CORRUPT and a sentence naming the line and
 * column: a tag closed by another's name, the file ending inside an
 * element, a second top element or none, text other than spaces outside
 * the top element, an attribute written twice or without quotes, a < in
 * a value, an & that doesn't begin a known entity, a comment, CDATA,
 * <? ?> or tag never ended, a zero byte, elements nested deeper than
 * XML_MAX_DEPTH. A DOCTYPE with declarations of its own, and a file in
 * UTF-16, are W_ERR_UNSUPPORTED. Where XML 1.0 is stricter than this
 * without the meaning changing (a "--" inside a comment, a "]]>" in text,
 * control bytes other than zero) it is let through: an addon the
 * original loads must load here.
 *
 * An XmlEvent, field by field (its strings belong to the reader and last
 * until the next call):
 *   kind      XML_START, XML_END, XML_TEXT, or XML_DONE at the end
 *   name      START, END: the element's name
 *   attrs     START: nattrs name and value pairs, in the order written,
 *             values with their entities read
 *   empty     START: written <a/>; its END is the next event
 *   text      TEXT: text_len bytes (zero bytes can't occur), entities
 *             read; a CDATA section's text exactly as written
 *   cdata     TEXT: 1 when it came from a CDATA section
 *   line      where it begins, counted from 1; column likewise, in bytes
 *   depth     START: how many elements it is inside (the top one: 0)
 */
#ifndef W_XML_H
#define W_XML_H

#include "013-errors.h"

#include <stddef.h>

#define XML_MAX_DEPTH 1024                /* elements inside elements; deeper is refused */

typedef enum {
    XML_START = 1,
    XML_END,
    XML_TEXT,
    XML_DONE,
} XmlKind;

typedef struct {
    const char *name;
    const char *value;
} XmlAttr;

typedef struct {
    XmlKind kind;
    const char *name;
    const XmlAttr *attrs;
    int nattrs;
    int empty;
    const char *text;
    size_t text_len;
    int cdata;
    int line, column;
    int depth;
} XmlEvent;

/* A reader over `len` bytes at `data` (the handle: docs/001, "Handles").
 * The bytes are not copied: they must stay until xml_reader_free. NULL
 * when there is no memory, with `why` filled. */
typedef struct XmlReader XmlReader;
XmlReader *xml_reader_new(const char *data, size_t len, WWhy *why);
void       xml_reader_free(XmlReader *x);

/* The next piece into `ev`. W_OK (and XML_DONE once the top element has
 * closed and only spaces, comments or <? ?> follow); otherwise the
 * file's fault, as above, in `why`, and every later call gives the same.
 * W_ERR_FULL: no memory. */
WErr xml_next(XmlReader *x, XmlEvent *ev, WWhy *why);

/* The attribute's value by name (exactly as written), or NULL. */
const char *xml_attr(const XmlEvent *ev, const char *name);

#endif
