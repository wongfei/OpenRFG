// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.inl
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the inline accessors of hkClassEnum and its items, from rfg.exe
// (the out-of-line instances it keeps).

// 0x00CF2940 (:19)
inline const char* hkClassEnum::getName() const
{
    return m_name;
}

// 0x00D0F190 (:24)
inline int hkClassEnum::getNumItems() const
{
    return m_numItems;
}

// 0x00D0F1A0 (:29)
inline const hkClassEnum::Item& hkClassEnum::getItem(int i) const
{
    return m_items[i];
}

// 0x00FA3DE0 (:35)
inline const hkClassEnum::Flags& hkClassEnum::getFlags() const
{
    return m_flags;
}

inline hkClassEnum::Flags& hkClassEnum::getFlags()
{
    return m_flags;
}

// 0x00D0F170 (:39)
inline const char* hkClassEnum::Item::getName() const
{
    return m_name;
}

// 0x00D0F180 (:42)
inline int hkClassEnum::Item::getValue() const
{
    return m_value;
}
