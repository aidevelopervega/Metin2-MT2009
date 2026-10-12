#pragma once
#include <Python27/frameobject.h>

#include "../eterBase/Singleton.h"

class CPythonLauncher : public CSingleton<CPythonLauncher>
{
	public:
		CPythonLauncher();
		virtual ~CPythonLauncher();

		void Clear();

		bool Create(const char* c_szProgramName="eter.python");
		void SetTraceFunc(int (*pFunc)(PyObject * obj, PyFrameObject * f, int what, PyObject *arg));
#ifndef __USE_CYTHON__
		bool RunFile(const char* c_szFileName);
		bool RunMemoryTextFile(const char* c_szFileName, UINT uFileSize, const VOID* c_pvFileData);
		bool RunLine(const char* c_szLine);
#endif
#ifndef __EXTRA_CYTHON__
		bool RunCompiledFile(const char* c_szFileName);
#endif
		const char* GetError();

	protected:
		PyObject* m_poModule;
		PyObject* m_poDic;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
