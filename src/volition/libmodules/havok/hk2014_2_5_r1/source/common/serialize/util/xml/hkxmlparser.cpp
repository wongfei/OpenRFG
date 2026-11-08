// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the DOM style XML reader of the XML packfile and object readers,
// from rfg.exe. The file statics taking a buffer are LTCG register-argument functions in rfg.exe (read from the asm).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/istream/hkistream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/reader/hkstreamreader.h"

#include <new>

// 0x00F6F200 (:15): the value of attribute a (a null name matches a null a), or d
const char* hkXmlParser::StartElement::getAttribute(const char* a, const char* d) const
{
    for (int i = 0; i < attributes.getSize(); i++)
    {
        const char* n = attributes[i].name.cString();
        const bool same = (n == nullptr || a == nullptr) ? n == a : hkString::strCmp(n, a) == 0;
        if (same)
        {
            return attributes[i].value.cString();
        }
    }
    return d;
}

// 0x00F6F2B0 (:27): collapses white space (and spaceChars) into single spaces, in a temp buffer
hkResult hkXmlParser::Characters::canonicalize(const char* spaceChars)
{
    hkArray<char,hkContainerTempAllocator> buf;
    buf.setSize(text.getLength() + 1);
    if (hkXmlParser::canonicalize(buf.begin(), text.cString(), spaceChars) == -1)
    {
        return HK_FAILURE;
    }
    text = buf.begin();
    return HK_SUCCESS;
}

// 0x00F6EF00 (:76): in place is allowed (dst <= src). &#N; (up to 9 digits, truncated to a char) and the five
// named entities; -1 for an unterminated or unknown entity. A named entity matches by prefix: the text up to the
// ';' is not compared whole ("&ltx;" reads as '<').
int hkXmlParser::translateEntities(char* dst, const char* src)
{
    static const char* const entities[] = { "<lt", ">gt", "&amp", "\"quot", "'apos", nullptr };
    char* d = dst;
    while (*src != 0)
    {
        const char c = *src;
        const char* next = src + 1;
        if (c != '&')
        {
            *d++ = c;
            src = next;
            continue;
        }
        const char* name = src + 1;
        const char* p = name;
        while (*p != ';')
        {
            if (*p == 0)
            {
                return -1;
            }
            p++;
        }
        next = p + 1;
        if (*name == '#')
        {
            const int n = int(p - src) - 2;
            if (n > 9)
            {
                return -1;
            }
            char digits[12];
            hkString::strNcpy(digits, src + 2, n);
            digits[n] = 0;
            *d++ = (char)hkString::atoi(digits, 0);
        }
        else
        {
            int i = 0;
            for (; entities[i] != nullptr; i++)
            {
                const char* e = entities[i];
                if (hkString::strNcmp(name, e + 1, hkString::strLen(e + 1)) == 0)
                {
                    *d++ = e[0];
                    break;
                }
            }
            if (entities[i] == nullptr)
            {
                return -1;
            }
        }
        src = next;
    }
    *d = 0;
    return int(d - dst);
}

// 0x00F6F010 (:132)
static bool inString(char c, const char* s)
{
    if (s != nullptr)
    {
        for (; *s != 0; s++)
        {
            if (c == *s)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00F6F0D0 (:184)
static hkBool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// 0x00F6F040 (:149): runs of white space (and spaceChars) become one space, leading ones dropped; a trailing space is
// cut, but the returned length still counts it
int hkXmlParser::canonicalize(char* dst, const char* src, const char* spaceChars)
{
    bool inSpace = true;
    char* d = dst;
    if (*src != 0)
    {
        do
        {
            const char c = *src++;
            if (isSpace(c) || inString(c, spaceChars))
            {
                if (!inSpace)
                {
                    *d++ = ' ';
                    inSpace = true;
                }
            }
            else
            {
                inSpace = false;
                *d++ = c;
            }
        } while (*src != 0);
        if (d != dst && inSpace)
        {
            d[-1] = 0;
            return int(d - dst);
        }
    }
    *d = 0;
    return int(d - dst);
}

// 0x00F6F0F0 (:189)
static const char* eatWhite(const char* p)
{
    while (isSpace(*p))
    {
        p++;
    }
    return p;
}

// 0x00F6F3B0 (:198): name="value" pairs (no escapes in values); fails on a missing '=' or quote. LTCG: buf in EAX.
static hkResult extractAttributes(const char* buf, hkArray<hkXmlParser::Attribute,hkContainerHeapAllocator>& attrs)
{
    const char* p = eatWhite(buf);
    while (*p != 0)
    {
        hkXmlParser::Attribute attr;
        const char* q = p;
        for (;;)
        {
            const char c = *q;
            if (isSpace(c) || c == '=')
            {
                break;
            }
            q++;
            if (c == 0)
            {
                return HK_FAILURE;
            }
        }
        attr.name.set(p, int(q - p));
        p = eatWhite(q);
        if (*p != '=')
        {
            return HK_FAILURE;
        }
        p = eatWhite(p + 1);
        if (*p != '"')
        {
            return HK_FAILURE;
        }
        const char* start = p + 1;
        p = start;
        while (*p != '"')
        {
            if (*p == 0)
            {
                return HK_FAILURE;
            }
            p++;
        }
        attr.value.set(start, int(p - start));
        new (&attrs.expandOne()) hkXmlParser::Attribute(attr);
        p = eatWhite(p + 1);
    }
    return HK_SUCCESS;
}

// 0x00F6F4F0 (:246)
hkXmlParser::hkXmlParser()
{
}

// 0x00F6F520 (:250): the nodes put back are deleted, not released
hkXmlParser::~hkXmlParser()
{
    for (int i = 0; i < m_pendingNodes.getSize(); i++)
    {
        delete m_pendingNodes[i];
    }
}

// 0x00F6F5A0 (:259): consumes up to and including the next '>', appending what comes before it to tag (if any).
// buf holds the bufSize bytes peeked at the stream position; it is refilled by peeking after each skip.
// LTCG: bufSize in EAX.
static void eatTag(char* buf, int bufSize, int bufCapacity, hkStreamReader* sb, hkArray<char,hkContainerHeapAllocator>* tag)
{
    for (;;)
    {
        int i = 0;
        while (i < bufSize && buf[i] != '>')
        {
            i++;
        }
        if (tag != nullptr)
        {
            tag->insertAt(tag->getSize(), buf, i);
        }
        if (i != bufSize)
        {
            sb->skip(i + 1);
            return;
        }
        sb->skip(bufSize);
        bufSize = sb->peek(buf, bufCapacity);
        if (bufSize == 0)
        {
            return;
        }
    }
}

// 0x00F6F610 (:288): consumes up to the next '<' (not included), appending it to text. LTCG: bufSize in EAX.
static void eatText(char* buf, int bufSize, int bufCapacity, hkStreamReader* sb, hkArray<char,hkContainerHeapAllocator>& text)
{
    for (;;)
    {
        int i = 0;
        while (i < bufSize && buf[i] != '<')
        {
            i++;
        }
        text.insertAt(text.getSize(), buf, i);
        if (i != bufSize)
        {
            sb->skip(i);
            return;
        }
        sb->skip(bufSize);
        bufSize = sb->peek(buf, bufCapacity);
        if (bufSize == 0)
        {
            return;
        }
    }
}

// 0x00F6F110 (:320): consumes up to and including "-->"; a '-' does not reset a partial match, and the match carries
// over a refill. LTCG: bufSize in EAX, sb in EDI.
static void eatComment(char* buf, int bufSize, int bufCapacity, hkStreamReader* sb)
{
    const char end[] = "-->";
    int matched = 0;
    for (;;)
    {
        int i = 0;
        for (; i < bufSize; i++)
        {
            if (buf[i] == end[matched])
            {
                matched++;
                if (end[matched] == 0)
                {
                    break;
                }
            }
            else if (buf[i] != '-')
            {
                matched = 0;
            }
        }
        if (i != bufSize)
        {
            sb->skip(i + 1);
            return;
        }
        sb->skip(bufSize);
        bufSize = sb->peek(buf, bufCapacity);
        if (bufSize == 0)
        {
            return;
        }
    }
}

// 0x00F6F970 (:361): the next element start, end or text node (a node put back first). Processing instructions
// and comments are skipped; text is gathered up to the next '<' and dropped when it is all white space. A
// self-closing tag gives its start node and puts back an end node.
hkResult hkXmlParser::nextNode(Node** ret, hkStreamReader* reader)
{
    hkIstream is(reader);
    if (m_pendingNodes.getSize() != 0)
    {
        *ret = m_pendingNodes.back();
        m_pendingNodes.popBack();
        return HK_SUCCESS;
    }
    *ret = nullptr;
    if (!is.isOk())
    {
        m_lastError = "End of stream";
        return HK_FAILURE;
    }
    hkArray<char,hkContainerHeapAllocator> text;
    hkStreamReader* sb = is.getStreamReader();
    char buf[32];
    int nbuf = sb->peek(buf, sizeof(buf));
    while (nbuf != 0)
    {
        if (buf[0] != '<')
        {
            eatText(buf, nbuf, sizeof(buf), sb, text);
        }
        else
        {
            if (nbuf < 2)
            {
                break;
            }
            if (buf[1] == '?')
            {
                eatTag(buf, nbuf, sizeof(buf), sb, nullptr);
            }
            else if (nbuf >= 4 && hkString::strNcmp("!--", buf + 1, 3) == 0)
            {
                eatComment(buf, nbuf, sizeof(buf), sb);
            }
            else if (text.getSize() != 0)
            {
                // the text before this tag, unless it is all white space (the tag is read by the next call)
                int i = 0;
                while (i < text.getSize() && isSpace(text[i]))
                {
                    i++;
                }
                if (i == text.getSize())
                {
                    text.setSizeUnchecked(0);
                }
                else
                {
                    text.pushBack(0);
                    // Original bug kept: a failed translation (-1) is not checked and becomes the text length
                    const int len = translateEntities(text.begin(), text.begin());
                    if (len == 1 && text[0] == 0)
                    {
                        *ret = new Characters((const char*)nullptr); // "&#0;" alone
                    }
                    else
                    {
                        *ret = new Characters(text.begin(), len);
                    }
                    return HK_SUCCESS;
                }
            }
            else
            {
                hkArray<char,hkContainerHeapAllocator> tag;
                eatTag(buf, nbuf, sizeof(buf), sb, &tag);
                if (tag.getSize() == 0)
                {
                    m_lastError.printf("Empty tag");
                    return HK_FAILURE;
                }
                tag.pushBack(0);
                int len = translateEntities(tag.begin(), tag.begin());
                if (len == -1)
                {
                    m_lastError.printf("Bad tag");
                    return HK_FAILURE;
                }
                tag.setSizeUnchecked(len);
                if (tag[1] == '/')
                {
                    hkStringBuf name(tag.begin() + 2);
                    *ret = new EndElement(name.cString());
                    return HK_SUCCESS;
                }
                const bool selfClosing = tag[len - 1] == '/';
                if (selfClosing)
                {
                    tag[len - 1] = 0;
                    len--;
                    tag.setSizeUnchecked(len);
                }
                int nameEnd = 0;
                while (nameEnd < len && tag[nameEnd] != ' ')
                {
                    nameEnd++;
                }
                hkArray<Attribute,hkContainerHeapAllocator> attrs;
                if (nameEnd != len)
                {
                    extractAttributes(tag.begin() + nameEnd, attrs); // result not checked
                    tag[nameEnd] = 0;
                    tag.setSizeUnchecked(nameEnd);
                }
                const char* name = tag.begin() + 1;
                StartElement* start = new StartElement(name);
                start->attributes.swap(attrs);
                if (selfClosing)
                {
                    putBack(new EndElement(name));
                }
                *ret = start;
                for (int i = attrs.getSize() - 1; i >= 0; i--)
                {
                    attrs[i].~Attribute();
                }
                return HK_SUCCESS;
            }
        }
        nbuf = sb->peek(buf, sizeof(buf));
    }
    m_lastError.printf("premature end of stream");
    return HK_FAILURE;
}

// 0x00F6F270 (:500)
void hkXmlParser::putBack(Node* node)
{
    m_pendingNodes.pushBack(node);
}

// 0x00F6F680 (:505): reads the nodes up to top's matching end into a tree rooted at top. The tree holds a reference
// to each node; end nodes are deleted.
hkResult hkXmlParser::expandNode(StartElement* top, hkTree<hkRefPtr<Node> >& tree, hkStreamReader* reader)
{
    hkIstream is(reader);
    tree.clear();
    Tree::Iter cur;
    {
        hkRefPtr<Node> ref(top);
        cur = tree.appendChild(nullptr, ref);
        ref = static_cast<Node*>(nullptr);
    }
    Node* node = nullptr;
    while (nextNode(&node, reader) == HK_SUCCESS)
    {
        if (node == nullptr)
        {
            return HK_SUCCESS;
        }
        switch (node->type)
        {
            case START_ELEMENT:
            case CHARACTERS:
            {
                // the tree takes over the parser's reference
                hkRefPtr<Node> ref(node);
                Tree::Iter child = tree.appendChild(cur, ref);
                node->removeReference();
                ref = static_cast<Node*>(nullptr);
                if (child->m_value->type == START_ELEMENT)
                {
                    cur = child;
                }
                node = nullptr;
                break;
            }
            case END_ELEMENT:
            {
                EndElement* end = static_cast<EndElement*>(node);
                StartElement* start = tree.getValue(cur)->asStart();
                if (start == nullptr)
                {
                    m_lastError.printf("Unexpected end tag '%s'", end->name.cString());
                    return HK_FAILURE;
                }
                const char* a = start->name.cString();
                const char* b = end->name.cString();
                const bool same = (a == nullptr || b == nullptr) ? a == b : hkString::strCmp(a, b) == 0;
                if (!same)
                {
                    m_lastError.printf("Expected tag to end '%s' but got '%s'", a, b);
                    return HK_FAILURE;
                }
                cur = tree.iterParent(cur);
                delete node;
                if (cur == nullptr)
                {
                    return HK_SUCCESS;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
    if (cur != nullptr)
    {
        // Original bug kept: asStart() is not checked (the open node is always a start element here)
        m_lastError.printf("Missing closing tag of '%s'", tree.getValue(cur)->asStart()->name.cString());
    }
    return HK_FAILURE;
}

// 0x00F6F180 (:566): the document's first element and everything in it; a first node of another kind leaks
hkResult hkXmlParser::parse(hkTree<hkRefPtr<Node> >& tree, hkStreamReader* reader)
{
    Node* node;
    if (nextNode(&node, reader) == HK_SUCCESS)
    {
        if (node == nullptr)
        {
            return HK_SUCCESS;
        }
        if (node->type == START_ELEMENT)
        {
            return expandNode(static_cast<StartElement*>(node), tree, reader);
        }
        m_lastError.printf("Document does not start with an element.");
    }
    return HK_FAILURE;
}

// 0x00F6F260 (:578)
const char* hkXmlParser::getLastError() const
{
    return m_lastError.cString();
}
