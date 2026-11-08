// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h
//
// Havok 2014.2.5 (havok_impl.md HK2): the math classes and their inline code, structured as Havok 2014.1 hkMath.h and
// its Header/hkMathHeader*.h parts (single precision, SSE2 only). The class headers route here: the inline code of
// each class uses the others, so every declaration comes first and every .inl after them.
#ifndef HK_MATH_MATH_H
#define HK_MATH_MATH_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include <math.h>
#include <float.h>

// hkMathHeaderTypes.h, hkMathHeaderConstantDefinitions.h, hkMathHeaderEnums.h, hkMathHeaderConstants.h
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheadertypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheaderconstantdefinitions.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheaderenums.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheaderconstants.h"

// hkMathHeaderForwardDeclarations.h: type discriminator for floating point resolution (single precision only)
template <typename POD>
struct hkRealTypes
{};

template <>
struct hkRealTypes<hkFloat32>
{
    typedef hkVector4f Vector;
    typedef hkVector4fParameter VectorParameter;
    typedef hkQuaternionf Quaternion;
    typedef hkQuaternionfParameter QuaternionParameter;
    typedef hkSimdFloat32 Scalar;
    typedef hkSimdFloat32Parameter ScalarParameter;
    typedef hkVector4fComparison Comparison;
    typedef hkVector4fComparisonParameter ComparisonParameter;
    typedef hkFloat32 Pod;
    typedef hkMatrix3f Matrix3;
    typedef hkMatrix4f Matrix4;
    typedef hkRotationf Rotation;
    typedef hkTransformf Transform;
    typedef hkQTransformf QTransform;
    typedef hkQsTransformf QsTransform;
};

// hkMathHeaderFuncs.h
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/functions/sse/hkssemathfuncs.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/functions/hkmathfuncs.h"

// hkMathHeaderIncludes.h: declarations
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qtransform/hkqtransformf.h"

// hkMathHeaderPlatformImplementations.h
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/sse/hkssevector4.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/sse/hkssevector4comparison.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/sse/hkssesimdreal.inl"

// hkMathHeaderSharedImplementations.h
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix4f.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.inl"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qtransform/hkqtransformf.inl"

// hkMathHeaderAliases1.h/2.h (single precision)
typedef hkQuadFloat32 hkQuadReal;
typedef hkSingleFloat32 hkSingleReal;
typedef hkVector4f hkVector4;
typedef hkVector4fParameter hkVector4Parameter;
typedef hkVector4fComparison hkVector4Comparison;
typedef hkVector4fComparisonParameter hkVector4ComparisonParameter;
typedef hkSimdFloat32 hkSimdReal;
typedef hkSimdFloat32Parameter hkSimdRealParameter;
typedef hkQuaternionf hkQuaternion;
typedef hkQuaternionfParameter hkQuaternionParameter;
typedef hkMatrix3f hkMatrix3;
typedef hkMatrix4f hkMatrix4;
typedef hkRotationf hkRotation;
typedef hkTransformf hkTransform;
typedef hkQTransformf hkQTransform;
typedef hkQsTransformf hkQsTransform;
#define g_vectorConstants g_vectorfConstants
#define HK_QUADREAL_CONSTANT HK_QUADFLOAT_CONSTANT
#define HK_TRANSPOSE3 HK_TRANSPOSE3f
#define HK_TRANSPOSE4 HK_TRANSPOSE4f
#define hkFourTransposedPoints hkFourTransposedPointsf
#define hkSweptTransform hkSweptTransformf
#define HK_REAL_PI HK_FLOAT_PI
#define HK_REAL_DEG_TO_RAD HK_FLOAT_DEG_TO_RAD
#define HK_REAL_RAD_TO_DEG HK_FLOAT_RAD_TO_DEG
#define HK_REAL_EPSILON HK_FLOAT_EPSILON
#define HK_REAL_MIN HK_FLOAT_MIN
#define HK_REAL_MAX HK_FLOAT_MAX
#define HK_REAL_HIGH HK_FLOAT_HIGH
#define HK_MASK_TO_VECTOR(x) (hkQuadReal) x
#define hkSimdReal_Minus1 hkSimdFloat32_Minus1
#define hkSimdReal_0 hkSimdFloat32_0
#define hkSimdReal_1 hkSimdFloat32_1
#define hkSimdReal_2 hkSimdFloat32_2
#define hkSimdReal_3 hkSimdFloat32_3
#define hkSimdReal_4 hkSimdFloat32_4
#define hkSimdReal_5 hkSimdFloat32_5
#define hkSimdReal_6 hkSimdFloat32_6
#define hkSimdReal_7 hkSimdFloat32_7
#define hkSimdReal_8 hkSimdFloat32_8
#define hkSimdReal_15 hkSimdFloat32_15
#define hkSimdReal_16 hkSimdFloat32_16
#define hkSimdReal_255 hkSimdFloat32_255
#define hkSimdReal_256 hkSimdFloat32_256
#define hkSimdReal_Inv0 hkSimdFloat32_Inv0
#define hkSimdReal_Inv1 hkSimdFloat32_Inv1
#define hkSimdReal_Inv2 hkSimdFloat32_Inv2
#define hkSimdReal_Inv3 hkSimdFloat32_Inv3
#define hkSimdReal_Inv4 hkSimdFloat32_Inv4
#define hkSimdReal_Inv5 hkSimdFloat32_Inv5
#define hkSimdReal_Inv6 hkSimdFloat32_Inv6
#define hkSimdReal_Inv7 hkSimdFloat32_Inv7
#define hkSimdReal_Inv8 hkSimdFloat32_Inv8
#define hkSimdReal_Inv_15 hkSimdFloat32_Inv_15
#define hkSimdReal_Inv_127 hkSimdFloat32_Inv_127
#define hkSimdReal_Inv_255 hkSimdFloat32_Inv_255
#define hkSimdReal_High hkSimdFloat32_High
#define hkSimdReal_Max hkSimdFloat32_Max
#define hkSimdReal_Min hkSimdFloat32_Min
#define hkSimdReal_Eps hkSimdFloat32_Eps
#define hkSimdReal_EpsSqrd hkSimdFloat32_EpsSqrd
#define hkSimdReal_MinusMax hkSimdFloat32_MinusMax
#define hkSimdReal_MinusMin hkSimdFloat32_MinusMin
#define hkSimdReal_PackHalf hkSimdFloat32_PackHalf
#define hkSimdReal_Pack16 hkSimdFloat32_Pack16
#define hkSimdReal_Unpack16 hkSimdFloat32_Unpack16
#define hkSimdReal_Pi hkSimdFloat32_Pi
#define hkSimdReal_PiOver2 hkSimdFloat32_PiOver2
#define hkSimdReal_PiOver4 hkSimdFloat32_PiOver4
#define hkSimdReal_4PiOver3 hkSimdFloat32_4PiOver3
#define hkSimdReal_TwoPi hkSimdFloat32_TwoPi
#define hkSimdReal_FourPi hkSimdFloat32_FourPi
#define hkSimdReal_Deg2Rad hkSimdFloat32_Deg2Rad
#define hkSimdReal_Rad2Deg hkSimdFloat32_Rad2Deg
#define hkSimdReal_Half hkSimdFloat32_Half

// hkMathHeaderAliases2.h
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrixdecomposition.h"

#endif // HK_MATH_MATH_H
