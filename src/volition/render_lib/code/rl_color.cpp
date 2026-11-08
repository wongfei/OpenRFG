// Original: volition/render_lib/code/rl_color.cpp
//
// Partial: the named colours, the constructors used so far and the sRGB to linear conversion. The other conversions
// and operators are not implemented yet.

#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

#include <math.h>

const rl_color rl_color::BLACK(0xFF000000u); // 0x01514868
const rl_color rl_color::WHITE(0xFFFFFFFFu); // 0x0151486C
const rl_color rl_color::RED(0xFFFF0000u); // 0x01514870
const rl_color rl_color::GREEN(0xFF00FF00u); // 0x01514874
const rl_color rl_color::BLUE(0xFF0000FFu); // 0x01514878
const rl_color rl_color::YELLOW(0xFFFFFF00u); // 0x0151487C
const rl_color rl_color::CYAN(0xFF00FFFFu); // 0x01514880
const rl_color rl_color::MAGENTA(0xFFFF00FFu); // 0x01514884
const rl_color rl_color::ORANGE(0xFFFF8000u); // 0x01514888
const rl_color rl_color::GREY(0xFF808080u); // 0x0151488C
const rl_color rl_color::MAROON(0xFF800000u); // 0x01514890
const rl_color rl_color::DARKGREEN(0xFF008000u); // 0x01514894
const rl_color rl_color::NAVY(0xFF000080u); // 0x01514898
const rl_color rl_color::OLIVE(0xFF808000u); // 0x0151489C
const rl_color rl_color::TEAL(0xFF008080u); // 0x015148A0
const rl_color rl_color::BROWN(0xFFA52A2Au); // 0x015148A4
const rl_color rl_color::VIOLET(0xFFEE82EEu); // 0x015148A8
const rl_color rl_color::TURQUOISE(0xFF40E0D0u); // 0x015148AC
const rl_color rl_color::SILVER(0xFFC0C0C0u); // 0x015148B0
const rl_color rl_color::PINK(0xFFFFC0CBu); // 0x015148B4

// 0x004B6E40 (.h:284)
rl_color_float::rl_color_float()
{
}

// 0x004B6E50 (.h:289)
rl_color_float::rl_color_float(float input_red, float input_green, float input_blue, float input_alpha)
    : red(input_red)
    , green(input_green)
    , blue(input_blue)
    , alpha(input_alpha)
{
}

// 0x004B6EA0 (.h:307): 0..255 to 0..1.
rl_color_float::rl_color_float(const rl_color& col)
    : red((float)col.red / 255.0f)
    , green((float)col.green / 255.0f)
    , blue((float)col.blue / 255.0f)
    , alpha((float)col.alpha / 255.0f)
{
}

// 0x004B6E80 (.h:298)
rl_color_float::rl_color_float(const rl_color_float& col)
    : red(col.red)
    , green(col.green)
    , blue(col.blue)
    , alpha(col.alpha)
{
}

// 0x004B6F10 (.h:316)
rl_color_float::rl_color_float(const vector& col, float a)
    : red(col.x)
    , green(col.y)
    , blue(col.z)
    , alpha(a)
{
}

// 0x004B6F40 (.h:324)
rl_color_float::rl_color_float(const vector4& col)
    : red(col.x)
    , green(col.y)
    , blue(col.z)
    , alpha(col.w)
{
}

// 0x006E1180 (.h:412)
float rl_color_float::sRGB_to_linear(float value)
{
    // The constants are floats widened to double (as in the original).
    if (value > 0.04045f)
    {
        const float base = (float)(((double)value + (double)0.055f) / (double)1.055f);
        return (float)pow((double)base, (double)2.4f);
    }
    return (float)((double)value / (double)12.92f);
}

// 0x006E11F0 (.h:430)
rl_color_float rl_color_float::from_sRGB_to_linear() const
{
    return rl_color_float(sRGB_to_linear(red), sRGB_to_linear(green), sRGB_to_linear(blue), alpha);
}

// 0x007104B0 (.h:160)
rl_color::rl_color(const rl_color& col)
    : uint_color(col.uint_color)
{
}

// 0x00919C30 (.h:175)
rl_color::rl_color(unsigned int input_color)
    : uint_color(input_color)
{
}

// 0x004B6D70 (.h:155)
// The color is left uninitialized.
rl_color::rl_color()
{
}

// 0x004B6D80 (.h:166)
rl_color::rl_color(unsigned char input_red, unsigned char input_green, unsigned char input_blue, unsigned char input_alpha)
{
    red = input_red;
    green = input_green;
    blue = input_blue;
    alpha = input_alpha;
}

// 0x004B6E20 (.h:268)
const rl_color& rl_color::set(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha)
{
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
    return *this;
}

// 0x004B71B0 (.h:445)
void rl_color_float::set(float input_red, float input_green, float input_blue, float input_alpha)
{
    red = input_red;
    green = input_green;
    blue = input_blue;
    alpha = input_alpha;
}
