#include "stdafx.h"
#include "file_loader.h"

CMemoryTextFileLoader::CMemoryTextFileLoader()
{
}

CMemoryTextFileLoader::~CMemoryTextFileLoader()
{
}

bool CMemoryTextFileLoader::SplitLine(DWORD dwLine, std::vector<std::string>* pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	size_t basePos = 0;

	do
	{
		size_t beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);
		if (beginPos == std::string::npos)
			return false;

		size_t endPos;

		if (c_rstLine[beginPos] == '#' && c_rstLine.compare(beginPos, 4, "#--#") != 0)
		{
			return false;
		}
		else if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of("\"", beginPos);

			if (endPos == std::string::npos)
				return false;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->emplace_back(c_rstLine.substr(beginPos, endPos == std::string::npos ? std::string::npos : endPos - beginPos));

		if (basePos == std::string::npos || c_rstLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)
			break;
	} while (basePos < c_rstLine.length());

	return true;
}

DWORD CMemoryTextFileLoader::GetLineCount()
{
	return static_cast<DWORD>(m_stLineVector.size());
}

bool CMemoryTextFileLoader::CheckLineIndex(DWORD dwLine)
{
	if (dwLine >= m_stLineVector.size())
		return false;

	return true;
}

const std::string & CMemoryTextFileLoader::GetLineString(DWORD dwLine)
{
	assert(CheckLineIndex(dwLine));
	return m_stLineVector[dwLine];
}

void CMemoryTextFileLoader::Bind(int bufSize, const void* c_pvBuf)
{
	m_stLineVector.clear();

	const char * c_pcBuf = (const char *)c_pvBuf;
	std::string stLine;
	int pos = 0;

	while (pos < bufSize)
	{
		const char c = c_pcBuf[pos++];

		if ('\n' == c || '\r' == c)
		{
			if (pos < bufSize)
				if ('\n' == c_pcBuf[pos] || '\r' == c_pcBuf[pos])
					++pos;

			m_stLineVector.emplace_back(stLine);
			stLine = "";
		}
		else if (c < 0)
		{
			stLine.append(c_pcBuf + (pos-1), 2);
			++pos;
		}
		else
		{
			stLine += c;
		}
	}

	m_stLineVector.emplace_back(stLine);
}

