#ifndef __MILESLIB_MINIAUDIO_ENGINE_H__
#define __MILESLIB_MINIAUDIO_ENGINE_H__

#if defined(MT2_USE_MINIAUDIO)

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

namespace mileslib_miniaudio
{

struct WasapiProbeResult
{
	bool contextInitialized = false;
	uint32_t playbackDeviceCount = 0;
	char defaultDeviceName[256] = {};
	bool deviceInitSucceeded = false;
	bool deviceStartSucceeded = false;
	uint32_t mixedFrames = 0;
};

struct VoiceSlot;
struct StreamSlot;

class MiniaudioEngine
{
public:
	static MiniaudioEngine& Instance();

	bool Initialize();
	void Destroy();
	bool IsInitialized() const { return m_bInitialized.load(std::memory_order_acquire); }
	bool IsUsingHardwareDevice() const { return m_bUsingHardwareDevice.load(std::memory_order_acquire); }

	void SetForceHeadlessMode(bool bForceHeadless) { m_bForceHeadless = bForceHeadless; }
	bool IsForceHeadlessMode() const { return m_bForceHeadless; }

	void Lock();
	void Unlock();

	// Listener 3D controls (matches Miles right-handed/inverted-Z convention)
	void SetListenerPosition(float fX, float fY, float fZ);
	void SetListenerDirection(float fxDir, float fyDir, float fzDir, float fxUp, float fyUp, float fzUp);
	void SetListenerVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float fMagnitude);
	void GetListenerPosition(float& rfX, float& rfY, float& rfZ) const;

	// Duration probe for CSoundData::ReadFromDisk()
	uint32_t ComputeBufferDurationMs(const void* pvData, uint32_t dwSize);

	// 2D / 3D Voice slot management
	VoiceSlot* AllocateVoiceSlot(bool bIs3D);
	void FreeVoiceSlot(VoiceSlot* pSlot);
	bool BindVoiceSoundData(VoiceSlot* pSlot, const void* pvData, uint32_t dwSize, const char* c_szFileName);
	void UnbindVoiceSoundData(VoiceSlot* pSlot);

	void PlayVoice(VoiceSlot* pSlot, int iLoopCount);
	void PauseVoice(VoiceSlot* pSlot);
	void ResumeVoice(VoiceSlot* pSlot);
	void StopVoice(VoiceSlot* pSlot);
	bool IsVoiceDone(VoiceSlot* pSlot);
	float GetVoiceVolume(VoiceSlot* pSlot) const;
	void SetVoiceVolume(VoiceSlot* pSlot, float fVolume);
	void SetVoicePosition(VoiceSlot* pSlot, float fX, float fY, float fZ);
	void SetVoiceOrientation(VoiceSlot* pSlot, float xFace, float yFace, float zFace, float xUp, float yUp, float zUp);
	void SetVoiceVelocity(VoiceSlot* pSlot, float fDistX, float fDistY, float fDistZ, float fMagnitude);
	void SetVoiceRange(VoiceSlot* pSlot, float fMinDist, float fMaxDist);
	void GetVoicePosition(VoiceSlot* pSlot, float& rfX, float& rfY, float& rfZ) const;

	// Stream slot management (BGM / ambient streams via EterPack VFS or disk)
	StreamSlot* OpenStreamSlot(const char* c_szFileName);
	void CloseStreamSlot(StreamSlot* pSlot);
	void PlayStream(StreamSlot* pSlot, int iLoopCount);
	void PauseStream(StreamSlot* pSlot);
	void ResumeStream(StreamSlot* pSlot);
	void StopStream(StreamSlot* pSlot);
	bool IsStreamDone(StreamSlot* pSlot) const;
	float GetStreamVolume(StreamSlot* pSlot) const;
	void SetStreamVolume(StreamSlot* pSlot, float fVolume);

	// Deterministic frame mixing helper for noDevice / headless mode & tests
	uint64_t PumpFrames(uint32_t frameCount, float* pOutInterleavedStereoF32 = nullptr);

	// Diagnostics & Telemetry for verification
	void RecordPackDataRead(bool bFromPack, uint32_t dwBytes);
	uint32_t GetPackDataReadCount() const { return m_uPackDataReads.load(std::memory_order_relaxed); }
	uint32_t GetDiskDataReadCount() const { return m_uDiskDataReads.load(std::memory_order_relaxed); }
	uint32_t GetVfsPackOpenCount() const { return m_uVfsPackOpens.load(std::memory_order_relaxed); }
	uint32_t GetVfsDiskOpenCount() const { return m_uVfsDiskOpens.load(std::memory_order_relaxed); }
	uint64_t GetTotalPackBytesRead() const { return m_uTotalPackBytesRead.load(std::memory_order_relaxed); }
	uint32_t GetDeviceLossNotificationCount() const { return m_uDeviceLossNotifications.load(std::memory_order_relaxed); }

	void IncrementVfsOpenCount(bool bFromPack, uint32_t dwBytes);
	void OnDeviceLossNotification();
	bool SimulateDeviceLossAndRecover();

	static bool ProbeWasapiHardware(WasapiProbeResult& rOutResult);

private:
	MiniaudioEngine();
	~MiniaudioEngine();
	MiniaudioEngine(const MiniaudioEngine&) = delete;
	MiniaudioEngine& operator=(const MiniaudioEngine&) = delete;

	struct EngineImpl;
	EngineImpl* m_pImpl;
	mutable std::recursive_mutex m_mutex;
	std::atomic<bool> m_bInitialized;
	std::atomic<bool> m_bUsingHardwareDevice;
	bool m_bForceHeadless;

	std::atomic<uint32_t> m_uPackDataReads;
	std::atomic<uint32_t> m_uDiskDataReads;
	std::atomic<uint32_t> m_uVfsPackOpens;
	std::atomic<uint32_t> m_uVfsDiskOpens;
	std::atomic<uint64_t> m_uTotalPackBytesRead;
	std::atomic<uint32_t> m_uDeviceLossNotifications;
};

} // namespace mileslib_miniaudio

#endif // MT2_USE_MINIAUDIO
#endif // __MILESLIB_MINIAUDIO_ENGINE_H__
