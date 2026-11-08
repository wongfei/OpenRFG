// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomlistener.h
//
// Havok 2014.2.5 (player_minimal.md P1): the listener base, from rfg.exe (inline in the SDK header).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomlistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"

// 0x00F1C910
hkpPhantomListener::hkpPhantomListener()
{
}

// 0x00F1C7F0
hkpPhantomListener::~hkpPhantomListener()
{
}

// 0x00833AA0
void hkpPhantomListener::phantomAddedCallback(hkpPhantom*)
{
}

// 0x00F1C800
void hkpPhantomListener::phantomRemovedCallback(hkpPhantom*)
{
}

// 0x00833AB0: a shape change in the world reads as a remove and an add
void hkpPhantomListener::phantomShapeSetCallback(hkpPhantom* phantom)
{
    if (phantom->m_world != nullptr)
    {
        phantomRemovedCallback(phantom);
        phantomAddedCallback(phantom);
    }
}

// 0x00833AE0
void hkpPhantomListener::phantomDeletedCallback(hkpPhantom*)
{
}
