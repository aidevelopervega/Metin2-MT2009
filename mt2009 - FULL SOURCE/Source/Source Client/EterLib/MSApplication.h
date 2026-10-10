#pragma once

#include "MSWindow.h"

class CMSApplication : public CMSWindow
{
	public:
		CMSApplication();
		virtual ~CMSApplication();

		void Initialize(HINSTANCE hInstance);

		void MessageLoop();

		bool IsMessage();
		bool MessageProcess();

	protected:
		void ClearWindowClass();

		LRESULT WindowProcedure(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
