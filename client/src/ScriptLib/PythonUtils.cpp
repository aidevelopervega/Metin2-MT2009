#include "StdAfx.h"
#include "PythonUtils.h"
#define PyLong_AsLong PyLong_AsLongLong
#define PyLong_AsUnsignedLong (unsigned long)PyLong_AsLongLong // it may return random values without the explicit cast
IPythonExceptionSender * g_pkExceptionSender = NULL;

bool __PyCallClassMemberFunc_ByCString(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, PyObject** poRet);
bool __PyCallClassMemberFunc_ByPyString(PyObject* poClass, PyObject* poFuncName, PyObject* poArgs, PyObject** poRet);
bool __PyCallClassMemberFunc(PyObject* poClass, PyObject* poFunc, PyObject* poArgs, PyObject** poRet);

PyObject * Py_BadArgument()
{
	PyErr_BadArgument();
	return NULL;
}

PyObject * Py_BuildException(const char * c_pszErr, ...)
{
	if (!c_pszErr)
		PyErr_Clear();
	else
	{
		char szErrBuf[512+1];
		va_list args;
		va_start(args, c_pszErr);
		vsnprintf(szErrBuf, sizeof(szErrBuf), c_pszErr, args);
		va_end(args);

		TraceError("%s", szErrBuf);
		PyErr_Clear();
	}

	return Py_BuildNone();
	//return NULL;
}

PyObject * Py_BuildNone()
{
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject* Py_BuildEmptyTuple()
{
	auto emptyTuple = Py_BuildValue("()");
	Py_INCREF(emptyTuple);
	return  emptyTuple;
}

PyObject* Py_BuildPointerValue(const void* ptr)
{
	if (!ptr)
		return Py_BuildValue("i", 0);
	return PyLong_FromVoidPtr(const_cast<void*>(ptr));
}

void Py_ReleaseNone()
{
	Py_DECREF(Py_None);
}

bool PyTuple_GetObject(PyObject* poArgs, int pos, PyObject** ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	*ret = poItem;
	return true;
}

// Py2 PyInt_AsLong davranisi: float verilirse sifira dogru kirpilir. Py3.10+ PyLong_AsLong(float) TypeError ile reddeder ve -1 dondurur;
// onceki kod bu hatayi yutup -1 kullaniyor ve bekleyen (pending) istisna birakiyordu. Gecersiz/tasan/NaN degerde artik false doner.
static bool M2Py_AsInteger64(PyObject* poItem, long long* ret)
{
	if (!poItem || !ret)
		return false;

	if (PyFloat_Check(poItem))
	{
		const double d = PyFloat_AS_DOUBLE(poItem);
		if (!(d > -9.2e18 && d < 9.2e18))
			return false;
		*ret = static_cast<long long>(d);
		return true;
	}

	const long long val = PyLong_AsLongLong(poItem);
	if (val == -1 && PyErr_Occurred())
	{
		PyErr_Clear();
		return false;
	}
	*ret = val;
	return true;
}

bool PyTuple_GetLong(PyObject* poArgs, int pos, long* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	long long val = 0;
	if (!M2Py_AsInteger64(poItem, &val))
		return false;
	*ret = static_cast<long>(val);
	return true;
}

bool PyTuple_GetLongLong(PyObject* poArgs, int pos, long long* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
	{
		if (PyErr_Occurred())
			PyErr_Clear();
		return false;
	}

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
	{
		if (PyErr_Occurred())
			PyErr_Clear();
		return false;
	}

	if (!ret || pos < 0 || PyBool_Check(poItem))
		return false;

	if (!PyLong_Check(poItem))
		return false;

	PyErr_Clear();
	int overflow = 0;
	const long long val = PyLong_AsLongLongAndOverflow(poItem, &overflow);
	if (overflow != 0 || PyErr_Occurred())
	{
		PyErr_Clear();
		return false;
	}

	*ret = val;
	return true;
}

bool PyTuple_GetUnsignedLongLong(PyObject* poArgs, int pos, unsigned long long* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
	{
		if (PyErr_Occurred())
			PyErr_Clear();
		return false;
	}

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
	{
		if (PyErr_Occurred())
			PyErr_Clear();
		return false;
	}

	if (!ret || pos < 0 || PyBool_Check(poItem))
		return false;

	if (!PyLong_Check(poItem))
		return false;

	PyErr_Clear();
	const unsigned long long val = PyLong_AsUnsignedLongLong(poItem);
	if (PyErr_Occurred())
	{
		PyErr_Clear();
		return false;
	}

	*ret = val;
	return true;
}

bool PyTuple_GetPointer(PyObject* poArgs, int pos, void** ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
	{
		if (PyErr_Occurred())
			PyErr_Clear();
		return false;
	}

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);
	if (!poItem || PyBool_Check(poItem))
	{
		if (PyErr_Occurred())
			PyErr_Clear();
		return false;
	}

	if (poItem == Py_None)
	{
		*ret = nullptr;
		return true;
	}

	PyErr_Clear();
	void* ptr = PyLong_AsVoidPtr(poItem);
	if (PyErr_Occurred())
	{
		PyErr_Clear();
		return false;
	}

	*ret = ptr;
	return true;
}

bool PyTuple_GetDouble(PyObject* poArgs, int pos, double* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	const double val = PyFloat_AsDouble(poItem);
	if (val == -1.0 && PyErr_Occurred())
	{
		PyErr_Clear();
		return false;
	}
	*ret = val;
	return true;
}

bool PyTuple_GetFloat(PyObject* poArgs, int pos, float* ret)
{
	if (!ret)
		return false;

	double val = 0.0;
	if (!PyTuple_GetDouble(poArgs, pos, &val))
		return false;

	*ret = float(val);
	return true;
}

bool PyTuple_GetByte(PyObject* poArgs, int pos, unsigned char* ret)
{
	if (!ret)
		return false;

	int val;
	bool result = PyTuple_GetInteger(poArgs,pos,&val);
	if (result)
		*ret = unsigned char(val);
	return result;
}

bool PyTuple_GetInteger(PyObject* poArgs, int pos, unsigned char* ret)
{
	if (!ret)
		return false;

	int val;
	bool result = PyTuple_GetInteger(poArgs,pos,&val);
	if (result)
		*ret = unsigned char(val);
	return result;
}

bool PyTuple_GetInteger(PyObject* poArgs, int pos, WORD* ret)
{
	if (!ret)
		return false;

	int val;
	bool result = PyTuple_GetInteger(poArgs,pos,&val);
	if (result)
		*ret = WORD(val);
	return result;
}

bool PyTuple_GetInteger(PyObject* poArgs, int pos, int* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	long long val = 0;
	if (!M2Py_AsInteger64(poItem, &val))
		return false;
	*ret = (int)val;
	return true;
}

bool PyTuple_GetUnsignedLong(PyObject* poArgs, int pos, unsigned long* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	long long val = 0;
	if (!M2Py_AsInteger64(poItem, &val))
		return false;
	*ret = static_cast<unsigned long>(val);
	return true;
}

bool PyTuple_GetUnsignedInteger(PyObject* poArgs, int pos, unsigned int* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	long long val = 0;
	if (!M2Py_AsInteger64(poItem, &val))
		return false;
	*ret = static_cast<unsigned int>(val);
	return true;
}

bool PyTuple_GetString(PyObject* poArgs, int pos, char** ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	if (!M2PyString_Check(poItem))
		return false;

	char* c_szRet = M2PyString_AsString(poItem);
	if (!c_szRet)
		return false;   // onceden NULL + bekleyen istisna ile true donup native tarafta NULL kullaniliyordu
	*ret = c_szRet;
	return true;
}

bool PyTuple_GetBoolean(PyObject* poArgs, int pos, bool* ret)
{
	if (!poArgs || !ret || pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	long long val = 0;
	if (!M2Py_AsInteger64(poItem, &val))
		return false;

	*ret = (val != 0);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, PyObject* poFunc, PyObject* poArgs)
{
	PyObject* poRet;

	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc(poClass, poFunc, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, const char* c_szFunc, PyObject* poArgs)
{
	PyObject* poRet;

	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc_ByPyString(PyObject* poClass, PyObject* poFuncName, PyObject* poArgs)
{
	PyObject* poRet;

	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc_ByPyString(poClass, poFuncName, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, bool* pisRet)
{
	PyObject* poRet;

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	long long val = 0;
	if (PyNumber_Check(poRet) && M2Py_AsInteger64(poRet, &val))
		*pisRet = (val != 0);
	else
		*pisRet = true;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, long * plRetValue)
{
	PyObject* poRet;

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	long long val = 0;
	if (PyNumber_Check(poRet) && M2Py_AsInteger64(poRet, &val))
	{
		*plRetValue = static_cast<long>(val);
		Py_DECREF(poRet);
		return true;
	}

	Py_DECREF(poRet);
	return false;
}

bool __PyCallClassMemberFunc_ByCString(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, PyObject** ppoRet)
{
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poFunc = PyObject_GetAttrString(poClass, (char *)c_szFunc);	// New Reference

	if (!poFunc)
	{
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc))
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs);	// New Reference

	if (!poRet)
	{
		if (g_pkExceptionSender)
			g_pkExceptionSender->Clear();

		PyErr_Print();

		if (g_pkExceptionSender)
			g_pkExceptionSender->Send();

		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

bool __PyCallClassMemberFunc_ByPyString(PyObject* poClass, PyObject* poFuncName, PyObject* poArgs, PyObject** ppoRet)
{
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poFunc = PyObject_GetAttr(poClass, poFuncName);	// New Reference

	if (!poFunc)
	{
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc))
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs);	// New Reference

	if (!poRet)
	{
		if (g_pkExceptionSender)
			g_pkExceptionSender->Clear();

		PyErr_Print();

		if (g_pkExceptionSender)
			g_pkExceptionSender->Send();

		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

bool __PyCallClassMemberFunc(PyObject* poClass, PyObject * poFunc, PyObject* poArgs, PyObject** ppoRet)
{
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!poFunc)
	{
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc))
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs);	// New Reference

	if (!poRet)
	{
		PyErr_Print();
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
