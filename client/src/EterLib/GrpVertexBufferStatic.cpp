#include "StdAfx.h"
#include "GrpVertexBufferStatic.h"

bool CStaticVertexBuffer::Create(int vtxCount, DWORD fvf, bool /*isManaged*/)
{
	return CGraphicVertexBuffer::Create(vtxCount, fvf, D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED);
}

CStaticVertexBuffer::CStaticVertexBuffer()
{
}

CStaticVertexBuffer::~CStaticVertexBuffer()
{
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
