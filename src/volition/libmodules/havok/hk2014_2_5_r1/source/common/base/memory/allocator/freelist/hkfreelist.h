// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelist.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

class hkBool;
class hkMemorySnapshot;

// size 0x38
class hkFreeList
{
public:
    typedef void (*MemoryWalkCallback)(void*, unsigned int, bool, int, void*);

    // size 0x4
    struct Element
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Element);
        static bool less(const Element& eA, const Element& eB); // 0x00D11720
        static Element*& next(Element* e); // 0x00D11730

        Element* m_next; // 0x00
    };

    // size 0x10
    struct Block
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Block);
        static bool less(const Block& bA, const Block& bB); // 0x00D11740
        static Block*& next(Block* b); // 0x00D11750

        Block* m_next; // 0x00
        unsigned char* m_elementsAlloc; // 0x04
        unsigned char* m_elements; // 0x08
        unsigned int m_numElements; // 0x0C
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x00CF4890, delete(void*, unsigned int) 0x00CF48A0
    hkFreeList(); // 0x00D11890
    hkFreeList(unsigned int elementSize, unsigned int align, unsigned int blockSize, hkMemoryAllocator* elementAllocator, hkMemoryAllocator* blockAllocator); // 0x00D121E0
    ~hkFreeList(); // 0x00CF48B0
    void init(unsigned int elementSize, unsigned int align, unsigned int blockSize, hkMemoryAllocator* elementAllocator, hkMemoryAllocator* blockAllocator); // 0x00D11C70
    hkBool canAlloc();
    void* alloc(); // 0x00CF4A10
    void free(void* data); // 0x00CF4A40
    void allocBatch(void** out, int sizeIn); // 0x00CF4950
    void freeBatch(void** in, int sizeIn); // 0x00CF49E0
    void freeAllMemory(); // 0x00D11950
    void freeAll(); // 0x00D11B60
    unsigned int getElementSize() const; // 0x00CF48C0
    hkBool isOk(); // 0x00CF48D0
    hkBool isFreeElementAvailable() const; // 0x00CF48F0
    void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const; // 0x00D11BA0
    int findGarbage(); // 0x00D11D30
    hkBool incrementalFindGarbage(int numBlocks, int& numBlocksOut); // 0x00D12410
    void garbageCollect(); // 0x00F51510
    int freeAllFreeBlocks(); // 0x00D11B20
    hkBool hasFreeBlocks() const; // 0x00CF4920
    unsigned int getNumFreeElements() const; // 0x00D11760
    unsigned int getTotalNumElements() const; // 0x00D11770
    unsigned int getNumUsedElements() const;
    unsigned int getBlockSize() const;
    int addToSnapshot(hkMemorySnapshot& snap, signed char usage /* hkMemorySnapshot::StatusBits */, int parentId); // 0x00D11EF0
    void walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), int pool, void* param); // 0x00D11E20
    const Block* getActiveBlocks() const; // 0x00CF4930
    const Block* getFreeBlocks() const; // 0x00CF4940
    static unsigned int bestBlockSize(unsigned int elementSpace, unsigned int align); // 0x00D11B40
    static int calcNumBlocks(const Block* blocks); // 0x00D118C0
    int _getSortedBlockHeads(const Block** heads, int numHeads); // 0x00D12180
public:
    void* addSpace(); // 0x00D12010
    unsigned int _calcNumFreeElements() const; // 0x00D119C0
    static unsigned int _calcTotalNumElements(Block* cur); // 0x00D11A00
    unsigned int _calcTotalNumElements() const; // 0x00D11A90
    unsigned int _calcNumUsedElements() const;
    int _freeBlocks(Block* cur); // 0x00D118E0
    unsigned int _calculateBlockAllocatedSize(Block* cur) const; // 0x00D11A20
    void _init(unsigned int elementSize, unsigned int align, unsigned int blockSize, hkMemoryAllocator* elementAllocator, hkMemoryAllocator* blockAllocator); // 0x00D11BF0
    static hkBool _compareBlocks(const Block*, const Block*);
    static hkBool _compareElements(const Element* a, const Element* b); // 0x00D11780
    void _addBlockElements(Block* block); // 0x00D117A0
    void _walkMemoryBlockList(Block* block, void (*callback)(void*, unsigned int, bool, int, void*), int pool, void* param); // 0x00D11AB0
    hkBool _checkFreeBlocks(); // 0x00D11CA0
    unsigned int _calcBlockFree(Block* block); // 0x00D12210
    void _moveTopToFree(); // 0x00D11990

    Element* m_free; // 0x00
    unsigned int m_elementSize; // 0x04
    Block* m_lastIncrementalBlock; // 0x08
    Block* m_activeBlocks; // 0x0C
    Block* m_freeBlocks; // 0x10
    unsigned int m_blockSize; // 0x14
    unsigned int m_align; // 0x18
    unsigned int m_numBlockElements; // 0x1C
    unsigned char* m_top; // 0x20
    unsigned char* m_blockEnd; // 0x24
    hkMemoryAllocator* m_elementAllocator; // 0x28
    hkMemoryAllocator* m_blockAllocator; // 0x2C
    unsigned int m_totalNumElements; // 0x30
public:
    unsigned int m_numFreeElements; // 0x34
};

hkFreeList::Block* _sortByAllocationAddress(hkFreeList::Block* headPtr); // 0x00D117C0
