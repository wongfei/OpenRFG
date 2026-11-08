// Original: volition/rfg/code/havok/havok_mopp_code.cpp
//
// MOPP code blobs (stub_impl.md S4), from rfg.exe: an hkpMoppCode over byte code that stays in the loaded file.

#include "volition/rfg/code/havok/havok_mopp_code.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"
#include "keen/base/trace.hpp"

// 'MOPP' little endian
static const unsigned int HAVOK_MOPP_CODE_SIGNATURE = 0x50504F4D;
// version 300 has no build type field: its byte code starts where later versions keep the build type
static const unsigned int HAVOK_MOPP_CODE_VERSION_NO_BUILD_TYPE = 300;

// 0x008330A0 (:88)
const hkpMoppCode* havok_mopp_code_load_in_place(const havok_mopp_code_header* p_header)
{
    if (p_header->m_signature != HAVOK_MOPP_CODE_SIGNATURE)
    {
        keen::debug::nativePrintArgs("havok_mopp_code_load_in_place: Trying to load invalid MOPP code! Maybe it needs to be re-crunched?");
        return nullptr;
    }
    const unsigned int version = p_header->m_version;
    if (version < HAVOK_MOPP_CODE_VERSION_NO_BUILD_TYPE)
    {
        keen::debug::nativePrintArgs("havok_mopp_code_load_in_place: Unsupported Havok version!");
        return nullptr;
    }
    if (p_header->m_byte_code_size == 0)
    {
        return nullptr;
    }
    hkpMoppCode::CodeInfo info;
    info.m_offset.m_quad = _mm_set_ps(p_header->m_scale, p_header->m_offset_z, p_header->m_offset_y, p_header->m_offset_x);
    hkpMoppCode::BuildType buildType = hkpMoppCode::BUILT_WITHOUT_CHUNK_SUBDIVISION;
    const unsigned char* byteCode = reinterpret_cast<const unsigned char*>(&p_header->m_build_type);
    if (version != HAVOK_MOPP_CODE_VERSION_NO_BUILD_TYPE)
    {
        buildType = static_cast<hkpMoppCode::BuildType>(p_header->m_build_type);
        byteCode += sizeof(unsigned int);
    }
    return new hkpMoppCode(info, byteCode, (int)p_header->m_byte_code_size, buildType);
}
