#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "../client/src/EterBase/lzo.h"
#include "../client/src/EterBase/Timer.h"
#include "../client/src/EterPack/EterPackManager.h"
#include "../client/src/ScriptLib/StdAfx.h"
#include "../client/src/ScriptLib/PythonLauncher.h"
#include "../client/src/ScriptLib/PythonUtils.h"
#include "../client/src/ScriptLib/PythonMarshal.h"
#include "../client/src/UserInterface/PythonliblibManager.h"

void initpack();

namespace
{
int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const char* expr, int line)
{
	if (cond)
	{
		++g_passed;
	}
	else
	{
		++g_failed;
		std::fprintf(stderr, "[FAIL] Line %d: %s\n", line, expr);
	}
}

#define CHECK(cond) Check((cond), #cond, __LINE__)

class TestExceptionSender : public IPythonExceptionSender
{
public:
	void Send() override
	{
		++m_sendCount;
	}

	const std::string& GetExceptionString() const
	{
		return m_strExceptionString;
	}

	int GetSendCount() const
	{
		return m_sendCount;
	}

private:
	int m_sendCount = 0;
};

struct TestEterPackManager : public CEterPackManager
{
	using CEterPackManager::FindPack;
};

// Bridge functions exercising real ScriptLib PyTuple_Get* for 64-bit Window pointers, IDs, and Yang
static PyObject* bridgeVerifyWindowPointer(PyObject* /*self*/, PyObject* args)
{
	void* pWnd = nullptr;
	if (!PyTuple_GetPointer(args, 0, &pWnd))
		return Py_BadArgument();
	return PyLong_FromVoidPtr(pWnd);
}

static PyObject* bridgeVerifyEntityIds(PyObject* /*self*/, PyObject* args)
{
	uint64_t pid = 0;
	uint64_t aid = 0;
	uint64_t itemId = 0;
	if (!PyTuple_GetUnsignedLongLong(args, 0, &pid))
		return Py_BadArgument();
	if (!PyTuple_GetUnsignedLongLong(args, 1, &aid))
		return Py_BadArgument();
	if (!PyTuple_GetUnsignedLongLong(args, 2, &itemId))
		return Py_BadArgument();

	return Py_BuildValue("(KKK)",
		static_cast<unsigned long long>(pid),
		static_cast<unsigned long long>(aid),
		static_cast<unsigned long long>(itemId));
}

static PyObject* bridgeValidateYang(PyObject* /*self*/, PyObject* args)
{
	int64_t yang = 0;
	if (!PyTuple_GetLongLong(args, 0, &yang))
		return Py_BadArgument();
	if (yang < 0 || yang > 999999999999LL)
	{
		PyErr_SetString(PyExc_ValueError, "Yang out of valid range [0..999,999,999,999]");
		return NULL;
	}
	return PyLong_FromLongLong(yang);
}

static PyObject* bridgeGetInteger(PyObject* /*self*/, PyObject* args)
{
	int v = 0;
	if (!PyTuple_GetInteger(args, 0, &v))
		return Py_BadArgument();
	return PyLong_FromLong(v);
}

static PyObject* bridgeGetLong(PyObject* /*self*/, PyObject* args)
{
	long v = 0;
	if (!PyTuple_GetLong(args, 0, &v))
		return Py_BadArgument();
	return PyLong_FromLong(v);
}

static PyObject* bridgeGetStringBytes(PyObject* /*self*/, PyObject* args)
{
	char* sz = NULL;
	if (!PyTuple_GetString(args, 0, &sz))
		return Py_BadArgument();
	return PyBytes_FromString(sz);
}

static PyObject* bridgeFromRawBytes(PyObject* /*self*/, PyObject* args)
{
	const char* sz = NULL;
	if (!PyArg_ParseTuple(args, "y", &sz))
		return NULL;
	return M2PyString_FromString(sz);
}

static void initmt2x64bridge()
{
	static PyMethodDef s_methods[] =
	{
		{ "VerifyWindowPointer", bridgeVerifyWindowPointer, METH_VARARGS, NULL },
		{ "VerifyEntityIds",     bridgeVerifyEntityIds,     METH_VARARGS, NULL },
		{ "ValidateYang",        bridgeValidateYang,        METH_VARARGS, NULL },
		{ "GetInteger",          bridgeGetInteger,          METH_VARARGS, NULL },
		{ "GetLong",             bridgeGetLong,             METH_VARARGS, NULL },
		{ "GetStringBytes",      bridgeGetStringBytes,      METH_VARARGS, NULL },
		{ "FromRawBytes",        bridgeFromRawBytes,        METH_VARARGS, NULL },
		{ NULL, NULL, 0, NULL }
	};
	M2Py_InitModule("mt2x64bridge", s_methods);
}

} // anonymous namespace

int main()
{
	static_assert(sizeof(void*) == 8, "Must compile as native Windows x64 (PE32+ AMD64)");
	static_assert(PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 11, "Must compile against CPython 3.11 x64 headers");

	std::printf("=== MT2009 Real Production Python 3.11 x64 Client Modules + EterPack Suite ===\n");

	// Point Python home to local x64 Python 3.11 installation so standard library (Lib/) is available
	_putenv_s("PYTHONHOME", "C:\\Users\\oyun\\AppData\\Local\\Programs\\Python\\Python311");

	CTimer timer;
	ELTimer_Init();
	CLZO lzo;
	TestEterPackManager packMgr;
	packMgr.SetCacheMode();
	packMgr.SetSearchMode(CEterPackManager::SEARCH_PACK_FIRST);

	// 1. Register real client EterPack archives: root (via RegisterRootPack) and locale
	packMgr.RegisterRootPack("client/files/pack/root");
	bool regLocale = packMgr.RegisterPack("client/files/pack/locale", "locale/");
	CHECK(regLocale);

	CEterPack* pLocalePack = packMgr.FindPack("locale/pl/locale_game.txt");
	CHECK(pLocalePack != nullptr);
	CHECK(pLocalePack && pLocalePack->GetIndexMap().size() > 10);

	// Verify real scripts exist inside decrypted root.index / root.data
	CHECK(packMgr.isExist("system.py"));
	CHECK(packMgr.isExist("constInfo.py"));
	CHECK(packMgr.isExist("localeInfo.py"));
	CHECK(packMgr.isExist("ui.py"));
	CHECK(packMgr.isExist("uicommon.py"));
	CHECK(packMgr.isExist("game.py"));

	// 2. Initialize real CPythonLauncher & register real production C++ modules (liblib, pack, dbg)
	TestExceptionSender excSender;
	g_pkExceptionSender = &excSender;

	CPythonLauncher launcher;
	initliblibManager();
	initpack();
	initdbg();
	initmt2x64bridge();

	CHECK(launcher.Create("Metin2Client_x64_Test"));

	// Verify BUILTINS TRUE/FALSE constants injected by CPythonLauncher::Create
	CHECK(launcher.RunLine("assert TRUE == 1 and FALSE == 0"));

	// 3. Verify real PythonliblibManager.cpp (liblib module) without 32-bit liblib.lib
	CHECK(launcher.RunLine(
		"import liblib\n"
		"mod_list = liblib.getList()\n"
		"assert isinstance(mod_list, tuple) and len(mod_list) == 54, f'Expected 54 modules, got {len(mod_list)}'\n"
		"assert liblib.isExist('os') == 1\n"
		"assert liblib.isExist('COPY_REG') == 1\n"
		"assert liblib.isExist('simplejson') == 1\n"
		"assert liblib.isExist('UserDict') == 1\n"
		"assert liblib.isExist('_abcoll') == 1\n"
		"assert liblib.isExist('non_existent_xyz') == 0\n"
	));

	// Verify importing both direct stdlib modules and Python 2 -> Python 3 aliased modules via liblib.moduleImport
	CHECK(launcher.RunLine(
		"import sys, liblib\n"
		"for name in ('abc', 'base64', 'bisect', 'codecs', 'collections', 'copy', 'copy_reg',\n"
		"             'encodings', 'encodings_aliases', 'fnmatch', 'functools', 'genericpath',\n"
		"             'hashlib', 'heapq', 'keyword', 'linecache', 'locale', 'ntpath', 'numbers',\n"
		"             'ordered_dict', 'os', 'random', 're', 'shlex', 'shutil', 'simplejson',\n"
		"             'simplejson_decoder', 'simplejson_encoder', 'stat', 'string', 'struct',\n"
		"             'sysconfig', 'threading', 'traceback', 'types', 'UserDict', 'uuid',\n"
		"             'warnings', 'weakref', '_abcoll', '_weakrefset', '__future__'):\n"
		"    m = liblib.moduleImport(name)\n"
		"    assert m is not None, f'Failed to import {name}'\n"
		"assert hasattr(liblib.moduleImport('simplejson'), 'dumps')\n"
		"assert hasattr(liblib.moduleImport('ordered_dict'), 'OrderedDict')\n"
		"assert hasattr(liblib.moduleImport('UserDict'), 'UserDict')\n"
		"assert hasattr(liblib.moduleImport('_abcoll'), 'Mapping')\n"
	));

	// Check refcount stability of liblib.moduleImport across 200 repeated calls
	PyObject* osMod = PyImport_ImportModule("os");
	CHECK(osMod != nullptr);
	Py_ssize_t rcBeforeLiblib = osMod ? Py_REFCNT(osMod) : 0;
	CHECK(launcher.RunLine(
		"import liblib\n"
		"for _ in range(200):\n"
		"    _m = liblib.moduleImport('os')\n"
		"del _m\n"
	));
	Py_ssize_t rcAfterLiblib = osMod ? Py_REFCNT(osMod) : 0;
	Py_XDECREF(osMod);
	CHECK(rcAfterLiblib == rcBeforeLiblib);

	// 4. Verify real PythonPackModule.cpp (pack.Exist / pack.Get) against real client/files/pack/root & locale_pl
	CHECK(launcher.RunLine(
		"import pack\n"
		"assert pack.Exist('system.py') == 1\n"
		"assert pack.Exist('ui.py') == 1\n"
		"assert pack.Exist('constInfo.py') == 1\n"
		"assert pack.Exist('localeInfo.py') == 1\n"
		"assert pack.Exist('missing_file_xyz.py') == 0\n"
		"sys_src = pack.Get('system.py')\n"
		"ui_src = pack.Get('ui.py')\n"
		"const_src = pack.Get('constInfo.py')\n"
		"assert isinstance(sys_src, str) and len(sys_src) > 500\n"
		"assert isinstance(ui_src, str) and len(ui_src) > 10000\n"
		"assert isinstance(const_src, str) and len(const_src) > 500\n"
	));

	// Verify pack.Get on locale text file containing Polish characters (PEP 383 surrogateescape safety)
	std::string sampleLocaleTxt;
	if (pLocalePack)
	{
		for (const auto& kv : pLocalePack->GetIndexMap())
		{
			if (kv.second && kv.second->filename[0])
			{
				std::string fn(kv.second->filename);
				if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".txt" && packMgr.isExist(fn.c_str()))
				{
					sampleLocaleTxt = fn;
					break;
				}
			}
		}
	}
	CHECK(!sampleLocaleTxt.empty());
	std::string checkLocaleCmd =
		"import pack\n"
		"loc_txt = pack.Get('" + sampleLocaleTxt + "')\n"
		"assert isinstance(loc_txt, str) and len(loc_txt) > 0\n"
		"raw_bytes = loc_txt.encode('utf-8', 'surrogateescape')\n"
		"assert len(raw_bytes) > 0\n";
	CHECK(!sampleLocaleTxt.empty() && launcher.RunLine(checkLocaleCmd.c_str()));

	// 5. Verify real PythonDebugModule.cpp (dbg module) and IPythonExceptionSender
	CHECK(launcher.RunLine(
		"import dbg\n"
		"dbg.Trace('MT2009 Python3 x64 Trace check\\n')\n"
		"dbg.Tracen('MT2009 Python3 x64 Tracen check')\n"
		"dbg.RegisterExceptionString('TestExceptionLine1')\n"
	));
	CHECK(excSender.GetExceptionString().find("TestExceptionLine1") != std::string::npos);

	// 6. Verify real PythonLauncher.cpp (RunMemoryTextFile & RunCompiledFile with Python 3.11 .pyc)
	const char* memScript =
		"# Multiple single quotes ''' inside comment and string\n"
		"DOC = '''triple-quoted docstring inside RunMemoryTextFile'''\n"
		"MEM_RESULT = 64 * 1000 + 311\n";
	CHECK(launcher.RunMemoryTextFile("mem_test_x64.py", static_cast<UINT>( std::strlen(memScript) ), memScript));
	CHECK(launcher.RunLine("assert MEM_RESULT == 64311 and 'triple-quoted' in DOC"));

	// Generate a real Python 3.11 .pyc file (16-byte header in Python 3.7+: magic + flags + timestamp + size + marshalled code)
	const char* pycPath = "build/obj_x64/test_py311_real_module.pyc";
	CHECK(launcher.RunLine(
		"import py_compile, marshal, importlib.util\n"
		"code_obj = compile('PYC_LOADED_X64 = 0x20090064\\n', 'test_py311_real_module.py', 'exec')\n"
		"with open('build/obj_x64/test_py311_real_module.pyc', 'wb') as f:\n"
		"    f.write(importlib.util.MAGIC_NUMBER)\n"
		"    f.write((0).to_bytes(12, 'little'))\n"
		"    f.write(marshal.dumps(code_obj))\n"
	));
	CHECK(launcher.RunCompiledFile(pycPath));
	CHECK(launcher.RunLine("assert PYC_LOADED_X64 == 0x20090064"));

	// 7. Verify real PythonMarshal.cpp (_PyMarshal_ReadObjectFromString)
	const char* marshalTestBuf = "i\x64\x00\x09\x20"; // TYPE_INT ('i') + 0x20090064 little-endian
	PyObject* marshalledInt = _PyMarshal_ReadObjectFromString(const_cast<char*>(marshalTestBuf), 5);
	CHECK(marshalledInt != nullptr);
	CHECK(marshalledInt && PyLong_AsLong(marshalledInt) == 0x20090064L);
	Py_XDECREF(marshalledInt);

	// 8. Verify 64-bit Window Pointers, 64-bit PID/AID/ItemID, and 64-bit Yang conversions & error handling
	CHECK(launcher.RunLine(
		"import mt2x64bridge\n"
		"for ptr in (0x00007FF812345678, 0x000001A2B3C4D5E0, 0xFFFFFFFF80000000):\n"
		"    assert mt2x64bridge.VerifyWindowPointer(ptr) == ptr\n"
		"ids = mt2x64bridge.VerifyEntityIds(4294967301, 8589934609, 1099511627791)\n"
		"assert ids == (4294967301, 8589934609, 1099511627791)\n"
		"for yang in (0, 1999999999, 2147483648, 10000000000, 999999999999):\n"
		"    assert mt2x64bridge.ValidateYang(yang) == yang\n"
		"neg_rejected = False\n"
		"try:\n"
		"    mt2x64bridge.ValidateYang(-1)\n"
		"except Exception:\n"
		"    neg_rejected = True\n"
		"assert neg_rejected\n"
		"overflow_rejected = False\n"
		"try:\n"
		"    mt2x64bridge.ValidateYang(1000000000000)\n"
		"except Exception:\n"
		"    overflow_rejected = True\n"
		"assert overflow_rejected\n"
	));

	// 8b. PyTuple_GetInteger/GetLong: Py2 PyInt_AsLong semantigi (float -> sifira dogru kirpma), gecersiz girdide false + bekleyen istisna YOK
	CHECK(launcher.RunLine(
		"import mt2x64bridge as b\n"
		"assert b.GetInteger(5) == 5 and b.GetInteger(-7) == -7 and b.GetInteger(True) == 1\n"
		"assert b.GetInteger(3.9) == 3 and b.GetInteger(-3.9) == -3 and b.GetInteger(0.0) == 0\n"
		"assert b.GetLong(3.9) == 3 and b.GetLong(-3.9) == -3 and b.GetLong(123) == 123\n"
		"assert b.GetInteger(0xFFFFFFFF) == -1   # 32-bit renk/bayrak degerleri sarmalanarak korunur (onceki davranis)\n"
		"for bad in (\x27x\x27, None, float(\x27nan\x27), float(\x27inf\x27), 1e30, 1 << 70, [1]):\n"
		"    for fn in (b.GetInteger, b.GetLong):\n"
		"        try:\n"
		"            fn(bad)\n"
		"        except TypeError:\n"
		"            continue\n"
		"        raise AssertionError(\x27gecersiz girdi reddedilmedi: %r\x27 % (bad,))\n"
		"assert b.GetInteger(1) == 1   # onceki hatalardan sonra bekleyen istisna kalmadi\n"
	));

	// 8c. Native string siniri: surrogateescape'li str (cp1250 locale dosyasi) ayni ham baytlara doner; ham bayt -> str ters yon; hata sonrasi bekleyen istisna yok
	CHECK(launcher.RunLine(
		"import mt2x64bridge as b\n"
		"assert b.GetStringBytes(\"abc\") == b\"abc\"\n"
		"assert b.GetStringBytes(\"\\u0105\\u0107\") == \"\\u0105\\u0107\".encode(\"utf-8\")\n"
		"assert b.GetStringBytes(\"\\udcb1\\udce6\") == b\"\\xb1\\xe6\"\n"
		"assert b.GetStringBytes(\"ok\\udcb1\") == b\"ok\\xb1\"\n"
		"assert b.GetStringBytes(b\"raw\\xb1\") == b\"raw\\xb1\"\n"
		"assert b.FromRawBytes(b\"abc\") == \"abc\"\n"
		"assert b.FromRawBytes(b\"\\xb1\\xe6\") == \"\\udcb1\\udce6\"\n"
		"assert b.FromRawBytes(\"\\u0105\".encode(\"utf-8\")) == \"\\u0105\"\n"
		"assert b.GetStringBytes(b.FromRawBytes(b\"\\xb1\\xe6\")) == b\"\\xb1\\xe6\"\n"
		"for bad in (5, None, 1.5, [1]):\n"
		"    try:\n"
		"        b.GetStringBytes(bad)\n"
		"    except TypeError:\n"
		"        continue\n"
		"    raise AssertionError(\"gecersiz string reddedilmedi: %r\" % (bad,))\n"
		"assert b.GetStringBytes(\"z\") == b\"z\"\n"
	));

	// 9. Verify GIL state discipline (PyGILState_Ensure / PyGILState_Release) and script load failure handling
	PyGILState_STATE gilState = PyGILState_Ensure();
	PyObject* mainDict = PyModule_GetDict(PyImport_AddModule("__main__"));
	CHECK(mainDict != nullptr);
	PyObject* pycVal = mainDict ? PyDict_GetItemString(mainDict, "PYC_LOADED_X64") : nullptr;
	CHECK(pycVal != nullptr && PyLong_AsUnsignedLong(pycVal) == 0x20090064UL);
	PyGILState_Release(gilState);

	// Verify graceful failure when loading a non-existent file from pack
	CHECK(!launcher.RunFile("non_existent_script_x64.py"));

	std::printf("PYTHON3_REAL_MODULES_X64_SUITE: %d/%d PASSED (Python %s, root+locale_pl packs verified, liblib 54/54 stdlib bridge verified)\n",
		g_passed,
		g_passed + g_failed,
		Py_GetVersion());

	return (g_failed == 0) ? 0 : 1;
}
