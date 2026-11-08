// Original: library/modules/runtime/resources/sources/global/resourcefactory.cpp
//
// Partial (SFX.md SX1): ResourceFactory's own members. Every factory links itself into a global registry when
// constructed; closeResourceFactoryRegistry (0x00CDE470), the resource manager's side, is not ported.

#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    bool ResourceFactory::s_resourceFactoryRegistryOpen = true; // 0x016F2784
    ResourceFactory* ResourceFactory::s_pFirstResourceFactory = nullptr; // 0x03A260B4

    // 0x00CDCC70 (:33)
    ResourceFactory::ResourceFactory()
    {
        m_pNextRegisteredFactory = s_pFirstResourceFactory;
        s_pFirstResourceFactory = this;
    }

    // 0x00CDCC90 (:44)
    ResourceFactory::~ResourceFactory()
    {
    }

    // 0x00CDCCA0 (:49)
    void ResourceFactory::create()
    {
    }

    // 0x00CDCCB0 (:57)
    void ResourceFactory::destroy()
    {
    }

    // 0x00C32B40 (resourcefactory.hpp:41)
    bool ResourceFactory::unlockAllResources(uint32 contextId)
    {
        (void)contextId;
        return true;
    }

    // 0x00CDCCC0 (:65)
    void ResourceFactory::postInitializeResource(Resource* pResource)
    {
        (void)pResource;
    }

    // 0x00C32B50 (resourcefactory.hpp:48)
    bool ResourceFactory::createEmergencyResource(Resource* pResource, const char* pFileName)
    {
        (void)pResource;
        (void)pFileName;
        return false;
    }
}
