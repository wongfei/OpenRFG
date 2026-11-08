// Original: library/modules/runtime/graphics/sources/global/edrammanager.hpp
// Implementation: library/modules/runtime/graphics/sources/global/edrammanager.cpp
//
// EdramManager, an allocator for embedded render-target memory (an Xbox 360 concept kept in the shared code; unused by
// the D3D11 backend). Placement of the request/result types inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x18
    struct EdramRequest
    {
        uint32 dataIds[5]; // 0x00
        uint32 slotCount; // 0x14
    };

    // size 0x40
    struct EdramAllocationRequest : public EdramRequest
    {
        uint32 sizesInTiles[5]; // 0x18
        int priorities[5]; // 0x2C
    };

    // size 0x104
    struct EdramTargetsToResolve
    {
        uint32 targetIds[64]; // 0x00
        uint32 targetCount; // 0x100
    };

    // size 0x120
    struct EdramAllocationResult
    {
        EdramTargetsToResolve targetsToResolve; // 0x00
        uint32 offset[5]; // 0x104
        bool isInMemory[5]; // 0x118
    };

    // size 0x5
    struct EdramFreeResult
    {
        bool isUnused[5]; // 0x00
    };

    // size 0x704
    class EdramManager
    {
    public:
        enum BlockState
        {
            BlockState_Invalid = 0,
            BlockState_Free = 1,
            BlockState_Reusable = 2,
            BlockState_Unresolved = 3,
            BlockState_Used = 4,
        };

        // size 0x1C
        struct EdramBlock
        {
            EdramBlock* pNext; // 0x00
            uint32 dataId; // 0x04
            int priority; // 0x08
            uint32 refCount; // 0x0C
            uint32 offset; // 0x10
            uint32 size; // 0x14
            BlockState state; // 0x18
        };

        void create(uint32 maxTileCount); // 0x00C63E00 (:7)
        void destroy(); // 0x00C5CED0 (:13)
        void clear(EdramTargetsToResolve* pResolve); // 0x00C5CEE0 (:18)
        bool allocate(EdramAllocationResult* pResult, const EdramAllocationRequest& request); // 0x00C63FD0 (:220)
        void free(EdramFreeResult* pResult, const EdramRequest& request); // 0x00C642F0 (:297)
    private:
        EdramBlock* allocateBlock(EdramTargetsToResolve* pResolve, uint32 sizeInTiles, uint32 dataId, int priority, bool useDataId); // 0x00C63E40 (:53)
    public:
        bool allocateBlock(EdramAllocationResult* pResult, uint32 sizeInTiles, uint32 dataId, int priority); // 0x00C64340 (:317)
        void freeBlock(EdramFreeResult* pResult, uint32 dataId); // 0x00C64390 (:328)
        uint32 getOffsetInEdRam(uint32 dataId) const; // 0x00C643D0 (:337)
        bool markBlockAsResolved(uint32 dataId); // 0x00C64400 (:349)
        uint32 getSizeInTiles() const;
    private:
        const EdramBlock* getBlockById(uint32 dataId) const; // 0x00C5D000 (:381)
        EdramBlock* getBlockById(uint32 dataId); // 0x00C5CFE0 (:365)
        EdramBlock* splitBlock(EdramBlock* pLeft, uint32 sizeOfLeftBlock); // 0x00C5CF50 (:180)

        uint32 m_maxTileCount; // 0x00
        EdramBlock m_blocks[64]; // 0x04
    };
}
