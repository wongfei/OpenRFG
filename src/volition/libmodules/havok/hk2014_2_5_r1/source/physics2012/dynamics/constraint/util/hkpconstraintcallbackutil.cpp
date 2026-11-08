// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): a constraint's own listeners, from rfg.exe. They are called from the back
// (listeners may remove themselves, leaving a null slot), then the null slots are compacted. The "conAddCb"/"conRemCb"/"conDeletedCb"
// monitor timers are not reproduced. Deleted (HK8); broken and repaired with hkpBreakableConstraintData.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"

// (an LTCG helper): removes the null slots, keeping the order
static void hkpConstraintCallbackUtil_cleanupNullPointers(hkSmallArray<hkpConstraintListener*>& cleanupArray)
{
    for (int i = cleanupArray.getSize() - 1; i >= 0; i--)
    {
        if (cleanupArray[i] == nullptr)
        {
            cleanupArray.removeAtAndCopy(i);
        }
    }
}

// 0x00E285F0
void hkpConstraintCallbackUtil::fireConstraintAdded(hkpConstraintInstance* constraint)
{
    for (int i = constraint->m_listeners.getSize() - 1; i >= 0; i--)
    {
        if (constraint->m_listeners[i] != nullptr)
        {
            constraint->m_listeners[i]->constraintAddedCallback(constraint);
        }
    }
    hkpConstraintCallbackUtil_cleanupNullPointers(constraint->m_listeners);
}

// 0x00E28680
void hkpConstraintCallbackUtil::fireConstraintRemoved(hkpConstraintInstance* constraint)
{
    for (int i = constraint->m_listeners.getSize() - 1; i >= 0; i--)
    {
        if (constraint->m_listeners[i] != nullptr)
        {
            constraint->m_listeners[i]->constraintRemovedCallback(constraint);
        }
    }
    hkpConstraintCallbackUtil_cleanupNullPointers(constraint->m_listeners);
}

// 0x00E28710
void hkpConstraintCallbackUtil::fireConstraintDeleted(hkpConstraintInstance* constraint)
{
    for (int i = constraint->m_listeners.getSize() - 1; i >= 0; i--)
    {
        if (constraint->m_listeners[i] != nullptr)
        {
            constraint->m_listeners[i]->constraintDeletedCallback(constraint);
        }
    }
    hkpConstraintCallbackUtil_cleanupNullPointers(constraint->m_listeners);
}

// 0x00E287A0
void hkpConstraintCallbackUtil::fireConstraintBroken(const hkpConstraintBrokenEvent& event)
{
    hkpConstraintInstance* constraint = event.m_constraintInstance;
    for (int i = constraint->m_listeners.getSize() - 1; i >= 0; i--)
    {
        if (constraint->m_listeners[i] != nullptr)
        {
            constraint->m_listeners[i]->constraintBreakingCallback(event);
        }
    }
    hkpConstraintCallbackUtil_cleanupNullPointers(constraint->m_listeners);
}

// 0x00E28830
void hkpConstraintCallbackUtil::fireConstraintRepaired(const hkpConstraintRepairedEvent& event)
{
    hkpConstraintInstance* constraint = event.m_constraintInstance;
    for (int i = constraint->m_listeners.getSize() - 1; i >= 0; i--)
    {
        if (constraint->m_listeners[i] != nullptr)
        {
            constraint->m_listeners[i]->constraintRepairedCallback(event);
        }
    }
    hkpConstraintCallbackUtil_cleanupNullPointers(constraint->m_listeners);
}
