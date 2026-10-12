#pragma once
class PythonCaptcha : public CSingleton<PythonCaptcha>
{
public:
	PythonCaptcha(void);
	virtual ~PythonCaptcha(void);

	typedef std::vector<BYTE> CAPTCHA_IMAGE;

	void SetCaptchaGraphicImage(DWORD pid, CGraphicImage* image);
	void ClearCaptchaGraphicImage(DWORD pid);
	void ClearAllCaptchaGraphicImage();
	CGraphicImage* GetCaptchaGraphicImage(DWORD pid);

	void SetCaptchaImage(CAPTCHA_IMAGE image) { m_CaptchaImage = image; }
	void ClearCaptchaImage();
	CAPTCHA_IMAGE GetCaptchaImage() { return m_CaptchaImage; }

private:
	CAPTCHA_IMAGE m_CaptchaImage;
	std::map<DWORD, CGraphicImage *> m_GraphicImageMap;
};


// Files shared by GameCore.top
