#if __has_include(<stdafx.h>)
#include <stdafx.h>
#elif __has_include("../../libthecore/include/stdafx.h")
#include "../../libthecore/include/stdafx.h"
#else
#include "stdafx.h"
#endif
#include <stdio.h>
#include <string.h>
#include "targa.h"

CTargaImage::CTargaImage()
	: m_pbuf(NULL), m_x(0), m_y(0)
{
	memset( &m_header, 0, sizeof(m_header) );
}

CTargaImage::~CTargaImage()
{
    if (m_pbuf)
	delete [] m_pbuf;
}

char * CTargaImage::GetBasePointer(int line)
{
    return m_pbuf + (static_cast<size_t>(m_x) * static_cast<size_t>(line)) * sizeof(DWORD);
}

void CTargaImage::Create(int x, int y)
{
    memset(&m_header, 0, sizeof(m_header));

    m_header.imgType	= 2;
    m_header.width	= static_cast<WORD>(x);
    m_header.height	= static_cast<WORD>(y);
    m_header.colorBits	= 32;
    m_header.desc	= 0x20;

    const size_t bufBytes = (x > 0 && y > 0) ? (static_cast<size_t>(x) * static_cast<size_t>(y) * sizeof(DWORD)) : 0;
    m_pbuf = new char[bufBytes];
    memset(m_pbuf, 0, bufBytes);

    m_x = x;
    m_y = y;
}

bool CTargaImage::Save(const char * filename)
{
    FILE * fp = fopen(filename, "wb");

    if (!fp)
	return false;

    fwrite(&m_header, sizeof(TGA_HEADER), 1, fp);
    const size_t bufBytes = (m_x > 0 && m_y > 0) ? (static_cast<size_t>(m_x) * static_cast<size_t>(m_y) * sizeof(DWORD)) : 0;
    fwrite(m_pbuf, bufBytes, 1, fp);

    fclose(fp);
    return true;
}
