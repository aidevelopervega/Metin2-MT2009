#pragma once

#include "GrpBase.h"

class CVertexShader : public CGraphicBase
{
	public:
		CVertexShader();
		virtual ~CVertexShader();

		void Destroy();
		bool CreateFromDiskFile(const char* c_szFileName, const DWORD* c_pdwVertexDecl);

		void Set();

	protected:
		void Initialize();

	protected:
		DWORD m_handle;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
