// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the listener base, every callback empty, from rfg.exe (inline in the SDK
// header).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"

// 0x0080DBA0
hkpConstraintListener::hkpConstraintListener()
{
}

// 0x0080D430
hkpConstraintListener::~hkpConstraintListener()
{
}

// 0x0080D440
void hkpConstraintListener::constraintAddedCallback(hkpConstraintInstance* constraint)
{
}

// 0x0080D450
void hkpConstraintListener::constraintRemovedCallback(hkpConstraintInstance* constraint)
{
}

// 0x0080D460
void hkpConstraintListener::constraintDeletedCallback(hkpConstraintInstance* constraint)
{
}

// 0x0080D470
void hkpConstraintListener::constraintViolatedCallback(hkpConstraintInstance* constraint)
{
}

// 0x0080D480
void hkpConstraintListener::constraintBreakingCallback(const hkpConstraintBrokenEvent& event)
{
}

// 0x0080D490
void hkpConstraintListener::constraintRepairedCallback(const hkpConstraintRepairedEvent& event)
{
}
