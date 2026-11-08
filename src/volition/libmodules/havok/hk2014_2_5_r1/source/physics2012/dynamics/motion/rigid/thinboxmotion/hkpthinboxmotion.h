// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/thinboxmotion/hkpthinboxmotion.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpboxmotion.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkQuaternionf;
class hkVector4f;

// size 0x120
class hkpThinBoxMotion : public hkpBoxMotion
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DDD1C0, delete(void* p, unsigned int nbytes) 0x00DDEF90
    static const hkClass& staticClass(); // 0x00DEF180
    hkpThinBoxMotion(hkFinishLoadedObjectFlag flag); // 0x00DDEF60
    hkpThinBoxMotion(const hkVector4f& position, const hkQuaternionf& rotation); // 0x00E28570
    virtual ~hkpThinBoxMotion(); // vtbl+0x00 0x00DDEFD0

    static const hkInternalClassMember Members[];
};
