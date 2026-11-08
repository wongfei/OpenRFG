// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/QsTransform/hkQsTransformf.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_QSTRANSFORMf_H)
#define HK_MATH_QSTRANSFORMf_H

class hkQsTransformf
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkQsTransformf);
        HK_DECLARE_POD_TYPE();

#ifndef HK_DISABLE_MATH_CONSTRUCTORS

            // This enum is used to specify a constructor that initializes to identity
        enum IdentityInitializer
        {
            IDENTITY,
        };

            // This enum is used to specify a constructor that initializes to zero
        enum ZeroInitializer
        {
            ZERO,
        };
        HK_FORCE_INLINE hkQsTransformf() { } // 0x00F96E70
        HK_FORCE_INLINE hkQsTransformf(IdentityInitializer init);
        HK_FORCE_INLINE hkQsTransformf(ZeroInitializer init);
        HK_FORCE_INLINE hkQsTransformf(hkVector4fParameter translation, hkQuaternionfParameter rotation, hkVector4fParameter scale);
        HK_FORCE_INLINE hkQsTransformf(hkVector4fParameter translation, hkQuaternionfParameter rotation);
        HK_FORCE_INLINE hkQsTransformf(const hkQsTransformf& other); // 0x00F95E30

#endif

        HK_FORCE_INLINE void set(hkVector4fParameter translation, hkQuaternionfParameter rotation, hkVector4fParameter scale); // 0x00FB7320
        HK_FORCE_INLINE void set(hkVector4fParameter translation, hkQuaternionfParameter rotation); // 0x00E731E0
        HK_FORCE_INLINE void setIdentity(); // 0x00D95E40
        HK_FORCE_INLINE static const hkQsTransformf& HK_CALL getIdentity(); // 0x00D95900
        HK_FORCE_INLINE const hkVector4f& getTranslation() const; // 0x00CF0660
        HK_FORCE_INLINE void setTranslation(hkVector4fParameter t); // 0x00F96E80
        HK_FORCE_INLINE const hkVector4f& getScale() const; // 0x00CF0680
        HK_FORCE_INLINE void setScale(hkVector4fParameter s); // 0x00F9A640
        HK_FORCE_INLINE const hkQuaternionf& getRotation() const; // 0x00CF0670
        HK_FORCE_INLINE void setRotation(hkQuaternionfParameter rotation); // 0x00F96E90
        HK_FORCE_INLINE void setRotation(const hkRotationf& rotation);
        HK_FORCE_INLINE void setInterpolate4( const hkQsTransformf& a, const hkQsTransformf& b, hkSimdFloat32Parameter t);
        void setFromTransformNoScale (const hkTransformf& transform); // 0x00FB6D30
        void copyToTransformNoScale (hkTransformf& transformOut) const; // 0x00FB6D60
        void setFromTransform (const hkTransformf& transform); // 0x00FB7240
        void setFromTransform(const hkQTransformf& qt); // 0x00FB6D80
        void copyToTransform (hkTransformf& transformOut) const; // 0x00FB6EC0
        HK_FORCE_INLINE void setInverse( const hkQsTransformf &t );
        HK_FORCE_INLINE void setMul( const hkQsTransformf &t1, const hkQsTransformf &t2 ); // 0x00F1EE30
        HK_FORCE_INLINE void setMulScaled( const hkQsTransformf &t1, const hkQsTransformf &t2 ); // 0x00E73210
        HK_FORCE_INLINE void setMulInverseMul( const hkQsTransformf& t1, const hkQsTransformf &t2 );
        HK_FORCE_INLINE void setMulMulInverse( const hkQsTransformf &t1, const hkQsTransformf &t2 );
        HK_FORCE_INLINE void setMulEq( const hkQsTransformf& b );
        void get4x4ColumnMajor(hkFloat32* HK_RESTRICT p) const; // 0x00FB6E10
        bool set4x4ColumnMajor(const hkFloat32* p); // 0x00FB71A0
        bool set(const hkMatrix4f& m); // 0x00FB71F0
        bool isOk(const hkFloat32 epsilon = hkFloat32(1e-3f)) const; // 0x00FB6DA0
        bool isApproximatelyEqual( const hkQsTransformf& other, hkFloat32 epsilon=hkFloat32(1e-3f) ) const; // 0x00FB6ED0
        HK_FORCE_INLINE hkBool32 isScaleUniform() const;
            /*
            ** Blending operations
            */

        HK_FORCE_INLINE void setZero();
        HK_FORCE_INLINE void blendAddMul(const hkQsTransformf& other, hkSimdFloat32Parameter weight );
        HK_FORCE_INLINE void blendAddMul(const hkQsTransformf& other );
        HK_FORCE_INLINE void blendWeight(hkSimdFloat32Parameter weight );
        HK_FORCE_INLINE void blendNormalize( hkSimdFloat32Parameter totalWeight );
        HK_FORCE_INLINE void blendNormalize();
        HK_FORCE_INLINE void fastRenormalize( hkSimdFloat32Parameter totalWeight );
        HK_FORCE_INLINE void fastRenormalize();
        static void HK_CALL fastRenormalizeBatch( hkQsTransformf* poseOut, hkFloat32* weight, hkUint32 numTransforms); // 0x00FB70D0
        static void HK_CALL fastRenormalizeBatch( hkQsTransformf* poseOut, hkFloat32 weight, hkUint32 numTransforms); // 0x00FB7140
        static void HK_CALL fastRenormalizeQuaternionBatch( hkQsTransformf* poseOut, hkUint32 numTransforms ); // 0x00FB6F80
    public:

        hkVector4f m_translation;
        hkQuaternionf m_rotation;
        hkVector4f m_scale;
};

#endif //HK_MATH_QSTRANSFORMf_H
