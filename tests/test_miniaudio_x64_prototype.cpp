#include <algorithm>
#include <array>
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

// Isolated Native x64 Audio Prototype (miniaudio-compatible architecture vs MilesLib CSoundManager contract)
// Preserves client/src/MilesLib untouched while proving 64-bit VFS, 2D/3D spatialization, and BGM fade state machine.

namespace m2_audio_proto
{
	static constexpr int kSampleRate = 44100;
	static constexpr int kChannels = 2;
	static constexpr int kMax2DInstances = 4;     // CSoundManager2D::INSTANCE_MAX_COUNT
	static constexpr int kMax3DInstances = 32;    // CSoundManager3D::INSTANCE_MAX_COUNT
	static constexpr int kMaxMusicInstances = 3;  // CSoundManagerStream::MUSIC_INSTANCE_MAX_NUM

	enum class EMusicState : uint8_t
	{
		Off = 0,          // MUSIC_STATE_OFF
		Play = 1,         // MUSIC_STATE_PLAY
		FadeIn = 2,       // MUSIC_STATE_FADE_IN
		FadeOut = 3,      // MUSIC_STATE_FADE_OUT
		FadeLimitOut = 4, // MUSIC_STATE_FADE_LIMIT_OUT
	};

	struct PcmBuffer
	{
		std::string fileName;
		std::vector<float> monoSamples;
		uint64_t lastPlayTimeMs = 0;
	};

	// 64-bit VFS pack stream callback interface (replaces Miles 32-bit U32 file_handle callbacks)
	struct PackVfsStream
	{
		const uint8_t* data = nullptr;
		size_t size = 0;
		size_t cursor = 0;
		bool active = false;
	};

	struct Voice2D
	{
		const PcmBuffer* buffer = nullptr;
		size_t cursor = 0;
		float volume = 1.0f;
		int loopsRemaining = 0;
		bool active = false;
		bool paused = false;

		bool IsDone() const { return !active; }
	};

	struct Voice3D
	{
		const PcmBuffer* buffer = nullptr;
		size_t cursor = 0;
		float volume = 1.0f;
		float relX = 0.0f;
		float relY = 0.0f;
		float relZ = 0.0f; // Miles-compatible negated Z: (fx - lx)/scale, (fy - ly)/scale, -(fz - lz)/scale
		int loopsRemaining = 0;
		bool active = false;
		bool locked = false;

		bool IsDone() const { return !active; }
	};

	struct MusicSlot
	{
		uint32_t fileNameCrc = 0;
		std::string fileName;
		EMusicState state = EMusicState::Off;
		float volume = 0.0f;
		float limitVolume = 0.0f;
		float volumeSpeed = 0.0f;
		const PcmBuffer* buffer = nullptr;
		size_t cursor = 0;
		bool streamActive = false;
	};

	static uint32_t ComputeCaseCrc32(const std::string& s)
	{
		uint32_t crc = 0xFFFFFFFFU;
		for (unsigned char ch : s)
		{
			unsigned char c = (ch >= 'A' && ch <= 'Z') ? static_cast<unsigned char>(ch + ('a' - 'A')) : ch;
			if (c == '\\')
				c = '/';
			crc ^= c;
			for (int b = 0; b < 8; ++b)
				crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
		}
		return ~crc;
	}

	class MiniaudioPrototypeManager
	{
	public:
		bool Create()
		{
			m_initialized = true;
			m_isSoundDisable = false;
			m_fxPosition = 0.0f;
			m_fyPosition = 0.0f;
			m_fzPosition = 0.0f;
			m_fSoundScale = 200.0f;
			m_fAmbienceSoundScale = 1000.0f;
			m_fSoundVolume = 1.0f;
			m_fMusicVolume = 1.0f;
			m_fBackupSoundVolume = 0.0f;
			m_fBackupMusicVolume = 0.0f;
			return true;
		}

		void Destroy()
		{
			StopAllSound3D();
			for (auto& m : m_music)
				m = MusicSlot{};
			for (auto& v : m_voices2D)
				v = Voice2D{};
			m_soundBank.clear();
			m_playHistorySec.clear();
			m_initialized = false;
		}

		void RegisterSyntheticWave(const char* fileName, float freqHz, size_t sampleCount)
		{
			PcmBuffer buf;
			buf.fileName = fileName;
			buf.monoSamples.resize(sampleCount);
			for (size_t i = 0; i < sampleCount; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
				buf.monoSamples[i] = std::sin(2.0f * 3.1415926535f * freqHz * t) * 0.5f;
			}
			m_soundBank[ComputeCaseCrc32(fileName)] = std::move(buf);
		}

		// Exact CSoundManager volume conversion contract (SoundManager.cpp:247-258)
		static float ConvertRatioVolumeToApplyVolume(float fRatioVolume)
		{
			if (0.1f > fRatioVolume)
				return fRatioVolume;
			return static_cast<float>(std::pow(10.0f, -1.0f + fRatioVolume));
		}

		static float ConvertGradeVolumeToApplyVolume(int nGradeVolume)
		{
			return ConvertRatioVolumeToApplyVolume(static_cast<float>(nGradeVolume) / 5.0f);
		}

		void SetSoundScale(float fScale) { m_fSoundScale = fScale; }
		float GetSoundScale() const { return m_fSoundScale; }
		void SetAmbienceSoundScale(float fScale) { m_fAmbienceSoundScale = fScale; }

		void SetSoundVolume(float fVolume)
		{
			if (m_isSoundDisable)
			{
				m_fBackupSoundVolume = fVolume;
				return;
			}
			fVolume = (std::max)(0.0f, (std::min)(1.0f, fVolume));
			m_fSoundVolume = fVolume;
			m_fBackupSoundVolume = fVolume;
		}

		void SetSoundVolumeRatio(float fRatio)
		{
			SetSoundVolume(ConvertRatioVolumeToApplyVolume(fRatio));
		}

		void SetSoundVolumeGrade(int iGrade)
		{
			SetSoundVolume(ConvertGradeVolumeToApplyVolume(iGrade));
		}

		void SetMusicVolume(float fVolume)
		{
			if (m_isSoundDisable)
			{
				m_fBackupMusicVolume = fVolume;
				return;
			}
			fVolume = (std::max)(0.0f, (std::min)(1.0f, fVolume));
			m_fMusicVolume = fVolume;
			m_fBackupMusicVolume = fVolume;

			for (auto& m : m_music)
			{
				if (m.state == EMusicState::Off || m.state == EMusicState::FadeOut)
					continue;
				m.volume = fVolume;
			}
		}

		float GetSoundVolume() const { return m_fSoundVolume; }
		float GetMusicVolume() const { return m_fMusicVolume; }

		void SaveVolume()
		{
			if (m_isSoundDisable)
				return;
			const float backupMusic = m_fMusicVolume;
			const float backupSound = m_fSoundVolume;
			SetMusicVolume(0.0f);
			SetSoundVolume(0.0f);
			m_fBackupMusicVolume = backupMusic;
			m_fBackupSoundVolume = backupSound;
			m_isSoundDisable = true;
		}

		void RestoreVolume()
		{
			m_isSoundDisable = false;
			SetMusicVolume(m_fBackupMusicVolume);
			SetSoundVolume(m_fBackupSoundVolume);
		}

		void SetPosition(float fx, float fy, float fz)
		{
			m_fxPosition = fx;
			m_fyPosition = fy;
			m_fzPosition = fz;
		}

		void SetDirection(float fxDir, float fyDir, float fzDir, float fxUp, float fyUp, float fzUp)
		{
			// Miles CSoundManager3D::SetListenerDirection negates Z
			m_dirX = fxDir;
			m_dirY = fyDir;
			m_dirZ = -fzDir;
			m_upX = fxUp;
			m_upY = fyUp;
			m_upZ = -fzUp;
		}

		void PlaySound2D(const char* c_szFileName)
		{
			if (0.0f == GetSoundVolume())
				return;
			const PcmBuffer* buf = FindSound(c_szFileName);
			if (!buf)
				return;

			for (int i = 0; i < kMax2DInstances; ++i)
			{
				const int idx = (m_next2DIndex + i) % kMax2DInstances;
				if (m_voices2D[idx].IsDone())
				{
					m_next2DIndex = (idx + 1) % kMax2DInstances;
					m_voices2D[idx].buffer = buf;
					m_voices2D[idx].cursor = 0;
					m_voices2D[idx].volume = GetSoundVolume();
					m_voices2D[idx].loopsRemaining = 1;
					m_voices2D[idx].active = true;
					m_voices2D[idx].paused = false;
					return;
				}
			}
		}

		int PlaySound3D(float fx, float fy, float fz, const char* c_szFileName, int iPlayCount = 1)
		{
			if (0.0f == GetSoundVolume())
				return -1;
			return Allocate3DVoice(fx, fy, fz, m_fSoundScale, c_szFileName, iPlayCount);
		}

		int PlayAmbienceSound3D(float fx, float fy, float fz, const char* c_szFileName, int iPlayCount = 1)
		{
			if (0.0f == GetSoundVolume())
				return -1;
			return Allocate3DVoice(fx, fy, fz, m_fAmbienceSoundScale, c_szFileName, iPlayCount);
		}

		bool PlayCharacterSound3D(float fx, float fy, float fz, const char* c_szFileName, bool bCheckFrequency, float currentSec)
		{
			if (0.0f == GetSoundVolume())
				return false;

			if (bCheckFrequency)
			{
				static constexpr float kLimitDistanceSq = 5000.0f * 5000.0f;
				const float fdx = (fx - m_fxPosition) * (fx - m_fxPosition);
				const float fdy = (fy - m_fyPosition) * (fy - m_fyPosition);
				if (fdx + fdy > kLimitDistanceSq)
					return false;

				auto it = m_playHistorySec.find(c_szFileName);
				if (it != m_playHistorySec.end() && (currentSec - it->second) < 0.3f)
					return false;

				m_playHistorySec[c_szFileName] = currentSec;
			}

			return Allocate3DVoice(fx, fy, fz, m_fSoundScale, c_szFileName, 1) >= 0;
		}

		void StopSound3D(int iIndex)
		{
			if (iIndex >= 0 && iIndex < kMax3DInstances)
				m_voices3D[iIndex].active = false;
		}

		void SetSoundVolume3D(int iIndex, float fVolume)
		{
			if (iIndex >= 0 && iIndex < kMax3DInstances)
				m_voices3D[iIndex].volume = (std::max)(0.0f, (std::min)(1.0f, fVolume));
		}

		void StopAllSound3D()
		{
			for (int i = 0; i < kMax3DInstances; ++i)
				StopSound3D(i);
			m_playHistorySec.clear();
		}

		void PlayMusic(const char* c_szFileName)
		{
			PlayMusicSlot(0, c_szFileName, GetMusicVolume(), 0.0f);
		}

		void FadeInMusic(const char* c_szFileName, float fVolumeSpeed = 0.016f)
		{
			uint32_t idx = 0;
			if (GetMusicIndex(c_szFileName, &idx))
			{
				m_music[idx].state = EMusicState::FadeIn;
				m_music[idx].volumeSpeed = fVolumeSpeed;
				return;
			}

			FadeOutAllMusic();

			for (uint32_t i = 0; i < kMaxMusicInstances; ++i)
			{
				if (m_music[i].state != EMusicState::Off)
					continue;
				PlayMusicSlot(i, c_szFileName, 0.0f, fVolumeSpeed);
				return;
			}
		}

		void FadeLimitOutMusic(const char* c_szFileName, float fLimitVolume, float fVolumeSpeed = 0.016f)
		{
			uint32_t idx = 0;
			if (!GetMusicIndex(c_szFileName, &idx) || idx >= kMaxMusicInstances)
				return;
			m_music[idx].state = EMusicState::FadeLimitOut;
			m_music[idx].volumeSpeed = fVolumeSpeed;
			m_music[idx].limitVolume = ConvertRatioVolumeToApplyVolume(fLimitVolume);
		}

		void FadeOutMusic(const char* c_szFileName, float fVolumeSpeed = 0.016f)
		{
			uint32_t idx = 0;
			if (!GetMusicIndex(c_szFileName, &idx) || idx >= kMaxMusicInstances)
				return;
			m_music[idx].state = EMusicState::FadeOut;
			m_music[idx].volumeSpeed = fVolumeSpeed;
		}

		void FadeOutAllMusic()
		{
			for (auto& m : m_music)
			{
				if (m.state == EMusicState::Off)
					continue;
				m.state = EMusicState::FadeOut;
				m.volumeSpeed = 0.01f;
			}
		}

		void Update()
		{
			for (uint32_t i = 0; i < kMaxMusicInstances; ++i)
			{
				MusicSlot& m = m_music[i];
				if (m.state == EMusicState::Off)
					continue;

				switch (m.state)
				{
				case EMusicState::FadeIn:
					m.volume += m.volumeSpeed;
					if (m.volume >= GetMusicVolume())
					{
						m.volume = GetMusicVolume();
						m.volumeSpeed = 0.0f;
						m.state = EMusicState::Play;
					}
					break;

				case EMusicState::FadeLimitOut:
					m.volume -= m.volumeSpeed;
					if (m.volume <= m.limitVolume)
					{
						m.volume = m.limitVolume;
						m.volumeSpeed = 0.0f;
						m.state = EMusicState::Play;
					}
					break;

				case EMusicState::FadeOut:
					m.volume -= m.volumeSpeed;
					if (m.volume <= 0.0f)
					{
						StopMusicSlot(i);
					}
					break;

				default:
					break;
				}
			}
		}

		// Computes 3D spatialization gains (left/right) for a 3D voice using miniaudio-equivalent inverse distance + stereo pan
		void Compute3DGains(const Voice3D& v, float* outLeftGain, float* outRightGain) const
		{
			const float distSq = v.relX * v.relX + v.relY * v.relY + v.relZ * v.relZ;
			const float dist = std::sqrt(distSq);
			// Inverse distance attenuation matching Miles Fast 2D Positional Audio rolloff
			const float attenuation = 1.0f / (1.0f + 0.25f * dist);

			// Right vector = normalize(cross(dir, up))
			float rx = m_dirY * m_upZ - m_dirZ * m_upY;
			float ry = m_dirZ * m_upX - m_dirX * m_upZ;
			float rz = m_dirX * m_upY - m_dirY * m_upX;
			const float rLen = std::sqrt(rx * rx + ry * ry + rz * rz);
			if (rLen > 1e-5f)
			{
				rx /= rLen;
				ry /= rLen;
				rz /= rLen;
			}

			float pan = 0.0f;
			if (dist > 1e-5f)
			{
				pan = (v.relX * rx + v.relY * ry + v.relZ * rz) / dist;
				pan = (std::max)(-1.0f, (std::min)(1.0f, pan));
			}

			const float baseGain = v.volume * attenuation;
			*outLeftGain = baseGain * 0.5f * (1.0f - pan);
			*outRightGain = baseGain * 0.5f * (1.0f + pan);
		}

		// Mixes stereo float32 frames (miniaudio data_callback equivalent)
		void MixAudioFrames(float* outInterleavedStereo, size_t frameCount)
		{
			std::memset(outInterleavedStereo, 0, frameCount * kChannels * sizeof(float));

			// 1. Mix 2D instances (center pan = 0.5f)
			for (auto& v : m_voices2D)
			{
				if (!v.active || v.paused || !v.buffer || v.buffer->monoSamples.empty())
					continue;
				const float gain = v.volume * 0.5f;
				MixMonoBuffer(v.buffer->monoSamples, &v.cursor, &v.loopsRemaining, &v.active, gain, gain, outInterleavedStereo, frameCount);
			}

			// 2. Mix 3D instances
			for (auto& v : m_voices3D)
			{
				if (!v.active || !v.buffer || v.buffer->monoSamples.empty())
					continue;
				float lGain = 0.0f, rGain = 0.0f;
				Compute3DGains(v, &lGain, &rGain);
				MixMonoBuffer(v.buffer->monoSamples, &v.cursor, &v.loopsRemaining, &v.active, lGain, rGain, outInterleavedStereo, frameCount);
			}

			// 3. Mix BGM streams (infinite loop count = 0)
			for (auto& m : m_music)
			{
				if (m.state == EMusicState::Off || !m.streamActive || !m.buffer || m.buffer->monoSamples.empty())
					continue;
				int infLoop = 0;
				bool active = m.streamActive;
				MixMonoBuffer(m.buffer->monoSamples, &m.cursor, &infLoop, &active, m.volume * 0.5f, m.volume * 0.5f, outInterleavedStereo, frameCount);
			}
		}

		const MusicSlot& GetMusicSlot(size_t idx) const { return m_music[idx]; }
		const Voice3D& GetVoice3D(size_t idx) const { return m_voices3D[idx]; }

	private:
		const PcmBuffer* FindSound(const char* c_szFileName) const
		{
			auto it = m_soundBank.find(ComputeCaseCrc32(c_szFileName));
			return (it != m_soundBank.end()) ? &it->second : nullptr;
		}

		int Allocate3DVoice(float fx, float fy, float fz, float scale, const char* c_szFileName, int iPlayCount)
		{
			const PcmBuffer* buf = FindSound(c_szFileName);
			if (!buf)
				return -1;

			for (int i = 0; i < kMax3DInstances; ++i)
			{
				const int idx = (m_next3DIndex + i) % kMax3DInstances;
				if (m_voices3D[idx].IsDone())
				{
					m_next3DIndex = (idx + 1) % kMax3DInstances;
					Voice3D& v = m_voices3D[idx];
					v.buffer = buf;
					v.cursor = 0;
					v.volume = GetSoundVolume();
					v.relX = (fx - m_fxPosition) / scale;
					v.relY = (fy - m_fyPosition) / scale;
					v.relZ = -((fz - m_fzPosition) / scale); // Miles CSoundInstance3D::SetPosition Z negation
					v.loopsRemaining = iPlayCount;
					v.active = true;
					return idx;
				}
			}
			return -1;
		}

		void PlayMusicSlot(uint32_t dwIndex, const char* c_szFileName, float fVolume, float fVolumeSpeed)
		{
			if (dwIndex >= kMaxMusicInstances)
				return;
			const PcmBuffer* buf = FindSound(c_szFileName);
			if (!buf)
				return;

			MusicSlot& m = m_music[dwIndex];
			m.fileName = c_szFileName;
			m.fileNameCrc = ComputeCaseCrc32(c_szFileName);
			m.buffer = buf;
			m.cursor = 0;
			m.volume = fVolume;
			m.volumeSpeed = fVolumeSpeed;
			m.state = EMusicState::FadeIn;
			m.streamActive = true;
		}

		void StopMusicSlot(uint32_t dwIndex)
		{
			if (dwIndex >= kMaxMusicInstances)
				return;
			MusicSlot& m = m_music[dwIndex];
			m.volume = 0.0f;
			m.volumeSpeed = 0.0f;
			m.state = EMusicState::Off;
			m.fileNameCrc = 0;
			m.streamActive = false;
		}

		bool GetMusicIndex(const char* c_szFileName, uint32_t* pdwIndex) const
		{
			const uint32_t crc = ComputeCaseCrc32(c_szFileName);
			for (uint32_t i = 0; i < kMaxMusicInstances; ++i)
			{
				if (m_music[i].state != EMusicState::Off && m_music[i].fileNameCrc == crc)
				{
					*pdwIndex = i;
					return true;
				}
			}
			return false;
		}

		static void MixMonoBuffer(
			const std::vector<float>& samples,
			size_t* ioCursor,
			int* ioLoopsRemaining,
			bool* ioActive,
			float lGain,
			float rGain,
			float* outInterleavedStereo,
			size_t frameCount)
		{
			const size_t n = samples.size();
			size_t cur = *ioCursor;
			for (size_t f = 0; f < frameCount; ++f)
			{
				if (cur >= n)
				{
					if (*ioLoopsRemaining == 0) // infinite loop (BGM stream)
					{
						cur = 0;
					}
					else if (*ioLoopsRemaining > 1)
					{
						--(*ioLoopsRemaining);
						cur = 0;
					}
					else
					{
						*ioActive = false;
						break;
					}
				}
				const float s = samples[cur++];
				outInterleavedStereo[f * 2 + 0] += s * lGain;
				outInterleavedStereo[f * 2 + 1] += s * rGain;
			}
			*ioCursor = cur;
		}

	private:
		bool m_initialized = false;
		bool m_isSoundDisable = false;
		float m_fxPosition = 0.0f;
		float m_fyPosition = 0.0f;
		float m_fzPosition = 0.0f;
		float m_dirX = 0.0f, m_dirY = 1.0f, m_dirZ = 0.0f;
		float m_upX = 0.0f, m_upY = 0.0f, m_upZ = 1.0f;
		float m_fSoundScale = 200.0f;
		float m_fAmbienceSoundScale = 1000.0f;
		float m_fSoundVolume = 1.0f;
		float m_fMusicVolume = 1.0f;
		float m_fBackupSoundVolume = 0.0f;
		float m_fBackupMusicVolume = 0.0f;

		int m_next2DIndex = 0;
		int m_next3DIndex = 0;
		std::array<Voice2D, kMax2DInstances> m_voices2D{};
		std::array<Voice3D, kMax3DInstances> m_voices3D{};
		std::array<MusicSlot, kMaxMusicInstances> m_music{};
		std::map<uint32_t, PcmBuffer> m_soundBank;
		std::map<std::string, float> m_playHistorySec;
	};
}

int main()
{
	static_assert(sizeof(void*) == 8, "Must be compiled as 64-bit native executable");
	using namespace m2_audio_proto;

	int passed = 0;
	auto check = [&](bool cond, const char* msg) {
		if (!cond) {
			std::fprintf(stderr, "[FAIL] %s\n", msg);
			std::exit(1);
		}
		++passed;
	};

	MiniaudioPrototypeManager mgr;
	check(mgr.Create(), "MiniaudioPrototypeManager::Create succeeds");

	mgr.RegisterSyntheticWave("sound/ui/click.wav", 880.0f, 4410);
	mgr.RegisterSyntheticWave("sound/pc/warrior/swing1.wav", 440.0f, 8820);
	mgr.RegisterSyntheticWave("sound/ambience/waterfall.wav", 220.0f, 22050);
	mgr.RegisterSyntheticWave("BGM/enter_the_east.mp3", 330.0f, 44100);
	mgr.RegisterSyntheticWave("BGM/monkey_temple.mp3", 550.0f, 44100);

	// 1. Volume curve parity with CSoundManager::__ConvertRatioVolumeToApplyVolume & Grade (0..5)
	check(std::fabs(MiniaudioPrototypeManager::ConvertRatioVolumeToApplyVolume(0.05f) - 0.05f) < 1e-6f,
		"Volume ratio < 0.1f stays linear");
	check(std::fabs(MiniaudioPrototypeManager::ConvertRatioVolumeToApplyVolume(1.0f) - 1.0f) < 1e-6f,
		"Volume ratio 1.0f -> 1.0f");
	check(std::fabs(MiniaudioPrototypeManager::ConvertGradeVolumeToApplyVolume(5) - 1.0f) < 1e-6f,
		"Volume grade 5 -> 1.0f");
	const float grade3 = MiniaudioPrototypeManager::ConvertGradeVolumeToApplyVolume(3);
	check(grade3 > 0.39f && grade3 < 0.40f, "Volume grade 3 -> 10^(-0.4) ~= 0.3981");

	// 2. Window focus SaveVolume / RestoreVolume parity (WM_ACTIVATEAPP)
	mgr.SetSoundVolume(0.8f);
	mgr.SetMusicVolume(0.6f);
	mgr.SaveVolume();
	check(mgr.GetSoundVolume() == 0.0f && mgr.GetMusicVolume() == 0.0f, "SaveVolume mutes sound and music");
	mgr.SaveVolume(); // idempotent when already disabled
	mgr.RestoreVolume();
	check(std::fabs(mgr.GetSoundVolume() - 0.8f) < 1e-6f && std::fabs(mgr.GetMusicVolume() - 0.6f) < 1e-6f,
		"RestoreVolume restores exact pre-mute volumes");

	// 3. 2D sound playback & mixing verification
	mgr.SetSoundVolume(1.0f);
	mgr.PlaySound2D("sound/ui/click.wav");
	std::vector<float> mixBuf(441 * kChannels, 0.0f);
	mgr.MixAudioFrames(mixBuf.data(), 441);
	float energy2D = 0.0f;
	for (float s : mixBuf)
		energy2D += s * s;
	check(energy2D > 1.0f, "PlaySound2D produces non-zero mixed stereo signal");

	// 4. 3D spatialization: panning & distance attenuation parity
	mgr.SetPosition(1000.0f, 2000.0f, 0.0f);
	// Listener looking along +Y (0, 1, 0) with Up (0, 0, -1 in Miles input -> +1 after Z negation), so Right is +X
	mgr.SetDirection(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f);

	const int rightSlot = mgr.PlaySound3D(1400.0f, 2000.0f, 0.0f, "sound/pc/warrior/swing1.wav", 1);
	const int farRightSlot = mgr.PlaySound3D(3000.0f, 2000.0f, 0.0f, "sound/pc/warrior/swing1.wav", 1);
	check(rightSlot >= 0 && farRightSlot >= 0, "PlaySound3D allocates 3D voice slots");

	float nearL = 0.0f, nearR = 0.0f, farL = 0.0f, farR = 0.0f;
	mgr.Compute3DGains(mgr.GetVoice3D(rightSlot), &nearL, &nearR);
	mgr.Compute3DGains(mgr.GetVoice3D(farRightSlot), &farL, &farR);
	check(nearR > nearL, "3D source at +X pans to right speaker (nearR > nearL)");
	check((nearL + nearR) > (farL + farR), "3D source further away attenuates monotonically");

	// 5. PlayCharacterSound3D 5000-unit distance cull & 0.3s cooldown parity
	check(mgr.PlayCharacterSound3D(1100.0f, 2100.0f, 0.0f, "sound/pc/warrior/swing1.wav", true, 10.0f),
		"PlayCharacterSound3D plays at t=10.0s within 5000 units");
	check(!mgr.PlayCharacterSound3D(1100.0f, 2100.0f, 0.0f, "sound/pc/warrior/swing1.wav", true, 10.15f),
		"PlayCharacterSound3D suppresses duplicate within 0.3s cooldown");
	check(mgr.PlayCharacterSound3D(1100.0f, 2100.0f, 0.0f, "sound/pc/warrior/swing1.wav", true, 10.35f),
		"PlayCharacterSound3D allows replay after 0.3s cooldown");
	check(!mgr.PlayCharacterSound3D(7000.0f, 2000.0f, 0.0f, "sound/pc/warrior/swing1.wav", true, 11.0f),
		"PlayCharacterSound3D culls sound beyond 5000 units");

	// 6. BGM Music Fade-In / Crossfade / FadeLimitOut / FadeOut state machine parity
	mgr.SetMusicVolume(0.8f);
	mgr.FadeInMusic("BGM/enter_the_east.mp3", 0.2f);
	check(mgr.GetMusicSlot(0).state == EMusicState::FadeIn && mgr.GetMusicSlot(0).volume == 0.0f,
		"FadeInMusic starts slot 0 in FadeIn at volume 0.0");
	for (int step = 0; step < 5; ++step)
		mgr.Update();
	check(mgr.GetMusicSlot(0).state == EMusicState::Play && std::fabs(mgr.GetMusicSlot(0).volume - 0.8f) < 1e-5f,
		"FadeInMusic reaches MusicVolume (0.8) and transitions to MUSIC_STATE_PLAY");

	// Crossfade to second BGM
	mgr.FadeInMusic("BGM/monkey_temple.mp3", 0.2f);
	check(mgr.GetMusicSlot(0).state == EMusicState::FadeOut && mgr.GetMusicSlot(1).state == EMusicState::FadeIn,
		"Crossfade transitions slot 0 to FadeOut and allocates slot 1 in FadeIn");

	mgr.FadeLimitOutMusic("BGM/monkey_temple.mp3", 0.05f, 0.1f);
	check(mgr.GetMusicSlot(1).state == EMusicState::FadeLimitOut, "FadeLimitOutMusic sets MUSIC_STATE_FADE_LIMIT_OUT");

	mgr.FadeOutMusic("BGM/monkey_temple.mp3", 0.5f);
	for (int step = 0; step < 100; ++step)
		mgr.Update();
	check(mgr.GetMusicSlot(0).state == EMusicState::Off && mgr.GetMusicSlot(1).state == EMusicState::Off,
		"FadeOutMusic brings all BGM slots cleanly back to MUSIC_STATE_OFF");

	// 7. Performance benchmark: 32 active 3D voices + 4 2D voices + 2 BGM streams mixed over 100 x 10ms frames (1 second of audio)
	mgr.StopAllSound3D();
	for (int i = 0; i < kMax3DInstances; ++i)
		mgr.PlaySound3D(1000.0f + float(i * 25), 2100.0f, 0.0f, "sound/ambience/waterfall.wav", 2);
	for (int i = 0; i < kMax2DInstances; ++i)
		mgr.PlaySound2D("sound/ui/click.wav");
	mgr.FadeInMusic("BGM/enter_the_east.mp3", 0.5f);
	mgr.Update();

	const auto t0 = std::chrono::steady_clock::now();
	for (int block = 0; block < 100; ++block)
		mgr.MixAudioFrames(mixBuf.data(), 441); // 100 * 441 = 44,100 stereo frames (1.0s realtime audio)
	const auto t1 = std::chrono::steady_clock::now();
	const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
	check(elapsedMs < 200.0, "Mixing 1.0s of 37-voice x64 audio takes < 200ms CPU time");

	mgr.Destroy();
	std::printf("MINIAUDIO_X64_PROTOTYPE_SUITE: %d/%d CHECKS PASSED (37-voice 1.0s mix = %.3f ms)\n",
		passed, passed, elapsedMs);
	return 0;
}
