#pragma once

#include "../eterLib/GrpScreen.h"

#include "EffectElementBaseInstance.h"
#include "SimpleLightData.h"

class CLightInstance : public CEffectElementBaseInstance
{
	public:
		friend class CLightData;

		CLightInstance();
		virtual ~CLightInstance();

	protected:

		void OnSetDataPointer(CEffectElementBase * pElement);

		void OnInitialize();
		void OnDestroy();

		bool OnUpdate(float fElapsedTime);
		void OnRender();

		DWORD			m_LightID;
		CLightData *	m_pData;
		DWORD			m_dwRangeIndex;

		DWORD			m_iLoopCount;

	public:
		static void DestroySystem();

		static CLightInstance* New();
		static void Delete(CLightInstance* pkData);

		static CDynamicPool<CLightInstance>		ms_kPool;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
