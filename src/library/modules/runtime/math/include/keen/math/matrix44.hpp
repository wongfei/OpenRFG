// Original: library/modules/runtime/math/include/keen/math/matrix44.hpp
// Implementation: library/modules/runtime/math/sources/global/matrix44.cpp
//
// Matrix44, a full 4x4 matrix (projection and view-projection matrices).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector4.hpp"

namespace keen
{
    struct Matrix33;
    struct Matrix43;
    struct Vector3;

    // size 0x40
    struct Matrix44
    {
        Matrix44(float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float);
        Matrix44(Vector4 right, Vector4 up, Vector4 at, Vector4 pos); // 0x00C3BE30 (matrix44.inl:8)
        Matrix44(const Matrix44& other); // 0x00CC8380 (matrix44.inl:24)
        Matrix44(const Matrix33&, Vector3);
        Matrix44(); // 0x004B2460 (matrix44.inl:4)
        Matrix44& create(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33); // 0x00C40280 (:18)
        static const Matrix44& getUnit();
        Matrix44& createUnit(); // 0x00C60F30 (matrix44.inl:60)
        Matrix44& createDiagonal(Vector4);
        Matrix44& createDiagonal(float value); // 0x00C5A630 (matrix44.inl:65)
        Matrix44& createFromAffineMatrix(const Matrix43& source); // 0x00C40340 (:28)
        Matrix44& createFromRotationMatrix(const Matrix33& source); // 0x00C40430 (:38)
        Matrix44& createRotationX(float angle); // 0x00C40500 (:48)
        Matrix44& createRotationY(float angle); // 0x00C405B0 (:61)
        Matrix44& createRotationZ(float angle); // 0x00C40650 (:74)
        Matrix44& createRotationAxisAngle(Vector3 axis, float angle); // 0x00C408E0 (:127)
        Matrix44& createScale3d(float scaleX, float scaleY, float scaleZ); // 0x00C407E0 (:107)
        Matrix44& createScale3d(Vector3 scale); // 0x00C40760 (:97)
        Matrix44& createScale3d(float scale); // 0x00C406F0 (:87)
        Matrix44& createTranslation(Vector3 translation); // 0x00C40860 (:117)
        Matrix44& createProjectionMatrixFovLH(float fovY, float aspect, float z0, float z1); // 0x00C40AB0 (:152)
        Matrix44& createProjectionMatrixFovRH(float fovY, float aspect, float z0, float z1); // 0x00C40B80 (:168)
        Matrix44& createOrthographicProjectionMatrixLH(float x0, float x1, float y0, float y1, float z0, float z1); // 0x00C40C40 (:184)
        Matrix44& createOrthographicProjectionMatrixRH(float x0, float x1, float y0, float y1, float z0, float z1); // 0x00C40D70 (:198)
        Matrix44& invert(const Matrix43& other); // 0x00C42A00 (:415)
        Matrix44& invert(const Matrix44& other); // 0x00C41F60 (:353)
        Matrix44& invert(); // 0x00C414C0 (:291)
        Matrix44& invert34(const Matrix44& other); // 0x00C40E90 (:223)
        Matrix44& invert34(); // 0x00C4EEB0 (:212)
        Matrix44& transpose(const Matrix43& other); // 0x00C42AD0 (:445)
        Matrix44& transpose(const Matrix44& other); // 0x00C42A30 (:433)
        Matrix44& transpose(); // 0x00C4F480 (:422)
        Matrix44& mulMatrix(const Matrix43& first, const Matrix44& second); // 0x00C5AB70 (matrix44.inl:468)
        Matrix44& mulMatrix(const Matrix44& first, const Matrix43& second); // 0x00C5A690 (matrix44.inl:361)
        Matrix44& mulMatrix(const Matrix44&, const Matrix44&);
        Matrix44& mulMatrix(const Matrix43& other); // 0x00C60F90 (matrix44.inl:162)
        Matrix44& mulMatrix(const Matrix44&);
        Vector4& transformVector(Vector4* pTarget, Vector4 source) const; // 0x00C3C000 (matrix44.inl:625)
        Vector3& transformVector(Vector3* pTarget, Vector3 source) const; // 0x00C3BE90 (matrix44.inl:550)
        Vector3& transformNormal(Vector3*, Vector3) const;

    protected:
        static const Matrix44 Unit; // 0x038BCBC0
    public:
        Vector4 x; // 0x00
        Vector4 y; // 0x10
        Vector4 z; // 0x20
        Vector4 w; // 0x30
    };
}
