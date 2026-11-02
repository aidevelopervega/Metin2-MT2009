#ifndef MT2_COMMON_IMAGE_STB_H
#define MT2_COMMON_IMAGE_STB_H

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#if defined(MT2_STB_IMAGE_IMPLEMENTATION)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100 4244 4456 4457 4701 4996)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#if __GNUC__ >= 7
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#endif

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#else
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#endif

namespace mt2::image
{
	using Pixel = uint32_t;

	enum class LoadStatus : uint8_t
	{
		Ok = 0,
		ErrorLoad = 1,
		ErrorWidth = 2,
		ErrorHeight = 3,
	};

	inline constexpr uint32_t kMaxImageDimension = 4096U;
	inline constexpr uint64_t kMaxImagePixels =
		static_cast<uint64_t>(kMaxImageDimension) * static_cast<uint64_t>(kMaxImageDimension);
	inline constexpr uint64_t kMaxImageBytes = kMaxImagePixels * sizeof(Pixel);
	inline constexpr uint64_t kMaxImageFileBytes = 16ULL * 1024ULL * 1024ULL;
	inline constexpr uint64_t kMaxSymbolFileBytes = 64ULL * 1024ULL;

	// Little-endian BGRA pixel layout (byte 0 = B, byte 1 = G, byte 2 = R, byte 3 = A -> 0xAARRGGBB).
	inline constexpr Pixel PackBGRA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
	{
		return (static_cast<uint32_t>(a) << 24U) |
			   (static_cast<uint32_t>(r) << 16U) |
			   (static_cast<uint32_t>(g) << 8U) |
			   static_cast<uint32_t>(b);
	}

	inline constexpr void UnpackBGRA(Pixel px, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) noexcept
	{
		b = static_cast<uint8_t>(px & 0xFFU);
		g = static_cast<uint8_t>((px >> 8U) & 0xFFU);
		r = static_cast<uint8_t>((px >> 16U) & 0xFFU);
		a = static_cast<uint8_t>((px >> 24U) & 0xFFU);
	}

	struct FileCloser
	{
		void operator()(std::FILE* fp) const noexcept
		{
			if (fp != nullptr)
				std::fclose(fp);
		}
	};
	using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;

	struct StbiPixelsDeleter
	{
		void operator()(unsigned char* ptr) const noexcept
		{
			if (ptr != nullptr)
				stbi_image_free(ptr);
		}
	};
	using UniqueStbiPixels = std::unique_ptr<unsigned char, StbiPixelsDeleter>;

	inline bool ComputeSafePixelCount(uint32_t width, uint32_t height, uint32_t maxDimension, size_t& outPixelCount) noexcept
	{
		outPixelCount = 0;
		if (width == 0U || height == 0U || width > maxDimension || height > maxDimension)
			return false;

		const uint64_t totalPixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
		if (totalPixels == 0ULL || totalPixels > kMaxImagePixels)
			return false;

		const uint64_t totalBytes = totalPixels * sizeof(Pixel);
		if (totalBytes > kMaxImageBytes || totalBytes > static_cast<uint64_t>(SIZE_MAX))
			return false;

		outPixelCount = static_cast<size_t>(totalPixels);
		return true;
	}

	inline bool ReadFileBounded(const char* filePath, std::vector<uint8_t>& outBytes, uint64_t maxFileBytes = kMaxImageFileBytes)
	{
		outBytes.clear();
		if (filePath == nullptr || filePath[0] == '\0' || maxFileBytes == 0ULL)
			return false;

		UniqueFile fp(std::fopen(filePath, "rb"));
		if (!fp)
			return false;

		if (std::fseek(fp.get(), 0, SEEK_END) != 0)
			return false;

		const long rawSize = std::ftell(fp.get());
		if (rawSize <= 0)
			return false;

		const uint64_t uSize = static_cast<uint64_t>(rawSize);
		if (uSize > maxFileBytes || uSize > static_cast<uint64_t>(INT_MAX))
			return false;

		if (std::fseek(fp.get(), 0, SEEK_SET) != 0)
			return false;

		outBytes.resize(static_cast<size_t>(uSize));
		const size_t readBytes = std::fread(outBytes.data(), 1, outBytes.size(), fp.get());
		if (readBytes != outBytes.size() || std::ferror(fp.get()) != 0)
		{
			outBytes.clear();
			return false;
		}

		return true;
	}

	inline bool HasNonTGAMagicSignature(const uint8_t* d, size_t n) noexcept
	{
		if (n >= 8U && d[0] == 0x89U && d[1] == 'P' && d[2] == 'N' && d[3] == 'G' &&
			d[4] == 0x0DU && d[5] == 0x0AU && d[6] == 0x1AU && d[7] == 0x0AU)
			return true; // PNG
		if (n >= 3U && d[0] == 0xFFU && d[1] == 0xD8U && d[2] == 0xFFU)
			return true; // JPEG
		if (n >= 2U && d[0] == 'B' && d[1] == 'M')
			return true; // BMP
		if (n >= 6U && d[0] == 'G' && d[1] == 'I' && d[2] == 'F' && d[3] == '8' &&
			(d[4] == '7' || d[4] == '9') && d[5] == 'a')
			return true; // GIF
		if (n >= 4U && d[0] == '8' && d[1] == 'B' && d[2] == 'P' && d[3] == 'S')
			return true; // PSD
		if (n >= 4U && d[0] == 0x53U && d[1] == 0x80U && d[2] == 0xF6U && d[3] == 0x34U)
			return true; // PIC
		if (n >= 2U && d[0] == 'P' && d[1] >= '1' && d[1] <= '6')
			return true; // PNM
		if (n >= 10U && (std::memcmp(d, "#?RADIANCE", 10) == 0 || std::memcmp(d, "#?RGBE", 6) == 0))
			return true; // HDR
		return false;
	}

	// stbi__tga_load in stb_image v2.30 does not check stbi__getn/stbi__get8 for premature EOF,
	// which would leave uninitialized heap bytes or zero-padded pixels on truncated TGA files.
	inline bool ValidateTGAPayloadIfTGA(const uint8_t* data, size_t size) noexcept
	{
		if (data == nullptr || size == 0U)
			return false;
		if (HasNonTGAMagicSignature(data, size))
			return true;
		if (size < 18U)
			return false;

		const uint8_t idLen = data[0];
		const uint8_t colorMapType = data[1];
		const uint8_t imageType = data[2];
		const uint16_t cmapStart = static_cast<uint16_t>(data[3]) | (static_cast<uint16_t>(data[4]) << 8U);
		const uint16_t cmapLen = static_cast<uint16_t>(data[5]) | (static_cast<uint16_t>(data[6]) << 8U);
		const uint8_t cmapBpp = data[7];
		const uint16_t width = static_cast<uint16_t>(data[12]) | (static_cast<uint16_t>(data[13]) << 8U);
		const uint16_t height = static_cast<uint16_t>(data[14]) | (static_cast<uint16_t>(data[15]) << 8U);
		const uint8_t bpp = data[16];

		if (colorMapType > 1U || width == 0U || height == 0U)
			return false;

		const size_t pixelBytes = (bpp == 15U) ? 2U : static_cast<size_t>(bpp / 8U);
		if (pixelBytes == 0U)
			return false;

		size_t pos = 18U + static_cast<size_t>(idLen);
		if (pos > size)
			return false;

		if (colorMapType == 1U)
		{
			if (cmapLen == 0U)
				return false;
			const size_t cmapEntryBytes = (cmapBpp == 15U) ? 2U : static_cast<size_t>(cmapBpp / 8U);
			if (cmapEntryBytes == 0U)
				return false;
			const size_t cmapBytes = static_cast<size_t>(cmapStart) + static_cast<size_t>(cmapLen) * cmapEntryBytes;
			if (size - pos < cmapBytes)
				return false;
			pos += cmapBytes;
		}

		const size_t totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
		if (imageType < 8U)
		{
			const size_t requiredPixelBytes = totalPixels * pixelBytes;
			return (size - pos) >= requiredPixelBytes;
		}

		size_t decodedPixels = 0U;
		while (decodedPixels < totalPixels)
		{
			if (pos >= size)
				return false;
			const uint8_t cmd = data[pos++];
			const size_t count = 1U + static_cast<size_t>(cmd & 0x7FU);
			const size_t packetBytes = (cmd & 0x80U) != 0U ? pixelBytes : (count * pixelBytes);
			if (size - pos < packetBytes)
				return false;
			pos += packetBytes;
			decodedPixels += count;
		}

		return true;
	}

	inline LoadStatus LoadBGRAFromMemory(
		const uint8_t* fileBytes,
		size_t fileSize,
		std::vector<Pixel>& outPixels,
		uint32_t& outWidth,
		uint32_t& outHeight,
		uint32_t expectedWidth = 0U,
		uint32_t expectedHeight = 0U,
		uint32_t maxDimension = kMaxImageDimension)
	{
		outPixels.clear();
		outWidth = 0U;
		outHeight = 0U;

		if (fileBytes == nullptr || fileSize == 0U || fileSize > static_cast<size_t>(INT_MAX))
			return LoadStatus::ErrorLoad;

		if (!ValidateTGAPayloadIfTGA(fileBytes, fileSize))
			return LoadStatus::ErrorLoad;

		int infoW = 0;
		int infoH = 0;
		int infoComp = 0;
		if (!stbi_info_from_memory(fileBytes, static_cast<int>(fileSize), &infoW, &infoH, &infoComp))
			return LoadStatus::ErrorLoad;

		if (infoW <= 0 || infoH <= 0 || infoComp <= 0)
			return LoadStatus::ErrorLoad;

		size_t safePixelCount = 0;
		if (!ComputeSafePixelCount(static_cast<uint32_t>(infoW), static_cast<uint32_t>(infoH), maxDimension, safePixelCount))
			return LoadStatus::ErrorLoad;

		int decodedW = 0;
		int decodedH = 0;
		int decodedComp = 0;
		UniqueStbiPixels rgba(stbi_load_from_memory(
			fileBytes,
			static_cast<int>(fileSize),
			&decodedW,
			&decodedH,
			&decodedComp,
			4));
		if (!rgba || decodedW != infoW || decodedH != infoH)
			return LoadStatus::ErrorLoad;

		outWidth = static_cast<uint32_t>(decodedW);
		outHeight = static_cast<uint32_t>(decodedH);

		if (expectedWidth != 0U && outWidth != expectedWidth)
			return LoadStatus::ErrorWidth;

		if (expectedHeight != 0U && outHeight != expectedHeight)
			return LoadStatus::ErrorHeight;

		outPixels.resize(safePixelCount);
		const unsigned char* src = rgba.get();
		for (size_t i = 0; i < safePixelCount; ++i)
		{
			const uint8_t r = src[i * 4U + 0U];
			const uint8_t g = src[i * 4U + 1U];
			const uint8_t b = src[i * 4U + 2U];
			const uint8_t a = src[i * 4U + 3U];
			outPixels[i] = PackBGRA(r, g, b, a);
		}

		return LoadStatus::Ok;
	}

	inline LoadStatus LoadBGRAFromFile(
		const char* filePath,
		std::vector<Pixel>& outPixels,
		uint32_t& outWidth,
		uint32_t& outHeight,
		uint32_t expectedWidth = 0U,
		uint32_t expectedHeight = 0U,
		uint32_t maxDimension = kMaxImageDimension,
		uint64_t maxFileBytes = kMaxImageFileBytes)
	{
		std::vector<uint8_t> rawBytes;
		if (!ReadFileBounded(filePath, rawBytes, maxFileBytes))
		{
			outPixels.clear();
			outWidth = 0U;
			outHeight = 0U;
			return LoadStatus::ErrorLoad;
		}

		return LoadBGRAFromMemory(
			rawBytes.data(),
			rawBytes.size(),
			outPixels,
			outWidth,
			outHeight,
			expectedWidth,
			expectedHeight,
			maxDimension);
	}

	inline bool ProbeAndValidateImageFromFile(
		const char* filePath,
		uint32_t& outWidth,
		uint32_t& outHeight,
		uint32_t maxDimension = kMaxImageDimension)
	{
		std::vector<Pixel> decodedPixels;
		return LoadBGRAFromFile(filePath, decodedPixels, outWidth, outHeight, 0U, 0U, maxDimension) == LoadStatus::Ok;
	}

	inline LoadStatus ValidateSymbolFileAndReadRaw(
		const char* filePath,
		std::vector<uint8_t>& outRawBytes,
		uint32_t expectedWidth = 64U,
		uint32_t expectedHeight = 128U,
		uint64_t maxSymbolBytes = kMaxSymbolFileBytes)
	{
		if (!ReadFileBounded(filePath, outRawBytes, maxSymbolBytes))
			return LoadStatus::ErrorLoad;

		std::vector<Pixel> decodedPixels;
		uint32_t width = 0U;
		uint32_t height = 0U;
		const LoadStatus status = LoadBGRAFromMemory(
			outRawBytes.data(),
			outRawBytes.size(),
			decodedPixels,
			width,
			height,
			expectedWidth,
			expectedHeight,
			kMaxImageDimension);
		if (status != LoadStatus::Ok)
		{
			outRawBytes.clear();
			return status;
		}

		return LoadStatus::Ok;
	}

	struct StbiFileWriteContext
	{
		std::FILE* fp = nullptr;
		bool hadError = false;
	};

	inline void StbiWriteFileCallback(void* context, void* data, int size) noexcept
	{
		auto* ctx = static_cast<StbiFileWriteContext*>(context);
		if (ctx == nullptr || ctx->fp == nullptr || ctx->hadError)
			return;
		if (data == nullptr || size <= 0)
		{
			ctx->hadError = true;
			return;
		}
		const size_t n = static_cast<size_t>(size);
		if (std::fwrite(data, 1, n, ctx->fp) != n)
			ctx->hadError = true;
	}

	inline bool SaveBGRAToTGAFile(
		const char* filePath,
		const Pixel* bgraPixels,
		uint32_t width,
		uint32_t height,
		uint32_t maxDimension = kMaxImageDimension)
	{
		if (filePath == nullptr || filePath[0] == '\0' || bgraPixels == nullptr)
			return false;

		size_t safePixelCount = 0;
		if (!ComputeSafePixelCount(width, height, maxDimension, safePixelCount))
			return false;

		std::vector<uint8_t> rgbaBytes(safePixelCount * 4U);
		for (size_t i = 0; i < safePixelCount; ++i)
		{
			uint8_t r = 0U;
			uint8_t g = 0U;
			uint8_t b = 0U;
			uint8_t a = 0U;
			UnpackBGRA(bgraPixels[i], r, g, b, a);
			rgbaBytes[i * 4U + 0U] = r;
			rgbaBytes[i * 4U + 1U] = g;
			rgbaBytes[i * 4U + 2U] = b;
			rgbaBytes[i * 4U + 3U] = a;
		}

		UniqueFile fp(std::fopen(filePath, "wb"));
		if (!fp)
			return false;

		StbiFileWriteContext writeCtx{fp.get(), false};
		const int writeOk = stbi_write_tga_to_func(
			&StbiWriteFileCallback,
			&writeCtx,
			static_cast<int>(width),
			static_cast<int>(height),
			4,
			rgbaBytes.data());
		if (writeOk == 0 || writeCtx.hadError)
			return false;

		if (std::fflush(fp.get()) != 0 || std::ferror(fp.get()) != 0)
			return false;

		return true;
	}
} // namespace mt2::image

#endif // MT2_COMMON_IMAGE_STB_H
