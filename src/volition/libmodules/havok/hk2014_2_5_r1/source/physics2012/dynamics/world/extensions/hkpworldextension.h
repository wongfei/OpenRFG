// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/extensions/hkpworldextension.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkpWorld;

// size 0x14
class hkpWorldExtension : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00E1F950
    virtual void performAttachments(hkpWorld*) = 0; // vtbl+0x10
    virtual void performDetachments(hkpWorld*) = 0; // vtbl+0x14
    int getId() const; // 0x00DE88E0
    hkpWorld* getWorld() const;
public:
    hkpWorldExtension(int id); // 0x00E1F920
public:
    void addedToWorld(hkpWorld* world); // 0x00DEAA10
    void removedFromWorld(hkpWorld* world); // 0x00DEAA60
    virtual ~hkpWorldExtension(); // vtbl+0x00 0x00DE88F0

public:
    hkpWorld* m_world; // 0x08
    int m_id; // 0x0C
    unsigned short m_attachmentCount; // 0x10
};
