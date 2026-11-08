// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpShape;
struct hkpShapeBufferStorage;

// size 0x4
class hkpShapeContainer
{
public:
    enum ReferencePolicy
    {
        REFERENCE_POLICY_IGNORE = 0,
        REFERENCE_POLICY_INCREMENT = 1,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpShapeContainer); // delete(void* p, unsigned int nbytes) 0x0071AA30
    static const hkClass& staticClass(); // 0x00D7C690
    virtual ~hkpShapeContainer(); // vtbl+0x00 0x007108F0 (.h:41)
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00D74310
    virtual unsigned int getFirstKey() const = 0; // vtbl+0x08
    virtual unsigned int getNextKey(unsigned int) const = 0; // vtbl+0x0C
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00D74300
    virtual const hkpShape* getChildShape(unsigned int, hkpShapeBufferStorage&) const = 0; // vtbl+0x14
    virtual bool isWeldingEnabled() const; // vtbl+0x18 0x00710900 (.h:79)
    hkpShapeContainer(); // 0x00710910

    // 0x00 vfptr
    static const hkInternalClassMember Members[];
};

// size 0x8
class hkpSingleShapeContainer : public hkpShapeContainer
{
public:
    static const hkClass& staticClass(); // 0x00D7C6A0
    hkpSingleShapeContainer(hkFinishLoadedObjectFlag); // 0x00D75200
    hkpSingleShapeContainer(const hkpShape* s, hkpShapeContainer::ReferencePolicy ref); // 0x00D74C10
    hkpSingleShapeContainer();
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSingleShapeContainer); // new(unsigned int n, void* p) 0x00D7C680, delete(void* p, unsigned int nbytes) 0x0071AAE0
    virtual ~hkpSingleShapeContainer(); // vtbl+0x00 0x0071AA90 (.h:110)
    virtual int getNumChildShapes() const; // vtbl+0x04 0x0071AAB0 (.h:118)
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x0071AAC0 (.h:121)
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x0071AAD0 (.h:124)
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00D74340
    const hkpShape* getChild() const; // 0x0071B6B0 (.h:134)
    const hkpShape* operator->() const; // 0x00D74CE0

    static const hkInternalClassMember Members[]; // 0x01353534
public:
    const hkpShape* m_childShape; // 0x04
};
