// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/tyremarks/hkptyremarksinfo.cpp
//
// Havok 2014.2.5 (stub_impl.md S8): per wheel ring buffers of tyre mark points, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/tyremarks/hkptyremarksinfo.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicledata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/hkpvehicleinstance.h"

// 0x00F7FB30
hkpTyremarksWheel::hkpTyremarksWheel()
{
    m_numPoints = 0;
    m_currentPosition = 0;
}

// 0x00F801E0 (hkptyremarksinfo.h:74)
hkpTyremarksWheel::~hkpTyremarksWheel()
{
}

// 0x00F7FAA0
void hkpTyremarksWheel::setNumPoints(int num_points)
{
    m_numPoints = num_points;
    m_currentPosition = 0;
    for (int i = 0; i < m_numPoints; i++)
    {
        hkpTyremarkPoint point;
        m_tyremarkPoints.pushBack(point);
    }
}

// 0x00F7F840
hkpTyremarkPoint::hkpTyremarkPoint()
{
    m_pointLeft.setZero();
    m_pointRight.setZero();
}

// 0x00F7F850
const hkpTyremarkPoint& hkpTyremarksWheel::getTyremarkPoint(int point) const
{
    return m_tyremarkPoints[(m_currentPosition + point) % m_numPoints];
}

// 0x00F7F870: the strength is in the w of the right point
float hkpTyremarkPoint::getTyremarkStrength() const
{
    return m_pointRight(3);
}

// 0x00F7F8D0: a zero strength point follows a zero strength point only once
void hkpTyremarksWheel::addTyremarkPoint(hkpTyremarkPoint& point)
{
    const int prev = (m_currentPosition - 1 + m_numPoints) % m_numPoints;
    if (point.getTyremarkStrength() != 0.0f || m_tyremarkPoints[prev].getTyremarkStrength() != 0.0f)
    {
        m_tyremarkPoints[m_currentPosition] = point;
        m_currentPosition = (m_currentPosition + 1) % m_numPoints;
    }
}

// 0x00F7FB60
hkpTyremarksInfo::hkpTyremarksInfo(const hkpVehicleData& data, int num_points)
{
    m_minTyremarkEnergy = 0.0f;
    m_maxTyremarkEnergy = 0.0f;
    m_tyremarksWheel.setSize(data.m_numWheels);
    for (int i = 0; i < m_tyremarksWheel.getSize(); i++)
    {
        hkpTyremarksWheel* wheel = new hkpTyremarksWheel();
        wheel->setNumPoints(num_points);
        m_tyremarksWheel[i] = wheel;
    }
}

// 0x00F7FC30
hkpTyremarksInfo::~hkpTyremarksInfo()
{
    for (int i = 0; i < m_tyremarksWheel.getSize(); i++)
    {
        m_tyremarksWheel[i]->removeReference();
    }
}

// 0x00F7F930
void hkpTyremarksInfo::updateTyremarksInfo(float timestep, const hkpVehicleInstance* vehicle)
{
    const hkpRigidBody* chassis = vehicle->getChassis();
    hkVector4f offset;
    offset.setMul(hkSimdFloat32::fromFloat(timestep), chassis->getLinearVelocity());
    hkVector4f side;
    side.setRotatedDir(chassis->getTransform().getRotation(), vehicle->m_data->m_chassisOrientation.getColumn<2>());

    for (int w = 0; w < vehicle->m_data->m_numWheels; w++)
    {
        const hkpVehicleInstance::WheelInfo& wheel = vehicle->m_wheelsInfo[w];

        // 255: full strength
        hkSimdFloat32 strength;
        const hkSimdFloat32 energy = hkSimdFloat32::fromFloat(wheel.m_skidEnergyDensity - m_minTyremarkEnergy);
        if (energy.getReal() <= 0.0f)
        {
            strength.setZero();
        }
        else
        {
            hkSimdFloat32 ratio;
            ratio.setDiv(energy, hkSimdFloat32::fromFloat(m_maxTyremarkEnergy - m_minTyremarkEnergy));
            strength.setMin(ratio * hkSimdFloat32::getConstant<HK_QUADREAL_255>(), hkSimdFloat32::getConstant<HK_QUADREAL_255>());
        }

        const float width = vehicle->m_data->m_wheelParams[w].m_width;
        hkVector4f start;
        // 0.05: lifts the mark off the ground
        start.setAddMul(wheel.m_contactPoint.getPosition(), wheel.m_contactPoint.getNormal(), hkSimdFloat32::fromFloat(0.05f));
        const hkSimdFloat32 half = hkSimdFloat32::fromFloat(width) * hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>();

        hkpTyremarkPoint point;
        point.m_pointLeft.setSubMul(start, side, half);
        point.m_pointLeft.add(offset);
        point.m_pointRight.setAddMul(start, side, half);
        point.m_pointRight.add(offset);
        point.m_pointLeft.setW(strength);
        point.m_pointRight.setW(strength);
        m_tyremarksWheel[w]->addTyremarkPoint(point);
    }
}

// 0x00F7F880
void hkpTyremarksInfo::getWheelTyremarksStrips(const hkpVehicleInstance* vehicle, int wheel, hkVector4f* strips_out) const
{
    const hkpTyremarksWheel* tyremarks = m_tyremarksWheel[wheel];
    const int num_points = tyremarks->m_numPoints;
    for (int p = 0; p < num_points; p++)
    {
        const hkpTyremarkPoint& point = tyremarks->getTyremarkPoint(p);
        *strips_out++ = point.m_pointLeft;
        *strips_out++ = point.m_pointRight;
    }
}
