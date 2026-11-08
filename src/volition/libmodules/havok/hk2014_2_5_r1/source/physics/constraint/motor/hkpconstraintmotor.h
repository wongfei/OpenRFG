// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/hkpconstraintmotor.h
//
// Havok 2014.2.5: the base of the constraint motors (hkCalcMotorData switches on m_type), from rfg.pdb.
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0xC
class hkpConstraintMotor : public hkReferencedObject
{
public:
    enum MotorType
    {
        TYPE_INVALID = 0,
        TYPE_POSITION = 1,
        TYPE_VELOCITY = 2,
        TYPE_SPRING_DAMPER = 3,
        TYPE_CALLBACK = 4,
        TYPE_MAX = 5,
    };

    hkpConstraintMotor() { m_type = TYPE_INVALID; }
    virtual hkpConstraintMotor* clone() const = 0; // vtbl+0x10

    hkEnum<MotorType,signed char> m_type; // 0x08
};
