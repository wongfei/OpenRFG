// Original: library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.cpp
//
// OcclusionQuery, a D3D11 occlusion query.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>

struct ID3D11Query;

namespace keen
{
    // size 0x4
    struct OcclusionQuery
    {
        ID3D11Query* pQuery; // 0x00
    };
}
