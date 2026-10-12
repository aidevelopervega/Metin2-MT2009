#ifndef __INC_ETERPACKCURSOR_H__
#define __INC_ETERPACKCURSOR_H__

#include "EterPack.h"

class CEterPackCursor
{
	public:
		CEterPackCursor(CEterPack * pack);
		~CEterPackCursor();

		bool		Open(const char* filename);
		void		Close();
		void		Seek(long offset);
		bool		Read(LPVOID data, long size);
		long		Size();

	private:
		CEterPack * m_pPack;
		CMappedFile m_file;
		LPCVOID 	m_pData;
		long		m_ReadPoint;
};

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
