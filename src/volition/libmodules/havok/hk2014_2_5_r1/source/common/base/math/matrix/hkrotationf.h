// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Matrix/hkRotationf.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_ROTATIONf_H)
#define HK_MATH_ROTATIONf_H

#ifdef HK_COMPILER_MSVC
#    define HK_FORCE_ALIGN_ROTATION __declspec(align(HK_FLOAT_ALIGNMENT))
#else
#    define HK_FORCE_ALIGN_ROTATION
#endif

class HK_FORCE_ALIGN_ROTATION hkRotationf : public hkMatrix3f
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkRotationf);
        HK_DECLARE_POD_TYPE();
        HK_FORCE_INLINE hkRotationf() {} // 0x00710690
        void set(hkQuaternionfParameter q); // 0x00CEFA00
        void setAxisAngle(hkVector4fParameter axis, hkFloat32 angle); // 0x00CEFA90
        void setAxisAngle(hkVector4fParameter axis, hkSimdFloat32Parameter angle); // 0x00CEFAD0
        bool isOrthonormal( hkFloat32 epsilon = hkFloat32(1e-5f) ) const; // 0x00CEFB00
        HK_FORCE_INLINE static const hkRotationf& HK_CALL getIdentity(); // 0x00F27230
        bool isOk() const; // 0x00CEFBD0
        void renormalize(); // 0x00CEFC00
};

#undef HK_FORCE_ALIGN_ROTATION

#endif //HK_MATH_ROTATIONf_H
