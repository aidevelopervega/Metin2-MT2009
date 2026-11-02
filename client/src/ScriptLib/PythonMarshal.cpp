/* Write Python objects to files and read them back.
   This is intended for writing and reading compiled Python code only;
   a true persistent storage facility would be much harder, since
   it would have to take circular links and sharing into account. */

#include "Stdafx.h"

typedef struct SWFILE
{
	FILE *		fp;
	int			error;
	int			depth;
	PyObject *	str;
	char *		ptr;
	char *		end;
} WFILE;

typedef WFILE RFILE; /* Same struct with different invariants */

#define rs_byte(p) (((p)->ptr != (p)->end) ? (unsigned char)*(p)->ptr++ : EOF)

#define r_byte(p) ((p)->fp ? getc((p)->fp) : rs_byte(p))

static int r_short(RFILE *p)
{
	short x;
	x = (short) r_byte(p);
	x |= (short) r_byte(p) << 8;
	/* Sign-extension, in case short greater than 16 bits */
	x |= -(x & 0x8000);
	return x;
}

static long r_long(RFILE *p)
{
	long x;
	FILE *fp = p->fp;
	if (fp) {
		x = getc(fp);
		x |= (long)getc(fp) << 8;
		x |= (long)getc(fp) << 16;
		x |= (long)getc(fp) << 24;
	}
	else {
		x = rs_byte(p);
		x |= (long)rs_byte(p) << 8;
		x |= (long)rs_byte(p) << 16;
		x |= (long)rs_byte(p) << 24;
	}
#if SIZEOF_LONG > 4
	/* Sign extension for 64-bit machines */
	x |= -(x & 0x80000000L);
#endif
	return x;
}

int _PyMarshal_ReadShortFromFile(FILE *fp)
{
	RFILE rf;
	rf.fp = fp;
	return r_short(&rf);
}

long _PyMarshal_ReadLongFromFile(FILE *fp)
{
	RFILE rf;
	rf.fp = fp;
	return r_long(&rf);
}

/* Return size of file in bytes; < 0 if unknown. */
static off_t getfilesize(FILE *fp)
{
	struct stat st;
	if (fstat(fileno(fp), &st) != 0)
		return -1;
	else
		return st.st_size;
}

/* If we can get the size of the file up-front, and it's reasonably small,
 * read it in one gulp and delegate to ...FromString() instead.  Much quicker
 * than reading a byte at a time from file; speeds .pyc imports.
 * CAUTION:  since this may read the entire remainder of the file, don't
 * call it unless you know you're done with the file.
 */
PyObject *_PyMarshal_ReadLastObjectFromFile(FILE *fp)
{
#define SMALL_FILE_LIMIT (1L << 14)
#define REASONABLE_FILE_LIMIT (1L << 18)

	off_t filesize;

	if (PyErr_Occurred())
	{
		fprintf(stderr, "XXX rd_object called with exception set\n");
		return NULL;
	}

	filesize = getfilesize(fp);
	if (filesize > 0) {
		char buf[SMALL_FILE_LIMIT];
		char* pBuf = NULL;
		if (filesize <= SMALL_FILE_LIMIT)
			pBuf = buf;
		else if (filesize <= REASONABLE_FILE_LIMIT)
			pBuf = (char *)PyMem_MALLOC(filesize);
		if (pBuf != NULL) {
			PyObject* v;
			size_t n = fread(pBuf, 1, filesize, fp);
			v = PyMarshal_ReadObjectFromString(pBuf, static_cast<Py_ssize_t>(n));
			if (pBuf != buf)
				PyMem_FREE(pBuf);
			return v;
		}

	}

	return _PyMarshal_ReadObjectFromFile(fp);
#undef SMALL_FILE_LIMIT
#undef REASONABLE_FILE_LIMIT
}

PyObject * _PyMarshal_ReadObjectFromFile(FILE *fp)
{
	if (!fp || PyErr_Occurred())
		return NULL;
	long cur = ftell(fp);
	if (cur < 0 || fseek(fp, 0, SEEK_END) != 0)
		return NULL;
	long end = ftell(fp);
	if (end < cur || fseek(fp, cur, SEEK_SET) != 0)
		return NULL;
	size_t rem = static_cast<size_t>(end - cur);
	char* pBuf = static_cast<char*>(PyMem_MALLOC(rem > 0 ? rem : 1));
	if (!pBuf)
		return PyErr_NoMemory();
	size_t n = fread(pBuf, 1, rem, fp);
	PyObject* v = PyMarshal_ReadObjectFromString(pBuf, static_cast<Py_ssize_t>(n));
	PyMem_FREE(pBuf);
	return v;
}

PyObject * _PyMarshal_ReadObjectFromString(char *str, int len)
{
	if (PyErr_Occurred())
		return NULL;
	return PyMarshal_ReadObjectFromString(str, static_cast<Py_ssize_t>(len));
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
