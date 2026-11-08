// Original: volition/stream2/code/stream2.h (inline template members; included at the end of stream2.h)
//
// stream2_data_hash keys data by (file name, srid): the name CRC in the high half of the key and, for SCM_CONTAINER
// containers only (stream2_container_data_is_unique), the srid in the low half. Data of SCM_PRIM containers is
// shared by every container holding the primitive, so its key has srid 0.

#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/math/crc.h"

// <material *,256,unsigned short> 0x0080B520
template<typename T, int N, typename I>
stream2_data_hash<T, N, I>::stream2_data_hash()
    : hash(false)
{
}

// <material *,256,unsigned short> 0x00808AC0
template<typename T, int N, typename I>
stream2_data_hash<T, N, I>::~stream2_data_hash()
{
}

// <material *,256,unsigned short> 0x007FA880 (.h:162)
template<typename T, int N, typename I>
uint64_t stream2_data_hash<T, N, I>::make_key(const char* fname, unsigned int srid)
{
    const unsigned int unique = ((stream2_container*)srid)->cdef->mode == SCM_CONTAINER ? srid : 0;
    return ((uint64_t)crc_stri(fname, 0) << 32) | unique;
}

// <material *,256,unsigned short> 0x00809A80 (.h:129)
template<typename T, int N, typename I>
bool stream2_data_hash<T, N, I>::get(const char* fname, unsigned int srid, T& data)
{
    if (srid == 0)
    {
        return false;
    }
    return hash.get(make_key(fname, srid), data);
}

// <material *,256,unsigned short> 0x00800FE0 (.h:145)
template<typename T, int N, typename I>
bool stream2_data_hash<T, N, I>::insert(const char* fname, unsigned int srid, T data)
{
    if (srid == 0)
    {
        return false;
    }
    return hash.insert_key_data(make_key(fname, srid), data);
}

// <material *,256,unsigned short> 0x00801030 (.h:153)
template<typename T, int N, typename I>
void stream2_data_hash<T, N, I>::remove(const char* fname, unsigned int srid)
{
    if (srid != 0)
    {
        hash.remove(make_key(fname, srid));
    }
}
