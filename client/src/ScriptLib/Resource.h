#pragma once

#include "../EffectLib/StdAfx.h"
#include "../eterlib/Resource.h"
#include "../eterlib/ResourceManager.h"

enum EResourceTypes
{
	RES_TYPE_UNKNOWN,
};

class CPythonResource : public CSingleton<CPythonResource>
{
	public:
		CPythonResource();
		virtual ~CPythonResource();

		void Destroy();

		void DumpFileList(const char * c_szFileName);

	protected:
		CResourceManager m_resManager;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
