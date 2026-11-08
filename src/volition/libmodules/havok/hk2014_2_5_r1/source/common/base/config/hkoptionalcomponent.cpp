// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK5): the optional component list, from rfg.exe. writeReport (S phase) is not
// reimplemented.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"

// 0x03A27000 (constant-initialized: the components' dynamic initializers prepend to it in any order)
hkOptionalComponent* hkOptionalComponent::s_components = nullptr;

// 0x00CF2810 (:22)
hkOptionalComponent::hkOptionalComponent(const char* name, void (*onLink)())
{
    m_next = s_components;
    m_onLink = onLink;
    m_name = name;
    m_funcPtr = nullptr;
    m_func = nullptr;
    m_isUsed.m_bool = 0;
    m_isRequested.m_bool = 0;
    s_components = this;
}

// 0x00CF2840 (:34)
hkOptionalComponent::hkOptionalComponent(const char* name, void** funcPtr, void* func)
{
    m_name = name;
    m_next = s_components;
    m_funcPtr = funcPtr;
    m_onLink = nullptr;
    m_func = func;
    m_isUsed.m_bool = 0;
    m_isRequested.m_bool = 0;
    s_components = this;
}

// 0x00CF2800 (:39)
const hkOptionalComponent* hkOptionalComponent::getFirstComponent()
{
    return s_components;
}

// 0x00CF2870 (:44)
void hkOptionalComponent::request()
{
    m_isRequested.m_bool = 1;
    if (m_onLink != nullptr)
    {
        m_onLink();
    }
    if (m_funcPtr != nullptr)
    {
        *m_funcPtr = m_func;
    }
}
