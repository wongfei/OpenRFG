// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); Node moved before the types
// derived from it by hand; inline members as in the 2014.1 SDK, checked against rfg.exe (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/tree/hktree.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

class hkStreamReader;

// size 0x18: a small DOM style XML reader over a stream (the XML packfile and object readers)
class hkXmlParser : public hkReferencedObject
{
public:
    enum NodeType
    {
        INVALID = 0,
        START_ELEMENT = 1,
        END_ELEMENT = 2,
        CHARACTERS = 3,
        MAX_ID = 4,
    };

    struct StartElement;
    struct EndElement;
    struct Characters;

    // size 0xC
    struct Node : public hkReferencedObject
    {
        HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F70540
        // 0x00F702A0 (hkxmlparser.h:55)
        Node(NodeType t) : type(t) {}
        inline StartElement* asStart(); // 0x00F53300
        inline EndElement* asEnd(); // 0x00FA86D0
        inline Characters* asCharacters(); // 0x00FA86E0
        virtual ~Node() {} // vtbl+0x00 0x00F702C0

        NodeType type; // 0x08
    };

    typedef hkTree<hkRefPtr<Node> > Tree;

    // size 0x8
    struct Attribute
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Attribute); // delete(void* p, unsigned int nbytes) 0x00F70510
        Attribute() {} // 0x00F70090
        ~Attribute() {} // 0x00F700B0

        hkStringPtr name; // 0x00
        hkStringPtr value; // 0x04
    };

    // size 0x1C
    struct StartElement : public Node
    {
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F70580, delete(void* p, unsigned int nbytes) 0x00F70E90
        // 0x00F70E50 (hkxmlparser.h:71)
        StartElement(const char* n) : Node(START_ELEMENT), name(n) {}
        const char* getAttribute(const char* a, const char* d) const; // 0x00F6F200
        // 0x00F70ED0: hkArray does not destroy its elements in this port (rfg.exe destroys them last first)
        virtual ~StartElement()
        {
            for (int i = attributes.getSize() - 1; i >= 0; i--)
            {
                attributes[i].~Attribute();
            }
        }

        hkStringPtr name; // 0x0C
        hkArray<Attribute,hkContainerHeapAllocator> attributes; // 0x10
    };

    // size 0x10
    struct EndElement : public Node
    {
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F705A0, delete(void* p, unsigned int nbytes) 0x00F70610
        // 0x00F705C0 (hkxmlparser.h:83)
        EndElement(const char* n) : Node(END_ELEMENT), name(n) {}
        virtual ~EndElement() {} // vtbl+0x00 0x00F705F0

        hkStringPtr name; // 0x0C
    };

    // size 0x10
    struct Characters : public Node
    {
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F70650, delete(void* p, unsigned int nbytes) 0x00F706C0
        // 0x00F70700 (hkxmlparser.h:92)
        Characters(const char* s, int len) : Node(CHARACTERS), text(s, len) {}
        // 0x00F70670 (hkxmlparser.h:91)
        Characters(const char* t) : Node(CHARACTERS), text(t) {}
        hkResult canonicalize(const char* spaceChars); // 0x00F6F2B0
        virtual ~Characters() {} // vtbl+0x00 0x00F706A0

        hkStringPtr text; // 0x0C
    };


    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x01057090, delete(void* p, unsigned int nbytes) 0x00F70C00
    hkXmlParser(); // 0x00F6F4F0
    virtual ~hkXmlParser(); // vtbl+0x00 0x00F6F520
    virtual hkResult nextNode(Node** ret, hkStreamReader* reader); // vtbl+0x10 0x00F6F970
    virtual void putBack(Node* node); // vtbl+0x14 0x00F6F270
    virtual hkResult expandNode(StartElement* top, hkTree<hkRefPtr<Node> >& tree, hkStreamReader* reader); // vtbl+0x18 0x00F6F680
    hkResult parse(hkTree<hkRefPtr<Node> >& tree, hkStreamReader* reader); // 0x00F6F180
    static int translateEntities(char* dst, const char* src); // 0x00F6EF00
    static int canonicalize(char* dst, const char* src, const char* spaceChars); // 0x00F6F040
    const char* getLastError() const; // 0x00F6F260

public:
    hkArray<Node*,hkContainerHeapAllocator> m_pendingNodes; // 0x08
    hkStringPtr m_lastError; // 0x14
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkxmlparser.inl"
