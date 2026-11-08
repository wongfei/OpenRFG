// Original: library/modules/runtime/graphics/sources/global/rendergeometry.cpp

#include "keen/graphics/rendergeometry.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"

namespace keen
{
    namespace graphics
    {
        // 0x00C5DCD0 (:8)
        bool getRenderGeometryDescription(RenderGeometryDescription* pTarget, const RenderGeometry* pGeometry)
        {
            if (pGeometry == nullptr)
            {
                return false;
            }
            pTarget->pVertexFormat = getVertexFormat(pGeometry);
            pTarget->vertexCount = getVertexCount(pGeometry);
            pTarget->indexCount = getIndexCount(pGeometry);
            pTarget->primitiveType = getPrimitiveType(pGeometry);
            pTarget->indexFormat = getIndexFormat(pGeometry);
            pTarget->cpuAccessMode = RenderGeometryAccessMode_None;
            return true;
        }
    }
}
