#pragma once

#include "GrpBase.h"

class CGraphicDib;

class CBlockTexture : public CGraphicBase
{
	public:
		CBlockTexture();
		virtual ~CBlockTexture();

		bool Create(CGraphicDib * pDIB, const RECT & c_rRect, DWORD dwWidth, DWORD dwHeight);
		void SetClipRect(const RECT & c_rRect);
		void Render(int ix, int iy);
		void InvalidateRect(const RECT & c_rsrcRect);

	protected:
		CGraphicDib * m_pDIB;
		RECT m_rect;
		RECT m_clipRect;
		BOOL m_bClipEnable;
		DWORD m_dwWidth;
		DWORD m_dwHeight;
		LPDIRECT3DTEXTURE9 m_lpd3dTexture;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
