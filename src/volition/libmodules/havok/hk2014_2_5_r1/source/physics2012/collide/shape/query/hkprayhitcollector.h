// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpRayHitCollector.inl). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

class hkpCdBody;
struct hkpShapeRayCastCollectorOutput;

// size 0x8
class hkpRayHitCollector
{
public:
    // 0x0080D7E0 (hkprayhitcollector.inl:15)
    hkpRayHitCollector()
    {
        reset();
    }

    virtual void addRayHit(const hkpCdBody&, const hkpShapeRayCastCollectorOutput&) = 0; // vtbl+0x00

    // 0x0080D7D0 (hkprayhitcollector.inl:10)
    void reset()
    {
        m_earlyOutHitFraction = 1.0f;
    }

    virtual ~hkpRayHitCollector() {} // vtbl+0x04 0x0080D7A0 (.h:46)
    static int shapeKeysFromCdBody(unsigned int* buf, int maxKeys, const hkpCdBody& body); // 0x00D83E70

    // 0x00 vfptr
    float m_earlyOutHitFraction; // 0x04
};
