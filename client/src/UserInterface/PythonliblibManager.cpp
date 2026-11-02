#include "stdafx.h"
#include "PythonliblibManager.h"

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
