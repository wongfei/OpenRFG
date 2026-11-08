// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxmanifold.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the box-box agents' feature manifold, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxmanifold.h"

// 0x00DD4230: the points' ids cleared (the manifold normal is left as it is)
hkpBoxBoxManifold::hkpBoxBoxManifold()
{
    for (int i = 0; i < 8; i++)
    {
        m_contactPoints[i].m_contactPointId = 0;
    }
    m_faceVertexFeatureCount = 0;
    m_numPoints = 0;
    m_isComplete = false;
}

// 0x00DD41C0: the index of the new point, -1 when the manifold already has the feature pair or is full
int hkpBoxBoxManifold::addPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpFeatureContactPoint& fcp)
{
    const int numPoints = m_numPoints;
    if (numPoints > 8)
    {
        return -1;
    }
    for (int i = numPoints - 1; i >= 0; i--)
    {
        if (m_contactPoints[i].m_featureIdA == fcp.m_featureIdA && m_contactPoints[i].m_featureIdB == fcp.m_featureIdB)
        {
            return -1;
        }
    }
    if (numPoints >= 8)
    {
        return -1;
    }
    m_contactPoints[numPoints] = fcp;
    m_numPoints++;
    return numPoints;
}

// 0x00DD4210: the last point takes its place; the manifold is no longer complete
void hkpBoxBoxManifold::removePoint(int i)
{
    m_isComplete = false;
    m_contactPoints[i] = m_contactPoints[m_numPoints - 1];
    m_numPoints--;
}
