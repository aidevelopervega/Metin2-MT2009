#include "stdafx.h"
#include "SoundInstance.h"
#include "MiniaudioEngine.h"

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
		mileslib_miniaudio::MiniaudioEngine::Instance().CloseStreamSlot(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
		m_stream = NULL;
	}
}

bool CSoundInstanceStream::Initialize()
{
	return true;
}

void CSoundInstanceStream::SetStream(HSTREAM stream)
{
	if (m_stream != NULL && m_stream != stream)
		mileslib_miniaudio::MiniaudioEngine::Instance().CloseStreamSlot(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
	m_stream = stream;
}

bool CSoundInstanceStream::IsDone() const
{
	return mileslib_miniaudio::MiniaudioEngine::Instance().IsStreamDone(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
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

	(void)dwPlayCycleTimeLimit;
	mileslib_miniaudio::MiniaudioEngine::Instance().PlayStream(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream), count);
}

void CSoundInstanceStream::Pause() const
{
	if (!IsData())
		return;

	mileslib_miniaudio::MiniaudioEngine::Instance().PauseStream(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
}

void CSoundInstanceStream::Resume() const
{
	if (!IsData())
		return;

	mileslib_miniaudio::MiniaudioEngine::Instance().ResumeStream(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
}

void CSoundInstanceStream::Stop()
{
	if (!IsData())
		return;

	mileslib_miniaudio::MiniaudioEngine::Instance().CloseStreamSlot(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
	m_stream = NULL;
}

void CSoundInstanceStream::GetVolume(float& rfVolume) const
{
	if (!IsData())
		return;

	rfVolume = mileslib_miniaudio::MiniaudioEngine::Instance().GetStreamVolume(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream));
}

void CSoundInstanceStream::SetVolume(float volume) const
{
	if (!IsData())
		return;

	volume = max(0.0f, min(1.0f, volume));
	mileslib_miniaudio::MiniaudioEngine::Instance().SetStreamVolume(reinterpret_cast<mileslib_miniaudio::StreamSlot*>(m_stream), volume);
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
