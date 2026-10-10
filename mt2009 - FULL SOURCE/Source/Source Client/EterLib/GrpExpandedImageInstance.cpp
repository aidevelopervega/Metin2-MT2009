#include "StdAfx.h"
#include "../eterBase/CRC32.h"
#include "GrpExpandedImageInstance.h"
#include "StateManager.h"

CDynamicPool<CGraphicExpandedImageInstance>		CGraphicExpandedImageInstance::ms_kPool;

void CGraphicExpandedImageInstance::CreateSystem(UINT uCapacity)
{
	ms_kPool.Create(uCapacity);
}

void CGraphicExpandedImageInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CGraphicExpandedImageInstance* CGraphicExpandedImageInstance::New()
{
	return ms_kPool.Alloc();
}

void CGraphicExpandedImageInstance::Delete(CGraphicExpandedImageInstance* pkImgInst)
{
	pkImgInst->Destroy();
	ms_kPool.Free(pkImgInst);
}

void CGraphicExpandedImageInstance::OnRender(RECT* pClipRect)
{
	CGraphicImage * pImage = m_roImage.GetPointer();
	CGraphicTexture * pTexture = pImage->GetTexturePointer();

	const RECT& c_rRect = pImage->GetRectReference();
	float texReverseWidth = 1.0f / float(pTexture->GetWidth());
	float texReverseHeight = 1.0f / float(pTexture->GetHeight());
	float su = (c_rRect.left - m_RenderingRect.left) * texReverseWidth;
	float sv = (c_rRect.top - m_RenderingRect.top) * texReverseHeight;
	float eu = (c_rRect.left + m_RenderingRect.right + (c_rRect.right-c_rRect.left)) * texReverseWidth;
	float ev = (c_rRect.top + m_RenderingRect.bottom + (c_rRect.bottom-c_rRect.top)) * texReverseHeight;

	float sx = m_v2Position.x - 0.5f;
	float sy = m_v2Position.y - 0.5f;
	float ex = m_v2Position.x - 0.5f;
	float ey = m_v2Position.y - 0.5f;

	if (!m_fRotation)
	{
		sx -= m_RenderingRect.left;
		sy -= m_RenderingRect.top;
		ex += float(pImage->GetWidth()) * m_v2Scale.x + m_RenderingRect.right;
		ey += float(pImage->GetHeight()) * m_v2Scale.y + m_RenderingRect.bottom;
		if ((0.0f < m_v2Scale.x && 0.0f > m_v2Scale.y) || (0.0f > m_v2Scale.x && 0.0f < m_v2Scale.y)) {
			STATEMANAGER.SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}
	}

	if (pClipRect)
	{
		if (m_fRotation)
		{
			TraceError("CGraphicExpandedImageInstance::OnRender You can not use m_fRotation and clip mask at the same time!");
			return;
		}

		const float width = ex - sx;
		const float height = ey - sy;
		const float uDiff = eu - su;
		const float vDiff = ev - sv;

		if (ex < pClipRect->left)
			return;

		if (ey < pClipRect->top)
			return;

		if (sx > pClipRect->right)
			return;

		if (sy > pClipRect->bottom)
			return;

		if (sx < pClipRect->left)
		{
			su += (pClipRect->left - sx) / width * uDiff;
			sx = pClipRect->left;
		}

		if (sy < pClipRect->top)
		{
			sv += (pClipRect->top - sy) / height * vDiff;
			sy = pClipRect->top;
		}

		if (ex > pClipRect->right)
		{
			eu -= (ex - pClipRect->right) / width * uDiff;
			ex = pClipRect->right;
		}

		if (ey > pClipRect->bottom)
		{
			ev -= (ey - pClipRect->bottom) / height * vDiff;
			ey = pClipRect->bottom;
		}
	}

	TPDTVertex vertices[4];
	vertices[0].position.x = sx;
	vertices[0].position.y = sy;
	vertices[0].position.z = 0.0f;
	vertices[0].texCoord = TTextureCoordinate(su, sv);
	vertices[0].diffuse = m_DiffuseColor;

	vertices[1].position.x = ex;
	vertices[1].position.y = sy;
	vertices[1].position.z = 0.0f;
	vertices[1].texCoord = TTextureCoordinate(eu, sv);
	vertices[1].diffuse = m_DiffuseColor;

	vertices[2].position.x = sx;
	vertices[2].position.y = ey;
	vertices[2].position.z = 0.0f;
	vertices[2].texCoord = TTextureCoordinate(su, ev);
	vertices[2].diffuse = m_DiffuseColor;

	vertices[3].position.x = ex;
	vertices[3].position.y = ey;
	vertices[3].position.z = 0.0f;
	vertices[3].texCoord = TTextureCoordinate(eu, ev);
	vertices[3].diffuse = m_DiffuseColor;

	if (m_fRotation)
	{
		float fimgHalfWidth = float(pImage->GetWidth()) / 2.0f * m_v2Scale.x;
		float fimgHalfHeight = float(pImage->GetHeight()) / 2.0f * m_v2Scale.y;

		for (int i = 0; i < 4; ++i)
		{
			vertices[i].position.x += m_v2Origin.x;
			vertices[i].position.y += m_v2Origin.y;
		}

		float fRadian = D3DXToRadian(m_fRotation);
		vertices[0].position.x += (-fimgHalfWidth * cosf(fRadian)) - (-fimgHalfHeight * sinf(fRadian));
		vertices[0].position.y += (-fimgHalfWidth * sinf(fRadian)) + (-fimgHalfHeight * cosf(fRadian));
		vertices[1].position.x += (+fimgHalfWidth * cosf(fRadian)) - (-fimgHalfHeight * sinf(fRadian));
		vertices[1].position.y += (+fimgHalfWidth * sinf(fRadian)) + (-fimgHalfHeight * cosf(fRadian));
		vertices[2].position.x += (-fimgHalfWidth * cosf(fRadian)) - (+fimgHalfHeight * sinf(fRadian));
		vertices[2].position.y += (-fimgHalfWidth * sinf(fRadian)) + (+fimgHalfHeight * cosf(fRadian));
		vertices[3].position.x += (+fimgHalfWidth * cosf(fRadian)) - (+fimgHalfHeight * sinf(fRadian));
		vertices[3].position.y += (+fimgHalfWidth * sinf(fRadian)) + (+fimgHalfHeight * cosf(fRadian));
	}

	switch (m_iRenderingMode)
	{
		case RENDERING_MODE_SCREEN:
		case RENDERING_MODE_COLOR_DODGE:
			STATEMANAGER.SaveRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
			STATEMANAGER.SaveRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			break;
		case RENDERING_MODE_MODULATE:
			STATEMANAGER.SaveRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
			STATEMANAGER.SaveRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
			break;
	}

	if (CGraphicBase::SetPDTStream(vertices, 4))
	{
		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);

		STATEMANAGER.SetTexture(0, pTexture->GetD3DTexture());
		STATEMANAGER.SetTexture(1, NULL);
		STATEMANAGER.SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
		STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);
	}
	//STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, c_FillRectIndices, D3DFMT_INDEX16, vertices, sizeof(TPDTVertex));
	/////////////////////////////////////////////////////////////

	STATEMANAGER.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	STATEMANAGER.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	STATEMANAGER.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	switch (m_iRenderingMode)
	{
		case RENDERING_MODE_SCREEN:
		case RENDERING_MODE_COLOR_DODGE:
		case RENDERING_MODE_MODULATE:
			STATEMANAGER.RestoreRenderState(D3DRS_SRCBLEND);
			STATEMANAGER.RestoreRenderState(D3DRS_DESTBLEND);
			break;
	}
	STATEMANAGER.SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
}

void CGraphicExpandedImageInstance::SetDepth(float fDepth)
{
	m_fDepth = fDepth;
}

void CGraphicExpandedImageInstance::SetOrigin()
{
	SetOrigin(float(GetWidth()) / 2.0f, float(GetHeight()) / 2.0f);
}

void CGraphicExpandedImageInstance::SetOrigin(float fx, float fy)
{
	m_v2Origin.x = fx;
	m_v2Origin.y = fy;
}

void CGraphicExpandedImageInstance::SetRotation(float fRotation)
{
	m_fRotation = fRotation;
}

void CGraphicExpandedImageInstance::SetScale(float fx, float fy)
{
	m_v2Scale.x = fx;
	m_v2Scale.y = fy;
}

void CGraphicExpandedImageInstance::SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom)
{
	if (IsEmpty())
		return;

	float fWidth = float(GetWidth());
	float fHeight = float(GetHeight());

	m_RenderingRect.left = fWidth * fLeft;
	m_RenderingRect.top = fHeight * fTop;
	m_RenderingRect.right = fWidth * fRight;
	m_RenderingRect.bottom = fHeight * fBottom;
}

void CGraphicExpandedImageInstance::SetRenderingMode(int iMode)
{
	m_iRenderingMode = iMode;
}

DWORD CGraphicExpandedImageInstance::Type()
{
	static DWORD s_dwType = GetCRC32("CGraphicExpandedImageInstance", strlen("CGraphicExpandedImageInstance"));
	return (s_dwType);
}

void CGraphicExpandedImageInstance::OnSetImagePointer()
{
	if (IsEmpty())
		return;

	SetOrigin(float(GetWidth()) / 2.0f, float(GetHeight()) / 2.0f);
}

BOOL CGraphicExpandedImageInstance::OnIsType(DWORD dwType)
{
	if (CGraphicExpandedImageInstance::Type() == dwType)
		return TRUE;

	return CGraphicImageInstance::IsType(dwType);
}

void CGraphicExpandedImageInstance::Initialize()
{
	m_iRenderingMode = RENDERING_MODE_NORMAL;
	m_fDepth = 0.0f;
	m_v2Origin.x = m_v2Origin.y = 0.0f;
	m_v2Scale.x = m_v2Scale.y = 1.0f;
	m_fRotation = 0.0f;
	memset(&m_RenderingRect, 0, sizeof(RECT));
}

void CGraphicExpandedImageInstance::Destroy()
{
	CGraphicImageInstance::Destroy();
	Initialize();
}

CGraphicExpandedImageInstance::CGraphicExpandedImageInstance()
{
	Initialize();
}

CGraphicExpandedImageInstance::~CGraphicExpandedImageInstance()
{
	Destroy();
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
