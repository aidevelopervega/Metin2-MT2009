#pragma once

#include "GrpVertexBuffer.h"

class CDynamicVertexBuffer : public CGraphicVertexBuffer
{
	public:
		CDynamicVertexBuffer();
		virtual ~CDynamicVertexBuffer();

		bool Create(int vtxCount, int fvf);

	protected:
		int m_vtxCount;
		int m_fvf;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
