#include "stdafx.h"
#include "PythonliblibManager.h"
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
		PyObject* retSubString = PyString_FromString(liblib_init_methods[i].func_name);
		PyTuple_SetItem(retTuple, i, retSubString);
	}
	return retTuple;
}

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
	m = Py_InitModule("liblib", methods);
}

// Files shared by GameCore.top
