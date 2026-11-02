#ifndef MT2_TESTS_GUILD_MARK_STB_TEST_SUITE_H
#define MT2_TESTS_GUILD_MARK_STB_TEST_SUITE_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <stb/mt2_image_stb.h>

extern CGuildMarkImage* NewMarkImage();
extern void DeleteMarkImage(CGuildMarkImage* pkImage);

namespace mt2::test_guild_mark_stb
{
	inline std::vector<uint8_t> BuildLegacyDevIL180Golden16x12TGA(std::vector<Pixel>& outExpectedLoadedBGRA)
	{
		constexpr int kW = 16;
		constexpr int kH = 12;
		outExpectedLoadedBGRA.resize(kW * kH);

		// Raw TGA file bytes exactly as emitted by DevIL 1.8.0 ilSave(IL_TGA) (1319 bytes with TGA 2.0 extension & footer).
		std::vector<uint8_t> tga(1319, 0);
		tga[2] = 2;    // Uncompressed true-color
		tga[12] = kW;  // Width = 16
		tga[14] = kH;  // Height = 12
		tga[16] = 32;  // 32 bits per pixel
		tga[17] = 8;   // 8 alpha bits, lower-left origin descriptor (0x08)

		for (int fileRow = 0; fileRow < kH; ++fileRow)
		{
			for (int x = 0; x < kW; ++x)
			{
				const uint8_t b = static_cast<uint8_t>((x + 1) & 0xFF);
				const uint8_t g = static_cast<uint8_t>((fileRow + 1) & 0xFF);
				const uint8_t r = static_cast<uint8_t>((x * 16 + fileRow + 0x20) & 0xFF);
				const uint8_t a = static_cast<uint8_t>(0x80 + ((x + fileRow) & 0x7F));
				const size_t off = 18U + static_cast<size_t>(fileRow * kW + x) * 4U;
				tga[off + 0] = b;
				tga[off + 1] = g;
				tga[off + 2] = r;
				tga[off + 3] = a;

				// Both DevIL ilLoad(IL_ORIGIN_UPPER_LEFT) and stbi_load flip lower-left (desc=0x08) rows into top-left memory order.
				const int memRow = (kH - 1) - fileRow;
				outExpectedLoadedBGRA[static_cast<size_t>(memRow * kW + x)] = mt2::image::PackBGRA(r, g, b, a);
			}
		}

		// Exact DevIL 1.8.0 TGA 2.0 extension area + footer metadata:
		tga[786] = 239;
		tga[787] = 1;
		const char* swId = "Developer's Image Library (DevIL)";
		std::memcpy(&tga[1224], swId, std::strlen(swId));
		tga[1265] = 180;
		tga[1267] = 32;
		tga[1292] = 3;
		tga[1293] = 18;
		tga[1294] = 3;
		const char* footerSig = "TRUEVISION-XFILE.";
		std::memcpy(&tga[1301], footerSig, std::strlen(footerSig) + 1);
		return tga;
	}

	inline std::vector<uint8_t> BuildUncompressedBMP24(uint32_t width, uint32_t height, std::vector<Pixel>& outExpectedBGRA)
	{
		outExpectedBGRA.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
		const uint32_t rowStride = (width * 3U + 3U) & ~3U;
		const uint32_t pixelDataSize = rowStride * height;
		const uint32_t fileSize = 54U + pixelDataSize;

		std::vector<uint8_t> bmp(fileSize, 0);
		bmp[0] = 'B';
		bmp[1] = 'M';
		std::memcpy(&bmp[2], &fileSize, 4);
		const uint32_t dataOffset = 54U;
		std::memcpy(&bmp[10], &dataOffset, 4);
		const uint32_t dibSize = 40U;
		std::memcpy(&bmp[14], &dibSize, 4);
		std::memcpy(&bmp[18], &width, 4);
		std::memcpy(&bmp[22], &height, 4); // Positive height = bottom-up DIB
		const uint16_t planes = 1U;
		const uint16_t bpp = 24U;
		std::memcpy(&bmp[26], &planes, 2);
		std::memcpy(&bmp[28], &bpp, 2);
		std::memcpy(&bmp[34], &pixelDataSize, 4);

		for (uint32_t y = 0; y < height; ++y)
		{
			const uint32_t fileRow = (height - 1U) - y;
			for (uint32_t x = 0; x < width; ++x)
			{
				const uint8_t r = static_cast<uint8_t>((x * 7U + y * 3U + 19U) & 0xFFU);
				const uint8_t g = static_cast<uint8_t>((x * 5U + y * 11U + 41U) & 0xFFU);
				const uint8_t b = static_cast<uint8_t>((x * 13U + y * 17U + 73U) & 0xFFU);
				const size_t off = 54U + static_cast<size_t>(fileRow) * rowStride + static_cast<size_t>(x) * 3U;
				bmp[off + 0] = b;
				bmp[off + 1] = g;
				bmp[off + 2] = r;
				outExpectedBGRA[static_cast<size_t>(y) * width + x] = mt2::image::PackBGRA(r, g, b, 0xFFU);
			}
		}
		return bmp;
	}

	inline bool WriteBinaryFile(const std::string& path, const void* data, size_t size)
	{
		mt2::image::UniqueFile fp(std::fopen(path.c_str(), "wb"));
		if (!fp)
			return false;
		if (size > 0 && std::fwrite(data, 1, size, fp.get()) != size)
			return false;
		return std::fflush(fp.get()) == 0;
	}

	inline int RunAllScenarios(const char* suiteTag, int& ioPassCount, int& ioFailCount)
	{
		const int initialFails = ioFailCount;
		auto check = [&](bool cond, const char* msg, int line) {
			if (cond)
			{
				++ioPassCount;
			}
			else
			{
				++ioFailCount;
				std::fprintf(stderr, "[FAIL] [%s] %s (guild_mark_stb_test_suite.h:%d)\n", suiteTag, msg, line);
			}
		};

		const std::string tmpPrefix = std::string("mt2_stb_test_") + suiteTag + "_";

		// ---------------------------------------------------------------------
		// Scenario 1: Open legacy DevIL 1.8.0-authored TGA file with stb
		// ---------------------------------------------------------------------
		{
			std::vector<Pixel> expectedBGRA;
			std::vector<uint8_t> devilTga = BuildLegacyDevIL180Golden16x12TGA(expectedBGRA);
			const std::string devilPath = tmpPrefix + "s1_devil_golden.tga";
			check(WriteBinaryFile(devilPath, devilTga.data(), devilTga.size()), "S1: write golden DevIL TGA fixture", __LINE__);

			std::vector<Pixel> loadedBGRA;
			uint32_t w = 0U, h = 0U;
			const mt2::image::LoadStatus st = mt2::image::LoadBGRAFromFile(devilPath.c_str(), loadedBGRA, w, h, 16U, 12U);
			check(st == mt2::image::LoadStatus::Ok && w == 16U && h == 12U, "S1: LoadBGRAFromFile decodes legacy DevIL TGA", __LINE__);
			check(loadedBGRA.size() == expectedBGRA.size() &&
				  std::memcmp(loadedBGRA.data(), expectedBGRA.data(), expectedBGRA.size() * sizeof(Pixel)) == 0,
				  "S1: Legacy DevIL TGA pixel-for-pixel parity with DevIL ilLoad(IL_ORIGIN_UPPER_LEFT)", __LINE__);
			std::remove(devilPath.c_str());
		}

		// ---------------------------------------------------------------------
		// Scenario 2 & 3: Re-open stb-written TGA files & lossless pixel round-trip
		// ---------------------------------------------------------------------
		{
			constexpr uint32_t kW = 16U, kH = 12U;
			std::vector<Pixel> orig(kW * kH);
			for (uint32_t y = 0; y < kH; ++y)
			{
				for (uint32_t x = 0; x < kW; ++x)
				{
					const uint8_t r = static_cast<uint8_t>((x * 15U + y * 7U + 3U) & 0xFFU);
					const uint8_t g = static_cast<uint8_t>((x * 9U + y * 19U + 50U) & 0xFFU);
					const uint8_t b = static_cast<uint8_t>((x * 23U + y * 11U + 100U) & 0xFFU);
					const uint8_t a = static_cast<uint8_t>((x * 31U + y * 13U) & 0xFFU);
					orig[y * kW + x] = mt2::image::PackBGRA(r, g, b, a);
				}
			}

			const std::string stbTgaPath = tmpPrefix + "s2_roundtrip.tga";
			check(mt2::image::SaveBGRAToTGAFile(stbTgaPath.c_str(), orig.data(), kW, kH),
				  "S2: SaveBGRAToTGAFile writes 16x12 TGA", __LINE__);

			std::vector<Pixel> reloaded;
			uint32_t rw = 0U, rh = 0U;
			check(mt2::image::LoadBGRAFromFile(stbTgaPath.c_str(), reloaded, rw, rh, kW, kH) == mt2::image::LoadStatus::Ok,
				  "S2: LoadBGRAFromFile re-opens stb TGA", __LINE__);
			check(rw == kW && rh == kH && reloaded.size() == orig.size() &&
				  std::memcmp(reloaded.data(), orig.data(), orig.size() * sizeof(Pixel)) == 0,
				  "S3: Lossless pixel-for-pixel RGBA/BGRA round-trip across all asymmetric coordinates", __LINE__);
			std::remove(stbTgaPath.c_str());
		}

		// ---------------------------------------------------------------------
		// Scenario 4: Exact RGBA <-> BGRA channel conversion & memory layout
		// ---------------------------------------------------------------------
		{
			const Pixel px = mt2::image::PackBGRA(0x12U, 0x34U, 0x56U, 0x78U);
			check(px == 0x78123456U, "S4: PackBGRA produces 0xAARRGGBB uint32_t", __LINE__);

			uint8_t rawBytes[4] = {};
			std::memcpy(rawBytes, &px, 4);
			check(rawBytes[0] == 0x56U && rawBytes[1] == 0x34U && rawBytes[2] == 0x12U && rawBytes[3] == 0x78U,
				  "S4: Little-endian memory layout is [B, G, R, A]", __LINE__);

			uint8_t r = 0U, g = 0U, b = 0U, a = 0U;
			mt2::image::UnpackBGRA(px, r, g, b, a);
			check(r == 0x12U && g == 0x34U && b == 0x56U && a == 0x78U,
				  "S4: UnpackBGRA restores exact R, G, B, A channels", __LINE__);
		}

		// ---------------------------------------------------------------------
		// Scenario 5: Alpha channel & transparent guild marks (SGuildMark)
		// ---------------------------------------------------------------------
		{
			SGuildMark mark{};
			std::memset(mark.m_apxBuf, 0, sizeof(mark.m_apxBuf));
			check(mark.IsEmpty(), "S5: Zero-initialized SGuildMark is empty (transparent 0x00000000)", __LINE__);

			mark.Clear();
			check(!mark.IsEmpty() && mark.m_apxBuf[0] == 0xff000000U && mark.m_apxBuf[SGuildMark::SIZE - 1] == 0xff000000U,
				  "S5: SGuildMark::Clear fills with opaque black 0xff000000", __LINE__);

			std::memset(mark.m_apxBuf, 0, sizeof(mark.m_apxBuf));
			mark.m_apxBuf[SGuildMark::SIZE / 2] = mt2::image::PackBGRA(0U, 0U, 0U, 1U); // Alpha = 1
			check(!mark.IsEmpty(), "S5: Non-zero alpha pixel makes SGuildMark non-empty", __LINE__);
		}

		// ---------------------------------------------------------------------
		// Scenario 6: 16x12 mark upload data integrity (BMP + TGA)
		// ---------------------------------------------------------------------
		{
			std::vector<Pixel> expectedMark;
			std::vector<uint8_t> bmp16x12 = BuildUncompressedBMP24(SGuildMark::WIDTH, SGuildMark::HEIGHT, expectedMark);
			const std::string bmpPath = tmpPrefix + "s6_mark_16x12.bmp";
			check(WriteBinaryFile(bmpPath, bmp16x12.data(), bmp16x12.size()), "S6: write 16x12 BMP mark fixture", __LINE__);

			std::vector<Pixel> loadedMark;
			uint32_t mw = 0U, mh = 0U;
			check(mt2::image::LoadBGRAFromFile(bmpPath.c_str(), loadedMark, mw, mh, SGuildMark::WIDTH, SGuildMark::HEIGHT) ==
				  mt2::image::LoadStatus::Ok,
				  "S6: LoadBGRAFromFile accepts valid 16x12 BMP mark", __LINE__);
			check(mw == 16U && mh == 12U && loadedMark.size() == SGuildMark::SIZE &&
				  std::memcmp(loadedMark.data(), expectedMark.data(), sizeof(Pixel) * SGuildMark::SIZE) == 0,
				  "S6: 16x12 BMP mark pixel data matches expected BGRA buffer", __LINE__);

			uint32_t probeW = 0U, probeH = 0U;
			check(mt2::image::ProbeAndValidateImageFromFile(bmpPath.c_str(), probeW, probeH) &&
				  probeW == 16U && probeH == 12U,
				  "S6: ProbeAndValidateImageFromFile (appGetImageInfo) returns (16, 12)", __LINE__);
			std::remove(bmpPath.c_str());
		}

		// ---------------------------------------------------------------------
		// Scenario 7: 64x128 guild symbol validation + raw byte & CRC32 invariance
		// ---------------------------------------------------------------------
		{
			std::vector<Pixel> expectedSymbolPx;
			std::vector<uint8_t> bmp64x128 = BuildUncompressedBMP24(64U, 128U, expectedSymbolPx);
			const std::string symPath = tmpPrefix + "s7_symbol_64x128.bmp";
			check(WriteBinaryFile(symPath, bmp64x128.data(), bmp64x128.size()), "S7: write 64x128 symbol fixture", __LINE__);

			std::vector<uint8_t> rawSymbolOut;
			const mt2::image::LoadStatus symSt = mt2::image::ValidateSymbolFileAndReadRaw(symPath.c_str(), rawSymbolOut, 64U, 128U);
			check(symSt == mt2::image::LoadStatus::Ok, "S7: ValidateSymbolFileAndReadRaw accepts valid 64x128 symbol", __LINE__);
			check(rawSymbolOut.size() == bmp64x128.size() &&
				  std::memcmp(rawSymbolOut.data(), bmp64x128.data(), bmp64x128.size()) == 0,
				  "S7: Symbol raw file bytes preserved 100% without re-encoding", __LINE__);

			const DWORD expectedCrc = GetCRC32(reinterpret_cast<const char*>(bmp64x128.data()), bmp64x128.size());
			const DWORD actualCrc = GetCRC32(reinterpret_cast<const char*>(rawSymbolOut.data()), rawSymbolOut.size());
			check(expectedCrc != 0U && expectedCrc == actualCrc, "S7: Symbol CRC32 matches original file bytes exactly", __LINE__);
			std::remove(symPath.c_str());
		}

		// ---------------------------------------------------------------------
		// Scenario 8, 9, 10, 11, 12: 512x512 Atlas Build/Save/Load, PutData/GetData,
		// SaveMark/DeleteMark/BuildAllBlocks/GetDiffBlocks, LZO block sync & CRC32
		// ---------------------------------------------------------------------
		{
			const std::string atlasPath1 = tmpPrefix + "s8_atlas_sender.tga";
			const std::string atlasPath2 = tmpPrefix + "s8_atlas_reloaded.tga";
			const std::string atlasPath3 = tmpPrefix + "s11_atlas_receiver.tga";

			CGuildMarkImage* pkSender = NewMarkImage();
			check(pkSender != nullptr, "S8: NewMarkImage sender allocation", __LINE__);
			check(pkSender->Build(atlasPath1.c_str()), "S8: CGuildMarkImage::Build creates 512x512 TGA", __LINE__);
			check(pkSender->Load(atlasPath1.c_str()), "S8: CGuildMarkImage::Load loads 512x512 TGA", __LINE__);
			check(pkSender->GetEmptyPosition() == 0U, "S8: Initial empty atlas GetEmptyPosition() == 0", __LINE__);

			// Scenario 9: PutData / GetData regional & boundary clipping checks
			Pixel regionIn[4 * 3];
			for (size_t i = 0; i < 12; ++i)
				regionIn[i] = 0xFF500000U + static_cast<Pixel>(i + 1U);
			pkSender->PutData(510U, 510U, 4U, 3U, regionIn); // Partially out-of-bounds (clips to 2x2 at 510..511, 510..511)

			Pixel regionOut[4 * 3] = {};
			pkSender->GetData(510U, 510U, 4U, 3U, regionOut);
			check(regionOut[0] == regionIn[0] && regionOut[1] == regionIn[1] &&
				  regionOut[2] == 0U && regionOut[3] == 0U &&
				  regionOut[4] == regionIn[4] && regionOut[5] == regionIn[5] &&
				  regionOut[8] == 0U,
				  "S9: PutData/GetData clips cleanly at 512x512 boundary and zeroes out-of-bounds output", __LINE__);

			// Out-of-bounds PutData/GetData safety:
			pkSender->PutData(512U, 0U, 4U, 4U, regionIn);
			pkSender->PutData(0U, 512U, 4U, 4U, regionIn);
			pkSender->PutData(0U, 0U, 0U, 4U, regionIn);
			pkSender->PutData(0U, 0U, 4U, 4U, nullptr);
			Pixel oobOut[4] = {1U, 2U, 3U, 4U};
			pkSender->GetData(600U, 600U, 2U, 2U, oobOut);
			check(oobOut[0] == 0U && oobOut[1] == 0U && oobOut[2] == 0U && oobOut[3] == 0U,
				  "S9: Out-of-bounds GetData safely zeroes destination buffer", __LINE__);

			// Restore clean 512x512 atlas for mark/block tests
			check(pkSender->Build(atlasPath1.c_str()) && pkSender->Load(atlasPath1.c_str()),
				  "S8: Re-initialize clean 512x512 atlas", __LINE__);

			// Scenario 10: SaveMark, DeleteMark, GetEmptyPosition, BuildAllBlocks, GetDiffBlocks
			Pixel mark0[SGuildMark::SIZE];
			Pixel mark37[SGuildMark::SIZE];
			for (size_t i = 0; i < SGuildMark::SIZE; ++i)
			{
				mark0[i] = mt2::image::PackBGRA(static_cast<uint8_t>(i & 0xFFU), 0x44U, 0x88U, 0xFFU);
				mark37[i] = mt2::image::PackBGRA(0xAAU, static_cast<uint8_t>((i * 3U) & 0xFFU), 0x22U, 0xC0U);
			}

			check(pkSender->SaveMark(0U, reinterpret_cast<BYTE*>(mark0)), "S10: SaveMark at posMark=0 (block 0)", __LINE__);
			check(pkSender->GetEmptyPosition() == 1U, "S10: GetEmptyPosition advances to 1 after posMark=0 filled", __LINE__);
			check(pkSender->SaveMark(37U, reinterpret_cast<BYTE*>(mark37)), "S10: SaveMark at posMark=37 (row 1, col 5 -> block 1)", __LINE__);
			check(!pkSender->SaveMark(CGuildMarkImage::MARK_TOTAL_COUNT, reinterpret_cast<BYTE*>(mark0)),
				  "S10: SaveMark rejects posMark >= MARK_TOTAL_COUNT (1280)", __LINE__);

			// Save to disk and reload into a second CGuildMarkImage to prove 512x512 persistence
			check(pkSender->Save(atlasPath2.c_str()), "S8: CGuildMarkImage::Save persists modified 512x512 atlas", __LINE__);

			CGuildMarkImage* pkReloaded = NewMarkImage();
			check(pkReloaded->Load(atlasPath2.c_str()), "S8: Second CGuildMarkImage loads saved 512x512 atlas", __LINE__);

			Pixel verifyMark0[SGuildMark::SIZE] = {};
			Pixel verifyMark37[SGuildMark::SIZE] = {};
			pkReloaded->GetData(0U, 0U, SGuildMark::WIDTH, SGuildMark::HEIGHT, verifyMark0);
			pkReloaded->GetData(5U * SGuildMark::WIDTH, 1U * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, verifyMark37);
			check(std::memcmp(verifyMark0, mark0, sizeof(mark0)) == 0 &&
				  std::memcmp(verifyMark37, mark37, sizeof(mark37)) == 0,
				  "S8: Reloaded 512x512 atlas preserves exact pixels for posMark=0 and posMark=37", __LINE__);

			DWORD senderCrcs[CGuildMarkImage::BLOCK_TOTAL_COUNT] = {};
			DWORD reloadedCrcs[CGuildMarkImage::BLOCK_TOTAL_COUNT] = {};
			pkSender->GetBlockCRCList(senderCrcs);
			pkReloaded->GetBlockCRCList(reloadedCrcs);
			check(std::memcmp(senderCrcs, reloadedCrcs, sizeof(senderCrcs)) == 0,
				  "S12: All 80 block CRC32 values match after 512x512 TGA Save -> Load", __LINE__);

			// Scenario 11 & 12: Diff block calculation, LZO decompression, and receiver synchronization
			CGuildMarkImage* pkReceiver = NewMarkImage();
			check(pkReceiver->Build(atlasPath3.c_str()) && pkReceiver->Load(atlasPath3.c_str()),
				  "S11: Initialize empty receiver atlas", __LINE__);

			DWORD receiverCrcs[CGuildMarkImage::BLOCK_TOTAL_COUNT] = {};
			pkReceiver->GetBlockCRCList(receiverCrcs);

			std::map<BYTE, const SGuildMarkBlock*> diffBlocks;
			pkSender->GetDiffBlocks(receiverCrcs, diffBlocks);
			check(diffBlocks.size() == 2U && diffBlocks.count(0U) == 1U && diffBlocks.count(1U) == 1U,
				  "S10: GetDiffBlocks identifies exactly dirty blocks 0 and 1", __LINE__);

			for (const auto& kv : diffBlocks)
			{
				const BYTE posBlock = kv.first;
				const SGuildMarkBlock* blk = kv.second;
				check(blk != nullptr && blk->m_sizeCompBuf > 0U && blk->m_sizeCompBuf <= SGuildMarkBlock::MAX_COMP_SIZE,
					  "S11: Compressed guild mark block size within wire bounds", __LINE__);
				check(pkReceiver->SaveBlockFromCompressedData(posBlock, blk->m_abCompBuf, static_cast<DWORD>(blk->m_sizeCompBuf)),
					  "S11: SaveBlockFromCompressedData decompresses LZO 2.10 block on receiver", __LINE__);
			}

			Pixel recvMark0[SGuildMark::SIZE] = {};
			Pixel recvMark37[SGuildMark::SIZE] = {};
			pkReceiver->GetData(0U, 0U, SGuildMark::WIDTH, SGuildMark::HEIGHT, recvMark0);
			pkReceiver->GetData(5U * SGuildMark::WIDTH, 1U * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, recvMark37);
			check(std::memcmp(recvMark0, mark0, sizeof(mark0)) == 0 &&
				  std::memcmp(recvMark37, mark37, sizeof(mark37)) == 0,
				  "S11: Receiver LZO-decompressed blocks match sender pixels byte-for-byte", __LINE__);

			pkReceiver->GetBlockCRCList(receiverCrcs);
			std::map<BYTE, const SGuildMarkBlock*> remainingDiffs;
			pkSender->GetDiffBlocks(receiverCrcs, remainingDiffs);
			check(remainingDiffs.empty() && std::memcmp(senderCrcs, receiverCrcs, sizeof(senderCrcs)) == 0,
				  "S12: Receiver CRC32 list equals sender CRC32 list with 0 remaining diff blocks", __LINE__);

			// DeleteMark restores empty position 0
			check(pkSender->DeleteMark(0U) && pkSender->GetEmptyPosition() == 0U,
				  "S10: DeleteMark(0) clears mark and restores GetEmptyPosition() == 0", __LINE__);

			DeleteMarkImage(pkReceiver);
			DeleteMarkImage(pkReloaded);
			DeleteMarkImage(pkSender);
			std::remove(atlasPath1.c_str());
			std::remove(atlasPath2.c_str());
			std::remove(atlasPath3.c_str());
		}

		// ---------------------------------------------------------------------
		// Scenario 13 & 14: Corrupted, truncated, oversized, wrong-dimension,
		// file access errors, memory bounds, and overflow protection
		// ---------------------------------------------------------------------
		{
			std::vector<Pixel> outPx;
			uint32_t w = 0U, h = 0U;

			// 14a: Null, empty, and non-existent file paths
			check(mt2::image::LoadBGRAFromFile(nullptr, outPx, w, h) == mt2::image::LoadStatus::ErrorLoad,
				  "S14: Reject nullptr file path", __LINE__);
			check(mt2::image::LoadBGRAFromFile("", outPx, w, h) == mt2::image::LoadStatus::ErrorLoad,
				  "S14: Reject empty file path", __LINE__);
			check(mt2::image::LoadBGRAFromFile("nonexistent_dir_xyz/no_such_file.tga", outPx, w, h) == mt2::image::LoadStatus::ErrorLoad,
				  "S14: Reject non-existent file path", __LINE__);
			check(!mt2::image::SaveBGRAToTGAFile("nonexistent_dir_xyz/cannot_write.tga", &w, 1U, 1U),
				  "S14: SaveBGRAToTGAFile fails cleanly on unwritable path", __LINE__);

			// 14b: Safe pixel count overflow & dimension upper-bound checks
			size_t safeCount = 0U;
			check(!mt2::image::ComputeSafePixelCount(0U, 12U, mt2::image::kMaxImageDimension, safeCount),
				  "S14: Reject width == 0", __LINE__);
			check(!mt2::image::ComputeSafePixelCount(16U, 0U, mt2::image::kMaxImageDimension, safeCount),
				  "S14: Reject height == 0", __LINE__);
			check(!mt2::image::ComputeSafePixelCount(4097U, 16U, mt2::image::kMaxImageDimension, safeCount),
				  "S14: Reject width > kMaxImageDimension (4096)", __LINE__);
			check(!mt2::image::ComputeSafePixelCount(65536U, 65536U, 100000U, safeCount),
				  "S14: Reject 65536x65536 pixel count exceeding kMaxImagePixels", __LINE__);

			// 13a: 0-byte file and garbage bytes
			const std::string corruptPath = tmpPrefix + "s13_corrupt.tga";
			check(WriteBinaryFile(corruptPath, "", 0U), "S13: create 0-byte file", __LINE__);
			check(mt2::image::LoadBGRAFromFile(corruptPath.c_str(), outPx, w, h) == mt2::image::LoadStatus::ErrorLoad,
				  "S13: Reject 0-byte image file", __LINE__);

			const uint8_t garbage[32] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF, 0x13, 0x37};
			check(mt2::image::LoadBGRAFromMemory(garbage, sizeof(garbage), outPx, w, h) == mt2::image::LoadStatus::ErrorLoad,
				  "S13: Reject random garbage bytes", __LINE__);

			// 13b: Truncated TGA file (valid 16x12 header, only 20 bytes of pixel payload instead of 768)
			std::vector<Pixel> dummyExpected;
			std::vector<uint8_t> validTga = BuildLegacyDevIL180Golden16x12TGA(dummyExpected);
			check(mt2::image::LoadBGRAFromMemory(validTga.data(), 38U, outPx, w, h, 16U, 12U) == mt2::image::LoadStatus::ErrorLoad,
				  "S13: Reject truncated 16x12 TGA payload with ErrorLoad", __LINE__);

			// 13c: Malicious oversized TGA header claiming 16384 x 16384 (1 GB RGBA allocation attempt)
			std::vector<uint8_t> maliciousOversizedTga(64, 0);
			maliciousOversizedTga[2] = 2; // uncompressed true-color
			maliciousOversizedTga[12] = 0x00;
			maliciousOversizedTga[13] = 0x40; // width = 16384
			maliciousOversizedTga[14] = 0x00;
			maliciousOversizedTga[15] = 0x40; // height = 16384
			maliciousOversizedTga[16] = 32;
			maliciousOversizedTga[17] = 8;
			check(mt2::image::LoadBGRAFromMemory(maliciousOversizedTga.data(), maliciousOversizedTga.size(), outPx, w, h) ==
				  mt2::image::LoadStatus::ErrorLoad,
				  "S13: Reject malicious 16384x16384 header before pixel allocation", __LINE__);

			// 13d: Wrong width and wrong height distinction for Mark (16x12) and Symbol (64x128)
			std::vector<uint8_t> bmp32x12 = BuildUncompressedBMP24(32U, 12U, dummyExpected);
			check(mt2::image::LoadBGRAFromMemory(bmp32x12.data(), bmp32x12.size(), outPx, w, h, 16U, 12U) ==
				  mt2::image::LoadStatus::ErrorWidth,
				  "S13: 32x12 image returns ErrorWidth when 16x12 expected", __LINE__);

			std::vector<uint8_t> bmp16x24 = BuildUncompressedBMP24(16U, 24U, dummyExpected);
			check(mt2::image::LoadBGRAFromMemory(bmp16x24.data(), bmp16x24.size(), outPx, w, h, 16U, 12U) ==
				  mt2::image::LoadStatus::ErrorHeight,
				  "S13: 16x24 image returns ErrorHeight when 16x12 expected", __LINE__);

			check(WriteBinaryFile(corruptPath, bmp32x12.data(), bmp32x12.size()), "S13: write 32x12 symbol test file", __LINE__);
			std::vector<uint8_t> rawSym;
			check(mt2::image::ValidateSymbolFileAndReadRaw(corruptPath.c_str(), rawSym, 64U, 128U) ==
				  mt2::image::LoadStatus::ErrorWidth && rawSym.empty(),
				  "S13: ValidateSymbolFileAndReadRaw rejects wrong symbol width with ErrorWidth", __LINE__);

			std::vector<uint8_t> bmp64x64 = BuildUncompressedBMP24(64U, 64U, dummyExpected);
			check(WriteBinaryFile(corruptPath, bmp64x64.data(), bmp64x64.size()), "S13: write 64x64 symbol test file", __LINE__);
			check(mt2::image::ValidateSymbolFileAndReadRaw(corruptPath.c_str(), rawSym, 64U, 128U) ==
				  mt2::image::LoadStatus::ErrorHeight && rawSym.empty(),
				  "S13: ValidateSymbolFileAndReadRaw rejects wrong symbol height with ErrorHeight", __LINE__);

			std::remove(corruptPath.c_str());
		}

		// ---------------------------------------------------------------------
		// Scenario 15: Repeated load/save cycle resource & handle leak test
		// ---------------------------------------------------------------------
		{
			const std::string cyclePath = tmpPrefix + "s15_cycle.tga";
			std::vector<Pixel> cycleIn(16U * 12U, 0xFF112233U);
			std::vector<Pixel> cycleOut;
			uint32_t cw = 0U, ch = 0U;
			bool allCyclesOk = true;

			for (int iter = 0; iter < 200; ++iter)
			{
				cycleIn[static_cast<size_t>(iter) % cycleIn.size()] = mt2::image::PackBGRA(
					static_cast<uint8_t>(iter & 0xFF),
					static_cast<uint8_t>((iter * 3) & 0xFF),
					static_cast<uint8_t>((iter * 7) & 0xFF),
					0xFFU);

				if (!mt2::image::SaveBGRAToTGAFile(cyclePath.c_str(), cycleIn.data(), 16U, 12U))
				{
					allCyclesOk = false;
					break;
				}
				if (mt2::image::LoadBGRAFromFile(cyclePath.c_str(), cycleOut, cw, ch, 16U, 12U) != mt2::image::LoadStatus::Ok ||
					cycleOut.size() != cycleIn.size() ||
					std::memcmp(cycleOut.data(), cycleIn.data(), cycleIn.size() * sizeof(Pixel)) != 0)
				{
					allCyclesOk = false;
					break;
				}
			}

			// On Windows, std::remove fails if any FILE* handle was left open during the 200 iterations!
			const int removeRc = std::remove(cyclePath.c_str());
			check(allCyclesOk && removeRc == 0,
				  "S15: 200 repeated Save/Load cycles succeed with zero leaked file handles or buffers", __LINE__);
		}

		return (ioFailCount == initialFails) ? 0 : 1;
	}
} // namespace mt2::test_guild_mark_stb

#endif // MT2_TESTS_GUILD_MARK_STB_TEST_SUITE_H
