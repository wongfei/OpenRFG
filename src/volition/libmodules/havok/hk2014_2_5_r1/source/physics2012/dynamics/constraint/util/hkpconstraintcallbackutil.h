// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


struct hkpConstraintBrokenEvent;
class hkpConstraintInstance;
struct hkpConstraintRepairedEvent;

// size 0x1
class hkpConstraintCallbackUtil
{
public:
    static void fireConstraintAdded(hkpConstraintInstance* constraint); // 0x00E285F0
    static void fireConstraintRemoved(hkpConstraintInstance* constraint); // 0x00E28680
    static void fireConstraintDeleted(hkpConstraintInstance* constraint); // 0x00E28710
    static void fireConstraintBroken(const hkpConstraintBrokenEvent& event); // 0x00E287A0
    static void fireConstraintRepaired(const hkpConstraintRepairedEvent& event); // 0x00E28830

};
