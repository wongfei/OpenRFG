// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxylistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

class hkContactPoint;
struct hkContainerHeapAllocator;
struct hkSimplexSolverInput;
class hkpCharacterProxy;
class hkpRigidBody;
struct hkpRootCdPoint;

// size 0x40
struct alignas(16) hkpCharacterObjectInteractionEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCharacterObjectInteractionEvent);

    hkVector4f m_position; // 0x00
    hkVector4f m_normal; // 0x10
    float m_objectImpulse; // 0x20
    float m_timestep; // 0x24
    float m_projectedVelocity; // 0x28
    float m_objectMassInv; // 0x2C
    hkpRigidBody* m_body; // 0x30
};

// size 0x20
struct hkpCharacterObjectInteractionResult
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCharacterObjectInteractionResult);

    hkVector4f m_objectImpulse; // 0x00
    hkVector4f m_impulsePosition; // 0x10
};

// size 0x4
class hkpCharacterProxyListener
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCharacterProxyListener); // delete(void* p, unsigned int nbytes) 0x00814480
    virtual ~hkpCharacterProxyListener(); // vtbl+0x00 0x0080CEA0 (.h:70)
    virtual void processConstraintsCallback(const hkpCharacterProxy* proxy, const hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold, hkSimplexSolverInput& input); // vtbl+0x04 0x0080CEB0 (.h:75)
    virtual void contactPointAddedCallback(const hkpCharacterProxy* proxy, const hkpRootCdPoint& point); // vtbl+0x08 0x0080CEC0 (.h:78)
    virtual void contactPointRemovedCallback(const hkpCharacterProxy* proxy, const hkpRootCdPoint& point); // vtbl+0x0C 0x0080CED0 (.h:81)
    virtual void characterInteractionCallback(hkpCharacterProxy* proxy, hkpCharacterProxy* otherProxy, const hkContactPoint& contact); // vtbl+0x10 0x0080CEE0 (.h:84)
    virtual void objectInteractionCallback(hkpCharacterProxy* proxy, const hkpCharacterObjectInteractionEvent& input, hkpCharacterObjectInteractionResult& output); // vtbl+0x14 0x0080CEF0 (.h:87)
    hkpCharacterProxyListener(); // 0x0080D630

    // 0x00 vfptr
};
