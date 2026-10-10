#include "stdafx.h"
#include "EffectUpdateDecorator.h"
#include "ParticleInstance.h"
namespace NEffectUpdateDecorator
{
	CBaseDecorator* CAirResistanceDecorator::__Clone(CParticleInstance* pfi, CParticleInstance* pi)
	{
		pi->m_fAirResistance = pfi->m_fAirResistance;
		return new CAirResistanceDecorator;
	}

	void CAirResistanceDecorator::__Excute(const CDecoratorData & d)
	{
		d.pInstance->m_v3Velocity *= 1.0f-d.pInstance->m_fAirResistance;
	}
	CBaseDecorator* CGravityDecorator::__Clone(CParticleInstance* pfi, CParticleInstance* pi)
	{
		pi->m_fGravity = pfi->m_fGravity;
		return new CGravityDecorator;
	}

	void CGravityDecorator::__Excute(const CDecoratorData& d)
	{
		d.pInstance->m_v3Velocity.z -= d.pInstance->m_fGravity * d.fElapsedTime;
	}

	CBaseDecorator* CRotationDecorator::__Clone(CParticleInstance* pfi, CParticleInstance* pi)
	{
		pi->m_fRotationSpeed = pfi->m_fRotationSpeed;
		return new CRotationDecorator;
	}

	void CRotationDecorator::__Excute(const CDecoratorData& d)
	{
		d.pInstance->m_fRotation += d.pInstance->m_fRotationSpeed * d.fElapsedTime;
	}
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
