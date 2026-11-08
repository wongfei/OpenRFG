// Original: library/modules/runtime/math/include/keen/math/matrix33.hpp
// Implementation: library/modules/runtime/math/sources/global/matrix33.cpp
//
// Matrix33, a 3x3 rotation/scale matrix stored as three row vectors.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x24
    struct Matrix33
    {
        Matrix33(Vector3, Vector3, Vector3);
        Matrix33(); // 0x0048A590 (.h:19)
        Matrix33& createScale(float scaleX, float scaleY, float scaleZ); // 0x00C3F470 (:220)
        Matrix33& createScale(Vector3 scaleVector); // 0x00C3F420 (:180)
        Matrix33& createScale(float scaleValue); // 0x00C3F3E0 (:170)
        Matrix33& createXRotate(float value); // 0x00C3F290 (:110)
        Matrix33& createYRotate(float value); // 0x00C3F300 (:130)
        Matrix33& createZRotate(float value); // 0x00C3F370 (:150)
        Matrix33& createAxisRotation(Vector3 axis, float value); // 0x00C4CEF0 (:85)
        Matrix33& createFromViewUpVector(Vector3 view, Vector3 up); // 0x00C4C4D0 (:15)
        Matrix33& createFromUpVector(Vector3 up); // 0x00C4C6B0 (:40)
        Matrix33& createFromViewVector(Vector3 view); // 0x00C4CAE0 (:63)
        Matrix33& createUnit(); // 0x00C5A1A0 (matrix33.inl:124)
        Matrix33& mulMatrix(const Matrix33& first, const Matrix33& second); // 0x00C46F70 (matrix33.inl:242)
        Matrix33& mulMatrix(const Matrix33&);
        Vector3& mulVector(Vector3* pTarget, Vector3 vector) const; // 0x00C3ABB0 (matrix33.inl:278)
        Matrix33& scale(float);
        Matrix33& scale(float, float, float);
        Matrix33& scale(Vector3 scaleVector); // 0x00C3A990 (matrix33.inl:50)
        void setX(Vector3 vec); // 0x00C3A930 (matrix33.inl:11)
        void setY(Vector3 vec); // 0x00C3A950 (matrix33.inl:16)
        void setZ(Vector3 vec); // 0x00C3A970 (matrix33.inl:21)
        void rotateX(float);
        void rotateY(float);
        void rotateZ(float);
        void invert(const Matrix33&);
        void invert();
        bool tryInvert(const Matrix33& other, float epsilon); // 0x00C47250 (matrix33.inl:434)
        float getDeterminant() const; // 0x00C3AA80 (matrix33.inl:75)
        Matrix33& transpose(const Matrix33& other); // 0x00C3AB70 (matrix33.inl:109)
        Matrix33& transpose(); // 0x00C3AB30 (matrix33.inl:86)
        void normalize(); // 0x00C46DC0 (matrix33.inl:68)
        Matrix33& orthonormalize(); // 0x00C4D310 (:230)
        bool isOrthogonal(float epsilon) const; // 0x00C4D0F0 (:190)
        bool isValidRotationScaleMatrix(float epsilon) const; // 0x00C4D2F0 (:209)
        bool isEqual(const Matrix33& other, float epsilon) const; // 0x00C4D7B0 (:261)
        Vector3 getX() const;
        Vector3 getY() const;
        Vector3 getZ() const;
        static const Matrix33& getUnit();

        Vector3 x; // 0x00
        Vector3 y; // 0x0C
        Vector3 z; // 0x18
    };

    // 0x0048A590 (.h:19)
    // Uninitialized.
    inline Matrix33::Matrix33()
    {
    }
}
