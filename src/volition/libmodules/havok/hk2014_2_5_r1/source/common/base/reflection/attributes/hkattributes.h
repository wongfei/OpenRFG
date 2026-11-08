// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/attributes/hkattributes.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkInternalClassMember;

// size 0x1
struct hkAttributeHideCriteria
{
    enum Types
    {
        NONE = 0,
        MODELER_IS_MAX = 1,
        MODELER_IS_MAYA = 2,
        UI_SCHEME_IS_DESTRUCTION = 4,
        UI_SCHEME_IS_DESTRUCTION_2012 = 8,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkAttributeHideCriteria);
    static const hkClass& staticClass(); // 0x00CE9E00

    static const hkInternalClassMember Members[];
};

// size 0x10
struct hkRangeRealAttribute
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkRangeRealAttribute);
    static const hkClass& staticClass(); // 0x00CE9F20
    hkRangeRealAttribute(float absmin, float absmax, float softmin, float softmax) : m_absmin(absmin), m_absmax(absmax), m_softmin(softmin), m_softmax(softmax) {} // 0x00F14920

    static const hkInternalClassMember Members[];
    float m_absmin; // 0x00
    float m_absmax; // 0x04
    float m_softmin; // 0x08
    float m_softmax; // 0x0C
};

// size 0x10
struct hkRangeInt32Attribute
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkRangeInt32Attribute);
    static const hkClass& staticClass(); // 0x00CE9F00
    hkRangeInt32Attribute(int absmin, int absmax, int softmin, int softmax) : m_absmin(absmin), m_absmax(absmax), m_softmin(softmin), m_softmax(softmax) {} // 0x00F15200

    static const hkInternalClassMember Members[];
    int m_absmin; // 0x00
    int m_absmax; // 0x04
    int m_softmin; // 0x08
    int m_softmax; // 0x0C
};

// size 0x14
struct hkUiAttribute
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkUiAttribute);
    static const hkClass& staticClass(); // 0x00CE9F80

    static const hkInternalClassMember Members[];
    hkBool m_visible; // 0x00
    hkBool m_editable; // 0x01
    hkEnum<enum hkAttributeHideCriteria::Types,signed char> m_hideCriteria; // 0x02
    const char* m_label; // 0x04
    const char* m_group; // 0x08
    const char* m_hideBaseClassMembers; // 0x0C
    hkBool m_endGroup; // 0x10
    hkBool m_endGroup2; // 0x11
    hkBool m_advanced; // 0x12
};

// size 0xC
struct hkGizmoAttribute
{
    enum GizmoType
    {
        POINT = 0,
        SPHERE = 1,
        PLANE = 2,
        ARROW = 3,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkGizmoAttribute);
    static const hkClass& staticClass(); // 0x00CE9E80

    static const hkInternalClassMember Members[];
    hkBool m_visible; // 0x00
    const char* m_label; // 0x04
    hkEnum<enum GizmoType,signed char> m_type; // 0x08
};

// size 0x1
struct hkModelerNodeTypeAttribute
{
    enum ModelerType
    {
        DEFAULT = 0,
        LOCATOR = 1,
    };

    static const hkClass& staticClass(); // 0x00CE9EC0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkModelerNodeTypeAttribute);

    static const hkInternalClassMember Members[];
    hkEnum<enum ModelerType,signed char> m_type; // 0x00
};

// size 0x1
struct hkLinkAttribute
{
    enum Link
    {
        NONE = 0,
        DIRECT_LINK = 1,
        CHILD = 2,
        MESH = 3,
        PARENT_NAME = 4,
        IMGSELECT = 5,
        NODE_UUID = 6,
    };

    static const hkClass& staticClass(); // 0x00CE9EA0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkLinkAttribute);

    static const hkInternalClassMember Members[];
    hkEnum<enum Link,signed char> m_type; // 0x00
};

// size 0x1
struct hkSemanticsAttribute
{
    enum Semantics
    {
        UNKNOWN = 0,
        DISTANCE = 1,
        ANGLE = 2,
        NORMAL = 3,
        POSITION = 4,
        COSINE_ANGLE = 5,
    };

    static const hkClass& staticClass(); // 0x00CE9F60
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSemanticsAttribute);

    static const hkInternalClassMember Members[];
    hkEnum<enum Semantics,signed char> m_type; // 0x00
};

// size 0x4
struct hkDescriptionAttribute
{
    static const hkClass& staticClass(); // 0x00CE9E40
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDescriptionAttribute);

    static const hkInternalClassMember Members[];
    const char* m_string; // 0x00
};

// size 0x1
struct hkArrayTypeAttribute
{
    enum ArrayType
    {
        NONE = 0,
        POINTSOUP = 1,
        ENTITIES = 2,
    };

    static const hkClass& staticClass(); // 0x00CE9DE0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkArrayTypeAttribute);

    static const hkInternalClassMember Members[];
    hkEnum<enum ArrayType,signed char> m_type; // 0x00
};

// size 0x4
struct hkDataObjectTypeAttribute
{
    static const hkClass& staticClass(); // 0x00CE9E20
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataObjectTypeAttribute);

    static const hkInternalClassMember Members[];
    const char* m_typeName; // 0x00
};

// size 0x4
struct hkDocumentationAttribute
{
    static const hkClass& staticClass(); // 0x00CE9E60
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDocumentationAttribute);

    static const hkInternalClassMember Members[];
    const char* (*m_getDocsSectionTag)(hkAttributeHideCriteria::Types); // 0x00
};

// size 0x4
struct hkPostFinishAttribute
{
    static const hkClass& staticClass(); // 0x00CE9EE0
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkPostFinishAttribute);

    static const hkInternalClassMember Members[];
    void (*m_postFinishFunction)(void*); // 0x00
};

// size 0x1
struct hkScriptableAttribute
{
    static const hkClass& staticClass(); // 0x00CE9F40
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkScriptableAttribute);

    static const hkInternalClassMember Members[];
};
