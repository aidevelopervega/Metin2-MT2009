#include "StdAfx.h"
#include "GrpPixelShader.h"
#include "GrpD3DXBuffer.h"
#include "StateManager.h"

CPixelShader::CPixelShader()
{
	Initialize();
}

CPixelShader::~CPixelShader()
{
	Destroy();
}

void CPixelShader::Initialize()
{
	m_handle=0;
}

void CPixelShader::Destroy()
{
	if (m_handle)
	{
		m_handle=0;
	}
}

bool CPixelShader::CreateFromDiskFile(const char* c_szFileName)
{
	Destroy();
	return false;
}

void CPixelShader::Set()
{
	//STATEMANAGER.SetPixelShader(m_handle);
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
