// Original: volition/render_lib/code/kghelper.hpp
//
// Small helpers that convert between render_lib and keen graphics types (formats, matrices, states).
#pragma once


struct float3x3;
class matrix;
class rl_color_float;
class vector;
class vector4;

namespace keen
{
    struct float3;
    struct float4;
}

keen::float3 createFloat3(const vector& v); // 0x004B7710 (.h:17)
keen::float4 createFloat4(const vector& v, float w); // 0x004B7750 (.h:22)
keen::float4 createFloat4(const rl_color_float& v); // 0x004B77A0 (.h:32)
void setFloat3(keen::float3* pValue, const vector& v); // 0x004B77F0 (.h:37)
void setFloat4(keen::float4* pValue, const vector& v, float w); // 0x004B7820 (.h:42)
void setFloat4(keen::float4* pValue, const vector4& v); // 0x004B7850 (.h:47)
void setFloat4(keen::float4* pValue, const rl_color_float& v); // 0x004B7880 (.h:52)
void setFloat4(keen::float4* pValue, const rl_color_float& v, float alphaOverride); // 0x004B78B0 (.h:57)
void setFloat3x3(float3x3* pValue, const matrix& v); // 0x004B78E0 (.h:80)
