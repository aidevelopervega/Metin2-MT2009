// MT2009 - Gomulu CPython 3.11.9 x64 + GERCEK EterPack/CPythonLauncher/pack/dbg/liblib calisma zamani harness'i.
// Kullanim: test_python311_pack_runtime_x64.exe <pack_dir(root.data/index + locale.data/index)> <staging_dir> <stubs.py> <driver.py>
// Python home AYARLANMAZ: istemcideki gibi sys.path = [exe_dir, exe_dir\Lib] ve stdlib yalnizca lib\ icindeki kaynaksiz .pyc'lerden gelir.
// Yerel istemci modulleri (app, wndMgr, player, ...) python311_native_stubs.py ile MOCKED_NATIVE'dir.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>

#include "../client/src/EterBase/lzo.h"
#include "../client/src/EterBase/Timer.h"
#include "../client/src/EterPack/EterPackManager.h"
#include "../client/src/ScriptLib/StdAfx.h"
#include "../client/src/ScriptLib/PythonLauncher.h"
#include "../client/src/ScriptLib/PythonUtils.h"
#include "../client/src/UserInterface/PythonliblibManager.h"

void initpack();

namespace
{
class TestExceptionSender : public IPythonExceptionSender
{
public:
	void Send() override { ++m_count; }
	int m_count = 0;
};

bool ReadAll(const char* path, std::string* out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	*out = ss.str();
	return true;
}

volatile LONG g_messageBoxes = 0;

BOOL CALLBACK CollectText(HWND h, LPARAM lp)
{
	char buf[4096];
	if (GetWindowTextA(h, buf, sizeof(buf)) > 0)
	{
		std::string* s = reinterpret_cast<std::string*>(lp);
		*s += buf;
		*s += " | ";
	}
	return TRUE;
}

BOOL CALLBACK FindDialogs(HWND h, LPARAM)
{
	DWORD pid = 0;
	GetWindowThreadProcessId(h, &pid);
	char cls[64];
	if (pid == GetCurrentProcessId() && GetClassNameA(h, cls, sizeof(cls)) && std::strcmp(cls, "#32770") == 0 && IsWindowVisible(h))
	{
		std::string text;
		CollectText(h, reinterpret_cast<LPARAM>(&text));
		EnumChildWindows(h, CollectText, reinterpret_cast<LPARAM>(&text));
		std::printf("MESSAGEBOX_CAPTURED: %.1500s\n", text.c_str());
		InterlockedIncrement(&g_messageBoxes);
		PostMessage(h, WM_CLOSE, 0, 0);
	}
	return TRUE;
}

// Uretim kodundaki Traceback()/LogBoxf modal MessageBox acar; basliksiz testte kilitlenmeyi onlemek icin yakalanip kapatilir.
DWORD WINAPI MessageBoxWatcher(LPVOID)
{
	for (;;)
	{
		EnumWindows(FindDialogs, 0);
		Sleep(100);
	}
}
} // namespace

int main(int argc, char** argv)
{
	static_assert(sizeof(void*) == 8, "native x64 gerekli");
	static_assert(PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 11, "CPython 3.11 basliklari gerekli");
	if (argc != 5)
	{
		std::fprintf(stderr, "usage: %s <pack_dir> <staging_dir> <stubs.py> <driver.py>\n", argv[0]);
		return 64;
	}
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	_putenv_s("PYTHONHOME", "");
	_putenv_s("PYTHONPATH", "");
	_putenv_s("PYTHONNOUSERSITE", "1");
	std::printf("HARNESS start\n");
	CreateThread(nullptr, 0, MessageBoxWatcher, nullptr, 0, nullptr);

	std::string stubs, driver;
	if (!ReadAll(argv[3], &stubs) || !ReadAll(argv[4], &driver))
	{
		std::fprintf(stderr, "cannot read stubs/driver\n");
		return 65;
	}

	CTimer timer;
	ELTimer_Init();
	CLZO lzo;
	CEterPackManager packMgr;
	packMgr.SetCacheMode();
	packMgr.SetSearchMode(CEterPackManager::SEARCH_PACK_FIRST);
	const std::string packDir = argv[1];
	packMgr.RegisterRootPack((packDir + "/root").c_str());
	if (!packMgr.RegisterPack((packDir + "/locale").c_str(), "locale/") || !packMgr.isExist("system.py"))
	{
		std::fprintf(stderr, "pack registration failed\n");
		return 66;
	}
	std::printf("HARNESS packs registered\n");

	TestExceptionSender excSender;
	g_pkExceptionSender = &excSender;
	CPythonLauncher launcher;
	std::printf("HARNESS python initialized: %s\n", Py_GetVersion());
	initliblibManager();
	initpack();
	initdbg();
	std::printf("HARNESS real modules liblib/pack/dbg registered\n");
	if (!launcher.Create("Metin2Client_x64_Test"))
	{
		std::fprintf(stderr, "launcher.Create failed\n");
		return 67;
	}
	std::printf("HARNESS launcher created\n");

	// UserInterface.cpp RunMainScript ile ayni builtins
	PyObject* builtins = M2Py_ImportBuiltins();
	PyModule_AddIntConstant(builtins, "__DEBUG__", 0);
	PyModule_AddIntConstant(builtins, "__USE_CYTHON__", 0);
	PyModule_AddIntConstant(builtins, "__USE_EXTRA_CYTHON__", 0);
	PyModule_AddStringConstant(builtins, "__COMMAND_LINE__", "");
	PyModule_AddStringConstant(builtins, "__MT2_STAGING__", argv[2]);
	char exe[MAX_PATH];
	GetModuleFileNameA(nullptr, exe, MAX_PATH);
	std::string libDir(exe);
	libDir = libDir.substr(0, libDir.find_last_of("\\/")) + "\\lib";
	PyModule_AddStringConstant(builtins, "__MT2_LIBDIR__", libDir.c_str());

	auto runGuarded = [&](const char* path) -> bool {
		std::string boot = std::string("import sys, traceback, io\n__MT2_GUARD_OK__ = 0\ntry:\n    exec(compile(io.open(r'") + path +
			"', 'rb').read().decode('utf-8'), r'" + path + "', 'exec'), globals())\n    __MT2_GUARD_OK__ = 1\nexcept BaseException:\n"
			"    traceback.print_exc(file=sys.__stderr__)\n    sys.__stderr__.flush()\n";
		return launcher.RunLine(boot.c_str());
	};
	auto guardOk = [&]() -> bool {
		PyGILState_STATE g = PyGILState_Ensure();
		PyObject* main = PyImport_AddModule("__main__");
		PyObject* v = main ? PyObject_GetAttrString(main, "__MT2_GUARD_OK__") : nullptr;
		bool r = v && PyLong_AsLong(v) == 1;
		Py_XDECREF(v);
		PyErr_Clear();
		PyGILState_Release(g);
		return r;
	};

	bool ok = runGuarded(argv[3]) && guardOk();
	std::printf("HARNESS stubs rc=%d\n", ok ? 1 : 0);
	// CPythonLauncher::RunFile("system.py") ile ayni islem (Get -> CR temizle -> compile -> __main__ icinde exec); fark: Python seviyesinde
	// tam traceback (uretim Traceback() py3'te istisna metnini kaybeder ve modal MessageBox acar).
	bool sysOk = false;
	if (ok)
	{
		launcher.RunLine("import sys, traceback, pack\n__MT2_GUARD_OK__ = 0\ntry:\n"
			"    exec(compile(pack.Get('system.py').replace('\\r', ''), 'system.py', 'exec'), globals())\n    __MT2_GUARD_OK__ = 1\n"
			"except BaseException:\n    traceback.print_exc(file=sys.__stderr__)\n    sys.__stderr__.flush()\n");
		sysOk = guardOk();
	}
	std::printf("HARNESS system.py (pack -> prototype.py) rc=%d\n", sysOk ? 1 : 0);
	PyModule_AddIntConstant(builtins, "__MT2_SYSTEM_RC__", sysOk ? 1 : 0);
	Py_XDECREF(builtins);

	bool drvOk = ok && runGuarded(argv[4]) && guardOk();
	int failed = -1;
	{
		PyGILState_STATE g = PyGILState_Ensure();
		PyObject* b = M2Py_ImportBuiltins();
		PyObject* v = b ? PyObject_GetAttrString(b, "__MT2_FAILED__") : nullptr;
		if (v)
			failed = static_cast<int>(PyLong_AsLong(v));
		else
			PyErr_Clear();
		Py_XDECREF(v);
		Py_XDECREF(b);
		PyGILState_Release(g);
	}
	std::printf("PYTHON311_PACK_RUNTIME_X64: driver_rc=%d failed_levels=%d messageboxes=%ld python=%s sizeof_void_p=%zu\n", drvOk ? 1 : 0, failed, static_cast<long>(g_messageBoxes), Py_GetVersion(), sizeof(void*));
	return (sysOk && drvOk && failed == 0 && g_messageBoxes == 0) ? 0 : 1;
}
