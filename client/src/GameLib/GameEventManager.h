#pragma once

#include "ActorInstance.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	CGameEventManager
//
//
//

class CGameEventManager : public CSingleton<CGameEventManager>, public CScreen
{
	public:
		CGameEventManager();
		virtual ~CGameEventManager();

		void SetCenterPosition(float fx, float fy, float fz);
		void Update();

		void ProcessEventScreenWaving(CActorInstance * pActorInstance, const CRaceMotionData::TScreenWavingEventData * c_pData);

	protected:
		TPixelPosition m_CenterPosition;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
