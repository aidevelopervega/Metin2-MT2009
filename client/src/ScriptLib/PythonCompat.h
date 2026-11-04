#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

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

// Py_BuildValue("s"/"z"/"s#") katı UTF-8 çözer: sunucu/locale kaynaklı cp1250/cp949 ham baytlarda UnicodeDecodeError + NULL döner (Py2'de char* ham bayttı).
// M2Py_BuildValue bu formatlarda dizgeleri M2PyString_FromString ile aynı "surrogateescape" semantiğiyle üretir (M2PyString_AsString ters yönde aynı baytları geri verir).
// Dizge içermeyen veya desteklenmeyen karakter içeren formatlar doğrudan Py_VaBuildValue'ya gider (davranış değişmez).
namespace m2py_detail
{
	inline bool IsSeparator(char c)
	{
		return c == ' ' || c == ',' || c == ':' || c == '\t';
	}

	inline bool FormatUsesEscapedString(const char* c_szFormat)
	{
		bool hasString = false;
		for (; *c_szFormat; ++c_szFormat)
		{
			const char c = *c_szFormat;
			if (c == 's' || c == 'z')
				hasString = true;
			else if (!IsSeparator(c) && !std::strchr("()#bBhHiIlkLKnfdONS", c))
				return false;
		}
		return hasString;
	}

	inline PyObject* BuildItem(const char*& f, va_list* ap);

	// chClose'a (veya format sonuna) kadar birimleri items'a ekler. Hata: items içindekiler serbest bırakılır, false döner.
	inline bool BuildSequence(const char*& f, va_list* ap, char chClose, std::vector<PyObject*>& items)
	{
		while (*f && *f != chClose)
		{
			if (IsSeparator(*f))
			{
				++f;
				continue;
			}
			PyObject* poItem = BuildItem(f, ap);
			if (!poItem)
			{
				for (PyObject* poDone : items)
					Py_DECREF(poDone);
				items.clear();
				return false;
			}
			items.push_back(poItem);
		}
		return true;
	}

	inline PyObject* MakeTuple(std::vector<PyObject*>& items)
	{
		PyObject* poTuple = PyTuple_New(static_cast<Py_ssize_t>(items.size()));
		if (!poTuple)
		{
			for (PyObject* poDone : items)
				Py_DECREF(poDone);
			return nullptr;
		}
		for (size_t i = 0; i < items.size(); ++i)
			PyTuple_SET_ITEM(poTuple, static_cast<Py_ssize_t>(i), items[i]);
		return poTuple;
	}

	inline PyObject* BuildItem(const char*& f, va_list* ap)
	{
		const char code = *f++;
		switch (code)
		{
			case '(':
			{
				std::vector<PyObject*> items;
				if (!BuildSequence(f, ap, ')', items))
					return nullptr;
				if (*f == ')')
					++f;
				return MakeTuple(items);
			}
			case 's':
			case 'z':
			{
				const char* c_szStr = va_arg(*ap, const char*);
				Py_ssize_t len = -1;
				if (*f == '#')
				{
					++f;
					len = va_arg(*ap, Py_ssize_t);
				}
				if (!c_szStr)
					Py_RETURN_NONE;
				if (len < 0)
					len = static_cast<Py_ssize_t>(std::strlen(c_szStr));
				return PyUnicode_DecodeUTF8(c_szStr, len, "surrogateescape");
			}
			case 'b': case 'B': case 'h': case 'H': case 'i':
				return PyLong_FromLong(va_arg(*ap, int));
			case 'I':
				return PyLong_FromUnsignedLong(va_arg(*ap, unsigned int));
			case 'l':
				return PyLong_FromLong(va_arg(*ap, long));
			case 'k':
				return PyLong_FromUnsignedLong(va_arg(*ap, unsigned long));
			case 'L':
				return PyLong_FromLongLong(va_arg(*ap, long long));
			case 'K':
				return PyLong_FromUnsignedLongLong(va_arg(*ap, unsigned long long));
			case 'n':
				return PyLong_FromSsize_t(va_arg(*ap, Py_ssize_t));
			case 'f': case 'd':
				return PyFloat_FromDouble(va_arg(*ap, double));
			case 'O':
			case 'S':
			case 'N':
			{
				PyObject* poObj = va_arg(*ap, PyObject*);
				if (!poObj)
				{
					if (!PyErr_Occurred())
						PyErr_SetString(PyExc_SystemError, "NULL object passed to M2Py_BuildValue");
					return nullptr;
				}
				if (code != 'N')
					Py_INCREF(poObj);
				return poObj;
			}
		}
		PyErr_SetString(PyExc_SystemError, "bad format char passed to M2Py_BuildValue");
		return nullptr;
	}
}

inline PyObject* M2Py_BuildValue(const char* c_szFormat, ...)
{
	va_list ap;
	va_start(ap, c_szFormat);
	PyObject* poResult = nullptr;

	if (!m2py_detail::FormatUsesEscapedString(c_szFormat))
	{
		poResult = Py_VaBuildValue(c_szFormat, ap);
	}
	else
	{
		std::vector<PyObject*> items;
		const char* f = c_szFormat;
		if (m2py_detail::BuildSequence(f, &ap, '\0', items))
		{
			if (items.size() == 1)
				poResult = items[0];
			else
				poResult = m2py_detail::MakeTuple(items);
		}
	}

	va_end(ap);
	return poResult;
}

// Tüm istemci modülleri (ScriptLib/PythonUtils.h üzerinden) Py_BuildValue çağrılarını bu sarmalayıcıdan geçirir; referans için "(Py_BuildValue)(...)" kullanılır.
#define Py_BuildValue(...) M2Py_BuildValue(__VA_ARGS__)

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
