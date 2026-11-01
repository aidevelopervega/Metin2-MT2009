#pragma once

#include <cstdint>
#include <cstddef>

// Dual-compatibility C API adapter for embedded Python 2.7 and CPython 3.x (x64 C++20)
#if PY_MAJOR_VERSION >= 3

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

inline char* M2PyString_AsString(PyObject* poItem)
{
	if (!poItem)
		return nullptr;
	if (PyUnicode_Check(poItem))
		return const_cast<char*>(PyUnicode_AsUTF8(poItem));
	if (PyBytes_Check(poItem))
		return PyBytes_AsString(poItem);
	return nullptr;
}

inline PyObject* M2PyString_FromString(const char* szStr)
{
	return PyUnicode_FromString(szStr ? szStr : "");
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

#else // PY_MAJOR_VERSION < 3

inline bool M2PyInt_Check(PyObject* poItem)
{
	return poItem != nullptr && PyInt_Check(poItem) && !PyBool_Check(poItem);
}

inline long M2PyInt_AsLong(PyObject* poItem)
{
	return PyInt_AsLong(poItem);
}

inline PyObject* M2PyInt_FromLong(long lVal)
{
	return PyInt_FromLong(lVal);
}

inline PyObject* M2PyInt_FromSize_t(size_t uVal)
{
	return PyInt_FromSize_t(uVal);
}

inline bool M2PyString_Check(PyObject* poItem)
{
	return poItem != nullptr && PyString_Check(poItem);
}

inline char* M2PyString_AsString(PyObject* poItem)
{
	return poItem ? PyString_AsString(poItem) : nullptr;
}

inline PyObject* M2PyString_FromString(const char* szStr)
{
	return PyString_FromString(szStr ? szStr : "");
}

inline PyObject* M2PyString_FromStringAndSize(const char* szBuf, Py_ssize_t len)
{
	return PyString_FromStringAndSize(szBuf, len);
}

inline PyObject* M2PyString_InternFromString(const char* szStr)
{
	return PyString_InternFromString(szStr ? szStr : "");
}

inline constexpr const char* M2Py_GetBuiltinModuleName()
{
	return "__builtin__";
}

inline PyObject* M2Py_ImportBuiltins()
{
	return PyImport_ImportModule(M2Py_GetBuiltinModuleName());
}

inline void M2Py_SetProgramName(const char* c_szProgramName)
{
	Py_SetProgramName(const_cast<char*>(c_szProgramName ? c_szProgramName : "metin2client"));
}

inline PyObject* M2PyEval_EvalCode(PyCodeObject* co, PyObject* globals, PyObject* locals)
{
	return PyEval_EvalCode(co, globals, locals);
}

inline PyObject* M2Py_InitModule(const char* c_szName, PyMethodDef* pMethods)
{
	return Py_InitModule(const_cast<char*>(c_szName), pMethods);
}

#endif // PY_MAJOR_VERSION >= 3
