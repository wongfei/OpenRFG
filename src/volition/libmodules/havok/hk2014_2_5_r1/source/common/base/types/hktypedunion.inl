// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hktypedunion.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the inline members of hkTypedUnion (Havok 2014.1 hkTypedUnion.inl,
// checked against the out-of-line instances rfg.exe keeps).

// 0x00FA7B70 (:150)
inline hkTypedUnion::hkTypedUnion()
{
    m_type = hkClassMember::TYPE_VOID;
}

// 0x00CF3640 (:154)
inline void hkTypedUnion::setObject(const void* orig, const hkClass* klass)
{
    m_type = hkClassMember::TYPE_POINTER;
    m_elem.m_variant.m_object = const_cast<void*>(orig);
    m_elem.m_variant.m_class = klass;
}

// 0x00CF3660 (:161)
inline void hkTypedUnion::setEnum(int val, const hkClassEnum* eklass)
{
    m_type = hkClassMember::TYPE_ENUM;
    m_elem.m_enumVariant.m_value = val;
    m_elem.m_enumVariant.m_enum = eklass;
}

// 0x00CF3680 (:168): a value that fits the 64-byte storage is copied in
inline hkResult hkTypedUnion::setSimple(const void* orig, const hkClassMember::Type type)
{
    const hkClassMember::TypeProperties& props = hkClassMember::getClassMemberTypeProperties(type);
    if (unsigned(props.m_size) <= sizeof(m_elem))
    {
        m_type = type;
        hkString::memCpy(&m_elem, orig, props.m_size);
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

inline hkClassMember::Type hkTypedUnion::getType() const
{
    return m_type;
}

// 0x00FA3DF0 (:188)
inline const hkTypedUnion::Storage& hkTypedUnion::getStorage() const
{
    return m_elem;
}
