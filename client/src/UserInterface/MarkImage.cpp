#include "stdafx.h"
#include "MarkImage.h"

#if !defined(_MSC_VER)
#include "crc32.h"
#include "lzo_manager.h"
#include "minilzo.h"
#define CLZO LZOManager
#else
#include "../EterBase/lzo.h"
#include "../EterBase/CRC32.h"
#define sys_err TraceError
#define sys_log //(n, format, ...) Tracenf(format, __VA_ARGS__)
#define thecore_memcpy memcpy
#define itertype(cont) typeof(cont.begin())
#endif

#define MT2_STB_IMAGE_IMPLEMENTATION
#include <stb/mt2_image_stb.h>

CGuildMarkImage * NewMarkImage()
{
	return new CGuildMarkImage;
}

void DeleteMarkImage(CGuildMarkImage * pkImage)
{
	delete pkImage;
}

CGuildMarkImage::CGuildMarkImage()
{
	memset(&m_apxImage, 0, sizeof(m_apxImage));
}

CGuildMarkImage::~CGuildMarkImage()
{
	Destroy();
}

void CGuildMarkImage::Destroy()
{
	memset(&m_apxImage, 0, sizeof(m_apxImage));
}

void CGuildMarkImage::Create()
{
	memset(&m_apxImage, 0, sizeof(m_apxImage));
}

bool CGuildMarkImage::Save(const char* c_szFileName)
{
	return mt2::image::SaveBGRAToTGAFile(c_szFileName, m_apxImage, WIDTH, HEIGHT);
}

bool CGuildMarkImage::Build(const char * c_szFileName)
{
	Destroy();
	Create();
	BuildAllBlocks();

	if (!Save(c_szFileName))
		return false;

	return true;
}

bool CGuildMarkImage::Load(const char * c_szFileName)
{
	if (c_szFileName == nullptr || c_szFileName[0] == '\0')
	{
		sys_err("CGuildMarkImage: invalid file name.");
		return false;
	}

	Destroy();
	Create();

	std::vector<Pixel> loadedPixels;
	uint32_t loadedWidth = 0U;
	uint32_t loadedHeight = 0U;
	const mt2::image::LoadStatus status = mt2::image::LoadBGRAFromFile(
		c_szFileName,
		loadedPixels,
		loadedWidth,
		loadedHeight,
		WIDTH,
		HEIGHT);

	if (status == mt2::image::LoadStatus::ErrorLoad)
	{
		sys_err("CGuildMarkImage: %s cannot open file.", c_szFileName);
		return false;
	}

	if (status == mt2::image::LoadStatus::ErrorWidth)
	{
		sys_err("CGuildMarkImage: %s width must be %u", c_szFileName, WIDTH);
		return false;
	}

	if (status == mt2::image::LoadStatus::ErrorHeight)
	{
		sys_err("CGuildMarkImage: %s height must be %u", c_szFileName, HEIGHT);
		return false;
	}

	if (loadedPixels.size() != static_cast<size_t>(WIDTH) * static_cast<size_t>(HEIGHT))
	{
		sys_err("CGuildMarkImage: %s invalid pixel count.", c_szFileName);
		return false;
	}

	memcpy(m_apxImage, loadedPixels.data(), sizeof(m_apxImage));
	BuildAllBlocks();
	return true;
}

void CGuildMarkImage::PutData(UINT x, UINT y, UINT width, UINT height, void * data)
{
	if (data == nullptr || width == 0U || height == 0U || x >= WIDTH || y >= HEIGHT)
		return;

	const Pixel * src = static_cast<const Pixel *>(data);
	const UINT copyCols = (x + width <= WIDTH) ? width : (WIDTH - x);
	const UINT copyRows = (y + height <= HEIGHT) ? height : (HEIGHT - y);

	for (UINT row = 0U; row < copyRows; ++row)
	{
		memcpy(&m_apxImage[(y + row) * WIDTH + x], src + static_cast<size_t>(row) * width, static_cast<size_t>(copyCols) * sizeof(Pixel));
	}
}

void CGuildMarkImage::GetData(UINT x, UINT y, UINT width, UINT height, void * data)
{
	if (data == nullptr || width == 0U || height == 0U)
		return;

	Pixel * dst = static_cast<Pixel *>(data);
	memset(dst, 0, static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(Pixel));

	if (x >= WIDTH || y >= HEIGHT)
		return;

	const UINT copyCols = (x + width <= WIDTH) ? width : (WIDTH - x);
	const UINT copyRows = (y + height <= HEIGHT) ? height : (HEIGHT - y);

	for (UINT row = 0U; row < copyRows; ++row)
	{
		memcpy(dst + static_cast<size_t>(row) * width, &m_apxImage[(y + row) * WIDTH + x], static_cast<size_t>(copyCols) * sizeof(Pixel));
	}
}

// SERVER
bool CGuildMarkImage::SaveMark(DWORD posMark, BYTE * pbImage)
{
	if (posMark >= MARK_TOTAL_COUNT)
	{
		sys_err("CGuildMarkImage::CopyMarkFromData: Invalid mark position %u", posMark);
		return false;
	}

	DWORD colMark = posMark % MARK_COL_COUNT;
	DWORD rowMark = posMark / MARK_COL_COUNT;

	printf("PutMark pos %u %ux%u\n", posMark, colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT);
	PutData(colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, pbImage);

	DWORD rowBlock = rowMark / SGuildMarkBlock::MARK_PER_BLOCK_HEIGHT;
	DWORD colBlock = colMark / SGuildMarkBlock::MARK_PER_BLOCK_WIDTH;

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	GetData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
	m_aakBlock[rowBlock][colBlock].Compress(apxBuf);
	return true;
}

bool CGuildMarkImage::DeleteMark(DWORD posMark)
{
	Pixel image[SGuildMark::SIZE];
	memset(&image, 0, sizeof(image));
	return SaveMark(posMark, (BYTE *) &image);
}

// CLIENT
bool CGuildMarkImage::SaveBlockFromCompressedData(DWORD posBlock, const BYTE * pbComp, DWORD dwCompSize)
{
	if (posBlock >= BLOCK_TOTAL_COUNT)
		return false;

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	size_t sizeBuf = sizeof(apxBuf);

	if (LZO_E_OK != lzo1x_decompress_safe(pbComp, dwCompSize, (BYTE *) apxBuf, (lzo_uint*) &sizeBuf, CLZO::Instance().GetWorkMemory()))
	{
		sys_err("CGuildMarkImage::CopyBlockFromCompressedData: cannot decompress, compressed size = %u", dwCompSize);
		return false;
	}

	if (sizeBuf != sizeof(apxBuf))
	{
		sys_err("CGuildMarkImage::CopyBlockFromCompressedData: image corrupted, decompressed size = %u", sizeBuf);
		return false;
	}

	DWORD rowBlock = posBlock / BLOCK_COL_COUNT;
	DWORD colBlock = posBlock % BLOCK_COL_COUNT;

	PutData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);

	m_aakBlock[rowBlock][colBlock].CopyFrom(pbComp, dwCompSize, GetCRC32((const char *) apxBuf, sizeof(Pixel) * SGuildMarkBlock::SIZE));
	return true;
}

void CGuildMarkImage::BuildAllBlocks()
{
	Pixel apxBuf[SGuildMarkBlock::SIZE];
	sys_log(0, "CGuildMarkImage::BuildAllBlocks");

	for (UINT row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (UINT col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMarkBlock::WIDTH, row * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
			m_aakBlock[row][col].Compress(apxBuf);
		}
}

DWORD CGuildMarkImage::GetEmptyPosition()
{
	SGuildMark kMark;

	for (DWORD row = 0; row < MARK_ROW_COUNT; ++row)
	{
		for (DWORD col = 0; col < MARK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMark::WIDTH, row * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, kMark.m_apxBuf);

			if (kMark.IsEmpty())
				return (row * MARK_COL_COUNT + col);
		}
	}

	return INVALID_MARK_POSITION;
}

void CGuildMarkImage::GetDiffBlocks(const DWORD * crcList, std::map<BYTE, const SGuildMarkBlock *> & mapDiffBlocks)
{
	BYTE posBlock = 0;

	for (DWORD row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (DWORD col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			if (m_aakBlock[row][col].m_crc != *crcList)
			{
				mapDiffBlocks.insert(std::map<BYTE, const SGuildMarkBlock *>::value_type(posBlock, &m_aakBlock[row][col]));
			}
			++crcList;
			++posBlock;
		}
}

void CGuildMarkImage::GetBlockCRCList(DWORD * crcList)
{
	for (DWORD row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (DWORD col = 0; col < BLOCK_COL_COUNT; ++col)
			*(crcList++) = m_aakBlock[row][col].GetCRC();
}

////////////////////////////////////////////////////////////////////////////////
void SGuildMark::Clear()
{
	for (DWORD iPixel = 0; iPixel < SIZE; ++iPixel)
		m_apxBuf[iPixel] = 0xff000000;
}

bool SGuildMark::IsEmpty()
{
	for (DWORD iPixel = 0; iPixel < SIZE; ++iPixel)
		if (m_apxBuf[iPixel] != 0x00000000)
			return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////
DWORD SGuildMarkBlock::GetCRC() const
{
	return m_crc;
}

void SGuildMarkBlock::CopyFrom(const BYTE * pbCompBuf, DWORD dwCompSize, DWORD crc)
{
	if (dwCompSize > MAX_COMP_SIZE)
		return;

	m_sizeCompBuf = dwCompSize;
	thecore_memcpy(m_abCompBuf, pbCompBuf, dwCompSize);
	m_crc = crc;
	//printf("SGuildMarkBlock::CopyFrom: %u > %u crc %u\n", sizeof(Pixel) * SGuildMarkBlock::SIZE, m_sizeCompBuf, m_crc);
}

void SGuildMarkBlock::Compress(const Pixel * pxBuf)
{
	m_sizeCompBuf = MAX_COMP_SIZE;

#if defined(_WIN64)
	if (LZO_E_OK != lzo1x_1_compress((const BYTE *) pxBuf,
		sizeof(Pixel) * SGuildMarkBlock::SIZE, m_abCompBuf,
		(lzo_uint*) &m_sizeCompBuf,
		CLZO::Instance().GetWorkMemory()))
#else
	if (LZO_E_OK != lzo1x_999_compress((const BYTE *) pxBuf,
		sizeof(Pixel) * SGuildMarkBlock::SIZE, m_abCompBuf,
		(lzo_uint*) &m_sizeCompBuf,
		CLZO::Instance().GetWorkMemory()))
#endif
	{
		sys_err("SGuildMarkBlock::Compress: Error! %u > %u", sizeof(Pixel) * SGuildMarkBlock::SIZE, m_sizeCompBuf);
		return;
	}

	//sys_log(0, "SGuildMarkBlock::Compress %u > %u", sizeof(Pixel) * SGuildMarkBlock::SIZE, m_sizeCompBuf);
	m_crc = GetCRC32((const char *) pxBuf, sizeof(Pixel) * SGuildMarkBlock::SIZE);
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
