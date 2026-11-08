// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/shapedepth/hkpshapedepthutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the depth of a shape hierarchy (the levels of containers), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/shapedepth/hkpshapedepthutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

// 0x03BACFE0 (zero in rfg.exe: nothing sets it)
unsigned char hkpShapeDepthUtil::s_defaultMinimumChildDepth;

// 0x0104EA30: a leaf has the minimum depth, a container one more than its deepest child (at least the minimum)
unsigned char hkpShapeDepthUtil::getShapeDepth(const hkpShape* shape)
{
    const hkpShapeContainer* container = shape->getContainer();
    if (container == nullptr)
    {
        return s_defaultMinimumChildDepth;
    }
    unsigned char deepest = 0;
    hkpShapeBuffer buffer;
    for (unsigned int key = container->getFirstKey(); key != 0xFFFFFFFF; key = container->getNextKey(key))
    {
        const unsigned char depth = getShapeDepth(container->getChildShape(key, buffer));
        if (depth > deepest)
        {
            deepest = depth;
        }
    }
    const unsigned char depth = (unsigned char)(deepest + 1);
    return depth > s_defaultMinimumChildDepth ? depth : s_defaultMinimumChildDepth;
}
