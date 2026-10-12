#include "StdAfx.h"
#include "TempFile.h"
#include "Utils.h"
#include "Debug.h"

CTempFile::~CTempFile()
{
	Destroy();
	DeleteFile(m_szFileName);
}

CTempFile::CTempFile(const char * c_pszPrefix)
{
	strncpy(m_szFileName, CreateTempFileName(c_pszPrefix), MAX_PATH);

	if (!Create(m_szFileName, CFileBase::FILEMODE_WRITE))
	{
		TraceError("CTempFile::CTempFile cannot create temporary file. (filename: %s)", m_szFileName);
		return;
	}
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
