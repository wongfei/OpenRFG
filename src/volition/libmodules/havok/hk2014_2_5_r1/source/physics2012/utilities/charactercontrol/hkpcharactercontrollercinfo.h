// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/hkpcharactercontrollercinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;

// size 0x8
class hkpCharacterControllerCinfo : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00F18060, delete(void* p, unsigned int nbytes) 0x00814360
    static const hkClass& staticClass(); // 0x00F18070
    hkpCharacterControllerCinfo(hkFinishLoadedObjectFlag flag) : hkReferencedObject(flag) {} // 0x00F17EF0
    hkpCharacterControllerCinfo(); // 0x0080CDC0 (.h:23)
    virtual ~hkpCharacterControllerCinfo(); // vtbl+0x00 0x0080CDD0

    static const hkInternalClassMember Members[];
};
