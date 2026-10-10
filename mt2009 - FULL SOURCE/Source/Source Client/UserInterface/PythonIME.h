#pragma once

#include "../eterBase/Singleton.h"
#include "../eterlib/IME.h"

class CPythonIME :
	public IIMEEventSink,
	public CIME,
	public CSingleton<CPythonIME>
{
public:
	CPythonIME();
	virtual ~CPythonIME();

	void MoveLeft();
	void MoveRight();
	void MoveHome();
	void MoveEnd();
	void SetCursorPosition(int iPosition);
	void Delete();

	void Create(HWND hWnd);

protected:
	virtual void OnTab();
	virtual void OnReturn();
	virtual void OnEscape();

	virtual bool OnWM_CHAR( WPARAM wParam, LPARAM lParam );
	virtual void OnUpdate();
	virtual void OnChangeCodePage();
	virtual void OnOpenCandidateList();
	virtual void OnCloseCandidateList();
	virtual void OnOpenReadingWnd();
	virtual void OnCloseReadingWnd();
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
