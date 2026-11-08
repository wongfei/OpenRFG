// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the singleton registration list (hkSingletonInitNode).
// Not reimplemented: populate, depopulate and findByName, which share singletons between DLLs (the game links
// Havok statically).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"

// ---- globals
hkSingletonInitNode* hkSingletonInitNode::m_head; // 0x03A27004

// 0x00CE6230 (hksingleton.h)
// Static initialization: the node goes to the front of the list.
hkSingletonInitNode::hkSingletonInitNode(const char* name, hkReferencedObject* (*func)(), void** ptr)
{
    m_name = name;
    m_createFunc = func;
    m_next = m_head;
    m_head = this;
    m_value = ptr;
}
