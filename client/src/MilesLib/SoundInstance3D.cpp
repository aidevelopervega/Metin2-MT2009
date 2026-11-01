#include "stdafx.h"
#include "SoundManager3D.h"
#include "../eterBase/Timer.h"
#if defined(MT2_USE_MINIAUDIO)
#include "MiniaudioEngine.h"
#endif

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
#if defined(MT2_USE_MINIAUDIO)
		mileslib_miniaudio::MiniaudioEngine::Instance().FreeVoiceSlot(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
		AIL_release_3D_sample_handle(m_sample);
#endif
		m_sample = NULL;
	}
}

bool CSoundInstance3D::Initialize()
{
	if (m_sample)
		return true;

#if defined(MT2_USE_MINIAUDIO)
	m_sample = reinterpret_cast<H3DSAMPLE>(mileslib_miniaudio::MiniaudioEngine::Instance().AllocateVoiceSlot(true));
#else
	m_sample = AIL_allocate_3D_sample_handle(ms_pProviderDefault->hProvider);
#endif
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

#if defined(MT2_USE_MINIAUDIO)
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
#else
	if (AIL_set_3D_sample_file(m_sample, lpData) == NULL)
	{
		TraceError("%s: %s", AIL_last_error(), pSoundData->GetFileName());
		pSoundData->Release();
		return false;
	}

	m_pSoundData = pSoundData;

	AIL_set_3D_position(m_sample, 0.0F, 0.0F, 0.0F);
	AIL_auto_update_3D_position(m_sample, 0);
#endif
	return true;
}

bool CSoundInstance3D::IsDone() const
{
#if defined(MT2_USE_MINIAUDIO)
	return mileslib_miniaudio::MiniaudioEngine::Instance().IsVoiceDone(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	return AIL_3D_sample_status(m_sample) == SMP_DONE;
#endif
}

void CSoundInstance3D::Play(int iLoopCount, DWORD dwPlayCycleTimeLimit) const
{
	if (!m_pSoundData)
		return;

	DWORD dwCurTime = ELTimer_GetMSec();

	if (dwCurTime - m_pSoundData->GetPlayTime() < dwPlayCycleTimeLimit)
		return;

	m_pSoundData->SetPlayTime(dwCurTime);

#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().PlayVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), iLoopCount);
#else
	AIL_set_3D_sample_loop_count(m_sample, iLoopCount);
	AIL_start_3D_sample(m_sample);
#endif
}

void CSoundInstance3D::Pause() const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().PauseVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_stop_3D_sample(m_sample);
#endif
}

void CSoundInstance3D::Resume() const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().ResumeVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_resume_3D_sample(m_sample);
#endif
}

void CSoundInstance3D::Stop()
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().StopVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_end_3D_sample(m_sample);
#endif
//	m_sample = NULL;
}

void CSoundInstance3D::GetVolume(float& rfVolume) const
{
#if defined(MT2_USE_MINIAUDIO)
	rfVolume = mileslib_miniaudio::MiniaudioEngine::Instance().GetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	rfVolume = AIL_3D_sample_volume(m_sample);
#endif
}

void CSoundInstance3D::SetVolume(float volume) const
{
	volume = max(0.0f, min(1.0f, volume));
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), volume);
#else
	AIL_set_3D_sample_volume(m_sample, volume);
#endif
}

void CSoundInstance3D::SetPosition(float x, float y, float z) const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoicePosition(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), x, y, z);
#else
	AIL_set_3D_position(m_sample, x, y, -z);
#endif
}

void CSoundInstance3D::SetOrientation(float x_face, float y_face, float z_face,
									  float x_normal, float y_normal, float z_normal) const
{
	assert(!" CSoundInstance3D::SetOrientation - Don't use this function");
//	AIL_set_3D_orientation(m_sample,
//						   x_face, y_face, z_face,
//						   x_normal, y_normal, z_normal);
}

void CSoundInstance3D::SetVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float fNagnitude) const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoiceVelocity(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), fDistanceX, fDistanceY, fDistanceZ, fNagnitude);
#else
	AIL_set_3D_velocity(m_sample, fDistanceX, fDistanceY, fDistanceZ, fNagnitude);
	AIL_auto_update_3D_position(m_sample, 1);
#endif
}

void CSoundInstance3D::UpdatePosition(float fElapsedTime)
{
#if defined(MT2_USE_MINIAUDIO)
	(void)fElapsedTime;
#else
	AIL_update_3D_position(m_sample, fElapsedTime);
#endif
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
