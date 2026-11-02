#include "stdafx.h"
#include "SoundManagerStream.h"
#include "MiniaudioEngine.h"

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

	ms_DIGDriver = reinterpret_cast<HDIGDRIVER>(&mileslib_miniaudio::MiniaudioEngine::Instance());

	for (int i = 0; i < MUSIC_INSTANCE_MAX_NUM; ++i)
		m_Instances[i].Initialize();

	return true;
}

void CSoundManagerStream::Destroy()
{
	for (int i = 0; i < MUSIC_INSTANCE_MAX_NUM; ++i)
	{
		m_Instances[i].Stop();
		m_Instances[i].Destroy();
	}

	CSoundBase::Destroy();
}

bool CSoundManagerStream::SetInstance(DWORD dwIndex, const char* filename)
{
	if (!CheckInstanceIndex(dwIndex))
		return false;

	HSTREAM hStream = reinterpret_cast<HSTREAM>(mileslib_miniaudio::MiniaudioEngine::Instance().OpenStreamSlot(filename));

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
