// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/tree/hktree.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SF): the 2014.1 SDK's template code, checked against the
// <hkRefPtr<hkXmlParser::Node>> instance in rfg.exe (the asserts are compiled out).

// <hkRefPtr<Node>> 0x00FAA910 (:12)
template<typename T>
inline hkTree<T>::hkTree()
    : m_root(nullptr)
{
}

// <hkRefPtr<Node>> 0x00FAB3F0 (:17)
template<typename T>
inline hkTree<T>::~hkTree()
{
    clear();
}

// <hkRefPtr<Node>> 0x00F70C60 (:23)
template<typename T>
inline void hkTree<T>::clear()
{
    if (m_root)
    {
        remove(m_root);
        m_root = nullptr;
    }
}

// <hkRefPtr<Node>> 0x00F70A30 (:33): the last child of iter, or the last root for a null iter
template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::appendChild(Iter iter, const T& value)
{
    Node* newNode = new Node(value);
    newNode->m_parent = iter;
    if (iter != nullptr)
    {
        if (Node* n = iter->m_firstChild)
        {
            while (Node* m = n->m_nextSibling)
            {
                n = m;
            }
            n->m_nextSibling = newNode;
        }
        else
        {
            iter->m_firstChild = newNode;
        }
    }
    else if (Node* c = m_root)
    {
        while (Node* n = c->m_nextSibling)
        {
            c = n;
        }
        c->m_nextSibling = newNode;
    }
    else
    {
        m_root = newNode;
    }
    return newNode;
}

// No instance in rfg.exe; the SDK default-constructs the node and assigns the value (Node has no default
// constructor here).
template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::appendSibling(Iter iter, const T& value)
{
    Node* newNode = new Node(value);
    newNode->m_parent = iter->m_parent;
    newNode->m_nextSibling = iter->m_nextSibling;
    iter->m_nextSibling = newNode;
    return newNode;
}

// <hkRefPtr<Node>> 0x00F70AE0 (:82): unhooks iter, then frees its subtree leaves first. Original bug kept (the SDK's):
// the walk also follows iter's next siblings, so they are freed with it (and stay linked from the previous sibling).
// rfg.exe only removes whole trees (clear), where that frees every root as intended.
template<typename T>
inline void hkTree<T>::remove(Iter iter)
{
    if (iter->m_parent)
    {
        Node* prev = iter->m_parent->m_firstChild;
        if (prev == iter)
        {
            iter->m_parent->m_firstChild = nullptr;
        }
        else
        {
            while (prev->m_nextSibling != iter)
            {
                prev = prev->m_nextSibling;
            }
            prev->m_nextSibling = iter->m_nextSibling;
        }
        iter->m_parent = nullptr;
    }
    Iter cur = iter;
    while (cur)
    {
        while (cur->m_firstChild)
        {
            cur = cur->m_firstChild;
        }
        Iter next = cur->m_nextSibling ? cur->m_nextSibling : cur->m_parent;
        if (Node* p = cur->m_parent)
        {
            p->m_firstChild = nullptr;
        }
        delete cur;
        cur = next;
    }
}

template<typename T>
inline void hkTree<T>::setValue(Iter i, const T& t)
{
    i->m_value = t;
}

// <hkRefPtr<Node>> 0x00F701B0 (:131)
template<typename T>
inline T& hkTree<T>::getValue(Iter i) const
{
    return i->m_value;
}

template<typename T>
inline int hkTree<T>::getDepth(Iter i) const
{
    int d = -1;
    Node* n = i;
    while (n)
    {
        n = n->m_parent;
        d += 1;
    }
    return d;
}

template<typename T>
inline int hkTree<T>::getNumChildren(Iter i) const
{
    int c = 0;
    Node* n = i->m_firstChild;
    while (n)
    {
        n = n->m_nextSibling;
        c += 1;
    }
    return c;
}

template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::iterGetFirst() const
{
    return const_cast<Iter>(m_root);
}

template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::iterNext(Iter i) const
{
    return i->m_nextSibling;
}

// <hkRefPtr<Node>> 0x00F701C0 (:179)
template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::iterParent(Iter i) const
{
    return i->m_parent;
}

template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::iterChildren(Iter i) const
{
    return i ? i->m_firstChild : m_root;
}

template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::iterNextPreOrder(Iter i) const
{
    if (i->m_firstChild)
    {
        return i->m_firstChild;
    }
    while (i->m_nextSibling == nullptr)
    {
        i = i->m_parent;
        if (i == nullptr)
        {
            return nullptr;
        }
    }
    return i->m_nextSibling;
}

template<typename T>
inline typename hkTree<T>::Iter hkTree<T>::iterNextPreOrderSkippingChildren(Iter i) const
{
    while (i->m_nextSibling == nullptr)
    {
        i = i->m_parent;
        if (i == nullptr)
        {
            return nullptr;
        }
    }
    return i->m_nextSibling;
}
