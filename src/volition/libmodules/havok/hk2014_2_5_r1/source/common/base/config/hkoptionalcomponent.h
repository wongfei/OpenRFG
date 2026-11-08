// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h
//
// Havok 2014.2.5 (havok_impl.md HK5): a system linked through a function pointer. Its definition (a global constructed
// by a dynamic initializer) registers it in a list; hkProductFeatures::initialize requests the systems the game uses,
// which assigns their function pointers or runs their registration callbacks. The PDB declaration with the macros of
// Havok 2014.1 (hkOptionalComponent.h). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkOstream;

// size 0x18
class hkOptionalComponent
{
public:
    HK_DECLARE_PLACEMENT_ALLOCATOR();

    typedef void (*OnRequestFunction)();

    hkOptionalComponent(const char* name, void (*onLink)()); // 0x00CF2810 (hkoptionalcomponent.cpp:22)
    hkOptionalComponent(const char* name, void** funcPtr, void* func); // 0x00CF2840 (hkoptionalcomponent.cpp:34)
    static const hkOptionalComponent* getFirstComponent(); // 0x00CF2800 (hkoptionalcomponent.cpp:39)
    static void writeReport(hkOstream& os); // 0x00CF2890 (hkoptionalcomponent.cpp:83): S phase, not reimplemented
    void request(); // 0x00CF2870 (hkoptionalcomponent.cpp:44)

    // 0x00D83F30 (.h:72)
    void markUsed()
    {
        m_isUsed.m_bool = 1;
    }

    // 0x00CF27C0 (.h:75)
    const char* getName() const
    {
        return m_name;
    }

    // 0x00CF27D0 (.h:78)
    hkBool isUsed() const
    {
        return m_isUsed;
    }

    // 0x00CF27E0 (.h:82)
    hkBool wasRequested() const
    {
        return m_isRequested;
    }

    // 0x00CF27F0
    const hkOptionalComponent* getNext() const
    {
        return m_next;
    }

protected:
    hkOptionalComponent* m_next; // 0x00
    const char* m_name; // 0x04
    void (*m_onLink)(); // 0x08
    void** m_funcPtr; // 0x0C
    void* m_func; // 0x10
    hkBool m_isUsed; // 0x14
    hkBool m_isRequested; // 0x15

    static hkOptionalComponent* s_components; // the list head (the last component constructed)
};

// Defines the component NAME, which assigns FUNC_ADDR to FUNC_PTR when it is requested.
#define HK_OPTIONAL_COMPONENT_DEFINE(NAME, FUNC_PTR, FUNC_ADDR) \
    hkOptionalComponent hkOptionalComponent_##NAME(#NAME, (void**)&FUNC_PTR, (void*)FUNC_ADDR)

// Defines the component NAME, which runs FUNC when it is requested.
#define HK_OPTIONAL_COMPONENT_DEFINE_MANUAL(NAME, FUNC) \
    hkOptionalComponent hkOptionalComponent_##NAME(#NAME, FUNC)

// Marks the component NAME as used (called by its implementation).
#define HK_OPTIONAL_COMPONENT_MARK_USED(NAME)                     \
    do                                                            \
    {                                                             \
        extern hkOptionalComponent hkOptionalComponent_##NAME;    \
        hkOptionalComponent_##NAME.markUsed();                    \
    } while (0)

// Requests the component NAME (hkProductFeatures::initialize).
#define HK_OPTIONAL_COMPONENT_REQUEST(NAME)                       \
    do                                                            \
    {                                                             \
        extern hkOptionalComponent hkOptionalComponent_##NAME;    \
        hkOptionalComponent_##NAME.request();                     \
    } while (0)
