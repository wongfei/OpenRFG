// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the hkSerializeMultiMap template functions (included by
// hkserializemultimap.h), as the 2014.1 SDK's hkSerializeMultiMap.inl. Instances: <void const*, int> 0x00F6A910..,
// the data world tracker's <hkDataClassDict*, ...> (SC), the copier's <hkDataObject_Handle, PointerPatch> (SD).
#pragma once

// :9 (<void const*,int> 0x00F6A910)
// A slot of the free chain, else a new one at the end.
template<typename K, typename V, typename IndexMap>
int hkSerializeMultiMap<K, V, IndexMap>::getFreeIndex()
{
    int i;
    if (m_freeChainStart != -1)
    {
        i = m_freeChainStart;
        m_freeChainStart = m_valueChain[m_freeChainStart].next;
    }
    else
    {
        i = m_valueChain.getSize();
        m_valueChain.expandOne();
    }
    return i;
}

// :26 (<void const*,int> 0x00FB0EC0)
template<typename K, typename V, typename IndexMap>
int hkSerializeMultiMap<K, V, IndexMap>::getFirstIndex(K k) const
{
    return m_indexMap.getWithDefault(k, -1);
}

// :32 (<void const*,int> 0x00F6A9E0)
// The new value becomes the head of the key's chain.
template<typename K, typename V, typename IndexMap>
hkBool32 hkSerializeMultiMap<K, V, IndexMap>::insert(K k, const V& value)
{
    const int chainIndex = m_indexMap.getWithDefault(k, -1);
    const int thisIndex = getFreeIndex();
    Value& ref = m_valueChain[thisIndex];
    ref.value = value;
    ref.next = chainIndex;
    return m_indexMap.insert(k, thisIndex);
}

// :43
template<typename K, typename V, typename IndexMap>
int hkSerializeMultiMap<K, V, IndexMap>::addPendingValue(const V& value, int nextIndex)
{
    const int thisIndex = getFreeIndex();
    Value& v = m_valueChain[thisIndex];
    v.value = value;
    v.next = nextIndex;
    return thisIndex;
}

// :53
template<typename K, typename V, typename IndexMap>
void hkSerializeMultiMap<K, V, IndexMap>::realizePendingKey(K k, int index)
{
    m_indexMap.insert(k, index);
}

// :61
template<typename K, typename V, typename IndexMap>
int hkSerializeMultiMap<K, V, IndexMap>::getNumKeys() const
{
    return m_indexMap.getSize();
}

// :67
template<typename K, typename V, typename IndexMap>
void hkSerializeMultiMap<K, V, IndexMap>::clear()
{
    m_valueChain.clear();
    m_indexMap.clear();
    m_freeChainStart = -1;
}

// :75 (<void const*,int> 0x00FB0EE0)
// Unlinks without a parent search: the next element is copied over the removed one and freed instead; at the tail,
// the head's value moves to the removed slot and the head is freed. Returns the index that now follows (or holds the
// next value).
template<typename K, typename V, typename IndexMap>
int hkSerializeMultiMap<K, V, IndexMap>::removeByIndex(K key, int removeIndex)
{
    int nextIndex = m_valueChain[removeIndex].next;
    int freeIndex = nextIndex;
    if (nextIndex != -1)
    {
        m_valueChain[removeIndex] = m_valueChain[nextIndex];
        nextIndex = removeIndex;
    }
    else
    {
        typename IndexMap::Iterator it = m_indexMap.findKey(key);
        const int headIndex = m_indexMap.getValue(it);
        if (headIndex == removeIndex)
        {
            // the only element: the key stays with an empty chain
            m_indexMap.setValue(it, -1);
            freeIndex = removeIndex;
            removeIndex = -1;
        }
        else if (m_valueChain[headIndex].next == removeIndex)
        {
            m_valueChain[headIndex].next = -1;
            freeIndex = removeIndex;
        }
        else
        {
            m_indexMap.setValue(it, m_valueChain[headIndex].next);
            m_valueChain[removeIndex].value = m_valueChain[headIndex].value;
            freeIndex = headIndex;
        }
    }
    m_valueChain[freeIndex].next = m_freeChainStart;
    m_freeChainStart = freeIndex;
    return nextIndex;
}

// :130
template<typename K, typename V, typename IndexMap>
void hkSerializeMultiMap<K, V, IndexMap>::removeByValue(K key, const V& v)
{
    for (int i = getFirstIndex(key); i != -1; i = getNextIndex(i))
    {
        if (getValue(i) == v)
        {
            removeByIndex(key, i);
            return;
        }
    }
}

// :146
// The whole chain goes to the free chain.
template<typename K, typename V, typename IndexMap>
void hkSerializeMultiMap<K, V, IndexMap>::removeKey(K key)
{
    typename IndexMap::Iterator it = m_indexMap.findKey(key);
    const int start = m_indexMap.getValue(it);
    m_indexMap.removeAtIterator(it);
    if (start != -1)
    {
        int end = start;
        while (m_valueChain[end].next != -1)
        {
            end = m_valueChain[end].next;
        }
        m_valueChain[end].next = m_freeChainStart;
        m_freeChainStart = start;
    }
}

// :165
template<typename K, typename V, typename IndexMap>
void hkSerializeMultiMap<K, V, IndexMap>::changeKey(K oldKey, K newKey)
{
    typename IndexMap::Iterator it = m_indexMap.findKey(oldKey);
    const int start = m_indexMap.getValue(it);
    m_indexMap.removeAtIterator(it);
    m_indexMap.insert(newKey, start);
}
