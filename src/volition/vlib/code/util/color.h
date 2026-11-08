// Original: volition/vlib/code/util/color.h
//
// 8-bit RGBA colour with named constants. Defined inline (no color.cpp in the PDB).
#pragma once

// size 0x4
class color
{
public:
    color(const color& other_color); // 0x00467F30 (.h:50)
    color(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a); // 0x00467F10 (.h:46)
    color(); // 0x0045B0E0 (.h:43)
    void set(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a); // 0x0045B0F0 (.h:56)
    bool operator==(const color& other_color) const; // 0x0058AF60 (.h:71)
    unsigned int toKeen() const;

    unsigned char red; // 0x00
    unsigned char green; // 0x01
    unsigned char blue; // 0x02
    unsigned char alpha; // 0x03
    static const color BLACK; // 0x0163EA00
    static const color WHITE; // 0x0163EA04
    static const color RED; // 0x0163EA08
    static const color GREEN; // 0x0163EA0C
    static const color BLUE; // 0x0163EA10
    static const color YELLOW; // 0x0163EA14
    static const color CYAN; // 0x0163EA18
    static const color MAGENTA; // 0x0163EA1C
    static const color ORANGE; // 0x0163EA20
    static const color GREY; // 0x0163EA24
    static const color MAROON; // 0x0163EA28
    static const color DARKGREEN; // 0x0163EA2C
    static const color NAVY; // 0x0163EA30
    static const color OLIVE; // 0x0163EA34
    static const color TEAL; // 0x0163EA38
    static const color BROWN; // 0x0163EA3C
    static const color VIOLET; // 0x0163EA40
    static const color TURQUOISE; // 0x0163EA44
    static const color SILVER; // 0x0163EA48
    static const color PINK; // 0x0163EA4C
};

// ---- inline members

// 0x0045B0E0 (.h:43)
// Uninitialized.
inline color::color()
{
}

// 0x00467F10 (.h:46)
inline color::color(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a)
    : red(_r)
    , green(_g)
    , blue(_b)
    , alpha(_a)
{
}

// 0x00467F30 (.h:50)
inline color::color(const color& other_color)
    : red(other_color.red)
    , green(other_color.green)
    , blue(other_color.blue)
    , alpha(other_color.alpha)
{
}

// 0x0045B0F0 (.h:56)
inline void color::set(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a)
{
    red = _r;
    green = _g;
    blue = _b;
    alpha = _a;
}

// 0x0058AF60 (.h:71)
inline bool color::operator==(const color& other_color) const
{
    return red == other_color.red && blue == other_color.blue && green == other_color.green && alpha == other_color.alpha;
}
