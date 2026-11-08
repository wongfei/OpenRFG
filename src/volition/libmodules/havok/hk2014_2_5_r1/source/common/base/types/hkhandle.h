// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkhandle.h
//
// Hand-written (stub_impl.md S4, serializer; code as the 2014.1 SDK's hkHandle): a typed integer handle with an
// invalid value; DISCRIMINANT only keeps handles of different uses apart.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size sizeof(TYPE)
template<typename TYPE, TYPE INVALID_VALUE, typename DISCRIMINANT>
struct hkHandle
{
    HK_DECLARE_POD_TYPE();
    typedef TYPE Type;
    enum { InvalidValue = INVALID_VALUE };

    explicit hkHandle(hkFinishLoadedObjectFlag) {}
    explicit hkHandle(int v) : m_value((TYPE)v) {}
    hkHandle() {}

    hkHandle& operator=(hkHandle other) { m_value = other.m_value; return *this; }
    bool operator==(hkHandle other) const { return m_value == other.m_value; }
    bool operator!=(hkHandle other) const { return m_value != other.m_value; }
    bool operator<(hkHandle other) const { return m_value < other.m_value; }
    bool operator<=(hkHandle other) const { return m_value <= other.m_value; }
    bool operator>(hkHandle other) const { return m_value > other.m_value; }
    bool operator>=(hkHandle other) const { return m_value >= other.m_value; }
    TYPE value() const { return m_value; }
    TYPE valueUnchecked() const { return m_value; }
    bool isValid() const { return m_value != INVALID_VALUE; }
    static hkHandle invalid() { return hkHandle(INVALID_VALUE); }

protected:
    TYPE m_value; // 0x00
};
