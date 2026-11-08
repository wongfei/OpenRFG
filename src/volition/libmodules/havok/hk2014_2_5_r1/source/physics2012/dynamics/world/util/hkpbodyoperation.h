// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpbodyoperation.h
//
// Havok 2014.2.5 (havok_impl.md HK7): an operation on a body queued in the world (hkpWorld::addBodyOperation).
// Layout from rfg.pdb, inline code from Havok 2014.1 (hkpBodyOperation.h). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpRigidBody;

// size 0x8
class hkpBodyOperation : public hkReferencedObject
{
public:
    enum ExecutionState
    {
        EXECUTE_SINGLE_THREADED = 0,
        EXECUTE_MULTI_THREADED = 1,
    };

    // size 0x2
    struct UpdateInfo
    {
        // 0x00E16420
        UpdateInfo()
        {
            m_bodyIsDeleted.m_bool = 0;
            m_bodyIsInWorld.m_bool = 1;
        }

        hkBool m_bodyIsDeleted; // 0x00
        hkBool m_bodyIsInWorld; // 0x01
    };

    HK_DECLARE_CLASS_ALLOCATOR();
    hkpBodyOperation() {}
    virtual ~hkpBodyOperation() {} // vtbl+0x00
    virtual void execute(hkpRigidBody* body, UpdateInfo& updateInfo) = 0; // vtbl+0x10
};
