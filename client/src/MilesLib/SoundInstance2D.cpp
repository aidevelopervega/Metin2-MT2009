#include "stdafx.h"
#include "SoundManager2D.h"
#include "MiniaudioEngine.h"

CSoundInstance2D::CSoundInstance2D() : m_sample(NULL), m_pSoundData(NULL)
{
}

CSoundInstance2D::~CSoundInstance2D()
{
	Destroy();
}

void CSoundInstance2D::Destroy()
{
	SAFE_RELEASE(m_pSoundData);

	if (m_sample)
	{
		mileslib_miniaudio::MiniaudioEngine::Instance().FreeVoiceSlot(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
		m_sample = NULL;
	}
}

bool CSoundInstance2D::Initialize()
{
	if (m_sample)
		return true;

	m_sample = reinterpret_cast<HSAMPLE>(mileslib_miniaudio::MiniaudioEngine::Instance().AllocateVoiceSlot(false));
	return m_sample ? true : false;
}

bool CSoundInstance2D::SetSound(CSoundData * pSoundData)
{
	assert(m_sample != NULL && pSoundData != NULL);

	LPVOID lpData = pSoundData->Get();

	if (!mileslib_miniaudio::MiniaudioEngine::Instance().BindVoiceSoundData(
		reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample),
		lpData,
		pSoundData->GetSize(),
		pSoundData->GetFileName()))
	{
		if (m_pSoundData != NULL)
		{
			m_pSoundData->Release();
			m_pSoundData = NULL;
		}
		pSoundData->Release();
		TraceError("miniaudio BindVoiceSoundData failed: %s", pSoundData->GetFileName());
		return false;
	}

	if (m_pSoundData != NULL)
	{
		m_pSoundData->Release();
		m_pSoundData = NULL;
	}

	m_pSoundData = pSoundData;
	return true;
}

bool CSoundInstance2D::IsDone() const
{
	return mileslib_miniaudio::MiniaudioEngine::Instance().IsVoiceDone(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance2D::Play(int iLoopCount, DWORD dwPlayCycleTimeLimit) const
{
	(void)dwPlayCycleTimeLimit;
	mileslib_miniaudio::MiniaudioEngine::Instance().PlayVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), iLoopCount);
}

void CSoundInstance2D::Pause() const
{
	mileslib_miniaudio::MiniaudioEngine::Instance().PauseVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance2D::Resume() const
{
	mileslib_miniaudio::MiniaudioEngine::Instance().ResumeVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance2D::Stop()
{
	mileslib_miniaudio::MiniaudioEngine::Instance().StopVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance2D::GetVolume(float& rfVolume) const
{
	rfVolume = mileslib_miniaudio::MiniaudioEngine::Instance().GetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance2D::SetVolume(float volume) const
{
	volume = max(0.0f, min(1.0f, volume));
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), volume);
}

void CSoundInstance2D::SetPosition(float x, float y, float z) const
{
	assert(!"must not call this method");
}

void CSoundInstance2D::SetOrientation(float x_face, float y_face, float z_face,
									  float x_normal, float y_normal, float z_normal) const
{
	assert(!"must not call this method");
}

void CSoundInstance2D::SetVelocity(float fx, float fy, float fz, float fMagnitude) const
{
	assert(!"must not call this method");
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
