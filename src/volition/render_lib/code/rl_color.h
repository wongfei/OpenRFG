// Original: volition/render_lib/code/rl_color.h
// Implementation: volition/render_lib/code/rl_color.cpp
//
// rl_color (8-bit RGBA) and rl_color_float (linear float RGBA) with conversions and sRGB helpers.
#pragma once


class rl_color;
class vector;
class vector4;

// size 0x10
class rl_color_float
{
public:
    rl_color_float(const vector4& col); // 0x004B6F40 (.h:324)
    rl_color_float(const vector& col, float a); // 0x004B6F10 (.h:316)
    rl_color_float(const rl_color& col); // 0x004B6EA0 (.h:307)
    rl_color_float(const rl_color_float& col); // 0x004B6E80 (.h:298)
    rl_color_float(float input_red, float input_green, float input_blue, float input_alpha); // 0x004B6E50 (.h:289)
    rl_color_float(); // 0x004B6E40 (.h:284)
    rl_color_float& operator*=(float scalar); // 0x004B6FD0 (.h:343)
    rl_color_float operator*(float scalar) const; // 0x004B70C0 (.h:359)
    rl_color_float operator*(const rl_color_float& col) const; // 0x004B7040 (.h:353)
    rl_color_float operator+(const rl_color_float& col) const; // 0x004B7130 (.h:365)
    bool operator==(const rl_color_float&) const;
    bool operator!=(const rl_color_float&) const;
    rl_color_float clamp(float, float) const;
    rl_color_float from_linear_to_sRGB() const;
    rl_color_float from_sRGB_to_linear() const; // 0x006E11F0 (.h:430)
    vector as_vector3() const;
    vector4 as_vector4() const;
    void set(float input_red, float input_green, float input_blue, float input_alpha); // 0x004B71B0 (.h:445)
    static float linear_to_sRGB(float);
    static float sRGB_to_linear(float value); // 0x006E1180 (.h:412)

    float red; // 0x00
    float green; // 0x04
    float blue; // 0x08
    float alpha; // 0x0C
};

// size 0x10
class rl_color_float_specialized : public rl_color_float
{
public:
    rl_color_float_specialized(const rl_color_float& v); // 0x00C9EF10 (vfx_effect_expressions.cpp:40)
    rl_color_float_specialized(float input_red, float input_green, float input_blue, float input_alpha); // 0x00C9EEE0 (vfx_effect_expressions.cpp:37)
    rl_color_float_specialized(); // 0x00C9EED0 (vfx_effect_expressions.cpp:34)
    rl_color_float_specialized operator+(const rl_color_float_specialized& _v) const; // 0x00C9EF30 (vfx_effect_expressions.cpp:46)
    rl_color_float_specialized operator-(const rl_color_float_specialized& _v) const; // 0x00C9EFB0 (vfx_effect_expressions.cpp:51)
    rl_color_float_specialized operator/(const rl_color_float_specialized&) const;

};

// size 0x4
class rl_color
{
public:
    // 0x004B6DA0 (.h:181)
    rl_color(const rl_color_float& col)
    {
        red = (unsigned char)(int)(col.red * 255.0f + 0.5f);
        green = (unsigned char)(int)(col.green * 255.0f + 0.5f);
        blue = (unsigned char)(int)(col.blue * 255.0f + 0.5f);
        alpha = (unsigned char)(int)(col.alpha * 255.0f + 0.5f);
    }
    rl_color(unsigned int input_color); // 0x00919C30 (.h:175)
    rl_color(unsigned char input_red, unsigned char input_green, unsigned char input_blue, unsigned char input_alpha); // 0x004B6D80 (.h:166)
    rl_color(const rl_color& col); // 0x007104B0 (.h:160)
    rl_color(); // 0x004B6D70 (.h:155)
    const rl_color& operator*=(float);
    rl_color operator*(float) const;
    const rl_color& operator+=(const rl_color&);
    rl_color operator+(const rl_color&);
    bool operator==(const rl_color&) const;
    bool operator!=(const rl_color&) const;
    const rl_color& set(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha); // 0x004B6E20 (.h:268)

    union
    {
        unsigned int uint_color; // 0x00
        struct
        {
            unsigned char blue; // 0x00
            unsigned char green; // 0x01
            unsigned char red; // 0x02
            unsigned char alpha; // 0x03
        };
    };
    static const rl_color BLACK; // 0x01514868
    static const rl_color WHITE; // 0x0151486C
    static const rl_color RED; // 0x01514870
    static const rl_color GREEN; // 0x01514874
    static const rl_color BLUE; // 0x01514878
    static const rl_color YELLOW; // 0x0151487C
    static const rl_color CYAN; // 0x01514880
    static const rl_color MAGENTA; // 0x01514884
    static const rl_color ORANGE; // 0x01514888
    static const rl_color GREY; // 0x0151488C
    static const rl_color MAROON; // 0x01514890
    static const rl_color DARKGREEN; // 0x01514894
    static const rl_color NAVY; // 0x01514898
    static const rl_color OLIVE; // 0x0151489C
    static const rl_color TEAL; // 0x015148A0
    static const rl_color BROWN; // 0x015148A4
    static const rl_color VIOLET; // 0x015148A8
    static const rl_color TURQUOISE; // 0x015148AC
    static const rl_color SILVER; // 0x015148B0
    static const rl_color PINK; // 0x015148B4
};
