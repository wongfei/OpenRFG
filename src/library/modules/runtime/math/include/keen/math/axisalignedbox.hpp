// Original: library/modules/runtime/math/include/keen/math/axisalignedbox.hpp
// Implementation: library/modules/runtime/math/sources/global/axisalignedbox.inl
//
// AxisAlignedBox, a min/max box (inline operations in sources/global/axisalignedbox.inl).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x18
    struct AxisAlignedBox
    {
        AxisAlignedBox(Vector3 point1, Vector3 point2); // 0x00C76CB0 (axisalignedbox.inl:8)
        AxisAlignedBox(); // 0x00C749C0 (.h:17)
        void create(const Vector3*, uint32);
        void create(Vector3 point1, Vector3 point2); // 0x00C749D0 (axisalignedbox.inl:18)
        void create(Vector3);
        void createUnion(AxisAlignedBox, AxisAlignedBox);
        void createIntersection(AxisAlignedBox, AxisAlignedBox);
        void createFromSphere(Vector3, float);
        void createFromCenterHalfSize(Vector3, Vector3);
        const Vector3& getMin() const; // 0x00C3ACE0 (.h:29)
        const Vector3& getMax() const; // 0x00C3ACF0 (.h:30)
        void clear(); // 0x00CBE5B0 (axisalignedbox.inl:168)
        void invalidate(); // 0x00C74B60 (axisalignedbox.inl:174)
        bool isValid() const;
        bool contains(Vector3) const;
        void extend(float minX, float maxX, float minY, float maxY, float minZ, float maxZ); // 0x00C74AA0 (axisalignedbox.inl:80)
        void move(Vector3);
        void move(float, float, float);
        void clip(Vector3, Vector3);
        void insert(AxisAlignedBox);
        void insert(Vector3 point); // 0x00C5E5D0 (axisalignedbox.inl:133)
        void insertSphere(Vector3, float);
        bool operator!=(AxisAlignedBox) const;
        float getWidth() const; // 0x00C3AD00 (.h:48)
        float getHeight() const; // 0x00C3AD10 (.h:49)
        float getLength() const; // 0x00C3AD20 (.h:50)
        Vector3 getCenter() const; // 0x00C3ADA0 (axisalignedbox.inl:199)
        void getCenter(Vector3* pResult) const; // 0x00C3AD30 (axisalignedbox.inl:191)
        float getMaxDimensionLength() const; // 0x00C76D90 (axisalignedbox.inl:206)
        float getMinDimensionLength() const;
        void getMaxDimensionRange(float*, float*) const;
        Vector3 getSize() const;
        Vector3 getHalfSize() const;
        void getCornerPoints(Vector3* points) const; // 0x00C3AE10 (axisalignedbox.inl:257)
        bool isOverlapping(AxisAlignedBox) const;

        Vector3 m_min; // 0x00
        Vector3 m_max; // 0x0C
    };
}
