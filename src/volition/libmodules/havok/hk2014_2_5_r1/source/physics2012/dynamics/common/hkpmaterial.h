// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/common/hkpmaterial.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;

// size 0xC
class hkpMaterial
{
public:
    enum ResponseType
    {
        RESPONSE_INVALID = 0,
        RESPONSE_SIMPLE_CONTACT = 1,
        RESPONSE_REPORTING = 2,
        RESPONSE_NONE = 3,
        RESPONSE_MAX_ID = 4,
    };

    static const hkClass& staticClass(); // 0x00DEE8A0
    hkpMaterial(hkFinishLoadedObjectFlag flag); // 0x00DDEAB0
    hkpMaterial(); // 0x00DDEAC0
    float getFriction() const; // 0x00B62DA0 (hkpmaterial.inl:17)
    float getRestitution() const; // 0x00DF28B0
    float getRollingFrictionMultiplier() const; // 0x00DF28C0
    void setFriction(float newFriction); // 0x00DF28D0
    void setRestitution(float newRestitution); // 0x00DF28E0
    void setRollingFrictionMultiplier(float newRollingFrictionMultiplier); // 0x00DF2D60
    static float getCombinedFriction(float frictionA, float frictionB); // 0x00E1B1D0
    static float getCombinedRestitution(float restitutionA, float restitutionB); // 0x00E1B200
    static float getCombinedRollingFrictionMultiplier(float multiplierA, float multiplierB); // 0x00E1B230
    void setResponseType(ResponseType t); // 0x00DF2D90
    ResponseType getResponseType() const;

public:
    hkEnum<enum ResponseType,signed char> m_responseType; // 0x00
    hkHalf m_rollingFrictionMultiplier; // 0x02
    float m_friction; // 0x04
    float m_restitution; // 0x08
};
