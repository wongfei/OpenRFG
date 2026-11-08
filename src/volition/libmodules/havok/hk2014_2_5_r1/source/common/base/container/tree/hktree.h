// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/tree/hktree.h
//
// Hand-written (stub_impl.md S4, serializer; layout from the PDB instance <hkRefPtr<hkXmlParser::Node>>, interface as
// the 2014.1 SDK's hkTree): a tree of values, each node linked to its first child, next sibling and parent. The
// template bodies are in hktree.inl (SF).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

// This port's hkRefPtr has no destructor (hkrefptr.h), so a tree node releases a reference counted value itself, as
// the SDK's implicit ~Node does through ~hkRefPtr.
template<typename T>
inline void hkTree_releaseValue(T&)
{
}

template<typename U>
inline void hkTree_releaseValue(hkRefPtr<U>& value)
{
    value = static_cast<U*>(nullptr);
}

// size 0x4
template<typename T>
class hkTree
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkTree);

    // size 0xC + sizeof(T)
    struct Node
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Node); // <hkRefPtr<Node>> new 0x00F703B0, delete 0x00F70490 (hktree.h:24)
        // <hkRefPtr<Node>> 0x00F708F0 (hktree.h:31)
        Node(const T& value) : m_firstChild(nullptr), m_nextSibling(nullptr), m_parent(nullptr), m_value(value) {}
        // <hkRefPtr<Node>> 0x00F70470
        ~Node() { hkTree_releaseValue(m_value); }

        Node* m_firstChild; // 0x00
        Node* m_nextSibling; // 0x04
        Node* m_parent; // 0x08
        T m_value; // 0x0C
    };

    typedef Node* Iter;

    inline hkTree(); // <hkRefPtr<Node>> 0x00FAA910
    inline ~hkTree(); // <hkRefPtr<Node>> 0x00FAB3F0
    inline void clear(); // <hkRefPtr<Node>> 0x00F70C60
    inline Iter appendSibling(Iter iter, const T& value);
    inline Iter appendChild(Iter iter, const T& value); // <hkRefPtr<Node>> 0x00F70A30
    inline void remove(Iter iter); // <hkRefPtr<Node>> 0x00F70AE0
    inline void setValue(Iter iter, const T& value);
    inline T& getValue(Iter i) const; // <hkRefPtr<Node>> 0x00F701B0
    inline int getDepth(Iter i) const;
    inline int getNumChildren(Iter i) const;
    inline Iter iterGetFirst() const;
    inline Iter iterNext(Iter i) const;
    inline Iter iterParent(Iter i) const; // <hkRefPtr<Node>> 0x00F701C0
    inline Iter iterChildren(Iter i) const;
    inline Iter iterNextPreOrder(Iter i) const;
    inline Iter iterNextPreOrderSkippingChildren(Iter i) const;

private:
    Node* m_root; // 0x00
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/tree/hktree.inl"
