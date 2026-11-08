// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentitylistener.h
//
// Seam only (Havok 2014.2.5).
#pragma once


class hkpEntity;

// size 0x4
class hkpEntityListener
{
public:
    virtual ~hkpEntityListener(); // vtbl+0x00 0x00B62FE0 (.h:20)
    virtual void entityAddedCallback(hkpEntity* entity); // vtbl+0x04 0x00833A40 (.h:23)
    virtual void entityRemovedCallback(hkpEntity* entity); // vtbl+0x08 0x00B62FF0 (.h:26)
    virtual void entityShapeSetCallback(hkpEntity* entity); // vtbl+0x0C 0x00833A50 (.h:30)
    virtual void entitySetMotionTypeCallback(hkpEntity* entity); // vtbl+0x10 0x00833A80 (.h:41)
    virtual void entityDeletedCallback(hkpEntity* entity); // vtbl+0x14 0x00833A90 (.h:44)
    hkpEntityListener(); // 0x00B64060

    // 0x00 vfptr
};
