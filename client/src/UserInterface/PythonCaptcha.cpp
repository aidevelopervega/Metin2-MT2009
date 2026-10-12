#include "StdAfx.h"
#include "PythonCaptcha.h"
#include "../EterPythonLib/PythonWindow.h"

PythonCaptcha::PythonCaptcha(void)
{
	m_GraphicImageMap.clear();
}

PythonCaptcha::~PythonCaptcha(void)
{
}

void PythonCaptcha::SetCaptchaGraphicImage(DWORD pid, CGraphicImage* image)
{
	if (GetCaptchaGraphicImage(pid))
		ClearCaptchaGraphicImage(pid);

	m_GraphicImageMap[pid] = image;
}

void PythonCaptcha::ClearCaptchaGraphicImage(DWORD pid)
{
	auto it = m_GraphicImageMap.find(pid);
	if (it != m_GraphicImageMap.end())
	{
		if (it->second && it->second->canDestroy())
		{
			it->second->DestroyDeviceObjects();
			delete it->second;
		}
		
		m_GraphicImageMap.erase(pid);
	}
}

void PythonCaptcha::ClearAllCaptchaGraphicImage()
{
	for (auto it = m_GraphicImageMap.begin(); it != m_GraphicImageMap.end(); ++it)
	{
		if (it->second && it->second->canDestroy())
		{
			it->second->DestroyDeviceObjects();
			delete it->second;
		}
	}

	m_GraphicImageMap.clear();
}

CGraphicImage* PythonCaptcha::GetCaptchaGraphicImage(DWORD pid)
{
	auto it = m_GraphicImageMap.find(pid);
	if (it != m_GraphicImageMap.end())
	{
		return it->second;
	}
	return nullptr;
}

void PythonCaptcha::ClearCaptchaImage()
{
	m_CaptchaImage.clear();
}

bool PyTuple_GetWindow2(PyObject* poArgs, int pos, UI::CWindow** ppRetWindow)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, pos, &iHandle))
		return false;
	if (!iHandle)
		return false;

	*ppRetWindow = (UI::CWindow*)iHandle;
	return true;
}

PyObject* captchaSetCaptchaImage(PyObject* poSelf, PyObject* poArgs)
{
	DWORD pid;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &pid))
		return Py_BuildException();

	UI::CWindow* pWindow;
	if (!PyTuple_GetWindow2(poArgs, 1, &pWindow))
		return Py_BuildException();
	
	auto captchaMgr = &PythonCaptcha::instance();
	CGraphicImage* pkCaptchaImage = captchaMgr->GetCaptchaGraphicImage(pid);

	if (pkCaptchaImage == nullptr)
	{
		pkCaptchaImage = new CGraphicImage("captcha.png", D3DX_FILTER_LINEAR);
		captchaMgr->SetCaptchaGraphicImage(pid, pkCaptchaImage);
	}
	else
	{
		pkCaptchaImage->OnClear();
	}

    auto imgData = PythonCaptcha::instance().GetCaptchaImage();
    if (!pkCaptchaImage->OnLoad(imgData.size(), imgData.data()))
    {
        return Py_BuildException();
    }

	PythonCaptcha::instance().ClearCaptchaImage();

	auto imageWnd = static_cast<UI::CImageBox*>(pWindow);
    if (!imageWnd || !imageWnd->LoadImageFromGraphicImage(pkCaptchaImage))
    {
        return Py_BuildException();
    }

	return Py_BuildNone();
}

PyObject* captchaClearCaptchaImage(PyObject* poSelf, PyObject* poArgs)
{
	DWORD pid;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &pid))
		return Py_BuildException();

	PythonCaptcha::instance().ClearCaptchaImage();
	PythonCaptcha::instance().ClearCaptchaGraphicImage(pid);
	return Py_BuildNone();
}

PyObject* captchaClearAllCaptchaImage(PyObject* poSelf, PyObject* poArgs)
{
	PythonCaptcha::instance().ClearAllCaptchaGraphicImage();
	return Py_BuildNone();
}

void initcaptcha()
{
	static PyMethodDef s_methods[] =
	{
		{ "SetCaptchaImage",					captchaSetCaptchaImage,						METH_VARARGS },
		{ "ClearCaptchaImage",					captchaClearCaptchaImage,					METH_VARARGS },
		{ "ClearAllCaptchaImage",					captchaClearAllCaptchaImage,					METH_VARARGS },

		{ NULL,										NULL,										NULL },
	};

	PyObject* poModule = Py_InitModule("captcha", s_methods);
	PyModule_AddIntConstant(poModule, "OK", CAPTCHA_OK);
	PyModule_AddIntConstant(poModule, "NO_ATTEMPTS", CAPTCHA_NO_ATTEMPTS);
	PyModule_AddIntConstant(poModule, "WRONG_CODE", CAPTCHA_WRONG_CODE);
	PyModule_AddIntConstant(poModule, "EXPIRE", CAPTCHA_EXPIRE);
	PyModule_AddIntConstant(poModule, "REQUEST_CHANGE", CAPTCHA_REQUEST_CHANGE);
	PyModule_AddIntConstant(poModule, "DISCONNECT", CAPTCHA_DISCONNECT);
}
// Files shared by GameCore.top
