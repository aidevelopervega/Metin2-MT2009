#include "stdafx.h"
#include "PythonliblibManager.h"
#ifndef MT2_USE_PYTHON3
#include "Python27/Python.h"
#ifdef _DEBUG
	#pragma comment (lib, "liblib.lib")
#else
	#pragma comment (lib, "liblib.lib")
#endif

struct liblib_SMethodDef
{
	const char* func_name;
	void (*func)();
};

PyMODINIT_FUNC initabc();
PyMODINIT_FUNC initbase64();
PyMODINIT_FUNC initbisect();
PyMODINIT_FUNC initcodecs();
PyMODINIT_FUNC initcollections();
PyMODINIT_FUNC initcopy();
PyMODINIT_FUNC initcopy_reg();
PyMODINIT_FUNC initdecimal();
PyMODINIT_FUNC initdifflib();
PyMODINIT_FUNC initencodings();
PyMODINIT_FUNC initencodings_aliases();
PyMODINIT_FUNC initfnmatch();
PyMODINIT_FUNC initfunctools();
PyMODINIT_FUNC initgenericpath();
PyMODINIT_FUNC inithashlib();
PyMODINIT_FUNC initheapq();
PyMODINIT_FUNC initkeyword();
PyMODINIT_FUNC initlinecache();
PyMODINIT_FUNC initlocale();
PyMODINIT_FUNC initntpath();
PyMODINIT_FUNC initnumbers();
PyMODINIT_FUNC initordered_dict();
PyMODINIT_FUNC initos();
PyMODINIT_FUNC initrandom();
PyMODINIT_FUNC initre();
PyMODINIT_FUNC initshlex();
PyMODINIT_FUNC initshutil();
PyMODINIT_FUNC initsimplejson();
PyMODINIT_FUNC initsimplejson_compat();
PyMODINIT_FUNC initsimplejson_decoder();
PyMODINIT_FUNC initsimplejson_encoder();
PyMODINIT_FUNC initsimplejson_errors();
PyMODINIT_FUNC initsimplejson_raw_json();
PyMODINIT_FUNC initsimplejson_scanner();
PyMODINIT_FUNC initsite();
PyMODINIT_FUNC initsre_compile();
PyMODINIT_FUNC initsre_constants();
PyMODINIT_FUNC initsre_parse();
PyMODINIT_FUNC initstat();
PyMODINIT_FUNC initstring();
PyMODINIT_FUNC initstruct();
PyMODINIT_FUNC initsubprocess();
PyMODINIT_FUNC initsysconfig();
PyMODINIT_FUNC initthreading();
PyMODINIT_FUNC inittraceback();
PyMODINIT_FUNC inittypes();
PyMODINIT_FUNC initUserDict();
PyMODINIT_FUNC inituuid();
PyMODINIT_FUNC initwarnings();
PyMODINIT_FUNC initweakref();
PyMODINIT_FUNC initwebbrowser();
PyMODINIT_FUNC init_abcoll();
PyMODINIT_FUNC init_weakrefset();
PyMODINIT_FUNC init__future__();

liblib_SMethodDef liblib_init_methods[] =
{
	{ "abc", initabc },
	{ "base64", initbase64 },
	{ "bisect", initbisect },
	{ "codecs", initcodecs },
	{ "collections", initcollections },
	{ "copy", initcopy },
	{ "copy_reg", initcopy_reg },
	{ "decimal", initdecimal },
	{ "difflib", initdifflib },
	{ "encodings", initencodings },
	{ "encodings_aliases", initencodings_aliases },
	{ "fnmatch", initfnmatch },
	{ "functools", initfunctools },
	{ "genericpath", initgenericpath },
	{ "hashlib", inithashlib },
	{ "heapq", initheapq },
	{ "keyword", initkeyword },
	{ "linecache", initlinecache },
	{ "locale", initlocale },
	{ "ntpath", initntpath },
	{ "numbers", initnumbers },
	{ "ordered_dict", initordered_dict },
	{ "os", initos },
	{ "random", initrandom },
	{ "re", initre },
	{ "shlex", initshlex },
	{ "shutil", initshutil },
	{ "simplejson", initsimplejson },
	{ "simplejson_compat", initsimplejson_compat },
	{ "simplejson_decoder", initsimplejson_decoder },
	{ "simplejson_encoder", initsimplejson_encoder },
	{ "simplejson_errors", initsimplejson_errors },
	{ "simplejson_raw_json", initsimplejson_raw_json },
	{ "simplejson_scanner", initsimplejson_scanner },
	{ "site", initsite },
	{ "sre_compile", initsre_compile },
	{ "sre_constants", initsre_constants },
	{ "sre_parse", initsre_parse },
	{ "stat", initstat },
	{ "string", initstring },
	{ "struct", initstruct },
	{ "subprocess", initsubprocess },
	{ "sysconfig", initsysconfig },
	{ "threading", initthreading },
	{ "traceback", inittraceback },
	{ "types", inittypes },
	{ "UserDict", initUserDict },
	{ "uuid", inituuid },
	{ "warnings", initwarnings },
	{ "weakref", initweakref },
	{ "webbrowser", initwebbrowser },
	{ "_abcoll", init_abcoll },
	{ "_weakrefset", init_weakrefset },
	{ "__future__", init__future__ },
	{ NULL, NULL },
};

static PyObject* liblib_isExist(PyObject *self, PyObject *args)
{
	char* func_name;

	if(!PyArg_ParseTuple(args, "s", &func_name))
		return NULL;

	for (int i = 0; NULL != liblib_init_methods[i].func_name;i++)
	{
		if (0 == _stricmp(liblib_init_methods[i].func_name, func_name))
		{
			return Py_BuildValue("i", 1);
		}
	}
	return Py_BuildValue("i", 0);
}

static PyObject* liblib_moduleImport(PyObject *self, PyObject *args)
{
	char* func_name;

	if(!PyArg_ParseTuple(args, "s", &func_name))
		return NULL;

	for (int i = 0; NULL != liblib_init_methods[i].func_name;i++)
	{
		if (0 == _stricmp(liblib_init_methods[i].func_name, func_name))
		{
			liblib_init_methods[i].func();
			if (PyErr_Occurred())
				return NULL;

			PyObject* m = PyDict_GetItemString(PyImport_GetModuleDict(), liblib_init_methods[i].func_name);
			if (m == NULL) {
				PyErr_SetString(PyExc_SystemError,
					"dynamic module not initialized properly");
				return NULL;
			}
			Py_INCREF(m);
			return Py_BuildValue("S", m);
		}
	}
	return NULL;
}

static PyObject* liblib_getList(PyObject *self, PyObject *args)
{
	int iTupleSize = 0;
	while (NULL != liblib_init_methods[iTupleSize].func_name) {iTupleSize++;}

	PyObject* retTuple = PyTuple_New(iTupleSize);
	for (int i = 0; NULL != liblib_init_methods[i].func_name; i++)
	{
		PyObject* retSubString = M2PyString_FromString(liblib_init_methods[i].func_name);
		PyTuple_SetItem(retTuple, i, retSubString);
	}
	return retTuple;
}
#else
struct liblib_SPy3ModuleAlias
{
	const char* legacy_name;
	const char* py3_target;
};

static const liblib_SPy3ModuleAlias s_liblibPy3Modules[] =
{
	{ "abc", "abc" },
	{ "base64", "base64" },
	{ "bisect", "bisect" },
	{ "codecs", "codecs" },
	{ "collections", "collections" },
	{ "copy", "copy" },
	{ "copy_reg", "copyreg" },
	{ "decimal", "decimal" },
	{ "difflib", "difflib" },
	{ "encodings", "encodings" },
	{ "encodings_aliases", "encodings.aliases" },
	{ "fnmatch", "fnmatch" },
	{ "functools", "functools" },
	{ "genericpath", "genericpath" },
	{ "hashlib", "hashlib" },
	{ "heapq", "heapq" },
	{ "keyword", "keyword" },
	{ "linecache", "linecache" },
	{ "locale", "locale" },
	{ "ntpath", "ntpath" },
	{ "numbers", "numbers" },
	{ "ordered_dict", "collections" },
	{ "os", "os" },
	{ "random", "random" },
	{ "re", "re" },
	{ "shlex", "shlex" },
	{ "shutil", "shutil" },
	{ "simplejson", "json" },
	{ "simplejson_compat", "json" },
	{ "simplejson_decoder", "json.decoder" },
	{ "simplejson_encoder", "json.encoder" },
	{ "simplejson_errors", "json" },
	{ "simplejson_raw_json", "json" },
	{ "simplejson_scanner", "json.scanner" },
	{ "site", "site" },
	{ "sre_compile", "sre_compile" },
	{ "sre_constants", "sre_constants" },
	{ "sre_parse", "sre_parse" },
	{ "stat", "stat" },
	{ "string", "string" },
	{ "struct", "struct" },
	{ "subprocess", "subprocess" },
	{ "sysconfig", "sysconfig" },
	{ "threading", "threading" },
	{ "traceback", "traceback" },
	{ "types", "types" },
	{ "UserDict", "collections" },
	{ "uuid", "uuid" },
	{ "warnings", "warnings" },
	{ "weakref", "weakref" },
	{ "webbrowser", "webbrowser" },
	{ "_abcoll", "collections.abc" },
	{ "_weakrefset", "_weakrefset" },
	{ "__future__", "__future__" },
	{ NULL, NULL },
};

static PyObject* liblib_isExist(PyObject* self, PyObject* args)
{
	char* func_name = NULL;
	if (!PyArg_ParseTuple(args, "s", &func_name))
		return NULL;

	for (int i = 0; NULL != s_liblibPy3Modules[i].legacy_name; ++i)
	{
		if (0 == _stricmp(s_liblibPy3Modules[i].legacy_name, func_name))
			return PyLong_FromLong(1);
	}
	return PyLong_FromLong(0);
}

static PyObject* liblib_moduleImport(PyObject* self, PyObject* args)
{
	char* func_name = NULL;
	if (!PyArg_ParseTuple(args, "s", &func_name))
		return NULL;

	for (int i = 0; NULL != s_liblibPy3Modules[i].legacy_name; ++i)
	{
		if (0 == _stricmp(s_liblibPy3Modules[i].legacy_name, func_name))
		{
			PyObject* m = PyImport_ImportModule(s_liblibPy3Modules[i].py3_target);
			if (!m)
				return NULL;

			if (0 != strcmp(s_liblibPy3Modules[i].legacy_name, s_liblibPy3Modules[i].py3_target))
			{
				PyObject* modulesDict = PyImport_GetModuleDict();
				if (modulesDict)
					PyDict_SetItemString(modulesDict, s_liblibPy3Modules[i].legacy_name, m);
			}
			return m;
		}
	}

	PyErr_Format(PyExc_ImportError, "No module named '%s' in liblib", func_name ? func_name : "");
	return NULL;
}

static PyObject* liblib_getList(PyObject* self, PyObject* args)
{
	int iTupleSize = 0;
	while (NULL != s_liblibPy3Modules[iTupleSize].legacy_name)
		++iTupleSize;

	PyObject* retTuple = PyTuple_New(iTupleSize);
	if (!retTuple)
		return NULL;

	for (int i = 0; i < iTupleSize; ++i)
	{
		PyObject* retSubString = M2PyString_FromString(s_liblibPy3Modules[i].legacy_name);
		PyTuple_SetItem(retTuple, i, retSubString);
	}
	return retTuple;
}
#endif

void initliblibManager()
{
	static struct PyMethodDef methods[] =
	{
		{"isExist", liblib_isExist, METH_VARARGS},
		{"moduleImport", liblib_moduleImport, METH_VARARGS},
		{"getList", liblib_getList, METH_VARARGS},
		{NULL, NULL},
	};

	PyObject* m;
	m = M2Py_InitModule("liblib", methods);
}

// Files shared by GameCore.top
