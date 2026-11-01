#define MT2_STANDALONE_PYTHON3_TEST 1
#include "../client/src/ScriptLib/StdAfx.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct DummyWindow
{
	uint64_t magic;
	int visible;
};

static DummyWindow* g_pLastRegisteredWindow = nullptr;
static DummyWindow* g_pLastShownWindow = nullptr;

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

int main()
{
	static_assert(sizeof(void*) == 8, "Must be compiled as 64-bit PE32+ (AMD64)");
	static_assert(PY_MAJOR_VERSION >= 3, "Must be compiled against CPython 3.x headers");

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

	// 3. Verify 64-bit unsigned IDs (> 2^32, > 2^53, UINT64_MAX) and negative rejection
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

	// 5. Verify M2PyString_* with UTF-8 Turkish text
	const char* turkishText = "Metin2 x64 Türkçe Karakter Testi: ğüşıöçĞÜŞİÖÇ";
	PyObject* pyStr = M2PyString_FromString(turkishText);
	PyObject* tupStr = PyTuple_Pack(1, pyStr);
	char* outStr = nullptr;
	check(PyTuple_GetString(tupStr, 0, &outStr) && outStr != nullptr && std::strcmp(outStr, turkishText) == 0,
		"PyTuple_GetString preserves Turkish UTF-8 text in Python 3");
	Py_DECREF(tupStr);
	Py_DECREF(pyStr);

	// 6. Verify M2Py_InitModule and end-to-end Python 3 script calling C++ with 64-bit window handle
	static PyMethodDef s_wndMethods[] = {
		{ "Register", testWndMgrRegister, METH_VARARGS, "Register 64-bit window" },
		{ "Show",     testWndMgrShow,     METH_VARARGS, "Show 64-bit window"     },
		{ nullptr,    nullptr,            0,            nullptr                  }
	};
	g_pLastRegisteredWindow = highPtr;
	g_pLastShownWindow = nullptr;
	PyObject* poMod = M2Py_InitModule("wndMgr", s_wndMethods);
	check(poMod != nullptr, "M2Py_InitModule('wndMgr') succeeds in Python 3");

	int rc = PyRun_SimpleString(
		"import sys, wndMgr\n"
		"assert sys.maxsize > (1 << 32)\n"
		"h = wndMgr.Register()\n"
		"assert h == 0x00007FF689ABCDEF\n"
		"echo = wndMgr.Show(h)\n"
		"assert echo == h\n"
	);
	check(rc == 0 && g_pLastShownWindow == highPtr, "Python 3 script round-trips 0x00007FF689ABCDEF through wndMgr.Register/Show");

	std::printf("PYTHON3_X64_CPP20_EMBED_SUITE: %d/%d CHECKS PASSED (Python %s)\n", passed, passed, Py_GetVersion());
	Py_FinalizeEx();
	return 0;
}
