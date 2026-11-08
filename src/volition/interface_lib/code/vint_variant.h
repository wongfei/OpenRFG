// Original: volition/interface_lib/code/vint_variant.h
// Implementation: volition/interface_lib/code/vint_variant.cpp
//
// The variant value of properties, data items and callback arguments: int, uint, uint64, float, string, bool, color,
// vector2f, callback, bitmap, font and sound (handles), with conversions both ways.
#pragma once

#include <stdint.h>

class color;
class vector2;
class vint_callback;
class vint_resource_bitmap;
class vint_resource_font;
class vint_resource_sound;

enum vint_variant_type
{
    VINT_PROP_TYPE_NONE = 0,
    VINT_PROP_TYPE_INT = 1,
    VINT_PROP_TYPE_UINT = 2,
    VINT_PROP_TYPE_FLOAT = 3,
    VINT_PROP_TYPE_STRING = 4,
    VINT_PROP_TYPE_BOOL = 5,
    VINT_PROP_TYPE_COLOR = 6,
    VINT_PROP_TYPE_VECTOR2F = 7,
    VINT_PROP_TYPE_CALLBACK = 8,
    VINT_PROP_TYPE_BITMAP = 9,
    VINT_PROP_TYPE_FONT = 10,
    VINT_PROP_TYPE_SOUND = 11,
    VINT_PROP_TYPE_UINT64 = 12,
    VINT_PROP_LAST_VALID_FOR_VARIANT = 12,
    VINT_PROP_TYPE_ENUM = 13,
    VINT_PROP_TYPE_VARIABLE = 14,
    NUM_VINT_PROP_TYPES = 15,
};

// size 0x8
struct vint_variant_vector2f
{
    void set(float _x, float _y); // 0x0045B110 (.h:73)
    bool operator==(const vint_variant_vector2f& _rh) const; // 0x0045B130 (.h:85)

    float x; // 0x00
    float y; // 0x04
};

// size 0xC
struct vint_variant_color
{
    void set(float _r, float _g, float _b); // 0x0045B180 (.h:96)
    bool operator==(const vint_variant_color& _rh) const; // 0x0045B1B0 (.h:103)

    float r; // 0x00
    float g; // 0x04
    float b; // 0x08
};

// size 0x20
class vint_variant
{
public:
    vint_variant(const vint_resource_sound* _b); // 0x00466C70 (:115)
    vint_variant(const vint_resource_font* _b); // 0x00466C40 (:102)
    vint_variant(const vint_resource_bitmap* _b); // 0x00466C10 (:89)
    vint_variant(const vint_variant_color& _v); // 0x00883120 (.h:177)
    vint_variant(const vint_variant_vector2f& _v); // 0x0045B340 (.h:170)
    vint_variant(const vector2& _v); // 0x0045B310 (.h:163)
    vint_variant(color _v); // 0x0045B2C0 (.h:156)
    vint_variant(bool _v) // 0x0045B2A0 (.h:149)
    {
        type = VINT_PROP_TYPE_BOOL;
        value_bool = _v;
        is_override_property = false;
    }
    vint_variant(float _v) // 0x0045B280 (.h:142)
    {
        type = VINT_PROP_TYPE_FLOAT;
        value_float = _v;
        is_override_property = false;
    }
    vint_variant(uint64_t);
    vint_variant(unsigned int _v); // 0x0045B260 (.h:128)
    vint_variant(int _v); // 0x0045B240 (.h:121)
    vint_variant(const char* _v); // 0x0045B220 (.h:114)
    vint_variant(const vint_variant& _v); // 0x00474490 (.h:111)
    vint_variant(); // 0x0045B210 (.h:110)
    ~vint_variant(); // 0x00476270 (:128)
    bool operator==(const vint_variant& _rh) const; // 0x00467120 (:879)
    vint_variant& operator=(const vint_variant& _v); // 0x00474210 (:663)
    vint_variant& operator=(vint_callback* _v); // 0x004670A0 (:747)
    vint_variant& operator=(const vint_variant_color& _v); // 0x0045B460 (.h:304)
    vint_variant& operator=(const vector2& _v); // 0x0045B430 (.h:285)
    vint_variant& operator=(color _color); // 0x0045B3B0 (.h:266)
    vint_variant& operator=(bool _bool); // 0x0045B390 (.h:247)
    vint_variant& operator=(double);
    vint_variant& operator=(float _float); // 0x0045B360 (.h:199)
    vint_variant& operator=(uint64_t _uint); // 0x00466F10 (:585)
    vint_variant& operator=(unsigned int _uint); // 0x0046F7D0 (:515)
    vint_variant& operator=(int _int); // 0x00466FC0 (:615)
    vint_variant& operator=(const char* _string); // 0x00474370 (:811)
    vint_variant& operator=(const vint_resource_sound* _b); // 0x0046F770 (:468)
    vint_variant& operator=(const vint_resource_font* _b); // 0x0046F710 (:442)
    vint_variant& operator=(const vint_resource_bitmap* _b); // 0x0046F6C0 (:416)
    vint_variant operator+(const vint_variant&) const;
    vint_variant& operator+=(const vint_variant& _r); // 0x00883150 (.h:332)
    vint_variant& operator*=(float);
    operator const char*() const;
    operator int() const; // 0x0045B4A0 (.h:419)
    operator unsigned int() const; // 0x0045B4F0 (.h:443)
    operator uint64_t() const; // 0x0045B560 (.h:467)
    operator float() const; // 0x0045B5C0 (.h:491)
    operator double() const; // 0x0045B660 (.h:512)
    operator bool() const; // 0x0045B700 (.h:545)
    operator vector2() const;
    operator vint_variant_vector2f() const;
    operator color() const;
    operator vint_variant_color() const;
    operator vint_resource_bitmap*() const;
    operator vint_resource_font*() const;
    operator vint_resource_sound*() const;
    operator vint_callback*() const;
    void set_color(float _r, float _g, float _b); // 0x0045B7F0 (.h:628)
    void set_vector2f(float _x, float _y); // 0x0045B830 (.h:642)
    static vint_variant_type get_type_from_name(const char* _name); // 0x00466CA0 (:133)
    static const char* get_name_from_type(vint_variant_type _type); // 0x00466CE0 (:150)
    void from_string(const char* _string, vint_variant_type _type); // 0x00473E90 (:157)
    void to_string(char* _string, unsigned int _length) const; // 0x00466CF0 (:273)
    void zero(); // 0x00474180 (:369)
    void clear(); // 0x00474360 (:741)
    vint_variant_type get_type() const; // 0x0045B860 (.h:663)
    void set_type(vint_variant_type _type); // 0x00467230 (:928)
    bool get_is_override_property() const; // 0x0045B870 (.h:668)
    void set_is_override_property(bool val); // 0x0045B880 (.h:673)
protected:
    void clean_up(); // 0x0046F910 (:779)

private:
    vint_variant_type type; // 0x00
    union
    {
        vint_variant_color value_color; // 0x08
        vint_variant_vector2f value_vector2f; // 0x08
        float value_float; // 0x08
        int value_int; // 0x08
        unsigned int value_uint; // 0x08
        uint64_t value_uint64; // 0x08
        const char* value_string; // 0x08
        bool value_bool; // 0x08
    };
    bool is_override_property; // 0x18
};
