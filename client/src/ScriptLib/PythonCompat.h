#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>

// CPython 3.11 (x64 C++20) C API helpers used across embedded client modules
static_assert(PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 11, "MT2009 client requires embedded CPython 3.11 x64");

inline bool M2PyInt_Check(PyObject* poItem)
{
	return poItem != nullptr && PyLong_Check(poItem) && !PyBool_Check(poItem);
}

inline long M2PyInt_AsLong(PyObject* poItem)
{
	return PyLong_AsLong(poItem);
}

inline PyObject* M2PyInt_FromLong(long lVal)
{
	return PyLong_FromLong(lVal);
}

inline PyObject* M2PyInt_FromSize_t(size_t uVal)
{
	return PyLong_FromSize_t(uVal);
}

inline bool M2PyString_Check(PyObject* poItem)
{
	return poItem != nullptr && (PyUnicode_Check(poItem) || PyBytes_Check(poItem));
}

// pack.Get()/RunMemoryTextFile UTF-8 olmayan ham baytlari (cp1250/cp949 locale dosyalari) "surrogateescape" ile str yapar. Native sinirda
// AYNI baytlar geri uretilmelidir (Py2: char* ham bayt). Strict PyUnicode_AsUTF8 bu str'lerde NULL + UnicodeEncodeError verir.
inline char* M2Py_SurrogateEscapedBytes(PyObject* poItem)
{
	static thread_local std::string s_ring[16];   // donen isaretci cagri suresince gecerli kalmali (PyUnicode_AsUTF8 onbellek semantigi gibi)
	static thread_local unsigned s_idx = 0;
	PyObject* poBytes = PyUnicode_AsEncodedString(poItem, "utf-8", "surrogateescape");
	if (!poBytes)
	{
		PyErr_Clear();
		return nullptr;
	}
	std::string& slot = s_ring[s_idx++ & 15];
	slot.assign(PyBytes_AS_STRING(poBytes), static_cast<size_t>(PyBytes_GET_SIZE(poBytes)));
	Py_DECREF(poBytes);
	return slot.data();
}

inline char* M2PyString_AsString(PyObject* poItem)
{
	if (!poItem)
		return nullptr;
	if (PyUnicode_Check(poItem))
	{
		const char* c_szUtf8 = PyUnicode_AsUTF8(poItem);
		if (c_szUtf8)
			return const_cast<char*>(c_szUtf8);
		PyErr_Clear();
		return M2Py_SurrogateEscapedBytes(poItem);
	}
	if (PyBytes_Check(poItem))
		return PyBytes_AsString(poItem);
	return nullptr;
}

inline PyObject* M2PyString_FromString(const char* szStr)
{
	// UTF-8 olmayan ham baytlar (cp1250 vb.) icin surrogateescape: Py_BuildValue("s") ve PyUnicode_FromString strict oldugundan kullanilmaz
	const char* c_szSafe = szStr ? szStr : "";
	return PyUnicode_DecodeUTF8(c_szSafe, static_cast<Py_ssize_t>(std::strlen(c_szSafe)), "surrogateescape");
}

inline PyObject* M2PyString_FromStringAndSize(const char* szBuf, Py_ssize_t len)
{
	return PyBytes_FromStringAndSize(szBuf, len);
}

inline PyObject* M2PyString_InternFromString(const char* szStr)
{
	return PyUnicode_InternFromString(szStr ? szStr : "");
}

inline constexpr const char* M2Py_GetBuiltinModuleName()
{
	return "builtins";
}

inline PyObject* M2Py_ImportBuiltins()
{
	return PyImport_ImportModule(M2Py_GetBuiltinModuleName());
}

inline void M2Py_SetProgramName(const char* c_szProgramName)
{
	static wchar_t s_wszProgramName[260] = L"metin2client";
	if (c_szProgramName && *c_szProgramName)
	{
		size_t i = 0;
		for (; i < 259 && c_szProgramName[i] != '\0'; ++i)
			s_wszProgramName[i] = static_cast<unsigned char>(c_szProgramName[i]);
		s_wszProgramName[i] = L'\0';
	}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	Py_SetProgramName(s_wszProgramName);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

inline PyObject* M2PyEval_EvalCode(PyCodeObject* co, PyObject* globals, PyObject* locals)
{
	return PyEval_EvalCode(reinterpret_cast<PyObject*>(co), globals, locals);
}

inline PyObject* M2Py_InitModule(const char* c_szName, PyMethodDef* pMethods)
{
	PyObject* poModule = PyImport_AddModule(c_szName);
	if (!poModule)
		return nullptr;

	if (pMethods && PyModule_AddFunctions(poModule, pMethods) != 0)
		return nullptr;

	return poModule;
}
