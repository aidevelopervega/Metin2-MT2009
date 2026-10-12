#ifndef __INC_ETERBASE_TEMPFILE_H__
#define __INC_ETERBASE_TEMPFILE_H__

#include "FileBase.h"

class CTempFile : public CFileBase
{
	public:
		CTempFile(const char * c_pszPrefix = NULL);
		virtual ~CTempFile();

	protected:
		char	m_szFileName[MAX_PATH+1];
};

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
