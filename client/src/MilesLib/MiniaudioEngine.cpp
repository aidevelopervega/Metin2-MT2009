#include "StdAfx.h"
#include "MiniaudioEngine.h"

#if defined(MT2_USE_MINIAUDIO)

#include "../EterPack/EterPackManager.h"
#include "SoundData.h"

#define MA_NO_JACK
#define MA_NO_PULSEAUDIO
#define MA_NO_ALSA
#define MA_NO_COREAUDIO
#define MA_NO_SNDIO
#define MA_NO_AUDIO4
#define MA_NO_OSS
#define MA_NO_AAUDIO
#define MA_NO_OPENSL
#define MA_NO_WEBAUDIO
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <algorithm>
#include <cstring>

namespace mileslib_miniaudio
{

namespace
{
std::recursive_mutex s_packAccessMutex;

struct EterPackVFSFile
{
	std::vector<uint8_t> data;
	size_t cursor = 0;
	bool fromPack = false;
};

struct EterPackVFS
{
	ma_vfs_callbacks cb;
	MiniaudioEngine* pOwner = nullptr;
};

static bool LoadFileViaEterPackOrDisk(const char* c_szFilePath, std::vector<uint8_t>& rOutBytes, bool& rbOutFromPack)
{
	rbOutFromPack = false;
	if (!c_szFilePath || !c_szFilePath[0])
		return false;

	std::lock_guard<std::recursive_mutex> lock(s_packAccessMutex);
	CMappedFile mappedFile;
	LPCVOID pvData = nullptr;

	if (CEterPackManager::Instance().GetFromPack(mappedFile, c_szFilePath, &pvData))
	{
		DWORD dwSize = mappedFile.Size();
		if (pvData && dwSize > 0)
		{
			const uint8_t* pBytes = static_cast<const uint8_t*>(pvData);
			rOutBytes.assign(pBytes, pBytes + dwSize);
			rbOutFromPack = true;
			mappedFile.Destroy();
			return true;
		}
		mappedFile.Destroy();
	}

	if (CEterPackManager::Instance().GetFromFile(mappedFile, c_szFilePath, &pvData))
	{
		DWORD dwSize = mappedFile.Size();
		if (pvData && dwSize > 0)
		{
			const uint8_t* pBytes = static_cast<const uint8_t*>(pvData);
			rOutBytes.assign(pBytes, pBytes + dwSize);
			rbOutFromPack = false;
			mappedFile.Destroy();
			return true;
		}
		mappedFile.Destroy();
	}

	return false;
}

static ma_result VfsOnOpen(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
	if (!pVFS || !pFilePath || !pFile)
		return MA_INVALID_ARGS;
	if ((openMode & MA_OPEN_MODE_WRITE) != 0)
		return MA_ACCESS_DENIED;

	std::vector<uint8_t> bytes;
	bool bFromPack = false;
	if (!LoadFileViaEterPackOrDisk(pFilePath, bytes, bFromPack))
		return MA_DOES_NOT_EXIST;

	EterPackVFS* pPackVfs = reinterpret_cast<EterPackVFS*>(pVFS);
	if (pPackVfs->pOwner)
		pPackVfs->pOwner->IncrementVfsOpenCount(bFromPack, static_cast<uint32_t>(bytes.size()));

	EterPackVFSFile* pVfsFile = new EterPackVFSFile();
	pVfsFile->data = std::move(bytes);
	pVfsFile->cursor = 0;
	pVfsFile->fromPack = bFromPack;

	*pFile = reinterpret_cast<ma_vfs_file>(pVfsFile);
	return MA_SUCCESS;
}

static ma_result VfsOnOpenW(ma_vfs* pVFS, const wchar_t* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
	if (!pVFS || !pFilePath || !pFile)
		return MA_INVALID_ARGS;

	char utf8Path[1024] = {};
	int converted = WideCharToMultiByte(CP_ACP, 0, pFilePath, -1, utf8Path, sizeof(utf8Path) - 1, nullptr, nullptr);
	if (converted <= 0)
		return MA_INVALID_ARGS;

	return VfsOnOpen(pVFS, utf8Path, openMode, pFile);
}

static ma_result VfsOnClose(ma_vfs* /*pVFS*/, ma_vfs_file file)
{
	if (!file)
		return MA_INVALID_ARGS;
	EterPackVFSFile* pVfsFile = reinterpret_cast<EterPackVFSFile*>(file);
	delete pVfsFile;
	return MA_SUCCESS;
}

static ma_result VfsOnRead(ma_vfs* /*pVFS*/, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
	if (pBytesRead)
		*pBytesRead = 0;
	if (!file || !pDst)
		return MA_INVALID_ARGS;

	EterPackVFSFile* pVfsFile = reinterpret_cast<EterPackVFSFile*>(file);
	if (pVfsFile->cursor >= pVfsFile->data.size())
		return MA_AT_END;

	size_t avail = pVfsFile->data.size() - pVfsFile->cursor;
	size_t toRead = (sizeInBytes < avail) ? sizeInBytes : avail;
	if (toRead > 0)
	{
		std::memcpy(pDst, pVfsFile->data.data() + pVfsFile->cursor, toRead);
		pVfsFile->cursor += toRead;
	}
	if (pBytesRead)
		*pBytesRead = toRead;

	return (toRead == 0) ? MA_AT_END : MA_SUCCESS;
}

static ma_result VfsOnWrite(ma_vfs* /*pVFS*/, ma_vfs_file /*file*/, const void* /*pSrc*/, size_t /*sizeInBytes*/, size_t* /*pBytesWritten*/)
{
	return MA_ACCESS_DENIED;
}

static ma_result VfsOnSeek(ma_vfs* /*pVFS*/, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
	if (!file)
		return MA_INVALID_ARGS;

	EterPackVFSFile* pVfsFile = reinterpret_cast<EterPackVFSFile*>(file);
	ma_int64 basePos = 0;
	if (origin == ma_seek_origin_start)
		basePos = 0;
	else if (origin == ma_seek_origin_current)
		basePos = static_cast<ma_int64>(pVfsFile->cursor);
	else if (origin == ma_seek_origin_end)
		basePos = static_cast<ma_int64>(pVfsFile->data.size());
	else
		return MA_INVALID_ARGS;

	ma_int64 target = basePos + offset;
	if (target < 0 || static_cast<size_t>(target) > pVfsFile->data.size())
		return MA_INVALID_ARGS;

	pVfsFile->cursor = static_cast<size_t>(target);
	return MA_SUCCESS;
}

static ma_result VfsOnTell(ma_vfs* /*pVFS*/, ma_vfs_file file, ma_int64* pCursor)
{
	if (!file || !pCursor)
		return MA_INVALID_ARGS;
	EterPackVFSFile* pVfsFile = reinterpret_cast<EterPackVFSFile*>(file);
	*pCursor = static_cast<ma_int64>(pVfsFile->cursor);
	return MA_SUCCESS;
}

static ma_result VfsOnInfo(ma_vfs* /*pVFS*/, ma_vfs_file file, ma_file_info* pInfo)
{
	if (!file || !pInfo)
		return MA_INVALID_ARGS;
	EterPackVFSFile* pVfsFile = reinterpret_cast<EterPackVFSFile*>(file);
	std::memset(pInfo, 0, sizeof(*pInfo));
	pInfo->sizeInBytes = pVfsFile->data.size();
	return MA_SUCCESS;
}

} // anonymous namespace

struct VoiceSlot
{
	bool bIs3D = false;
	bool bHasDecoder = false;
	bool bHasSound = false;
	bool bPaused = false;
	float fVolume = 1.0f;
	float fMinDist = 500.0f;
	float fMaxDist = 5000.0f;
	float fPosX = 0.0f;
	float fPosY = 0.0f;
	float fPosZ = 0.0f;
	std::vector<uint8_t> ownedAudioBytes;
	ma_decoder decoder = {};
	ma_sound sound = {};
};

struct StreamSlot
{
	bool bHasDecoder = false;
	bool bHasSound = false;
	bool bPaused = false;
	float fVolume = 1.0f;
	std::string fileName;
	ma_decoder decoder = {};
	ma_sound sound = {};
};

struct MiniaudioEngine::EngineImpl
{
	EterPackVFS vfs = {};
	ma_engine engine = {};
	bool engineInitialized = false;
	float listenerX = 0.0f;
	float listenerY = 0.0f;
	float listenerZ = 0.0f;
};

MiniaudioEngine& MiniaudioEngine::Instance()
{
	static MiniaudioEngine s_instance;
	return s_instance;
}

MiniaudioEngine::MiniaudioEngine()
	: m_pImpl(new EngineImpl())
	, m_bInitialized(false)
	, m_bUsingHardwareDevice(false)
	, m_bForceHeadless(false)
	, m_uPackDataReads(0)
	, m_uDiskDataReads(0)
	, m_uVfsPackOpens(0)
	, m_uVfsDiskOpens(0)
	, m_uTotalPackBytesRead(0)
	, m_uDeviceLossNotifications(0)
{
}

MiniaudioEngine::~MiniaudioEngine()
{
	Destroy();
	delete m_pImpl;
	m_pImpl = nullptr;
}

void MiniaudioEngine::Lock()
{
	m_mutex.lock();
}

void MiniaudioEngine::Unlock()
{
	m_mutex.unlock();
}

bool MiniaudioEngine::Initialize()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_bInitialized.load(std::memory_order_acquire))
		return true;

	std::memset(&m_pImpl->vfs, 0, sizeof(m_pImpl->vfs));
	m_pImpl->vfs.pOwner = this;
	m_pImpl->vfs.cb.onOpen = VfsOnOpen;
	m_pImpl->vfs.cb.onOpenW = VfsOnOpenW;
	m_pImpl->vfs.cb.onClose = VfsOnClose;
	m_pImpl->vfs.cb.onRead = VfsOnRead;
	m_pImpl->vfs.cb.onWrite = VfsOnWrite;
	m_pImpl->vfs.cb.onSeek = VfsOnSeek;
	m_pImpl->vfs.cb.onTell = VfsOnTell;
	m_pImpl->vfs.cb.onInfo = VfsOnInfo;

	if (!m_bForceHeadless)
	{
		ma_engine_config hwConfig = ma_engine_config_init();
		hwConfig.listenerCount = 1;

		if (ma_engine_init(&hwConfig, &m_pImpl->engine) == MA_SUCCESS)
		{
			m_pImpl->engineInitialized = true;
			m_bUsingHardwareDevice.store(true, std::memory_order_release);
			m_bInitialized.store(true, std::memory_order_release);
			return true;
		}
	}

	// Headless / noDevice mode fallback (when no physical audio device exists or forced headless in test)
	ma_engine_config headlessConfig = ma_engine_config_init();
	headlessConfig.listenerCount = 1;
	headlessConfig.noDevice = MA_TRUE;
	headlessConfig.channels = 2;
	headlessConfig.sampleRate = 48000;

	if (ma_engine_init(&headlessConfig, &m_pImpl->engine) != MA_SUCCESS)
		return false;

	m_pImpl->engineInitialized = true;
	m_bUsingHardwareDevice.store(false, std::memory_order_release);
	m_bInitialized.store(true, std::memory_order_release);
	return true;
}

void MiniaudioEngine::Destroy()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_bInitialized.load(std::memory_order_acquire))
		return;

	if (m_pImpl && m_pImpl->engineInitialized)
	{
		ma_engine_uninit(&m_pImpl->engine);
		m_pImpl->engineInitialized = false;
	}

	m_bUsingHardwareDevice.store(false, std::memory_order_release);
	m_bInitialized.store(false, std::memory_order_release);
}

void MiniaudioEngine::SetListenerPosition(float fX, float fY, float fZ)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
		return;

	m_pImpl->listenerX = fX;
	m_pImpl->listenerY = fY;
	m_pImpl->listenerZ = fZ;
	// Metin2 world coordinates (+X right, +Y forward, +Z up) are right-handed, matching miniaudio
	ma_engine_listener_set_position(&m_pImpl->engine, 0, fX, fY, fZ);
}

void MiniaudioEngine::GetListenerPosition(float& rfX, float& rfY, float& rfZ) const
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
	{
		rfX = rfY = rfZ = 0.0f;
		return;
	}
	rfX = m_pImpl->listenerX;
	rfY = m_pImpl->listenerY;
	rfZ = m_pImpl->listenerZ;
}

void MiniaudioEngine::SetListenerDirection(float fxDir, float fyDir, float fzDir, float fxUp, float fyUp, float fzUp)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
		return;

	ma_engine_listener_set_direction(&m_pImpl->engine, 0, fxDir, fyDir, fzDir);
	ma_engine_listener_set_world_up(&m_pImpl->engine, 0, fxUp, fyUp, fzUp);
}

void MiniaudioEngine::SetListenerVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float /*fMagnitude*/)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
		return;

	ma_engine_listener_set_velocity(&m_pImpl->engine, 0, fDistanceX, fDistanceY, fDistanceZ);
}

uint32_t MiniaudioEngine::ComputeBufferDurationMs(const void* pvData, uint32_t dwSize)
{
	if (!pvData || dwSize == 0)
		return 0;

	ma_decoder decoder = {};
	ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 2, 48000);
	if (ma_decoder_init_memory(pvData, dwSize, &cfg, &decoder) != MA_SUCCESS)
		return 0;

	ma_uint64 totalFrames = 0;
	ma_result res = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
	ma_uint32 sampleRate = decoder.outputSampleRate;
	ma_decoder_uninit(&decoder);

	if (res != MA_SUCCESS || sampleRate == 0 || totalFrames == 0)
		return 0;

	uint64_t ms = (totalFrames * 1000ULL) / sampleRate;
	return static_cast<uint32_t>(ms);
}

VoiceSlot* MiniaudioEngine::AllocateVoiceSlot(bool bIs3D)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_bInitialized.load(std::memory_order_acquire))
		return nullptr;

	VoiceSlot* pSlot = new VoiceSlot();
	pSlot->bIs3D = bIs3D;
	return pSlot;
}

void MiniaudioEngine::UnbindVoiceSoundData(VoiceSlot* pSlot)
{
	if (!pSlot)
		return;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	// Always uninit ma_sound and ma_decoder BEFORE releasing ownedAudioBytes
	if (pSlot->bHasSound)
	{
		ma_sound_stop(&pSlot->sound);
		ma_sound_uninit(&pSlot->sound);
		pSlot->bHasSound = false;
	}
	if (pSlot->bHasDecoder)
	{
		ma_decoder_uninit(&pSlot->decoder);
		pSlot->bHasDecoder = false;
	}
	pSlot->ownedAudioBytes.clear();
	pSlot->bPaused = false;
}

void MiniaudioEngine::FreeVoiceSlot(VoiceSlot* pSlot)
{
	if (!pSlot)
		return;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	UnbindVoiceSoundData(pSlot);
	delete pSlot;
}

bool MiniaudioEngine::BindVoiceSoundData(VoiceSlot* pSlot, const void* pvData, uint32_t dwSize, const char* /*c_szFileName*/)
{
	if (!pSlot || !pvData || dwSize == 0)
		return false;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
		return false;

	UnbindVoiceSoundData(pSlot);

	const uint8_t* pSrc = static_cast<const uint8_t*>(pvData);
	pSlot->ownedAudioBytes.assign(pSrc, pSrc + dwSize);

	ma_decoder_config decCfg = ma_decoder_config_init(
		ma_format_f32,
		pSlot->bIs3D ? 1 : 2,
		ma_engine_get_sample_rate(&m_pImpl->engine));

	if (ma_decoder_init_memory(pSlot->ownedAudioBytes.data(), pSlot->ownedAudioBytes.size(), &decCfg, &pSlot->decoder) != MA_SUCCESS)
	{
		pSlot->ownedAudioBytes.clear();
		return false;
	}
	pSlot->bHasDecoder = true;

	ma_uint32 soundFlags = pSlot->bIs3D ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
	if (ma_sound_init_from_data_source(&m_pImpl->engine, &pSlot->decoder, soundFlags, nullptr, &pSlot->sound) != MA_SUCCESS)
	{
		ma_decoder_uninit(&pSlot->decoder);
		pSlot->bHasDecoder = false;
		pSlot->ownedAudioBytes.clear();
		return false;
	}
	pSlot->bHasSound = true;
	pSlot->fVolume = 1.0f;
	ma_sound_set_volume(&pSlot->sound, 1.0f);

	if (pSlot->bIs3D)
	{
		ma_sound_set_attenuation_model(&pSlot->sound, ma_attenuation_model_inverse);
		ma_sound_set_min_distance(&pSlot->sound, pSlot->fMinDist);
		ma_sound_set_max_distance(&pSlot->sound, pSlot->fMaxDist);
		ma_sound_set_position(&pSlot->sound, pSlot->fPosX, pSlot->fPosY, pSlot->fPosZ);
	}

	return true;
}

void MiniaudioEngine::PlayVoice(VoiceSlot* pSlot, int iLoopCount)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;

	// Miles convention: iLoopCount == 0 -> infinite loop, iLoopCount == 1 -> play once, >1 -> loop
	ma_bool32 bLoop = (iLoopCount == 0 || iLoopCount > 1) ? MA_TRUE : MA_FALSE;
	ma_sound_set_looping(&pSlot->sound, bLoop);
	ma_sound_seek_to_pcm_frame(&pSlot->sound, 0);
	ma_sound_start(&pSlot->sound);
	pSlot->bPaused = false;
}

void MiniaudioEngine::PauseVoice(VoiceSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;
	ma_sound_stop(&pSlot->sound);
	pSlot->bPaused = true;
}

void MiniaudioEngine::ResumeVoice(VoiceSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;
	ma_sound_start(&pSlot->sound);
	pSlot->bPaused = false;
}

void MiniaudioEngine::StopVoice(VoiceSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;
	ma_sound_stop(&pSlot->sound);
	ma_sound_seek_to_pcm_frame(&pSlot->sound, 0);
	pSlot->bPaused = false;
}

bool MiniaudioEngine::IsVoiceDone(VoiceSlot* pSlot)
{
	if (!pSlot)
		return true;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return true;
	if (pSlot->bPaused)
		return false;
	return !ma_sound_is_playing(&pSlot->sound) || ma_sound_at_end(&pSlot->sound);
}

float MiniaudioEngine::GetVoiceVolume(VoiceSlot* pSlot) const
{
	if (!pSlot)
		return 0.0f;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return pSlot->fVolume;
}

void MiniaudioEngine::SetVoiceVolume(VoiceSlot* pSlot, float fVolume)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	fVolume = (std::max)(0.0f, (std::min)(1.0f, fVolume));
	pSlot->fVolume = fVolume;
	if (pSlot->bHasSound)
		ma_sound_set_volume(&pSlot->sound, fVolume);
}

void MiniaudioEngine::SetVoicePosition(VoiceSlot* pSlot, float fX, float fY, float fZ)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	pSlot->fPosX = fX;
	pSlot->fPosY = fY;
	pSlot->fPosZ = fZ;
	if (pSlot->bHasSound && pSlot->bIs3D)
		ma_sound_set_position(&pSlot->sound, fX, fY, fZ);
}

void MiniaudioEngine::GetVoicePosition(VoiceSlot* pSlot, float& rfX, float& rfY, float& rfZ) const
{
	if (!pSlot)
	{
		rfX = rfY = rfZ = 0.0f;
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	rfX = pSlot->fPosX;
	rfY = pSlot->fPosY;
	rfZ = pSlot->fPosZ;
}

void MiniaudioEngine::SetVoiceOrientation(VoiceSlot* pSlot, float xFace, float yFace, float zFace, float /*xUp*/, float /*yUp*/, float /*zUp*/)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (pSlot->bHasSound && pSlot->bIs3D)
		ma_sound_set_direction(&pSlot->sound, xFace, yFace, zFace);
}

void MiniaudioEngine::SetVoiceVelocity(VoiceSlot* pSlot, float fDistX, float fDistY, float fDistZ, float /*fMagnitude*/)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (pSlot->bHasSound && pSlot->bIs3D)
		ma_sound_set_velocity(&pSlot->sound, fDistX, fDistY, fDistZ);
}

void MiniaudioEngine::SetVoiceRange(VoiceSlot* pSlot, float fMinDist, float fMaxDist)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	pSlot->fMinDist = fMinDist;
	pSlot->fMaxDist = fMaxDist;
	if (pSlot->bHasSound && pSlot->bIs3D)
	{
		ma_sound_set_min_distance(&pSlot->sound, fMinDist);
		ma_sound_set_max_distance(&pSlot->sound, fMaxDist);
	}
}

StreamSlot* MiniaudioEngine::OpenStreamSlot(const char* c_szFileName)
{
	if (!c_szFileName || !c_szFileName[0])
		return nullptr;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
		return nullptr;

	StreamSlot* pSlot = new StreamSlot();
	pSlot->fileName = c_szFileName;

	ma_decoder_config decCfg = ma_decoder_config_init(
		ma_format_f32,
		2,
		ma_engine_get_sample_rate(&m_pImpl->engine));

	if (ma_decoder_init_vfs(&m_pImpl->vfs.cb, c_szFileName, &decCfg, &pSlot->decoder) != MA_SUCCESS)
	{
		delete pSlot;
		return nullptr;
	}
	pSlot->bHasDecoder = true;

	ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
	if (ma_sound_init_from_data_source(&m_pImpl->engine, &pSlot->decoder, flags, nullptr, &pSlot->sound) != MA_SUCCESS)
	{
		ma_decoder_uninit(&pSlot->decoder);
		delete pSlot;
		return nullptr;
	}

	pSlot->bHasSound = true;
	pSlot->fVolume = 1.0f;
	return pSlot;
}

void MiniaudioEngine::CloseStreamSlot(StreamSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (pSlot->bHasSound)
	{
		ma_sound_stop(&pSlot->sound);
		ma_sound_uninit(&pSlot->sound);
		pSlot->bHasSound = false;
	}
	if (pSlot->bHasDecoder)
	{
		ma_decoder_uninit(&pSlot->decoder);
		pSlot->bHasDecoder = false;
	}
	delete pSlot;
}

void MiniaudioEngine::PlayStream(StreamSlot* pSlot, int iLoopCount)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;

	ma_bool32 bLoop = (iLoopCount == 0 || iLoopCount > 1) ? MA_TRUE : MA_FALSE;
	ma_sound_set_looping(&pSlot->sound, bLoop);
	ma_sound_seek_to_pcm_frame(&pSlot->sound, 0);
	ma_sound_start(&pSlot->sound);
	pSlot->bPaused = false;
}

void MiniaudioEngine::PauseStream(StreamSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;
	ma_sound_stop(&pSlot->sound);
	pSlot->bPaused = true;
}

void MiniaudioEngine::ResumeStream(StreamSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;
	ma_sound_start(&pSlot->sound);
	pSlot->bPaused = false;
}

void MiniaudioEngine::StopStream(StreamSlot* pSlot)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return;
	ma_sound_stop(&pSlot->sound);
	ma_sound_seek_to_pcm_frame(&pSlot->sound, 0);
	pSlot->bPaused = false;
}

bool MiniaudioEngine::IsStreamDone(StreamSlot* pSlot) const
{
	if (!pSlot)
		return true;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!pSlot->bHasSound)
		return true;
	if (pSlot->bPaused)
		return false;
	return !ma_sound_is_playing(&pSlot->sound) || ma_sound_at_end(&pSlot->sound);
}

float MiniaudioEngine::GetStreamVolume(StreamSlot* pSlot) const
{
	if (!pSlot)
		return 0.0f;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return pSlot->fVolume;
}

void MiniaudioEngine::SetStreamVolume(StreamSlot* pSlot, float fVolume)
{
	if (!pSlot)
		return;
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	fVolume = (std::max)(0.0f, (std::min)(1.0f, fVolume));
	pSlot->fVolume = fVolume;
	if (pSlot->bHasSound)
		ma_sound_set_volume(&pSlot->sound, fVolume);
}

uint64_t MiniaudioEngine::PumpFrames(uint32_t frameCount, float* pOutInterleavedStereoF32)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized || frameCount == 0)
		return 0;

	std::vector<float> scratch;
	float* pTarget = pOutInterleavedStereoF32;
	if (!pTarget)
	{
		scratch.resize(static_cast<size_t>(frameCount) * 2);
		pTarget = scratch.data();
	}

	ma_uint64 framesRead = 0;
	if (ma_engine_read_pcm_frames(&m_pImpl->engine, pTarget, frameCount, &framesRead) != MA_SUCCESS)
		return 0;

	return static_cast<uint64_t>(framesRead);
}

void MiniaudioEngine::RecordPackDataRead(bool bFromPack, uint32_t dwBytes)
{
	if (bFromPack)
	{
		m_uPackDataReads.fetch_add(1, std::memory_order_relaxed);
		m_uTotalPackBytesRead.fetch_add(dwBytes, std::memory_order_relaxed);
	}
	else
	{
		m_uDiskDataReads.fetch_add(1, std::memory_order_relaxed);
	}
}

void MiniaudioEngine::IncrementVfsOpenCount(bool bFromPack, uint32_t dwBytes)
{
	if (bFromPack)
	{
		m_uVfsPackOpens.fetch_add(1, std::memory_order_relaxed);
		m_uTotalPackBytesRead.fetch_add(dwBytes, std::memory_order_relaxed);
	}
	else
	{
		m_uVfsDiskOpens.fetch_add(1, std::memory_order_relaxed);
	}
}

void MiniaudioEngine::OnDeviceLossNotification()
{
	m_uDeviceLossNotifications.fetch_add(1, std::memory_order_relaxed);
}

bool MiniaudioEngine::SimulateDeviceLossAndRecover()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!m_pImpl || !m_pImpl->engineInitialized)
		return false;

	OnDeviceLossNotification();
	if (m_bUsingHardwareDevice.load(std::memory_order_acquire))
	{
		if (ma_engine_stop(&m_pImpl->engine) != MA_SUCCESS)
			return false;
		if (ma_engine_start(&m_pImpl->engine) != MA_SUCCESS)
			return false;
	}

	return PumpFrames(256, nullptr) == 256;
}

static void WasapiProbeDataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount)
{
	if (pOutput)
		std::memset(pOutput, 0, static_cast<size_t>(frameCount) * pDevice->playback.channels * sizeof(float));
	if (pDevice && pDevice->pUserData)
	{
		std::atomic<uint32_t>* pCounter = static_cast<std::atomic<uint32_t>*>(pDevice->pUserData);
		pCounter->fetch_add(frameCount, std::memory_order_relaxed);
	}
}

bool MiniaudioEngine::ProbeWasapiHardware(WasapiProbeResult& rOutResult)
{
	rOutResult = WasapiProbeResult{};

	ma_backend backends[] = { ma_backend_wasapi };
	ma_context context = {};
	ma_context_config ctxCfg = ma_context_config_init();
	if (ma_context_init(backends, 1, &ctxCfg, &context) != MA_SUCCESS)
		return false;

	rOutResult.contextInitialized = true;

	ma_device_info* pPlaybackInfos = nullptr;
	ma_uint32 playbackCount = 0;
	ma_device_info* pCaptureInfos = nullptr;
	ma_uint32 captureCount = 0;

	if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) == MA_SUCCESS)
	{
		rOutResult.playbackDeviceCount = playbackCount;
		for (ma_uint32 i = 0; i < playbackCount; ++i)
		{
			if (pPlaybackInfos[i].isDefault || i == 0)
			{
				strncpy_s(rOutResult.defaultDeviceName, sizeof(rOutResult.defaultDeviceName), pPlaybackInfos[i].name, _TRUNCATE);
				if (pPlaybackInfos[i].isDefault)
					break;
			}
		}

		if (playbackCount > 0)
		{
			std::atomic<uint32_t> frameCounter{0};
			ma_device_config devCfg = ma_device_config_init(ma_device_type_playback);
			devCfg.playback.format = ma_format_f32;
			devCfg.playback.channels = 2;
			devCfg.sampleRate = 48000;
			devCfg.dataCallback = WasapiProbeDataCallback;
			devCfg.pUserData = &frameCounter;

			ma_device device = {};
			if (ma_device_init(&context, &devCfg, &device) == MA_SUCCESS)
			{
				rOutResult.deviceInitSucceeded = true;
				if (ma_device_start(&device) == MA_SUCCESS)
				{
					rOutResult.deviceStartSucceeded = true;
					Sleep(30);
					rOutResult.mixedFrames = frameCounter.load(std::memory_order_relaxed);
					ma_device_stop(&device);
				}
				ma_device_uninit(&device);
			}
		}
	}

	ma_context_uninit(&context);
	return rOutResult.contextInitialized;
}

} // namespace mileslib_miniaudio

#endif // MT2_USE_MINIAUDIO
