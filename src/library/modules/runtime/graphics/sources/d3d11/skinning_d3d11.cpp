// Original: library/modules/runtime/graphics/sources/d3d11/skinning_d3d11.cpp
//
// SkinningD3D11: CPU skinning into a dynamic vertex buffer, with skinning/morphing tasks on the task system.
//
// Partial: construction, create/destroy and flip, which the graphics system calls every frame. The skinning paths
// (tasks, skin buffer mapping) are not ported: the render subset has no skinned meshes, and a graphics system created
// with skinningBufferSizeInBytes 0 never uses them.

#include "library/modules/runtime/graphics/sources/d3d11/skinning_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/task/taskqueue.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // 0x00C5B5C0 (.h:45)
    SkinnedGeometryInstance::SkinnedGeometryInstance()
    {
        pVertexBuffer = nullptr;
        skinBufferOffset = 0;
    }

    // 0x00C5B5B0 (.h:34)
    SkinningBatch::SkinningBatch()
    {
        pSkinningBuffer = nullptr;
        pMappedBufferData = nullptr;
    }

    // 0x00C62D10 (:140)
    SkinningD3D11::SkinningD3D11()
    {
        m_pSkinningBuffer = nullptr;
        m_currentBufferPosition = 0;
        m_size = 0;
        m_pSkinningJointMatrices = nullptr;
        m_currentJointMatricesPosition = 0;
        m_pTaskQueue = nullptr;
    }

    // 0x00C615B0
    SkinningD3D11::~SkinningD3D11()
    {
    }

    // 0x00C62D50 (:150)
    bool SkinningD3D11::create(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, TaskSystem* pTaskSystem, uint32 maxInstanceCount, uint32 bufferSizeInBytes)
    {
        m_size = bufferSizeInBytes;
        if (bufferSizeInBytes == 0)
        {
            return true;
        }
        TaskQueueParameters queueParameters;
        queueParameters.priority = 1;
        queueParameters.alignment = 16;
        queueParameters.maxTaskSize = sizeof(MorphingTask);
        queueParameters.maxTaskCount = maxInstanceCount;
        m_pTaskQueue = task::createTaskQueue(pAllocator, pTaskSystem, queueParameters);
        if (m_pTaskQueue == nullptr)
        {
            debug::nativePrintArgs("Couldn't create task queue for d3d11 skinning.\n");
            return false;
        }
        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth = m_size;
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;
        if (FAILED(pGraphicsSystem->pDevice->CreateBuffer(&bufferDesc, nullptr, &m_pSkinningBuffer)))
        {
            debug::nativePrintArgs("[graphics] Could not create vertex buffer!\n");
            task::destroyTaskQueue(pAllocator, m_pTaskQueue);
            m_pTaskQueue = nullptr;
            return false;
        }
        for (uint32 i = 0; i < 2; ++i)
        {
            if (!m_instances[i].create(pAllocator, maxInstanceCount, 16))
            {
                return false;
            }
        }
        m_currentFrameIndex = 0;
        // Magic number: 16 KB of joint matrices per instance.
        m_pSkinningJointMatrices = (SoftwareSkinningJointMatrixData*)pAllocator->allocate(maxInstanceCount << 14, 16, 0, nullptr);
        m_currentJointMatricesPosition = 0;
        m_skinningBatch.pMappedBufferData = nullptr;
        // Magic number: 5,000,000 bytes of morph target scratch.
        m_morphBufferSize = 5000000;
        m_pMorphBufferData = pAllocator->allocate(5000000, 16, 0, nullptr);
        return true;
    }

    // 0x00C6B640 (:220)
    void SkinningD3D11::destroy(MemoryAllocator* pAllocator)
    {
        // Notes (decompiled behaviour, kept as is): the morph buffer data is not freed.
        m_instances[1].destroy(pAllocator);
        m_instances[0].destroy(pAllocator);
        if (m_pSkinningJointMatrices != nullptr)
        {
            pAllocator->free(m_pSkinningJointMatrices);
            m_pSkinningJointMatrices = nullptr;
        }
        if (m_pSkinningBuffer != nullptr)
        {
            m_pSkinningBuffer->Release();
            m_pSkinningBuffer = nullptr;
        }
        if (m_pTaskQueue != nullptr)
        {
            task::destroyTaskQueue(pAllocator, m_pTaskQueue);
            m_pTaskQueue = nullptr;
        }
    }

    // 0x00C62F00 (:316)
    void SkinningD3D11::flip()
    {
        if (m_size != 0)
        {
            m_currentFrameIndex = 1 - m_currentFrameIndex;
            m_currentBufferPosition = 0;
            m_currentJointMatricesPosition = 0;
            m_instances[m_currentFrameIndex].clear();
        }
    }
}
