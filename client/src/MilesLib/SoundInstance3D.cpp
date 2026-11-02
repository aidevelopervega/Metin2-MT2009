#include "stdafx.h"
#include "SoundManager3D.h"
#include "../eterBase/Timer.h"
#include "MiniaudioEngine.h"

CSoundInstance3D::CSoundInstance3D() : m_sample(NULL), m_pSoundData(NULL)
{
}

CSoundInstance3D::~CSoundInstance3D()
{
	Destroy();
}

void CSoundInstance3D::Destroy()
{
	SAFE_RELEASE(m_pSoundData);

	if (m_sample)
	{
		mileslib_miniaudio::MiniaudioEngine::Instance().FreeVoiceSlot(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
		m_sample = NULL;
	}
}

bool CSoundInstance3D::Initialize()
{
	if (m_sample)
		return true;

	m_sample = reinterpret_cast<H3DSAMPLE>(mileslib_miniaudio::MiniaudioEngine::Instance().AllocateVoiceSlot(true));
	return m_sample ? true : false;
}

bool CSoundInstance3D::SetSound(CSoundData* pSoundData)
{
	assert(m_sample != NULL && pSoundData != NULL);

	LPVOID lpData = pSoundData->Get();

	if (m_pSoundData != NULL)
	{
		m_pSoundData->Release();
		m_pSoundData = NULL;
	}

	if (!mileslib_miniaudio::MiniaudioEngine::Instance().BindVoiceSoundData(
		reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample),
		lpData,
		pSoundData->GetSize(),
		pSoundData->GetFileName()))
	{
		TraceError("miniaudio BindVoiceSoundData 3D failed: %s", pSoundData->GetFileName());
		pSoundData->Release();
		return false;
	}
	m_pSoundData = pSoundData;
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoicePosition(
		reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), 0.0f, 0.0f, 0.0f);
	return true;
}

bool CSoundInstance3D::IsDone() const
{
	return mileslib_miniaudio::MiniaudioEngine::Instance().IsVoiceDone(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance3D::Play(int iLoopCount, DWORD dwPlayCycleTimeLimit) const
{
	if (!m_pSoundData)
		return;

	DWORD dwCurTime = ELTimer_GetMSec();

	if (dwCurTime - m_pSoundData->GetPlayTime() < dwPlayCycleTimeLimit)
		return;

	m_pSoundData->SetPlayTime(dwCurTime);

	mileslib_miniaudio::MiniaudioEngine::Instance().PlayVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), iLoopCount);
}

void CSoundInstance3D::Pause() const
{
	mileslib_miniaudio::MiniaudioEngine::Instance().PauseVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance3D::Resume() const
{
	mileslib_miniaudio::MiniaudioEngine::Instance().ResumeVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance3D::Stop()
{
	mileslib_miniaudio::MiniaudioEngine::Instance().StopVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance3D::GetVolume(float& rfVolume) const
{
	rfVolume = mileslib_miniaudio::MiniaudioEngine::Instance().GetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
}

void CSoundInstance3D::SetVolume(float volume) const
{
	volume = max(0.0f, min(1.0f, volume));
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), volume);
}

void CSoundInstance3D::SetPosition(float x, float y, float z) const
{
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoicePosition(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), x, y, z);
}

void CSoundInstance3D::SetOrientation(float x_face, float y_face, float z_face,
									  float x_normal, float y_normal, float z_normal) const
{
	assert(!" CSoundInstance3D::SetOrientation - Don't use this function");
}

void CSoundInstance3D::SetVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float fNagnitude) const
{
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoiceVelocity(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), fDistanceX, fDistanceY, fDistanceZ, fNagnitude);
}

void CSoundInstance3D::UpdatePosition(float fElapsedTime)
{
	(void)fElapsedTime;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
