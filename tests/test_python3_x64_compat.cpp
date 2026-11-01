#define MT2_STANDALONE_PYTHON3_TEST 1
#include "../client/src/ScriptLib/StdAfx.h"
#include "../client/src/ScriptLib/PythonMarshal.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Stubs required by ScriptLib/PythonDebugModule.cpp in standalone test mode
static std::string g_lastTraceError;
void LogBox(const char* /*c_szMsg*/, const char* /*c_szCaption*/, void* /*hWnd*/) {}
void Trace(const char* /*c_szMsg*/) {}
void Tracen(const char* /*c_szMsg*/) {}
void TraceError(const char* c_szFormat, ...)
{
	char buf[512 + 1];
	va_list args;
	va_start(args, c_szFormat);
	std::vsnprintf(buf, sizeof(buf), c_szFormat, args);
	va_end(args);
	g_lastTraceError = buf;
}

namespace
{
	class TestExceptionSender final : public IPythonExceptionSender
	{
	public:
		void Send() override { ++m_sendCount; }
		const std::string& GetString() const { return m_strExceptionString; }
		int GetSendCount() const { return m_sendCount; }
	private:
		int m_sendCount = 0;
	};

	struct DummyWindow
	{
		uint64_t magic;
		int visible;
	};

	struct DummySkillGradeData
	{
		std::string strName;
		void* pImage;
		uint16_t wMotionIndex;
	};

	static DummyWindow* g_pLastRegisteredWindow = nullptr;
	static DummyWindow* g_pLastShownWindow = nullptr;
	static DummySkillGradeData g_dummyGradeData{ "Hava Kılıcı (P)", reinterpret_cast<void*>(0x00007FF711223344ULL), 42 };
	static long long g_playerElk = 1000000000000LL; // GOLD_MAX (1 Trillion Yang)
	static unsigned long long g_lastGiveItemID = 0ULL;
	static unsigned int g_lastGiveTargetVID = 0U;
	static std::string g_lastChatLine;
	static int g_traceCallCount = 0;
	static int g_lastTracedLine = 0;

	static PyObject* testWndMgrRegister(PyObject* /*poSelf*/, PyObject* /*poArgs*/)
	{
		return Py_BuildPointerValue(g_pLastRegisteredWindow);
	}

	static PyObject* testWndMgrShow(PyObject* /*poSelf*/, PyObject* poArgs)
	{
		DummyWindow* pWin = nullptr;
		if (!PyTuple_GetPointer(poArgs, 0, &pWin) || !pWin)
			return Py_BuildException("Invalid 64-bit window handle");
		g_pLastShownWindow = pWin;
		return Py_BuildValue("K", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pWin)));
	}

	static PyObject* testSkillGetGradeData(PyObject* /*poSelf*/, PyObject* /*poArgs*/)
	{
		return Py_BuildPointerValue(&g_dummyGradeData);
	}

	// Mirrors hardened playerSendDragonSoulRefinePacket dictionary parsing in PythonPlayerModule.cpp
	static constexpr int kDsRefineWindowMaxNum = 15;
	static PyObject* testSendDragonSoulRefinePacket(PyObject* /*poSelf*/, PyObject* poArgs)
	{
		unsigned char bSubHeader = 0;
		PyObject* pDic = nullptr;
		if (!PyTuple_GetByte(poArgs, 0, &bSubHeader))
			return Py_BuildException();
		if (!PyTuple_GetObject(poArgs, 1, &pDic) || !PyDict_Check(pDic))
			return Py_BuildException();

		Py_ssize_t pos = 0;
		PyObject* key = nullptr;
		PyObject* value = nullptr;
		int parsedCount = 0;
		while (PyDict_Next(pDic, &pos, &key, &value))
		{
			int i = M2PyInt_AsLong(key);
			if (i < 0 || i >= kDsRefineWindowMaxNum)
				return Py_BuildException("DragonSoul slot index out of bounds");

			unsigned char winType = 0;
			int cell = 0;
			if (!PyTuple_GetByte(value, 0, &winType) || !PyTuple_GetInteger(value, 1, &cell))
				return Py_BuildException("DragonSoul invalid item pos tuple");
			++parsedCount;
		}
		return M2PyInt_FromLong(parsedCount);
	}

	static PyObject* testPlayerGetElk(PyObject* /*poSelf*/, PyObject* /*poArgs*/)
	{
		return PyLong_FromLongLong(g_playerElk);
	}

	static PyObject* testPlayerSetElk(PyObject* /*poSelf*/, PyObject* poArgs)
	{
		long long llGold = 0;
		if (!PyTuple_GetLongLong(poArgs, 0, &llGold))
			return Py_BuildException("Invalid 64-bit Yang value");
		if (llGold < 0 || llGold > 1000000000000LL)
			return Py_BuildException("Yang out of GOLD_MAX bounds");
		g_playerElk = llGold;
		return Py_BuildNone();
	}

	static PyObject* testNetSendGiveItemPacket(PyObject* /*poSelf*/, PyObject* poArgs)
	{
		unsigned int dwTargetVID = 0;
		unsigned long long ullItemID = 0;
		int iCount = 0;
		if (!PyTuple_GetUnsignedInteger(poArgs, 0, &dwTargetVID))
			return Py_BuildException("Invalid VID");
		if (!PyTuple_GetUnsignedLongLong(poArgs, 1, &ullItemID))
			return Py_BuildException("Invalid 64-bit ITEM_ID");
		if (!PyTuple_GetInteger(poArgs, 2, &iCount))
			return Py_BuildException("Invalid item count");
		g_lastGiveTargetVID = dwTargetVID;
		g_lastGiveItemID = ullItemID;
		return Py_BuildValue("i", iCount);
	}

	static PyObject* testChatAppendChat(PyObject* /*poSelf*/, PyObject* poArgs)
	{
		int iType = 0;
		char* szText = nullptr;
		if (!PyTuple_GetInteger(poArgs, 0, &iType) || !PyTuple_GetString(poArgs, 1, &szText))
			return Py_BuildException("Invalid chat args");
		g_lastChatLine = szText ? szText : "";
		return Py_BuildNone();
	}

	// Mirrors PythonLauncher.cpp TraceFunc using Python 3.9+ PyFrame_GetCode / PyFrame_GetLineNumber
	static int TestTraceFunc(PyObject* /*obj*/, PyFrameObject* f, int what, PyObject* /*arg*/)
	{
		if (what == PyTrace_LINE && f != nullptr)
		{
			PyCodeObject* code = PyFrame_GetCode(f);
			g_lastTracedLine = PyFrame_GetLineNumber(f);
			if (code)
				Py_DECREF(code);
			++g_traceCallCount;
		}
		return 0;
	}
}

int main()
{
	static_assert(sizeof(void*) == 8, "Must be compiled as 64-bit PE32+ (AMD64)");
	static_assert(PY_MAJOR_VERSION >= 3, "Must be compiled against CPython 3.x headers");

	M2Py_SetProgramName("metin2client_x64_test");
	Py_Initialize();
	assert(Py_IsInitialized());

	int passed = 0;
	auto check = [&](bool cond, const char* msg) {
		if (!cond) {
			std::fprintf(stderr, "[FAIL] %s\n", msg);
			std::exit(1);
		}
		++passed;
	};

	// 1. Verify 64-bit pointer handle round-trip (> 2^32 address)
	DummyWindow* highPtr = reinterpret_cast<DummyWindow*>(static_cast<uintptr_t>(0x00007FF689ABCDEFULL));
	PyObject* pyHandle = Py_BuildPointerValue(highPtr);
	check(pyHandle != nullptr, "Py_BuildPointerValue(>2^32) non-null");

	PyObject* tuple1 = PyTuple_Pack(1, pyHandle);
	DummyWindow* outPtr = nullptr;
	check(PyTuple_GetPointer(tuple1, 0, &outPtr), "PyTuple_GetPointer(>2^32) succeeds");
	check(outPtr == highPtr, "PyTuple_GetPointer preserves full 64-bit address 0x00007FF689ABCDEF");
	Py_DECREF(tuple1);
	Py_DECREF(pyHandle);

	// 2. Verify nullptr and Py_None handle behavior + rejection of bool/string
	PyObject* nullHandle = Py_BuildPointerValue(nullptr);
	PyObject* tupleNull = PyTuple_Pack(1, nullHandle);
	void* rawNull = reinterpret_cast<void*>(0x1234ULL);
	check(PyTuple_GetPointer(tupleNull, 0, &rawNull) && rawNull == nullptr, "PyTuple_GetPointer(0) -> nullptr");
	Py_DECREF(tupleNull);
	Py_DECREF(nullHandle);

	PyObject* tupleNone = PyTuple_Pack(1, Py_None);
	rawNull = reinterpret_cast<void*>(0x1234ULL);
	check(PyTuple_GetPointer(tupleNone, 0, &rawNull) && rawNull == nullptr, "PyTuple_GetPointer(None) -> nullptr");
	Py_DECREF(tupleNone);

	PyObject* tupleBool = PyTuple_Pack(1, Py_True);
	check(!PyTuple_GetPointer(tupleBool, 0, &rawNull), "PyTuple_GetPointer rejects bool True");
	Py_DECREF(tupleBool);

	// 3. Verify 64-bit unsigned IDs (> 2^32, > 2^53, UINT64_MAX) and negative/overflow rejection
	const unsigned long long testIds[] = {
		0ULL,
		4294967296ULL,          // 2^32
		4294967311ULL,          // > 2^32
		9007199254740993ULL,    // 2^53 + 1
		18446744073709551615ULL // UINT64_MAX
	};
	for (unsigned long long expected : testIds)
	{
		PyObject* pyVal = PyLong_FromUnsignedLongLong(expected);
		PyObject* tup = PyTuple_Pack(1, pyVal);
		unsigned long long actual = 0;
		check(PyTuple_GetUnsignedLongLong(tup, 0, &actual) && actual == expected, "PyTuple_GetUnsignedLongLong exact 64-bit match");
		Py_DECREF(tup);
		Py_DECREF(pyVal);
	}

	PyObject* pyNeg = PyLong_FromLongLong(-1LL);
	PyObject* tupNeg = PyTuple_Pack(1, pyNeg);
	unsigned long long dummyU64 = 0;
	check(!PyTuple_GetUnsignedLongLong(tupNeg, 0, &dummyU64), "PyTuple_GetUnsignedLongLong rejects -1");
	Py_DECREF(tupNeg);
	Py_DECREF(pyNeg);

	// 4. Verify signed 64-bit Yang/points (> 2^32, GOLD_MAX = 1,000,000,000,000, INT64_MIN, INT64_MAX)
	const long long testYang[] = {
		-9223372036854775807LL - 1LL,
		-1LL,
		0LL,
		5000000000LL,           // 5B Yang (> 2^32)
		999999999999LL,         // GOLD_MAX - 1
		1000000000000LL,        // GOLD_MAX
		9223372036854775807LL   // INT64_MAX
	};
	for (long long expected : testYang)
	{
		PyObject* pyVal = PyLong_FromLongLong(expected);
		PyObject* tup = PyTuple_Pack(1, pyVal);
		long long actual = 0;
		check(PyTuple_GetLongLong(tup, 0, &actual) && actual == expected, "PyTuple_GetLongLong exact 64-bit match");
		Py_DECREF(tup);
		Py_DECREF(pyVal);
	}

	// Verify PyTuple_GetLongLong rejects > INT64_MAX (UINT64_MAX)
	PyObject* pyOverI64 = PyLong_FromUnsignedLongLong(18446744073709551615ULL);
	PyObject* tupOverI64 = PyTuple_Pack(1, pyOverI64);
	long long dummyI64 = 0;
	check(!PyTuple_GetLongLong(tupOverI64, 0, &dummyI64) && PyErr_Occurred() == nullptr,
		"PyTuple_GetLongLong rejects UINT64_MAX overflow and clears PyErr");
	Py_DECREF(tupOverI64);
	Py_DECREF(pyOverI64);

	// 5. Verify M2PyString_* with UTF-8 Turkish text & interning
	const char* turkishText = "Metin2 x64 Türkçe Karakter Testi: ğüşıöçĞÜŞİÖÇ";
	PyObject* pyStr = M2PyString_FromString(turkishText);
	PyObject* tupStr = PyTuple_Pack(1, pyStr);
	char* outStr = nullptr;
	check(PyTuple_GetString(tupStr, 0, &outStr) && outStr != nullptr && std::strcmp(outStr, turkishText) == 0,
		"PyTuple_GetString preserves Turkish UTF-8 text in Python 3");
	Py_DECREF(tupStr);
	Py_DECREF(pyStr);

	PyObject* pyIntern1 = M2PyString_InternFromString("OnUpdate");
	PyObject* pyIntern2 = M2PyString_InternFromString("OnUpdate");
	check(pyIntern1 != nullptr && pyIntern1 == pyIntern2, "M2PyString_InternFromString interns identical pointer");
	Py_DECREF(pyIntern1);
	Py_DECREF(pyIntern2);

	// 6. Verify M2PyInt_FromLong & M2PyInt_FromSize_t & M2Py_ImportBuiltins
	PyObject* pySize = M2PyInt_FromSize_t(static_cast<size_t>(0x123456789ABCDEF0ULL));
	check(M2PyInt_Check(pySize), "M2PyInt_Check recognizes 64-bit size_t PyLong");
	Py_DECREF(pySize);

	check(std::strcmp(M2Py_GetBuiltinModuleName(), "builtins") == 0, "M2Py_GetBuiltinModuleName() == 'builtins' on Python 3");
	PyObject* builtins = M2Py_ImportBuiltins();
	check(builtins != nullptr, "M2Py_ImportBuiltins() succeeds on Python 3");
	PyModule_AddIntConstant(builtins, "TRUE", 1);
	PyModule_AddIntConstant(builtins, "FALSE", 0);
	Py_DECREF(builtins);

	// 7. Verify real ScriptLib initdbg() and dbg.RegisterExceptionString
	TestExceptionSender excSender;
	g_pkExceptionSender = &excSender;
	initdbg();
	int rcDbg = PyRun_SimpleString(
		"import dbg\n"
		"assert TRUE == 1 and FALSE == 0\n"
		"dbg.RegisterExceptionString('x64_dbg_trace_ok')\n"
	);
	check(rcDbg == 0 && excSender.GetString() == "x64_dbg_trace_ok", "ScriptLib initdbg() and dbg.RegisterExceptionString work in Python 3");
	g_pkExceptionSender = nullptr;

	// 8. Verify M2Py_InitModule, 64-bit pointer handles, SGradeData pointer, Py_BuildException non-SystemError contract, and Dragon Soul bounds check
	static PyMethodDef s_wndMethods[] = {
		{ "Register",            testWndMgrRegister,            METH_VARARGS, "Register 64-bit window" },
		{ "Show",                testWndMgrShow,                METH_VARARGS, "Show 64-bit window"     },
		{ "GetGradeData",        testSkillGetGradeData,         METH_VARARGS, "Get skill grade ptr"    },
		{ "SendDSRefinePacket",  testSendDragonSoulRefinePacket, METH_VARARGS, "Send DS refine packet"  },
		{ nullptr,               nullptr,                       0,            nullptr                  }
	};
	g_pLastRegisteredWindow = highPtr;
	g_pLastShownWindow = nullptr;
	g_lastTraceError.clear();
	PyObject* poMod = M2Py_InitModule("wndMgr", s_wndMethods);
	check(poMod != nullptr, "M2Py_InitModule('wndMgr') succeeds in Python 3");

	int rc = PyRun_SimpleString(
		"import sys, wndMgr\n"
		"assert sys.maxsize > (1 << 32)\n"
		"h = wndMgr.Register()\n"
		"assert h == 0x00007FF689ABCDEF\n"
		"echo = wndMgr.Show(h)\n"
		"assert echo == h\n"
		"gd = wndMgr.GetGradeData()\n"
		"assert gd > 0\n"
		"assert wndMgr.SendDSRefinePacket(1, {0: (1, 5), 14: (1, 12)}) == 2\n"
		"assert wndMgr.SendDSRefinePacket(1, {-1: (1, 5)}) is None\n"
		"assert wndMgr.SendDSRefinePacket(1, {15: (1, 5)}) is None\n"
	);
	check(rc == 0 && g_pLastShownWindow == highPtr && g_lastTraceError == "DragonSoul slot index out of bounds",
		"Python 3 script round-trips 64-bit handles and enforces DragonSoul bounds without SystemError");

	// 9. Verify ScriptLib _PyMarshal_ReadObjectFromString and M2PyEval_EvalCode on Python 3 bytecode
	int rcMarshal = PyRun_SimpleString(
		"import marshal\n"
		"co = compile('marshal_result = (1 << 40) + 777', '<m2_marshal_test>', 'exec')\n"
		"raw_bytes = marshal.dumps(co)\n"
	);
	check(rcMarshal == 0, "Python 3 marshal.dumps(code) succeeds");
	PyObject* mainMod = PyImport_AddModule("__main__");
	PyObject* mainDict = PyModule_GetDict(mainMod);
	PyObject* pyRawBytes = PyDict_GetItemString(mainDict, "raw_bytes");
	check(pyRawBytes != nullptr && PyBytes_Check(pyRawBytes), "raw_bytes present in __main__");
	PyObject* unmarshaled = _PyMarshal_ReadObjectFromString(
		PyBytes_AsString(pyRawBytes),
		static_cast<int>(PyBytes_Size(pyRawBytes))
	);
	check(unmarshaled != nullptr && PyCode_Check(unmarshaled), "_PyMarshal_ReadObjectFromString unmarshals Python 3 code object");
	PyObject* evalRes = M2PyEval_EvalCode(reinterpret_cast<PyCodeObject*>(unmarshaled), mainDict, mainDict);
	check(evalRes != nullptr, "M2PyEval_EvalCode executes unmarshaled Python 3 code object");
	Py_DECREF(evalRes);
	Py_DECREF(unmarshaled);
	PyObject* pyMarshalResult = PyDict_GetItemString(mainDict, "marshal_result");
	check(pyMarshalResult != nullptr && PyLong_AsLongLong(pyMarshalResult) == ((1LL << 40) + 777LL),
		"M2PyEval_EvalCode computed exact 64-bit result (1 << 40) + 777");

	// 10. Verify client Python scripts (simplejson + ordered_dict + Version.py) under CPython 3.11 x64
	int rcScripts = PyRun_SimpleString(
		"import ast, pathlib, importlib.util\n"
		"for p in sorted(list(pathlib.Path('client/files/lib/simplejson').glob('*.py')) + [pathlib.Path('client/src/UserInterface/Version.py')]):\n"
		"    ast.parse(p.read_text(encoding='utf-8'), filename=str(p))\n"
		"spec = importlib.util.spec_from_file_location('m2_ordered_dict', 'client/files/lib/simplejson/ordered_dict.py')\n"
		"mod = importlib.util.module_from_spec(spec)\n"
		"spec.loader.exec_module(mod)\n"
		"od = mod.OrderedDict([('a', 1), ('b', 2), ('c', 10**15)])\n"
		"assert list(od.keys()) == ['a', 'b', 'c']\n"
		"assert od.popitem(last=True) == ('c', 10**15)\n"
		"assert od.popitem(last=False) == ('a', 1)\n"
	);
	check(rcScripts == 0, "Client Python scripts (simplejson/*.py, ordered_dict.py, Version.py) pass AST & execution in Python 3 x64");

	// 11. Verify Multi-Module Registration (player, net, chat) + UI Script CRLF Loading + TraceFunc (PyFrame_GetCode/GetLineNumber)
	static PyMethodDef s_playerMethods[] = {
		{ "GetElk", testPlayerGetElk, METH_VARARGS, "Get 64-bit Yang" },
		{ "SetElk", testPlayerSetElk, METH_VARARGS, "Set 64-bit Yang" },
		{ nullptr,  nullptr,          0,            nullptr           }
	};
	static PyMethodDef s_netMethods[] = {
		{ "SendGiveItemPacket", testNetSendGiveItemPacket, METH_VARARGS, "Send 64-bit item packet" },
		{ nullptr,              nullptr,                   0,            nullptr                   }
	};
	static PyMethodDef s_chatMethods[] = {
		{ "AppendChat", testChatAppendChat, METH_VARARGS, "Append chat line" },
		{ nullptr,      nullptr,            0,            nullptr            }
	};
	check(M2Py_InitModule("player", s_playerMethods) != nullptr, "M2Py_InitModule('player') succeeds");
	check(M2Py_InitModule("net", s_netMethods) != nullptr, "M2Py_InitModule('net') succeeds");
	check(M2Py_InitModule("chat", s_chatMethods) != nullptr, "M2Py_InitModule('chat') succeeds");
	PyModule_AddIntConstant(M2Py_InitModule("chat", s_chatMethods), "CHAT_TYPE_INFO", 1);

	// Simulate CPythonLauncher::RunMemoryTextFile CRLF -> LF normalization + Py_CompileStringFlags + PyEval_SetTrace
	std::string crlfUiScript =
		"import player, net, chat\r\n"
		"assert player.GetElk() == 1000000000000\r\n"
		"player.SetElk(500000000000)\r\n"
		"assert player.GetElk() == 500000000000\r\n"
		"player.SetElk(1000000000001)\r\n" // Out of GOLD_MAX -> Py_BuildException returns None, keeps 500B
		"assert player.GetElk() == 500000000000\r\n"
		"cnt = net.SendGiveItemPacket(424242, 9007199254740993, 7)\r\n"
		"assert cnt == 7\r\n"
		"chat.AppendChat(chat.CHAT_TYPE_INFO, 'Envanter güncellendi: 500 Milyar Yang')\r\n";

	for (size_t i = 0; i < crlfUiScript.size(); ++i)
	{
		if (crlfUiScript[i] == '\r' && i + 1 < crlfUiScript.size() && crlfUiScript[i + 1] == '\n')
			crlfUiScript.erase(i, 1);
	}

	g_traceCallCount = 0;
	g_lastTracedLine = 0;
	PyEval_SetTrace(TestTraceFunc, nullptr);
	PyObject* uiCode = Py_CompileStringFlags(crlfUiScript.c_str(), "uiInventory.py", Py_file_input, nullptr);
	check(uiCode != nullptr, "Py_CompileStringFlags compiles CRLF-normalized UI script");
	PyObject* uiEval = M2PyEval_EvalCode(reinterpret_cast<PyCodeObject*>(uiCode), mainDict, mainDict);
	PyEval_SetTrace(nullptr, nullptr);
	check(uiEval != nullptr, "M2PyEval_EvalCode executes UI script with player/net/chat bindings");
	Py_DECREF(uiEval);
	Py_DECREF(uiCode);
	check(g_playerElk == 500000000000LL &&
		g_lastGiveTargetVID == 424242U &&
		g_lastGiveItemID == 9007199254740993ULL &&
		g_lastChatLine == "Envanter güncellendi: 500 Milyar Yang" &&
		g_traceCallCount >= 8 && g_lastTracedLine >= 8,
		"UI script updated 64-bit Yang, >2^53 ITEM_ID, Turkish chat, and triggered Python 3.11 PyFrame_GetCode/GetLineNumber trace");

	// 12. Verify PEP 552 16-byte .pyc file header + _PyMarshal_ReadLastObjectFromFile (PythonLauncher.cpp:RunCompiledFile path)
	int rcPyc = PyRun_SimpleString(
		"import importlib.util, marshal, struct\n"
		"co_pyc = compile('pyc_loaded_val = 0x7FF600001111', 'root/uicharacter.py', 'exec')\n"
		"pyc_payload = importlib.util.MAGIC_NUMBER + struct.pack('<III', 0, 1700000000, 128) + marshal.dumps(co_pyc)\n"
	);
	check(rcPyc == 0, "Generated PEP 552 16-byte .pyc image in memory");
	PyObject* pyPycPayload = PyDict_GetItemString(mainDict, "pyc_payload");
	check(pyPycPayload != nullptr && PyBytes_Check(pyPycPayload), "pyc_payload bytes available");
	FILE* tmpPyc = nullptr;
	fopen_s(&tmpPyc, "scratch_test_uicharacter.pyc", "wb+");
	check(tmpPyc != nullptr, "Opened temporary PEP 552 .pyc file");
	std::fwrite(PyBytes_AsString(pyPycPayload), 1, static_cast<size_t>(PyBytes_Size(pyPycPayload)), tmpPyc);
	std::fflush(tmpPyc);
	std::rewind(tmpPyc);
	long magic = _PyMarshal_ReadLongFromFile(tmpPyc);
	check(magic == PyImport_GetMagicNumber(), "_PyMarshal_ReadLongFromFile matches PyImport_GetMagicNumber()");
	(void)_PyMarshal_ReadLongFromFile(tmpPyc); // PEP 552 bitfield
	(void)_PyMarshal_ReadLongFromFile(tmpPyc); // mtime
	(void)_PyMarshal_ReadLongFromFile(tmpPyc); // source size
	PyObject* fileCodeObj = _PyMarshal_ReadLastObjectFromFile(tmpPyc);
	std::fclose(tmpPyc);
	std::remove("scratch_test_uicharacter.pyc");
	check(fileCodeObj != nullptr && PyCode_Check(fileCodeObj), "_PyMarshal_ReadLastObjectFromFile reads code object after 16-byte PEP 552 header");
	PyObject* pycEval = M2PyEval_EvalCode(reinterpret_cast<PyCodeObject*>(fileCodeObj), mainDict, mainDict);
	check(pycEval != nullptr, "M2PyEval_EvalCode runs .pyc code object");
	Py_DECREF(pycEval);
	Py_DECREF(fileCodeObj);
	PyObject* pyLoadedVal = PyDict_GetItemString(mainDict, "pyc_loaded_val");
	check(pyLoadedVal != nullptr && PyLong_AsUnsignedLongLong(pyLoadedVal) == 0x7FF600001111ULL,
		"PEP 552 .pyc execution produced exact 64-bit 0x7FF600001111");

	// 13. Reference Management & Zero-Leak Audit (PyCallClassMemberFunc / PyCallClassMemberFunc_ByPyString across 1,000 iterations)
	int rcUiClass = PyRun_SimpleString(
		"class UIScriptWindow:\n"
		"    def __init__(self):\n"
		"        self.calls = 0\n"
		"        self.last_handle = 0\n"
		"        self.last_yang = 0\n"
		"    def OnUpdate(self, h, yang):\n"
		"        self.calls += 1\n"
		"        self.last_handle = h\n"
		"        self.last_yang = yang\n"
		"        return 1\n"
		"    def OnClick(self, flag):\n"
		"        return bool(flag)\n"
		"ui_win_instance = UIScriptWindow()\n"
	);
	check(rcUiClass == 0, "Defined UIScriptWindow instance for refcount leak audit");
	PyObject* poWin = PyDict_GetItemString(mainDict, "ui_win_instance");
	check(poWin != nullptr, "Retrieved ui_win_instance");
	PyObject* poOnUpdateName = M2PyString_InternFromString("OnUpdate");

	// Warm-up call so any internal Python method cache stabilizes
	long warmRet = 0;
	check(PyCallClassMemberFunc(poWin, "OnUpdate", Py_BuildValue("(KL)", 0x00007FF689ABCDEFULL, 5000000000LL), &warmRet) && warmRet == 1,
		"Warm-up PyCallClassMemberFunc(OnUpdate) succeeds");

	const Py_ssize_t refWinBefore = Py_REFCNT(poWin);
	for (int iter = 0; iter < 1000; ++iter)
	{
		long lRet = 0;
		bool ok1 = PyCallClassMemberFunc(poWin, "OnUpdate", Py_BuildValue("(KL)", 0x00007FF689ABCDEFULL, 5000000000LL), &lRet);
		bool bRet = false;
		bool ok2 = PyCallClassMemberFunc(poWin, "OnClick", Py_BuildValue("(i)", 1), &bRet);
		bool ok3 = PyCallClassMemberFunc_ByPyString(poWin, poOnUpdateName, Py_BuildValue("(KL)", 0x00007FF689ABCDEFULL, 1000000000000LL));
		bool ok4 = PyCallClassMemberFunc(poWin, "OnMissingCallback", Py_BuildValue("(i)", iter));
		bool ok5 = PyCallClassMemberFunc(nullptr, "OnUpdate", Py_BuildValue("(i)", iter));
		if (!ok1 || lRet != 1 || !ok2 || !bRet || !ok3 || ok4 || ok5 || PyErr_Occurred() != nullptr)
		{
			check(false, "PyCallClassMemberFunc iteration failed or left dangling exception");
		}
	}
	const Py_ssize_t refWinAfter = Py_REFCNT(poWin);
	Py_DECREF(poOnUpdateName);
	check(refWinAfter == refWinBefore, "Zero reference count leak on UIScriptWindow across 5,000 C->Python callback invocations");

	std::printf("PYTHON3_X64_CPP20_EMBED_SUITE: %d/%d CHECKS PASSED (Python %s)\n", passed, passed, Py_GetVersion());
	Py_FinalizeEx();
	return 0;
}
