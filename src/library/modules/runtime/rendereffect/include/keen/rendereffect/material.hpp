// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/material.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/material.cpp
//
// Material, the material resource (a Resource holding the effect's material data).
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    namespace MaterialFormat
    {
        struct MaterialInitializationData;
    }
}

namespace keen
{
    struct BaseMaterialData;
    struct RenderEffectSystem;

    // size 0xC
    class Material : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x5254414D,
        };

        const BaseMaterialData* getData() const;
    private:
        Material(const Material&);
    protected:
        Material(uint32 key); // 0x00CC8540 (:26)
        ~Material(); // 0x00CC8560 (:31)
        bool initialize(const MaterialFormat::MaterialInitializationData* pInitializationData, uint32 initializationSize, uint32 contentFormatKey, const RenderEffectSystem* pRenderEffectSystem); // 0x00CC8570 (:36)
        void shutdown(); // 0x00CC8590 (:78)
    private:
        Material& operator=(const Material&);

        const BaseMaterialData* m_pData; // 0x08
    };
}
