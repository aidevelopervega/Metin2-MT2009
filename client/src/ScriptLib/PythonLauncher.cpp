#include "StdAfx.h"
#include <frameobject.h>
#include "../eterPack/EterPackManager.h"

#include "PythonLauncher.h"
#ifdef __THEMIDA__
#include "ThemidaSDK.h"
#endif

CPythonLauncher::CPythonLauncher()
{
	Py_NoSiteFlag = 1;
	Py_Initialize();
}

CPythonLauncher::~CPythonLauncher()
{
	Clear();
}

void CPythonLauncher::Clear()
{
	Py_Finalize();
}

std::string g_stTraceBuffer[512];
int	g_nCurTraceN = 0;

static std::string M2FormatFetchedException(PyObject* exc, PyObject* v)
{
	std::string result;
	if (exc && PyExceptionClass_Check(exc))
	{
		const char* className = PyExceptionClass_Name(exc);
		if (className && *className)
			result.append(className);
	}
	if (v && v != Py_None)
	{
		const char* errStr = nullptr;
		PyObject* vStrObj = nullptr;
		if (M2PyString_Check(v))
		{
			errStr = M2PyString_AsString(v);
		}
		else
		{
			vStrObj = PyObject_Str(v);
			if (vStrObj)
				errStr = M2PyString_AsString(vStrObj);
			else
				PyErr_Clear();
		}
		if (errStr && *errStr)
		{
			if (!result.empty())
				result.append(": ");
			result.append(errStr);
		}
		Py_XDECREF(vStrObj);
	}
	return result;
}

void Traceback()
{
	std::string str;

	for (int i = 0; i < g_nCurTraceN; ++i)
	{
		str.append(g_stTraceBuffer[i]);
		str.append("\n");
	}

	PyObject * exc = nullptr;
	PyObject * v = nullptr;
	PyObject * tb = nullptr;

	PyErr_Fetch(&exc, &v, &tb);

	const std::string errText = M2FormatFetchedException(exc, v);
	if (!errText.empty())
	{
		str.append("Error: ");
		str.append(errText);

		Tracef("%s\n", errText.c_str());
	}
	Py_XDECREF(exc);
	Py_XDECREF(v);
	Py_XDECREF(tb);
	LogBoxf("Traceback:\n\n%s\n", str.c_str());
}

int TraceFunc(PyObject * obj, PyFrameObject * f, int what, PyObject *arg)
{
	const char * funcname;
	char szTraceBuffer[128];

	switch (what)
	{
		case PyTrace_CALL:
			if (g_nCurTraceN >= 512)
				return 0;

			{
				PyCodeObject* pCode = PyFrame_GetCode(f);
				int iLineNo = PyFrame_GetLineNumber(f);
				funcname = pCode ? M2PyString_AsString(pCode->co_name) : "";
				const char* szFileName = pCode ? M2PyString_AsString(pCode->co_filename) : "";
				_snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Call: File \"%s\", line %d, in %s",
						  szFileName ? szFileName : "",
						  iLineNo,
						  funcname ? funcname : "");
				Py_XDECREF(pCode);
			}

			g_stTraceBuffer[g_nCurTraceN++]=szTraceBuffer;
			break;

		case PyTrace_RETURN:
			if (g_nCurTraceN > 0)
				--g_nCurTraceN;
			break;

		case PyTrace_EXCEPTION:
			if (g_nCurTraceN >= 512)
				return 0;

			PyObject * exc_type, * exc_value, * exc_traceback;

			PyTuple_GetObject(arg, 0, &exc_type);
			PyTuple_GetObject(arg, 1, &exc_value);
			PyTuple_GetObject(arg, 2, &exc_traceback);

			{
				PyCodeObject* pCode = PyFrame_GetCode(f);
				int iLineNo = PyFrame_GetLineNumber(f);
				const char* szFileName = pCode ? M2PyString_AsString(pCode->co_filename) : "";
				const char* szCodeName = pCode ? M2PyString_AsString(pCode->co_name) : "";
				_snprintf(szTraceBuffer, sizeof(szTraceBuffer), "Exception: File \"%s\", line %d, in %s",
						  szFileName ? szFileName : "",
						  iLineNo,
						  szCodeName ? szCodeName : "");
				Py_XDECREF(pCode);
			}

			g_stTraceBuffer[g_nCurTraceN++]=szTraceBuffer;

			break;
	}
	return 0;
}

void CPythonLauncher::SetTraceFunc(int (*pFunc)(PyObject * obj, PyFrameObject * f, int what, PyObject *arg))
{
	PyEval_SetTrace(pFunc, NULL);
}

bool CPythonLauncher::Create(const char* c_szProgramName)
{
	M2Py_SetProgramName(c_szProgramName);
#ifdef _DEBUG
	PyEval_SetTrace(TraceFunc, NULL);
#endif
	m_poModule = PyImport_AddModule((char *) "__main__");

	if (!m_poModule)
		return false;

	m_poDic = PyModule_GetDict(m_poModule);

    PyObject * builtins = M2Py_ImportBuiltins();
	PyModule_AddIntConstant(builtins, "TRUE", 1);
	PyModule_AddIntConstant(builtins, "FALSE", 0);
    PyDict_SetItemString(m_poDic, "__builtins__", builtins);
	Py_DECREF(builtins);
	return true;
}

#ifndef __EXTRA_CYTHON__
bool CPythonLauncher::RunCompiledFile(const char* c_szFileName)
{
	FILE * fp = fopen(c_szFileName, "rb");

	if (!fp)
		return false;

	PyCodeObject *co;
	PyObject *v;
	long magic;
	long PyImport_GetMagicNumber(void);

	magic = _PyMarshal_ReadLongFromFile(fp);

	if (magic != PyImport_GetMagicNumber())
	{
		PyErr_SetString(PyExc_RuntimeError, "Bad magic number in .pyc file");
		fclose(fp);
		return false;
	}

	_PyMarshal_ReadLongFromFile(fp);
	_PyMarshal_ReadLongFromFile(fp);
	_PyMarshal_ReadLongFromFile(fp);
	v = _PyMarshal_ReadLastObjectFromFile(fp);

	fclose(fp);

	if (!v || !PyCode_Check(v))
	{
		Py_XDECREF(v);
		PyErr_SetString(PyExc_RuntimeError, "Bad code object in .pyc file");
		return false;
	}

	co = (PyCodeObject *) v;
	v = M2PyEval_EvalCode(co, m_poDic, m_poDic);

	Py_DECREF(co);
	if (!v)
	{
		Traceback();
		return false;
	}

	Py_DECREF(v);
	PyErr_Clear();

	return true;
}
#endif

#ifndef __USE_CYTHON__
bool CPythonLauncher::RunMemoryTextFile(const char* c_szFileName, UINT uFileSize, const VOID* c_pvFileData)
{
	const CHAR* c_pcFileData=(const CHAR*)c_pvFileData;

	std::string stConvFileData;
	stConvFileData.reserve(uFileSize);
	for (UINT i = 0; i < uFileSize; ++i)
	{
		if (c_pcFileData[i] != 13)
			stConvFileData += c_pcFileData[i];
	}

	PyObject* pySrc = PyUnicode_DecodeUTF8(stConvFileData.data(), static_cast<Py_ssize_t>(stConvFileData.size()), "surrogateescape");
	if (!pySrc)
	{
		Traceback();
		return false;
	}

	PyObject* builtins = M2Py_ImportBuiltins();
	PyObject* compileFn = builtins ? PyObject_GetAttrString(builtins, "compile") : NULL;
	Py_XDECREF(builtins);
	if (!compileFn)
	{
		Py_DECREF(pySrc);
		Traceback();
		return false;
	}

	PyObject* co = PyObject_CallFunction(compileFn, "Oss", pySrc, c_szFileName ? c_szFileName : "<memory>", "exec");
	Py_DECREF(compileFn);
	Py_DECREF(pySrc);
	if (!co)
	{
		Traceback();
		return false;
	}

	PyObject* v = M2PyEval_EvalCode(reinterpret_cast<PyCodeObject*>(co), m_poDic, m_poDic);
	Py_DECREF(co);
	if (!v)
	{
		Traceback();
		return false;
	}
	Py_DECREF(v);
	return true;
}

bool CPythonLauncher::RunFile(const char* c_szFileName)
{
	std::string acBufData;

	{
		CMappedFile file;
		const VOID* pvData;
		CEterPackManager::Instance().Get(file, c_szFileName, &pvData);

		if (file.Size() == 0)
			return false;

		acBufData.resize(file.Size());
		memcpy(acBufData.data(), pvData, acBufData.size());
	}

	return RunMemoryTextFile(c_szFileName, acBufData.size(), acBufData.data());
}

bool CPythonLauncher::RunLine(const char* c_szSrc)
{
	PyObject* v = PyRun_String((char*)c_szSrc, Py_file_input, m_poDic, m_poDic);

	if (!v)
	{
		Traceback();
		return false;
	}

	Py_DECREF(v);
	return true;
}
#endif

const char* CPythonLauncher::GetError()
{
	static thread_local std::string s_stLastError;
	s_stLastError.clear();

	PyObject* exc = nullptr;
	PyObject* v = nullptr;
	PyObject* tb = nullptr;

	PyErr_Fetch(&exc, &v, &tb);

	if (v && v != Py_None)
	{
		if (M2PyString_Check(v))
		{
			const char* c_szErr = M2PyString_AsString(v);
			if (c_szErr)
				s_stLastError = c_szErr;
		}
		else
		{
			PyObject* poStr = PyObject_Str(v);
			if (poStr)
			{
				const char* c_szErr = M2PyString_AsString(poStr);
				if (c_szErr)
					s_stLastError = c_szErr;
				Py_DECREF(poStr);
			}
			else
			{
				PyErr_Clear();
			}
		}
	}

	if (s_stLastError.empty() && exc && PyExceptionClass_Check(exc))
	{
		const char* c_szClass = PyExceptionClass_Name(exc);
		if (c_szClass)
			s_stLastError = c_szClass;
	}

	Py_XDECREF(exc);
	Py_XDECREF(v);
	Py_XDECREF(tb);

	return s_stLastError.c_str();
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
