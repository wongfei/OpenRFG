// Original: library/modules/runtime/resources/include/keen/resources/resourcerequest.hpp
// Implementation: library/modules/runtime/resources/sources/resourcerequest.cpp
//
// ResourceRequest, an asynchronous request to load a set of resources (state and load flags).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/internallist.hpp"

namespace keen
{
    class Resource;

    // size 0x9C
    class ResourceRequest : public InternalListBase::Listable
    {
    public:
        enum State
        {
            State_Idle = 0,
            State_Loading = 1,
            State_Complete = 2,
        };

        enum LoadFlags
        {
            LoadFlags_Optional = 1,
            LoadFlags_Reload = 2,
            LoadFlags_HighPriorityHack = 4,
        };

        enum
        {
            ResourceNameCapacity = 128,
        };

        ResourceRequest(); // 0x00CDE6E0 (:8)
        bool openIntern(const char* pName, uint32 resourceContextIndex, uint32 type, uint32 flags, uint8 resourceTag); // 0x00CDCEE0 (:25)
        void* closeIntern(uint32 resourceType); // 0x00CDCF50 (:42)
        State getState() const; // 0x00CDCAE0 (.h:50)
        bool isIdle() const; // 0x00CDCAF0 (.h:51)
        bool isLoading() const; // 0x00CDCB00 (.h:52)
        bool isComplete() const;
        bool isAborted() const; // 0x00CDCB10 (.h:54)
        bool hasResourceName() const;
    private:
        void clear(); // 0x00CDCEB0 (:13)
        void setState(State state); // 0x00CDCB20 (.h:63)
        void setResource(Resource* pResource); // 0x00CDCB30 (.h:64)
        void setAbortLoading(bool abort); // 0x00CDCB40 (.h:65)
        const char* getResourceName() const; // 0x00CDCB50 (.h:67)
        uint32 getResourceType() const; // 0x00CDCB60 (.h:68)
        uint32 getResourceContextId() const; // 0x00CDCB70 (.h:69)
        bool* getAbortLoading(); // 0x00CDCB80 (.h:70)
        bool getReloadFlag() const; // 0x00CDCB90 (.h:71)
        bool isOptional() const; // 0x00CDCBA0 (.h:72)

        State m_state; // 0x08
        uint32 m_resourceContextIndex; // 0x0C
        char m_resourceName[128]; // 0x10
        uint32 m_resourceType; // 0x90
        Resource* m_pResource; // 0x94
        uint8 m_resourceTag; // 0x98
        bool m_abortLoading; // 0x99
        uint8 m_loadFlags; // 0x9A
    };
}
