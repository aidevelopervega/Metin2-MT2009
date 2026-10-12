#pragma once

#include "GrpBase.h"

class CPixelShader : public CGraphicBase
{
	public:
		CPixelShader();
		virtual ~CPixelShader();

		void Destroy();
		bool CreateFromDiskFile(const char* c_szFileName);

		void Set();

	protected:
		void Initialize();

	protected:
		DWORD m_handle;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
