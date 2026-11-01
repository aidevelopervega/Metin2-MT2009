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

inline PyObject* M2Py_InitModule(const char* c_szName, PyMethodDef* pMethods)
{
	return Py_InitModule(const_cast<char*>(c_szName), pMethods);
}

#endif // PY_MAJOR_VERSION >= 3
