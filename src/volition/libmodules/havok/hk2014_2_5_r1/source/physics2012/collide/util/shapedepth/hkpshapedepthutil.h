// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/shapedepth/hkpshapedepthutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpShape;

// size 0x1
class hkpShapeDepthUtil
{
public:
    static unsigned char getShapeDepth(const hkpShape* shape); // 0x0104EA30

    static unsigned char s_defaultMinimumChildDepth; // 0x03BACFE0
};
