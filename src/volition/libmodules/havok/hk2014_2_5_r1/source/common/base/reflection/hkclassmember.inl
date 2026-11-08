// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the inline accessors of hkClassMember, from rfg.exe (the
// out-of-line instances it keeps).

// 0x00CF2930 (:10)
inline const char* hkClassMember::getName() const
{
    return m_name;
}

// 0x00CF3600 (:15)
inline hkClassMember::Type hkClassMember::getType() const
{
    return m_type;
}

// 0x00FC29A0 (:20)
inline void hkClassMember::setType(hkClassMember::Type type)
{
    m_type = type;
}

// 0x00CF3610 (:25)
inline hkClassMember::Type hkClassMember::getSubType() const
{
    return m_subtype;
}

// 0x00FC29B0 (:30)
inline void hkClassMember::setSubType(hkClassMember::Type subtype)
{
    m_subtype = subtype;
}

// 0x00F586E0 (:35)
inline int hkClassMember::getOffset() const
{
    return m_offset;
}

// 0x00F660C0 (:40)
inline void hkClassMember::setOffset(int offset)
{
    m_offset = static_cast<unsigned short>(offset);
}

// 0x00D0DFE0 (:45)
inline const hkClassMember::Flags& hkClassMember::getFlags() const
{
    return m_flags;
}

// 0x00FC2960 (:50)
inline hkClassMember::Flags& hkClassMember::getFlags()
{
    return m_flags;
}

// 0x00F660D0 (:55)
inline hkBool hkClassMember::hasClass() const
{
    return m_class != nullptr;
}

// 0x00F77D40 (:60)
inline hkBool hkClassMember::hasEnumClass() const
{
    return m_enum != nullptr;
}
