// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpaccurateinertiatensorcomputer.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the mass properties of the convex hull of a point set, with an exact hull
// (hkgpConvexHull), from rfg.exe. A point is a sphere, a segment a capsule, a polygon is thickened by a growing margin
// until it has volume; a solid gets the volume and surface of its convex radius added (inertia scaled to match).
// rfg.exe installs it as hkInertiaTensorComputer::s_computeConvexHullMassPropertiesFunction through the optional
// component hkpAccurateInertiaTensorComputer (hkProductFeatures::initialize).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpaccurateinertiatensorcomputer.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkoptionalcomponent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/geometryutilities/inertia/hkinertiatensorcomputer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhull.h"

#include <emmintrin.h>

// 0x016F50EC (hkpconvexshape.cpp)
extern float hkConvexShapeDefaultRadius;

// 0x03A380B8, constructed by `dynamic initializer for 'hkOptionalComponent_hkpAccurateInertiaTensorComputer''
// 0x01098000 (:16): hkProductFeatures::initialize installs computeConvexHullMassProperties through it.
HK_OPTIONAL_COMPONENT_DEFINE(hkpAccurateInertiaTensorComputer, hkInertiaTensorComputer::s_computeConvexHullMassPropertiesFunction, hkpAccurateInertiaTensorComputer::computeConvexHullMassProperties);

namespace
{
inline float first(__m128 v) { return _mm_cvtss_f32(v); }

// a strided vertex as (x, y, z, 0)
inline __m128 loadVertex(const hkStridedVertices& vertices, int i)
{
    const float* f = (const float*)((const char*)vertices.m_vertices + i * vertices.m_striding);
    return _mm_movelh_ps(_mm_castpd_ps(_mm_load_sd((const double*)f)), _mm_load_ss(f + 2));
}
}

// 0x00F20440
hkResult hkpAccurateInertiaTensorComputer::computeConvexHullMassProperties(const hkStridedVertices& vertices, float r, hkMassProperties& result)
{
    HK_OPTIONAL_COMPONENT_MARK_USED(hkpAccurateInertiaTensorComputer);
    hkgpConvexHull hull;
    hkgpConvexHull::BuildConfig config;
    config.m_allowLowerDimensions = true;
    config.m_alwaysComputeProjectionPlane = false;
    hull.build(vertices, config);
    const int dimensions = hull.getDimensions();
    const float minRadius = 1.1920929e-05f; // 0x37480000
    const float radius = first(_mm_max_ss(_mm_set_ss(r), _mm_set_ss(minRadius)));
    __m128 radiusVolume = _mm_setzero_ps();

    switch (dimensions)
    {
    case 0:
        return hkInertiaTensorComputer::computeSphereVolumeMassProperties(radius, 1.0f, result);

    case 1:
    {
        hkVector4f a;
        hkVector4f b;
        a.m_quad = loadVertex(vertices, 0);
        b.m_quad = loadVertex(vertices, 1);
        if (vertices.m_numVertices > 2)
        {
            // the extreme points along the first two (compared with 0, not with the first point)
            const __m128 direction = _mm_sub_ps(b.m_quad, a.m_quad);
            float maxDistance = 0.0f;
            float minDistance = 0.0f;
            int maxIndex = 0;
            int minIndex = 0;
            for (int i = 1; i < vertices.m_numVertices; ++i)
            {
                const __m128 p = _mm_mul_ps(loadVertex(vertices, i), direction);
                const float d = first(_mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p, p, 0x55), _mm_shuffle_ps(p, p, 0x00)), _mm_shuffle_ps(p, p, 0xAA)));
                if (d > maxDistance)
                {
                    maxIndex = i;
                    maxDistance = d;
                }
                if (!(d >= minDistance)) // ucomiss/jnc
                {
                    minIndex = i;
                    minDistance = d;
                }
            }
            a.m_quad = loadVertex(vertices, minIndex);
            b.m_quad = loadVertex(vertices, maxIndex);
        }
        return hkInertiaTensorComputer::computeCapsuleVolumeMassProperties(a, b, radius, 1.0f, result);
    }

    case 2:
    {
        // the original radius here (not the clamped one)
        const float defaultMargin = hkConvexShapeDefaultRadius * 0.2f; // 0x3E4CCCCD
        float margin = first(_mm_max_ss(_mm_set_ss(defaultMargin), _mm_set_ss(r)));
        for (;;)
        {
            do
            {
                hull.expandByPlanarMargin(margin);
                if (hull.getDimensions() == 3)
                {
                    hull.buildMassProperties();
                }
                margin *= 2.0f;
            } while (hull.getDimensions() != 3);
            const float volume = first(hull.getVolume().m_real);
            if (!(volume < minRadius) && volume == volume) // ucomiss/jc: NaN loops too
            {
                break;
            }
        }
        break;
    }

    case 3:
    {
        hull.buildMassProperties();
        if (r > 0.0f)
        {
            // the volume of the radius: a sphere (4/3 pi r^3) plus the surface times r
            const __m128 rs = _mm_set1_ps(r);
            const __m128 sphere = _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(4.1887903f), rs), rs), rs); // 0x40860A92
            radiusVolume = _mm_add_ps(sphere, _mm_mul_ps(rs, hull.getSurfaceArea().m_real));
        }
        break;
    }

    default:
        return hkResult(HK_FAILURE);
    }

    result.m_centerOfMass = hull.getCenterOfMass();
    result.m_inertiaTensor = hull.getWorldInertia();
    const __m128 volume = _mm_add_ps(hull.getVolume().m_real, radiusVolume);
    result.m_volume = first(volume);
    result.m_mass = 1.0f;
    if (first(volume) > 0.0f && first(radiusVolume) > 0.0f)
    {
        // scale the inertia by 1 + 2 * radiusVolume / (3 * volume)
        hkSimdFloat32 scale;
        scale.m_real = _mm_add_ps(_mm_div_ps(_mm_add_ps(radiusVolume, radiusVolume), _mm_mul_ps(volume, g_vectorfConstants[HK_QUADREAL_3])), g_vectorfConstants[HK_QUADREAL_1]);
        result.m_inertiaTensor.mul(scale);
    }
    return hkResult(HK_SUCCESS);
}
