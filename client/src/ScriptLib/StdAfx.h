#pragma once

#ifdef MT2_STANDALONE_PYTHON3_TEST
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#ifdef _DEBUG
	#undef _DEBUG
	#include <Python.h>
	#define _DEBUG
#else
	#include <Python.h>
#endif
#include <frameobject.h>
#include <marshal.h>
typedef unsigned short WORD;
#include "PythonUtils.h"
#include "PythonMarshal.h"

void LogBox(const char* c_szMsg, const char* c_szCaption = nullptr, void* hWnd = nullptr);
void Trace(const char* c_szMsg);
void Tracen(const char* c_szMsg);
void TraceError(const char* c_szFormat, ...);
void initdbg();

class IPythonExceptionSender
{
	public:
		void Clear()
		{
			m_strExceptionString = "";
		}

		void RegisterExceptionString(const char * c_szString)
		{
			m_strExceptionString += c_szString;
		}

		virtual void Send() = 0;

	protected:
		std::string m_strExceptionString;
};

extern IPythonExceptionSender * g_pkExceptionSender;
#else // !MT2_STANDALONE_PYTHON3_TEST

#include "../eterLib/StdAfx.h"
#include "../eterGrnLib/StdAfx.h"

#include "../UserInterface/Locale_inc.h"
#ifdef AT
#undef AT // @warme667
#endif

#ifdef MT2_USE_PYTHON3
	#ifdef _DEBUG
		#undef _DEBUG
		#include <Python.h>
		#define _DEBUG
	#else
		#include <Python.h>
	#endif
	#include <frameobject.h>
	#include <marshal.h>
#else
	#ifdef _DEBUG
		#undef _DEBUG
		#include <Python27/Python.h>
		#define _DEBUG
	#else
		#include <Python27/Python.h>
	#endif
	#include <Python27/node.h>
	#include <Python27/grammar.h>
	#include <Python27/token.h>
	#include <Python27/parsetok.h>
	#include <Python27/errcode.h>
	#include <Python27/compile.h>
	#include <Python27/symtable.h>
	#include <Python27/eval.h>
	#include <Python27/marshal.h>
#endif

#ifdef AT
#undef AT // @warme667
#endif

#include "PythonUtils.h"
#include "PythonLauncher.h"
#include "PythonMarshal.h"
#include "Resource.h"

void initdbg();

// PYTHON_EXCEPTION_SENDER
class IPythonExceptionSender
{
	public:
		void Clear()
		{
			m_strExceptionString = "";
		}

		void RegisterExceptionString(const char * c_szString)
		{
			m_strExceptionString += c_szString;
		}

		virtual void Send() = 0;

	protected:
		std::string m_strExceptionString;
};

extern IPythonExceptionSender * g_pkExceptionSender;

void SetExceptionSender(IPythonExceptionSender * pkExceptionSender);
// END_OF_PYTHON_EXCEPTION_SENDER
#endif // MT2_STANDALONE_PYTHON3_TEST
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
