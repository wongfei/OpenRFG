// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/1axissweep/hk1axissweep.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the one-axis sweep between two lists of integer boxes sorted by their x
// minimum, from rfg.exe (the version with a fixed pair buffer; the collection-collection agent uses it). Each list
// ends with at least four end markers (x minimum 0xFFFFFFFF), as the scan tests four boxes at a time.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/1axissweep/hk1axissweep.h"

// size 0x8
struct hkKeyPair
{
    unsigned int m_keyA; // 0x00
    unsigned int m_keyB; // 0x04
};

// 2014.1 hk1AxisSweep.h: disjoint on y or z (the sign bit of the four differences).
static HK_FORCE_INLINE unsigned int hk1AxisSweep_yzDisjoint(const hk1AxisSweep::AabbInt& aabb, const hk1AxisSweep::AabbInt& other)
{
    const unsigned int yab = aabb.m_max[1] - other.m_min[1];
    const unsigned int yba = other.m_max[1] - aabb.m_min[1];
    const unsigned int zab = aabb.m_max[2] - other.m_min[2];
    const unsigned int zba = other.m_max[2] - aabb.m_min[2];
    return ((yab | yba) | (zab | zba)) & 0x80000000;
}

static HK_FORCE_INLINE unsigned int hk1AxisSweep_key(const hk1AxisSweep::AabbInt& aabb)
{
    return *(const unsigned int*)((const char*)&aabb + 0xC); // the key in m_min's padding (AabbInt::getKey)
}

// The pair (a, b) when the buffer has room, else counted as skipped.
static HK_FORCE_INLINE hkKeyPair* hk1AxisSweep_appendPair(unsigned int keyA, unsigned int keyB, hkKeyPair* pairOut, const hkKeyPair* end, hkPadSpu<int>& numPairsSkipped)
{
    if (pairOut < end)
    {
        pairOut->m_keyA = keyA;
        pairOut->m_keyB = keyB;
        return pairOut + 1;
    }
    numPairsSkipped = numPairsSkipped + 1;
    return pairOut;
}

// The boxes of list (from 'from', four at a time) starting at or before maxX that overlap query on y and z. flip
// puts the query's key second (the query from list b).
template<int flip>
static HK_FORCE_INLINE hkKeyPair* hk1AxisSweep_scanList(const hk1AxisSweep::AabbInt& query, const hk1AxisSweep::AabbInt* list, unsigned int maxX, hkKeyPair* pairsOut, const hkKeyPair* end, hkPadSpu<int>& numPairsSkipped)
{
    do
    {
        const unsigned int d0 = hk1AxisSweep_yzDisjoint(query, list[0]);
        const unsigned int d1 = hk1AxisSweep_yzDisjoint(query, list[1]);
        const unsigned int d2 = hk1AxisSweep_yzDisjoint(query, list[2]);
        const unsigned int d3 = hk1AxisSweep_yzDisjoint(query, list[3]);
        if ((d0 & d1 & d2 & d3) == 0)
        {
            for (int i = 0; i < 4; i++)
            {
                const unsigned int disjoint = i == 0 ? d0 : (i == 1 ? d1 : (i == 2 ? d2 : d3));
                if (disjoint == 0 && (i == 0 || list[i].m_min[0] <= maxX))
                {
                    if (flip)
                    {
                        pairsOut = hk1AxisSweep_appendPair(hk1AxisSweep_key(list[i]), hk1AxisSweep_key(query), pairsOut, end, numPairsSkipped);
                    }
                    else
                    {
                        pairsOut = hk1AxisSweep_appendPair(hk1AxisSweep_key(query), hk1AxisSweep_key(list[i]), pairsOut, end, numPairsSkipped);
                    }
                }
            }
        }
        list += 4;
    } while (list[0].m_min[0] <= maxX);
    return pairsOut;
}

// 0x00FB51F0: the pairs (a key, b key) of overlapping boxes; returns their number. Pairs beyond maxNumPairs are
// counted in numPairsSkipped.
int hk1AxisSweep::collide(const AabbInt* pa, int numA, const AabbInt* pb, int numB, hkKeyPair* pairsOut, int maxNumPairs, hkPadSpu<int>& numPairsSkipped)
{
    numPairsSkipped = 0;
    hkKeyPair* out = pairsOut;
    const hkKeyPair* end = pairsOut + maxNumPairs;
    for (;;)
    {
        if (pa->m_min[0] > pb->m_min[0])
        {
            // b starts first: the a boxes up to b's maximum
            if (numB < 1)
            {
                break;
            }
            const unsigned int maxX = pb->m_max[0];
            if (pa->m_min[0] <= maxX)
            {
                out = hk1AxisSweep_scanList<1>(*pb, pa, maxX, out, end, numPairsSkipped);
            }
            pb++;
            numB--;
        }
        else
        {
            if (numA < 1)
            {
                break;
            }
            const unsigned int maxX = pa->m_max[0];
            if (pb->m_min[0] <= maxX)
            {
                out = hk1AxisSweep_scanList<0>(*pa, pb, maxX, out, end, numPairsSkipped);
            }
            pa++;
            numA--;
        }
    }
    return (int)(out - pairsOut);
}

// 0x00DB79D0: ordered by x minimum
bool hk1AxisSweep::AabbInt::operator<(const AabbInt& aabb1) const
{
    return m_min[0] < aabb1.m_min[0];
}

// 0x00DB95B0: all 0x20 bytes, the key included
void hk1AxisSweep::AabbInt::operator=(const AabbInt& other)
{
    hkAabbUint32::operator=(other);
}
