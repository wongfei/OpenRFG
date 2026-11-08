// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/destruction/breakablebody/hkpbreakablebody.h
//
// Havok 2014.2.5 (havok_impl.md HK7): the breakable body, only as far as hkpRigidBody::clone needs it (destruction
// is out of scope, havok_impl.md section 2). Layout and virtuals from rfg.pdb. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpBreakableShape;
class hkpRigidBody;

// size 0x14
class hkpBreakableBody : public hkReferencedObject
{
public:
    virtual hkpBreakableBody* cloneBreakableBody(hkpRigidBody* body) const = 0; // vtbl+0x10

protected:
    hkReferencedObject* m_controller; // 0x08 (hkRefPtr<Controller>)
    const hkReferencedObject* m_breakableShape; // 0x0C (hkRefPtr<const hkpBreakableShape>)
public:
    unsigned char m_bodyTypeAndFlags; // 0x10
protected:
    hkHalf m_constraintStrength; // 0x12
};
