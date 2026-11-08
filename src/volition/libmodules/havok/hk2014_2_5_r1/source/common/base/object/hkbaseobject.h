// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkbaseobject.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.cpp
//
// Hand-written (Havok 2014.2.5, havok_impl.md HK1): the root of Havok's polymorphic classes (a vtable pointer). Slot
// 0x04 is the empty marker function Havok declares first (absent from the PDB, Havok 2014.1 hkBaseObject.h).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkFinishLoadedObjectFlag;

// size 0x4
class hkBaseObject
{
public:
    HK_DECLARE_PLACEMENT_ALLOCATOR(); // deleting destructor 0x00710580 does not free
    hkBaseObject() {}
    hkBaseObject(hkFinishLoadedObjectFlag flag); // 0x00CE7460 (.h:33)
    virtual ~hkBaseObject(); // vtbl+0x00 0x00710540
    virtual void __first_virtual_table_function__(); // vtbl+0x04 0x00710560 (.h:38)
};
