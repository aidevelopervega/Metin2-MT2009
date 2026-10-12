#include "stdafx.h"
#include "AntiMacro.h"
#include "windows.h"

HHOOK MouseHooks;
HHOOK KeyboardHooks;

AntiMacro* AntiMacro::instance = nullptr;

AntiMacro* AntiMacro::GetInstance(){

	if (!instance)
		instance = new AntiMacro();

	return instance;

}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam){

	if (nCode == HC_ACTION){

		if (wParam == WM_RBUTTONDOWN || wParam == WM_LBUTTONDOWN){

			MSLLHOOKSTRUCT* hookStruct = (MSLLHOOKSTRUCT*)lParam;

			if ((hookStruct->flags & LLMHF_INJECTED) == LLMHF_INJECTED)
				return TRUE;

			if ((hookStruct->flags & LLMHF_LOWER_IL_INJECTED) == LLMHF_LOWER_IL_INJECTED)
				return TRUE;

		}

	}

	return CallNextHookEx(MouseHooks, nCode, wParam, lParam);

}

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam){

	if (nCode == HC_ACTION){

		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN){

			KBDLLHOOKSTRUCT* hookStruct = (KBDLLHOOKSTRUCT*)lParam;

			if ((hookStruct->flags & LLKHF_INJECTED) == LLKHF_INJECTED)
				return TRUE;

			if ((hookStruct->flags & LLKHF_LOWER_IL_INJECTED) == LLKHF_LOWER_IL_INJECTED)
				return TRUE;

		}
	}

	return CallNextHookEx(KeyboardHooks, nCode, wParam, lParam);

}

void AntiMacro::ThreadFunc() {

	HINSTANCE hMod = GetModuleHandle(NULL);

	MouseHooks = SetWindowsHookExA(WH_MOUSE_LL, MouseHookProc, hMod, NULL);
	KeyboardHooks = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardHookProc, hMod, NULL);

	if (MouseHooks || KeyboardHooks){

		MSG message;
		while (GetMessageA(&message, NULL, 0, 0)){
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}

	}

	if (MouseHooks) {
		UnhookWindowsHookEx(MouseHooks);
		MouseHooks = NULL;
	}

	if (KeyboardHooks) {
		UnhookWindowsHookEx(KeyboardHooks);
		KeyboardHooks = NULL;
	}

}

void AntiMacro::Initialize() {
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, NULL, 0, NULL);
}

// Files shared by GameCore.top
