// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrixdecomposition.inl
//
// Havok 2014.2.5: ported from Havok 2014.1 (Common/Base/Math/Matrix/hkMatrixDecomposition.inl), checked against rfg.exe.

// 0x00FB7F20
template <typename FT>
void HK_CALL hkMatrixDecompositionImpl<FT>::decompose4x4ColTransform(const typename hkRealTypes<FT>::Pod* matrixIn, typename hkMatrixDecompositionImpl<FT>::Decomposition& decompOut)
{
    typename hkRealTypes<FT>::Matrix4 m; m.set4x4ColumnMajor(matrixIn);
    decomposeMatrix(m, decompOut);
}

// 0x00FB7710
template <typename FT>
void HK_CALL hkMatrixDecompositionImpl<FT>::decomposeMatrix(const typename hkRealTypes<FT>::Matrix4& matrixIn, typename hkMatrixDecompositionImpl<FT>::Decomposition& decompOut)
{
    const int maxNumIterations = 30; // 0x1E in the binary; Havok's note: usually around 7
    const typename hkRealTypes<FT>::Pod tolerance = hkRealTypes<FT>::Scalar::template getConstant<HK_QUADREAL_EPS>().getReal();

    typename hkRealTypes<FT>::Matrix4 m4 = matrixIn;
    m4.resetFourthRow();

    typename hkRealTypes<FT>::Transform t; m4.get(t);

    // Translation
    decompOut.m_translation = t.getTranslation();

    // Basis (rotation matrix): the polar decomposition iteration Q = (Q + inverse(transpose(Q))) / 2
    {
        typename hkRealTypes<FT>::Rotation qCur, qNext, qInvT;

        int numIterations = 0;

        qNext = t.getRotation();
        do
        {
            qCur = qNext;

            // 2014.1 copies qCur and calls invert(tolerance); 2014.2.5 has setInverse (0x00CF9CB0). Its result is
            // dropped: a singular matrix leaves qInvT zero and the loop carries on.
            qInvT.setInverse(qCur);
            qInvT.transpose();

            typename hkRealTypes<FT>::Matrix3 temp;
            temp = qCur;
            temp.add(qInvT);
            qNext.setMul(hkRealTypes<FT>::Scalar::template getConstant<HK_QUADREAL_INV_2>(), temp);

            numIterations++;
        } while ((numIterations < maxNumIterations) && (!qNext.isApproximatelyEqual(qCur, tolerance)));

        decompOut.m_basis = qNext;
    }

    // Quaternion + "flips" flag
    {
        typename hkRealTypes<FT>::Vector c1 = decompOut.m_basis.template getColumn<0>();
        c1.template normalize<3>();
        typename hkRealTypes<FT>::Vector c2 = decompOut.m_basis.template getColumn<1>();
        c2.template normalize<3>();
        typename hkRealTypes<FT>::Vector c3 = decompOut.m_basis.template getColumn<2>();
        c3.template normalize<3>();

        typename hkRealTypes<FT>::Vector r0; r0.setCross(c2, c3);
        const typename hkRealTypes<FT>::Scalar determinant = c1.template dot<3>(r0);
        const typename hkRealTypes<FT>::Comparison detLessZero = determinant.lessZero();

        // The rotation is a change of hand-ness: one column is flipped and the scale counteracts it.
        c1.setFlipSign(c1, detLessZero);

        decompOut.m_flips = detLessZero.anyIsSet();

        typename hkRealTypes<FT>::Rotation rot;
        rot.setCols(c1, c2, c3);
        decompOut.m_rotation.set(rot);
        decompOut.m_rotation.normalize();
    }

    // Scale and skew
    {
        typename hkRealTypes<FT>::Transform TR; TR.set(decompOut.m_rotation, decompOut.m_translation);
        typename hkRealTypes<FT>::Transform TRInv; TRInv.setInverse(TR);

        typename hkRealTypes<FT>::Matrix4 TRInv4; TRInv4.set(TRInv);
        decompOut.m_scaleAndSkew.setMul(TRInv4, m4);
    }

    const typename hkRealTypes<FT>::Vector one = hkRealTypes<FT>::Vector::template getConstant<HK_QUADREAL_1>();
    typename hkRealTypes<FT>::Scalar eps; eps.setFromFloat(typename hkRealTypes<FT>::Pod(1e-3f));

    // Scale: the diagonal
    decompOut.m_scaleAndSkew.getDiagonal(decompOut.m_scale);
    decompOut.m_scale.setW(one);
    decompOut.m_hasScale = !decompOut.m_scale.template allEqual<4>(one, eps);

    // Skew: inverse(scale) * scaleAndSkew. The reciprocal is unguarded in the binary (no zero check), so a zero scale
    // component gives infinities here, as in rfg.exe.
    {
        typename hkRealTypes<FT>::Vector iscale;
        iscale.template setReciprocal<HK_ACC_MID, HK_DIV_IGNORE>(decompOut.m_scale);
        iscale.setW(one);
        typename hkRealTypes<FT>::Matrix4 scaleInv; scaleInv.setDiagonal(iscale);
        decompOut.m_skew.setMul(scaleInv, decompOut.m_scaleAndSkew);

        decompOut.m_hasSkew = !decompOut.m_skew.isApproximatelyIdentity(eps);
    }
}
