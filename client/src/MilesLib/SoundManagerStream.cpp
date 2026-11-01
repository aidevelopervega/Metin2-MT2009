#include "stdafx.h"
#include "SoundManagerStream.h"
#if defined(MT2_USE_MINIAUDIO)
#include "MiniaudioEngine.h"
#endif

CSoundManagerStream::CSoundManagerStream()
{
}

CSoundManagerStream::~CSoundManagerStream()
{
}

bool CSoundManagerStream::Initialize()
{
	CSoundBase::Initialize();

	if (ms_DIGDriver)
		return true;

#if defined(MT2_USE_MINIAUDIO)
	ms_DIGDriver = reinterpret_cast<HDIGDRIVER>(&mileslib_miniaudio::MiniaudioEngine::Instance());
#else
	ms_DIGDriver = AIL_open_digital_driver(44100, 16, 2, 0);
#endif

	for (int i = 0; i < MUSIC_INSTANCE_MAX_NUM; ++i)
		m_Instances[i].Initialize();

	return true;
}

void CSoundManagerStream::Destroy()
{
	for (int i=0; i<MUSIC_INSTANCE_MAX_NUM; ++i)
#if defined(MT2_USE_MINIAUDIO)
	{
		m_Instances[i].Stop();
		m_Instances[i].Destroy();
	}
#else
		m_Instances[i].Stop();
#endif

	CSoundBase::Destroy();
}

bool CSoundManagerStream::SetInstance(DWORD dwIndex, const char* filename)
{
	if (!CheckInstanceIndex(dwIndex))
		return false;

#if defined(MT2_USE_MINIAUDIO)
	HSTREAM hStream = reinterpret_cast<HSTREAM>(mileslib_miniaudio::MiniaudioEngine::Instance().OpenStreamSlot(filename));
#else
	HSTREAM hStream = AIL_open_stream(ms_DIGDriver, filename, 0);
#endif

	if (NULL == hStream)
		return false;

	m_Instances[dwIndex].SetStream(hStream);

	return true;
}

CSoundInstanceStream * CSoundManagerStream::GetInstance(DWORD dwIndex)
{
	if (!CheckInstanceIndex(dwIndex))
		return NULL;

	return &m_Instances[dwIndex];
}

bool CSoundManagerStream::CheckInstanceIndex(DWORD dwIndex)
{
	if (dwIndex >= DWORD(MUSIC_INSTANCE_MAX_NUM))
		return false;

	return true;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
