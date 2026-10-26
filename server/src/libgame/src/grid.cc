#include <string.h>
#include <stdio.h>
#include <algorithm>
#if __has_include(<memcpy.h>)
#include <memcpy.h>
#elif __has_include("../../libthecore/include/memcpy.h")
#include "../../libthecore/include/memcpy.h"
#else
#include "memcpy.h"
#endif

#if __has_include(<stl.h>)
#include <stl.h>
#elif __has_include("../../common/stl.h")
#include "../../common/stl.h"
#else
#include "stl.h"
#endif
#include "grid.h"

CGrid::CGrid(int w, int h) : m_iWidth(w), m_iHeight(h)
{
    const size_t gridSize = (m_iWidth > 0 && m_iHeight > 0) ? (static_cast<size_t>(m_iWidth) * static_cast<size_t>(m_iHeight)) : 0;
    m_pGrid = new char[gridSize];
    memset(m_pGrid, 0, sizeof(char) * gridSize);
}

CGrid::CGrid(CGrid * pkGrid, int w, int h) : m_iWidth(w), m_iHeight(h)
{
    const size_t gridSize = (m_iWidth > 0 && m_iHeight > 0) ? (static_cast<size_t>(m_iWidth) * static_cast<size_t>(m_iHeight)) : 0;
    m_pGrid = new char[gridSize];
    memset(m_pGrid, 0, sizeof(char) * gridSize);
    int iSize = std::min(w * h, pkGrid->m_iWidth * pkGrid->m_iHeight);
    if (iSize > 0)
	thecore_memcpy(m_pGrid, pkGrid->m_pGrid, sizeof(char) * static_cast<size_t>(iSize));
}

CGrid::~CGrid()
{
    delete [] m_pGrid;
}

void CGrid::Clear()
{
    const size_t gridSize = (m_iWidth > 0 && m_iHeight > 0) ? (static_cast<size_t>(m_iWidth) * static_cast<size_t>(m_iHeight)) : 0;
    memset(m_pGrid, 0, sizeof(char) * gridSize);
}

int CGrid::FindBlank(int w, int h)
{
    if (w > m_iWidth || h > m_iHeight)
	return -1;

    int iRow;

    for (iRow = 0; iRow < m_iHeight; ++iRow)
    {
	for (int iCol = 0; iCol < m_iWidth; ++iCol)
	{
	    int iIndex = iRow * m_iWidth + iCol;

	    if (IsEmpty(iIndex, w, h))
		return iIndex;
	}
    }

    return -1;
}

bool CGrid::Put(int iPos, int w, int h)
{
    if (!IsEmpty(iPos, w, h))
	return false;

    for (int y = 0; y < h; ++y)
    {
	int iStart = iPos + (y * m_iWidth);
	m_pGrid[iStart] = true;

	int x = 1;
	while (x < w)
	    m_pGrid[iStart + x++] = true;
    }

    return true;
}

void CGrid::Get(int iPos, int w, int h)
{
    if (iPos < 0 || iPos >= m_iWidth * m_iHeight)
	return;

    for (int y = 0; y < h; ++y)
    {
	int iStart = iPos + (y * m_iWidth);
	m_pGrid[iStart] = false;

	int x = 1;
	while (x < w)
	    m_pGrid[iStart + x++] = false;
    }
}

bool CGrid::IsEmpty(int iPos, int w, int h)
{
    if (iPos < 0)
	return false;

    int iRow = iPos / m_iWidth;

    if (iRow + h > m_iHeight)
	return false;

    if (iPos + w > iRow * m_iWidth + m_iWidth)
	return false;

    for (int y = 0; y < h; ++y)
    {
	int iStart = iPos + (y * m_iWidth);

	if (m_pGrid[iStart])
	    return false;

	int x = 1;
	while (x < w)
	    if (m_pGrid[iStart + x++])
		return false;
    }

    return true;
}

void CGrid::Print()
{
    printf("Grid %p\n", this);

    for (int y = 0; y < m_iHeight; ++y)
    {
	for (int x = 0; x < m_iWidth; ++x)
	    printf("%d", m_pGrid[y * m_iWidth + x]);

	printf("\n");
    }
}

unsigned int CGrid::GetSize()
{
    return (m_iWidth > 0 && m_iHeight > 0) ? static_cast<unsigned int>(m_iWidth * m_iHeight) : 0U;
}
