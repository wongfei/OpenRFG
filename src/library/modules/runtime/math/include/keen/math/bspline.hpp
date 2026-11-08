// Original: library/modules/runtime/math/include/keen/math/bspline.hpp
// Implementation: library/modules/runtime/math/sources/global/bspline.cpp
//
// BicubicSpline, a namespace of uniform cubic B-spline helpers over Vector3 control points; the generic templates
// (Generic<T, Accessor>, VectorAccessor<T>) are instantiated for Vector3 only.
// The templates are written by hand; addresses are of the <Vector3> instantiations.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    struct Vector3;

    namespace BicubicSpline
    {
        // size 0x1; the vector operations Generic uses, for T = Vector3
        template<typename T>
        struct VectorAccessor
        {
            static const T& get(const T* pData, uint32 index); // 0x00C45A20 (.h:37)
            static const T& add(T* pResult, const T& value1, const T& value2); // 0x00C45A30 (.h:41)
            static const T& sub(T* pResult, const T& value1); // 0x00C45AC0 (.h:45)
            static const T& sub(T* pResult, const T& value1, const T& value2); // 0x00C45B40 (.h:49)
            static const T& scale(T* pResult, float scalarValue); // 0x00C45BD0 (.h:53)
            static float getLength(const T& value); // 0x00C56390 (.h:59)
            static const T& setScaled(T* pResult, const T& value1, float scalarValue); // 0x00C45C30 (.h:63)
            static const T& addScaled(T* pResult, const T& value1, float scalarValue); // 0x00C45CA0 (.h:71)
            static void clear(T* pResult); // 0x00C45D40 (.h:75)
            static float getSquareLength(const T& value1); // 0x00C45D60 (.h:79)
            static void normalize(T*);
            static float dotProduct(const T& value1, const T& value2); // 0x00C45D90 (.h:88)
        };

        // The spline algorithms, generic over the control point type; the free functions in bspline.cpp forward to
        // Generic<Vector3, VectorAccessor<Vector3>>.
        template<typename T, typename Accessor>
        struct Generic
        {
            static float getArcLength(const T* pCvs, uint32 controlPointCount); // 0x00C59120 (.h:117)
            static float getArcLength(const T& p0, const T& p1, const T& p2, const T& p3); // 0x00C589D0 (.h:131)
            static void getPosition(T* pPosition, float linearPos, const T* pCvs, uint32 controlPointCount, bool closed); // 0x00C54C10 (.h:189)
            static void getTangent(T* pTangent, float linearPos, const T* pCvs, uint32 controlPointCount, bool closed); // 0x00C54F40 (.h:229)
            static float getDistancePos(const T& pos, const T* pCvs, uint32 controlPointCount, bool closed); // 0x00C552B0 (.h:266)
            static float getNewLinearPosition(float oldLinearPos, float distance, bool forward, const T* pCvs, uint32 controlPointCount, bool closed, bool noLoop, float maxError); // 0x00C55670 (.h:325)
            static float getNearestCV(const T& pos, const T* pCvs, uint32 controlPointCount); // 0x00C55990 (.h:428)
            static float getNearestPoint(const T& pos, const T* pCvs, uint32 controlPointCount, bool closed); // 0x00C55DC0 (.h:449)
            static float getNearestPoint2(const T& pos, const T* pCvs, uint32 controlPointCount, float aspect); // 0x00C56110 (.h:503)
            static void calculateConstantSpeedTime(float* pSplineTime, float normalizedTime, const T* pCvs, uint32 controlPointCount, bool closed, float* pSplineArcLength); // 0x00C59990 (.h:545)
            static void getConstantSpeedPosition(T* pPosition, float* pSplineTime, float normalizedTime, const T* pCvs, uint32 controlPointCount, bool closed, float* pSplineArcLength); // 0x00C59D00 (.h:630)
        };
    }

    namespace BicubicSpline
    {
        float getLinearLength(uint32 controlPointCount); // 0x00C3B9A0 (:10)
        bool isKeyFrame(uint32 controlPointIndex); // 0x00C3B9D0 (:15)
        bool isPointAfterKeyFrame(uint32 controlPointIndex); // 0x00C3B9F0 (:20)
        bool isPointBeforeKeyFrame(uint32 controlPointIndex); // 0x00C3BA10 (:25)
        float getArcLength(const Vector3* pCvs, uint32 controlPointCount); // 0x00C59D90 (:30)
        float getArcLength(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3); // 0x00C59110 (:35)
        void getPosition(Vector3* pPosition, float linearPos, const Vector3* pCvs, uint32 controlPointCount, bool closed); // 0x00C56530 (:40)
        void getTangent(Vector3* pTangent, float linearPos, const Vector3* pCvs, uint32 controlPointCount, bool closed); // 0x00C56560 (:45)
        float getDistancePos(const Vector3& pos, const Vector3* pCvs, uint32 controlPointCount, bool closed); // 0x00C56590 (:50)
        float getNewLinearPosition(float oldLinearPos, float distance, bool forward, const Vector3* pCvs, uint32 controlPointCount, bool closed, bool noLoop, float maxError); // 0x00C565A0 (:55)
        float getNearestCV(const Vector3& pos, const Vector3* pCvs, uint32 controlPointCount); // 0x00C565F0 (:60)
        float getNearestPoint(const Vector3& pos, const Vector3* pCvs, uint32 controlPointCount, bool closed); // 0x00C56600 (:65)
        float getNearestPoint2(const Vector3& pos, const Vector3* pCvs, uint32 controlPointCount, float aspect); // 0x00C56610 (:70)
        void calculateConstantSpeedTime(float* pSplineTime, float normalizedTime, const Vector3* pCvs, uint32 controlPointCount, bool closed, float* pSplineArcLength); // 0x00C59DA0 (:75)
        void getConstantSpeedPosition(Vector3* pPosition, float* pSplineTime, float normalizedTime, const Vector3* pCvs, uint32 controlPointCount, bool closed, float* pSplineArcLength); // 0x00C59DD0 (:80)
    }
}
