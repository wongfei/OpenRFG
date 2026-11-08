// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkprootcdpoint.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"

class hkSimdFloat32;
class hkVector4f;
class hkpCollidable;

// size 0x30
struct hkpRootCdPoint
{
    const hkContactPoint& getContact() const;
    void setContact(const hkVector4f&, const hkVector4f&, const hkSimdFloat32&);
    void setContact(const hkVector4f&, const hkVector4f&, float);
    void setContact(const hkContactPoint&);
    void setContactDistance(const hkSimdFloat32&);
    void setContactDistance(float);

    hkContactPoint m_contact; // 0x00
    const hkpCollidable* m_rootCollidableA; // 0x20
    unsigned int m_shapeKeyA; // 0x24
    const hkpCollidable* m_rootCollidableB; // 0x28
    unsigned int m_shapeKeyB; // 0x2C
};
