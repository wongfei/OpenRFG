// Original: volition/vlib/code/util/endian_type.h (inline members; included at the end of endian_type.h)
//
// Partial: et_ptr_offset get_offset, set_offset, to_pointer, from_pointer, the copy constructor and assignments,
// operator[], operator T* and operator-> only.
#pragma once

// <char,0> 0x00595630 (.h:460)
template<typename T, int NULLABLE>
int et_ptr_offset<T, NULLABLE>::get_offset() const
{
    return m_offset;
}

// <char,0> 0x00595640 (.h:465)
template<typename T, int NULLABLE>
void et_ptr_offset<T, NULLABLE>::set_offset(int offset)
{
    m_offset = offset;
}

// <char,0> 0x00595850 (.h:338)
template<typename T, int NULLABLE>
T* et_ptr_offset<T, NULLABLE>::to_pointer() const
{
    // With NULLABLE = 1 an offset of 0 or -1 means nullptr.
    if (NULLABLE != 0 && (m_offset == 0 || m_offset == -1))
    {
        return nullptr;
    }
    return (T*)((char*)this + m_offset);
}

// <char,0> 0x005C0510 (.h:347)
template<typename T, int NULLABLE>
void et_ptr_offset<T, NULLABLE>::from_pointer(T* const value)
{
    // With NULLABLE = 1 nullptr is stored as -1.
    if (NULLABLE != 0 && value == nullptr)
    {
        m_offset = -1;
        return;
    }
    m_offset = (int)((char*)value - (char*)this);
}

// <char,1> 0x005C5CC0 (.h:472)
// A copy points at the same target (the offset rebased on the new address).
template<typename T, int NULLABLE>
et_ptr_offset<T, NULLABLE>::et_ptr_offset(const et_ptr_offset<T, NULLABLE>& value)
{
    from_pointer(value.to_pointer());
}

// <char,1> 0x005C5CF0 (.h:483)
template<typename T, int NULLABLE>
et_ptr_offset<T, NULLABLE>& et_ptr_offset<T, NULLABLE>::operator=(const et_ptr_offset<T, NULLABLE>& value)
{
    from_pointer(value.to_pointer());
    return *this;
}

// <terrain_occluder,0> 0x00935590 (.h:453)
template<typename T, int NULLABLE>
et_ptr_offset<T, NULLABLE>& et_ptr_offset<T, NULLABLE>::operator=(T* const value)
{
    from_pointer(value);
    return *this;
}

// <char,0> 0x00595620 (.h:331)
// An unset offset is -1.
template<typename T, int NULLABLE>
et_ptr_offset<T, NULLABLE>::et_ptr_offset()
    : m_offset(-1)
{
}

// <char,0> 0x005B3500 (.h:383)
template<typename T, int NULLABLE>
et_ptr_offset<T, NULLABLE>::operator T*() const
{
    return to_pointer();
}

// <rl_renderable_instance,0> 0x005D6780 (.h:360)
template<typename T, int NULLABLE>
T* et_ptr_offset<T, NULLABLE>::operator->() const
{
    return to_pointer();
}

// <unsigned char,0> 0x00501F60 (.h:408)
template<typename T, int NULLABLE>
T& et_ptr_offset<T, NULLABLE>::operator[](int i)
{
    return to_pointer()[i];
}

// <int,0> 0x00C948C0 (.h:414)
template<typename T, int NULLABLE>
T& et_ptr_offset<T, NULLABLE>::operator[](int i) const
{
    return to_pointer()[i];
}

// <unsigned int,0> 0x00800D40 (.h:396)
template<typename T, int NULLABLE>
T& et_ptr_offset<T, NULLABLE>::operator[](unsigned int i)
{
    return to_pointer()[i];
}

// <rl_render_block,0> 0x00501F80 (.h:402)
template<typename T, int NULLABLE>
T& et_ptr_offset<T, NULLABLE>::operator[](unsigned int i) const
{
    return to_pointer()[i];
}

// <char> 0x0091E180 (.h:601)
template<typename T>
void et_sized_native_pointer<T>::from_pointer(T* const value)
{
    m_storage.m_pointer_type.m_pointer = value;
}

// <char> 0x005D3110 (.h:590)
template<typename T>
T* et_sized_native_pointer<T>::to_pointer() const
{
    return m_storage.m_pointer_type.m_pointer;
}

// <char> 0x005D6730 (.h:635)
template<typename T>
et_sized_native_pointer<T>::operator T*() const
{
    return to_pointer();
}

// <rl_material_map_data> to unsigned char* 0x00CA2840 (.h:641)
// The stored pointer as another pointer type.
template<typename T>
template<typename U>
et_sized_native_pointer<T>::operator U*() const
{
    return (U*)to_pointer();
}

// <char> 0x00924060 (.h:700)
template<typename T>
et_sized_native_pointer<T>& et_sized_native_pointer<T>::operator=(T* const value)
{
    from_pointer(value);
    return *this;
}

// <const vehicle_component_data::hinge> 0x00B6F180 (.h:612)
template<typename T>
T* et_sized_native_pointer<T>::operator->() const
{
    return to_pointer();
}

// <et_sized_native_pointer<rfg_base_destroyable_shape>> 0x00924090 (.h:666)
template<typename T>
T& et_sized_native_pointer<T>::operator[](int i) const
{
    return to_pointer()[i];
}

// <vfx_effect_template::emitter> 0x005D67B0 (.h:660)
template<typename T>
T& et_sized_native_pointer<T>::operator[](int i)
{
    return to_pointer()[i];
}

// <vehicle_mesh_light> 0x00B6F150 (.h:654)
template<typename T>
T& et_sized_native_pointer<T>::operator[](unsigned int i) const
{
    return to_pointer()[i];
}

// <unsigned int> 0x00B6F100 (.h:648)
template<typename T>
T& et_sized_native_pointer<T>::operator[](unsigned int i)
{
    return to_pointer()[i];
}
