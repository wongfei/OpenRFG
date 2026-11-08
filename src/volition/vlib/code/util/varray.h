// Original: volition/vlib/code/util/varray.h
// Implementation: volition/vlib/code/util/varray.cpp (comparison helpers); the templates are inline in this header
//
// Volition's arrays (decompiled behaviour):
// - base_array<T>: a view over external storage (elt, capacity array_size, count num). It never allocates:
//   grow() always fails, and add() past the capacity reports an error and returns -1. remove(i) shifts the tail
//   down; remove_unsorted(i) moves the last element into the hole.
// - farray<T, N>: base_array with inline storage T data[N] (elt = data, array_size = N).
// - circ_array<T, N>: ring of the last N values; operator[](0) is the newest, [i] goes back in time.
// Method addresses are for one instantiation, named in <>. These are the most instantiated templates in the binary.
#pragma once

// size 0x10
template<typename T>
class base_array
{
public:
    virtual ~base_array(); // vtbl+0x00 <int> 0x00707680 (.h:94)
    void init(T* data_ptr, int size, int num_existing_elements); // <int> 0x00707690 (.h:98)
    void clear();
    bool grow(int n); // <int> 0x00707D30 (.h:120), always false
    void copy(const base_array<T>& other_array); // <int> 0x0077B640 (.h:127)
    bool resize(int);
    int add(const T*, unsigned int);
    int add(const T& t); // <int> 0x0070AC60 (.h:156), index or -1 when full
    T* add_in_place(int&);
    T* add_in_place();
    T* add_many_in_place(int);
    bool is_member(const T&) const;
    int add_unique(const T& t); // <int> 0x008BC020 (.h:237)
    int contains(const T& t) const; // <int> 0x008A2650 (.h:248)
    int find(const T& t, bool from_end) const; // <int> 0x008A3690 (.h:261)
    bool get_random_element(T& returned_value) const; // <int> 0x00987550 (.h:283)
    void remove_matching(const T& t); // <int> 0x008BC090 (.h:306)
    void remove(int i); // <int> 0x008A2680 (.h:320)
    bool remove_unsorted(int i); // <int> 0x00B6F780 (.h:335)
    void remove_all(); // <int> 0x00762500 (.h:347)
    void set(int, const T&);
    int size() const; // <int> 0x007076D0 (.h:360)
    int capacity() const; // <int> 0x007076E0 (.h:366)
    bool full() const; // <int> 0x008A26B0 (.h:372)
    int bytes_allocated() const;
    int bytes_used() const;
    T* elements();
    const T& operator[](unsigned int) const;
    T& operator[](unsigned int);
    const T& operator[](int i) const; // <int> 0x00766380 (.h:409)
    T& operator[](int i); // <int> 0x007076F0 (.h:399)
private:
    void quick_sort(T*, int, int, bool (*)(const T&, const T&));
public:
    void sort(bool (*)(const T&, const T&));
    base_array(); // <int> 0x007054F0

    // 0x00 vfptr
protected:
    T* elt; // 0x04
    int array_size; // 0x08
    int num; // 0x0C
};

// size 0x10 + N * sizeof(T) (natural alignment)
template<typename T, int N>
class farray : public base_array<T>
{
public:
    farray(const farray<T, N>&);
    farray(); // <int,16> 0x00778DC0 (.h:512)
    virtual ~farray(); // vtbl+0x00 <int,16> 0x00762A40 (.h:523)
    farray<T, N> operator=(const farray<T, N>&);
    const T& operator[](unsigned int) const;
    T& operator[](unsigned int i); // <int,16> 0x008A26D0 (.h:558)
    const T& operator[](int i) const; // <int,16> 0x008A26C0 (.h:548)
    T& operator[](int i); // <int,16> 0x00762A50 (.h:538)
private:
    void copy_farray(const farray<T, N>&);

    T data[N]; // 0x10
};

// size N * sizeof(T) + 8
template<typename T, int N>
farray<T, N> farray<T, N>::operator=(const farray<T, N>& other)
{
    this->copy(other);
    return *this;
}

template<typename T, int N>
class circ_array
{
public:
    circ_array(); // <int,30> 0x00B6F6B0 (.h:623)
    ~circ_array(); // <int,30> 0x00B6F6C0 (.h:630)
    int size() const; // <int,30> 0x00B6F6D0 (.h:635)
    int capacity() const;
    const T& operator[](int) const;
    T& operator[](int i); // <int,30> 0x00B6F6E0 (.h:642), data[(offset - i + N) % N]
    int add(const T*, unsigned int);
    int add(const T& t); // <int,30> 0x00B6F700 (.h:656)
    void remove_all();
    int find(const T&, bool) const;
    bool is_member(const T&) const;

private:
    T data[N]; // 0x00
    int num;
    int offset; // index of the newest element
};

template<typename T>
int find_in_array(const T& to_find, const T* arr, int num_entries) // <unsigned int> 0x008A5D50 (.h:749)
{
    for (int i = 0; i < num_entries; i++)
    {
        if (arr[i] == to_find)
        {
            return i;
        }
    }
    return -1;
}

// Comparators for base_array::sort.
bool cmp_int(const int& t1, const int& t2); // 0x00594000 (:12)
bool cmp_uint(const unsigned int& t1, const unsigned int& t2); // 0x00594020 (:17)
bool cmp_float(const float& t1, const float& t2); // 0x00594030 (:22)
bool cmp_str(const char*& t1, const char*& t2); // 0x00594060 (:27)
bool cmp_istr(const char*& t1, const char*& t2); // 0x005940B0 (:32)
bool cmp_str(const char* const& t1, const char* const& t2); // 0x005940D0 (:37)
bool cmp_istr(const char* const& t1, const char* const& t2); // 0x00594120 (:42)

#include "volition/vlib/code/util/varray.inl"
