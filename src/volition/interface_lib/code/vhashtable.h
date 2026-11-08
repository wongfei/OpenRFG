// Original: volition/interface_lib/code/vhashtable.h
//
// A small chained hash table template (key, value, hasher); the bitmap sheets' image lookup is its one instantiation.
// The methods are in vhashtable.inl.
#pragma once

// size (<checksum_stri,unsigned int,bitmap_image_hasher>) 0x14
template<typename K, typename V, typename H>
class vhashtable
{
public:
    typedef V* T_REFERENCE_PTR;
    typedef const V* T_REFERENCE_CONST_PTR;

    vhashtable(); // <checksum_stri,unsigned int,bitmap_image_hasher> 0x00467480 (vhashtable.inl:22)
    virtual ~vhashtable(); // vtbl+0x00 <checksum_stri,unsigned int,bitmap_image_hasher> 0x004672C0 (.h:70)
    virtual void initialize(V* table_buffer, unsigned int table_size); // vtbl+0x04 <checksum_stri,unsigned int,bitmap_image_hasher> 0x004674A0 (vhashtable.inl:31)
    virtual bool insert(const K& key, const V& reference, H& functor); // vtbl+0x08 <checksum_stri,unsigned int,bitmap_image_hasher> 0x00470D90 (vhashtable.inl:70)
    bool find(const V*&, const K&, H&) const;
    virtual bool find(V*& p_reference, const K& key, H& functor); // vtbl+0x0C <checksum_stri,unsigned int,bitmap_image_hasher> 0x00470DF0 (vhashtable.inl:137)
    virtual bool remove(const K& key, H& functor); // vtbl+0x10 <checksum_stri,unsigned int,bitmap_image_hasher> 0x004674C0 (vhashtable.inl:198)
    virtual void clear(); // vtbl+0x14 <checksum_stri,unsigned int,bitmap_image_hasher> 0x00467500 (vhashtable.inl:221)
protected:
    unsigned int key_to_index(H& functor, const K& key) const; // <checksum_stri,unsigned int,bitmap_image_hasher> 0x00467EE0 (vhashtable.inl:48)

    // 0x00 vfptr
    unsigned int m_table_size; // 0x04
    V* m_table; // 0x08
    unsigned int m_num_in_table; // 0x0C
    unsigned int m_search_limit; // 0x10
};
