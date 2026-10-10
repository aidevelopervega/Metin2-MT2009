#pragma once

#include "GrpVertexBuffer.h"

class CStaticVertexBuffer : public CGraphicVertexBuffer
{
	public:
		CStaticVertexBuffer();
		virtual ~CStaticVertexBuffer();

		bool Create(int vtxCount, DWORD fvf, bool isManaged=true);
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
