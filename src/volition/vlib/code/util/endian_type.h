// Original: volition/vlib/code/util/endian_type.h
// Implementation: volition/vlib/code/util/endian_type.cpp; the templates are inline in this header
//
// Pointer types for data loaded as one block (packfile directories, zone/mesh/terrain/chunk files). Only the two
// pointer templates exist in this build; the PDB has no endian-swapped value types (et_uint32 and the like), so the
// formats are stored in native (little-endian) order.
//
// et_ptr_offset<T, NULLABLE> is a self-relative pointer: the target is (char*)this + m_offset (decompiled). With
// NULLABLE = 1, an offset of 0 or -1 means nullptr. et_ptr_offset_to_pointer_func / pointer_to_et_ptr_offset_func
// do the conversion for code that only has the base address (one specialization per NULLABLE value).
//
// et_sized_native_pointer<T> is a native pointer padded to 8 bytes, so a structure has the same layout in 32-bit and
// 64-bit builds; the loader patches the pointer after loading. __compile_time_assert is a static assert in the source
// (its condition is not recorded).
#pragma once

#include <stdint.h>

template<int NULLABLE>
void et_ptr_offset_to_pointer_func(int base_address, int applied_offset, void*& as_pointer); // <1> 0x004B5190 (.h:291), <0> 0x004B51C0 (.h:297)
template<int NULLABLE>
void pointer_to_et_ptr_offset_func(int base_address, int& applied_offset, const void* as_pointer); // <1> 0x00508D00 (.h:310), <0> 0x00508D20 (.h:316)

// size 0x4
template<typename T, int NULLABLE>
struct et_ptr_offset
{
    et_ptr_offset(const et_ptr_offset<T, NULLABLE>& value); // <char,1> 0x005C5CC0 (.h:472)
    et_ptr_offset(T* const value); // <spline_vehicles,0> 0x00B608A0 (.h:447)
    et_ptr_offset(); // <char,0> 0x00595620 (.h:331)
    T* to_pointer() const; // <char,0> 0x00595850 (.h:338)
    void from_pointer(T* const value); // <char,0> 0x005C0510 (.h:347), m_offset = value - this
    T* operator->() const; // <rl_renderable_instance,0> 0x005D6780 (.h:360)
    const T& operator*() const;
    T& operator*();
    operator T&() const;
    operator T*() const; // <char,0> 0x005B3500 (.h:383)
    template<typename U>
    operator U*() const; // <unsigned char,0> to rl_morph_target_vbuffer* 0x004D9150 (.h:389)
    T& operator[](int i) const; // <int,0> 0x00C948C0 (.h:414)
    T& operator[](int i); // <unsigned char,0> 0x00501F60 (.h:408)
    T& operator[](unsigned int i) const; // <rl_render_block,0> 0x00501F80 (.h:402)
    T& operator[](unsigned int i); // <unsigned int,0> 0x00800D40 (.h:396)
    bool operator==(const unsigned int&);
    bool operator!=(const unsigned int&);
    et_ptr_offset<T, NULLABLE>& operator=(const et_ptr_offset<T, NULLABLE>& value); // <char,1> 0x005C5CF0 (.h:483)
    et_ptr_offset<T, NULLABLE>& operator=(T* const value); // <terrain_occluder,0> 0x00935590 (.h:453)
    int get_offset() const; // <char,0> 0x00595630 (.h:460)
    void set_offset(int offset); // <char,0> 0x00595640 (.h:465)
private:
    operator int() const;
    operator unsigned int() const;

protected:
    int m_offset; // 0x00
};

// size 0x8
template<typename T>
struct et_sized_native_pointer
{
    typedef char __compile_time_assert[1];

    // size 0x8
    union storage_type
    {
        // size 0x8
        struct pointer_type
        {
            T* m_pointer; // 0x00
            void* m_pointer_padding; // 0x04
        };

        pointer_type m_pointer_type; // 0x00
        uint64_t m_pointer_type_padding; // 0x00
    };

    T* to_pointer() const; // <char> 0x005D3110 (.h:590)
    void from_pointer(T* const value); // <char> 0x0091E180 (.h:601)
    T* operator->() const; // <const vehicle_component_data::hinge> 0x00B6F180 (.h:612)
    const T& operator*() const;
    T& operator*();
    operator T&() const;
    operator T*() const; // <char> 0x005D6730 (.h:635)
    template<typename U>
    operator U*() const; // <rl_material_map_data> to unsigned char* 0x00CA2840 (.h:641)
    T& operator[](int i) const; // <et_sized_native_pointer<rfg_base_destroyable_shape>> 0x00924090 (.h:666)
    T& operator[](int i); // <vfx_effect_template::emitter> 0x005D67B0 (.h:660)
    T& operator[](unsigned int i) const; // <vehicle_mesh_light> 0x00B6F150 (.h:654)
    T& operator[](unsigned int i); // <unsigned int> 0x00B6F100 (.h:648)
    bool operator==(const unsigned int& other); // <rl_mesh_data> 0x00CA1190 (.h:688)
    bool operator!=(const unsigned int& other); // <general_object_base> 0x00A45F70 (.h:694)
    et_sized_native_pointer<T>& operator=(const et_sized_native_pointer<T>& value); // <const hkpMoppCode> 0x00B6F0F0 (.h:706)
    et_sized_native_pointer<T>& operator=(T* const value); // <char> 0x00924060 (.h:700)

protected:
    storage_type m_storage; // 0x00
};

#include "volition/vlib/code/util/endian_type.inl"
