// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix4f.inl
//
// Havok 2014.2.5: ported from Havok 2014.1 (Common/Base/Math/Matrix/hkMatrix4f.inl), checked against rfg.exe.

HK_FORCE_INLINE void hkMatrix4f::setDiagonal( hkVector4fParameter v )
{
    const hkVector4f* HK_RESTRICT id = (const hkVector4f*)(g_vectorfConstants + HK_QUADREAL_1000);
    m_col0.setMul(id[0], v);
    m_col1.setMul(id[1], v);
    m_col2.setMul(id[2], v);
    m_col3.setMul(id[3], v);
}

HK_FORCE_INLINE void hkMatrix4f::getDiagonal( hkVector4f& v ) const
{
    const hkVector4f* HK_RESTRICT id = (const hkVector4f*)(g_vectorfConstants + HK_QUADREAL_1000);
    v.setZero();
    v.addMul(id[0], m_col0);
    v.addMul(id[1], m_col1);
    v.addMul(id[2], m_col2);
    v.addMul(id[3], m_col3);
}

HK_FORCE_INLINE hkBool32 hkMatrix4f::isApproximatelyIdentity( hkSimdFloat32Parameter epsilon ) const
{
    hkVector4f epsilon_v; epsilon_v.setAll(epsilon);
    const hkVector4f* HK_RESTRICT id = (const hkVector4f*)(g_vectorfConstants + HK_QUADREAL_1000);

    hkVector4f t0; t0.setSub(m_col0, id[0]); t0.setAbs( t0 );
    hkVector4f t1; t1.setSub(m_col1, id[1]); t1.setAbs( t1 );
    hkVector4f t2; t2.setSub(m_col2, id[2]); t2.setAbs( t2 );
    hkVector4f t3; t3.setSub(m_col3, id[3]); t3.setAbs( t3 );

    t0.setMax( t0, t1 );
    t2.setMax( t2, t3 );
    t0.setMax( t0, t2 );
    return t0.lessEqual( epsilon_v ).allAreSet();
}

HK_FORCE_INLINE void hkMatrix4f::setMul( const hkMatrix4f& a, const hkMatrix4f& b )
{
    hkVector4f col0, col1, col2, col3;

    col0.setMul( b.m_col0.getComponent<0>(), a.m_col0 );
    col1.setMul( b.m_col1.getComponent<0>(), a.m_col0 );
    col2.setMul( b.m_col2.getComponent<0>(), a.m_col0 );
    col3.setMul( b.m_col3.getComponent<0>(), a.m_col0 );

    col0.addMul( b.m_col0.getComponent<1>(), a.m_col1 );
    col1.addMul( b.m_col1.getComponent<1>(), a.m_col1 );
    col2.addMul( b.m_col2.getComponent<1>(), a.m_col1 );
    col3.addMul( b.m_col3.getComponent<1>(), a.m_col1 );

    col0.addMul( b.m_col0.getComponent<2>(), a.m_col2 );
    col1.addMul( b.m_col1.getComponent<2>(), a.m_col2 );
    col2.addMul( b.m_col2.getComponent<2>(), a.m_col2 );
    col3.addMul( b.m_col3.getComponent<2>(), a.m_col2 );

    col0.addMul( b.m_col0.getComponent<3>(), a.m_col3 );
    col1.addMul( b.m_col1.getComponent<3>(), a.m_col3 );
    col2.addMul( b.m_col2.getComponent<3>(), a.m_col3 );
    col3.addMul( b.m_col3.getComponent<3>(), a.m_col3 );

    m_col0 = col0;
    m_col1 = col1;
    m_col2 = col2;
    m_col3 = col3;
}

HK_FORCE_INLINE void hkMatrix4f::resetFourthRow()
{
    m_col0.zeroComponent<3>();
    m_col1.zeroComponent<3>();
    m_col2.zeroComponent<3>();
    m_col3.setComponent<3>( hkSimdFloat32::getConstant<HK_QUADREAL_1>() );
}

HK_FORCE_INLINE void hkMatrix4f::set(const hkTransformf& t)
{
    const hkRotationf& r = t.getRotation();
    m_col0 = r.getColumn<0>();
    m_col1 = r.getColumn<1>();
    m_col2 = r.getColumn<2>();
    m_col3 = t.getTranslation();
    resetFourthRow();
}

HK_FORCE_INLINE void hkMatrix4f::get(hkTransformf& t) const
{
    t.getRotation().setCols(m_col0, m_col1, m_col2);
    t.setTranslation(m_col3);
}
