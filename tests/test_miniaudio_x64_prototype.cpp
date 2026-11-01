// ============================================================================
// MT2009 Native Windows x64 — Real miniaudio (v0.11.26) Prototype & MilesLib
// Behavioral Comparison Suite (tests/test_miniaudio_x64_prototype.cpp)
//
// IMPORTANT:
// - Uses the REAL official single-header miniaudio library:
//   extern/client/include/miniaudio/miniaudio.h (v0.11.26, commit 877ea4a7...)
// - Exercises real miniaudio C APIs:
//   * ma_version / ma_version_string
//   * ma_context_init / ma_context_get_devices / ma_context_uninit
//   * ma_vfs_callbacks (custom in-memory EterPack VFS)
//   * ma_decoder_init_memory / ma_decoder_init_vfs / ma_decoder_read_pcm_frames
//   * ma_resource_manager_init / ma_resource_manager_uninit
//   * ma_engine_init / ma_engine_read_pcm_frames / ma_engine_uninit
//   * ma_sound_init_from_file (2D, 3D spatialized, and streamed BGM)
//   * ma_sound_set_position / ma_sound_set_attenuation_model / ma_sound_set_looping
//   * ma_device_init / ma_device_start / ma_device_stop / ma_device_uninit (null backend)
// - Compares and preserves MilesLib (CSoundManager) volume, 3D scaling, culling,
//   cooldown, and 3-slot BGM crossfade state machine contracts without deleting
//   or replacing the existing client/src/MilesLib files.
// - Physical speaker hardware output is explicitly reported as UNVERIFIED
//   (verified via headless ma_engine_read_pcm_frames + ma_backend_null device).
// ============================================================================

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define MINIAUDIO_IMPLEMENTATION
#include "../extern/client/include/miniaudio/miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
	constexpr uint32_t kSampleRate = 44100;
	constexpr uint32_t kEngineChannels = 2;
	constexpr double kPi = 3.14159265358979323846;

	// Build an in-memory 16-bit PCM RIFF/WAVE file (simulating an EterPack asset buffer)
	std::vector<uint8_t> BuildWavPcm16(uint32_t sampleRate, uint16_t channels, uint32_t frameCount, double freqHzL, double freqHzR = 0.0, float amplitude = 0.5f)
	{
		if (freqHzR <= 0.0)
			freqHzR = freqHzL;

		const uint32_t dataBytes = frameCount * channels * sizeof(int16_t);
		const uint32_t riffChunkSize = 36 + dataBytes;
		const uint32_t byteRate = sampleRate * channels * sizeof(int16_t);
		const uint16_t blockAlign = channels * sizeof(int16_t);
		const uint16_t bitsPerSample = 16;
		const uint32_t fmtChunkSize = 16;
		const uint16_t audioFormat = 1; // PCM

		std::vector<uint8_t> wav(44 + dataBytes, 0);
		auto writeAt = [&](size_t off, const void* src, size_t len) {
			std::memcpy(wav.data() + off, src, len);
		};

		writeAt(0, "RIFF", 4);
		writeAt(4, &riffChunkSize, 4);
		writeAt(8, "WAVE", 4);
		writeAt(12, "fmt ", 4);
		writeAt(16, &fmtChunkSize, 4);
		writeAt(20, &audioFormat, 2);
		writeAt(22, &channels, 2);
		writeAt(24, &sampleRate, 4);
		writeAt(28, &byteRate, 4);
		writeAt(32, &blockAlign, 2);
		writeAt(34, &bitsPerSample, 2);
		writeAt(36, "data", 4);
		writeAt(40, &dataBytes, 4);

		int16_t* pcm = reinterpret_cast<int16_t*>(wav.data() + 44);
		for (uint32_t i = 0; i < frameCount; ++i)
		{
			const double t = static_cast<double>(i) / static_cast<double>(sampleRate);
			const double sL = std::sin(2.0 * kPi * freqHzL * t) * amplitude;
			pcm[i * channels + 0] = static_cast<int16_t>(std::clamp(sL * 32767.0, -32767.0, 32767.0));
			if (channels >= 2)
			{
				const double sR = std::sin(2.0 * kPi * freqHzR * t) * amplitude;
				pcm[i * channels + 1] = static_cast<int16_t>(std::clamp(sR * 32767.0, -32767.0, 32767.0));
			}
		}
		return wav;
	}

	// ========================================================================
	// Real miniaudio Custom VFS (`ma_vfs_callbacks`) modeling EterPack archives
	// ========================================================================
	struct PackMemoryVFS
	{
		ma_vfs_callbacks cb;
		std::map<std::string, std::vector<uint8_t>> files;
		uint64_t openCount = 0;
		uint64_t readBytesTotal = 0;

		struct FileHandle
		{
			const std::vector<uint8_t>* data = nullptr;
			size_t cursor = 0;
			PackMemoryVFS* owner = nullptr;
		};

		static ma_result OnOpen(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
		{
			if (!pVFS || !pFilePath || !pFile)
				return MA_INVALID_ARGS;
			if ((openMode & MA_OPEN_MODE_WRITE) != 0)
				return MA_ACCESS_DENIED;

			auto* self = static_cast<PackMemoryVFS*>(pVFS);
			std::string normalized(pFilePath);
			for (char& ch : normalized)
			{
				if (ch == '\\')
					ch = '/';
			}

			auto it = self->files.find(normalized);
			if (it == self->files.end())
				return MA_DOES_NOT_EXIST;

			auto* h = new FileHandle{ &it->second, 0, self };
			*pFile = static_cast<ma_vfs_file>(h);
			++self->openCount;
			return MA_SUCCESS;
		}

		static ma_result OnOpenW(ma_vfs* pVFS, const wchar_t* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
		{
			if (!pFilePath)
				return MA_INVALID_ARGS;
			std::string narrow;
			for (const wchar_t* p = pFilePath; *p != 0; ++p)
				narrow.push_back(static_cast<char>(*p & 0x7F));
			return OnOpen(pVFS, narrow.c_str(), openMode, pFile);
		}

		static ma_result OnClose(ma_vfs* /*pVFS*/, ma_vfs_file file)
		{
			if (!file)
				return MA_INVALID_ARGS;
			delete static_cast<FileHandle*>(file);
			return MA_SUCCESS;
		}

		static ma_result OnRead(ma_vfs* /*pVFS*/, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
		{
			if (pBytesRead)
				*pBytesRead = 0;
			if (!file || !pDst)
				return MA_INVALID_ARGS;

			auto* h = static_cast<FileHandle*>(file);
			const size_t avail = (h->cursor < h->data->size()) ? (h->data->size() - h->cursor) : 0;
			const size_t toCopy = (std::min)(sizeInBytes, avail);
			if (toCopy > 0)
			{
				std::memcpy(pDst, h->data->data() + h->cursor, toCopy);
				h->cursor += toCopy;
				h->owner->readBytesTotal += toCopy;
			}
			if (pBytesRead)
				*pBytesRead = toCopy;
			return (toCopy == 0 && sizeInBytes > 0) ? MA_AT_END : MA_SUCCESS;
		}

		static ma_result OnWrite(ma_vfs* /*pVFS*/, ma_vfs_file /*file*/, const void* /*pSrc*/, size_t /*sizeInBytes*/, size_t* /*pBytesWritten*/)
		{
			return MA_NOT_IMPLEMENTED;
		}

		static ma_result OnSeek(ma_vfs* /*pVFS*/, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
		{
			if (!file)
				return MA_INVALID_ARGS;
			auto* h = static_cast<FileHandle*>(file);
			int64_t target = 0;
			if (origin == ma_seek_origin_start)
				target = offset;
			else if (origin == ma_seek_origin_current)
				target = static_cast<int64_t>(h->cursor) + offset;
			else if (origin == ma_seek_origin_end)
				target = static_cast<int64_t>(h->data->size()) + offset;
			else
				return MA_INVALID_ARGS;

			if (target < 0 || static_cast<size_t>(target) > h->data->size())
				return MA_INVALID_ARGS;
			h->cursor = static_cast<size_t>(target);
			return MA_SUCCESS;
		}

		static ma_result OnTell(ma_vfs* /*pVFS*/, ma_vfs_file file, ma_int64* pCursor)
		{
			if (!file || !pCursor)
				return MA_INVALID_ARGS;
			*pCursor = static_cast<ma_int64>(static_cast<FileHandle*>(file)->cursor);
			return MA_SUCCESS;
		}

		static ma_result OnInfo(ma_vfs* /*pVFS*/, ma_vfs_file file, ma_file_info* pInfo)
		{
			if (!file || !pInfo)
				return MA_INVALID_ARGS;
			std::memset(pInfo, 0, sizeof(*pInfo));
			pInfo->sizeInBytes = static_cast<ma_uint64>(static_cast<FileHandle*>(file)->data->size());
			return MA_SUCCESS;
		}

		PackMemoryVFS()
		{
			std::memset(&cb, 0, sizeof(cb));
			cb.onOpen = OnOpen;
			cb.onOpenW = OnOpenW;
			cb.onClose = OnClose;
			cb.onRead = OnRead;
			cb.onWrite = OnWrite;
			cb.onSeek = OnSeek;
			cb.onTell = OnTell;
			cb.onInfo = OnInfo;
		}
	};

	// ========================================================================
	// Real miniaudio-backed MilesLib CSoundManager Adapter Prototype
	// Preserves exact MilesLib (client/src/MilesLib/SoundManager.{h,cpp}) contract
	// ========================================================================
	class CMilesMiniaudioAdapter
	{
	public:
		static constexpr int kSoundMaxCount = 32;
		static constexpr int kMusicMaxCount = 3; // CSoundManagerStream::MUSIC_INSTANCE_MAX_NUM

		enum EMusicState
		{
			MUSIC_STATE_OFF = 0,
			MUSIC_STATE_PLAY,
			MUSIC_STATE_FADE_IN,
			MUSIC_STATE_FADE_OUT,
			MUSIC_STATE_FADE_LIMIT_OUT,
		};

		struct SoundSlot
		{
			bool initialized = false;
			bool is3D = false;
			ma_sound sound{};
			float baseVolume = 1.0f;
			int loopCount = 1;
		};

		struct MusicSlot
		{
			bool initialized = false;
			EMusicState state = MUSIC_STATE_OFF;
			std::string filename;
			ma_sound sound{};
			float volume = 0.0f;
			float limitVolume = 0.0f;
			float fadeSpeed = 0.01f;
		};

		bool Create(PackMemoryVFS* pVFS)
		{
			if (m_initialized)
				Destroy();

			ma_resource_manager_config rmConfig = ma_resource_manager_config_init();
			rmConfig.pVFS = pVFS;
			rmConfig.decodedFormat = ma_format_f32;
			rmConfig.decodedChannels = 0;
			rmConfig.decodedSampleRate = kSampleRate;

			if (ma_resource_manager_init(&rmConfig, &m_resourceManager) != MA_SUCCESS)
				return false;
			m_rmInitialized = true;

			ma_engine_config engConfig = ma_engine_config_init();
			engConfig.pResourceManager = &m_resourceManager;
			engConfig.noDevice = MA_TRUE; // Deterministic PCM frame pull (or null-device callback)
			engConfig.channels = kEngineChannels;
			engConfig.sampleRate = kSampleRate;
			engConfig.listenerCount = 1;

			if (ma_engine_init(&engConfig, &m_engine) != MA_SUCCESS)
			{
				ma_resource_manager_uninit(&m_resourceManager);
				m_rmInitialized = false;
				return false;
			}

			// MilesLib listener stays at local origin because MilesLib transforms 3D positions
			// relative to listener (fx - m_fxPosition) / m_fSoundScale
			ma_engine_listener_set_position(&m_engine, 0, 0.0f, 0.0f, 0.0f);
			ma_engine_listener_set_direction(&m_engine, 0, 0.0f, 0.0f, -1.0f);
			ma_engine_listener_set_world_up(&m_engine, 0, 0.0f, 1.0f, 0.0f);

			m_initialized = true;
			return true;
		}

		void Destroy()
		{
			for (auto& slot : m_soundSlots)
			{
				if (slot.initialized)
				{
					ma_sound_uninit(&slot.sound);
					slot.initialized = false;
				}
			}
			for (auto& mslot : m_musicSlots)
			{
				if (mslot.initialized)
				{
					ma_sound_uninit(&mslot.sound);
					mslot.initialized = false;
					mslot.state = MUSIC_STATE_OFF;
					mslot.filename.clear();
				}
			}
			if (m_initialized)
			{
				ma_engine_uninit(&m_engine);
				m_initialized = false;
			}
			if (m_rmInitialized)
			{
				ma_resource_manager_uninit(&m_resourceManager);
				m_rmInitialized = false;
			}
			m_playSoundHistoryMap.clear();
		}

		~CMilesMiniaudioAdapter()
		{
			Destroy();
		}

		// MilesLib SoundManager.cpp:39-45 exact formula
		static float ConvertRatioVolumeToApplyVolume(float fVolumeRatio)
		{
			if (0.1f > fVolumeRatio)
				return fVolumeRatio;
			return static_cast<float>(std::pow(10.0f, (-1.0f + fVolumeRatio)));
		}

		// MilesLib SoundManager.cpp:47-51 exact formula
		static float ConvertGradeVolumeToApplyVolume(int nVolumeGrade)
		{
			return ConvertRatioVolumeToApplyVolume(0.1f * static_cast<float>(nVolumeGrade));
		}

		void SetSoundScale(float fScale) { m_fSoundScale = fScale; }
		float GetSoundScale() const { return m_fSoundScale; }
		void SetAmbienceSoundScale(float fScale) { m_fAmbienceSoundScale = fScale; }

		void SetSoundVolumeRatio(float fRatio)
		{
			m_fSoundVolume = ConvertRatioVolumeToApplyVolume(fRatio);
		}
		void SetSoundVolumeGrade(int iGrade)
		{
			m_fSoundVolume = ConvertGradeVolumeToApplyVolume(iGrade);
		}
		void SetMusicVolume(float fVolume)
		{
			m_fMusicVolume = fVolume;
			for (auto& mslot : m_musicSlots)
			{
				if (mslot.initialized && mslot.state == MUSIC_STATE_PLAY)
				{
					mslot.volume = m_fMusicVolume;
					ma_sound_set_volume(&mslot.sound, mslot.volume);
				}
			}
		}

		float GetSoundVolume() const { return m_fSoundVolume; }
		float GetMusicVolume() const { return m_fMusicVolume; }

		void SaveVolume()
		{
			if (m_isSoundDisable)
				return;
			m_fBackupMusicVolume = m_fMusicVolume;
			m_fBackupSoundVolume = m_fSoundVolume;
			m_fMusicVolume = 0.0f;
			m_fSoundVolume = 0.0f;
			m_isSoundDisable = true;
		}

		void RestoreVolume()
		{
			m_isSoundDisable = false;
			m_fMusicVolume = m_fBackupMusicVolume;
			m_fSoundVolume = m_fBackupSoundVolume;
		}

		void SetPosition(float fx, float fy, float fz)
		{
			m_fxPosition = fx;
			m_fyPosition = fy;
			m_fzPosition = fz;
		}

		void SetDirection(float fxDir, float fyDir, float fzDir, float fxUp, float fyUp, float fzUp)
		{
			if (!m_initialized)
				return;
			ma_engine_listener_set_direction(&m_engine, 0, fxDir, fyDir, -fzDir);
			ma_engine_listener_set_world_up(&m_engine, 0, fxUp, fyUp, -fzUp);
		}

		bool PlaySound2D(const char* c_szFileName, float fVolume = 1.0f)
		{
			if (!m_initialized || !c_szFileName || m_fSoundVolume <= 0.0f)
				return false;

			int idx = AllocateSoundSlot();
			if (idx < 0)
				return false;

			SoundSlot& slot = m_soundSlots[idx];
			const ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
			if (ma_sound_init_from_file(&m_engine, c_szFileName, flags, nullptr, nullptr, &slot.sound) != MA_SUCCESS)
				return false;

			slot.initialized = true;
			slot.is3D = false;
			slot.baseVolume = fVolume;
			ma_sound_set_volume(&slot.sound, m_fSoundVolume * fVolume);
			return ma_sound_start(&slot.sound) == MA_SUCCESS;
		}

		int PlaySound3D(float fx, float fy, float fz, const char* c_szFileName, int iLoopCount = 1, float fScale = 0.0f)
		{
			if (!m_initialized || !c_szFileName || m_fSoundVolume <= 0.0f)
				return -1;

			int idx = AllocateSoundSlot();
			if (idx < 0)
				return -1;

			SoundSlot& slot = m_soundSlots[idx];
			const ma_uint32 flags = MA_SOUND_FLAG_DECODE;
			if (ma_sound_init_from_file(&m_engine, c_szFileName, flags, nullptr, nullptr, &slot.sound) != MA_SUCCESS)
				return -1;

			slot.initialized = true;
			slot.is3D = true;
			slot.baseVolume = 1.0f;
			slot.loopCount = iLoopCount;

			const float scale = (fScale > 0.0f) ? fScale : m_fSoundScale;
			const float relX = (fx - m_fxPosition) / scale;
			const float relY = (fy - m_fyPosition) / scale;
			const float relZ = -((fz - m_fzPosition) / scale);

			ma_sound_set_spatialization_enabled(&slot.sound, MA_TRUE);
			ma_sound_set_attenuation_model(&slot.sound, ma_attenuation_model_inverse);
			ma_sound_set_min_distance(&slot.sound, 1.0f);
			ma_sound_set_max_distance(&slot.sound, 50.0f);
			ma_sound_set_position(&slot.sound, relX, relY, relZ);
			ma_sound_set_looping(&slot.sound, (iLoopCount == 0) ? MA_TRUE : MA_FALSE);
			ma_sound_set_volume(&slot.sound, m_fSoundVolume);

			if (ma_sound_start(&slot.sound) != MA_SUCCESS)
			{
				ma_sound_uninit(&slot.sound);
				slot.initialized = false;
				return -1;
			}
			return idx;
		}

		int PlayAmbienceSound3D(float fx, float fy, float fz, const char* c_szFileName, int iLoopCount = 1)
		{
			return PlaySound3D(fx, fy, fz, c_szFileName, iLoopCount, m_fAmbienceSoundScale);
		}

		// MilesLib SoundManager.cpp:277-302 exact 5000.0f 2D distance cull + 0.3s cooldown
		int PlayCharacterSound3D(float fx, float fy, float fz, const char* c_szFileName, float fCurTimeSec, bool bCheckFrequency = false)
		{
			if (0.0f == m_fSoundVolume)
				return -1;

			const float dx = fx - m_fxPosition;
			const float dy = fy - m_fyPosition;
			const float fDistanceSq = dx * dx + dy * dy;
			if (fDistanceSq > 25000000.0f) // 5000 * 5000
				return -1;

			if (bCheckFrequency)
			{
				auto it = m_playSoundHistoryMap.find(c_szFileName);
				if (it != m_playSoundHistoryMap.end())
				{
					if (fCurTimeSec - it->second < 0.3f)
						return -1;
				}
				m_playSoundHistoryMap[c_szFileName] = fCurTimeSec;
			}
			return PlaySound3D(fx, fy, fz, c_szFileName, 1);
		}

		void StopSound3D(int iIndex)
		{
			if (iIndex < 0 || iIndex >= kSoundMaxCount)
				return;
			SoundSlot& slot = m_soundSlots[iIndex];
			if (slot.initialized)
			{
				ma_sound_stop(&slot.sound);
				ma_sound_uninit(&slot.sound);
				slot.initialized = false;
			}
		}

		void StopAllSound3D()
		{
			for (int i = 0; i < kSoundMaxCount; ++i)
				StopSound3D(i);
		}

		// MilesLib BGM crossfade state machine (SoundManager.cpp:365-558)
		void FadeInMusic(const char* c_szFileName, float fVolumeSpeed = 0.01f)
		{
			if (!m_initialized || !c_szFileName)
				return;

			int idx = -1;
			for (int i = 0; i < kMusicMaxCount; ++i)
			{
				if (m_musicSlots[i].initialized && m_musicSlots[i].filename == c_szFileName)
				{
					m_musicSlots[i].state = MUSIC_STATE_FADE_IN;
					m_musicSlots[i].fadeSpeed = fVolumeSpeed;
					return;
				}
				if (!m_musicSlots[i].initialized && idx < 0)
					idx = i;
			}
			if (idx < 0)
				return;

			MusicSlot& mslot = m_musicSlots[idx];
			const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
			if (ma_sound_init_from_file(&m_engine, c_szFileName, flags, nullptr, nullptr, &mslot.sound) != MA_SUCCESS)
				return;

			mslot.initialized = true;
			mslot.filename = c_szFileName;
			mslot.state = MUSIC_STATE_FADE_IN;
			mslot.volume = 0.0f;
			mslot.fadeSpeed = fVolumeSpeed;
			ma_sound_set_looping(&mslot.sound, MA_TRUE);
			ma_sound_set_volume(&mslot.sound, 0.0f);
			ma_sound_start(&mslot.sound);
		}

		void FadeOutMusic(const char* c_szFileName, float fVolumeSpeed = 0.01f)
		{
			for (auto& mslot : m_musicSlots)
			{
				if (mslot.initialized && mslot.filename == c_szFileName)
				{
					mslot.state = MUSIC_STATE_FADE_OUT;
					mslot.fadeSpeed = fVolumeSpeed;
				}
			}
		}

		void FadeLimitOutMusic(const char* c_szFileName, float fLimitVolume, float fVolumeSpeed = 0.01f)
		{
			for (auto& mslot : m_musicSlots)
			{
				if (mslot.initialized && mslot.filename == c_szFileName)
				{
					mslot.state = MUSIC_STATE_FADE_LIMIT_OUT;
					mslot.limitVolume = fLimitVolume;
					mslot.fadeSpeed = fVolumeSpeed;
				}
			}
		}

		void FadeOutAllMusic()
		{
			for (auto& mslot : m_musicSlots)
			{
				if (mslot.initialized)
				{
					mslot.state = MUSIC_STATE_FADE_OUT;
					mslot.fadeSpeed = 0.01f;
				}
			}
		}

		void Update()
		{
			for (auto& slot : m_soundSlots)
			{
				if (slot.initialized && ma_sound_at_end(&slot.sound))
				{
					ma_sound_uninit(&slot.sound);
					slot.initialized = false;
				}
			}

			for (auto& mslot : m_musicSlots)
			{
				if (!mslot.initialized || mslot.state == MUSIC_STATE_OFF)
					continue;

				switch (mslot.state)
				{
				case MUSIC_STATE_FADE_IN:
					mslot.volume += mslot.fadeSpeed;
					if (mslot.volume >= m_fMusicVolume)
					{
						mslot.volume = m_fMusicVolume;
						mslot.state = MUSIC_STATE_PLAY;
					}
					ma_sound_set_volume(&mslot.sound, mslot.volume);
					break;

				case MUSIC_STATE_FADE_LIMIT_OUT:
					mslot.volume -= mslot.fadeSpeed;
					if (mslot.volume <= mslot.limitVolume)
					{
						mslot.volume = mslot.limitVolume;
						mslot.state = MUSIC_STATE_OFF;
					}
					ma_sound_set_volume(&mslot.sound, mslot.volume);
					break;

				case MUSIC_STATE_FADE_OUT:
					mslot.volume -= mslot.fadeSpeed;
					if (mslot.volume <= 0.0f)
					{
						mslot.volume = 0.0f;
						mslot.state = MUSIC_STATE_OFF;
						ma_sound_stop(&mslot.sound);
						ma_sound_uninit(&mslot.sound);
						mslot.initialized = false;
						mslot.filename.clear();
					}
					else
					{
						ma_sound_set_volume(&mslot.sound, mslot.volume);
					}
					break;

				default:
					break;
				}
			}
		}

		ma_result ReadPcmFrames(float* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead = nullptr)
		{
			return ma_engine_read_pcm_frames(&m_engine, pFramesOut, frameCount, pFramesRead);
		}

		const MusicSlot& GetMusicSlot(int idx) const { return m_musicSlots[idx]; }
		ma_engine* GetEngine() { return &m_engine; }

	private:
		int AllocateSoundSlot()
		{
			for (int i = 0; i < kSoundMaxCount; ++i)
			{
				if (!m_soundSlots[i].initialized)
					return i;
				if (ma_sound_at_end(&m_soundSlots[i].sound))
				{
					ma_sound_uninit(&m_soundSlots[i].sound);
					m_soundSlots[i].initialized = false;
					return i;
				}
			}
			return -1;
		}

		bool m_rmInitialized = false;
		bool m_initialized = false;
		bool m_isSoundDisable = false;
		ma_resource_manager m_resourceManager{};
		ma_engine m_engine{};
		SoundSlot m_soundSlots[kSoundMaxCount]{};
		MusicSlot m_musicSlots[kMusicMaxCount]{};
		std::map<std::string, float> m_playSoundHistoryMap;

		float m_fxPosition = 0.0f;
		float m_fyPosition = 0.0f;
		float m_fzPosition = 0.0f;
		float m_fSoundScale = 200.0f;
		float m_fAmbienceSoundScale = 1000.0f;
		float m_fSoundVolume = 1.0f;
		float m_fMusicVolume = 1.0f;
		float m_fBackupSoundVolume = 0.0f;
		float m_fBackupMusicVolume = 0.0f;
	};

	struct StereoRms
	{
		double leftRms = 0.0;
		double rightRms = 0.0;
		float peakAbs = 0.0f;
	};

	StereoRms ComputeStereoRms(const std::vector<float>& interleavedStereo)
	{
		StereoRms out{};
		const size_t frames = interleavedStereo.size() / 2;
		if (frames == 0)
			return out;

		double sumSqL = 0.0;
		double sumSqR = 0.0;
		for (size_t i = 0; i < frames; ++i)
		{
			const float l = interleavedStereo[i * 2 + 0];
			const float r = interleavedStereo[i * 2 + 1];
			sumSqL += static_cast<double>(l) * static_cast<double>(l);
			sumSqR += static_cast<double>(r) * static_cast<double>(r);
			out.peakAbs = (std::max)(out.peakAbs, (std::max)(std::fabs(l), std::fabs(r)));
		}
		out.leftRms = std::sqrt(sumSqL / static_cast<double>(frames));
		out.rightRms = std::sqrt(sumSqR / static_cast<double>(frames));
		return out;
	}

	struct NullDeviceCallbackState
	{
		ma_engine* pEngine = nullptr;
		std::atomic<uint64_t> callbackCount{ 0 };
		std::atomic<uint64_t> framesMixed{ 0 };
	};

	void NullDeviceDataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount)
	{
		auto* st = static_cast<NullDeviceCallbackState*>(pDevice->pUserData);
		if (!st || !st->pEngine || !pOutput)
			return;
		ma_uint64 framesRead = 0;
		ma_engine_read_pcm_frames(st->pEngine, pOutput, frameCount, &framesRead);
		st->callbackCount.fetch_add(1, std::memory_order_relaxed);
		st->framesMixed.fetch_add(framesRead, std::memory_order_relaxed);
	}
}

int main()
{
	static_assert(sizeof(void*) == 8, "Must be compiled as 64-bit PE32+ (AMD64)");

	int passed = 0;
	auto check = [&](bool cond, const char* msg) {
		if (!cond)
		{
			std::fprintf(stderr, "[FAIL] %s\n", msg);
			std::exit(1);
		}
		++passed;
	};

	// 1. Verify real miniaudio library version macros & runtime ma_version()
	ma_uint32 verMajor = 0, verMinor = 0, verRev = 0;
	ma_version(&verMajor, &verMinor, &verRev);
	check(verMajor == MA_VERSION_MAJOR && verMinor == MA_VERSION_MINOR && verRev == MA_VERSION_REVISION,
		"ma_version() matches compiled MA_VERSION_* macros");
	check(verMajor == 0 && verMinor == 11 && verRev == 26,
		"Real miniaudio version is pinned 0.11.26");
	check(std::strcmp(ma_version_string(), "0.11.26") == 0,
		"ma_version_string() == '0.11.26'");

	// 2. Probe WASAPI context & audio devices (strict check without unconditional check(true))
	ma_context probeCtx{};
	ma_uint32 playbackDeviceCount = 0;
	const char* activeBackendName = "none";
	bool ctxInitOk = false;
	bool getDevicesOk = false;
	bool wasapiDeviceInitOk = false;
	ma_backend wasapiBackends[] = { ma_backend_wasapi };
	if (ma_context_init(wasapiBackends, 1, nullptr, &probeCtx) == MA_SUCCESS)
	{
		ctxInitOk = true;
		activeBackendName = ma_get_backend_name(probeCtx.backend);
		ma_device_info* pPlaybackInfos = nullptr;
		ma_device_info* pCaptureInfos = nullptr;
		ma_uint32 captureCount = 0;
		if (ma_context_get_devices(&probeCtx, &pPlaybackInfos, &playbackDeviceCount, &pCaptureInfos, &captureCount) == MA_SUCCESS)
		{
			getDevicesOk = true;
			if (playbackDeviceCount > 0)
			{
				ma_device_config hwDevCfg = ma_device_config_init(ma_device_type_playback);
				hwDevCfg.playback.format = ma_format_f32;
				hwDevCfg.playback.channels = kEngineChannels;
				hwDevCfg.sampleRate = kSampleRate;
				ma_device hwDevice{};
				if (ma_device_init(&probeCtx, &hwDevCfg, &hwDevice) == MA_SUCCESS)
				{
					wasapiDeviceInitOk = true;
					ma_device_uninit(&hwDevice);
				}
			}
		}
		ma_context_uninit(&probeCtx);
	}
	check(ctxInitOk && getDevicesOk && (playbackDeviceCount == 0 || wasapiDeviceInitOk),
		"WASAPI ma_context_init, ma_context_get_devices, and (when hardware present) ma_device_init verified");

	// 3. Populate custom in-memory EterPack VFS with valid RIFF/WAVE assets
	PackMemoryVFS vfs;
	vfs.files["sound/ui/click.wav"] = BuildWavPcm16(kSampleRate, 1, kSampleRate / 2, 440.0);
	vfs.files["sound/monster/wolf/attack.wav"] = BuildWavPcm16(kSampleRate, 1, kSampleRate / 2, 660.0);
	vfs.files["sound/ambience/campfire.wav"] = BuildWavPcm16(kSampleRate, 1, kSampleRate / 4, 220.0);
	vfs.files["bgm/enter_the_east.wav"] = BuildWavPcm16(kSampleRate, 2, kSampleRate, 330.0, 495.0);
	vfs.files["bgm/xmas.wav"] = BuildWavPcm16(kSampleRate, 2, kSampleRate, 528.0, 792.0);

	// 4. Verify real ma_decoder_init_memory & ma_decoder_init_vfs
	{
		const auto& rawWav = vfs.files["sound/ui/click.wav"];
		ma_decoder_config decCfg = ma_decoder_config_init(ma_format_f32, 2, kSampleRate);
		ma_decoder memDecoder{};
		check(ma_decoder_init_memory(rawWav.data(), rawWav.size(), &decCfg, &memDecoder) == MA_SUCCESS,
			"ma_decoder_init_memory decodes RIFF/WAVE asset");
		ma_uint64 totalFrames = 0;
		check(ma_decoder_get_length_in_pcm_frames(&memDecoder, &totalFrames) == MA_SUCCESS && totalFrames == kSampleRate / 2,
			"ma_decoder_get_length_in_pcm_frames returns exact 22050 frames");
		std::vector<float> decodedBuf(1024 * 2, 0.0f);
		ma_uint64 framesRead = 0;
		check(ma_decoder_read_pcm_frames(&memDecoder, decodedBuf.data(), 1024, &framesRead) == MA_SUCCESS && framesRead == 1024,
			"ma_decoder_read_pcm_frames reads 1024 decoded f32 stereo frames");
		const StereoRms decRms = ComputeStereoRms(decodedBuf);
		check(decRms.leftRms > 0.1 && std::fabs(decRms.leftRms - decRms.rightRms) < 1e-5,
			"ma_decoder_read_pcm_frames produces non-zero balanced stereo PCM from mono WAV");
		ma_decoder_uninit(&memDecoder);

		ma_decoder vfsDecoder{};
		check(ma_decoder_init_vfs(&vfs, "sound\\ui\\click.wav", &decCfg, &vfsDecoder) == MA_SUCCESS,
			"ma_decoder_init_vfs opens backslash path via custom EterPack VFS");
		ma_decoder_uninit(&vfsDecoder);

		check(ma_decoder_init_vfs(&vfs, "sound/ui/nonexistent.wav", &decCfg, &vfsDecoder) == MA_DOES_NOT_EXIST,
			"ma_decoder_init_vfs returns MA_DOES_NOT_EXIST for missing asset");
	}

	// 5. Verify MilesLib volume conversion curve (SoundManager.cpp:39-51)
	check(std::fabs(CMilesMiniaudioAdapter::ConvertRatioVolumeToApplyVolume(0.0f) - 0.0f) < 1e-6f,
		"MilesLib volume ratio 0.0 -> 0.0");
	check(std::fabs(CMilesMiniaudioAdapter::ConvertRatioVolumeToApplyVolume(0.05f) - 0.05f) < 1e-6f,
		"MilesLib volume ratio < 0.1 stays linear");
	check(std::fabs(CMilesMiniaudioAdapter::ConvertRatioVolumeToApplyVolume(0.5f) - std::pow(10.0f, -0.5f)) < 1e-5f,
		"MilesLib volume ratio 0.5 -> 10^(-0.5) logarithmic curve");
	check(std::fabs(CMilesMiniaudioAdapter::ConvertGradeVolumeToApplyVolume(10) - 1.0f) < 1e-6f,
		"MilesLib volume grade 10 -> 1.0");

	// 6. Initialize real ma_engine & ma_resource_manager via CMilesMiniaudioAdapter
	CMilesMiniaudioAdapter mgr;
	check(mgr.Create(&vfs), "CMilesMiniaudioAdapter::Create initializes real ma_resource_manager + ma_engine");

	// 7. Verify SaveVolume / RestoreVolume idempotency (SoundManager.cpp:122-140)
	mgr.SetSoundVolumeRatio(1.0f);
	mgr.SetMusicVolume(0.8f);
	mgr.SaveVolume();
	check(mgr.GetSoundVolume() == 0.0f && mgr.GetMusicVolume() == 0.0f,
		"SaveVolume mutes sound and music volumes");
	mgr.SaveVolume(); // Second call while disabled must NOT overwrite backup with 0
	mgr.RestoreVolume();
	check(std::fabs(mgr.GetSoundVolume() - 1.0f) < 1e-6f && std::fabs(mgr.GetMusicVolume() - 0.8f) < 1e-6f,
		"RestoreVolume restores original volumes after repeated SaveVolume()");

	// 8. Verify 2D sound playback (PlaySound2D) via real ma_sound_init_from_file + ma_engine_read_pcm_frames
	check(mgr.PlaySound2D("sound/ui/click.wav", 1.0f),
		"PlaySound2D initializes and starts real 2D ma_sound via custom VFS");
	std::vector<float> mixBuf(4096 * 2, 0.0f);
	ma_uint64 framesRead = 0;
	check(mgr.ReadPcmFrames(mixBuf.data(), 4096, &framesRead) == MA_SUCCESS && framesRead == 4096,
		"ma_engine_read_pcm_frames renders 4096 frames of 2D sound");
	const StereoRms rms2D = ComputeStereoRms(mixBuf);
	check(rms2D.leftRms > 0.05 && std::fabs(rms2D.leftRms - rms2D.rightRms) < 1e-5,
		"PlaySound2D produces identical left/right stereo RMS energy");
	mgr.StopAllSound3D();

	// 9. Verify 3D spatialization (right vs left panning & distance attenuation) with real ma_sound 3D spatializer
	mgr.Destroy();
	check(mgr.Create(&vfs), "Re-create clean ma_engine for 3D spatialization test");
	mgr.SetPosition(1000.0f, 2000.0f, 0.0f);
	mgr.SetSoundScale(200.0f);

	// Sound placed to the RIGHT of listener (+X = +600 world units -> +3.0 scaled units)
	int slotRight = mgr.PlaySound3D(1600.0f, 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 1);
	check(slotRight >= 0, "PlaySound3D(+X right of listener) succeeds");
	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mgr.ReadPcmFrames(mixBuf.data(), 4096, &framesRead);
	const StereoRms rmsRight = ComputeStereoRms(mixBuf);
	mgr.StopSound3D(slotRight);

	// Sound placed to the LEFT of listener (-X = -600 world units -> -3.0 scaled units)
	int slotLeft = mgr.PlaySound3D(400.0f, 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 1);
	check(slotLeft >= 0, "PlaySound3D(-X left of listener) succeeds");
	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mgr.ReadPcmFrames(mixBuf.data(), 4096, &framesRead);
	const StereoRms rmsLeft = ComputeStereoRms(mixBuf);
	mgr.StopSound3D(slotLeft);

	check(rmsRight.rightRms > rmsRight.leftRms * 1.25 && rmsLeft.leftRms > rmsLeft.rightRms * 1.25,
		"Real ma_engine 3D spatializer pans +X sound to right channel and -X sound to left channel");

	// Near vs Far 3D attenuation test (along -Z forward axis: world fz = +200 -> relZ = -1.0 vs world fz = +4000 -> relZ = -20.0)
	int slotNear = mgr.PlaySound3D(1000.0f, 2000.0f, 200.0f, "sound/monster/wolf/attack.wav", 1);
	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mgr.ReadPcmFrames(mixBuf.data(), 4096, &framesRead);
	const StereoRms rmsNear = ComputeStereoRms(mixBuf);
	mgr.StopSound3D(slotNear);

	int slotFar = mgr.PlaySound3D(1000.0f, 2000.0f, 4000.0f, "sound/monster/wolf/attack.wav", 1);
	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mgr.ReadPcmFrames(mixBuf.data(), 4096, &framesRead);
	const StereoRms rmsFar = ComputeStereoRms(mixBuf);
	mgr.StopSound3D(slotFar);

	check(rmsNear.leftRms > rmsFar.leftRms * 3.0,
		"Real ma_engine inverse distance attenuation reduces far 3D sound energy > 3x vs near sound");

	// 10. Verify PlayCharacterSound3D 5000.0f 2D distance culling & 0.3s cooldown (SoundManager.cpp:277-302)
	check(mgr.PlayCharacterSound3D(6001.0f, 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 10.0f, true) == -1,
		"PlayCharacterSound3D culls sound beyond 5000.0f 2D distance");
	int charSlot1 = mgr.PlayCharacterSound3D(1200.0f, 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 10.0f, true);
	check(charSlot1 >= 0, "PlayCharacterSound3D plays within 5000.0f when cooldown expired");
	check(mgr.PlayCharacterSound3D(1200.0f, 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 10.15f, true) == -1,
		"PlayCharacterSound3D suppresses duplicate sound within 0.3s cooldown window");
	int charSlot2 = mgr.PlayCharacterSound3D(1200.0f, 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 10.35f, true);
	check(charSlot2 >= 0, "PlayCharacterSound3D allows sound after 0.3s cooldown window");
	mgr.StopAllSound3D();

	// 11. Verify BGM streaming & 3-slot crossfade state machine (FadeInMusic / FadeLimitOutMusic / FadeOutMusic)
	mgr.SetMusicVolume(0.6f);
	mgr.FadeInMusic("bgm/enter_the_east.wav", 0.2f);
	check(mgr.GetMusicSlot(0).initialized && mgr.GetMusicSlot(0).state == CMilesMiniaudioAdapter::MUSIC_STATE_FADE_IN,
		"FadeInMusic initializes BGM slot 0 in MUSIC_STATE_FADE_IN");
	for (int tick = 0; tick < 5; ++tick)
		mgr.Update();
	check(mgr.GetMusicSlot(0).state == CMilesMiniaudioAdapter::MUSIC_STATE_PLAY &&
		std::fabs(mgr.GetMusicSlot(0).volume - 0.6f) < 1e-5f,
		"FadeInMusic reaches MUSIC_STATE_PLAY at target m_fMusicVolume (0.6)");

	// Crossfade to second BGM while limiting first BGM
	mgr.FadeLimitOutMusic("bgm/enter_the_east.wav", 0.2f, 0.2f);
	mgr.FadeInMusic("bgm/xmas.wav", 0.2f);
	for (int tick = 0; tick < 5; ++tick)
		mgr.Update();
	check(mgr.GetMusicSlot(0).state == CMilesMiniaudioAdapter::MUSIC_STATE_OFF &&
		std::fabs(mgr.GetMusicSlot(0).volume - 0.2f) < 1e-5f &&
		mgr.GetMusicSlot(1).state == CMilesMiniaudioAdapter::MUSIC_STATE_PLAY,
		"Crossfade transitions BGM 0 to limit volume (0.2) and BGM 1 to MUSIC_STATE_PLAY");

	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mgr.ReadPcmFrames(mixBuf.data(), 4096, &framesRead);
	const StereoRms rmsBgm = ComputeStereoRms(mixBuf);
	check(rmsBgm.leftRms > 0.05 && rmsBgm.rightRms > 0.05,
		"Streamed BGM tracks render non-zero stereo PCM frames through ma_engine");

	mgr.FadeOutAllMusic();
	for (int tick = 0; tick < 65; ++tick)
		mgr.Update();
	check(!mgr.GetMusicSlot(0).initialized && !mgr.GetMusicSlot(1).initialized,
		"FadeOutAllMusic stops and uninitializes all BGM streams cleanly");

	// 12. Benchmark 35 concurrent real miniaudio voices (32 3D sounds + 3 BGM streams) over 1.0s (44,100 stereo frames)
	mgr.SetMusicVolume(0.5f);
	mgr.FadeInMusic("bgm/enter_the_east.wav", 0.5f);
	mgr.FadeInMusic("bgm/xmas.wav", 0.5f);
	mgr.PlayAmbienceSound3D(1100.0f, 2100.0f, 0.0f, "sound/ambience/campfire.wav", 0);
	for (int i = 0; i < 31; ++i)
	{
		mgr.PlaySound3D(1000.0f + static_cast<float>(i * 25), 2000.0f, 0.0f, "sound/monster/wolf/attack.wav", 0);
	}
	mgr.Update();

	std::vector<float> oneSecStereo(kSampleRate * 2, 0.0f);
	const auto t0 = std::chrono::steady_clock::now();
	check(mgr.ReadPcmFrames(oneSecStereo.data(), kSampleRate, &framesRead) == MA_SUCCESS && framesRead == kSampleRate,
		"ma_engine mixes 44,100 frames (1.0s) across 34 concurrent voices");
	const auto t1 = std::chrono::steady_clock::now();
	const double mixMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
	const StereoRms StressRms = ComputeStereoRms(oneSecStereo);
	check(StressRms.leftRms > 0.05 && StressRms.rightRms > 0.05 && mixMs < 250.0,
		"Real miniaudio 34-voice 1.0s mix completes well within real-time budget (< 250 ms)");

	// 13. Verify real ma_device asynchronous audio thread callback using ma_backend_null
	{
		ma_context nullCtx{};
		ma_backend nullBackends[] = { ma_backend_null };
		check(ma_context_init(nullBackends, 1, nullptr, &nullCtx) == MA_SUCCESS,
			"ma_context_init(ma_backend_null) succeeds for headless loopback device");

		NullDeviceCallbackState cbState{};
		cbState.pEngine = mgr.GetEngine();

		ma_device_config devCfg = ma_device_config_init(ma_device_type_playback);
		devCfg.playback.format = ma_format_f32;
		devCfg.playback.channels = kEngineChannels;
		devCfg.sampleRate = kSampleRate;
		devCfg.dataCallback = NullDeviceDataCallback;
		devCfg.pUserData = &cbState;

		ma_device nullDevice{};
		check(ma_device_init(&nullCtx, &devCfg, &nullDevice) == MA_SUCCESS,
			"ma_device_init(ma_backend_null) succeeds");
		check(ma_device_start(&nullDevice) == MA_SUCCESS,
			"ma_device_start starts real miniaudio worker thread");

		for (int waitIter = 0; waitIter < 50 && cbState.framesMixed.load() < 2048; ++waitIter)
			ma_sleep(10);

		check(cbState.callbackCount.load() > 0 && cbState.framesMixed.load() > 0,
			"ma_device worker thread invokes dataCallback and pulls real PCM frames");
		check(ma_device_stop(&nullDevice) == MA_SUCCESS,
			"ma_device_stop cleanly stops worker thread");
		ma_device_uninit(&nullDevice);
		ma_context_uninit(&nullCtx);
	}

	// 14. Clean teardown & custom VFS traffic + ma_resource_manager buffer deduplication verification
	mgr.Destroy();
	check(vfs.openCount >= 7 && vfs.openCount <= 25 && vfs.readBytesTotal > 50000,
		"Custom EterPack VFS served all miniaudio requests with ma_resource_manager buffer deduplication");

	std::printf(
		"MINIAUDIO_REAL_API_X64_SUITE: %d/%d CHECKS PASSED (miniaudio v%s, host_backend=%s, host_playback_devices=%u, vfs_opens=%llu, vfs_bytes=%llu, 34-voice 1.0s mix=%.3f ms, PHYSICAL_SPEAKER_OUTPUT=UNVERIFIED)\n",
		passed, passed, ma_version_string(), activeBackendName, playbackDeviceCount,
		static_cast<unsigned long long>(vfs.openCount), static_cast<unsigned long long>(vfs.readBytesTotal), mixMs);
	return 0;
}
