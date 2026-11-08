// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h
//
// Seam only (Havok 2014.2.5): base value types that Volition headers hold or return by value. Havok internals stay a
// black box.
#pragma once

// Havok 2014.1 hkBaseTypes.h: the scalar types (Win32).
typedef float hkFloat32;
typedef double hkDouble64;
typedef float hkReal;
typedef signed char hkInt8;
typedef unsigned char hkUint8;
typedef unsigned char hkUchar;
typedef short hkInt16;
typedef unsigned short hkUint16;
typedef int hkInt32;
typedef unsigned int hkUint32;
typedef long long hkInt64;
typedef unsigned long long hkUint64;
typedef unsigned long hkUlong;
typedef long hkLong;
typedef unsigned int hkBool32;
typedef hkUint16 hkFloat16Storage;
typedef float hkTime;

// size 0x4
// Tag passed to the constructors that finish an object loaded in place (they leave the loaded members alone).
class hkFinishLoadedObjectFlag
{
public:
    hkFinishLoadedObjectFlag() : m_finishing(0) {} // 0x0080C190

    int m_finishing; // 0x00
};

// size 0x1
class hkBool
{
public:
    // inline in Havok (hkBaseTypes.h)
    hkBool() = default;
    hkBool(bool b) : m_bool(static_cast<char>(b)) {}
    operator bool() const { return m_bool != 0; }
    hkBool& operator=(bool e)
    {
        m_bool = static_cast<char>(e);
        return *this;
    }
    hkBool operator==(bool e) const { return (m_bool != 0) == e; }
    hkBool operator!=(bool e) const { return (m_bool != 0) != e; }

    char m_bool; // 0x00
};

// size sizeof(STORAGE)
// An enum stored in a fixed-size integer (hkpCollisionFilter::m_type).
template<typename ENUM, typename STORAGE>
class hkEnum
{
public:
    // Havok 2014.1 hkBaseTypes.h (added in HK6)
    hkEnum& operator=(ENUM e)
    {
        m_storage = static_cast<STORAGE>(e);
        return *this;
    }
    operator ENUM() const { return static_cast<ENUM>(m_storage); }

    STORAGE m_storage; // 0x00
};

enum hkResultEnum
{
    HK_SUCCESS = 0,
    HK_FAILURE = 1,
};

// size 0x4
// A function result (Havok 2014.1 hkBaseTypes.h).
struct hkResult
{
    hkResult() {}
    hkResult(hkResultEnum b) : m_enum(b) {}
    // Havok 2014.1 hkBaseTypes.h (added in HK6)
    bool operator==(hkResultEnum e) const { return m_enum == e; }
    bool operator!=(hkResultEnum e) const { return m_enum != e; }

    hkResultEnum m_enum; // 0x00
};

// size sizeof(STORAGE)
// A set of enum bit flags stored in a fixed-size integer (hkMemorySystem::FlagBits).
template<typename BITS, typename STORAGE>
class hkFlags
{
public:
    typedef STORAGE StorageType;
    hkFlags() : m_storage(0) {}
    hkFlags(STORAGE s) : m_storage(s) {}

    // Havok 2014.1 hkBaseTypes.h
    void clear() { m_storage = 0; }
    void clear(STORAGE mask) { m_storage &= ~mask; }
    void setAll(STORAGE s) { m_storage = s; }
    void orWith(STORAGE s) { m_storage |= s; }
    void xorWith(STORAGE s) { m_storage ^= s; }
    void andWith(STORAGE s) { m_storage &= s; }
    void setWithMask(STORAGE s, STORAGE mask) { m_storage = (m_storage & ~mask) | (s & mask); }
    STORAGE get() const { return m_storage; }
    STORAGE get(STORAGE mask) const { return m_storage & mask; }
    hkBool32 anyIsSet(STORAGE mask) const { return (m_storage & mask); }
    hkBool32 noneIsSet(STORAGE mask) const { return (m_storage & mask) == 0; }
    bool allAreSet(STORAGE mask) const { return (m_storage & mask) == mask; }
    bool operator==(const hkFlags& f) const { return f.m_storage == m_storage; }
    hkBool32 operator!=(const hkFlags& f) const { return f.m_storage ^ m_storage; }

    STORAGE m_storage; // 0x00
};

// size sizeof(T)
// A value padded for the SPU on PlayStation 3; plain storage on Win32 (hkStepInfo, M2).
template<typename T>
class hkPadSpu
{
public:
    hkPadSpu() = default;
    hkPadSpu(T t) : m_storage(t) {}
    T val() const { return m_storage; }
    operator T() const { return m_storage; }
    T operator->() const { return m_storage; } // pointer types only (Havok 2014.1 hkPadSpu)
    // added in HK6 (the GSK code counts its feature dimensions in place)
    hkPadSpu& operator+=(T t)
    {
        m_storage += t;
        return *this;
    }
    hkPadSpu& operator-=(T t)
    {
        m_storage -= t;
        return *this;
    }
    hkPadSpu& operator|=(T t)
    {
        m_storage |= t;
        return *this;
    }

private:
    T m_storage; // 0x00
};

// size 0x2
class hkHalf
{
public:
    typedef short Storage;

    hkHalf(const double&);
    hkHalf(const float&);
    hkHalf(); // 0x00F98C50
    hkHalf& operator=(const double&);
    hkHalf& operator=(const float& f); // 0x00F8C0F0
    hkHalf& operator=(const hkHalf& other); // 0x00D184A0
    void store(float* f) const; // 0x00B62A20 (.h:315)
    void store(double*) const;
    bool operator==(const hkHalf&) const;
    bool isZero() const;
    bool isOne() const;
    bool isMax() const;
    void setZero(); // 0x00DA02F0
    void setOne(); // 0x00DA0300
    void setMax();
    void setNegativeOne();
    operator float() const; // 0x00813F00 (.h:329)
    float getFloat32() const; // 0x0080C1A0 (.h:334)
    double getDouble() const;
    float getReal() const; // 0x00D184B0
    short getStorage() const; // 0x00E9A5B0
    void setStorage(short);

private:
    short m_value; // 0x00
};

// size 0x2
// A 16-bit IEEE half float (Havok 2014.1 hkBaseTypes.h; hkVector4f and hkSimdFloat32 load and store it).
class hkFloat16
{
public:

    static const class hkClass& staticClass(); // 0x00CEB6E0

public:

    inline hkFloat16& operator=(const hkFloat16& other)
    {
        m_value = other.m_value;
        return *this;
    }

    inline void setZero()
    {
        m_value = 0x0000;
    } 

    inline void setOne()
    {
        m_value = 0x3c00;
    }

    template <bool ROUND>
    inline void setReal(hkReal r);

    inline hkFloat32 getFloat32() const
    {
        enum {
            sign_pos = 15,
            half_sign = 1 << sign_pos,
            half_exponent = 0x00007C00,
            mantissa_width   = 23,
            mantissa_width_h = 10,
            mantissa_mask = (1 << mantissa_width_h) - 1,
            adjust = mantissa_width - mantissa_width_h,
            half_bias_offset = 0x0001C000
        };

        const hkUint32
            sign = m_value & half_sign,
            exponent = m_value & half_exponent,
            mantissa = m_value & mantissa_mask,
            exp_offset = exponent + half_bias_offset,
            bits = (sign << (sign_pos+1)) | ((exp_offset | mantissa) << adjust);

        typedef union { hkUint32 i; hkFloat32 f; } i2f;
        const i2f* bits_f = (const i2f*)&bits;
        return exponent ? bits_f->f : 0.0f;
    }

    inline hkReal getReal() const
    {
        return hkReal(getFloat32());
    }

    inline hkUint16 getBits() const
    {
        return m_value;
    }

    inline void setBits(hkUint16 b)
    {
        m_value = b;
    }

    inline hkFloat16() { }
    inline explicit hkFloat16(const hkFloat32& f);

private:
    hkUint16 m_value;
};


template <>
inline void hkFloat16::setReal<true>(hkReal r)
{
    const hkUint32 infinity_i  = 0x47FFE000; // Largest positive half
    const hkUint32 halfNormMin = 0x38800000; // Smallest positive normalized half

    union { float f; int i; } infinity; infinity.i = infinity_i;

    enum {
        mantissa_width   = 23,
        mantissa_width_h = 10,
        mantissa_mask = (1<<mantissa_width) - 1,
        sign_mask = 0x80000000,
        adjust = mantissa_width - mantissa_width_h,
        exponent_fix = hkUint8(-(127-15)) << mantissa_width
    };

    union { float f; int i; } bits; bits.f = hkFloat32(r);
    const hkUint32
        bits_i = (bits.f > infinity.f) ? infinity.i : bits.i,
        exp = bits_i + exponent_fix,
        round = (bits_i + 0xFFFu) + ((bits_i >> adjust) & 1u),
        result = (exp & ~mantissa_mask) | (round & mantissa_mask),
        h = (result >> adjust) | ((bits_i & sign_mask) >> 16),
        absbits = bits_i & 0x7FFFFFFF;
    m_value = (absbits >= halfNormMin) ? (hkUint16)h : 0;
}

template <bool ROUND>
inline void hkFloat16::setReal(hkReal r)
{
    const hkUint32 infinity_i  = 0x47FFE000; // Largest positive half
    const hkUint32 halfNormMin = 0x38800000; // Smallest positive normalized half

    union { float f; int i; } infinity; infinity.i = infinity_i;

    enum {
        mantissa_width   = 23,
        mantissa_width_h = 10,
        mantissa_mask = (1<<mantissa_width) - 1,
        sign_mask = 0x80000000,
        adjust = mantissa_width - mantissa_width_h,
        exponent_fix = hkUint8(-(127-15)) << mantissa_width
    };

    union { float f; int i; } bits; bits.f = hkFloat32(r);
    const hkUint32
        bits_i = (bits.f > infinity.f) ? infinity.i : bits.i,
        exp = bits_i + exponent_fix,
        result = (exp & ~mantissa_mask) | (bits_i & mantissa_mask),
        h = (result >> adjust) | ((bits_i & sign_mask) >> 16),
        absbits = bits_i & 0x7FFFFFFF;
    m_value = (absbits >= halfNormMin) ? (hkUint16)h : 0;
}

inline hkFloat16::hkFloat16(const hkFloat32& f)
{
    setReal<true>(f);
}

// Havok 2014.1 hkBaseDefs.h: pointer arithmetic in bytes (added in HK6).
template<typename TYPE>
inline TYPE* hkAddByteOffset(TYPE* base, long offset)
{
    return reinterpret_cast<TYPE*>(reinterpret_cast<char*>(base) + offset);
}

template<typename TYPE>
inline const TYPE* hkAddByteOffsetConst(const TYPE* base, long offset)
{
    return reinterpret_cast<const TYPE*>(reinterpret_cast<const char*>(base) + offset);
}

inline long hkGetByteOffset(const void* base, const void* pntr)
{
    return static_cast<long>(reinterpret_cast<const char*>(pntr) - reinterpret_cast<const char*>(base));
}

inline int hkGetByteOffsetInt(const void* base, const void* pntr)
{
    return static_cast<int>(hkGetByteOffset(base, pntr));
}
