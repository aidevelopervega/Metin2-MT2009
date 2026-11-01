#include "StdAfx.h"
#include "SoundData.h"

#include "../EterPack/EterPackManager.h"
#include "../eterBase/Timer.h"
#if defined(MT2_USE_MINIAUDIO)
#include "MiniaudioEngine.h"
#include <cstdlib>
#include <cstring>
#endif

bool CSoundData::ms_isSoundFile[SOUND_FILE_MAX_NUM];
CMappedFile CSoundData::ms_SoundFile[SOUND_FILE_MAX_NUM];

const char * CSoundData::GetFileName()
{
	return m_filename;
}

void CSoundData::Assign(const char* filename)
{
	assert(m_assigned == false);

	strncpy(m_filename, filename, sizeof(m_filename)-1);
	m_assigned = true;
}

LPVOID CSoundData::Get()
{
	++m_iRefCount;
	m_dwAccessTime = ELTimer_GetMSec();

	if (!m_data)
		ReadFromDisk();

	if (m_flag & FLAG_DATA_SIZE)
		return ((S32 *) m_data + 1);
	else
		return (m_data);
}

ULONG CSoundData::GetSize()
{
	return m_size;
}

void CSoundData::Release()
{
	assert(m_iRefCount != 0);
	--m_iRefCount;
	m_dwAccessTime = ELTimer_GetMSec();
}

DWORD CSoundData::GetPlayTime()
{
	return m_dwPlayTime;
}

void CSoundData::SetPlayTime(DWORD dwPlayTime)
{
	m_dwPlayTime = dwPlayTime;
}

DWORD CSoundData::GetAccessTime()
{
	return m_dwAccessTime;
}

bool CSoundData::ReadFromDisk()
{
	assert(m_assigned == true);

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

#if defined(MT2_USE_MINIAUDIO)
	mileslib_miniaudio::MiniaudioEngine::Instance().Lock();
	CMappedFile roFile;
	LPCVOID pMap = NULL;
	bool bFromPack = false;
	if (CEterPackManager::Instance().GetFromPack(roFile, m_filename, &pMap))
	{
		bFromPack = true;
	}
	else if (!CEterPackManager::Instance().GetFromFile(roFile, m_filename, &pMap))
	{
		mileslib_miniaudio::MiniaudioEngine::Instance().Unlock();
		return false;
	}

	DWORD dwSize = roFile.Size();
	if (!pMap || dwSize == 0)
	{
		roFile.Destroy();
		mileslib_miniaudio::MiniaudioEngine::Instance().Unlock();
		return false;
	}

	m_data = std::malloc(dwSize);
	if (!m_data)
	{
		roFile.Destroy();
		mileslib_miniaudio::MiniaudioEngine::Instance().Unlock();
		return false;
	}

	std::memcpy(m_data, pMap, dwSize);
	m_size = dwSize;
	m_flag = 0;
	roFile.Destroy();

	mileslib_miniaudio::MiniaudioEngine::Instance().RecordPackDataRead(bFromPack, dwSize);

	mileslib_miniaudio::MiniaudioEngine::Instance().Unlock();
	return true;
#else
	U32* s = (U32 *) AIL_file_read(m_filename, FILE_READ_WITH_SIZE);

	if (s == NULL)
		return false;

	S32 type = AIL_file_type(s + 1, s[0]);
	AILSOUNDINFO info;

	switch (type)
	{
		case AILFILETYPE_PCM_WAV:
			{
				m_data = s;
				m_size = *((S32 *) s);
				m_flag |= FLAG_DATA_SIZE;
			}
			break;

		case AILFILETYPE_ADPCM_WAV:
			{
				AIL_WAV_info(s + 1, &info);
				AIL_decompress_ADPCM(&info, &m_data, &m_size);
				AIL_mem_free_lock(s);
			}
			break;

		case AILFILETYPE_MPEG_L3_AUDIO:
			{
				AIL_decompress_ASI(s + 1, *((S32 *) s), m_filename, &m_data, &m_size, 0);
				AIL_mem_free_lock(s);
			}
			break;

		default:
			assert(!"Unknown File Type");
			AIL_mem_free_lock(s);
			return false;
	}

	return true;
#endif
}

bool CSoundData::isSlotIndex(DWORD dwIndex)
{
	if (dwIndex >= SOUND_FILE_MAX_NUM)
		return false;

	if (!ms_isSoundFile[dwIndex])
		return false;

	return true;
}

int CSoundData::GetEmptySlotIndex()
{
	for (int i = 0; i < SOUND_FILE_MAX_NUM; ++i)
	{
		if (!ms_isSoundFile[i])
			return i;
	}

	return -1;
}

U32 AILCALLBACK CSoundData::open_callback(char const * filename, U32 *file_handle)
{
	int iIndex = GetEmptySlotIndex();

	if (-1 == iIndex)
		return 0;

	LPCVOID	pMap;

	if (!CEterPackManager::Instance().Get(ms_SoundFile[iIndex], filename, &pMap))
		return 0;

	ms_isSoundFile[iIndex] = true;

	*file_handle = iIndex;
	return 1;
}

void AILCALLBACK CSoundData::close_callback(U32 file_handle)
{
	if (!isSlotIndex(file_handle))
		return;

	ms_SoundFile[file_handle].Destroy();
	ms_isSoundFile[file_handle] = false;
}

S32 AILCALLBACK CSoundData::seek_callback(U32 file_handle, S32 offset, U32 type)
{
	if (!isSlotIndex(file_handle))
		return 0;

	return ms_SoundFile[file_handle].Seek(offset, type);
}

U32 AILCALLBACK CSoundData::read_callback(U32 file_handle, void * buffer, U32 bytes)
{
	if (!isSlotIndex(file_handle))
		return 0;

	DWORD dwRealSize = min(ms_SoundFile[file_handle].Size(), bytes);
	ms_SoundFile[file_handle].Read(buffer, dwRealSize);
	return dwRealSize;
}

void CSoundData::SetPackMode()
{
	// if sound data reads from pack file, the callbacks of the MSS should be changed.
#if !defined(MT2_USE_MINIAUDIO)
	AIL_set_file_callbacks(open_callback, close_callback, seek_callback, read_callback);
#endif

	for (int i = 0; i < SOUND_FILE_MAX_NUM; ++i)
		ms_isSoundFile[i] = false;
}

void CSoundData::Destroy()
{
	if (m_data)
	{
#if defined(MT2_USE_MINIAUDIO)
		std::free(m_data);
#else
		AIL_mem_free_lock(m_data);
#endif
		m_data = NULL;
	}
}

CSoundData::CSoundData() :
m_assigned(false),
m_iRefCount(0),
m_dwPlayTime(0),
m_dwAccessTime(ELTimer_GetMSec()),
m_size(0),
m_data(NULL),
m_flag(0)
{
}

CSoundData::~CSoundData()
{
	Destroy();
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
