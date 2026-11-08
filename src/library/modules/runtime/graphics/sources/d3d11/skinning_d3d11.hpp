// Original: library/modules/runtime/graphics/sources/d3d11/skinning_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/skinning_d3d11.cpp
//
// SkinningD3D11, GPU skinning through stream-out (skinned vertices written to per-instance buffers), the tasks that
// prepare joint matrices and morph targets, SkinningBatch and SkinnedGeometryInstance.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include "keen/base/databuffer.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/task/tasktypes.hpp"

struct ID3D11Buffer;

namespace keen
{
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    struct Matrix43;
    class MemoryAllocator;
    struct RenderGeometry;
    class SkinningD3D11;
    struct SoftwareSkinningJointMatrixData;
    struct TaskQueue;
    struct TaskSystem;

    // size 0x1C
    struct SkinningTask : public Task
    {
        SkinningTask(); // 0x00C6E1D0 (:17)
        static void doTask(const TaskContext* pContext); // 0x00C6B410 (:33)

        uint8* pSkinBuffer; // 0x04
        uint32 requiredSkinBufferSize; // 0x08
        SoftwareSkinningJointMatrixData* pJointMatrices; // 0x0C
        const RenderGeometry* pFirstGeometry; // 0x10
        uint32 geometryCount; // 0x14
        uint32 geometryStride; // 0x18
    };

    // size 0x2C
    struct MorphingTask : public SkinningTask
    {
        MorphingTask(); // 0x00C6E1E0 (:58)
        static void doTask(const TaskContext* pContext); // 0x00C6B450 (:73)

        uint8* pMorphBuffer; // 0x1C
        uint8* pTargetGeometries; // 0x20
        const float* pMorphWeights; // 0x24
        uint32 morphWeightCount; // 0x28
    };

    // size 0xC
    struct SkinnedGeometryInstance
    {
        SkinnedGeometryInstance(); // 0x00C5B5C0 (.h:45)

        ID3D11Buffer* pVertexBuffer; // 0x00
        uint8* pMappedData; // 0x04
        uint32 skinBufferOffset; // 0x08
    };

    // size 0x8
    struct SkinningBatch
    {
        SkinningBatch(); // 0x00C5B5B0 (.h:34)

        SkinningD3D11* pSkinningBuffer; // 0x00
        uint8* pMappedBufferData; // 0x04
    };

    // size 0x54
    class SkinningD3D11
    {
    public:
        SkinningD3D11(); // 0x00C62D10 (:140)
        bool create(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, TaskSystem* pTaskSystem, uint32 maxInstanceCount, uint32 bufferSizeInBytes); // 0x00C62D50 (:150)
        void destroy(MemoryAllocator* pAllocator); // 0x00C6B640 (:220)
        bool pushSkinningTask(uint8* pSkinBuffer, uint32 requiredSkinBufferSize, const RenderGeometry* pFirstGeometry, uint32 geometryCount, uint32 geometryStride, const Matrix43* pJointMatrices, uint32 jointMatrixCount); // 0x00C6E1F0 (:329)
        bool pushMorphTask(uint8* pMorphGeometryBuffer, uint8* pMorphBuffer, const RenderGeometry* pFirstGeometry, uint32 geometryCount, uint32 geometryStride, const float* pMorphWeights, uint32 morphWeightCount, uint8* pSkinBuffer, uint32 requiredSkinBufferSize, const Matrix43* pJointMatrices, uint32 jointMatrixCount); // 0x00C6E260 (:346)
        SkinningBatch* beginSkinning(GraphicsCommandBuffer* pCommandBuffer); // 0x00C5CB50 (:244)
        void endSkinning(SkinningBatch* pSkinningBatch, GraphicsCommandBuffer* pCommandBuffer); // 0x00C5CBC0 (:264)
        SkinnedGeometryInstance* openSkinBuffer(SkinningBatch* pSkinningBatch, uint32 size); // 0x00C6B6D0 (:279)
        void closeSkinBuffer(); // 0x00C5CC00 (:311)
        uint32 getCurrentPosition() const;
        ID3D11Buffer* getVertexBuffer() const;
        void flip(); // 0x00C62F00 (:316)
        uint8* allocateMorphData(uint32 size); // 0x00C5CC10 (:368)
        ~SkinningD3D11(); // 0x00C615B0

    private:
        ID3D11Buffer* m_pSkinningBuffer; // 0x00
        SkinningBatch m_skinningBatch; // 0x04
        SizedArray<SkinnedGeometryInstance> m_instances[2]; // 0x0C
        uint32 m_currentFrameIndex; // 0x24
        uint32 m_currentBufferPosition; // 0x28
        uint32 m_size; // 0x2C
        uint32 m_bufferSize; // 0x30
        SoftwareSkinningJointMatrixData* m_pSkinningJointMatrices; // 0x34
        uint32 m_currentJointMatricesPosition; // 0x38
        TaskQueue* m_pTaskQueue; // 0x3C
        DataBuffer m_morphBuffer; // 0x40
        uint32 m_morphBufferSize; // 0x4C
        void* m_pMorphBufferData; // 0x50
    };
}
