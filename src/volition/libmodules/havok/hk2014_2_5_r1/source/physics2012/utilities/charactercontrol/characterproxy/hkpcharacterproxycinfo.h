// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxycinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/hkpcharactercontrollercinfo.h"

class hkClass;
struct hkInternalClassMember;
class hkpShapePhantom;

// size 0x90
class alignas(16) hkpCharacterProxyCinfo : public hkpCharacterControllerCinfo
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00F18000, delete(void* p, unsigned int nbytes) 0x008143F0
    static const hkClass& staticClass(); // 0x00F18020
    hkpCharacterProxyCinfo(hkFinishLoadedObjectFlag flag) : hkpCharacterControllerCinfo(flag) {} // 0x00F18010
    hkpCharacterProxyCinfo(); // 0x0080CDE0 (.h:28)
    virtual ~hkpCharacterProxyCinfo(); // vtbl+0x00 0x0080CE90

    static const hkInternalClassMember Members[];
    alignas(16) hkVector4f m_position; // 0x10
    hkVector4f m_velocity; // 0x20
    float m_dynamicFriction; // 0x30
    float m_staticFriction; // 0x34
    float m_keepContactTolerance; // 0x38
    hkVector4f m_up; // 0x40
    float m_extraUpStaticFriction; // 0x50
    float m_extraDownStaticFriction; // 0x54
    hkpShapePhantom* m_shapePhantom; // 0x58
    float m_keepDistance; // 0x5C
    float m_contactAngleSensitivity; // 0x60
    unsigned int m_userPlanes; // 0x64
    float m_maxCharacterSpeedForSolver; // 0x68
    float m_characterStrength; // 0x6C
    float m_characterMass; // 0x70
    float m_maxSlope; // 0x74
    float m_penetrationRecoverySpeed; // 0x78
    int m_maxCastIterations; // 0x7C
    hkBool m_refreshManifoldInCheckSupport; // 0x80
};
