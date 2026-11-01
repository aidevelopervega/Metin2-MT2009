#include "stdafx.h"
#include "SoundManager2D.h"
#if defined(MT2_USE_MINIAUDIO)
#include "MiniaudioEngine.h"
#endif

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
#if defined(MT2_USE_MINIAUDIO)
		mileslib_miniaudio::MiniaudioEngine::Instance().FreeVoiceSlot(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
		AIL_release_sample_handle(m_sample);
#endif
		m_sample = NULL;
	}
}

bool CSoundInstance2D::Initialize()
{
	if (m_sample)
		return true;

#if defined(MT2_USE_MINIAUDIO)
	m_sample = reinterpret_cast<HSAMPLE>(mileslib_miniaudio::MiniaudioEngine::Instance().AllocateVoiceSlot(false));
#else
	m_sample = AIL_allocate_sample_handle(ms_DIGDriver);
#endif
	return m_sample ? true : false;
}

bool CSoundInstance2D::SetSound(CSoundData * pSoundData)
{
	assert(m_sample != NULL && pSoundData != NULL);

	LPVOID lpData = pSoundData->Get();

#if defined(MT2_USE_MINIAUDIO)
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
#else
	AIL_init_sample(m_sample);

    if (AIL_set_sample_file(m_sample, lpData, pSoundData->GetSize()) == NULL)
	{
		if (m_pSoundData != NULL)
		{
			m_pSoundData->Release();
			m_pSoundData = NULL;
		}

		pSoundData->Release();
		TraceError("%s: %s", AIL_last_error(), pSoundData->GetFileName());
		return false;
	}

#endif
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
#if defined(MT2_USE_MINIAUDIO)
	return mileslib_miniaudio::MiniaudioEngine::Instance().IsVoiceDone(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	return AIL_sample_status(m_sample) == SMP_DONE;
#endif
}

void CSoundInstance2D::Play(int iLoopCount, DWORD dwPlayCycleTimeLimit) const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().PlayVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), iLoopCount);
#else
    AIL_set_sample_loop_count(m_sample, iLoopCount);
	AIL_start_sample(m_sample);
#endif
}

void CSoundInstance2D::Pause() const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().PauseVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_stop_sample(m_sample);
#endif
}

void CSoundInstance2D::Resume() const
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().ResumeVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_resume_sample(m_sample);
#endif
}

void CSoundInstance2D::Stop()
{
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().StopVoice(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_end_sample(m_sample);
	m_sample = NULL;
#endif
}

void CSoundInstance2D::GetVolume(float& rfVolume) const
{
#if defined(MT2_USE_MINIAUDIO)
	rfVolume = mileslib_miniaudio::MiniaudioEngine::Instance().GetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample));
#else
	AIL_sample_volume_pan(m_sample, &rfVolume, NULL);
#endif
}

void CSoundInstance2D::SetVolume(float volume) const
{
	volume = max(0.0f, min(1.0f, volume));
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().SetVoiceVolume(reinterpret_cast<mileslib_miniaudio::VoiceSlot*>(m_sample), volume);
#else
	AIL_set_sample_volume_pan(m_sample, volume, 0.5f);
#endif
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
