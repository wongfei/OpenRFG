// Original: library/modules/runtime/resources/include/keen/resources/resource.hpp
// Implementation: library/modules/runtime/resources/sources/global/resource.cpp
//
// Resource, the base of every resource managed by the keen resource system (key, tag, reference count and load
// flags). The resource manager is in resourcemanager.hpp (misc_classes.md M1).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct ResourceFindDebugDataResult;

    // size 0x8
    class Resource
    {
    public:
        enum
        {
            DefaultTag = 254,
            InvalidTag = 255,
        };

        enum
        {
            Flags_FullyLoaded = 32768,
            Flags_FileMissing = 16384,
            Flags_Shift_ReferenceCount = 0,
            Flags_Mask_ReferenceCount = 8191,
        };

        uint32 getKey() const; // 0x00CDC3A0 (.h:82)
        bool isFileMissing() const;
        bool isFullyLoaded() const; // 0x00CDC3B0 (.h:85)
        const char* getFilename() const; // 0x00CDCBF0 (:74)
        uint32 getReferenceCount() const; // 0x00CDC3C0 (.h:89)
        void addReference() const; // 0x00CDC3D0 (.h:90)
        void releaseReference() const; // 0x00CDCBE0 (:53)
        uint8 getTag() const; // 0x00C32850 (.h:93)
        bool findDebugData(ResourceFindDebugDataResult* pResult, uint32 key) const; // 0x00CDCC00 (:93)
    protected:
        Resource(uint32 key); // 0x00CDCBB0 (:26)
        ~Resource(); // 0x00CDCBD0 (:42)
        void setFileMissing(bool);
        void setFullyLoaded(bool set); // 0x00CDC3F0 (.h:126)

        uint16 m_flags; // 0x00
        uint8 m_tag; // 0x02
        uint8 m_unused; // 0x03
    private:
        uint32 m_key; // 0x04
    };
}
