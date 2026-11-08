// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape_default.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3, HK5): the default height field casts (DDA, or the coarse min/max tree when the
// field has coarse levels) and the optional component hkpSampledHeightField_AllCasts that installs them as the
// static cast functions (requested by hkProductFeatures::initialize).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"

// 0x00D83F80 (:14)
void hkpSampledHeightField_registerAllRayCastFunctions()
{
    hkpSampledHeightFieldShape::s_rayCastFunc = &hkpSampledHeightFieldShape::castRayDefault;
    hkpSampledHeightFieldShape::s_sphereCastFunc = &hkpSampledHeightFieldShape::castSphereDefault;
}

// 0x03A287C4, constructed by `dynamic initializer for 'hkOptionalComponent_hkpSampledHeightField_AllCasts'' 0x01094D40
// (:21)
HK_OPTIONAL_COMPONENT_DEFINE_MANUAL(hkpSampledHeightField_AllCasts, hkpSampledHeightField_registerAllRayCastFunctions);

// 0x00D83F40 (:28)
void HK_CALL hkpSampledHeightFieldShape::castRayDefault(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    HK_OPTIONAL_COMPONENT_MARK_USED(hkpSampledHeightField_AllCasts);
    if (m_coarseness > 0)
    {
        castRayCoarseTree(input, cdBody, collector);
        return;
    }
    castRayDda(input, cdBody, collector);
}

// 0x00D83F60 (:41)
void HK_CALL hkpSampledHeightFieldShape::castSphereDefault(const hkpHeightFieldShape::hkpSphereCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const
{
    HK_OPTIONAL_COMPONENT_MARK_USED(hkpSampledHeightField_AllCasts);
    if (m_coarseness > 0)
    {
        castSphereCoarseTree(input, cdBody, collector);
        return;
    }
    castSphereDda(input, cdBody, collector);
}
