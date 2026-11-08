// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/largeblock/hklargeblockallocator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/hkmemoryallocator.h"

class hkBool;
class hkMemorySnapshot;
struct hkResult;

// size 0xDC
class hkLargeBlockAllocator : public hkMemoryAllocator, public hkMemoryAllocator::ExtendedInterface
{
public:
    enum AllocType
    {
        CRITICAL_ALLOC = 0,
        NORMAL_ALLOC = 1,
    };

    typedef unsigned int BinIndex;
    typedef unsigned int BinMap;
    typedef int SignedSizeT;
    typedef void (*MemBlockCallback)(void*, unsigned int, void*);

    // size 0x8
    struct MemChunk
    {
        bool isPinuse() const; // 0x00CF5E30
        bool isInuse() const; // 0x00CF5E40
        unsigned int getChunkSize() const; // 0x00CF5E50
        void clearPinuse(); // 0x00CF5E60
        void clearInuse();
        bool isNextPinuse();
        MemChunk* nextChunk(); // 0x00CF5E70
        MemChunk* previousChunk(); // 0x00CF5E80
        MemChunk* chunkPlusOffset(unsigned int s); // 0x00CF5E90
        MemChunk* chunkMinusOffset(unsigned int s); // 0x00CF5EA0
        void* getPayload(); // 0x00CF5EB0
        unsigned int getPayloadSize(); // 0x00CF5EC0
        static MemChunk* toChunk(void* in); // 0x00CF5ED0
        void setInuseAndPinuse(unsigned int s); // 0x00CF5EE0
        void setInuse(unsigned int);
        void setSizeAndPinuseOfInuseChunk(unsigned int s); // 0x00CF5F00
        unsigned int getFoot(unsigned int);
        void setFoot(unsigned int s); // 0x00CF5F10
        void setSizeAndPinuseOfFreeChunk(unsigned int s); // 0x00CF5F20
        void setFreeWithPinuse(unsigned int s, MemChunk* n); // 0x00CF5F40
        static bool isAligned(const void* ptr); // 0x00CF5F60

        static const unsigned int PINUSE_BIT; // 0x012F9370
        static const unsigned int CINUSE_BIT; // 0x012F9374
        static const unsigned int INUSE_BITS; // 0x012F9378
        static const unsigned int ALIGN; // 0x012F937C
        static const unsigned int ALIGN_MASK; // 0x012F9380
        static const unsigned int PAYLOAD_OFFSET; // 0x012F9384
        unsigned int prevFoot; // 0x00
        unsigned int head; // 0x04
    };

    // size 0x10
    struct FreeMemChunk : public MemChunk
    {
        FreeMemChunk* next; // 0x08
        FreeMemChunk* prev; // 0x0C
    };

    // size 0x20
    struct MemTreeChunk : public FreeMemChunk
    {
        MemTreeChunk* leftMostChild() const; // 0x00CF5F70

        MemTreeChunk* child[2]; // 0x10
        MemTreeChunk* parent; // 0x18
        unsigned int index; // 0x1C
    };

    // size 0x18
    struct MemPage
    {
        MemChunk* getFirstChunk() const; // 0x00CF5F80
        MemChunk* getFooter() const; // 0x00CF5F90
        unsigned int getMaxChunkSize() const; // 0x00CF5FA0
        unsigned int getContentsSize() const;
        char* getStart() const; // 0x00CF5FB0
        char* getEnd(); // 0x00CF5FC0

        MemPage* m_prev; // 0x00
        MemPage* m_next; // 0x04
        int m_numAllocs; // 0x08
        int m_size; // 0x0C
        char* m_start; // 0x10
        char* m_end; // 0x14
    };

    // size 0x14
    struct FixedMemoryBlockServer : public hkMemoryAllocator
    {
        HK_DECLARE_PLACEMENT_ALLOCATOR(); // delete(void*, unsigned int) 0x00CF7420
        FixedMemoryBlockServer(void* startIn, int size); // 0x00CF70E0
        virtual ~FixedMemoryBlockServer(); // vtbl+0x00 0x00CF61E0
        virtual void* blockAlloc(int nbytes); // vtbl+0x04 0x00CF7430
        virtual void blockFree(void* p, int nbytes); // vtbl+0x08 0x00CF7440
        virtual void* bufAlloc(int& size); // vtbl+0x0C 0x00CF61F0
        virtual void bufFree(void* data, int size); // vtbl+0x10 0x00CF6250
        virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const; // vtbl+0x20 0x00CF6270
        bool inUse(); // 0x00CF5FD0
        bool resize(void* data, int oldSize, unsigned int newSize, unsigned int& sizeOut); // 0x00CF6220
        virtual int getAllocatedSize(const void* obj, int nbytes) const; // vtbl+0x24 0x00CF7450

        char* m_start; // 0x04
        char* m_end; // 0x08
        char* m_limit; // 0x0C
        char* m_break; // 0x10
    };

    // size 0x4
    class LimitedMemoryListener
    {
    public:
        HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x00811960, delete(void*, unsigned int) 0x00811980
        virtual ~LimitedMemoryListener(); // vtbl+0x00 0x00811970 (.h:260)
        virtual void cannotAllocate(unsigned int) = 0; // vtbl+0x04
        virtual void allocationFailure(unsigned int) = 0; // vtbl+0x08
        LimitedMemoryListener(); // 0x00811A90

        // 0x00 vfptr
    };

    // size 0xC
    class Iterator
    {
    public:
        Iterator(); // 0x00CF6000
        Iterator(MemChunk* chunk, MemPage* page, MemChunk* end); // 0x00CF5FE0
        hkBool isValid() const; // 0x00CF73F0
        unsigned int getSize() const; // 0x00CF6010
        void* getAddress() const; // 0x00CF6020
        hkBool isInuse() const; // 0x00CF7400

        MemChunk* m_chunk; // 0x00
        MemPage* m_page; // 0x04
        MemChunk* m_end; // 0x08
    };

    HK_DECLARE_PLACEMENT_ALLOCATOR(); // new(unsigned int, void* p) 0x00811950, delete(void*, unsigned int) 0x00CF73B0
    hkLargeBlockAllocator();
    hkLargeBlockAllocator(void* block, int size); // 0x00CF7160
    hkLargeBlockAllocator(hkMemoryAllocator* server); // 0x00CF7110
    void init(hkMemoryAllocator*);
    virtual ~hkLargeBlockAllocator(); // vtbl+0x00 0x00CF62A0
    virtual void* blockAlloc(int bytes); // vtbl+0x04 0x00CF68A0
    virtual void garbageCollect(); // vtbl+0x04 0x00CF6B40
    virtual void blockFree(void* mem, int); // vtbl+0x08 0x00CF6D60
    virtual void incrementalGarbageCollect(int numBlocks); // vtbl+0x08 0x00CF6AB0
    virtual hkResult setMemorySoftLimit(unsigned int maxMemory); // vtbl+0x0C 0x00CF6AC0
    virtual unsigned int getMemorySoftLimit() const; // vtbl+0x10 0x00CF6AD0
    virtual bool canAllocTotal(int numBytes); // vtbl+0x14 0x00CF6B00
    virtual hkResult walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), void* param); // vtbl+0x18 0x00CF71B0
    virtual unsigned int getApproxTotalAllocated() const; // vtbl+0x1C 0x00CF6AE0
    virtual void getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const; // vtbl+0x20 0x00CF7330
    virtual void setScrubValues(unsigned int allocValue, unsigned int freeValue); // vtbl+0x20 0x00CF6AF0
    virtual int getAllocatedSize(const void* mem, int size) const; // vtbl+0x24 0x00CF6E30
    virtual int addToSnapshot(hkMemorySnapshot& snap, int parentId); // vtbl+0x24 0x00CF7220
    virtual hkMemoryAllocator::ExtendedInterface* getExtendedInterface(); // vtbl+0x2C 0x00CF73C0
    hkBool isValidAlloc(void* in); // 0x00CF6CC0
    void freeAll(); // 0x00CF63B0
    void forAllAllocs(void (*callback)(void*, unsigned int, void*), void* param); // 0x00CF6E40
    hkBool checkAllocations(void** allocs, int size); // 0x00CF7090
    void setLimitedMemoryListener(LimitedMemoryListener* listener); // 0x00811990 (.h:330)
    LimitedMemoryListener* getLimitedMemoryListener() const;
    unsigned int findLargestBlockSize() const; // 0x00CF6730
    unsigned int getTopBlockSize() const;
    unsigned int getSumAllocatedSize() const;
    unsigned int getSumAllocatedWithOverhead() const;
    unsigned int calculateMemoryUsedByThisAllocator() const; // 0x00CF6FC0
    Iterator getIterator(); // 0x00CF6EC0
    hkBool nextBlock(Iterator& iter); // 0x00CF6F00
    hkMemoryAllocator* getBlockServer() const;
public:
    bool _checkUsedAlloc(const void* mem) const; // 0x00CF6FE0
    hkBool _checkFreeChunk(MemChunk* p); // 0x00CF7040
    unsigned int _calculateUsed() const; // 0x00CF6F60
    unsigned int _findLargestTreeBlockSize(MemTreeChunk* t, unsigned int largest) const; // 0x00CF6700
    void _markTreeMap(unsigned int i); // 0x00CF6030
    void _clearTreeMap(unsigned int i); // 0x00CF6050
    bool _isTreeMapMarked(unsigned int i) const; // 0x00CF6070
    void _insertLargeChunk(MemTreeChunk* x, unsigned int s); // 0x00CF6400
    void _unlinkLargeChunk(MemTreeChunk* x); // 0x00CF64C0
    void* _allocLarge(unsigned int nb); // 0x00CF6570
    void* _allocFromTop(unsigned int nb); // 0x00CF67A0
    void _makeTopValid() const; // 0x00CF6780
    void* _alloc(unsigned int nb); // 0x00CF6860
    void _init(); // 0x00CF6300
    static unsigned int _computeTreeIndex(unsigned int s); // 0x00CF6090
    static unsigned int _bitToIndex(unsigned int x); // 0x00CF6100
    static unsigned int _padRequest(unsigned int req); // 0x00CF6150
    static int _bitForTreeIndex(int);
    static int _leftShiftForTreeIndex(int i); // 0x00CF6160
    static unsigned int _minSizeForTreeIndex(int);
    hkBool _resizeSingleBlockServerPage(unsigned int newSize); // 0x00CF67E0
    bool _isOkAddress(const void* a) const; // 0x00CF6180
    static int _leastBit(int x); // 0x00CF6190
    static int _leftBits(int x); // 0x00CF61A0
    static int _indexToBit(int x); // 0x00CF61B0
    static int _sameOrLeftBits(int);
    static hkBool _comparePointers(void*, void*);
    hkBool _hasPages(); // 0x00CF61C0
    static void _addAlloc(void* data, unsigned int size, void* param); // 0x00CF7370

    static const unsigned int NTREEBINS; // 0x012F9388
    static const unsigned int TREEBIN_SHIFT; // 0x012F938C
    static const unsigned int MIN_LARGE_SIZE; // 0x012F9390
    static const unsigned int SIZE_T_BITSIZE; // 0x012F9394
    LimitedMemoryListener* m_limitedListener; // 0x08
    unsigned int m_treemap; // 0x0C
    char* m_leastAddr; // 0x10
    MemChunk* m_top; // 0x14
    unsigned int m_topsize; // 0x18
    MemTreeChunk* m_treebins[32]; // 0x1C
    FixedMemoryBlockServer m_fixedBlockServer; // 0x9C
    hkMemoryAllocator* m_server; // 0xB0
    MemChunk m_zeroChunk; // 0xB4
    MemPage m_pages; // 0xBC
    unsigned int m_sumAllocatedWithMgrOverhead; // 0xD4
    unsigned int m_sumAllocatedSize; // 0xD8
};
