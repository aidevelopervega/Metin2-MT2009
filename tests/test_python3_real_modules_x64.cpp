#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "../client/src/EterBase/lzo.h"
#include "../client/src/EterBase/tea.h"
#include "../client/src/EterBase/Timer.h"
#include "../client/src/EterPack/EterPackManager.h"
#include "../client/src/ScriptLib/StdAfx.h"
#include "../client/src/ScriptLib/PythonLauncher.h"
#include "../client/src/ScriptLib/PythonUtils.h"
#include "../client/src/ScriptLib/PythonMarshal.h"
#include "../client/src/MilesLib/SoundManager.h"
#include "../client/src/UserInterface/StdAfx.h"
#include "../client/src/UserInterface/PythonliblibManager.h"
#include "../client/src/UserInterface/PythonExchange.h"
#include "../client/src/UserInterface/PythonSafeBox.h"

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

static PyObject* bridgeGetUnsignedLong(PyObject* /*self*/, PyObject* args)
{
	unsigned long v = 0;
	if (!PyTuple_GetUnsignedLong(args, 0, &v))
		return Py_BadArgument();
	return PyLong_FromUnsignedLong(v);
}

static PyObject* bridgeGetUnsignedInteger(PyObject* /*self*/, PyObject* args)
{
	unsigned int v = 0;
	if (!PyTuple_GetUnsignedInteger(args, 0, &v))
		return Py_BadArgument();
	return PyLong_FromUnsignedLong(v);
}

static PyObject* bridgeGetBoolean(PyObject* /*self*/, PyObject* args)
{
	bool v = false;
	if (!PyTuple_GetBoolean(args, 0, &v))
		return Py_BadArgument();
	return PyBool_FromLong(v ? 1 : 0);
}

static PyObject* bridgeGetFloat(PyObject* /*self*/, PyObject* args)
{
	float v = 0.0f;
	if (!PyTuple_GetFloat(args, 0, &v))
		return Py_BadArgument();
	return PyFloat_FromDouble(static_cast<double>(v));
}

static PyObject* bridgeGetDouble(PyObject* /*self*/, PyObject* args)
{
	double v = 0.0;
	if (!PyTuple_GetDouble(args, 0, &v))
		return Py_BadArgument();
	return PyFloat_FromDouble(v);
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

// Exact hardened implementation of appLoadLocaleAddr from client/src/UserInterface/PythonApplicationModule.cpp
static PyObject* bridgeLoadLocaleAddr(PyObject* /*poSelf*/, PyObject* poArgs)
{
	char* addrPath;
	if (!PyTuple_GetString(poArgs, 0, &addrPath))
		return Py_BuildException();

	FILE* fp = fopen(addrPath, "rb");
	if (!fp)
		return Py_BuildException();

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return Py_BuildException();
	}

	const long fileSize = ftell(fp);
	if (fileSize < static_cast<long>(sizeof(unsigned int)) || fileSize > 16 * 1024 * 1024)
	{
		fclose(fp);
		return Py_BuildException();
	}

	if (fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);
		return Py_BuildException();
	}

	const size_t size = static_cast<size_t>(fileSize);
	const size_t paddedSize = (size % 8 != 0) ? (size + 8 - (size % 8)) : size;
	std::vector<char> enc(paddedSize, 0);
	if (fread(enc.data(), 1, size, fp) != size)
	{
		fclose(fp);
		return Py_BuildException();
	}
	fclose(fp);

	static const unsigned char key[16] = {
		0x82, 0x1b, 0x34, 0xae,
		0x12, 0x3b, 0xfb, 0x17,
		0xd7, 0x2c, 0x39, 0xae,
		0x41, 0x98, 0xf1, 0x63
	};

	std::vector<char> buf(paddedSize, 0);
	tea_decrypt(reinterpret_cast<unsigned long*>(buf.data()),
		reinterpret_cast<const unsigned long*>(enc.data()),
		reinterpret_cast<const unsigned long*>(key),
		static_cast<int>(size));

	unsigned int retSize = 0;
	memcpy(&retSize, buf.data(), sizeof(unsigned int));
	if (retSize > size - sizeof(unsigned int))
		return Py_BuildException();

	const char* ret = buf.data() + sizeof(unsigned int);
	return Py_BuildValue("s#", ret, static_cast<Py_ssize_t>(retSize));
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
		{ "GetUnsignedLong",     bridgeGetUnsignedLong,     METH_VARARGS, NULL },
		{ "GetUnsignedInteger",  bridgeGetUnsignedInteger,  METH_VARARGS, NULL },
		{ "GetBoolean",          bridgeGetBoolean,          METH_VARARGS, NULL },
		{ "GetFloat",            bridgeGetFloat,            METH_VARARGS, NULL },
		{ "GetDouble",           bridgeGetDouble,           METH_VARARGS, NULL },
		{ "GetStringBytes",      bridgeGetStringBytes,      METH_VARARGS, NULL },
		{ "FromRawBytes",        bridgeFromRawBytes,        METH_VARARGS, NULL },
		{ "LoadLocaleAddr",      bridgeLoadLocaleAddr,      METH_VARARGS, NULL },
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
	CSoundManager soundMgr;
	CPythonExchange exchangeMgr;
	CPythonSafeBox safeboxMgr;
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

	// 2. Initialize real CPythonLauncher & register 7 real production C++ modules (liblib, pack, dbg, profiler, snd, exchange, safebox)
	TestExceptionSender excSender;
	g_pkExceptionSender = &excSender;

	CPythonLauncher launcher;
	initliblibManager();
	initpack();
	initdbg();
	initProfiler();
	initsnd();
	initTrade();
	initsafebox();
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

	// 8d. Native -> Python dizge siniri (M2Py_BuildValue): UTF-8/ASCII girdide gercek Py_BuildValue ile birebir ayni; cp1250 ham baytta surrogateescape ile basarili
	{
		PyGILState_STATE gil8d = PyGILState_Ensure();
		auto same = [](PyObject* a, PyObject* b) -> bool
		{
			bool ok = (a != nullptr && b != nullptr && Py_TYPE(a) == Py_TYPE(b) && PyObject_RichCompareBool(a, b, Py_EQ) == 1);
			Py_XDECREF(a);
			Py_XDECREF(b);
			PyErr_Clear();
			return ok;
		};
		const char* c_szUtf8 = "Ala ma kota \xC4\x85\xC4\x87";
		const char* c_szCp1250 = "ok\xB1\xE6";   // cp1250 ham baytlar (gecersiz UTF-8)

		CHECK(same(Py_BuildValue("s", "abc"), (Py_BuildValue)("s", "abc")));
		CHECK(same(Py_BuildValue("s", c_szUtf8), (Py_BuildValue)("s", c_szUtf8)));
		CHECK(same(Py_BuildValue("(is)", 7, "x"), (Py_BuildValue)("(is)", 7, "x")));
		CHECK(same(Py_BuildValue("(s)", "x"), (Py_BuildValue)("(s)", "x")));
		CHECK(same(Py_BuildValue("si", "x", -5), (Py_BuildValue)("si", "x", -5)));
		CHECK(same(Py_BuildValue("(iss)", 1, "a", "b"), (Py_BuildValue)("(iss)", 1, "a", "b")));
		CHECK(same(Py_BuildValue("(isss)", 1, "a", "b", "c"), (Py_BuildValue)("(isss)", 1, "a", "b", "c")));
		CHECK(same(Py_BuildValue("(sissii)", "a", 1, "b", "c", 2, 3), (Py_BuildValue)("(sissii)", "a", 1, "b", "c", 2, 3)));
		CHECK(same(Py_BuildValue("(KsKsi)", 4294967301ULL, "p", 8589934609ULL, "q", 9), (Py_BuildValue)("(KsKsi)", 4294967301ULL, "p", 8589934609ULL, "q", 9)));
		CHECK(same(Py_BuildValue("(iiiiiLss)", 1, 2, 3, 4, 5, 999999999999LL, "u", "v"), (Py_BuildValue)("(iiiiiLss)", 1, 2, 3, 4, 5, 999999999999LL, "u", "v")));
		CHECK(same(Py_BuildValue("isiilK", 1, "n", 2, 3, -4L, 5ULL), (Py_BuildValue)("isiilK", 1, "n", 2, 3, -4L, 5ULL)));
		CHECK(same(Py_BuildValue("sff", "t", 1.5f, -2.25f), (Py_BuildValue)("sff", "t", 1.5f, -2.25f)));
		CHECK(same(Py_BuildValue("iifs", 1, 2, 0.5f, "z"), (Py_BuildValue)("iifs", 1, 2, 0.5f, "z")));
		CHECK(same(Py_BuildValue("s#", "abcdef", static_cast<Py_ssize_t>(3)), PyUnicode_FromString("abc")));
		CHECK(same(Py_BuildValue("z#", static_cast<const char*>(nullptr), static_cast<Py_ssize_t>(4)), Py_NewRef(Py_None)));
		CHECK(same(Py_BuildValue("s", static_cast<const char*>(nullptr)), Py_NewRef(Py_None)));
		CHECK(same(Py_BuildValue("z", static_cast<const char*>(nullptr)), Py_NewRef(Py_None)));
		// Varargs tam sayi terfileri (b/B/h/H/i/I/l/k/L/K/n) ve ic ice demetler: gercek CPython Py_BuildValue / _Py_BuildValue_SizeT ile birebir esit
		CHECK(same(Py_BuildValue("(sbBhH)", "p", static_cast<int>(-1), static_cast<int>(255), static_cast<int>(-2), static_cast<int>(65535)),
		           (Py_BuildValue)("(sbBhH)", "p", static_cast<int>(-1), static_cast<int>(255), static_cast<int>(-2), static_cast<int>(65535))));
		CHECK(same(Py_BuildValue("((s(ii))(z#K))", "in", 10, -20, "xyz99", static_cast<Py_ssize_t>(3), 4294967301ULL),
		           _Py_BuildValue_SizeT("((s(ii))(z#K))", "in", 10, -20, "xyz99", static_cast<Py_ssize_t>(3), 4294967301ULL)));
		CHECK(same(Py_BuildValue("(s, i: n)", "sep", -7, static_cast<Py_ssize_t>(123456)),
		           (Py_BuildValue)("(s, i: n)", "sep", -7, static_cast<Py_ssize_t>(123456))));
		// Gomulu NUL (\0) iceren s# / z# dizgesi
		{
			const char c_ab0cd[] = { 'a', 'b', '\0', 'c', '\xB1', 'd' };
			PyObject* poEmb = Py_BuildValue("s#", c_ab0cd, static_cast<Py_ssize_t>(6));
			CHECK(poEmb != nullptr && PyUnicode_GET_LENGTH(poEmb) == 6);
			char* c_szEmbBack = poEmb ? M2PyString_AsString(poEmb) : nullptr;
			CHECK(c_szEmbBack != nullptr && std::memcmp(c_szEmbBack, c_ab0cd, 6) == 0);
			Py_XDECREF(poEmb);
		}
		// Negatif s#/z# uzunlugu SystemError uretmeli (strlen'e dusmemeli)
		{
			PyObject* poNeg1 = Py_BuildValue("s#", "abc", static_cast<Py_ssize_t>(-1));
			CHECK(poNeg1 == nullptr && PyErr_ExceptionMatches(PyExc_SystemError));
			PyErr_Clear();
			PyObject* poNeg2 = Py_BuildValue("z#", static_cast<const char*>(nullptr), static_cast<Py_ssize_t>(-5));
			CHECK(poNeg2 == nullptr && PyErr_ExceptionMatches(PyExc_SystemError));
			PyErr_Clear();
		}
		// O/S/N referans sahipligi, hata yolunda N DECREF (hata oncesi, hata sonrasi ve ic ice demet) ve mevcut istisna korunumu
		{
			PyObject* poKeep = PyList_New(0);
			PyObject* poSteal = PyList_New(0);
			PyObject* poMine = Py_BuildValue("sNsi", "a", poSteal, "b", 3);
			CHECK(poMine != nullptr && PyTuple_Size(poMine) == 4 && PyTuple_GET_ITEM(poMine, 1) == poSteal);
			Py_ssize_t rcKeep = Py_REFCNT(poKeep);
			PyObject* poO = Py_BuildValue("(sOS)", "k", poKeep, poKeep);
			CHECK(poO != nullptr && Py_REFCNT(poKeep) == rcKeep + 2);
			Py_XDECREF(poO);
			CHECK(Py_REFCNT(poKeep) == rcKeep);

			// 1) Hata veren O'dan ONCE gelen N: DECREF edilmeli
			PyObject* poBefore = PyList_New(0);
			Py_INCREF(poBefore); // refcnt = 2; M2Py_BuildValue 1 referansi calmali ve hata yolunda dusurmeli -> refcnt = 1 kalmali
			PyObject* poFail1 = Py_BuildValue("(sNO)", "x", poBefore, static_cast<PyObject*>(nullptr));
			CHECK(poFail1 == nullptr && PyErr_ExceptionMatches(PyExc_SystemError) && Py_REFCNT(poBefore) == 1);
			PyErr_Clear();
			Py_DECREF(poBefore);

			// 2) Hata veren O'dan SONRA gelen N ve ic ice ((sO)N): ConsumeRemainingOnError ile DECREF edilmeli ve onceki istisna korunmali
			PyObject* poAfter = PyList_New(0);
			Py_INCREF(poAfter); // refcnt = 2
			PyErr_SetString(PyExc_ValueError, "prior_custom_error");
			PyObject* poFail2 = Py_BuildValue("((sO)N)", "x", static_cast<PyObject*>(nullptr), poAfter);
			CHECK(poFail2 == nullptr && PyErr_ExceptionMatches(PyExc_ValueError) && Py_REFCNT(poAfter) == 1);
			PyErr_Clear();
			Py_DECREF(poAfter);

			Py_XDECREF(poMine);
			Py_XDECREF(poKeep);
		}
		// M2Py_SurrogateEscapedBytes: 64 eszamanli cp1250 dizge isaretcisinin tamami gecerli kalmali (256 slotluk halka tampon)
		{
			std::vector<PyObject*> objs;
			std::vector<const char*> ptrs;
			std::vector<std::string> expected;
			for (int i = 0; i < 64; ++i)
			{
				std::string raw = "item_\xB1\xE6_" + std::to_string(i);
				PyObject* po = Py_BuildValue("s", raw.c_str());
				CHECK(po != nullptr);
				objs.push_back(po);
				expected.push_back(raw);
				ptrs.push_back(M2PyString_AsString(po));
			}
			bool allAlive = true;
			for (size_t i = 0; i < ptrs.size(); ++i)
			{
				if (!ptrs[i] || expected[i] != ptrs[i])
					allAlive = false;
			}
			CHECK(allAlive);
			for (PyObject* po : objs)
				Py_XDECREF(po);
		}

		// cp1250 ham bayt: gercek Py_BuildValue UnicodeDecodeError ile NULL doner; sarmalayici surrogateescape ile ayni baytlari geri uretir
		PyObject* poStrict = (Py_BuildValue)("s", c_szCp1250);
		CHECK(poStrict == nullptr && PyErr_ExceptionMatches(PyExc_UnicodeDecodeError));
		PyErr_Clear();
		PyObject* poEsc = Py_BuildValue("s", c_szCp1250);
		CHECK(poEsc != nullptr && PyUnicode_Check(poEsc));
		char* c_szBack = poEsc ? M2PyString_AsString(poEsc) : nullptr;
		CHECK(c_szBack != nullptr && std::strcmp(c_szBack, c_szCp1250) == 0);
		Py_XDECREF(poEsc);
		PyObject* poTupleEsc = Py_BuildValue("(is)", 3, c_szCp1250);
		CHECK(poTupleEsc != nullptr && PyTuple_Size(poTupleEsc) == 2);
		char* c_szBack2 = poTupleEsc ? M2PyString_AsString(PyTuple_GET_ITEM(poTupleEsc, 1)) : nullptr;
		CHECK(c_szBack2 != nullptr && std::strcmp(c_szBack2, c_szCp1250) == 0);
		Py_XDECREF(poTupleEsc);
		CHECK(!PyErr_Occurred());
		PyGILState_Release(gil8d);
	}

	// 8e. GERCEK locale paketi (client/files/pack/locale) tum .txt satirlari: ham bayt -> M2Py_BuildValue("s") -> M2PyString_AsString bayt-bayt ayni (cp1250/cp949 dahil)
	{
		PyGILState_STATE gil8e = PyGILState_Ensure();
		size_t txtFiles = 0, lines = 0, nonUtf8Lines = 0, mismatches = 0, buildFailures = 0;
		if (pLocalePack)
		{
			for (const auto& kv : pLocalePack->GetIndexMap())
			{
				if (!kv.second || !kv.second->filename[0])
					continue;
				std::string fn(kv.second->filename);
				if (fn.size() <= 4 || fn.substr(fn.size() - 4) != ".txt")
					continue;
				CMappedFile txtFile;
				const void* txtData = nullptr;
				if (!packMgr.Get(txtFile, fn.c_str(), &txtData) || !txtData)
					continue;
				++txtFiles;
				const char* c_pBegin = static_cast<const char*>(txtData);
				const char* c_pEnd = c_pBegin + txtFile.Size();
				while (c_pBegin < c_pEnd)
				{
					const char* c_pNl = static_cast<const char*>(std::memchr(c_pBegin, '\n', static_cast<size_t>(c_pEnd - c_pBegin)));
					const char* c_pLineEnd = c_pNl ? c_pNl : c_pEnd;
					std::string line(c_pBegin, c_pLineEnd);
					c_pBegin = c_pNl ? c_pNl + 1 : c_pEnd;
					if (line.find('\0') != std::string::npos)
						continue;   // char* semantigi NUL'da biter; bu satirlar native sinira zaten tasinamaz
					++lines;
					PyObject* poStr = Py_BuildValue("s", line.c_str());
					if (!poStr)
					{
						++buildFailures;
						PyErr_Clear();
						continue;
					}
					if (PyUnicode_AsUTF8(poStr) == nullptr)   // strict UTF-8 kodlanamiyor => surrogateescape'li (ham cp1250/cp949) satir
					{
						++nonUtf8Lines;
						PyErr_Clear();
					}
					const char* c_szBack = M2PyString_AsString(poStr);
					if (!c_szBack || line != c_szBack)
						++mismatches;
					Py_DECREF(poStr);
				}
			}
		}
		std::printf("LOCALE_TXT_BOUNDARY files=%zu lines=%zu non_utf8_lines=%zu mismatches=%zu build_failures=%zu\n", txtFiles, lines, nonUtf8Lines, mismatches, buildFailures);
		CHECK(txtFiles > 0 && lines > 0);
		CHECK(nonUtf8Lines > 0);   // gercek locale paketinde UTF-8 olmayan (cp1250/cp949) satirlar var: sinir gercekten sinaniyor
		CHECK(mismatches == 0 && buildFailures == 0);
		CHECK(!PyErr_Occurred());
		PyGILState_Release(gil8e);
	}

	// 9. Verify GIL state discipline (PyGILState_Ensure / PyGILState_Release) and script load failure handling
	PyGILState_STATE gilState = PyGILState_Ensure();
	PyObject* mainDict = PyModule_GetDict(PyImport_AddModule("__main__"));
	CHECK(mainDict != nullptr);
	PyObject* pycVal = mainDict ? PyDict_GetItemString(mainDict, "PYC_LOADED_X64") : nullptr;
	CHECK(pycVal != nullptr && PyLong_AsUnsignedLong(pycVal) == 0x20090064UL);
	PyGILState_Release(gilState);

	// Verify graceful failure when loading a non-existent file from pack
	CHECK(!launcher.RunFile("non_existent_script_x64.py"));

	// 10. Verify real profiler, snd, exchange, and safebox C++ production modules (7 real native modules total)
	exchangeMgr.Start();
	exchangeMgr.SetSelfName("Gracz\xB1\xE6");
	exchangeMgr.SetTargetName("Kupiec");
	exchangeMgr.SetElkToSelf(999999999999LL);
	exchangeMgr.SetElkToTarget(500000000000LL);
	exchangeMgr.SetItemToSelf(0, 27987, 200);
	exchangeMgr.SetItemMetinSocketToSelf(0, 1, 28430);
	exchangeMgr.SetItemAttributeToSelf(0, 0, 72, 45);
	exchangeMgr.SetAcceptToSelf(1);

	safeboxMgr.OpenSafeBox(3);
	safeboxMgr.SetMoney(888888888888LL);
	TItemData sbItem{};
	sbItem.vnum = 19;
	sbItem.count = 1;
	sbItem.flags = 4;
	sbItem.alSockets[0] = 28437;
	sbItem.aAttr[0].bType = 1;
	sbItem.aAttr[0].sValue = 2000;
	safeboxMgr.SetItemData(0, sbItem);
	safeboxMgr.OpenMall(1);
	safeboxMgr.SetMallItemData(0, sbItem);

	CHECK(launcher.RunLine(
		"import profiler, snd, exchange, safebox\n"
		"profiler.Push('frame_test')\n"
		"profiler.Pop('frame_test')\n"
		"snd.SetSoundVolume(3)\n"
		"snd.SetSoundVolumef(0.5)\n"
		"snd.SetMusicVolume(0.25)\n"
		"snd.StopAllSound()\n"
		"assert exchange.EXCHANGE_ITEM_MAX_NUM == 12\n"
		"assert exchange.isTrading() == 1\n"
		"assert exchange.GetElkFromSelf() == 999999999999\n"
		"assert exchange.GetElkFromTarget() == 500000000000\n"
		"assert exchange.GetNameFromSelf().encode('utf-8', 'surrogateescape') == b'Gracz\\xb1\\xe6'\n"
		"assert exchange.GetNameFromTarget() == 'Kupiec'\n"
		"assert exchange.GetItemVnumFromSelf(0) == 27987\n"
		"assert exchange.GetItemCountFromSelf(0) == 200\n"
		"assert exchange.GetItemMetinSocketFromSelf(0, 1) == 28430\n"
		"assert exchange.GetItemAttributeFromSelf(0, 0) == (72, 45)\n"
		"assert exchange.GetAcceptFromSelf() == 1\n"
		"assert safebox.SAFEBOX_PAGE_SIZE == 45\n"
		"assert safebox.GetCurrentSafeboxSize() == 135\n"
		"assert safebox.GetMoney() == 888888888888\n"
		"assert safebox.GetItemID(0) == 19 and safebox.GetItemCount(0) == 1 and safebox.GetItemFlags(0) == 4\n"
		"assert safebox.GetItemMetinSocket(0, 0) == 28437\n"
		"assert safebox.GetItemAttribute(0, 0) == (1, 2000)\n"
		"assert safebox.GetMallSize() == 45 and safebox.GetMallItemID(0) == 19\n"
	));

	// 11. Verify hardened PyTuple_GetUnsignedLong/UnsignedInteger/Boolean/Float/Double, negative pos, PyCallClassMemberFunc, and CPythonLauncher::GetError()
	CHECK(launcher.RunLine(
		"import mt2x64bridge as b\n"
		"assert b.GetUnsignedLong(0xDEADBEEF) == 0xDEADBEEF\n"
		"assert b.GetUnsignedInteger(0xCAFEBABE) == 0xCAFEBABE\n"
		"assert b.GetUnsignedLong(7.8) == 7 and b.GetUnsignedInteger(7.8) == 7\n"
		"assert b.GetBoolean(1) is True and b.GetBoolean(0) is False and b.GetBoolean(True) is True\n"
		"assert abs(b.GetFloat(2.5) - 2.5) < 1e-6 and abs(b.GetDouble(-3.25) + 3.25) < 1e-9\n"
		"for bad in ('abc', None, [1]):\n"
		"    for fn in (b.GetUnsignedLong, b.GetUnsignedInteger, b.GetBoolean, b.GetFloat, b.GetDouble):\n"
		"        rejected = False\n"
		"        try:\n"
		"            fn(bad)\n"
		"        except TypeError:\n"
		"            rejected = True\n"
		"        assert rejected, f'{fn} did not reject {bad!r}'\n"
		"assert b.GetUnsignedLong(42) == 42\n"
	));
	{
		PyGILState_STATE gil11 = PyGILState_Ensure();
		PyObject* emptyTuple = PyTuple_New(0);
		int dummyInt = 0;
		unsigned long dummyUL = 0;
		double dummyD = 0.0;
		bool dummyB = false;
		char* dummyS = nullptr;
		CHECK(!PyTuple_GetInteger(emptyTuple, -1, &dummyInt) && !PyErr_Occurred());
		CHECK(!PyTuple_GetUnsignedLong(emptyTuple, -1, &dummyUL) && !PyErr_Occurred());
		CHECK(!PyTuple_GetDouble(emptyTuple, -1, &dummyD) && !PyErr_Occurred());
		CHECK(!PyTuple_GetBoolean(emptyTuple, -1, &dummyB) && !PyErr_Occurred());
		CHECK(!PyTuple_GetString(emptyTuple, -1, &dummyS) && !PyErr_Occurred());
		Py_DECREF(emptyTuple);

		// Verify CPythonLauncher::GetError() on normalized Python 3 BaseException instance
		PyErr_SetString(PyExc_ValueError, "py311_normalized_err_42");
		PyObject *exc = nullptr, *val = nullptr, *tb = nullptr;
		PyErr_Fetch(&exc, &val, &tb);
		PyErr_NormalizeException(&exc, &val, &tb);
		PyErr_Restore(exc, val, tb);
		const char* gotErr = launcher.GetError();
		CHECK(gotErr != nullptr && std::strcmp(gotErr, "py311_normalized_err_42") == 0);
		CHECK(!PyErr_Occurred());
		PyGILState_Release(gil11);
	}

	// 12. Verify hardened appLoadLocaleAddr (TEA decryption, 8-byte block padding, and retSize OOB read prevention)
	{
		static const unsigned char kLocaleTeaKey[16] = {
			0x82, 0x1b, 0x34, 0xae,
			0x12, 0x3b, 0xfb, 0x17,
			0xd7, 0x2c, 0x39, 0xae,
			0x41, 0x98, 0xf1, 0x63
		};
		auto writeTeaFile = [&](const char* path, uint32_t declaredSize, const void* payload, size_t payloadLen, size_t truncateTo = 0)
		{
			const size_t rawLen = sizeof(uint32_t) + payloadLen;
			const size_t padLen = (rawLen % 8 != 0) ? (rawLen + 8 - (rawLen % 8)) : rawLen;
			std::vector<unsigned char> plain(padLen, 0);
			std::vector<unsigned char> cipher(padLen, 0);
			std::memcpy(plain.data(), &declaredSize, sizeof(uint32_t));
			if (payload && payloadLen > 0)
				std::memcpy(plain.data() + sizeof(uint32_t), payload, payloadLen);
			tea_encrypt(reinterpret_cast<unsigned long*>(cipher.data()),
				reinterpret_cast<const unsigned long*>(plain.data()),
				reinterpret_cast<const unsigned long*>(kLocaleTeaKey),
				static_cast<int>(padLen));
			const size_t writeLen = (truncateTo > 0 && truncateTo < padLen) ? truncateTo : padLen;
			FILE* f = std::fopen(path, "wb");
			if (f)
			{
				std::fwrite(cipher.data(), 1, writeLen, f);
				std::fclose(f);
			}
		};

		const char* validPayload = "127.0.0.1:11002\xB1\xE6";
		const uint32_t validLen = static_cast<uint32_t>(std::strlen(validPayload));
		writeTeaFile("build/obj_x64/test_locale_valid.addr", validLen, validPayload, validLen);
		// Malicious file declaring retSize = 4096 inside a 16-byte file (previously caused 4KB OOB stack read)
		writeTeaFile("build/obj_x64/test_locale_oob.addr", 4096U, "bad", 3);
		// Malicious file declaring retSize = 0xFFFFFFFF (previously caused integer wrap / OOB read)
		writeTeaFile("build/obj_x64/test_locale_maxuint.addr", 0xFFFFFFFFU, "bad", 3);
		// Unaligned file (11 bytes, size % 8 != 0) with malicious retSize = 50 (previously overflowed _alloca by 5 bytes in tea_decrypt)
		writeTeaFile("build/obj_x64/test_locale_unaligned.addr", 50U, "1234567", 7, 11);
		// Tiny 2-byte file (< sizeof(unsigned int))
		{
			FILE* f = std::fopen("build/obj_x64/test_locale_tiny.addr", "wb");
			if (f) { std::fwrite("ab", 1, 2, f); std::fclose(f); }
		}

		CHECK(launcher.RunLine(
			"import mt2x64bridge as b\n"
			"res = b.LoadLocaleAddr('build/obj_x64/test_locale_valid.addr')\n"
			"assert isinstance(res, str) and res.encode('utf-8', 'surrogateescape') == b'127.0.0.1:11002\\xb1\\xe6'\n"
			"assert b.LoadLocaleAddr('build/obj_x64/test_locale_missing_xyz.addr') is None\n"
			"assert b.LoadLocaleAddr('build/obj_x64/test_locale_oob.addr') is None\n"
			"assert b.LoadLocaleAddr('build/obj_x64/test_locale_maxuint.addr') is None\n"
			"assert b.LoadLocaleAddr('build/obj_x64/test_locale_unaligned.addr') is None\n"
			"assert b.LoadLocaleAddr('build/obj_x64/test_locale_tiny.addr') is None\n"
		));
	}

	// 13. Verify CEterPack::NewIndex TEterPackIndex struct layout and padding byte determinism (Item 5)
	{
		static_assert(sizeof(TEterPackIndex) == 192, "TEterPackIndex must be 192 bytes");
		static_assert(offsetof(TEterPackIndex, id) == 0, "id offset");
		static_assert(offsetof(TEterPackIndex, filename) == 4, "filename offset");
		static_assert(offsetof(TEterPackIndex, filename_crc) == 168, "filename_crc offset (3 padding bytes at 165..167)");
		static_assert(offsetof(TEterPackIndex, real_data_size) == 172, "real_data_size offset");
		static_assert(offsetof(TEterPackIndex, data_size) == 176, "data_size offset");
		static_assert(offsetof(TEterPackIndex, data_crc) == 180, "data_crc offset");
		static_assert(offsetof(TEterPackIndex, data_position) == 184, "data_position offset");
		static_assert(offsetof(TEterPackIndex, compressed_type) == 188, "compressed_type offset (3 padding bytes at 189..191)");

		auto checkIndexPaddingZero = [](const TEterPackIndex* idx) -> bool
		{
			if (!idx)
				return false;
			const unsigned char* raw = reinterpret_cast<const unsigned char*>(idx);
			// Padding 1: bytes 165, 166, 167 (between filename[161] and filename_crc)
			if (raw[165] != 0 || raw[166] != 0 || raw[167] != 0)
				return false;
			// Padding 2: bytes 189, 190, 191 (tail padding after compressed_type)
			if (raw[189] != 0 || raw[190] != 0 || raw[191] != 0)
				return false;
			// Unused bytes in filename[161] after NUL terminator
			const size_t fnLen = std::strlen(idx->filename);
			for (size_t i = fnLen; i <= FILENAME_MAX_LEN; ++i)
			{
				if (static_cast<unsigned char>(idx->filename[i]) != 0)
					return false;
			}
			return true;
		};

		// 13a. Verify all 923 decrypted on-disk TEterPackIndex records in out2/root.index (237) and out2/locale.index (686)
		CEterFileDict out2RootDict, out2LocaleDict;
		CEterPack out2RootPack, out2LocalePack;
		CHECK(out2RootPack.Create(out2RootDict, "build/python311_migration/out2/root", "", true));
		CHECK(out2LocalePack.Create(out2LocaleDict, "build/python311_migration/out2/locale", "locale/", true));

		int rootChecked = 0, rootDirtyPad = 0;
		for (const auto& kv : out2RootPack.GetIndexMap())
		{
			if (kv.second && kv.second->filename[0])
			{
				++rootChecked;
				if (!checkIndexPaddingZero(kv.second))
					++rootDirtyPad;
			}
		}
		CHECK(rootChecked == 237 && rootDirtyPad == 0);

		int localeChecked = 0, localeDirtyPad = 0;
		for (const auto& kv : out2LocalePack.GetIndexMap())
		{
			if (kv.second && kv.second->filename[0])
			{
				++localeChecked;
				if (!checkIndexPaddingZero(kv.second))
					++localeDirtyPad;
			}
		}
		CHECK(localeChecked == 686 && localeDirtyPad == 0);

		// 13b. Poison heap & free-list padding bytes, write a pack, and verify on-disk .index padding bytes are 0x00
		std::remove("build/obj_x64/test_pad_det.index");
		std::remove("build/obj_x64/test_pad_det.data");
		{
			// Churn heap with 0xCC-filled TEterPackIndex-sized blocks
			std::vector<TEterPackIndex*> blocks;
			for (int i = 0; i < 32; ++i)
			{
				TEterPackIndex* b = new TEterPackIndex;
				std::memset(b, 0xCC, sizeof(TEterPackIndex));
				blocks.push_back(b);
			}
			for (TEterPackIndex* b : blocks)
				delete b;

			CEterFileDict tmpDict;
			CEterPack tmpPack;
			CHECK(tmpPack.Create(tmpDict, "build/obj_x64/test_pad_det", "", false));
			const char* payload1 = "print('padding determinism test 1')\n";
			CHECK(tmpPack.Put("a_first.py", payload1, static_cast<long>(std::strlen(payload1)), COMPRESSED_TYPE_SECURITY));

			auto findInMap = [](CEterPack& pack, const char* name) -> TEterPackIndex* {
				for (const auto& kv : pack.GetIndexMap())
				{
					if (kv.second && std::strcmp(kv.second->filename, name) == 0)
						return kv.second;
				}
				return nullptr;
			};

			TEterPackIndex* idx1 = findInMap(tmpPack, "a_first.py");
			CHECK(idx1 != nullptr && checkIndexPaddingZero(idx1));

			// Deliberately dirty padding bytes of idx1 before deleting it into m_FreeIndexList
			if (idx1)
			{
				unsigned char* raw1 = reinterpret_cast<unsigned char*>(idx1);
				raw1[165] = 0xDE; raw1[166] = 0xAD; raw1[167] = 0xBE;
				raw1[189] = 0xCA; raw1[190] = 0xFE; raw1[191] = 0xEF;
				CHECK(tmpPack.Delete("a_first.py"));
			}

			// Next Put of <= 256 bytes reuses idx1 from m_FreeIndexList; verify NewIndex zeroes all padding bytes
			const char* payload2 = "print('reused free-list index')\n";
			CHECK(tmpPack.Put("b_reused.py", payload2, static_cast<long>(std::strlen(payload2)), COMPRESSED_TYPE_SECURITY));
			TEterPackIndex* idx2 = findInMap(tmpPack, "b_reused.py");
			CHECK(idx2 != nullptr && checkIndexPaddingZero(idx2));
			CHECK(tmpPack.EncryptIndexFile());
		}
		{
			CEterFileDict rdDict;
			CEterPack rdPack;
			CHECK(rdPack.Create(rdDict, "build/obj_x64/test_pad_det", "", true));
			TEterPackIndex* rdIdx = nullptr;
			for (const auto& kv : rdPack.GetIndexMap())
			{
				if (kv.second && std::strcmp(kv.second->filename, "b_reused.py") == 0)
				{
					rdIdx = kv.second;
					break;
				}
			}
			CHECK(rdIdx != nullptr && checkIndexPaddingZero(rdIdx));
		}
		std::remove("build/obj_x64/test_pad_det.index");
		std::remove("build/obj_x64/test_pad_det.data");
	}

	std::printf("PYTHON3_REAL_MODULES_X64_SUITE: %d/%d PASSED (Python %s, 7 real C++ modules: pack+dbg+liblib+profiler+snd+exchange+safebox, root+locale_pl packs verified)\n",
		g_passed,
		g_passed + g_failed,
		Py_GetVersion());

	return (g_failed == 0) ? 0 : 1;
}
