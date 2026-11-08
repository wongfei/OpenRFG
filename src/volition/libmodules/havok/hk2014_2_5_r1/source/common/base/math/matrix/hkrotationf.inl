// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.inl
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Matrix/hkRotationf.inl), checked against rfg.exe.
HK_FORCE_INLINE /*static*/ const hkRotationf& HK_CALL hkRotationf::getIdentity()
{
    union { const hkQuadFloat32* r; const hkRotationf* m; } r2m;
    r2m.r = g_vectorfConstants + HK_QUADREAL_1000;
    return *r2m.m;
}
