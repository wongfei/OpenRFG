// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SD), from rfg.exe (decompile and asm; _readArrayItems,
// readBinaryValue and readClass from the asm, their decompiles are LTCG-garbled). Reads a binary tagfile into a
// data world: tagged records (file info, class metadata, objects), integers as zigzag-like varints (the sign in bit 0,
// six value bits in the first byte, seven in each following byte while bit 7 is set), strings as a length or a
// negative index into the strings read so far, objects as a bitfield of the members present followed by their
// values. Remembered objects are numbered in order; a reference to one not read yet is kept as a forward reference
// and assigned when the object arrives.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/iarchive/hkiarchive.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hktagstringlist.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

#include <new>

class hkOstreamLogger;

namespace
{
    // size 0x1C
    // A run of varints read through a buffer: each refill reads as many bytes as there are integers left, which
    // never passes the run's end (every integer takes at least one byte).
    struct IntReader
    {
        // :56 (0x00F6BBC0)
        IntReader(hkIArchive* archive, int size) : m_remaining(size), m_archive(archive)
        {
            m_buffer.setSize(size);
            m_buffer.setSizeUnchecked(0);
            m_cur = m_buffer.begin();
            m_end = m_buffer.begin() + m_buffer.getSize();
        }

        // :63 (0x00F6B5F0)
        hkUint8 _fillBuffer()
        {
            m_buffer.setSizeUnchecked(m_remaining);
            m_archive->readArrayGeneric(m_buffer.begin(), 1, m_remaining);
            m_cur = m_buffer.begin();
            m_end = m_buffer.begin() + m_buffer.getSize();
            return *m_cur++;
        }

        // :73 (0x00F6B620)
        hkUint8 readUint8()
        {
            if (m_cur >= m_end)
            {
                return _fillBuffer();
            }
            return *m_cur++;
        }

        // :77 (0x00F6B660)
        int readInt()
        {
            hkUint8 b = readUint8();
            unsigned int u = (b >> 1) & 63;
            const unsigned int neg = b & 1;
            int shift = 6;
            while (b & 0x80)
            {
                b = readUint8();
                u |= (unsigned int)(b & 0x7F) << shift;
                shift += 7;
            }
            m_remaining--;
            return neg ? -(int)u : (int)u;
        }

        // :94 (0x00F6BC40)
        void readArray(hkDataArray& arr)
        {
            const int n = m_remaining;
            hkArray<int, hkContainerTempAllocator> ints;
            ints.setSize(n);
            for (int i = 0; i < n; i++)
            {
                ints[i] = readInt();
            }
            arr.setAll(ints.begin(), n);
        }

        hkBool isAtEnd() const { return m_remaining == 0; }

        int m_remaining; // 0x00
        hkIArchive* m_archive; // 0x04
        const hkUint8* m_cur; // 0x08
        const hkUint8* m_end; // 0x0C
        hkArray<hkUint8, hkContainerTempAllocator> m_buffer; // 0x10
    };

    // size 0x50
    struct Reader
    {
        // size 0x18
        // The references to a remembered object that was not read yet.
        struct ForwardReferences
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ForwardReferences); // new 0x00F6B090, delete 0x00F6B0B0 (:931)

            // :933 (0x00F6C140). The handle is copy-constructed into the array as the SDK's hkArray::pushBack does:
            // the values' operator= assigns through the handle into the world (ours, pushBack, assigns).
            void remember(hkDataObject_Value value) { ::new (&m_objectRefs.expandOne()) hkDataObject_Value(value); }

            // :937 (0x00F6C190)
            void remember(hkDataArray_Value value) { ::new (&m_arrayRefs.expandOne()) hkDataArray_Value(value); }

            // :941 (0x00F6B0E0)
            void assign(hkDataObject& o)
            {
                for (int i = 0; i < m_arrayRefs.getSize(); i++)
                {
                    m_arrayRefs[i] = o;
                }
                for (int i = 0; i < m_objectRefs.getSize(); i++)
                {
                    m_objectRefs[i] = o;
                }
            }

            hkArray<hkDataArray_Value, hkContainerHeapAllocator> m_arrayRefs; // 0x00
            hkArray<hkDataObject_Value, hkContainerHeapAllocator> m_objectRefs; // 0x0C
        };

        // :133 (0x00F6C570): class 0 and strings 0 ("") and 1 (null) are predefined
        Reader(hkStreamReader* sr, hkDataWorld* cont)
            : m_tagfileVersion(-1), m_ia(sr, false), m_world(cont), m_log(nullptr), m_numPrevStringsStatic(0)
        {
            m_classes.pushBack(nullptr);
            m_prevStrings.pushBack(const_cast<char*>(""));
            m_prevStrings.pushBack(nullptr);
            m_numPrevStringsStatic = 2;
        }

        // :144 (0x00F6C670). Original bug kept: the forward references still pending (objects never read) are
        // leaked with the map.
        ~Reader()
        {
            for (int i = m_numPrevStringsStatic; i < m_prevStrings.getSize(); i++)
            {
                hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_prevStrings[i]);
            }
            m_forwardReferences.clearAndDeallocate();
        }

        hkResult readHeader(hkBinaryTagfile::Header& out);

        // :220 (0x00F6ACB0)
        hkUint64 readInt64()
        {
            hkUint8 b;
            m_ia.readArrayGeneric(&b, 1, 1);
            const unsigned int neg = b & 1;
            hkUint64 u = (b >> 1) & 63;
            int shift = 6;
            while (b & 0x80)
            {
                m_ia.readArrayGeneric(&b, 1, 1);
                u |= hkUint64(b & 0x7F) << shift;
                shift += 7;
            }
            return neg ? (hkUint64)(-(hkInt64)u) : u;
        }

        // :240 (<int> 0x00F6ADF0, <hkBinaryTagfile::TagType> 0x00F6AE60, <hkLegacyType::Type> 0x00F6AF10)
        template<typename T>
        T readInt()
        {
            hkUint8 b;
            m_ia.readArrayGeneric(&b, 1, 1);
            unsigned int u = (b >> 1) & 63;
            const unsigned int neg = b & 1;
            int shift = 6;
            while (b & 0x80)
            {
                m_ia.readArrayGeneric(&b, 1, 1);
                u |= (unsigned int)(b & 0x7F) << shift;
                shift += 7;
            }
            return (T)(neg ? -(int)u : (int)u);
        }

        // :268 (0x00F6B870): a new string (owned, appended to the list), or -index into the list
        const char* readString()
        {
            const int len = readInt<int>();
            if (len > 0)
            {
                char* s = static_cast<char*>(hkMemoryRouter::easyAlloc(hkMemoryRouter::getInstance().heap(), len + 1));
                m_ia.readRaw(s, len);
                s[len] = 0;
                m_prevStrings.pushBack(s);
                return s;
            }
            return m_prevStrings[-len];
        }

        // :285 (0x00F6AD60)
        float readFloat()
        {
            float f;
            m_ia.readArrayGeneric(&f, 4, 1);
            return f;
        }

        void readBitfield(int numMembers, hkLocalArray<hkUint8>& bout);
        void _readArrayInts(hkDataArray& arr, int size);
        void _readArrayItems(hkDataArray& arr, int nelem);
        void readBinaryValue(hkDataObject_Value value, const hkDataClass_MemberInfo& minfo);
        template<typename Value>
        void readObjectIntoValue(Value value);
        hkDataObject readObjectTopLevel(hkBinaryTagfile::TagType objectTag, const hkDataClassImpl* klassImpl);
        hkResult readClass();
        hkDataObject readObjectTree();

        int m_tagfileVersion; // 0x00
        hkIArchive m_ia; // 0x04
        hkDataWorld* m_world; // 0x14
        hkOstreamLogger* m_log; // 0x18
        hkArray<hkDataClassImpl*, hkContainerHeapAllocator> m_classes; // 0x1C
        hkArray<char*, hkContainerHeapAllocator> m_prevStrings; // 0x28
        int m_numPrevStringsStatic; // 0x34
        hkArray<hkDataObjectImpl*, hkContainerHeapAllocator> m_rememberedObjects; // 0x38
        hkPointerMap<int, ForwardReferences*, hkContainerHeapAllocator> m_forwardReferences; // 0x44
    };

    // :154 (0x00F6BDB0). Original bug kept: the unknown-version warning prints m_tagfileVersion (still -1 when the
    // header is read on its own) instead of the version read.
    hkResult Reader::readHeader(hkBinaryTagfile::Header& out)
    {
        hkUint32 magic1;
        m_ia.readArrayGeneric(&magic1, 4, 1);
        if (!m_ia.isOk())
        {
            return HK_FAILURE;
        }
        hkUint32 magic2;
        m_ia.readArrayGeneric(&magic2, 4, 1);
        if (!m_ia.isOk() || !hkBinaryTagfile::isBinaryMagic(magic1, magic2))
        {
            return HK_FAILURE;
        }
        out.m_magic1 = magic1;
        out.m_magic2 = magic2;
        out.m_fileInfo = readInt<int>();
        if (!m_ia.isOk())
        {
            return HK_FAILURE;
        }
        out.m_version = readInt<int>();
        if (!m_ia.isOk())
        {
            return HK_FAILURE;
        }
        switch (out.m_version)
        {
            case 0:
            case 1:
            case 2:
            case 3:
                return HK_SUCCESS;
            case 4:
            case 5:
            {
                m_rememberedObjects.pushBack(nullptr);
                out.m_sdk = readString();
                if (out.m_version != 4)
                {
                    hkUint16 v;
                    m_ia.readArrayGeneric(&v, 2, 1);
                    out.m_maxpredicate = v;
                    m_ia.readArrayGeneric(&v, 2, 1);
                    out.m_numPredicates = v;
                    if (!m_ia.isOk())
                    {
                        return HK_FAILURE;
                    }
                    for (int i = 0; i < out.m_numPredicates; i++)
                    {
                        m_ia.readArrayGeneric(&v, 2, 1);
                        out.m_truePredicates.pushBack(v);
                        if (!m_ia.isOk())
                        {
                            return HK_FAILURE;
                        }
                    }
                }
                return HK_SUCCESS;
            }
            default:
            {
                char buf[512];
                hkErrStream os(buf, sizeof(buf));
                os << "Unrecognised tagfile version " << m_tagfileVersion;
                hkErrorFwd::messageWarning(0x2AB6036F, buf, "Tagfile\\Binary\\hkBinaryTagfileReader.cpp", 208);
                return HK_FAILURE;
            }
        }
    }

    // :290 (0x00F6B970): one byte per member, from ceil(numMembers / 8) bytes of bits (low bit first)
    void Reader::readBitfield(int numMembers, hkLocalArray<hkUint8>& bout)
    {
        const int numMembersRounded = (numMembers + 7) & ~7;
        const int numBytes = numMembersRounded / 8;
        bout.setSize(numMembersRounded);
        hkUint8 bytes[64];
        m_ia.readArrayGeneric(bytes, 1, numBytes);
        for (int i = 0; i < numBytes; i++)
        {
            const hkUint8 b = bytes[i];
            bout[i * 8 + 0] = b & 1;
            bout[i * 8 + 1] = (b >> 1) & 1;
            bout[i * 8 + 2] = (b >> 2) & 1;
            bout[i * 8 + 3] = (b >> 3) & 1;
            bout[i * 8 + 4] = (b >> 4) & 1;
            bout[i * 8 + 5] = (b >> 5) & 1;
            bout[i * 8 + 6] = (b >> 6) & 1;
            bout[i * 8 + 7] = b >> 7;
        }
        bout.setSize(numMembers);
    }

    // :323 (0x00F6C760)
    void Reader::_readArrayInts(hkDataArray& arr, int size)
    {
        if (size > 0)
        {
            IntReader reader(&m_ia, size);
            reader.readArray(arr);
        }
    }

    // :333 (0x00F6CDE0): nelem items of the array's type
    void Reader::_readArrayItems(hkDataArray& arr, int nelem)
    {
        hkTypeManager::Type* type = arr.getType();
        switch (type->getSubType())
        {
            case hkTypeManager::SUB_TYPE_BYTE:
            {
                hkArray<hkUint8, hkContainerTempAllocator> buf(nelem);
                buf.setSize(nelem);
                m_ia.readArrayGeneric(buf.begin(), 1, nelem);
                arr.setAll(buf.begin(), nelem);
                break;
            }
            case hkTypeManager::SUB_TYPE_INT:
            {
                // version 3 added the stored integer size; 64-bit ones are read one by one
                if (m_tagfileVersion >= 3)
                {
                    const int intSize = readInt<int>();
                    if (intSize != 4)
                    {
                        for (int i = 0; i < nelem; i++)
                        {
                            arr[i] = (hkInt64)readInt64();
                        }
                        break;
                    }
                }
                _readArrayInts(arr, nelem);
                break;
            }
            case hkTypeManager::SUB_TYPE_REAL:
            {
                hkArray<float, hkContainerTempAllocator> buf(nelem);
                buf.setSize(nelem);
                m_ia.readArrayGeneric(buf.begin(), 4, nelem);
                arr.setAll(buf.begin(), nelem);
                break;
            }
            case hkTypeManager::SUB_TYPE_TUPLE:
            {
                if (!type->isVec())
                {
                    break;
                }
                // vec4 tuples store only the components the file says (3 for vectors written without w)
                hkStridedBasicArray sba;
                if (arr.asStridedBasicArray(sba) == HK_SUCCESS)
                {
                    const int numReals = type->getTupleSize();
                    int numRealsStored = numReals;
                    if (numReals == 4)
                    {
                        numRealsStored = readInt<int>();
                    }
                    hkArray<float, hkContainerTempAllocator> buf;
                    buf.setSize(numReals * nelem);
                    m_ia.readArrayGeneric(buf.begin(), 4, numRealsStored * nelem);
                    if (numReals != numRealsStored)
                    {
                        // expanded in place, last vector first (the stored vectors are 3 reals)
                        for (int i = nelem - 1; i >= 0; i--)
                        {
                            float* dst = buf.begin() + i * numReals;
                            const float* src = buf.begin() + i * numRealsStored;
                            dst[3] = 0.0f;
                            dst[2] = src[2];
                            dst[1] = src[1];
                            dst[0] = src[0];
                        }
                    }
                    arr.setAll(buf.begin(), nelem);
                }
                else
                {
                    const int numReals = type->getTupleSize();
                    int numRealsStored = numReals;
                    float r[16];
                    r[3] = 0.0f;
                    if (numReals == 4)
                    {
                        numRealsStored = readInt<int>();
                    }
                    for (int i = 0; i < nelem; i++)
                    {
                        m_ia.readArrayGeneric(r, 4, numRealsStored);
                        arr[i].setVec(r, numReals);
                    }
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_CSTRING:
            {
                for (int i = 0; i < nelem; i++)
                {
                    arr[i] = readString();
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_CLASS:
            {
                // a struct array is stored by member: the members present, then each one's column of values
                arr.setSize(nelem);
                hkDataClass klass = arr.getClass();
                const int numMembers = klass.getNumMembers();
                hkLocalArray<hkUint8> memberPresent(128);
                readBitfield(numMembers, memberPresent);
                hkLocalArray<hkDataClass_MemberInfo> minfos(numMembers);
                minfos.setSize(numMembers);
                for (int i = 0; i < numMembers; i++)
                {
                    new (&minfos[i]) hkDataClass_MemberInfo(); // hkArrayUtil::construct
                }
                klass.getAllMemberInfo(minfos);
                for (int i = 0; i < numMembers; i++)
                {
                    if (memberPresent[i])
                    {
                        hkDataArray a = arr.swizzleObjectMember(minfos[i].m_name);
                        hkTypeManager::Type* mtype = minfos[i].m_type;
                        if (mtype->isTuple() && !mtype->isVec())
                        {
                            for (int j = 0; j < nelem; j++)
                            {
                                hkDataArray t = a[j].asArray();
                                _readArrayItems(t, mtype->getTupleSize());
                            }
                        }
                        else
                        {
                            _readArrayItems(a, nelem);
                        }
                    }
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_POINTER:
            {
                for (int i = 0; i < nelem; i++)
                {
                    readObjectIntoValue(arr[i]);
                }
                break;
            }
            case hkTypeManager::SUB_TYPE_ARRAY:
            {
                arr.setSize(nelem);
                for (int i = 0; i < nelem; i++)
                {
                    const int asize = readInt<int>();
                    hkDataArray a = arr[i].asArray();
                    a.setSize(asize);
                    _readArrayItems(a, asize);
                }
                break;
            }
            default:
                break;
        }
    }

    // :525 (0x00F6D960)
    void Reader::readBinaryValue(hkDataObject_Value value, const hkDataClass_MemberInfo& minfo)
    {
        hkDataObject obj(value.m_impl);
        hkTypeManager::Type* type = minfo.m_type;
        if (type->isTuple() && !type->isVec())
        {
            hkDataArray a = value.asArray();
            _readArrayItems(a, type->getTupleSize());
        }
        else if (type->isArray())
        {
            hkTypeManager& typeManager = m_world->getTypeManager();
            hkDataClass_MemberInfo info = minfo;
            const int asize = readInt<int>();
            // an array of an unnamed class (a homogeneous array) names its class in the file
            hkTypeManager::Type* valueType = value.getType();
            if (valueType->getParent()->isClass() && valueType->getParent()->getTypeName() == nullptr)
            {
                const int classIndex = readInt<int>();
                info.m_type = typeManager.makeArray(typeManager.addClass(m_classes[classIndex]->getName()));
            }
            hkDataArray a(m_world->newArray(obj, value.m_handle, info));
            a.setSize(asize);
            _readArrayItems(a, asize);
        }
        else
        {
            switch (type->getSubType())
            {
                case hkTypeManager::SUB_TYPE_BYTE:
                {
                    hkUint8 b;
                    m_ia.readArrayGeneric(&b, 1, 1);
                    value = (int)b;
                    break;
                }
                case hkTypeManager::SUB_TYPE_INT:
                {
                    value = (hkInt64)readInt64();
                    break;
                }
                case hkTypeManager::SUB_TYPE_REAL:
                {
                    value = readFloat();
                    break;
                }
                case hkTypeManager::SUB_TYPE_TUPLE:
                {
                    if (type->isVec())
                    {
                        const int n = type->getTupleSize();
                        float r[16];
                        for (int i = 0; i < n; i++)
                        {
                            m_ia.readArrayGeneric(&r[i], 4, 1);
                        }
                        value.setVec(r, n);
                    }
                    break;
                }
                case hkTypeManager::SUB_TYPE_CLASS:
                {
                    // an embedded struct (version 2 on); before that it was stored as a reference
                    if (m_tagfileVersion >= 2)
                    {
                        const hkDataClassImpl* k = m_world->findClass(type->getTypeName());
                        value = readObjectTopLevel(hkBinaryTagfile::TAG_OBJECT, k).getImplementation();
                        break;
                    }
                    // fall through
                }
                case hkTypeManager::SUB_TYPE_POINTER:
                {
                    readObjectIntoValue(value);
                    break;
                }
                case hkTypeManager::SUB_TYPE_CSTRING:
                {
                    value = readString();
                    break;
                }
                default:
                    break;
            }
        }
    }

    // :639 (<hkDataArray_Value> 0x00F6CC20, <hkDataObject_Value> 0x00F6EA20)
    template<typename Value>
    void Reader::readObjectIntoValue(Value value)
    {
        if (m_tagfileVersion < 2)
        {
            value = readObjectTopLevel(hkBinaryTagfile::TAG_NONE, nullptr);
        }
        else
        {
            const int id = readInt<int>();
            if (id < m_rememberedObjects.getSize())
            {
                value = m_rememberedObjects[id];
            }
            else
            {
                hkPointerMap<int, ForwardReferences*>::Iterator it = m_forwardReferences.findOrInsertKey(id, nullptr);
                ForwardReferences* refs = m_forwardReferences.getValue(it);
                if (refs == nullptr)
                {
                    refs = new ForwardReferences();
                    m_forwardReferences.setValue(it, refs);
                }
                refs->remember(value);
            }
        }
    }

    // :673 (0x00F6DEC0)
    hkDataObject Reader::readObjectTopLevel(hkBinaryTagfile::TagType objectTag, const hkDataClassImpl* klassImpl)
    {
        if (objectTag == hkBinaryTagfile::TAG_NONE)
        {
            objectTag = readInt<hkBinaryTagfile::TagType>();
        }
        switch (objectTag)
        {
            case hkBinaryTagfile::TAG_OBJECT_BACKREF:
            {
                const int id = readInt<int>();
                return hkDataObject(m_rememberedObjects[id]);
            }
            case hkBinaryTagfile::TAG_OBJECT_NULL:
            {
                return hkDataObject();
            }
            default:
                break;
        }
        if (klassImpl == nullptr)
        {
            klassImpl = m_classes[readInt<int>()];
        }
        hkDataClass klass(const_cast<hkDataClassImpl*>(klassImpl));
        const int numMembers = klass.getNumMembers();
        hkDataObject obj(m_world->newObject(klass));
        if (objectTag != hkBinaryTagfile::TAG_OBJECT)
        {
            if (objectTag != hkBinaryTagfile::TAG_OBJECT_REMEMBER)
            {
                char buf[512];
                hkErrStream os(buf, sizeof(buf));
                os << "corrupt file";
                if (hkErrorFwd::messageError(0x484994D5, buf, "Tagfile\\Binary\\hkBinaryTagfileReader.cpp", 716) != 0)
                {
                    __debugbreak(); // HK_BREAKPOINT
                }
            }
            const int id = m_rememberedObjects.getSize();
            m_rememberedObjects.pushBack(obj.getImplementation());
            hkPointerMap<int, ForwardReferences*>::Iterator it = m_forwardReferences.findKey(id);
            if (m_forwardReferences.isValid(it))
            {
                ForwardReferences* refs = m_forwardReferences.getValue(it);
                refs->assign(obj);
                delete refs;
                m_forwardReferences.removeAtIterator(it);
            }
        }
        hkLocalArray<hkUint8> memberPresent(128);
        readBitfield(numMembers, memberPresent);
        hkLocalArray<hkDataClass_MemberInfo> minfos(numMembers);
        minfos.setSize(numMembers);
        for (int i = 0; i < numMembers; i++)
        {
            new (&minfos[i]) hkDataClass_MemberInfo(); // hkArrayUtil::construct
        }
        klass.getAllMemberInfo(minfos);
        hkLocalArray<hkDataObject::MemberHandle> handles(numMembers);
        handles.setSize(numMembers);
        obj.getAllMemberHandles(handles);
        for (int i = 0; i < numMembers; i++)
        {
            if (memberPresent[i])
            {
                readBinaryValue(hkDataObject_Value(obj.getImplementation(), handles[i]), minfos[i]);
            }
        }
        return obj;
    }

    // :749 (0x00F6C7C0). Original quirk kept: the member classes are looked up (findClass) and the result dropped.
    hkResult Reader::readClass()
    {
        hkDataClass::Cinfo cinfo;
        cinfo.name = readString();
        cinfo.version = readInt<int>();
        if (m_world->getType() == hkDataWorld::TYPE_NATIVE)
        {
            return HK_FAILURE; // classes cannot be added to a native world
        }
        const int parentIndex = readInt<int>();
        cinfo.parent = (parentIndex >= 0 && m_classes[parentIndex] != nullptr) ? m_classes[parentIndex]->getName() : nullptr;
        const int numMembers = readInt<int>();
        cinfo.members.setSize(numMembers);
        for (int i = 0; i < numMembers; i++)
        {
            hkDataClass::Cinfo::Member& m = cinfo.members[i];
            m.name = readString();
            const hkLegacyType::Type type = readInt<hkLegacyType::Type>();
            const int tupleCount = (type & hkLegacyType::TYPE_TUPLE) ? readInt<int>() : 0;
            const int basic = type & hkLegacyType::TYPE_MASK_BASIC_TYPES;
            const char* className = nullptr;
            if (basic == hkLegacyType::TYPE_STRUCT || basic == hkLegacyType::TYPE_OBJECT)
            {
                className = readString();
                if (className != nullptr)
                {
                    m_world->findClass(className);
                }
            }
            m.type = m_world->getTypeManager().getType(type, className, tupleCount);
        }
        m_classes.pushBack(m_world->newClass(cinfo));
        return HK_SUCCESS;
    }

    // :797 (0x00F6E530): the records up to TAG_FILE_END; the contents are the first remembered object (version 2
    // on) or the first object (before)
    hkDataObject Reader::readObjectTree()
    {
        hkUint32 magic0, magic1;
        m_ia.readArrayGeneric(&magic0, 4, 1);
        m_ia.readArrayGeneric(&magic1, 4, 1);
        if (magic0 != hkBinaryTagfile::BINARY_MAGIC_0 || magic1 != hkBinaryTagfile::BINARY_MAGIC_1)
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "This does not look like a binary tagfile (magic number mismatch)";
            hkErrorFwd::messageWarning(0x2AB6036F, buf, "Tagfile\\Binary\\hkBinaryTagfileReader.cpp", 806);
            return hkDataObject();
        }
        for (hkBinaryTagfile::TagType tag = readInt<hkBinaryTagfile::TagType>(); tag != hkBinaryTagfile::TAG_EOF;
             tag = readInt<hkBinaryTagfile::TagType>())
        {
            switch (tag)
            {
                case hkBinaryTagfile::TAG_FILE_INFO:
                {
                    m_tagfileVersion = readInt<int>();
                    switch (m_tagfileVersion)
                    {
                        case 0:
                            break;
                        case 1:
                        {
                            // version 1 indexes the static string list
                            if (readInt<int>() != HK_TAG_LIST_VERSION)
                            {
                                return hkDataObject();
                            }
                            m_prevStrings.setSizeUnchecked(0);
                            m_prevStrings.pushBack(const_cast<char*>(""));
                            m_prevStrings.pushBack(nullptr);
                            m_prevStrings.append(const_cast<char**>(HK_TAG_STRING_LIST), HK_NUM_TAG_STRINGS);
                            m_numPrevStringsStatic = HK_NUM_TAG_STRINGS + 2;
                            break;
                        }
                        case 2:
                        case 3:
                        case 4:
                        case 5:
                        {
                            m_rememberedObjects.pushBack(nullptr);
                            if (m_tagfileVersion > 3)
                            {
                                readString(); // the SDK version
                            }
                            if (m_tagfileVersion > 4)
                            {
                                // the predicates, skipped
                                hkUint16 maxPredicate, numPredicates, predicate;
                                m_ia.readArrayGeneric(&maxPredicate, 2, 1);
                                m_ia.readArrayGeneric(&numPredicates, 2, 1);
                                for (int i = numPredicates; i != 0; i--)
                                {
                                    m_ia.readArrayGeneric(&predicate, 2, 1);
                                }
                            }
                            break;
                        }
                        default:
                        {
                            char buf[512];
                            hkErrStream os(buf, sizeof(buf));
                            os << "Unrecognised tagfile version " << m_tagfileVersion;
                            hkErrorFwd::messageWarning(0x2AB6036F, buf, "Tagfile\\Binary\\hkBinaryTagfileReader.cpp",
                                                       872);
                            return hkDataObject();
                        }
                    }
                    break;
                }
                case hkBinaryTagfile::TAG_METADATA:
                {
                    if (readClass() == HK_FAILURE)
                    {
                        return hkDataObject();
                    }
                    break;
                }
                case hkBinaryTagfile::TAG_OBJECT:
                case hkBinaryTagfile::TAG_OBJECT_REMEMBER:
                case hkBinaryTagfile::TAG_OBJECT_NULL:
                {
                    hkDataObject obj = readObjectTopLevel(tag, nullptr);
                    if (m_tagfileVersion < 2)
                    {
                        return obj;
                    }
                    break;
                }
                case hkBinaryTagfile::TAG_FILE_END:
                {
                    return hkDataObject(m_rememberedObjects[1]);
                }
                default:
                    return hkDataObject();
            }
        }
        return hkDataObject();
    }
}

// :962 (0x00F6B140)
hkBinaryTagfileReader::hkBinaryTagfileReader()
{
}

// :966 (0x00F6B190)
hkDataObject hkBinaryTagfileReader::load(hkStreamReader* stream, hkDataWorld& cont)
{
    Reader reader(stream, &cont);
    return reader.readObjectTree();
}

// :972 (0x00F6B150)
hkResult hkBinaryTagfileReader::readHeader(hkStreamReader* stream, hkBinaryTagfile::Header& out)
{
    Reader reader(stream, nullptr);
    return reader.readHeader(out);
}
