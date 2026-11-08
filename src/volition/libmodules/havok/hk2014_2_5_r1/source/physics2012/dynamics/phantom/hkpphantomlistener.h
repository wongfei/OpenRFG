// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomlistener.h
//
// Seam only (Havok 2014.2.5).
#pragma once


class hkpPhantom;

// size 0x4
class hkpPhantomListener
{
public:
    virtual ~hkpPhantomListener(); // vtbl+0x00 0x00F1C7F0
    virtual void phantomAddedCallback(hkpPhantom*); // vtbl+0x04 0x00833AA0 (.h:25)
    virtual void phantomRemovedCallback(hkpPhantom*); // vtbl+0x08 0x00F1C800
    virtual void phantomShapeSetCallback(hkpPhantom* phantom); // vtbl+0x0C 0x00833AB0 (.h:32)
    virtual void phantomDeletedCallback(hkpPhantom*); // vtbl+0x10 0x00833AE0 (.h:43)
    hkpPhantomListener(); // 0x00F1C910

    // 0x00 vfptr
};
