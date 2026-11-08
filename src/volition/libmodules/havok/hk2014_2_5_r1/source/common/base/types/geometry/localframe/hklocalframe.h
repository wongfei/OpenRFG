// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/localframe/hklocalframe.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
class hkLocalFrameCollector;
class hkLocalFrameGroup;
class hkMemoryAllocator;
class hkTransformf;
class hkVector4f;

// size 0x8
class hkLocalFrame : public hkReferencedObject
{
public:
    static const hkClass& staticClass(); // 0x00CE9060
    hkLocalFrame(hkFinishLoadedObjectFlag flag); // 0x00CE8FE0
    hkLocalFrame();
    void getTransformToRoot(hkTransformf& transform) const; // 0x00D00190
    void getPositionInRoot(hkVector4f& position) const; // 0x00D00200
    virtual ~hkLocalFrame(); // vtbl+0x00 0x00CE8FF0
    virtual void getLocalTransform(hkTransformf&) const = 0; // vtbl+0x10
    virtual void setLocalTransform(const hkTransformf&) = 0; // vtbl+0x14
    virtual void getLocalPosition(hkVector4f& position) const; // vtbl+0x18 0x00D00290
    virtual void getNearbyFrames(const hkVector4f&, float, hkLocalFrameCollector&) const = 0; // vtbl+0x1C
    virtual const char* getName() const = 0; // vtbl+0x20
    virtual const hkLocalFrame* getParentFrame() const = 0; // vtbl+0x24
    virtual void setParentFrame(const hkLocalFrame*) = 0; // vtbl+0x28
    virtual int getNumChildFrames() const = 0; // vtbl+0x2C
    virtual hkLocalFrame* getChildFrame(int) const = 0; // vtbl+0x30
    virtual const hkLocalFrameGroup* getGroup() const = 0; // vtbl+0x34
    void getDescendants(hkArray<hkLocalFrame const*,hkContainerHeapAllocator>&) const;
    virtual void setGroup(const hkLocalFrameGroup*) = 0; // vtbl+0x38
    virtual void getDescendants(hkArrayBase<hkLocalFrame const*>& descendants, hkMemoryAllocator& a) const; // vtbl+0x3C 0x00D002F0

};
