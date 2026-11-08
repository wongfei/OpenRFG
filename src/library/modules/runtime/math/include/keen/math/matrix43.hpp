// Original: library/modules/runtime/math/include/keen/math/matrix43.hpp
// Implementation: library/modules/runtime/math/sources/global/matrix43.cpp
//
// Matrix43, an affine transform: a Matrix33 rotation/scale plus a Vector3 translation (the world and view matrices of
// the renderer).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/matrix33.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    struct Quaternion;

    // size 0x30
    struct Matrix43
    {
        Matrix43(Vector3 nx, Vector3 ny, Vector3 nz, Vector3 npos); // 0x00C3AC90 (matrix43.inl:12)
        Matrix43(); // 0x0048A5A0 (matrix43.inl:8)
        static const Matrix43& getUnit(); // 0x004B23C0 (matrix43.inl:20)
        void createUnit(); // 0x0048A5B0 (matrix43.inl:25)
        Matrix43& mulMatrix(const Matrix43& lhs, const Matrix43& rhs); // 0x00C5A1E0 (matrix43.inl:279)
        Matrix43& mulMatrix(const Matrix43&);
        Vector3& mulVector(Vector3* pTarget, Vector3 vector) const; // 0x00C32970 (matrix43.inl:416)
        Vector3& mulNormal(Vector3*, Vector3) const;
        void decompose(Quaternion* pRotation, Vector3* pScale, Vector3* pTranslation) const; // 0x00C57670 (:17)
        bool tryDecompose(Quaternion* pRotation, Vector3* pScale, Vector3* pTranslation) const; // 0x00C57E80 (:51)
        void compose(Quaternion rotation, Vector3 scale, Vector3 translation); // 0x00C3F750 (:88)
        void setX(Vector3);
        void setY(Vector3);
        void setZ(Vector3);
        void setPos(Vector3);
        const Vector3& getX() const;
        const Vector3& getY() const;
        const Vector3& getZ() const;
        const Vector3& getPos() const;
        void rotateX(float);
        void rotateY(float);
        void rotateZ(float);
        bool isEqual(const Matrix43& other, float epsilon) const; // 0x00C4E4E0 (:167)
        void invert(const Matrix43& other); // 0x00C3FA60 (:102)
        void invert(); // 0x00C4DF10 (:95)
        void invertOrthoNormal(const Matrix43& other); // 0x00C40150 (:182)
        void invertOrthoNormal(); // 0x00C40020 (:173)

    protected:
        static const Matrix43 Unit; // 0x038BCC10
    public:
        Matrix33 rot; // 0x00
        Vector3 pos; // 0x24
    };

    // 0x0048A5A0 (matrix43.inl:8)
    // Uninitialized.
    inline Matrix43::Matrix43()
    {
    }
}
