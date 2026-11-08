// Original: volition/interface_lib/code/vint_variant.cpp
//
// The variant (UI.md UI2): a typed value with conversions. Assignment keeps the type once it is set (a variant of
// another type ignores the value), except from VINT_PROP_TYPE_NONE, which takes the new type. Callback, bitmap, font
// and sound variants hold the resource's handle and a reference on it. The header-inline members (".h" lines) are
// out-of-line functions in rfg.exe and are defined here. Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/vint_variant.h"

#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/interface_lib/code/resources/vint_resource_sound.h"
#include "volition/interface_lib/code/vint_callback.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"

#include <stdio.h>

// ---- file-static globals
static const char* Type_names[15] = {"none", "int", "uint", "float", "string", "bool", "color", "vector2f", "callback", "bitmap", "font", "sound", "uint64", "enum", "variable"}; // 0x014F5B28

// ---- header-inline members (vint_variant.h)

// 0x0045B110 (.h:73)
void vint_variant_vector2f::set(float _x, float _y)
{
    x = _x;
    y = _y;
}

// 0x0045B130 (.h:85)
bool vint_variant_vector2f::operator==(const vint_variant_vector2f& _rh) const
{
    return x == _rh.x && y == _rh.y;
}

// 0x0045B180 (.h:96)
void vint_variant_color::set(float _r, float _g, float _b)
{
    r = _r;
    g = _g;
    b = _b;
}

// 0x0045B1B0 (.h:103)
bool vint_variant_color::operator==(const vint_variant_color& _rh) const
{
    return r == _rh.r && g == _rh.g && b == _rh.b;
}

// 0x0045B210 (.h:110)
vint_variant::vint_variant()
{
    type = VINT_PROP_TYPE_NONE;
    is_override_property = false;
}

// 0x00474490 (.h:111)
vint_variant::vint_variant(const vint_variant& _v)
{
    // is_override_property is left as it is (rfg.exe: only the type is cleared before the assignment).
    type = VINT_PROP_TYPE_NONE;
    *this = _v;
}

// 0x0045B220 (.h:114)
vint_variant::vint_variant(const char* _v)
{
    type = VINT_PROP_TYPE_STRING;
    value_string = _v;
    is_override_property = false;
}

// 0x0045B240 (.h:121)
vint_variant::vint_variant(int _v)
{
    type = VINT_PROP_TYPE_INT;
    value_int = _v;
    is_override_property = false;
}

// 0x0045B260 (.h:128)
vint_variant::vint_variant(unsigned int _v)
{
    type = VINT_PROP_TYPE_UINT;
    value_uint = _v;
    is_override_property = false;
}

// 0x0045B2C0 (.h:156)
vint_variant::vint_variant(color _v)
{
    // Original bug kept: the channels are not divided by 255 (operator=(color) does divide), so a colour constructed
    // from a color holds 0..255 components.
    type = VINT_PROP_TYPE_COLOR;
    value_color.r = (float)_v.red;
    value_color.g = (float)_v.green;
    value_color.b = (float)_v.blue;
    is_override_property = false;
}

// 0x0045B310 (.h:163)
vint_variant::vint_variant(const vector2& _v)
{
    type = VINT_PROP_TYPE_VECTOR2F;
    value_vector2f.x = _v.x;
    value_vector2f.y = _v.y;
    is_override_property = false;
}

// 0x0045B340 (.h:170)
vint_variant::vint_variant(const vint_variant_vector2f& _v)
{
    type = VINT_PROP_TYPE_VECTOR2F;
    value_vector2f.x = _v.x;
    value_vector2f.y = _v.y;
    is_override_property = false;
}

// 0x00883120 (.h:177)
vint_variant::vint_variant(const vint_variant_color& _v)
{
    type = VINT_PROP_TYPE_COLOR;
    value_color = _v;
    is_override_property = false;
}

// 0x0045B360 (.h:199)
vint_variant& vint_variant::operator=(float _float)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_FLOAT;
    }
    else if (type != VINT_PROP_TYPE_FLOAT)
    {
        return *this;
    }
    value_float = _float;
    return *this;
}

// 0x0045B390 (.h:247)
vint_variant& vint_variant::operator=(bool _bool)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_BOOL;
    }
    else if (type != VINT_PROP_TYPE_BOOL)
    {
        return *this;
    }
    value_bool = _bool;
    return *this;
}

// 0x0045B3B0 (.h:266)
vint_variant& vint_variant::operator=(color _color)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_COLOR;
    }
    else if (type != VINT_PROP_TYPE_COLOR)
    {
        return *this;
    }
    value_color.r = (float)_color.red / 255.0f;
    value_color.g = (float)_color.green / 255.0f;
    value_color.b = (float)_color.blue / 255.0f;
    return *this;
}

// 0x0045B430 (.h:285)
vint_variant& vint_variant::operator=(const vector2& _v)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_VECTOR2F;
    }
    else if (type != VINT_PROP_TYPE_VECTOR2F)
    {
        return *this;
    }
    value_vector2f.x = _v.x;
    value_vector2f.y = _v.y;
    return *this;
}

// 0x0045B460 (.h:304)
vint_variant& vint_variant::operator=(const vint_variant_color& _v)
{
    if (type == VINT_PROP_TYPE_NONE || type == VINT_PROP_TYPE_COLOR)
    {
        type = VINT_PROP_TYPE_COLOR;
        value_color = _v;
    }
    return *this;
}

// 0x00883150 (.h:332)
vint_variant& vint_variant::operator+=(const vint_variant& _r)
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
        value_int += _r.value_int;
        break;
    case VINT_PROP_TYPE_FLOAT:
        value_float += _r.value_float;
        break;
    case VINT_PROP_TYPE_COLOR:
        value_color.r += _r.value_color.r;
        value_color.g += _r.value_color.g;
        value_color.b += _r.value_color.b;
        // Clamped at the top only.
        if (value_color.r - 1.0f >= 0.0f)
        {
            value_color.r = 1.0f;
        }
        if (value_color.g - 1.0f >= 0.0f)
        {
            value_color.g = 1.0f;
        }
        if (value_color.b - 1.0f >= 0.0f)
        {
            value_color.b = 1.0f;
        }
        break;
    case VINT_PROP_TYPE_VECTOR2F:
        value_vector2f.x += _r.value_vector2f.x;
        value_vector2f.y += _r.value_vector2f.y;
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 += _r.value_uint64;
        break;
    default:
        break;
    }
    return *this;
}

// 0x0045B490 (.h:400)
vint_variant::operator const char*() const
{
    if (type == VINT_PROP_TYPE_STRING)
    {
        return value_string;
    }
    return nullptr;
}

// 0x0045B4A0 (.h:419)
vint_variant::operator int() const
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_UINT64:
        return value_int;
    case VINT_PROP_TYPE_FLOAT:
        return (int)value_float;
    case VINT_PROP_TYPE_BOOL:
        // true converts to -1 (rfg.exe: neg of the bool)
        return value_bool ? -1 : 0;
    default:
        return 0;
    }
}

// 0x0045B4F0 (.h:443)
vint_variant::operator unsigned int() const
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_BITMAP:
    case VINT_PROP_TYPE_FONT:
    case VINT_PROP_TYPE_SOUND:
    case VINT_PROP_TYPE_UINT64:
        return value_uint;
    case VINT_PROP_TYPE_FLOAT:
        // x87 truncation to 64 bits, low half kept
        return (unsigned int)(int64_t)value_float;
    default:
        return 0;
    }
}

// 0x0045B560 (.h:467)
vint_variant::operator uint64_t() const
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
        return (uint64_t)(int64_t)value_int;
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_BITMAP:
    case VINT_PROP_TYPE_FONT:
    case VINT_PROP_TYPE_SOUND:
        return value_uint;
    case VINT_PROP_TYPE_FLOAT:
        // _ftol2: truncation to a signed 64-bit integer
        return (uint64_t)(int64_t)value_float;
    case VINT_PROP_TYPE_UINT64:
        return value_uint64;
    default:
        return 0;
    }
}

// 0x0045B5C0 (.h:491)
vint_variant::operator float() const
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
        return (float)value_int;
    case VINT_PROP_TYPE_UINT:
        return (float)value_uint;
    case VINT_PROP_TYPE_FLOAT:
        return value_float;
    case VINT_PROP_TYPE_UINT64:
        return (float)value_uint64;
    default:
        return 0.0f;
    }
}

// 0x0045B660 (.h:512)
vint_variant::operator double() const
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
        return (double)value_int;
    case VINT_PROP_TYPE_UINT:
        return (double)value_uint;
    case VINT_PROP_TYPE_FLOAT:
        return (double)value_float;
    case VINT_PROP_TYPE_UINT64:
        return (double)value_uint64;
    default:
        return 0.0;
    }
}

// 0x0045B700 (.h:545)
vint_variant::operator bool() const
{
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
        return value_int != 0;
    case VINT_PROP_TYPE_FLOAT:
        return value_float != 0.0f;
    case VINT_PROP_TYPE_BOOL:
        return value_bool;
    case VINT_PROP_TYPE_UINT64:
        return value_uint64 != 0;
    default:
        return false;
    }
}

// 0x0045B770 (.h:569)
vint_variant::operator vector2() const
{
    vector2 result(0.0f, 0.0f);
    if (type == VINT_PROP_TYPE_VECTOR2F)
    {
        result.x = value_vector2f.x;
        result.y = value_vector2f.y;
    }
    return result;
}

// 0x008832F0 (.h:585)
vint_variant::operator vint_variant_vector2f() const
{
    vint_variant_vector2f result;
    if (type == VINT_PROP_TYPE_VECTOR2F)
    {
        result = value_vector2f;
        return result;
    }
    result.y = 0.0f;
    result.x = 0.0f;
    return result;
}

// 0x00467F60 (.h:596)
vint_variant::operator color() const
{
    color result(0, 0, 0, 0);
    if (type == VINT_PROP_TYPE_COLOR)
    {
        result.red = (unsigned char)(int)(value_color.r * 255.0f);
        result.green = (unsigned char)(int)(value_color.g * 255.0f);
        result.blue = (unsigned char)(int)(value_color.b * 255.0f);
        result.alpha = 0xFF;
    }
    return result;
}

// 0x0045B7A0 (.h:612)
vint_variant::operator vint_variant_color() const
{
    vint_variant_color result;
    if (type == VINT_PROP_TYPE_COLOR)
    {
        result = value_color;
        return result;
    }
    result.r = 0.0f;
    result.g = 0.0f;
    result.b = 0.0f;
    return result;
}

// 0x0045B7F0 (.h:628)
void vint_variant::set_color(float _r, float _g, float _b)
{
    if (type == VINT_PROP_TYPE_NONE || type == VINT_PROP_TYPE_COLOR)
    {
        type = VINT_PROP_TYPE_COLOR;
        value_color.r = _r;
        value_color.g = _g;
        value_color.b = _b;
    }
}

// 0x0045B830 (.h:642)
void vint_variant::set_vector2f(float _x, float _y)
{
    if (type == VINT_PROP_TYPE_NONE || type == VINT_PROP_TYPE_VECTOR2F)
    {
        type = VINT_PROP_TYPE_VECTOR2F;
        value_vector2f.x = _x;
        value_vector2f.y = _y;
    }
}

// 0x0045B860 (.h:663)
vint_variant_type vint_variant::get_type() const
{
    return type;
}

// 0x0045B870 (.h:668)
bool vint_variant::get_is_override_property() const
{
    return is_override_property;
}

// 0x0045B880 (.h:673)
void vint_variant::set_is_override_property(bool val)
{
    is_override_property = val;
}

// ---- vint_variant.cpp

// 0x00466C10 (:89)
vint_variant::vint_variant(const vint_resource_bitmap* _b)
{
    type = VINT_PROP_TYPE_BITMAP;
    is_override_property = false;
    if (_b == nullptr)
    {
        value_uint = 0;
        return;
    }
    value_uint = _b->get_handle();
    _b->add_ref();
}

// 0x00466C40 (:102)
vint_variant::vint_variant(const vint_resource_font* _b)
{
    type = VINT_PROP_TYPE_FONT;
    is_override_property = false;
    if (_b == nullptr)
    {
        value_uint = 0;
        return;
    }
    value_uint = _b->get_handle();
    _b->add_ref();
}

// 0x00466C70 (:115)
vint_variant::vint_variant(const vint_resource_sound* _b)
{
    type = VINT_PROP_TYPE_SOUND;
    is_override_property = false;
    if (_b == nullptr)
    {
        value_uint = 0;
        return;
    }
    value_uint = _b->get_handle();
    _b->add_ref();
}

// 0x00476270 (:128)
vint_variant::~vint_variant()
{
    clean_up();
    type = VINT_PROP_TYPE_NONE;
}

// 0x00466CA0 (:133)
vint_variant_type vint_variant::get_type_from_name(const char* _name)
{
    if (_name == nullptr)
    {
        return VINT_PROP_TYPE_NONE;
    }
    for (int i = VINT_PROP_TYPE_NONE; i < NUM_VINT_PROP_TYPES; ++i)
    {
        if (v_stricmp(_name, Type_names[i]) == 0)
        {
            return (vint_variant_type)i;
        }
    }
    return VINT_PROP_TYPE_NONE;
}

// 0x00466CE0 (:150)
const char* vint_variant::get_name_from_type(vint_variant_type _type)
{
    return Type_names[_type];
}

// 0x00473E90 (:157)
void vint_variant::from_string(const char* _string, vint_variant_type _type)
{
    if (type != VINT_PROP_TYPE_NONE && _type != type)
    {
        return;
    }
    switch (_type)
    {
    case VINT_PROP_TYPE_INT:
        value_int = 0;
        if (_string != nullptr)
        {
            sscanf_s(_string, "%i", &value_int);
        }
        break;
    case VINT_PROP_TYPE_UINT:
        value_uint = 0;
        if (_string != nullptr)
        {
            sscanf_s(_string, "%u", &value_uint);
        }
        break;
    case VINT_PROP_TYPE_FLOAT:
        value_float = 0.0f;
        if (_string != nullptr)
        {
            sscanf_s(_string, "%f", &value_float);
        }
        break;
    case VINT_PROP_TYPE_STRING:
        value_string = nullptr;
        if (_string != nullptr)
        {
            value_string = _string;
        }
        break;
    case VINT_PROP_TYPE_BOOL:
        value_bool = _string != nullptr && v_stricmp(_string, "true") == 0;
        break;
    case VINT_PROP_TYPE_COLOR:
        value_color.r = 0.0f;
        value_color.g = 0.0f;
        value_color.b = 0.0f;
        if (_string != nullptr
            && sscanf_s(_string, "%f,%f,%f", &value_color.r, &value_color.g, &value_color.b) < 3)
        {
            // not three numbers: a colour table name
            const vint_variant_color* c = vint_color_table_find(_string, -1);
            if (c != nullptr)
            {
                value_color = *c;
            }
        }
        break;
    case VINT_PROP_TYPE_VECTOR2F:
        value_vector2f.x = 0.0f;
        value_vector2f.y = 0.0f;
        if (_string != nullptr)
        {
            sscanf_s(_string, "%f,%f", &value_vector2f.x, &value_vector2f.y);
        }
        break;
    case VINT_PROP_TYPE_CALLBACK:
        // callbacks cannot be parsed: the type is not set
        return;
    case VINT_PROP_TYPE_BITMAP:
    {
        value_uint = 0;
        vint_resource_bitmap* b = _string != nullptr ? vint_resource_bitmap::load(_string) : nullptr;
        if (b != nullptr)
        {
            value_uint = b->get_handle();
        }
        break;
    }
    case VINT_PROP_TYPE_FONT:
    {
        value_uint = 0;
        vint_resource_font* f = _string != nullptr ? vint_resource_font::load(_string) : nullptr;
        if (f != nullptr)
        {
            value_uint = f->get_handle();
        }
        break;
    }
    case VINT_PROP_TYPE_SOUND:
    {
        value_uint = 0;
        vint_resource_sound* s = _string != nullptr ? vint_resource_sound::load(_string) : nullptr;
        if (s != nullptr)
        {
            value_uint = s->get_handle();
        }
        break;
    }
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = 0;
        if (_string != nullptr)
        {
            uint64_t v = 0;
            sscanf_s(_string, "%llu", &v);
            value_uint64 = v;
        }
        break;
    default:
        return;
    }
    type = _type;
}

// 0x00466CF0 (:273)
void vint_variant::to_string(char* _string, unsigned int _length) const
{
    if (_string == nullptr)
    {
        return;
    }
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
        _snprintf_safe(_string, _length, "%i", value_int);
        return;
    case VINT_PROP_TYPE_UINT:
        _snprintf_safe(_string, _length, "%u", value_uint);
        return;
    case VINT_PROP_TYPE_FLOAT:
        _snprintf_safe(_string, _length, "%g", (double)value_float);
        return;
    case VINT_PROP_TYPE_STRING:
        _snprintf_safe(_string, _length, "%s", value_string);
        return;
    case VINT_PROP_TYPE_BOOL:
        _snprintf_safe(_string, _length, "%s", value_bool ? "true" : "false");
        return;
    case VINT_PROP_TYPE_COLOR:
        _snprintf_safe(_string, _length, "%g,%g,%g", (double)value_color.r, (double)value_color.g, (double)value_color.b);
        return;
    case VINT_PROP_TYPE_VECTOR2F:
        _snprintf_safe(_string, _length, "%g,%g", (double)value_vector2f.x, (double)value_vector2f.y);
        return;
    case VINT_PROP_TYPE_BITMAP:
    case VINT_PROP_TYPE_FONT:
    case VINT_PROP_TYPE_SOUND:
        keen::debug::nativePrintArgs("vint_variant::to_string() - This operation not supported on this build");
        break;
    case VINT_PROP_TYPE_UINT64:
        _snprintf_safe(_string, _length, "%llu", value_uint64);
        return;
    default:
        break;
    }
    *_string = '\0';
}

// 0x00474180 (:369)
void vint_variant::zero()
{
    clean_up();
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_STRING:
    case VINT_PROP_TYPE_CALLBACK:
    case VINT_PROP_TYPE_BITMAP:
    case VINT_PROP_TYPE_FONT:
    case VINT_PROP_TYPE_SOUND:
        value_uint = 0;
        break;
    case VINT_PROP_TYPE_FLOAT:
        value_float = 0.0f;
        break;
    case VINT_PROP_TYPE_BOOL:
        value_bool = false;
        break;
    case VINT_PROP_TYPE_COLOR:
        value_color.b = 0.0f;
        value_color.g = 0.0f;
        value_color.r = 0.0f;
        break;
    case VINT_PROP_TYPE_VECTOR2F:
        value_vector2f.y = 0.0f;
        value_vector2f.x = 0.0f;
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = 0;
        break;
    default:
        break;
    }
}

// 0x0046F6C0 (:416)
vint_variant& vint_variant::operator=(const vint_resource_bitmap* _b)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_BITMAP;
    }
    else
    {
        if (type != VINT_PROP_TYPE_BITMAP)
        {
            return *this;
        }
        vint_resource_bitmap* old = vint_resource_bitmap::find(value_uint);
        if (old != nullptr)
        {
            old->release();
        }
    }
    if (_b != nullptr)
    {
        value_uint = _b->get_handle();
        _b->add_ref();
        return *this;
    }
    value_uint = 0;
    return *this;
}

// 0x0046F710 (:442)
vint_variant& vint_variant::operator=(const vint_resource_font* _b)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_FONT;
    }
    else
    {
        if (type != VINT_PROP_TYPE_FONT)
        {
            return *this;
        }
        vint_resource_font* old = vint_resource_font::find(value_uint);
        if (old != nullptr)
        {
            old->release();
        }
    }
    if (_b != nullptr)
    {
        value_uint = _b->get_handle();
        _b->add_ref();
        return *this;
    }
    value_uint = 0;
    return *this;
}

// 0x0046F770 (:468)
vint_variant& vint_variant::operator=(const vint_resource_sound* _b)
{
    if (type == VINT_PROP_TYPE_NONE)
    {
        type = VINT_PROP_TYPE_SOUND;
    }
    else
    {
        if (type != VINT_PROP_TYPE_SOUND)
        {
            return *this;
        }
        vint_resource_sound* old = vint_resource_sound::find(value_uint);
        if (old != nullptr)
        {
            old->release();
        }
    }
    if (_b != nullptr)
    {
        value_uint = _b->get_handle();
        _b->add_ref();
        return *this;
    }
    value_uint = 0;
    return *this;
}

// 0x00466E90 (:494)
vint_variant::operator vint_resource_bitmap*() const
{
    if (type != VINT_PROP_TYPE_BITMAP)
    {
        return nullptr;
    }
    return vint_resource_bitmap::find(value_uint);
}

// 0x00466EB0 (:501)
vint_variant::operator vint_resource_font*() const
{
    if (type != VINT_PROP_TYPE_FONT)
    {
        return nullptr;
    }
    return vint_resource_font::find(value_uint);
}

// 0x00466EE0 (:508)
vint_variant::operator vint_resource_sound*() const
{
    if (type != VINT_PROP_TYPE_SOUND)
    {
        return nullptr;
    }
    return vint_resource_sound::find(value_uint);
}

// 0x0046F7D0 (:515)
vint_variant& vint_variant::operator=(unsigned int _uint)
{
    switch (type)
    {
    case VINT_PROP_TYPE_NONE:
    case VINT_PROP_TYPE_UINT:
        value_uint = _uint;
        type = VINT_PROP_TYPE_UINT;
        return *this;
    case VINT_PROP_TYPE_INT:
        value_uint = _uint;
        return *this;
    case VINT_PROP_TYPE_FLOAT:
        value_float = (float)_uint;
        return *this;
    case VINT_PROP_TYPE_BITMAP:
        if (vint_resource_bitmap* b = vint_resource_bitmap::find(_uint))
        {
            vint_resource_bitmap::release(value_uint);
            b->add_ref();
            value_uint = _uint;
            return *this;
        }
        break;
    case VINT_PROP_TYPE_FONT:
        if (vint_resource_font* f = vint_resource_font::find(_uint))
        {
            vint_resource_font::release(value_uint);
            f->add_ref();
            value_uint = _uint;
            return *this;
        }
        break;
    case VINT_PROP_TYPE_SOUND:
        if (vint_resource_sound* s = vint_resource_sound::find(_uint))
        {
            vint_resource_sound::release(value_uint);
            s->add_ref();
            value_uint = _uint;
            return *this;
        }
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = _uint;
        return *this;
    default:
        return *this;
    }
    // Original bug kept: an unknown resource handle zeroes the handle without releasing the old reference.
    value_uint = 0;
    return *this;
}

// 0x00466F10 (:585)
vint_variant& vint_variant::operator=(uint64_t _uint)
{
    switch (type)
    {
    case VINT_PROP_TYPE_NONE:
    case VINT_PROP_TYPE_UINT:
        type = VINT_PROP_TYPE_UINT;
        value_uint = (unsigned int)_uint;
        break;
    case VINT_PROP_TYPE_INT:
        value_int = (int)_uint;
        break;
    case VINT_PROP_TYPE_FLOAT:
        value_float = (float)_uint;
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = _uint;
        break;
    default:
        break;
    }
    return *this;
}

// 0x00466FC0 (:615)
vint_variant& vint_variant::operator=(int _int)
{
    switch (type)
    {
    case VINT_PROP_TYPE_NONE:
    case VINT_PROP_TYPE_INT:
        type = VINT_PROP_TYPE_INT;
        value_int = _int;
        break;
    case VINT_PROP_TYPE_UINT:
        value_int = _int;
        break;
    case VINT_PROP_TYPE_FLOAT:
        value_float = (float)_int;
        break;
    case VINT_PROP_TYPE_CALLBACK:
        if (value_uint != (unsigned int)_int)
        {
            if (vint_callback* old = vint_callback::find_by_handle(value_uint))
            {
                old->release();
            }
            if (vint_callback* cb = vint_callback::find_by_handle(_int))
            {
                value_int = _int;
                cb->add_ref();
            }
            else
            {
                value_uint = 0;
            }
        }
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = (uint64_t)(int64_t)_int;
        break;
    default:
        break;
    }
    return *this;
}

// 0x00474210 (:663)
vint_variant& vint_variant::operator=(const vint_variant& _v)
{
    if (type != VINT_PROP_TYPE_NONE && _v.type != type)
    {
        return *this;
    }
    // Self-assignment releases the reference before taking it again (the original has no check either).
    clean_up();
    type = _v.type;
    switch (type)
    {
    case VINT_PROP_TYPE_NONE:
        break;
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_STRING:
    case VINT_PROP_TYPE_ENUM:
        value_uint = _v.value_uint;
        break;
    case VINT_PROP_TYPE_FLOAT:
        value_float = _v.value_float;
        break;
    case VINT_PROP_TYPE_BOOL:
        value_bool = _v.value_bool;
        break;
    case VINT_PROP_TYPE_COLOR:
        value_color = _v.value_color;
        break;
    case VINT_PROP_TYPE_VECTOR2F:
        value_vector2f = _v.value_vector2f;
        break;
    case VINT_PROP_TYPE_CALLBACK:
        if (vint_callback* cb = vint_callback::find_by_handle(_v.value_uint))
        {
            cb->add_ref();
        }
        value_uint = _v.value_uint;
        break;
    case VINT_PROP_TYPE_BITMAP:
        value_uint = _v.value_uint;
        vint_resource_bitmap::add_ref(value_uint);
        break;
    case VINT_PROP_TYPE_FONT:
        value_uint = _v.value_uint;
        vint_resource_font::add_ref(value_uint);
        break;
    case VINT_PROP_TYPE_SOUND:
        value_uint = _v.value_uint;
        vint_resource_sound::add_ref(value_uint);
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = _v.value_uint64;
        break;
    default:
        keen::debug::nativePrintArgs("Don't know how to handle this type");
        break;
    }
    return *this;
}

// 0x00474360 (:741)
void vint_variant::clear()
{
    clean_up();
    type = VINT_PROP_TYPE_NONE;
}

// 0x004670A0 (:747)
vint_variant& vint_variant::operator=(vint_callback* _v)
{
    if (type == VINT_PROP_TYPE_NONE || type == VINT_PROP_TYPE_CALLBACK)
    {
        // Original bug kept: the old callback's reference is not released.
        type = VINT_PROP_TYPE_CALLBACK;
        if (_v != nullptr)
        {
            _v->add_ref();
            value_uint = _v->get_handle();
            return *this;
        }
        value_uint = 0;
    }
    return *this;
}

// 0x004670E0 (:769)
vint_variant::operator vint_callback*() const
{
    if (type == VINT_PROP_TYPE_CALLBACK && value_uint != 0)
    {
        return vint_callback::find_by_handle(value_uint);
    }
    return nullptr;
}

// 0x0046F910 (:779)
void vint_variant::clean_up()
{
    switch (type)
    {
    case VINT_PROP_TYPE_CALLBACK:
        if (vint_callback* cb = vint_callback::find_by_handle(value_uint))
        {
            // vint_callback::release: the last reference destroys the callback. value_uint keeps the dead handle.
            cb->release();
        }
        break;
    case VINT_PROP_TYPE_BITMAP:
        if (vint_resource_bitmap* b = vint_resource_bitmap::find(value_uint))
        {
            b->release();
        }
        value_uint = 0;
        break;
    case VINT_PROP_TYPE_FONT:
        vint_resource_font::release(value_uint);
        value_uint = 0;
        break;
    case VINT_PROP_TYPE_SOUND:
        vint_resource_sound::release(value_uint);
        value_uint = 0;
        break;
    default:
        break;
    }
}

// 0x00474370 (:811)
vint_variant& vint_variant::operator=(const char* _string)
{
    switch (type)
    {
    case VINT_PROP_TYPE_NONE:
        value_string = _string;
        type = VINT_PROP_TYPE_STRING;
        return *this;
    case VINT_PROP_TYPE_STRING:
        value_string = _string;
        return *this;
    case VINT_PROP_TYPE_COLOR:
        if (const vint_variant_color* c = vint_color_table_find(_string, -1))
        {
            value_color = *c;
        }
        return *this;
    case VINT_PROP_TYPE_BITMAP:
    {
        if (vint_resource_bitmap* old = vint_resource_bitmap::find(value_uint))
        {
            old->release();
        }
        vint_resource_bitmap* b = vint_resource_bitmap::load(_string);
        if (b != nullptr)
        {
            value_uint = b->get_handle();
            return *this;
        }
        break;
    }
    case VINT_PROP_TYPE_FONT:
    {
        vint_resource_font::release(value_uint);
        vint_resource_font* f = vint_resource_font::load(_string);
        if (f != nullptr)
        {
            value_uint = f->get_handle();
            return *this;
        }
        break;
    }
    case VINT_PROP_TYPE_SOUND:
    {
        vint_resource_sound::release(value_uint);
        vint_resource_sound* s = vint_resource_sound::load(_string);
        if (s != nullptr)
        {
            value_uint = s->get_handle();
            return *this;
        }
        break;
    }
    default:
        return *this;
    }
    value_uint = 0;
    return *this;
}

// 0x00467120 (:879)
bool vint_variant::operator==(const vint_variant& _rh) const
{
    if (type != _rh.type || type == VINT_PROP_TYPE_NONE)
    {
        return false;
    }
    switch (type)
    {
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_CALLBACK:
    case VINT_PROP_TYPE_BITMAP:
    case VINT_PROP_TYPE_FONT:
    case VINT_PROP_TYPE_SOUND:
        return value_int == _rh.value_int;
    case VINT_PROP_TYPE_FLOAT:
        return value_float == _rh.value_float;
    case VINT_PROP_TYPE_STRING:
        if (value_string == _rh.value_string)
        {
            return true;
        }
        if (value_string == nullptr || _rh.value_string == nullptr)
        {
            return false;
        }
        return v_stricmp(value_string, _rh.value_string) == 0;
    case VINT_PROP_TYPE_BOOL:
        return value_bool == _rh.value_bool;
    case VINT_PROP_TYPE_COLOR:
        return _rh.value_color == value_color;
    case VINT_PROP_TYPE_VECTOR2F:
        return _rh.value_vector2f == value_vector2f;
    case VINT_PROP_TYPE_UINT64:
        return value_uint64 == _rh.value_uint64;
    default:
        return false;
    }
}

// 0x00467230 (:928)
void vint_variant::set_type(vint_variant_type _type)
{
    if (type != VINT_PROP_TYPE_NONE)
    {
        return;
    }
    type = _type;
    switch (_type)
    {
    case VINT_PROP_TYPE_FLOAT:
        value_float = 0.0f;
        break;
    case VINT_PROP_TYPE_BOOL:
        value_bool = false;
        break;
    case VINT_PROP_TYPE_COLOR:
        value_color.r = 0.0f;
        value_color.g = 0.0f;
        value_color.b = 0.0f;
        break;
    case VINT_PROP_TYPE_VECTOR2F:
        value_vector2f.x = 0.0f;
        value_vector2f.y = 0.0f;
        break;
    case VINT_PROP_TYPE_UINT64:
        value_uint64 = 0;
        break;
    case VINT_PROP_TYPE_INT:
    case VINT_PROP_TYPE_UINT:
    case VINT_PROP_TYPE_STRING:
    case VINT_PROP_TYPE_CALLBACK:
    case VINT_PROP_TYPE_BITMAP:
    case VINT_PROP_TYPE_FONT:
    case VINT_PROP_TYPE_SOUND:
        value_uint = 0;
        break;
    default:
        break;
    }
}
