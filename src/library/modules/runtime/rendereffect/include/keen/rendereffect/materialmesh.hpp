// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/materialmesh.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/materialmesh.cpp
//
// MaterialMesh, one sub-mesh of a model: geometry plus material handle.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"

namespace keen
{
    namespace ModelFormat
    {
        struct MaterialMeshInitializationData;
    }
}

namespace keen
{
    struct GraphicsSystem;
    struct MaterialHandleType;
    struct VertexFormat;

    // size 0x34
    class MaterialMesh
    {
    public:
        MaterialMesh(); // 0x00CC8680 (:56)
        ~MaterialMesh(); // 0x00CC8690 (:61)
        bool create(GraphicsSystem* pGraphicsSystem, const ModelFormat::MaterialMeshInitializationData& initializationData, const char* pDebugName); // 0x00CC86A0 (:66)
        void destroy(GraphicsSystem* pGraphicsSystem); // 0x00CC8790 (:105)
        const MaterialHandleType* getMaterial() const;
        const RenderGeometry& getRenderGeometry() const; // 0x00CC8620 (.h:39)
        const VertexFormat* getVertexFormat() const;
        void setSkinningBufferOffset(uint32 offset); // 0x00CC87D0 (:116)

    private:
        RenderGeometry m_geometry; // 0x00
        const MaterialHandleType* m_material; // 0x2C
        const VertexFormat* m_pVertexFormat; // 0x30
    };
}
