// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelist.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): a free list of fixed-size elements. Elements come from blocks: a block's
// header (Block) is either in front of its elements (no block allocator: one buffer from the element allocator) or
// separate (16 bytes from the block allocator, the elements from the element allocator). New elements are taken from
// [m_top, m_blockEnd) of the newest block, freed ones go on the m_free list. Garbage collection finds the blocks whose
// elements are all free and moves them to m_freeBlocks, from where freeAllFreeBlocks gives them back.
// The inline functions of hkfreelist.inl/.h with out-of-line copies are defined here too.
//
// m_numFreeElements counts the free elements of all blocks (the m_free list, [m_top, m_blockEnd) and the free
// blocks); m_totalNumElements counts the elements of all blocks.
//
// Not reimplemented: addToSnapshot (hkMemorySnapshot is S phase): nothing is recorded.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelist.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include <stdint.h>

// 0x00D117C0 (:16)
// Merge sort of a block list by element storage address (blocks with a header in front sort by m_elementsAlloc 0,
// i.e. first), as hkAlgorithm::sortList but inlined with that key.
hkFreeList::Block* _sortByAllocationAddress(hkFreeList::Block* headPtr)
{
    if (headPtr == nullptr)
    {
        return nullptr;
    }
    hkFreeList::Block* list = headPtr;
    int insize = 1;
    for (;;)
    {
        hkFreeList::Block head;
        head.m_next = nullptr;
        hkFreeList::Block* tail = &head;
        int nmerges = 0;
        hkFreeList::Block* p = list;
        while (p != nullptr)
        {
            nmerges++;
            hkFreeList::Block* q = p;
            int psize = 0;
            for (int i = 0; i < insize && q != nullptr; i++)
            {
                q = q->m_next;
                psize++;
            }
            int qsize = insize;
            while (psize > 0 || (qsize > 0 && q != nullptr))
            {
                hkFreeList::Block* e;
                if (psize == 0)
                {
                    e = q;
                    q = q->m_next;
                    qsize--;
                }
                else if (qsize == 0 || q == nullptr || !(q->m_elementsAlloc < p->m_elementsAlloc))
                {
                    e = p;
                    p = p->m_next;
                    psize--;
                }
                else
                {
                    e = q;
                    q = q->m_next;
                    qsize--;
                }
                tail->m_next = e;
                tail = e;
            }
            p = q;
        }
        tail->m_next = nullptr;
        if (nmerges < 2)
        {
            return head.m_next;
        }
        list = head.m_next;
        insize *= 2;
    }
}

// ---- Element / Block (hkfreelist.h)
// 0x00D11720 (.h:47)
bool hkFreeList::Element::less(const Element& eA, const Element& eB)
{
    return &eA < &eB;
}

// 0x00D11730 (.h:48)
hkFreeList::Element*& hkFreeList::Element::next(Element* e)
{
    return e->m_next;
}

// 0x00D11740 (.h:59)
bool hkFreeList::Block::less(const Block& bA, const Block& bB)
{
    return &bA < &bB;
}

// 0x00D11750 (.h:60)
hkFreeList::Block*& hkFreeList::Block::next(Block* b)
{
    return b->m_next;
}

// ---- construction
// 0x00D11890 (:102)
hkFreeList::hkFreeList()
{
    m_free = nullptr;
    m_activeBlocks = nullptr;
    m_freeBlocks = nullptr;
    m_blockSize = 0;
    m_align = 0;
    m_top = nullptr;
    m_blockEnd = nullptr;
    m_numFreeElements = 0;
    m_totalNumElements = 0;
    m_blockAllocator = nullptr;
    m_elementAllocator = nullptr;
    m_elementSize = 0;
}

// 0x00D121E0 (:97)
hkFreeList::hkFreeList(unsigned int elementSize, unsigned int align, unsigned int blockSize, hkMemoryAllocator* elementAllocator, hkMemoryAllocator* blockAllocator)
{
    _init(elementSize, align, blockSize, elementAllocator, blockAllocator);
}

// 0x00CF48B0 (.h:41)
hkFreeList::~hkFreeList()
{
    freeAllMemory();
}

// 0x00D11BF0 (:121)
// The element size is rounded up to the alignment; no element allocator means the thread's heap.
void hkFreeList::_init(unsigned int elementSize, unsigned int align, unsigned int blockSize, hkMemoryAllocator* elementAllocator, hkMemoryAllocator* blockAllocator)
{
    if (elementAllocator == nullptr)
    {
        elementAllocator = &hkMemoryRouter::getInstance().heap();
    }
    m_elementAllocator = elementAllocator;
    m_blockAllocator = blockAllocator;
    m_free = nullptr;
    m_activeBlocks = nullptr;
    m_freeBlocks = nullptr;
    m_blockSize = blockSize;
    m_align = align;
    m_top = nullptr;
    m_blockEnd = nullptr;
    m_numFreeElements = 0;
    m_totalNumElements = 0;
    m_lastIncrementalBlock = nullptr;
    if (elementSize < align)
    {
        m_elementSize = align;
        m_numBlockElements = blockSize / align;
        return;
    }
    m_elementSize = (elementSize - 1 + align) & ~(align - 1);
    m_numBlockElements = blockSize / m_elementSize;
}

// 0x00D11C70 (:169)
void hkFreeList::init(unsigned int elementSize, unsigned int align, unsigned int blockSize, hkMemoryAllocator* elementAllocator, hkMemoryAllocator* blockAllocator)
{
    freeAllMemory();
    _init(elementSize, align, blockSize, elementAllocator, blockAllocator);
}

// ---- allocation (hkfreelist.inl)
// 0x00CF4A10 (.inl:71)
void* hkFreeList::alloc()
{
    if (m_free != nullptr)
    {
        m_numFreeElements--;
        Element* e = m_free;
        m_free = e->m_next;
        return e;
    }
    if (m_top < m_blockEnd)
    {
        m_numFreeElements--;
        unsigned char* p = m_top;
        m_top = p + m_elementSize;
        return p;
    }
    return addSpace();
}

// 0x00CF4A40 (.inl:114)
void hkFreeList::free(void* data)
{
    m_numFreeElements++;
    ((Element*)data)->m_next = m_free;
    m_free = (Element*)data;
}

// 0x00CF4950 (.inl:10)
// From the free list first, then from the top of the block, then new blocks.
void hkFreeList::allocBatch(void** out, int sizeIn)
{
    void** cur = out;
    void** end = out + sizeIn;
    Element* e = m_free;
    while (cur < end && e != nullptr)
    {
        *cur++ = e;
        e = e->m_next;
    }
    m_numFreeElements -= (unsigned int)(cur - out);
    m_free = e;
    while (cur < end)
    {
        if (m_top < m_blockEnd)
        {
            void** start = cur;
            do
            {
                if (cur >= end)
                {
                    break;
                }
                *cur++ = m_top;
                m_top += m_elementSize;
            } while (m_top < m_blockEnd);
            m_numFreeElements -= (unsigned int)(cur - start);
        }
        else
        {
            *cur++ = addSpace();
        }
    }
}

// 0x00CF49E0 (.inl:51)
// Null entries are skipped.
void hkFreeList::freeBatch(void** in, int sizeIn)
{
    m_numFreeElements += sizeIn;
    Element* head = m_free;
    while (--sizeIn >= 0)
    {
        Element* e = (Element*)*in++;
        if (e == nullptr)
        {
            m_numFreeElements--;
        }
        else
        {
            e->m_next = head;
            head = e;
        }
    }
    m_free = head;
}

// 0x00F51510 (.inl:124)
void hkFreeList::garbageCollect()
{
    findGarbage();
    freeAllFreeBlocks();
}

// 0x00D12010 (:765)
// The free list and the block top are exhausted: reuses a free block, or allocates a new one (the default block
// size holds 256 bytes of elements, at least one, plus the alignment slack) and returns its first element.
void* hkFreeList::addSpace()
{
    Block* block = m_freeBlocks;
    if (block != nullptr)
    {
        m_freeBlocks = block->m_next;
        m_top = block->m_elements;
        m_blockEnd = block->m_elements + block->m_numElements * m_elementSize;
        block->m_next = m_activeBlocks;
        m_numFreeElements--;
        m_activeBlocks = block;
        unsigned char* p = m_top;
        m_top = p + m_elementSize;
        return p;
    }
    if (m_blockSize == 0)
    {
        int numElements = (int)(256 / (unsigned long long)m_elementSize); // 256: bytes of elements per default block
        if (numElements == 0)
        {
            numElements = 1;
        }
        m_blockSize = bestBlockSize(m_elementSize * numElements, m_align);
    }
    if (m_blockAllocator == nullptr)
    {
        // the header in front of the elements
        int size = (int)m_blockSize;
        block = (Block*)m_elementAllocator->bufAlloc(size);
        if (block == nullptr)
        {
            return nullptr;
        }
        unsigned char* elements = (unsigned char*)((uintptr_t)((unsigned char*)block + sizeof(Block) + m_align - 1) & ~(uintptr_t)(m_align - 1));
        block->m_elementsAlloc = nullptr;
        block->m_elements = elements;
        block->m_numElements = (unsigned int)((unsigned char*)block + size - elements) / m_elementSize;
    }
    else
    {
        block = (Block*)m_blockAllocator->blockAlloc(sizeof(Block));
        if (block == nullptr)
        {
            return nullptr;
        }
        int size = (int)m_blockSize;
        unsigned char* alloc = (unsigned char*)m_elementAllocator->bufAlloc(size);
        if (alloc == nullptr)
        {
            m_blockAllocator->blockFree(block, sizeof(Block));
            return nullptr;
        }
        block->m_elementsAlloc = alloc;
        unsigned char* elements = (unsigned char*)((uintptr_t)(alloc + m_align - 1) & ~(uintptr_t)(m_align - 1));
        block->m_elements = elements;
        if (elements == alloc && (unsigned int)size == m_blockSize)
        {
            block->m_numElements = m_numBlockElements;
        }
        else
        {
            block->m_numElements = (unsigned int)(alloc + size - elements) / m_elementSize;
        }
    }
    m_top = block->m_elements;
    m_blockEnd = block->m_elements + block->m_numElements * m_elementSize;
    block->m_next = m_activeBlocks;
    m_activeBlocks = block;
    m_totalNumElements += block->m_numElements;
    unsigned char* p = m_top;
    m_numFreeElements += block->m_numElements - 1;
    m_top = p + m_elementSize;
    return p;
}

// ---- blocks
// 0x00D118C0 (:175)
int hkFreeList::calcNumBlocks(const Block* blocks)
{
    int n = 0;
    for (; blocks != nullptr; blocks = blocks->m_next)
    {
        n++;
    }
    return n;
}

// 0x00D118E0 (:243)
// Gives the blocks back; their elements leave both counts.
int hkFreeList::_freeBlocks(Block* cur)
{
    int n = 0;
    while (cur != nullptr)
    {
        m_totalNumElements -= cur->m_numElements;
        m_numFreeElements -= cur->m_numElements;
        Block* next = cur->m_next;
        n++;
        if (m_blockAllocator == nullptr)
        {
            m_elementAllocator->bufFree(cur, m_blockSize);
        }
        else
        {
            m_elementAllocator->bufFree(cur->m_elementsAlloc, m_blockSize);
            m_blockAllocator->blockFree(cur, sizeof(Block));
        }
        cur = next;
    }
    return n;
}

// 0x00D11950 (:277)
void hkFreeList::freeAllMemory()
{
    _freeBlocks(m_activeBlocks);
    m_activeBlocks = nullptr;
    _freeBlocks(m_freeBlocks);
    m_freeBlocks = nullptr;
    m_free = nullptr;
    m_blockSize = 0;
    m_numFreeElements = 0;
    m_totalNumElements = 0;
    m_top = nullptr;
    m_blockEnd = nullptr;
    m_lastIncrementalBlock = nullptr;
}

// 0x00D11990 (:375)
// The rest of the top block goes onto the free list (in address order: the last element ends up first).
void hkFreeList::_moveTopToFree()
{
    Element* head = m_free;
    for (unsigned char* p = m_top; p < m_blockEnd; p += m_elementSize)
    {
        ((Element*)p)->m_next = head;
        head = (Element*)p;
    }
    m_free = head;
    m_blockEnd = nullptr;
    m_top = nullptr;
}

// 0x00D119C0 (:461)
unsigned int hkFreeList::_calcNumFreeElements() const
{
    int n = 0;
    for (const Element* e = m_free; e != nullptr; e = e->m_next)
    {
        n++;
    }
    unsigned int total = n + (unsigned int)(m_blockEnd - m_top) / m_elementSize;
    for (const Block* b = m_freeBlocks; b != nullptr; b = b->m_next)
    {
        total += b->m_numElements;
    }
    return total;
}

// 0x00D11A00 (:486)
unsigned int hkFreeList::_calcTotalNumElements(Block* cur)
{
    unsigned int n = 0;
    for (; cur != nullptr; cur = cur->m_next)
    {
        n += cur->m_numElements;
    }
    return n;
}

// 0x00D11A90 (:528)
unsigned int hkFreeList::_calcTotalNumElements() const
{
    return _calcTotalNumElements(m_freeBlocks) + _calcTotalNumElements(m_activeBlocks);
}

// 0x00D11A20 (:497)
unsigned int hkFreeList::_calculateBlockAllocatedSize(Block* cur) const
{
    unsigned int size = 0;
    if (m_blockAllocator == nullptr)
    {
        for (; cur != nullptr; cur = cur->m_next)
        {
            size += m_elementAllocator->getAllocatedSize(cur, m_blockSize);
        }
        return size;
    }
    for (; cur != nullptr; cur = cur->m_next)
    {
        const int header = m_blockAllocator->getAllocatedSize(cur, sizeof(Block));
        const int elements = m_elementAllocator->getAllocatedSize(cur->m_elementsAlloc, m_blockSize);
        size += header + elements;
    }
    return size;
}

// 0x00D11AB0 (:623)
void hkFreeList::_walkMemoryBlockList(Block* block, void (*callback)(void*, unsigned int, bool, int, void*), int pool, void* param)
{
    for (; block != nullptr; block = block->m_next)
    {
        unsigned char* p = block->m_elements;
        unsigned char* end = p + block->m_numElements * m_elementSize;
        for (; p < end; p += m_elementSize)
        {
            callback(p, m_elementSize, false, pool, param);
        }
    }
}

// 0x00D11B20 (:757)
int hkFreeList::freeAllFreeBlocks()
{
    const int n = _freeBlocks(m_freeBlocks);
    m_freeBlocks = nullptr;
    return n;
}

// 0x00D11B40 (:914)
// The element space plus the alignment slack (at least 16, the header).
unsigned int hkFreeList::bestBlockSize(unsigned int elementSpace, unsigned int align)
{
    if (align <= 16)
    {
        return elementSpace + 16;
    }
    return align + elementSpace;
}

// 0x00D11B60 (:921)
// Every element is free again: all active blocks become free blocks.
void hkFreeList::freeAll()
{
    m_numFreeElements = m_totalNumElements;
    Block* b = m_activeBlocks;
    if (b != nullptr)
    {
        m_free = nullptr;
        while (b->m_next != nullptr)
        {
            b = b->m_next;
        }
        b->m_next = m_freeBlocks;
        m_freeBlocks = m_activeBlocks;
        m_activeBlocks = nullptr;
        m_top = nullptr;
        m_blockEnd = nullptr;
        m_lastIncrementalBlock = nullptr;
    }
}

// 0x00D11BA0 (:957)
void hkFreeList::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const
{
    stats.m_allocated = _calculateBlockAllocatedSize(m_freeBlocks) + _calculateBlockAllocatedSize(m_activeBlocks);
    const unsigned int available = m_elementSize * m_numFreeElements;
    stats.m_available = available;
    stats.m_inUse = (m_totalNumElements - m_numFreeElements) * m_elementSize;
    stats.m_largestBlock = m_elementSize;
    stats.m_totalAvailable = available;
}

// 0x00D11CA0 (:186)
// The counts match the lists, and every free element is an aligned element of an active block.
hkBool hkFreeList::_checkFreeBlocks()
{
    hkBool result;
    if (_calcNumFreeElements() != m_numFreeElements || _calcTotalNumElements() != m_totalNumElements)
    {
        result.m_bool = 0;
        return result;
    }
    for (Element* e = m_free; e != nullptr; e = e->m_next)
    {
        if (((uintptr_t)e & (m_align - 1)) != 0)
        {
            result.m_bool = 0;
            return result;
        }
        Block* b = m_activeBlocks;
        for (;; b = b->m_next)
        {
            if (b == nullptr)
            {
                result.m_bool = 0;
                return result;
            }
            if ((unsigned char*)e >= b->m_elements && (unsigned char*)e < b->m_elements + b->m_numElements * m_elementSize)
            {
                break;
            }
        }
        if ((unsigned int)((unsigned char*)e - b->m_elements) % m_elementSize != 0)
        {
            result.m_bool = 0;
            return result;
        }
    }
    result.m_bool = 1;
    return result;
}

// 0x00CF48D0 (.h:97)
hkBool hkFreeList::isOk()
{
    return _checkFreeBlocks();
}

// 0x00D11780 (.h:184)
hkBool hkFreeList::_compareElements(const Element* a, const Element* b)
{
    hkBool result;
    result.m_bool = a < b;
    return result;
}

// 0x00D117A0 (.h:188)
void hkFreeList::_addBlockElements(Block* block)
{
    m_top = block->m_elements;
    m_blockEnd = block->m_elements + block->m_numElements * m_elementSize;
}

// 0x00CF48C0 (.h:94)
unsigned int hkFreeList::getElementSize() const
{
    return m_elementSize;
}

// 0x00CF48F0 (.h:100)
hkBool hkFreeList::isFreeElementAvailable() const
{
    hkBool result;
    result.m_bool = (m_free != nullptr || m_top < m_blockEnd) ? 1 : 0;
    return result;
}

// 0x00CF4920 (.h:121)
hkBool hkFreeList::hasFreeBlocks() const
{
    hkBool result;
    result.m_bool = m_freeBlocks != nullptr;
    return result;
}

// 0x00D11760 (.h:124)
unsigned int hkFreeList::getNumFreeElements() const
{
    return m_numFreeElements;
}

// 0x00D11770 (.h:127)
unsigned int hkFreeList::getTotalNumElements() const
{
    return m_totalNumElements;
}

// 0x00CF4930 (.h:144)
const hkFreeList::Block* hkFreeList::getActiveBlocks() const
{
    return m_activeBlocks;
}

// 0x00CF4940 (.h:146)
const hkFreeList::Block* hkFreeList::getFreeBlocks() const
{
    return m_freeBlocks;
}

// ---- garbage collection
// 0x00D11D30 (:533)
// Sorts the active blocks and the free list by address, then walks both: a block whose elements are all on the free
// list (consecutively, since both are sorted) moves to the free blocks and its elements leave the free list.
int hkFreeList::findGarbage()
{
    if (m_activeBlocks == nullptr)
    {
        return 0;
    }
    m_activeBlocks = _sortByAllocationAddress(m_activeBlocks);
    _moveTopToFree();
    Element* free = hkAlgorithm::sortList(m_free, Element::next, Element::less);
    m_free = free;

    Element freeHead; // link to the first free element still listed
    freeHead.m_next = free;
    Block blockHead;
    blockHead.m_next = m_activeBlocks;
    Element* prevFree = &freeHead;
    Block* prevBlock = &blockHead;
    int numFound = 0;
    Block* block = m_activeBlocks;
    while (block != nullptr)
    {
        Element* e = (Element*)block->m_elements;
        Element* end = (Element*)(block->m_elements + block->m_numElements * m_elementSize);
        if (e == free)
        {
            do
            {
                if (e >= end)
                {
                    break;
                }
                free = free->m_next;
                e = (Element*)((unsigned char*)e + m_elementSize);
            } while (e == free);
        }
        if (e == end)
        {
            // all free: unlink the block and drop its elements from the free list
            numFound++;
            prevBlock->m_next = block->m_next;
            block->m_next = m_freeBlocks;
            m_freeBlocks = block;
            block = prevBlock->m_next;
            prevFree->m_next = free;
        }
        else
        {
            while (free != nullptr && free < end)
            {
                free = free->m_next;
            }
            while (prevFree->m_next != free)
            {
                prevFree = prevFree->m_next;
            }
            prevBlock = block;
            block = block->m_next;
        }
    }
    m_free = freeHead.m_next;
    m_activeBlocks = blockHead.m_next;
    m_lastIncrementalBlock = nullptr;
    return numFound;
}

// 0x00D12210 (:299)
// Takes the block's elements off the free list (into a LIFO buffer). Returns 1 when the whole block is free (with
// the top of the block, if it is the current one): its elements stay off the list and the top is dropped. Otherwise
// the elements go back onto the list, sorted by address, and 0 is returned.
unsigned int hkFreeList::_calcBlockFree(Block* block)
{
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    const int bufferBytes = (int)((block->m_numElements * sizeof(Element*) + 127) & ~127u); // hkLocalBuffer: 128-byte multiples
    Element** taken = (Element**)stack.fastBlockAlloc(bufferBytes);
    unsigned char* start = block->m_elements;
    unsigned char* end = start + m_elementSize * block->m_numElements;
    unsigned int numTaken = 0;
    Element* prev = (Element*)this; // m_free is the first word: the list head acts as an element
    for (Element* e = m_free; e != nullptr; e = e->m_next)
    {
        if ((unsigned char*)e >= start && (unsigned char*)e < end)
        {
            taken[numTaken++] = e;
            prev->m_next = e->m_next;
        }
        else
        {
            prev = e;
        }
    }
    if (m_top < start || m_top >= end)
    {
        if (numTaken == block->m_numElements)
        {
            stack.fastBlockFree(taken, bufferBytes);
            return 1;
        }
    }
    else if ((unsigned int)(m_blockEnd - m_top) / m_elementSize + numTaken == block->m_numElements)
    {
        m_top = nullptr;
        m_blockEnd = nullptr;
        stack.fastBlockFree(taken, bufferBytes);
        return 1;
    }
    if ((int)numTaken > 0)
    {
        if ((int)numTaken > 1)
        {
            hkAlgorithm::quickSortRecursive(taken, 0, (int)numTaken - 1, _compareElements);
        }
        Element* e = taken[0];
        for (unsigned int i = 1; i < numTaken; i++)
        {
            e->m_next = taken[i];
            e = taken[i];
        }
        taken[numTaken - 1]->m_next = m_free;
        m_free = taken[0];
    }
    stack.fastBlockFree(taken, bufferBytes);
    return 0;
}

// 0x00D12410 (:395)
// Checks up to numBlocks active blocks after the last checked one; whole free blocks move to the free blocks.
// True when the end of the active list was reached.
hkBool hkFreeList::incrementalFindGarbage(int numBlocks, int& numBlocksOut)
{
    hkBool result;
    int i = 0;
    for (; i < numBlocks; i++)
    {
        Block* block;
        if (m_lastIncrementalBlock == nullptr)
        {
            block = m_activeBlocks;
            if (block == nullptr)
            {
                numBlocksOut = i;
                m_lastIncrementalBlock = nullptr;
                result.m_bool = 1;
                return result;
            }
            if (_calcBlockFree(block) == 0)
            {
                m_lastIncrementalBlock = block;
            }
            else
            {
                m_activeBlocks = block->m_next;
                block->m_next = m_freeBlocks;
                m_freeBlocks = block;
            }
        }
        else
        {
            block = m_lastIncrementalBlock->m_next;
            if (block == nullptr)
            {
                numBlocksOut = i;
                m_lastIncrementalBlock = nullptr;
                result.m_bool = 1;
                return result;
            }
            if (_calcBlockFree(block) == 0)
            {
                m_lastIncrementalBlock = block;
            }
            else
            {
                m_lastIncrementalBlock->m_next = block->m_next;
                block->m_next = m_freeBlocks;
                m_freeBlocks = block;
            }
        }
    }
    numBlocksOut = i;
    result.m_bool = m_lastIncrementalBlock == nullptr;
    return result;
}

// 0x00D12180 (:978)
// Sorts both block lists by address and returns their heads (active first; empty lists are left out).
int hkFreeList::_getSortedBlockHeads(const Block** heads, int numHeads)
{
    m_activeBlocks = hkAlgorithm::sortList(m_activeBlocks, Block::next, Block::less);
    m_freeBlocks = hkAlgorithm::sortList(m_freeBlocks, Block::next, Block::less);
    int n = 0;
    if (m_activeBlocks != nullptr)
    {
        heads[n++] = m_activeBlocks;
    }
    if (m_freeBlocks != nullptr)
    {
        heads[n++] = m_freeBlocks;
    }
    return n;
}

// 0x00D11E20 (:640)
// Calls back every element: those of free blocks as free, then those of the active blocks (sorted by address) as
// used unless on the (sorted) free list.
void hkFreeList::walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), int pool, void* param)
{
    _walkMemoryBlockList(m_freeBlocks, callback, pool, param);
    if (m_activeBlocks == nullptr && m_free == nullptr)
    {
        return;
    }
    m_activeBlocks = _sortByAllocationAddress(m_activeBlocks);
    _moveTopToFree();
    Element* free = hkAlgorithm::sortList(m_free, Element::next, Element::less);
    m_free = free;
    for (Block* b = m_activeBlocks; b != nullptr; b = b->m_next)
    {
        unsigned char* end = b->m_elements + b->m_numElements * m_elementSize;
        for (unsigned char* p = b->m_elements; p < end; p += m_elementSize)
        {
            if ((Element*)p == free)
            {
                callback(p, m_elementSize, false, pool, param);
                free = free->m_next;
            }
            else
            {
                callback(p, m_elementSize, true, pool, param);
            }
        }
    }
}

// 0x00D11EF0 (:688)
// hkMemorySnapshot is S phase (not reimplemented): nothing is recorded.
int hkFreeList::addToSnapshot(hkMemorySnapshot& snap, signed char usage, int parentId)
{
    return 0;
}
