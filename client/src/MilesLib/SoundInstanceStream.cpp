#include "stdafx.h"
#include "SoundInstance.h"
#if defined(MT2_USE_MINIAUDIO)
#include "MiniaudioEngine.h"
#endif

CSoundInstanceStream::CSoundInstanceStream() : m_stream(NULL)
{
}

CSoundInstanceStream::~CSoundInstanceStream()
{
	Destroy();
}

void CSoundInstanceStream::Destroy()
{
	if (m_stream != NULL)
	{
#if defined(MT2_USE_MINIAUDIO)
		mileslib_miniaudio::MiniaudioEngine::Instance().CloseStreamSlot(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#else
		AIL_close_stream(m_stream);
#endif
		m_stream = NULL;
	}
}

bool CSoundInstanceStream::Initialize()
{
	return true;
}

void CSoundInstanceStream::SetStream(HSTREAM stream)
{
#if defined(MT2_USE_MINIAUDIO)
	if (m_stream != NULL && m_stream != stream)
		mileslib_miniaudio::MiniaudioEngine::Instance().CloseStreamSlot(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#endif
	m_stream = stream;
}

bool CSoundInstanceStream::IsDone() const
{
#if defined(MT2_USE_MINIAUDIO)
	return mileslib_miniaudio::MiniaudioEngine::Instance().IsStreamDone(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#else
	return AIL_stream_status(m_stream) == -1;
#endif
}

bool CSoundInstanceStream::IsData() const
{
	if (m_stream)
		return true;

	return false;
}

void CSoundInstanceStream::Play(int count, DWORD dwPlayCycleTimeLimit) const
{
	if (!IsData())
		return;

#if defined(MT2_USE_MINIAUDIO)
	(void)dwPlayCycleTimeLimit;
	mileslib_miniaudio::MiniaudioEngine::Instance().PlayStream(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream), count);
#else
	AIL_set_stream_loop_count(m_stream, count);
	AIL_start_stream(m_stream);
#endif
}

void CSoundInstanceStream::Pause() const
{
	if (!IsData())
		return;

#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().PauseStream(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#else
	AIL_pause_stream(m_stream, 1);
#endif
}

void CSoundInstanceStream::Resume() const
{
	if (!IsData())
		return;

#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().ResumeStream(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#else
	AIL_pause_stream(m_stream, 0);
#endif
}

void CSoundInstanceStream::Stop()
{
	if (!IsData())
		return;

#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().CloseStreamSlot(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#else
	AIL_close_stream(m_stream);
#endif
	m_stream = NULL;
}

void CSoundInstanceStream::GetVolume(float& rfVolume) const
{
	float tmp;

	if (!IsData())
		return;

#if defined(MT2_USE_MINIAUDIO)
	(void)tmp;
	rfVolume = mileslib_miniaudio::MiniaudioEngine::Instance().GetStreamVolume(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
#else
	AIL_stream_volume_levels(m_stream, &rfVolume, &tmp);
#endif
}

void CSoundInstanceStream::SetVolume(float volume) const
{
	if (!IsData())
		return;

	volume = max(0.0f, min(1.0f, volume));
#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().SetStreamVolume(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream), volume);
#else
	AIL_set_stream_volume_levels(m_stream, volume, volume);
#endif
}

bool CSoundInstanceStream::SetSound(CSoundData* pSound)
{
	return true;
}

void CSoundInstanceStream::SetPosition(float x, float y, float z) const
{
}

void CSoundInstanceStream::SetOrientation(float x_face, float y_face, float z_face,
										  float x_normal, float y_normal, float z_normal) const
{
}

void CSoundInstanceStream::SetVelocity(float fx, float fy, float fz, float fMagnitude) const
{
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
