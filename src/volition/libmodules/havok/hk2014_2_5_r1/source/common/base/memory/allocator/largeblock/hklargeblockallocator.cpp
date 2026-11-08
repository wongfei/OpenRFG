// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/largeblock/hklargeblockallocator.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the large block allocator, a dlmalloc-style allocator (Doug Lea's tree bins
// only) over pages from a server allocator. The game builds it over one fixed block (havok_memory_init: all of
// Havok_mempool), served by the embedded FixedMemoryBlockServer, which hands out its whole range as one page.
//
// Chunks: prevFoot (size of the previous chunk when it is free), head (size | CINUSE 2 | PINUSE 1), then the payload
// at +16 (chunks are 16-byte aligned, sizes multiples of 16, at least 256: MIN_LARGE_SIZE). Free chunks are kept in
// 32 size-indexed bitwise tries (m_treebins, m_treemap); the last chunk of the newest page is the top chunk
// (m_top, m_topsize), whose header is only written on demand (_makeTopValid). Each page ends with a footer chunk of
// 16 bytes whose head links to the next page's first chunk.
//
// Allocation: the best fit from the tries, otherwise the top (leaving at least 256 bytes), otherwise the fixed page
// grows (resize) or a new page is taken from the server; when that fails the limited memory listener may free
// memory (cannotAllocate) for one more try, then is told about the failure (allocationFailure).
//
// The ExtendedInterface virtuals that use members (getApproxTotalAllocated, canAllocTotal, garbageCollect, walkMemory,
// addToSnapshot) are entered with `this` at the interface (+4); the decompiler shows their members 4 bytes early.
//
// Notes (decompiled behaviour, kept as is):
// - FixedMemoryBlockServer::bufFree sets the break to null instead of back to the start, so the fixed block cannot
//   be handed out again; any other size breaks into the debugger (int3).
// - getMemorySoftLimit returns 1 and setMemorySoftLimit fails: the soft limit lives in the heap allocator.
// - _resizeSingleBlockServerPage moves the footer down by the size difference from the old footer head (2 for the
//   only page), so the footer head gets (2 & ~3) - delta | 2.
// Not reimplemented: addToSnapshot (hkMemorySnapshot is S phase) records nothing.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/largeblock/hklargeblockallocator.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include <intrin.h>
#include <stdint.h>

namespace
{
const unsigned int PINUSE_BIT = 1;
const unsigned int CINUSE_BIT = 2;
const unsigned int INUSE_BITS = 3;
const unsigned int SIZE_MASK = ~INUSE_BITS;
const unsigned int CHUNK_OVERHEAD = 16; // the chunk header in front of the payload (keeps payloads 16-byte aligned)
const unsigned int MIN_CHUNK = 256; // MIN_LARGE_SIZE: the smallest chunk and the smallest top left over

typedef hkLargeBlockAllocator::MemChunk MemChunk;
typedef hkLargeBlockAllocator::MemTreeChunk MemTreeChunk;
typedef hkLargeBlockAllocator::MemPage MemPage;

MemChunk* chunkAt(void* p, unsigned int offset)
{
    return (MemChunk*)((char*)p + offset);
}
}

// ---- statics (hklargeblockallocator.h)
const unsigned int hkLargeBlockAllocator::MemChunk::PINUSE_BIT = 1; // 0x012F9370
const unsigned int hkLargeBlockAllocator::MemChunk::CINUSE_BIT = 2; // 0x012F9374
const unsigned int hkLargeBlockAllocator::MemChunk::INUSE_BITS = 3; // 0x012F9378
const unsigned int hkLargeBlockAllocator::MemChunk::ALIGN = 16; // 0x012F937C
const unsigned int hkLargeBlockAllocator::MemChunk::ALIGN_MASK = 15; // 0x012F9380
const unsigned int hkLargeBlockAllocator::MemChunk::PAYLOAD_OFFSET = 16; // 0x012F9384
const unsigned int hkLargeBlockAllocator::NTREEBINS = 32; // 0x012F9388
const unsigned int hkLargeBlockAllocator::TREEBIN_SHIFT = 8; // 0x012F938C
const unsigned int hkLargeBlockAllocator::MIN_LARGE_SIZE = 256; // 0x012F9390
const unsigned int hkLargeBlockAllocator::SIZE_T_BITSIZE = 32; // 0x012F9394

// ---- MemChunk (hklargeblockallocator.h:74-154)
// 0x00CF5E30
bool hkLargeBlockAllocator::MemChunk::isPinuse() const
{
    return (head & PINUSE_BIT) != 0;
}

// 0x00CF5E40
bool hkLargeBlockAllocator::MemChunk::isInuse() const
{
    return (head & CINUSE_BIT) != 0;
}

// 0x00CF5E50
unsigned int hkLargeBlockAllocator::MemChunk::getChunkSize() const
{
    return head & SIZE_MASK;
}

// 0x00CF5E60
void hkLargeBlockAllocator::MemChunk::clearPinuse()
{
    head &= ~PINUSE_BIT;
}

// 0x00CF5E70
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemChunk::nextChunk()
{
    return chunkAt(this, head & SIZE_MASK);
}

// 0x00CF5E80
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemChunk::previousChunk()
{
    return (MemChunk*)((char*)this - prevFoot);
}

// 0x00CF5E90
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemChunk::chunkPlusOffset(unsigned int s)
{
    return chunkAt(this, s);
}

// 0x00CF5EA0
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemChunk::chunkMinusOffset(unsigned int s)
{
    return (MemChunk*)((char*)this - s);
}

// 0x00CF5EB0
void* hkLargeBlockAllocator::MemChunk::getPayload()
{
    return (char*)this + CHUNK_OVERHEAD;
}

// 0x00CF5EC0
unsigned int hkLargeBlockAllocator::MemChunk::getPayloadSize()
{
    return (head & SIZE_MASK) - CHUNK_OVERHEAD;
}

// 0x00CF5ED0
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemChunk::toChunk(void* in)
{
    return (MemChunk*)((char*)in - CHUNK_OVERHEAD);
}

// 0x00CF5EE0
void hkLargeBlockAllocator::MemChunk::setInuseAndPinuse(unsigned int s)
{
    head = s | INUSE_BITS;
    chunkAt(this, s)->head |= PINUSE_BIT;
}

// 0x00CF5F00
void hkLargeBlockAllocator::MemChunk::setSizeAndPinuseOfInuseChunk(unsigned int s)
{
    head = s | INUSE_BITS;
}

// 0x00CF5F10
void hkLargeBlockAllocator::MemChunk::setFoot(unsigned int s)
{
    chunkAt(this, s)->prevFoot = s;
}

// 0x00CF5F20
void hkLargeBlockAllocator::MemChunk::setSizeAndPinuseOfFreeChunk(unsigned int s)
{
    head = s | PINUSE_BIT;
    chunkAt(this, s)->prevFoot = s;
}

// 0x00CF5F40
void hkLargeBlockAllocator::MemChunk::setFreeWithPinuse(unsigned int s, MemChunk* n)
{
    n->head &= ~PINUSE_BIT;
    head = s | PINUSE_BIT;
    chunkAt(this, s)->prevFoot = s;
}

// 0x00CF5F60
bool hkLargeBlockAllocator::MemChunk::isAligned(const void* ptr)
{
    return ((uintptr_t)ptr & 15) == 0;
}

// 0x00CF5F70 (.h:182)
hkLargeBlockAllocator::MemTreeChunk* hkLargeBlockAllocator::MemTreeChunk::leftMostChild() const
{
    return child[0] != nullptr ? child[0] : child[1];
}

// ---- MemPage (.h:205-218)
// 0x00CF5F80
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemPage::getFirstChunk() const
{
    return (MemChunk*)m_start;
}

// 0x00CF5F90
hkLargeBlockAllocator::MemChunk* hkLargeBlockAllocator::MemPage::getFooter() const
{
    return (MemChunk*)(m_end - CHUNK_OVERHEAD);
}

// 0x00CF5FA0
unsigned int hkLargeBlockAllocator::MemPage::getMaxChunkSize() const
{
    return (unsigned int)(m_end - CHUNK_OVERHEAD - m_start);
}

// 0x00CF5FB0
char* hkLargeBlockAllocator::MemPage::getStart() const
{
    return m_start;
}

// 0x00CF5FC0
char* hkLargeBlockAllocator::MemPage::getEnd()
{
    return m_end;
}

// ---- Iterator (.h:358-365)
// 0x00CF5FE0
hkLargeBlockAllocator::Iterator::Iterator(MemChunk* chunk, MemPage* page, MemChunk* end)
    : m_chunk(chunk)
    , m_page(page)
    , m_end(end)
{
}

// 0x00CF6000
hkLargeBlockAllocator::Iterator::Iterator()
    : m_chunk(nullptr)
{
}

// 0x00CF73F0
hkBool hkLargeBlockAllocator::Iterator::isValid() const
{
    hkBool result;
    result.m_bool = m_chunk != nullptr;
    return result;
}

// 0x00CF6010
unsigned int hkLargeBlockAllocator::Iterator::getSize() const
{
    return m_chunk->head & SIZE_MASK;
}

// 0x00CF6020
void* hkLargeBlockAllocator::Iterator::getAddress() const
{
    return (char*)m_chunk + CHUNK_OVERHEAD;
}

// 0x00CF7400
hkBool hkLargeBlockAllocator::Iterator::isInuse() const
{
    hkBool result;
    result.m_bool = (char)((m_chunk->head >> 1) & 1);
    return result;
}

// ---- FixedMemoryBlockServer
// 0x00CF70E0 (:26)
// The block is shrunk to 16-byte boundaries.
hkLargeBlockAllocator::FixedMemoryBlockServer::FixedMemoryBlockServer(void* startIn, int size)
{
    char* start = (char*)(((uintptr_t)startIn + 15) & ~(uintptr_t)15);
    char* end = (char*)(((uintptr_t)startIn + size) & ~(uintptr_t)15);
    m_start = start;
    m_end = end;
    m_limit = end;
    m_break = start;
}

// 0x00CF61E0
hkLargeBlockAllocator::FixedMemoryBlockServer::~FixedMemoryBlockServer()
{
}

// 0x00CF7430 (.h:233)
void* hkLargeBlockAllocator::FixedMemoryBlockServer::blockAlloc(int nbytes)
{
    return nullptr;
}

// 0x00CF7440 (.h:234)
void hkLargeBlockAllocator::FixedMemoryBlockServer::blockFree(void* p, int nbytes)
{
}

// 0x00CF61F0 (:39)
// The whole block, once.
void* hkLargeBlockAllocator::FixedMemoryBlockServer::bufAlloc(int& size)
{
    char* start = m_start;
    if (m_break != start)
    {
        return nullptr;
    }
    const int available = (int)(m_limit - start);
    if (available < size)
    {
        return nullptr;
    }
    m_break = start + available;
    size = available;
    return m_start;
}

// 0x00CF6250 (:83)
void hkLargeBlockAllocator::FixedMemoryBlockServer::bufFree(void* data, int size)
{
    if (m_break != m_start && (int)(m_break - m_start) == size)
    {
        m_break = nullptr; // see the notes
        return;
    }
    __debugbreak(); // HK_BREAKPOINT
}

// 0x00CF6220 (:59)
bool hkLargeBlockAllocator::FixedMemoryBlockServer::resize(void* data, int oldSize, unsigned int newSize, unsigned int& sizeOut)
{
    char* start = m_start;
    if (start == data)
    {
        char* newBreak = start + newSize;
        if (start <= newBreak && newBreak <= m_limit)
        {
            m_break = newBreak;
            sizeOut = (unsigned int)(newBreak - start);
            return true;
        }
    }
    return false;
}

// 0x00CF6270 (:96)
void hkLargeBlockAllocator::FixedMemoryBlockServer::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const
{
    const long used = (long)(m_break - m_start);
    stats.m_allocated = used;
    stats.m_inUse = used;
    stats.m_peakInUse = used;
    const long left = (long)(m_end - m_break);
    stats.m_available = left;
    stats.m_totalAvailable = left;
    stats.m_largestBlock = (long)(m_end - m_start);
}

// 0x00CF5FD0 (.h:238)
bool hkLargeBlockAllocator::FixedMemoryBlockServer::inUse()
{
    return m_start != nullptr;
}

// 0x00CF7450 (.h:235)
int hkLargeBlockAllocator::FixedMemoryBlockServer::getAllocatedSize(const void* obj, int nbytes) const
{
    return nbytes;
}

// ---- LimitedMemoryListener
// 0x00811A90
hkLargeBlockAllocator::LimitedMemoryListener::LimitedMemoryListener()
{
}

// 0x00811970 (.h:260)
hkLargeBlockAllocator::LimitedMemoryListener::~LimitedMemoryListener()
{
}

// ---- construction
// 0x00CF7110 (:181)
hkLargeBlockAllocator::hkLargeBlockAllocator(hkMemoryAllocator* server)
    : m_fixedBlockServer(nullptr, 0)
{
    m_limitedListener = nullptr;
    m_server = server;
    _init();
}

// 0x00CF7160 (:189)
// Over a fixed block (havok_memory_init).
hkLargeBlockAllocator::hkLargeBlockAllocator(void* block, int size)
    : m_fixedBlockServer(block, size)
{
    m_limitedListener = nullptr;
    m_server = &m_fixedBlockServer;
    _init();
}

// 0x00CF62A0 (:194)
hkLargeBlockAllocator::~hkLargeBlockAllocator()
{
    MemPage* page = m_pages.m_next;
    while (page != &m_pages)
    {
        MemPage* next = page->m_next;
        m_server->bufFree(page, page->m_size);
        page = next;
    }
}

// 0x00CF6300 (:209)
void hkLargeBlockAllocator::_init()
{
    m_sumAllocatedSize = 0;
    m_sumAllocatedWithMgrOverhead = 0;
    m_treemap = 0;
    m_leastAddr = nullptr;
    for (int i = 0; i < 32; i++)
    {
        m_treebins[i] = nullptr;
    }
    m_zeroChunk.prevFoot = 0;
    m_topsize = 0;
    m_zeroChunk.head = INUSE_BITS;
    m_top = &m_zeroChunk;
    m_pages.m_next = &m_pages;
    m_pages.m_prev = &m_pages;
}

// 0x00CF63B0 (:238)
void hkLargeBlockAllocator::freeAll()
{
    MemPage* page = m_pages.m_next;
    while (page != &m_pages)
    {
        MemPage* next = page->m_next;
        m_server->bufFree(page, page->m_size);
        page = next;
    }
    _init();
}

// ---- tree bins (.h:414-507)
// 0x00CF6030
void hkLargeBlockAllocator::_markTreeMap(unsigned int i)
{
    m_treemap |= 1u << (i & 31);
}

// 0x00CF6050
void hkLargeBlockAllocator::_clearTreeMap(unsigned int i)
{
    m_treemap &= ~(1u << (i & 31));
}

// 0x00CF6070
bool hkLargeBlockAllocator::_isTreeMapMarked(unsigned int i) const
{
    return ((1u << (i & 31)) & m_treemap) != 0;
}

// 0x00CF6090 (.h:431)
// dlmalloc's compute_tree_index: two bins per power of two from 256 bytes, the last bin (31) for 16 MB and up.
unsigned int hkLargeBlockAllocator::_computeTreeIndex(unsigned int s)
{
    unsigned int x = s >> TREEBIN_SHIFT;
    if (x == 0)
    {
        return 0;
    }
    if (x > 0xFFFF)
    {
        return NTREEBINS - 1;
    }
    unsigned int y = x;
    unsigned int n = ((y - 0x100) >> 16) & 8;
    unsigned int k = (((y <<= n) - 0x1000) >> 16) & 4;
    n += k;
    n += k = (((y <<= k) - 0x4000) >> 16) & 2;
    k = 14 - n + ((y <<= k) >> 15);
    return (k << 1) + ((s >> (k + (TREEBIN_SHIFT - 1)) & 1));
}

// 0x00CF6100 (.h:446)
// The index of the single set bit of x.
unsigned int hkLargeBlockAllocator::_bitToIndex(unsigned int x)
{
    unsigned int y = x - 1;
    unsigned int k = (y >> 12) & 16;
    unsigned int n = k;
    y >>= k;
    n += k = (y >> 5) & 8;
    y >>= k;
    n += k = (y >> 2) & 4;
    y >>= k;
    n += k = (y >> 1) & 2;
    y >>= k;
    n += k = (y >> 1) & 1;
    y >>= k;
    return n + y;
}

// 0x00CF6150 (.h:459)
unsigned int hkLargeBlockAllocator::_padRequest(unsigned int req)
{
    return (req + CHUNK_OVERHEAD + 15) & ~15u;
}

// 0x00CF6160 (.h:470)
int hkLargeBlockAllocator::_leftShiftForTreeIndex(int i)
{
    return i == (int)NTREEBINS - 1 ? 0 : (int)(SIZE_T_BITSIZE - 1) - ((i >> 1) + (int)TREEBIN_SHIFT - 2);
}

// 0x00CF6180 (.h:487)
bool hkLargeBlockAllocator::_isOkAddress(const void* a) const
{
    return a >= m_leastAddr;
}

// 0x00CF6190 (.h:490)
int hkLargeBlockAllocator::_leastBit(int x)
{
    return x & -x;
}

// 0x00CF61A0 (.h:493)
int hkLargeBlockAllocator::_leftBits(int x)
{
    return (x << 1) | -(x << 1);
}

// 0x00CF61B0 (.h:496)
int hkLargeBlockAllocator::_indexToBit(int x)
{
    return 1 << (x & 31);
}

// 0x00CF61C0 (.h:507)
hkBool hkLargeBlockAllocator::_hasPages()
{
    hkBool result;
    result.m_bool = m_pages.m_next != &m_pages;
    return result;
}

// 0x00CF6400 (:256)
// dlmalloc's insert_large_chunk: a chunk of a size already in the trie joins that node's ring.
void hkLargeBlockAllocator::_insertLargeChunk(MemTreeChunk* x, unsigned int s)
{
    const unsigned int idx = _computeTreeIndex(s);
    MemTreeChunk** h = &m_treebins[idx];
    x->index = idx;
    x->child[0] = nullptr;
    x->child[1] = nullptr;
    if (!_isTreeMapMarked(idx))
    {
        _markTreeMap(idx);
        *h = x;
        x->parent = (MemTreeChunk*)h; // the bin slot stands in for the parent
        x->next = x;
        x->prev = x;
        return;
    }
    MemTreeChunk* t = *h;
    unsigned int k = s << _leftShiftForTreeIndex(idx);
    for (;;)
    {
        if ((t->head & SIZE_MASK) == s)
        {
            FreeMemChunk* f = t->next;
            f->prev = x;
            t->next = x;
            x->prev = t;
            x->next = f;
            x->parent = nullptr;
            return;
        }
        MemTreeChunk** c = &t->child[(k >> 31) & 1];
        k <<= 1;
        if (*c == nullptr)
        {
            *c = x;
            x->parent = t;
            x->next = x;
            x->prev = x;
            return;
        }
        t = *c;
    }
}

// 0x00CF64C0 (:338)
// dlmalloc's unlink_large_chunk: a ring member is unlinked; a trie node is replaced by its rightmost leaf.
void hkLargeBlockAllocator::_unlinkLargeChunk(MemTreeChunk* x)
{
    MemTreeChunk* xp = x->parent;
    MemTreeChunk* r;
    if (x->prev != x)
    {
        FreeMemChunk* f = x->next;
        r = (MemTreeChunk*)x->prev;
        f->prev = r;
        r->next = f;
    }
    else
    {
        MemTreeChunk** rp;
        if (((r = *(rp = &x->child[1])) != nullptr) || ((r = *(rp = &x->child[0])) != nullptr))
        {
            MemTreeChunk** cp;
            while ((*(cp = &r->child[1]) != nullptr) || (*(cp = &r->child[0]) != nullptr))
            {
                r = *(rp = cp);
            }
            *rp = nullptr;
        }
    }
    if (xp == nullptr)
    {
        return;
    }
    MemTreeChunk** h = &m_treebins[x->index];
    if (x == *h)
    {
        if ((*h = r) == nullptr)
        {
            _clearTreeMap(x->index);
            return;
        }
    }
    else
    {
        if (xp->child[0] == x)
        {
            xp->child[0] = r;
        }
        else
        {
            xp->child[1] = r;
        }
        if (r == nullptr)
        {
            return;
        }
    }
    r->parent = xp;
    MemTreeChunk* c0 = x->child[0];
    if (c0 != nullptr)
    {
        r->child[0] = c0;
        c0->parent = r;
    }
    MemTreeChunk* c1 = x->child[1];
    if (c1 != nullptr)
    {
        r->child[1] = c1;
        c1->parent = r;
    }
}

// 0x00CF6570 (:420)
// dlmalloc's tmalloc_large: the smallest free chunk of at least nb bytes; the remainder goes back to the tries when
// it is a chunk of its own (256 bytes or more), otherwise it stays in the allocation.
void* hkLargeBlockAllocator::_allocLarge(unsigned int nb)
{
    MemTreeChunk* v = nullptr;
    unsigned int rsize = 0u - nb;
    MemTreeChunk* t;
    const unsigned int idx = _computeTreeIndex(nb);
    if ((t = m_treebins[idx]) != nullptr)
    {
        unsigned int sizebits = nb << _leftShiftForTreeIndex(idx);
        MemTreeChunk* rst = nullptr; // the deepest untaken right subtree
        for (;;)
        {
            const unsigned int trem = (t->head & SIZE_MASK) - nb;
            if (trem < rsize)
            {
                v = t;
                if ((rsize = trem) == 0)
                {
                    break;
                }
            }
            MemTreeChunk* rt = t->child[1];
            t = t->child[(sizebits >> 31) & 1];
            if (rt != nullptr && rt != t)
            {
                rst = rt;
            }
            if (t == nullptr)
            {
                t = rst;
                break;
            }
            sizebits <<= 1;
        }
    }
    if (t == nullptr && v == nullptr)
    {
        const unsigned int leftbits = (unsigned int)_leftBits(_indexToBit(idx)) & m_treemap;
        if (leftbits != 0)
        {
            t = m_treebins[_bitToIndex((unsigned int)_leastBit(leftbits))];
        }
    }
    while (t != nullptr)
    {
        const unsigned int trem = (t->head & SIZE_MASK) - nb;
        if (trem < rsize)
        {
            rsize = trem;
            v = t;
        }
        t = t->leftMostChild();
    }
    if (v == nullptr)
    {
        return nullptr;
    }
    MemChunk* r = chunkAt(v, nb);
    _unlinkLargeChunk(v);
    if (rsize < MIN_CHUNK)
    {
        v->setInuseAndPinuse(rsize + nb);
        m_sumAllocatedWithMgrOverhead += rsize + nb;
        m_sumAllocatedSize += rsize - CHUNK_OVERHEAD + nb;
        return v->getPayload();
    }
    v->head = nb | INUSE_BITS;
    m_sumAllocatedWithMgrOverhead += nb;
    m_sumAllocatedSize += nb - CHUNK_OVERHEAD;
    r->head = rsize | PINUSE_BIT;
    chunkAt(r, rsize)->prevFoot = rsize;
    _insertLargeChunk((MemTreeChunk*)r, rsize);
    return v->getPayload();
}

// 0x00CF6700 (:545)
// The largest chunk on the rightmost path of a trie.
unsigned int hkLargeBlockAllocator::_findLargestTreeBlockSize(MemTreeChunk* t, unsigned int largest) const
{
    while (t != nullptr)
    {
        const unsigned int size = t->head & SIZE_MASK;
        if (largest < size)
        {
            largest = size;
        }
        t = t->child[1] != nullptr ? t->child[1] : t->child[0];
    }
    return largest;
}

// 0x00CF6730 (:566)
unsigned int hkLargeBlockAllocator::findLargestBlockSize() const
{
    const unsigned int topsize = m_topsize;
    const int idx = (int)_computeTreeIndex(topsize);
    for (int i = (int)NTREEBINS - 1; i >= idx; i--)
    {
        if (m_treebins[i] != nullptr)
        {
            return _findLargestTreeBlockSize(m_treebins[i], topsize);
        }
    }
    return topsize;
}

// 0x00CF6780 (:590)
void hkLargeBlockAllocator::_makeTopValid() const
{
    m_top->head = m_topsize | PINUSE_BIT;
    chunkAt(m_top, m_topsize)->prevFoot = m_topsize;
}

// 0x00CF67A0 (:598)
void* hkLargeBlockAllocator::_allocFromTop(unsigned int nb)
{
    m_sumAllocatedWithMgrOverhead += nb;
    m_sumAllocatedSize += nb - CHUNK_OVERHEAD;
    MemChunk* p = m_top;
    MemChunk* r = chunkAt(p, nb);
    const unsigned int rsize = m_topsize - nb;
    m_topsize = rsize;
    m_top = r;
    r->prevFoot = nb;
    r->head = rsize | PINUSE_BIT;
    p->head = nb | INUSE_BITS;
    return p->getPayload();
}

// 0x00CF67E0 (:628)
// Grows or shrinks the only page of the fixed block server to newSize bytes; the top takes the difference.
hkBool hkLargeBlockAllocator::_resizeSingleBlockServerPage(unsigned int newSize)
{
    hkBool result;
    MemPage* page = m_pages.m_next;
    unsigned int sizeOut = 0;
    if (!m_fixedBlockServer.resize(page, page->m_size, newSize, sizeOut))
    {
        result.m_bool = 0;
        return result;
    }
    const unsigned int footerHead = chunkAt(m_top, m_topsize)->head;
    const unsigned int delta = (sizeOut - page->m_size) & ~15u;
    m_topsize += delta;
    page->m_end += delta;
    page->m_size = sizeOut;
    chunkAt(m_top, m_topsize)->head = ((footerHead & SIZE_MASK) - delta) | CINUSE_BIT; // see the notes
    result.m_bool = 1;
    return result;
}

// 0x00CF6860 (:659)
void* hkLargeBlockAllocator::_alloc(unsigned int nb)
{
    if (m_treemap != 0)
    {
        void* p = _allocLarge(nb);
        if (p != nullptr)
        {
            return p;
        }
    }
    if (m_topsize < nb + MIN_CHUNK)
    {
        return nullptr;
    }
    return _allocFromTop(nb);
}

// 0x00CF68A0 (:684)
void* hkLargeBlockAllocator::blockAlloc(int bytes)
{
    unsigned int nb = _padRequest((unsigned int)bytes);
    if (nb < MIN_CHUNK)
    {
        nb = MIN_CHUNK;
    }
    void* p = _alloc(nb);
    if (p != nullptr)
    {
        return p;
    }

    // the fixed block's page can only grow in place
    if (m_fixedBlockServer.m_start != nullptr && m_pages.m_next != &m_pages)
    {
        MemPage* page = m_pages.m_next;
        if (_resizeSingleBlockServerPage(page->m_size + nb))
        {
            return _allocFromTop(nb);
        }
        if (m_limitedListener != nullptr)
        {
            m_limitedListener->cannotAllocate(nb);
            p = _alloc(nb);
            if (p != nullptr)
            {
                return p;
            }
            if (_resizeSingleBlockServerPage(page->m_size + nb))
            {
                return _allocFromTop(nb);
            }
            m_limitedListener->allocationFailure(nb);
        }
        return nullptr;
    }

    // a new page: the page header, the chunk, the footer and alignment slack
    int pageSize = (int)nb + 0x138; // 0x138: header (0x18) + footer (0x10) + minimum top (0x100) + alignment (0x10)
    MemPage* page = (MemPage*)m_server->bufAlloc(pageSize);
    if (page == nullptr)
    {
        if (m_limitedListener == nullptr)
        {
            return nullptr;
        }
        m_limitedListener->cannotAllocate(nb);
        p = _alloc(nb);
        if (p != nullptr)
        {
            return p;
        }
        pageSize = (int)nb + 0x138;
        page = (MemPage*)m_server->bufAlloc(pageSize);
        if (page == nullptr)
        {
            m_limitedListener->allocationFailure(nb);
            return nullptr;
        }
    }
    page->m_numAllocs = 0;
    page->m_size = pageSize;
    page->m_end = (char*)(((uintptr_t)page + pageSize) & ~(uintptr_t)15);
    page->m_start = (char*)(((uintptr_t)(page + 1) + 15) & ~(uintptr_t)15);

    // pages are kept sorted by address
    MemPage* next = m_pages.m_next;
    while (next != &m_pages && next < page)
    {
        next = next->m_next;
    }
    page->m_next = next;
    page->m_prev = next->m_prev;
    next->m_prev->m_next = page;
    next->m_prev = page;

    if (m_top != &m_zeroChunk)
    {
        // the old top becomes an ordinary free chunk
        _makeTopValid();
        _insertLargeChunk((MemTreeChunk*)m_top, m_top->head & SIZE_MASK);
    }
    m_topsize = (unsigned int)(page->m_end - CHUNK_OVERHEAD - page->m_start);
    m_top = (MemChunk*)page->m_start;
    _makeTopValid();
    MemChunk* footer = (MemChunk*)(page->m_end - CHUNK_OVERHEAD);
    footer->head = CINUSE_BIT;
    if (page->m_next != &m_pages)
    {
        footer->head = (unsigned int)(page->m_next->m_start - (char*)footer) | CINUSE_BIT;
    }
    if (page->m_prev != &m_pages)
    {
        MemChunk* prevFooter = (MemChunk*)(page->m_prev->m_end - CHUNK_OVERHEAD);
        prevFooter->head = (unsigned int)((char*)m_top - (char*)prevFooter) | (prevFooter->head & INUSE_BITS);
    }
    return _allocFromTop(nb);
}

// 0x00CF6D60 (:1086)
// Coalesces with free neighbours (into the top when it is the next chunk) and puts the chunk into the tries.
void hkLargeBlockAllocator::blockFree(void* mem, int)
{
    if (mem == nullptr)
    {
        return;
    }
    MemChunk* p = MemChunk::toChunk(mem);
    unsigned int psize = p->head & SIZE_MASK;
    m_sumAllocatedWithMgrOverhead -= psize;
    m_sumAllocatedSize += CHUNK_OVERHEAD - psize;
    MemChunk* next = chunkAt(p, psize);
    if (!p->isPinuse())
    {
        const unsigned int prevsize = p->prevFoot;
        p = (MemChunk*)((char*)p - prevsize);
        _unlinkLargeChunk((MemTreeChunk*)p);
        psize += prevsize;
    }
    const unsigned int nhead = next->head;
    if ((nhead & CINUSE_BIT) == 0)
    {
        if (next == m_top)
        {
            m_topsize += psize;
            m_top = p;
            p->head = m_topsize | PINUSE_BIT;
            return;
        }
        psize += nhead & SIZE_MASK;
        _unlinkLargeChunk((MemTreeChunk*)next);
        p->head = psize | PINUSE_BIT;
        chunkAt(p, psize)->prevFoot = psize;
        _insertLargeChunk((MemTreeChunk*)p, psize);
        return;
    }
    next->head = nhead & ~PINUSE_BIT;
    p->head = psize | PINUSE_BIT;
    chunkAt(p, psize)->prevFoot = psize;
    _insertLargeChunk((MemTreeChunk*)p, psize);
}

// 0x00CF6E30 (:1171)
int hkLargeBlockAllocator::getAllocatedSize(const void* mem, int size) const
{
    return (int)((((const MemChunk*)((const char*)mem - CHUNK_OVERHEAD))->head & SIZE_MASK) - CHUNK_OVERHEAD);
}

// 0x00CF73C0 (.h:303)
hkMemoryAllocator::ExtendedInterface* hkLargeBlockAllocator::getExtendedInterface()
{
    return this;
}

// 0x00CF7330 (:1274)
// The pages are allocated, the user sizes are in use; the largest block is the largest free chunk or the top.
void hkLargeBlockAllocator::getMemoryStatistics(hkMemoryAllocator::MemoryStatistics& stats) const
{
    const unsigned int allocated = calculateMemoryUsedByThisAllocator();
    stats.m_allocated = allocated;
    stats.m_inUse = m_sumAllocatedSize;
    stats.m_available = allocated - m_sumAllocatedSize;
    stats.m_largestBlock = findLargestBlockSize();
    stats.m_totalAvailable = 0;
}

// ---- ExtendedInterface
// 0x00CF6AB0 (:837)
void hkLargeBlockAllocator::incrementalGarbageCollect(int numBlocks)
{
}

// 0x00CF6AC0 (:842)
hkResult hkLargeBlockAllocator::setMemorySoftLimit(unsigned int maxMemory)
{
    return hkResult(HK_FAILURE);
}

// 0x00CF6AD0 (:847)
unsigned int hkLargeBlockAllocator::getMemorySoftLimit() const
{
    return 1; // see the notes
}

// 0x00CF6AE0 (:852)
// The user sizes, without the chunk headers (the heap allocator's bytes in use are this less its free elements).
unsigned int hkLargeBlockAllocator::getApproxTotalAllocated() const
{
    return m_sumAllocatedSize;
}

// 0x00CF6AF0 (:857)
void hkLargeBlockAllocator::setScrubValues(unsigned int allocValue, unsigned int freeValue)
{
}

// 0x00CF6B00 (:862)
// Tries the allocation.
bool hkLargeBlockAllocator::canAllocTotal(int numBytes)
{
    void* p = blockAlloc(numBytes);
    if (p != nullptr)
    {
        blockFree(p, numBytes);
        return true;
    }
    return false;
}

// 0x00CF6B40 (:916)
// Gives back the pages that are one free chunk. With a page server, the largest free chunk ending a page becomes
// the top; with the fixed block, its page shrinks to the top's end plus 0x120 bytes when the top holds 32 KB or more.
void hkLargeBlockAllocator::garbageCollect()
{
    _makeTopValid();
    MemPage* page = m_pages.m_next;
    while (page != &m_pages)
    {
        MemTreeChunk* first = (MemTreeChunk*)page->m_start;
        const unsigned int head = first->head;
        if ((head & CINUSE_BIT) == 0 && (char*)chunkAt(first, head & SIZE_MASK) == page->m_end - CHUNK_OVERHEAD)
        {
            MemPage* next = page->m_next;
            page->m_prev->m_next = next;
            page->m_next->m_prev = page->m_prev;
            if ((MemChunk*)first == m_top)
            {
                m_top = &m_zeroChunk;
                m_topsize = 0;
            }
            else
            {
                _unlinkLargeChunk(first);
            }
            m_server->bufFree(page, page->m_size);
            page = next;
        }
        else
        {
            page = page->m_next;
        }
    }
    if (m_pages.m_next == &m_pages)
    {
        m_top = &m_zeroChunk;
        m_topsize = 0;
        return;
    }
    if (m_fixedBlockServer.m_start == nullptr)
    {
        if (m_top != &m_zeroChunk)
        {
            _insertLargeChunk((MemTreeChunk*)m_top, m_topsize);
            m_top = &m_zeroChunk;
            m_topsize = 0;
        }
        MemTreeChunk* best = nullptr;
        for (page = m_pages.m_next; page != &m_pages; page = page->m_next)
        {
            MemChunk* footer = (MemChunk*)(page->m_end - CHUNK_OVERHEAD);
            if ((footer->head & PINUSE_BIT) == 0)
            {
                MemTreeChunk* last = (MemTreeChunk*)((char*)footer - footer->prevFoot);
                if (best == nullptr || (best->head & SIZE_MASK) < (last->head & SIZE_MASK))
                {
                    best = last;
                }
            }
        }
        if (best != nullptr)
        {
            _unlinkLargeChunk(best);
            m_top = best;
            m_topsize = best->head & SIZE_MASK;
            return;
        }
        m_top = &m_zeroChunk;
        m_topsize = 0;
    }
    else if (m_topsize > 0x7FFF) // 32 KB
    {
        _resizeSingleBlockServerPage((unsigned int)((char*)m_top + 0x120 - m_pages.m_next->m_start)); // 0x120: page header slack + footer + minimum top
    }
}

// 0x00CF71B0 (:875)
hkResult hkLargeBlockAllocator::walkMemory(void (*callback)(void*, unsigned int, bool, int, void*), void* param)
{
    Iterator it = getIterator();
    while (it.m_chunk != nullptr)
    {
        callback((char*)it.m_chunk + CHUNK_OVERHEAD, it.m_chunk->head & SIZE_MASK, ((it.m_chunk->head >> 1) & 1) != 0, 0, param);
        nextBlock(it);
    }
    return hkResult(HK_SUCCESS);
}

// 0x00CF7220 (:888)
// hkMemorySnapshot is S phase (not reimplemented): nothing is recorded.
int hkLargeBlockAllocator::addToSnapshot(hkMemorySnapshot& snap, int parentId)
{
    return -1;
}

// ---- inspection
// 0x00CF6CC0 (:1036)
hkBool hkLargeBlockAllocator::isValidAlloc(void* in)
{
    hkBool result;
    result.m_bool = 0;
    if (((uintptr_t)in & 15) != 0 || (((MemChunk*)((char*)in - CHUNK_OVERHEAD))->head & CINUSE_BIT) == 0)
    {
        return result;
    }
    for (MemPage* page = m_pages.m_next; page != &m_pages; page = page->m_next)
    {
        if (page->m_start <= (char*)in && (char*)in < page->m_end)
        {
            char* footer = page->m_end - CHUNK_OVERHEAD;
            for (char* c = page->m_start; c != footer; c += ((MemChunk*)c)->head & SIZE_MASK)
            {
                if (c == (char*)in - CHUNK_OVERHEAD)
                {
                    result.m_bool = 1;
                    return result;
                }
            }
            return result;
        }
    }
    return result;
}

// 0x00CF6E40 (:1178)
void hkLargeBlockAllocator::forAllAllocs(void (*callback)(void*, unsigned int, void*), void* param)
{
    m_top->head = m_topsize | PINUSE_BIT;
    for (MemPage* page = m_pages.m_next; page != &m_pages; page = page->m_next)
    {
        char* footer = page->m_end - CHUNK_OVERHEAD;
        for (char* c = page->m_start; c != footer; c += ((MemChunk*)c)->head & SIZE_MASK)
        {
            const unsigned int head = ((MemChunk*)c)->head;
            if ((head & CINUSE_BIT) != 0)
            {
                callback(c + CHUNK_OVERHEAD, (head & SIZE_MASK) - CHUNK_OVERHEAD, param);
            }
        }
    }
}

// 0x00CF6EC0 (:1207)
hkLargeBlockAllocator::Iterator hkLargeBlockAllocator::getIterator()
{
    m_top->head = m_topsize | PINUSE_BIT;
    MemPage* page = m_pages.m_next;
    if (page == &m_pages)
    {
        return Iterator();
    }
    return Iterator((MemChunk*)page->m_start, page, (MemChunk*)(page->m_end - CHUNK_OVERHEAD));
}

// 0x00CF6F00 (:1221)
hkBool hkLargeBlockAllocator::nextBlock(Iterator& iter)
{
    hkBool result;
    result.m_bool = 0;
    if (iter.m_chunk == nullptr)
    {
        return result;
    }
    iter.m_chunk = chunkAt(iter.m_chunk, iter.m_chunk->head & SIZE_MASK);
    if (iter.m_chunk == iter.m_end)
    {
        MemPage* next = iter.m_page->m_next;
        if (next == &m_pages)
        {
            iter.m_chunk = nullptr;
            return result;
        }
        iter.m_page = next;
        iter.m_chunk = (MemChunk*)next->m_start;
        iter.m_end = (MemChunk*)(next->m_end - CHUNK_OVERHEAD);
    }
    result.m_bool = 1;
    return result;
}

// 0x00CF6F60 (:1246)
unsigned int hkLargeBlockAllocator::_calculateUsed() const
{
    unsigned int used = 0;
    _makeTopValid();
    for (const MemPage* page = m_pages.m_next; page != &m_pages; page = page->m_next)
    {
        for (char* c = page->m_start; c != page->m_end - CHUNK_OVERHEAD; c += ((MemChunk*)c)->head & SIZE_MASK)
        {
            const unsigned int head = ((MemChunk*)c)->head;
            if ((head & CINUSE_BIT) != 0)
            {
                used += head & SIZE_MASK;
            }
        }
    }
    return used;
}

// 0x00CF6FC0 (:1284)
unsigned int hkLargeBlockAllocator::calculateMemoryUsedByThisAllocator() const
{
    unsigned int size = 0;
    for (const MemPage* page = m_pages.m_next; page != &m_pages; page = page->m_next)
    {
        size += page->m_size;
    }
    return size;
}

// 0x00CF6FE0 (:1382)
bool hkLargeBlockAllocator::_checkUsedAlloc(const void* mem) const
{
    const MemChunk* p = (const MemChunk*)((const char*)mem - CHUNK_OVERHEAD);
    if (((uintptr_t)mem & 15) != 0 || (const char*)p < m_leastAddr)
    {
        return false;
    }
    const unsigned int head = p->head;
    if ((head & CINUSE_BIT) == 0)
    {
        return false;
    }
    const unsigned int size = head & SIZE_MASK;
    if (size < 16 || (((const MemChunk*)((const char*)p + size))->head & PINUSE_BIT) == 0)
    {
        return false;
    }
    if ((head & PINUSE_BIT) == 0)
    {
        const unsigned int prevHead = ((const MemChunk*)((const char*)p - p->prevFoot))->head;
        if ((prevHead & CINUSE_BIT) != 0 || (prevHead & SIZE_MASK) != p->prevFoot)
        {
            return false;
        }
    }
    return true;
}

// 0x00CF7040 (:1437)
hkBool hkLargeBlockAllocator::_checkFreeChunk(MemChunk* p)
{
    hkBool result;
    const unsigned int size = p->head & SIZE_MASK;
    const unsigned int nextHead = chunkAt(p, size)->head;
    result.m_bool = (((uintptr_t)p & 15) == 0 && (p->head & CINUSE_BIT) == 0 && (char*)p >= m_leastAddr &&
                     (nextHead & PINUSE_BIT) == 0 && (nextHead & CINUSE_BIT) != 0 && (p->head & PINUSE_BIT) != 0)
                        ? 1
                        : 0;
    return result;
}

// 0x00CF7090 (:1483)
hkBool hkLargeBlockAllocator::checkAllocations(void** allocs, int size)
{
    hkBool result;
    result.m_bool = 1;
    for (int i = 0; i < size; i++)
    {
        if (!_checkUsedAlloc(allocs[i]))
        {
            result.m_bool = 0;
            break;
        }
    }
    return result;
}

// 0x00CF7370 (:1495)
// param is an hkArray<void*> (heap allocator).
void hkLargeBlockAllocator::_addAlloc(void* data, unsigned int size, void* param)
{
    ((hkArray<void*, hkContainerHeapAllocator>*)param)->pushBack(data);
}
