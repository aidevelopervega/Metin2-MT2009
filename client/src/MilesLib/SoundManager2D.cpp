#include "Stdafx.h"
#include "SoundManager2D.h"
#if defined(MT2_USE_MINIAUDIO)
#include "MiniaudioEngine.h"
#endif

CSoundManager2D::CSoundManager2D()
{
}

CSoundManager2D::~CSoundManager2D()
{
}

bool CSoundManager2D::Initialize()
{
	CSoundBase::Initialize();

	if (ms_DIGDriver)
		return true;

#if defined(MT2_USE_MINIAUDIO)
	ms_DIGDriver = reinterpret_cast<HDIGDRIVER>(&mileslib_miniaudio::MiniaudioEngine::Instance());
#else
	ms_DIGDriver = AIL_open_digital_driver(44100, 16, 2, 0);
#endif

	for (int i = 0; i < INSTANCE_MAX_COUNT; ++i)
		ms_Instances[i].Initialize();
/*	ms_DIGDriver = AIL_open_digital_driver(44100,
										   (DIG_F_STEREO_16 & DIG_F_16BITS_MASK) ? 16 : 8,
										   (DIG_F_STEREO_16 & DIG_F_16BITS_MASK) ? 2 : 1,
										   0);
*/
	return true;
}

void CSoundManager2D::Destroy()
{
	for (int i = 0; i < INSTANCE_MAX_COUNT; ++i)
		ms_Instances[i].Destroy();

	if (ms_DIGDriver != NULL)
	{
#if !defined(MT2_USE_MINIAUDIO)
		AIL_close_digital_driver(ms_DIGDriver);
#endif
		ms_DIGDriver = NULL;
	}

	CSoundBase::Destroy();
}

ISoundInstance * CSoundManager2D::GetInstance(const char * c_pszFileName)
{
	DWORD dwFileCRC = GetFileCRC(c_pszFileName);
	TSoundDataMap::iterator itor = ms_dataMap.find(dwFileCRC);

	CSoundData * pkSoundData;

	if (itor == ms_dataMap.end())
		pkSoundData = AddFile(dwFileCRC, c_pszFileName); // CSoundBase::AddFile
	else
		pkSoundData = itor->second;

	assert(pkSoundData != NULL);

	static DWORD k = 0;

	DWORD start = k++;
	DWORD end = start + INSTANCE_MAX_COUNT;

	while (start < end)
	{
		CSoundInstance2D * pkInst = &ms_Instances[start % INSTANCE_MAX_COUNT];

		if (pkInst->IsDone())
		{
			if (!pkInst->SetSound(pkSoundData))
				TraceError("CSoundManager2D::GetInstance (filename: %s)", c_pszFileName);
			return (pkInst);
		}

		++start;
	}

	return NULL;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
