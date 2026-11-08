// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/model.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/model.cpp
//
// Model, the model resource: material meshes, hierarchy, bounds and named bone lookup. Placement of
// ModelNamedBoneLookup inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/staticarray.hpp"
#include "keen/math/axisalignedbox.hpp"
#include "keen/rendereffect/materialmesh.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    namespace ModelFormat
    {
        struct ModelInitializationData;
    }
}

namespace keen
{
    class File;
    struct GraphicsSystem;
    struct Matrix43;
    struct ModelHandleType;
    struct ModelHierarchy;
    struct ModelPose;

    // size 0x8
    struct ModelNamedBoneLookup
    {
        enum
        {
            InvalidBoneIndex = 65535,
        };

        uint32 boneIndexCount; // 0x00
        uint16* pBoneIndicies; // 0x04
    };

    // size 0x44
    class Model : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x4C444F4D,
        };

        const StaticArray<MaterialMesh>& getMaterialMeshes() const; // 0x00CC8630 (.h:53)
        const ModelHierarchy* getHierarchy() const; // 0x00CC8640 (.h:55)
        const ModelPose* getDefaultPose() const; // 0x00CC8650 (.h:56)
        const AxisAlignedBox& getBoundingBox() const;
        bool findJointIndexByName(uint32* pIndex, uint32 jointNameCrc) const; // 0x00CC8820 (:117)
        const Matrix43* findHelperMatrix(uint32 id) const; // 0x00CC87E0 (:26)
        uint32 getHelperMatrixCount() const;
        const Matrix43* getHelperMatrices() const;
        const uint32* getHelperMatrixKeys() const;
        const ModelNamedBoneLookup* getNamedBoneLookup() const;
        uint32 getMorphTargetCount() const; // 0x00CC8660 (.h:69)
    private:
        Model(const Model&);
    protected:
        Model(uint32 key); // 0x00CC9340 (.h:80)
        ~Model(); // 0x00CC9360 (.h:81)
        bool initialize(const ModelFormat::ModelInitializationData* pInitializationData, uint32 initializationDataSize, GraphicsSystem* pGraphicsSystem); // 0x00CC9370 (:39)
        void destroy(GraphicsSystem* pGraphicsSystem); // 0x00CC94F0 (:92)
    private:
        Model& operator=(const Model&);
        void readMaterialMeshes(File&, uint8, bool);
        void readHierarchy(File&);
        void readCompressionOffsetScale(File&);
        void readHelpersChunk(File&);

        StaticArray<MaterialMesh> m_materialMeshes; // 0x08
        const ModelHierarchy* m_pHierarchy; // 0x10
        const ModelPose* m_pDefaultPose; // 0x14
        const ModelNamedBoneLookup* m_pNamedBoneLookup; // 0x18
        const Matrix43* m_pHelperMatrices; // 0x1C
        const uint32* m_pHelperMatrixKeys; // 0x20
        uint32 m_helperMatrixCount; // 0x24
        AxisAlignedBox m_boundingBox; // 0x28
        uint32 m_morphTargetCount; // 0x40
    };

    const Model* getFromHandle(const ModelHandleType* handle); // 0x00CC8670 (.h:112)
}
