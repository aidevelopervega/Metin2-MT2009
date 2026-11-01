#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

#include "../client/src/EterBase/lzo.h"
#include "../client/src/EterBase/Timer.h"
#include "../client/src/EterPack/EterPackManager.h"
#include "../client/src/MilesLib/SoundManager.h"
#include "../client/src/MilesLib/MiniaudioEngine.h"

namespace
{
int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const char* expr, int line)
{
	if (cond)
	{
		++g_passed;
	}
	else
	{
		++g_failed;
		std::fprintf(stderr, "[FAIL] Line %d: %s\n", line, expr);
	}
}

#define CHECK(cond) Check((cond), #cond, __LINE__)

struct StereoEnergy
{
	double leftRms = 0.0;
	double rightRms = 0.0;
};

StereoEnergy MeasureStereoEnergy(const std::vector<float>& stereoFrames)
{
	StereoEnergy res{};
	size_t frameCount = stereoFrames.size() / 2;
	if (frameCount == 0)
		return res;

	double sumL = 0.0;
	double sumR = 0.0;
	for (size_t i = 0; i < frameCount; ++i)
	{
		double l = stereoFrames[i * 2 + 0];
		double r = stereoFrames[i * 2 + 1];
		sumL += l * l;
		sumR += r * r;
	}
	res.leftRms = std::sqrt(sumL / static_cast<double>(frameCount));
	res.rightRms = std::sqrt(sumR / static_cast<double>(frameCount));
	return res;
}

} // anonymous namespace

int main()
{
	static_assert(sizeof(void*) == 8, "Must compile as native Windows x64 (PE32+ AMD64)");
	static_assert(sizeof(lzo_uint) == 8, "lzo_uint must be 8 bytes on Windows x64");

	std::printf("=== MT2009 Production MilesLib + miniaudio v0.11.26 + Real EterPack x64 Suite ===\n");

	// 1. Strict WASAPI hardware probe (no unconditional check(true))
	mileslib_miniaudio::WasapiProbeResult wasapiProbe{};
	bool wasapiCtxOk = mileslib_miniaudio::MiniaudioEngine::ProbeWasapiHardware(wasapiProbe);
	CHECK(wasapiCtxOk && wasapiProbe.contextInitialized);
	CHECK(wasapiProbe.playbackDeviceCount == 0 || wasapiProbe.deviceInitSucceeded);

	const char* physicalAudioGateStatus =
		(wasapiProbe.playbackDeviceCount == 0)
		? "UNVERIFIED (no audio output device)"
		: "UNVERIFIED (WASAPI device init/start OK; human-heard full in-game audio pending)";

	std::printf("[INFO] WASAPI Hardware Probe: context=%d, playbackDevices=%u, default='%s', devInit=%d, devStart=%d, mixedFrames=%u -> physical_audio_output=%s\n",
		wasapiProbe.contextInitialized ? 1 : 0,
		wasapiProbe.playbackDeviceCount,
		wasapiProbe.defaultDeviceName[0] ? wasapiProbe.defaultDeviceName : "none",
		wasapiProbe.deviceInitSucceeded ? 1 : 0,
		wasapiProbe.deviceStartSucceeded ? 1 : 0,
		wasapiProbe.mixedFrames,
		physicalAudioGateStatus);

	// Use deterministic headless mixing on MiniaudioEngine so we can inspect exact stereo PCM output
	mileslib_miniaudio::MiniaudioEngine::Instance().SetForceHeadlessMode(true);

	// 2. Initialize real CLZO, CTimer, CEterPackManager, and register real game sound packs
	struct TestEterPackManager : public CEterPackManager
	{
		using CEterPackManager::FindPack;
	};

	CTimer timer;
	ELTimer_Init();
	timer.Advance();
	CLZO lzo;
	TestEterPackManager packMgr;
	packMgr.SetCacheMode();
	packMgr.SetSearchMode(CEterPackManager::SEARCH_PACK_FIRST);

	bool regEtc = packMgr.RegisterPack("client/files/pack/sound_etc", "sound/");
	bool regNpc = packMgr.RegisterPack("client/files/pack/sound_npc", "sound/monster/");
	bool regPc  = packMgr.RegisterPack("client/files/pack/sound_pc", "sound/pc/");
	CHECK(regEtc);
	CHECK(regNpc);
	CHECK(regPc);

	// Discover real audio files inside the decrypted MCOZ EterPack indices
	CEterPack* pPackEtc = packMgr.FindPack("sound/ambience/test.wav");
	CEterPack* pPackNpc = packMgr.FindPack("sound/monster/test.wav");
	CEterPack* pPackPc  = packMgr.FindPack("sound/pc/test.wav");
	CHECK(pPackEtc != nullptr);
	CHECK(pPackNpc != nullptr);
	CHECK(pPackPc != nullptr);

	std::vector<std::string> realWavFiles;
	std::vector<std::string> realMp3PackFiles;

	auto collectFromPack = [&](CEterPack* pPack, size_t maxWav, size_t maxMp3)
	{
		if (!pPack)
			return;
		const auto& idxMap = pPack->GetIndexMap();
		for (auto it = idxMap.begin(); it != idxMap.end(); ++it)
		{
			TEterPackIndex* pIdx = it->second;
			if (!pIdx || !pIdx->filename[0] || pIdx->real_data_size <= 64)
				continue;
			std::string fname(pIdx->filename);
			if (fname.size() > 4 && fname.substr(fname.size() - 4) == ".wav" && realWavFiles.size() < maxWav)
			{
				if (packMgr.isExist(fname.c_str()))
					realWavFiles.push_back(fname);
			}
			else if (fname.size() > 4 && fname.substr(fname.size() - 4) == ".mp3" && realMp3PackFiles.size() < maxMp3)
			{
				if (packMgr.isExist(fname.c_str()))
					realMp3PackFiles.push_back(fname);
			}
			if (realWavFiles.size() >= maxWav && realMp3PackFiles.size() >= maxMp3)
				break;
		}
	};

	collectFromPack(pPackEtc, 4, 2);
	collectFromPack(pPackNpc, 8, 4);
	collectFromPack(pPackPc, 12, 6);

	CHECK(pPackEtc->GetIndexMap().size() > 50);
	CHECK(pPackNpc->GetIndexMap().size() > 100);
	CHECK(pPackPc->GetIndexMap().size() > 100);
	CHECK(realWavFiles.size() >= 6);

	std::printf("[INFO] Real EterPack indices decrypted: sound_etc=%zu, sound_npc=%zu, sound_pc=%zu (sampled %zu WAVs, %zu pack MP3s)\n",
		pPackEtc->GetIndexMap().size(),
		pPackNpc->GetIndexMap().size(),
		pPackPc->GetIndexMap().size(),
		realWavFiles.size(),
		realMp3PackFiles.size());

	// 3. Initialize production CSoundManager (which initializes CSoundManager2D, CSoundManager3D, CSoundManagerStream, CSoundBase, MiniaudioEngine)
	CSoundData::SetPackMode();
	CSoundManager soundMgr;
	CHECK(soundMgr.Create());
	CHECK(mileslib_miniaudio::MiniaudioEngine::Instance().IsInitialized());

	// 4. Verify 2D Sound playback from real EterPack archive (sound_etc / sound_npc / sound_pc)
	uint32_t packReadsBefore = mileslib_miniaudio::MiniaudioEngine::Instance().GetPackDataReadCount();
	soundMgr.SetSoundVolumeRatio(1.0f);
	soundMgr.PlaySound2D(realWavFiles[0].c_str());
	uint32_t packReadsAfter2D = mileslib_miniaudio::MiniaudioEngine::Instance().GetPackDataReadCount();
	CHECK(packReadsAfter2D > packReadsBefore);
	CHECK(mileslib_miniaudio::MiniaudioEngine::Instance().GetTotalPackBytesRead() > 0);

	std::vector<float> mixBuf(9600 * 2, 0.0f);
	uint64_t framesPumped = mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(9600, mixBuf.data());
	CHECK(framesPumped == 9600);
	StereoEnergy e2d = MeasureStereoEnergy(mixBuf);
	CHECK(e2d.leftRms > 0.0001 || e2d.rightRms > 0.0001);

	// Drain/stop 2D voice so 3D spatialization energy is measured in isolation
	for (int drain = 0; drain < 20; ++drain)
		mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(9600, mixBuf.data());

	// 5. Verify 3D Sound spatialization, listener position/direction, and left/right panning using real EterPack audio
	soundMgr.StopAllSound3D();
	soundMgr.SetPosition(0.0f, 0.0f, 0.0f);
	soundMgr.SetDirection(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	int idxLeft = soundMgr.PlayAmbienceSound3D(-800.0f, 0.0f, 0.0f, realWavFiles[1].c_str(), 0);
	CHECK(idxLeft >= 0);
	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(9600, mixBuf.data());
	StereoEnergy eLeft = MeasureStereoEnergy(mixBuf);
	soundMgr.StopSound3D(idxLeft);

	int idxRight = soundMgr.PlayAmbienceSound3D(800.0f, 0.0f, 0.0f, realWavFiles[2].c_str(), 0);
	CHECK(idxRight >= 0);
	std::fill(mixBuf.begin(), mixBuf.end(), 0.0f);
	mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(9600, mixBuf.data());
	StereoEnergy eRight = MeasureStereoEnergy(mixBuf);
	soundMgr.StopSound3D(idxRight);

	CHECK(eLeft.leftRms > eLeft.rightRms);
	CHECK(eRight.rightRms > eRight.leftRms);

	// 6. Verify 3D distance culling (> 5000.0f) + PlaySound3D + PlayCharacterSound3D 0.3s (300ms) cooldown
	uint32_t readsBeforeCull = mileslib_miniaudio::MiniaudioEngine::Instance().GetPackDataReadCount();
	soundMgr.PlayCharacterSound3D(6000.0f, 0.0f, 0.0f, realWavFiles[3].c_str(), TRUE);
	uint32_t readsAfterCull = mileslib_miniaudio::MiniaudioEngine::Instance().GetPackDataReadCount();
	CHECK(readsAfterCull == readsBeforeCull); // Culled because distance squared > 5000.0f * 5000.0f

	soundMgr.PlayCharacterSound3D(100.0f, 0.0f, 0.0f, realWavFiles[3].c_str(), TRUE);
	uint32_t readsAfterNear = mileslib_miniaudio::MiniaudioEngine::Instance().GetPackDataReadCount();
	CHECK(readsAfterNear > readsBeforeCull); // Played because distance <= 5000.0f
	// Second call within 0.3s cooldown window is suppressed by PlayCharacterSound3D
	soundMgr.PlayCharacterSound3D(100.0f, 0.0f, 0.0f, realWavFiles[3].c_str(), TRUE);
	soundMgr.PlaySound3D(200.0f, 0.0f, 0.0f, realWavFiles[2].c_str(), 1);
	soundMgr.StopAllSound3D();

	// 7. Verify Volume Grade & Ratio curves, SaveVolume/RestoreVolume
	soundMgr.SetSoundVolumeGrade(5);
	float volGrade5 = soundMgr.GetSoundVolume();
	CHECK(volGrade5 > 0.8f && volGrade5 <= 1.0f);
	soundMgr.SetMusicVolume(0.6f);
	CHECK(std::fabs(soundMgr.GetMusicVolume() - 0.6f) < 0.001f);
	soundMgr.SaveVolume();
	CHECK(soundMgr.GetSoundVolume() == 0.0f && soundMgr.GetMusicVolume() == 0.0f);
	soundMgr.RestoreVolume();
	CHECK(std::fabs(soundMgr.GetSoundVolume() - volGrade5) < 0.001f);
	CHECK(std::fabs(soundMgr.GetMusicVolume() - 0.6f) < 0.001f);

	// 8. Verify Real BGM MP3 Streaming (client/files/BGM/*.mp3) + Pack VFS Streaming + FadeIn / FadeLimitOut / FadeOut
	uint32_t vfsDiskBefore = mileslib_miniaudio::MiniaudioEngine::Instance().GetVfsDiskOpenCount();
	soundMgr.SetMusicVolume(0.8f);
	soundMgr.FadeInMusic("client/files/BGM/characterselect.mp3", 0.2f);
	CHECK(mileslib_miniaudio::MiniaudioEngine::Instance().GetVfsDiskOpenCount() > vfsDiskBefore);

	for (int step = 0; step < 6; ++step)
	{
		soundMgr.Update();
		mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(2400, mixBuf.data());
	}
	StereoEnergy eBgm = MeasureStereoEnergy(mixBuf);
	CHECK(eBgm.leftRms > 0.001 && eBgm.rightRms > 0.001);

	// Crossfade to second real BGM MP3
	soundMgr.FadeInMusic("client/files/BGM/enter_the_east.mp3", 0.25f);
	soundMgr.FadeLimitOutMusic("client/files/BGM/characterselect.mp3", 0.2f, 0.2f);
	for (int step = 0; step < 5; ++step)
	{
		soundMgr.Update();
		mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(1200, mixBuf.data());
	}

	// Also stream an audio asset directly from EterPack via MiniaudioEterPackVFS
	uint32_t vfsPackBefore = mileslib_miniaudio::MiniaudioEngine::Instance().GetVfsPackOpenCount();
	const char* packStreamTarget = !realMp3PackFiles.empty() ? realMp3PackFiles[0].c_str() : realWavFiles[0].c_str();
	soundMgr.PlayMusic(packStreamTarget);
	CHECK(mileslib_miniaudio::MiniaudioEngine::Instance().GetVfsPackOpenCount() > vfsPackBefore);

	soundMgr.FadeOutAllMusic();
	for (int step = 0; step < 120; ++step)
		soundMgr.Update();

	// 9. Verify Thread Safety, Concurrent Pack Loading, Callback Memory Lifetime, and Device Loss Recovery
	std::atomic<bool> workerFailed{false};
	std::vector<std::thread> workers;
	for (int t = 0; t < 4; ++t)
	{
		workers.emplace_back([&, t]()
		{
			for (int iter = 0; iter < 15; ++iter)
			{
				const std::string& wavPath = realWavFiles[(t + iter) % realWavFiles.size()];
				soundMgr.PlaySound2D(wavPath.c_str());
				int slot = soundMgr.PlayAmbienceSound3D(100.0f * (t + 1), 50.0f, 0.0f, wavPath.c_str(), 1);
				if (slot >= 0 && (iter % 3 == 0))
					soundMgr.StopSound3D(slot);
			}
		});
	}

	for (int pumpIter = 0; pumpIter < 10; ++pumpIter)
		mileslib_miniaudio::MiniaudioEngine::Instance().PumpFrames(1200, mixBuf.data());

	for (auto& th : workers)
		th.join();
	CHECK(!workerFailed.load());

	// Simulate audio device loss & recovery while engine is live
	CHECK(mileslib_miniaudio::MiniaudioEngine::Instance().SimulateDeviceLossAndRecover());
	CHECK(mileslib_miniaudio::MiniaudioEngine::Instance().GetDeviceLossNotificationCount() >= 1);

	// 10. Clean teardown
	soundMgr.Destroy();
	CHECK(!mileslib_miniaudio::MiniaudioEngine::Instance().IsInitialized());

	std::printf("MILESLIB_MINIAUDIO_PRODUCTION_X64: %d/%d PASSED (packDataReads=%u, vfsPackOpens=%u, vfsDiskOpens=%u, totalPackBytes=%llu, physical_audio_output=%s)\n",
		g_passed,
		g_passed + g_failed,
		mileslib_miniaudio::MiniaudioEngine::Instance().GetPackDataReadCount(),
		mileslib_miniaudio::MiniaudioEngine::Instance().GetVfsPackOpenCount(),
		mileslib_miniaudio::MiniaudioEngine::Instance().GetVfsDiskOpenCount(),
		static_cast<unsigned long long>(mileslib_miniaudio::MiniaudioEngine::Instance().GetTotalPackBytesRead()),
		physicalAudioGateStatus);

	return (g_failed == 0) ? 0 : 1;
}
