// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/extendedmeshshape/hkpextendedmeshshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"

class hkAabb;
class hkClass;
struct hkInternalClassMember;
class hkTransformf;
class hkpMeshMaterial;
class hkpMeshShape;
struct hkpShapeBufferStorage;

// size 0xF0
class hkpExtendedMeshShape : public hkpShapeCollection
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 13,
    };

    enum IndexStridingType
    {
        INDICES_INVALID = 0,
        INDICES_INT8 = 1,
        INDICES_INT16 = 2,
        INDICES_INT32 = 3,
        INDICES_MAX_ID = 4,
    };

    enum MaterialIndexStridingType
    {
        MATERIAL_INDICES_INVALID = 0,
        MATERIAL_INDICES_INT8 = 1,
        MATERIAL_INDICES_INT16 = 2,
        MATERIAL_INDICES_MAX_ID = 3,
    };

    enum SubpartType
    {
        SUBPART_TRIANGLES = 0,
        SUBPART_SHAPE = 1,
        SUBPART_TYPE_MAX = 2,
    };

    enum SubpartTypesAndFlags
    {
        SUBPART_TYPE_MASK = 1,
        SUBPART_MATERIAL_INDICES_MASK = 6,
        SUBPART_MATERIAL_INDICES_SHIFT = 1,
        SUBPART_NUM_MATERIALS_MASK = 65528,
        SUBPART_NUM_MATERIALS_SHIFT = 3,
    };

    // size 0x14
    struct Subpart
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Subpart); // new(unsigned int n, void* p) 0x00D7CEF0
        static const hkClass& staticClass(); // 0x00D7CF30
        HK_DECLARE_POD_TYPE();
        Subpart(hkFinishLoadedObjectFlag flag); // 0x00D7CF00
        Subpart(SubpartType type); // 0x00D936A0
        SubpartType getType() const; // 0x00E277C0
        void setType(SubpartType newValue); // 0x00D93630
        MaterialIndexStridingType getMaterialIndexStridingType() const; // 0x00D93650
        void setMaterialIndexStridingType(MaterialIndexStridingType newValue); // 0x00D93660
        unsigned short getNumMaterials() const; // 0x00DA0330
        void setNumMaterials(unsigned short newValue); // 0x00D93680

        static const hkInternalClassMember Members[]; // 0x013549E0
    public:
        unsigned short m_typeAndFlags; // 0x00
    public:
        unsigned short m_shapeInfo; // 0x02
        short m_materialStriding; // 0x04
        unsigned short m_materialIndexStriding; // 0x06
        const void* m_materialIndexBase; // 0x08
        const hkpMeshMaterial* m_materialBase; // 0x0C
        unsigned long m_userData; // 0x10
    };

    // size 0x70
    struct TrianglesSubpart : public Subpart
    {
        static const hkClass& staticClass(); // 0x00D7CF60
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(TrianglesSubpart); // new(unsigned int n, void* p) 0x00D7CF10
        HK_DECLARE_POD_TYPE();
        TrianglesSubpart(hkFinishLoadedObjectFlag flag); // 0x00D7D040
        TrianglesSubpart(); // 0x00D95EF0
        void setTranslation(const hkVector4f&);
        void setRotation(const hkQuaternionf&);
        void setScaling(const hkVector4f&);
        const hkVector4f& getTranslation() const;
        const hkQuaternionf& getRotation() const;
        const hkVector4f& getScaling() const;

        static const hkInternalClassMember Members[];
        int m_numTriangleShapes; // 0x14
        const float* m_vertexBase; // 0x18
        int m_numVertices; // 0x1C
        const void* m_indexBase; // 0x20
        unsigned short m_vertexStriding; // 0x24
        int m_triangleOffset; // 0x28
        unsigned short m_indexStriding; // 0x2C
        hkEnum<enum IndexStridingType,signed char> m_stridingType; // 0x2E
        signed char m_flipAlternateTriangles; // 0x2F
        hkVector4f m_extrusion; // 0x30
        hkQsTransformf m_transform; // 0x40
    };

    // size 0x40
    struct ShapesSubpart : public Subpart
    {
        enum
        {
            FLAG_NONE = 0,
            FLAG_TRANSLATE_SET = 1,
            FLAG_ROTATE_SET = 2,
        };

        static const hkClass& staticClass(); // 0x00D7CF80
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ShapesSubpart); // new(unsigned int n, void* p) 0x00D7CF20, delete(void* p, unsigned int nbytes) 0x00D7D050
        ShapesSubpart(hkFinishLoadedObjectFlag flag); // 0x00D94780
        ShapesSubpart(const ShapesSubpart&);
        ShapesSubpart(const hkpConvexShape* const * childShapes, int numChildShapes, const hkTransformf& transform); // 0x00D94980
        ShapesSubpart(const hkpConvexShape* const * childShapes, int numChildShapes, const hkVector4f& offset); // 0x00D947A0
        ShapesSubpart(); // 0x00D96DB0
        void afterReflectNew(); // 0x00D93BE0
        ~ShapesSubpart(); // 0x00D94B80
        void setFlags(int f); // 0x00D95940
        int getFlags() const; // 0x00D95950
        const hkVector4f& getTranslation() const; // 0x00D935C0
        const hkQuaternionf& getRotation() const; // 0x00D935D0

        static const hkInternalClassMember Members[]; // 0x01354B10
        hkArray<hkRefPtr<hkpConvexShape>,hkContainerHeapAllocator> m_childShapes; // 0x14
    public:
        hkQuaternionf m_rotation; // 0x20
        hkVector4f m_translation; // 0x30
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x0080D790, delete(void* p, unsigned int nbytes) 0x00831760
    static const hkClass& staticClass(); // 0x00D7CFB0
    hkpExtendedMeshShape(hkFinishLoadedObjectFlag flag); // 0x00D95420
    hkpExtendedMeshShape(const hkpMeshShape* meshShape); // 0x00D94FB0
    hkpExtendedMeshShape(float radius, int numBitsForSubpartIndex); // 0x00D94EC0
    void afterReflectNew(); // 0x00D94C40
    virtual ~hkpExtendedMeshShape(); // vtbl+0x00 0x00D95310
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00D936E0
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00D93960
    int getNumTrianglesSubparts() const; // 0x00D95960
    int getNumShapesSubparts() const; // 0x00D95970
    const TrianglesSubpart& getTrianglesSubpartAt(int i) const; // 0x00DA1970
    TrianglesSubpart& getTrianglesSubpartAt(int);
    const ShapesSubpart& getShapesSubpartAt(int i) const; // 0x00DA1980
    ShapesSubpart& getShapesSubpartAt(int);
    const Subpart& getSubPart(unsigned int key) const; // 0x00E27B90
    int getSubPartIndex(unsigned int key) const; // 0x00D935E0
    int getTerminalIndexInSubPart(unsigned int key) const; // 0x00D93610
    int getNumBitsForSubpartIndex() const; // 0x00DA0310
    int getSubpartShapeKeyBase(int) const;
    SubpartType getSubpartType(unsigned int key) const; // 0x00D93600
    unsigned short getShapeInfo(unsigned int shapeKey) const; // 0x00E27BD0
    void setShapeInfo(unsigned int, unsigned short);
    const hkArray<TrianglesSubpart,hkContainerHeapAllocator>& getTriangleSubparts() const;
    const hkArray<ShapesSubpart,hkContainerHeapAllocator>& getShapesSubparts() const;
    const hkpMeshMaterial* getMeshMaterial(unsigned int key) const; // 0x00D93750
    float getRadius() const; // 0x00DA0320
    void setRadius(float);
    virtual unsigned int getNextKey(unsigned int initialKey) const; // vtbl+0x0C 0x00D93A00
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00D937F0
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00D93D90
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D93B20
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D938B0
    virtual void initWeldingInfo(hkpWeldingUtility::WeldingType weldingType); // vtbl+0x48 0x00D94510
    virtual void setWeldingInfo(unsigned int key, short weldingInfo); // vtbl+0x4C 0x00D93C70
    virtual void addTrianglesSubpart(const TrianglesSubpart& part); // vtbl+0x50 0x00D946C0
    virtual int addShapesSubpart(const ShapesSubpart& part); // vtbl+0x54 0x00D94E10
public:
    static void calcAabbExtents(const ShapesSubpart& part, hkAabb& out); // 0x00D93820
    static void calcAabbExtents(TrianglesSubpart& part, hkAabb& out); // 0x00D94280
public:
    void recalcAabbExtents(); // 0x00D94380
public:
    TrianglesSubpart* expandOneTriangleSubparts(); // 0x00D94490
    ShapesSubpart* expandOneShapesSubparts(); // 0x00D94D80
    void freeTriangleSubparts(); // 0x00D93720
    void freeShapesSubparts(); // 0x00D93730
    int _getNumChildShapesInTrianglesSubpart(const TrianglesSubpart& subpart, int subpartIndex) const; // 0x00D94630
    int _getNumChildShapesInShapesSubpart(const ShapesSubpart& subpart) const; // 0x00D93740
    void assertTrianglesSubpartValidity(const TrianglesSubpart& part); // 0x00D93700
    void assertShapesSubpartValidity(const ShapesSubpart& part); // 0x00D93710

public:
    static const hkInternalClassMember Members[]; // 0x016F5910
public:
    alignas(16) TrianglesSubpart m_embeddedTrianglesSubpart; // 0x20
public:
    hkVector4f m_aabbHalfExtents; // 0x90
    hkVector4f m_aabbCenter; // 0xA0
    const hkClass* m_materialClass; // 0xB0
    int m_numBitsForSubpartIndex; // 0xB4
public:
    hkArray<TrianglesSubpart,hkContainerHeapAllocator> m_trianglesSubparts; // 0xB8
    hkArray<ShapesSubpart,hkContainerHeapAllocator> m_shapesSubparts; // 0xC4
public:
    hkArray<unsigned short,hkContainerHeapAllocator> m_weldingInfo; // 0xD0
    hkEnum<enum hkpWeldingUtility::WeldingType,unsigned char> m_weldingType; // 0xDC
    unsigned int m_defaultCollisionFilterInfo; // 0xE0
    int m_cachedNumChildShapes; // 0xE4
public:
    float m_triangleRadius; // 0xE8
    int m_padding; // 0xEC
};

// template instance: const void* hkGetArrayElemWithByteStriding<void>(const void* base, int index, int striding, int dmaGroup, bool waitForCompletion); // 0x00D95720
// template instance: const unsigned short* hkGetArrayElemWithByteStriding<unsigned short>(const unsigned short* base, int index, int striding, int dmaGroup, bool waitForCompletion); // 0x00D95690
// template instance: const hkRefPtr<hkpConvexShape>* hkGetArrayElemWithByteStriding<hkRefPtr<hkpConvexShape> >(const hkRefPtr<hkpConvexShape>* base, int index, int striding, int dmaGroup, bool waitForCompletion); // 0x00D95700
// template instance: const hkpMeshMaterial* hkGetArrayElemWithByteStriding<hkpMeshMaterial>(const hkpMeshMaterial* base, int index, int striding, int dmaGroup, bool waitForCompletion); // 0x00D95730
// template instance: const void* hkGetArrayElemWithByteStridingHalfCacheSize<void>(const void* base, int index, int striding, int dmaGroup, bool waitForCompletion); // 0x00D95680
