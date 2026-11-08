// Original: library/modules/runtime/math/sources/global/matrix43.cpp
//
// Partial (SFX.md SX1): Matrix43::Unit, getUnit and the general inverse the sound system's listener uses. Float rule as
// keen sound: products that are summed or subtracted before the store are computed in double.

#include "keen/math/matrix43.hpp"

namespace keen
{
    // The dynamic initializer of Unit (0x0108F460): the identity.
    static Matrix43 createUnitMatrix()
    {
        Matrix43 unit;
        unit.rot.x.x = 1.0f;
        unit.rot.x.y = 0.0f;
        unit.rot.x.z = 0.0f;
        unit.rot.y.x = 0.0f;
        unit.rot.y.y = 1.0f;
        unit.rot.y.z = 0.0f;
        unit.rot.z.x = 0.0f;
        unit.rot.z.y = 0.0f;
        unit.rot.z.z = 1.0f;
        unit.pos.x = 0.0f;
        unit.pos.y = 0.0f;
        unit.pos.z = 0.0f;
        return unit;
    }

    const Matrix43 Matrix43::Unit = createUnitMatrix(); // 0x038BCC10

    // 0x004B23C0 (matrix43.inl:20)
    const Matrix43& Matrix43::getUnit()
    {
        return Unit;
    }

    // 0x00C3FA60 (:102)
    // The adjugate (cofactors with the translation part) divided by the determinant.
    void Matrix43::invert(const Matrix43& other)
    {
        const double o0 = other.rot.x.x, o1 = other.rot.x.y, o2 = other.rot.x.z;
        const double o3 = other.rot.y.x, o4 = other.rot.y.y, o5 = other.rot.y.z;
        const double o6 = other.rot.z.x, o7 = other.rot.z.y, o8 = other.rot.z.z;
        const double o9 = other.pos.x, o10 = other.pos.y, o11 = other.pos.z;

        // Products with the translation, rounded to float.
        const double zyPz = (float)(o7 * o11);
        const double zzPy = (float)(o8 * o10);
        const double zxPz = (float)(o6 * o11);
        const double zxPy = (float)(o6 * o10);
        const double pxZz = (float)(o9 * o8);
        const double pxZy = (float)(o9 * o7);

        rot.x.x = (float)(o8 * o4 - o5 * o7);
        rot.y.x = (float)(o5 * o6 - o8 * o3);
        rot.z.x = (float)(o3 * o7 - o6 * o4);
        const float posX = (float)(o3 * zzPy + zxPz * o4 + o5 * pxZy);
        pos.x = (float)((double)posX - (o3 * zyPz + pxZz * o4 + o5 * zxPy));

        rot.x.y = (float)(o7 * o2 - o8 * o1);
        rot.y.y = (float)(o8 * o0 - o6 * o2);
        rot.z.y = (float)(o1 * o6 - o0 * o7);
        const float posY = (float)(o0 * zyPz + o1 * pxZz + zxPy * o2);
        pos.y = (float)((double)posY - (o0 * zzPy + o1 * zxPz + pxZy * o2));

        const double xyYz = (float)(o1 * o5);
        const double xzYy = (float)(o2 * o4);
        const double xxYy = (float)(o0 * o4);
        const double xxYz = (float)(o0 * o5);
        const double xyYx = (float)(o1 * o3);
        const double xzYx = (float)(o2 * o3);
        rot.x.z = (float)(xyYz - xzYy);
        rot.y.z = (float)(xzYx - xxYz);
        rot.z.z = (float)(xxYy - xyYx);
        const float posZ = (float)(o10 * xxYz + o9 * xzYy + xyYx * o11);
        pos.z = (float)((double)posZ - (o10 * xzYx + o9 * xyYz + xxYy * o11));

        const float determinant = (float)(o0 * rot.x.x + o1 * rot.y.x + (double)rot.z.x * o2);
        const float inverseDeterminant = (float)(1.0 / (double)determinant);
        rot.x.x *= inverseDeterminant;
        rot.x.y *= inverseDeterminant;
        rot.x.z *= inverseDeterminant;
        rot.y.x *= inverseDeterminant;
        rot.y.y *= inverseDeterminant;
        rot.y.z *= inverseDeterminant;
        rot.z.x *= inverseDeterminant;
        rot.z.y *= inverseDeterminant;
        rot.z.z *= inverseDeterminant;
        pos.x *= inverseDeterminant;
        pos.y *= inverseDeterminant;
        pos.z *= inverseDeterminant;
    }
}
