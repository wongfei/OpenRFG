// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Quaternion/hkQuaternionf.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_QUATERNIONf_H)
#define HK_MATH_QUATERNIONf_H

HK_PASS_IN_REG
class hkQuaternionf
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkQuaternionf);
        HK_DECLARE_POD_TYPE();

#ifndef HK_DISABLE_MATH_CONSTRUCTORS
        HK_FORCE_INLINE hkQuaternionf() { } // 0x0080C560 (.h:60)
        HK_FORCE_INLINE hkQuaternionf(hkFloat32 ix, hkFloat32 iy, hkFloat32 iz, hkFloat32 r);
        HK_FORCE_INLINE hkQuaternionf(hkSimdFloat32Parameter ix, hkSimdFloat32Parameter iy, hkSimdFloat32Parameter iz, hkSimdFloat32Parameter r);
        explicit HK_FORCE_INLINE hkQuaternionf(const hkRotationf& r); // 0x0080C9B0 (hkquaternionf.inl:44)
        HK_FORCE_INLINE hkQuaternionf(hkVector4fParameter axis, hkFloat32 angle); // 0x0080C9D0 (hkquaternionf.inl:59)
#endif

        HK_FORCE_INLINE void operator= (const hkQuaternionf& q); // 0x00941C10 (hkquaternionf.inl:65)
        HK_FORCE_INLINE void set(hkFloat32 ix, hkFloat32 iy, hkFloat32 iz, hkFloat32 r); // 0x0080C9F0 (hkquaternionf.inl:70)
        HK_FORCE_INLINE void set(hkSimdFloat32Parameter ix, hkSimdFloat32Parameter iy, hkSimdFloat32Parameter iz, hkSimdFloat32Parameter r);
        HK_FORCE_INLINE static const hkQuaternionf& HK_CALL getIdentity(); // 0x00E72270
        HK_FORCE_INLINE void setIdentity(); // 0x008140C0 (hkquaternionf.inl:82)
        HK_FORCE_INLINE void setInverse( hkQuaternionfParameter q ); // 0x00EA8E40
        HK_FORCE_INLINE void setMul(hkSimdFloat32Parameter r, hkQuaternionfParameter q);
        HK_FORCE_INLINE void addMul(hkSimdFloat32Parameter r, hkQuaternionfParameter q);
        HK_FORCE_INLINE void setMul(hkQuaternionfParameter q0, hkQuaternionfParameter q1); // 0x00B71150 (hkquaternionf.inl:104)
        HK_FORCE_INLINE void mul(hkQuaternionfParameter q); // 0x00F83090
        HK_FORCE_INLINE void setMulInverse(hkQuaternionfParameter q0, hkQuaternionfParameter q1); // 0x009703A0 (hkquaternionf.inl:135)
        HK_FORCE_INLINE void setInverseMul(hkQuaternionfParameter q0, hkQuaternionfParameter q1); // 0x00CEF100
        HK_FORCE_INLINE void estimateAngleTo(hkQuaternionfParameter to, hkVector4f& angleOut) const;
        void setAxisAngle(hkVector4fParameter axis, hkFloat32 angle); // 0x00CED040
        void setAxisAngle(hkVector4fParameter axis, hkSimdFloat32Parameter angle); // 0x00CECED0
        void setAxisAngle_Approximate(hkVector4fParameter axis, hkSimdFloat32Parameter angle); // 0x00CED900
        void setFromEulerAngles(hkFloat32 roll, hkFloat32 pitch, hkFloat32 yaw); // 0x00CED7B0
        void setFromEulerAngles(hkSimdFloat32Parameter roll, hkSimdFloat32Parameter pitch, hkSimdFloat32Parameter yaw); // 0x00CED070
        void setFromEulerAngles_Approximate(hkSimdFloat32Parameter roll, hkSimdFloat32Parameter pitch, hkSimdFloat32Parameter yaw); // 0x00CED800
        void set(const hkRotationf& r); // 0x00CEDF10
        HK_FORCE_INLINE void _set(const hkRotationf& r); // 0x00CEF7F0
        void removeAxisComponent (hkVector4fParameter axis); // 0x00CEDAC0
        void decomposeRestAxis (hkVector4fParameter axis, hkQuaternionf& restOut, hkSimdFloat32& angleOut) const; // 0x00CEDDD0
        void setSlerp(hkQuaternionfParameter q0, hkQuaternionfParameter q1, hkSimdFloat32Parameter t); // 0x00CEE0C0
        void setBarycentric(const hkQuaternionf* HK_RESTRICT qVerts, hkVector4fParameter vLambda); // 0x00CEE690
        /* access access */

        HK_FORCE_INLINE void setReal(hkFloat32 r); // 0x0080C9A0 (hkquaternionf.inl:28)
        HK_FORCE_INLINE hkFloat32 getReal() const; // 0x00941C00 (hkquaternionf.inl:23)
        HK_FORCE_INLINE const hkSimdFloat32 getRealPart() const; // 0x00970380 (hkquaternionf.inl:33)
        HK_FORCE_INLINE void setRealPart(hkSimdFloat32Parameter r);
        HK_FORCE_INLINE void setImag(hkVector4fParameter i); // 0x0080C980 (hkquaternionf.inl:17)
        HK_FORCE_INLINE const hkVector4f& getImag() const; // 0x00941BF0 (hkquaternionf.inl:12)
        HK_FORCE_INLINE hkFloat32 getAngle() const; // 0x00D958D0
        hkSimdFloat32 getAngleSr() const; // 0x00CED810
        HK_FORCE_INLINE void getAxis(hkVector4f &axis) const; // 0x00CFCDB0
        HK_FORCE_INLINE hkBool32 hasValidAxis() const; // 0x00CFCAE0
        HK_FORCE_INLINE const hkFloat32& operator()(int i) const; // 0x0080CA20 (hkquaternionf.inl:180)
        template <int I> HK_FORCE_INLINE const hkSimdFloat32 getComponent() const;
        hkBool32 isOk(const hkFloat32 epsilon = hkFloat32(1e-3f)) const; // 0x00CED990
        HK_FORCE_INLINE void setClosest(hkQuaternionfParameter q, hkQuaternionfParameter qReference); // 0x00CEF1A0
        void setFlippedRotation(hkVector4fParameter from); // 0x00CED9F0
        HK_FORCE_INLINE void normalize(); // 0x00CEF580
        HK_FORCE_INLINE void setAndNormalize(const hkRotationf& r); // 0x00CEFEF0

        //
        // advanced interface
        //

        template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE void normalize();
        template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE void setAndNormalize(const hkRotationf& r);
    public:

        hkVector4f m_vec;
};

#endif //HK_MATH_QUATERNIONf_H
