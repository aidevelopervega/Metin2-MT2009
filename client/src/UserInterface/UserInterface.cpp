#include "StdAfx.h"
#include "PythonApplication.h"
#include "ProcessScanner.h"
#include "PythonExceptionSender.h"
#include "resource.h"
#include "Version.h"
#include "ThemidaSDK.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include "../eterPack/EterPackManager.h"
#include "../eterLib/Util.h"
#include "../CWebBrowser/CWebBrowser.h"
#include "../eterBase/CPostIt.h"

#include "CheckLatestFiles.h"

extern "C" {
extern int _fltused;
volatile int _AVOID_FLOATING_POINT_LIBRARY_BUG = _fltused;
};

#pragma comment(linker, "/NODEFAULTLIB:libci.lib")

#pragma comment( lib, "version.lib" )

#pragma comment( lib, "imagehlp.lib" )
#pragma comment( lib, "devil.lib" )

#if GrannyProductMinorVersion==4
#pragma comment( lib, "granny2.4.0.10.lib" )
#elif GrannyProductMinorVersion==7
#pragma comment( lib, "granny2.7.0.30.lib" )
#elif GrannyProductMinorVersion==8
#pragma comment( lib, "granny2.8.49.0.lib" )
#elif GrannyProductMinorVersion==9
#pragma comment( lib, "granny2.9.12.0.lib" )
#elif GrannyProductMinorVersion==11
#pragma comment( lib, "granny2.11.8.0.lib" )
#else
#error "unknown granny version"
#endif
#pragma comment( lib, "mss32.lib" )
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "imm32.lib" )
#pragma comment( lib, "oldnames.lib" )
#pragma comment( lib, "SpeedTreeRT.lib" )
#pragma comment( lib, "dinput8.lib" )
#pragma comment( lib, "dxguid.lib" )
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "strmiids.lib" )
#pragma comment( lib, "ddraw.lib" )
#pragma comment( lib, "dmoguids.lib" )
//#pragma comment( lib, "wsock32.lib" )
#include <stdlib.h>
#include <cryptopp/cryptoppLibLink.h>
bool __IS_TEST_SERVER_MODE__=false;

#ifdef __USE_CYTHON__
#ifndef __CYTHON_PYD__
// don't include these two files .h .cpp if you're implementing cython via .pyd
#include "PythonrootlibManager.h"
// it would be better including such file in the project, but this is easier at this moment:
//#include "PythonrootlibManager.cpp"

#ifdef __EXTRA_CYTHON__
// don't include these two files .h .cpp if you're implementing cython via .pyd
#include "PythonuiscriptlibManager.h"
#include "PythonliblibManager.h"
// it would be better including such file in the project, but this is easier at this moment:
//#include "PythonuiscriptlibManager.cpp"
#endif

#endif
#endif


#ifdef LEV_ANTICHEAT
#include "MemoryModule.h"
#include "dll_bytes.h"
#include "junkcode.h"

size_t dllData_len = sizeof(dllData);

FARPROC GetObfuscatedFunction(HMEMORYMODULE hModule, const char* funcName) {
	char key = 0x5A;
	std::string obfuscatedName(funcName);
	for (auto& ch : obfuscatedName) {
		ch ^= key; // prosty xor tylko na potrzeby ukrycia stringa
	}
	return MemoryGetProcAddress(hModule, obfuscatedName.c_str());
}

void CallFunction(void* func) { // skitrany call w assamblerze ktory dodatkowo podczas dekompilacji rozpierdala funkcje i ciezko sie odnalezc

	JUNK_CODE_1
		__asm {
		mov eax, func
		call eax
	}
	JUNK_CODE_2

}


void Step1Junk(HMEMORYMODULE hModule) { // taktyczna zmylka ktora nic nie robi

	struct FakeData {
		int id;
		uint32_t flags;
		float value;
	};

	FakeData data[3] = {
		{ 1, 0xDEADBEEF, 3.14f },
		{ 2, 0xBADF00D, 1.618f },
		{ 3, 0xCAFEBABE, 2.718f }
	};

	for (int i = 0; i < 3; i++) {
		volatile int temp = data[i].id * 2;
		temp ^= data[i].flags;
		temp += static_cast<int>(data[i].value);
	}

	unsigned char hash[16] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xBA, 0xAD, 0xF0, 0x0D,
							   0xC0, 0xFF, 0xEE, 0x42, 0x13, 0x37, 0xAB, 0xCD };

	for (int i = 0; i < 16; i++) {
		hash[i] ^= (unsigned char)(i * 31);
	}

	void* fakeBuffer = malloc(128);
	if (fakeBuffer) {
		memset(fakeBuffer, 0, 128);
		free(fakeBuffer);
	}

	char* fakePointer = (char*)hModule;
	if (fakePointer) {
		fakePointer += 16;
		volatile char temp = *fakePointer;
	}

	__asm {
		mov eax, 0x12345678
		xor eax, 0x87654321
		add eax, 0xABCDEF
		sub eax, 0xABCDEF
		xor eax, 0x12345678

		mov ebx, 0xDEADBEEF
		not ebx
		xor ebx, 0xBADF00D
		add ebx, 0xCAFEBABE
		sub ebx, 0xCAFEBABE

		mov ecx, 0x1337C0DE
		rol ecx, 4
		ror ecx, 4
		xor ecx, ecx
	}
}

void FuncCall(FARPROC func) {
	CallFunction((void*)func);
}

HMEMORYMODULE InitDXmodule() {
#ifdef __THEMIDA__
	VM_START
#endif
		auto result = MemoryLoadLibrary(dllData, sizeof(dllData));
#ifdef __THEMIDA__
	VM_END
#endif
	return result;
}

void StartAntiCheat() {

	HMEMORYMODULE hModule = InitDXmodule();
	if (hModule != NULL) {
		const char funcName[] = { 'D' ^ 0x5A, 'X' ^ 0x5A, '9' ^ 0x5A, 'I' ^ 0x5A, 'n' ^ 0x5A, 'i' ^ 0x5A, 't' ^ 0x5A,
								  'C' ^ 0x5A, 'o' ^ 0x5A, 'n' ^ 0x5A, 't' ^ 0x5A, 'e' ^ 0x5A, 'x' ^ 0x5A, 't' ^ 0x5A, '\0' }; // ukryty string

		FARPROC pInitThreadFunc = GetObfuscatedFunction(hModule, funcName);
		if (pInitThreadFunc) {
			Step1Junk(hModule);
			FuncCall(pInitThreadFunc);
		}
	}

}
#endif


#ifndef _DEBUG
// #define ENABLE_PYLIB_CHECK
#endif

#ifdef ENABLE_PYLIB_CHECK
#define ENABLE_MILES_CHECK
#endif
// #define ENABLE_DAEMONPROTECTION

#ifdef ENABLE_DAEMONPROTECTION
#define DPDLL_FILENAME	"dpdll.dll"
#define DPDLL_CRC32		0x48104810
#define DPDLL_FILESIZE	1234567+4
#endif

extern bool SetDefaultCodePage(DWORD codePage);

static const char * sc_apszPythonLibraryFilenames[] =
{
	"UserDict.py",
	"__future__.py",
	"copy_reg.py",
	"linecache.py",
	"ntpath.py",
	"os.py",
	"site.py",
	"stat.py",
	"string.py",
	"traceback.py",
	"types.py",
	"\n",
};

#ifdef ENABLE_PYLIB_CHECK
constexpr int PrintLevel = 0;
#define PYFOLD "./lib"

template <class ...Args>
void PrintMe(int level, const Args& ...Arguments)
{
	if (PrintLevel >= level) TraceError(Arguments...);
}

// #define PYFORCE

typedef struct PyLibFiles_s
{
	std::string sFileName;
	size_t stSize;
	DWORD dwCRC32;
} PyLibFiles_t;

PyLibFiles_t PyLibFilesTable[] =
{
#if PY_VERSION_HEX==0x020706f0
	{ PYFOLD"/abc.pyc", 6187, 3834771731 },
	{ PYFOLD"/base64.pyc", 10487, 1996714007 },
	{ PYFOLD"/bisect.pyc", 3236, 3116899751 },
	{ PYFOLD"/codecs.pyc", 36978, 2928014693 },
	{ PYFOLD"/collections.pyc", 26172, 385366131 },
	{ PYFOLD"/copy.pyc", 12170, 3615987315 },
	{ PYFOLD"/copy_reg.pyc", 5157, 536292604 },
	{ PYFOLD"/decimal.pyc", 166979, 1923314712 },
	{ PYFOLD"/difflib.pyc", 60764, 2221213162 },
	{ PYFOLD"/fnmatch.pyc", 3732, 4270526278 },
	{ PYFOLD"/functools.pyc", 6193, 3257285433 },
	{ PYFOLD"/genericpath.pyc", 3303, 1652596334 },
	{ PYFOLD"/hashlib.pyc", 4200, 2503749184 },
	{ PYFOLD"/heapq.pyc", 13896, 2948659214 },
	{ PYFOLD"/keyword.pyc", 2169, 2178546341 },
	{ PYFOLD"/linecache.pyc", 3235, 4048207604 },
	{ PYFOLD"/locale.pyc", 49841, 4114662314 },
	{ PYFOLD"/ntpath.pyc", 11961, 2765879465 },
	{ PYFOLD"/numbers.pyc", 12958, 4255643833 },
	{ PYFOLD"/os.pyc", 25769, 911432770 },
	{ PYFOLD"/pyexpat.pyd", 127488, 2778492911 },
	{ PYFOLD"/pyexpat_d.pyd", 194560, 2589182738 },
	{ PYFOLD"/random.pyc", 24779, 385008081 },
	{ PYFOLD"/re.pyc", 13178, 1671609387 },
	{ PYFOLD"/rootlib.pyd", 15077376, 2433437728 },
	{ PYFOLD"/shlex.pyc", 7610, 1476140107 },
	{ PYFOLD"/shutil.pyc", 19273, 1873281015 },
	{ PYFOLD"/site.pyc", 20019, 3897044925 },
	{ PYFOLD"/sre_compile.pyc", 11107, 1620746411 },
	{ PYFOLD"/sre_constants.pyc", 6108, 3900811275 },
	{ PYFOLD"/sre_parse.pyc", 19244, 1459430047 },
	{ PYFOLD"/stat.pyc", 2791, 1375966108 },
	{ PYFOLD"/string.pyc", 19656, 1066063587 },
	{ PYFOLD"/struct.pyc", 222, 4038890026 },
	{ PYFOLD"/subprocess.pyc", 32616, 410545072 },
	{ PYFOLD"/sysconfig.pyc", 17571, 1529083148 },
	{ PYFOLD"/threading.pyc", 43020, 826693473 },
	{ PYFOLD"/traceback.pyc", 11703, 3768933732 },
	{ PYFOLD"/types.pyc", 2530, 920695307 },
	{ PYFOLD"/uiscriptlib.pyd", 2546176, 1681086392 },
	{ PYFOLD"/UserDict.pyc", 9000, 1431875928 },
	{ PYFOLD"/uuid.pyc", 20323, 3888846613 },
	{ PYFOLD"/warnings.pyc", 13232, 3752454002 },
	{ PYFOLD"/weakref.pyc", 16037, 2124701469 },
	{ PYFOLD"/webbrowser.pyc", 18805, 684340249 },
	{ PYFOLD"/_abcoll.pyc", 22339, 2365844594 },
	{ PYFOLD"/_locale.pyc", 49841, 4114662314 },
	{ PYFOLD"/_weakrefset.pyc", 10490, 1576811346 },
	{ PYFOLD"/__future__.pyc", 4431, 2857792867 },
	{ PYFOLD"/encodings/aliases.pyc", 8803, 3888310600 },
	{ PYFOLD"/encodings/cp949.pyc", 2009, 1824094431 },
	{ PYFOLD"/encodings/__init__.pyc", 4510, 2926961588 },
	{ PYFOLD"/simplejson/compat.py", 815, 3827205248 },
	{ PYFOLD"/simplejson/compat.pyc", 1146, 3730404197 },
	{ PYFOLD"/simplejson/decoder.py", 14519, 2619744402 },
	{ PYFOLD"/simplejson/decoder.pyc", 12084, 3426124783 },
	{ PYFOLD"/simplejson/encoder.py", 28570, 2368098015 },
	{ PYFOLD"/simplejson/encoder.pyc", 21129, 1695212591 },
	{ PYFOLD"/simplejson/errors.py", 1779, 1360890353 },
	{ PYFOLD"/simplejson/errors.pyc", 2259, 3051573946 },
	{ PYFOLD"/simplejson/ordered_dict.py", 2945, 3685384954 },
	{ PYFOLD"/simplejson/raw_json.py", 217, 343443373 },
	{ PYFOLD"/simplejson/raw_json.pyc", 593, 460690619 },
	{ PYFOLD"/simplejson/scanner.py", 2971, 2791326292 },
	{ PYFOLD"/simplejson/scanner.pyc", 2783, 1554352801 },
	{ PYFOLD"/simplejson/tool.py", 1136, 3612394048 },
	{ PYFOLD"/simplejson/__init__.py", 23745, 4143432427 },
	{ PYFOLD"/simplejson/__init__.pyc", 21690, 328477213 },
	{ PYFOLD"/xml/__init__.pyc", 1117, 3531597556 },
	{ PYFOLD"/xml/dom/domreg.pyc", 3506, 2127674645 },
	{ PYFOLD"/xml/dom/expatbuilder.pyc", 36698, 316034696 },
	{ PYFOLD"/xml/dom/minicompat.pyc", 4144, 747596376 },
	{ PYFOLD"/xml/dom/minidom.pyc", 74704, 1543233763 },
	{ PYFOLD"/xml/dom/minidomer.pyc", 73, 3191306169 },
	{ PYFOLD"/xml/dom/nodefilter.pyc", 1243, 3805409468 },
	{ PYFOLD"/xml/dom/xmlbuilder.pyc", 18659, 4118801318 },
	{ PYFOLD"/xml/dom/__init__.pyc", 7337, 343751384 },
	{ PYFOLD"/xml/parsers/expat.pyc", 326, 2425747752 },
	{ PYFOLD"/xml/parsers/invers.pyc", 16, 1384725046 },
	{ PYFOLD"/xml/parsers/__init__.pyc", 353, 1691127318 },
#elif PY_VERSION_HEX==0x020203f0
#else
#error "unknown python version"
#endif
};

bool checkPyLibDir(const string szDirName)
{
	bool HasHack = false;

	char szDirNamePath[MAX_PATH];
	sprintf(szDirNamePath, "%s\\*", szDirName.c_str());

	WIN32_FIND_DATA f;
	HANDLE h = FindFirstFile(szDirNamePath, &f);

	if (h == INVALID_HANDLE_VALUE) { return HasHack; }

	do
	{
		if (HasHack)
			break;
		const char * name = f.cFileName;

		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { continue; }

		if (f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			char filePath[MAX_PATH];
			sprintf(filePath, "%s%s%s", szDirName.c_str(), "\\", name);
			PrintMe(1, "sub %s", filePath);
            checkPyLibDir(filePath);
		}
		else
		{
			// start processing file
			PrintMe(1, "starting %s", name);
			std::string sName(name);
			std::string sPathName(szDirName+"/"+name);
			// change \\ to /
			std::replace(sPathName.begin(), sPathName.end(), '\\', '/');
			PrintMe(1, "path %s", sPathName.c_str());
			// lower file name
			std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);
			{
				PrintMe(1, "verify %s", sName.c_str());
				bool isPyLibFound = false;
				for (PyLibFiles_t *i1=std::begin(PyLibFilesTable), *e1=std::end(PyLibFilesTable); i1<e1; i1++)
				{
					if (!i1->sFileName.compare(sPathName))
					{
						PrintMe(1, "found %s==%s", i1->sFileName.c_str(), sName.c_str());
						DWORD dwCrc32 = GetFileCRC32(sPathName.c_str());
						// assert(dwCrc32);
						DWORD dwFileSize = f.nFileSizeLow;
						if (i1->stSize!=dwFileSize)
						{
							PrintMe(1, "wrong size %u==%u", i1->stSize, dwFileSize);
							HasHack = true;
							PrintMe(0, "wrong size %u for %s", dwFileSize, sPathName.c_str());
							return HasHack;
						}
						else if (i1->dwCRC32 != dwCrc32)
						{
							PrintMe(1, "wrong crc32 %u==%u", i1->dwCRC32, dwCrc32);
							HasHack = true;
							PrintMe(0, "wrong crc32 %u for %s", dwCrc32, sPathName.c_str());
							return HasHack;
						}
						PrintMe(1, "right size %u==%u", i1->stSize, dwFileSize);
						PrintMe(1, "right crc32 %u==%u", i1->dwCRC32, dwCrc32);
						PrintMe(2, "{ \"%s\", %u, %u},", sPathName.c_str(), dwFileSize, dwCrc32);
						isPyLibFound = true;
						break;
					}
				}
				// block ambiguous pyc/d files
				if (!isPyLibFound)
				{
					PrintMe(1, "not found %s", sName.c_str());
#ifdef PYFORCE
					HasHack = true;
					PRINTME(0, "ambiguous file for %s", sPathName.c_str());
					return HasHack;
#endif
				}
				PrintMe(1, "skipping file(%s) hack(%u) found(%u)", sName.c_str(), HasHack, isPyLibFound);
			}
		}

	} while (FindNextFile(h, &f));
	FindClose(h);
	return HasHack;
}

bool __CheckPyLibFiles()
{
	PrintMe(1, "__CheckPyLibFiles processing " PYFOLD);
	if (checkPyLibDir(PYFOLD))
		return false;
	return true;
}
#endif

#ifdef ENABLE_MILES_CHECK
#include <algorithm>
#include "../EterBase/Filename.h"
// #include "../EterBase/CRC32.h"

typedef struct MilesFiles_s
{
	std::string sFileName;
	size_t stSize;
	DWORD dwCRC32;
} MilesFiles_t;

typedef struct MilesExten_s
{
	std::string ExtName;
	bool IsUni;
} MilesExten_t;

MilesExten_t MilesExtenTable[] = {
	{"dll", false},
	{"asi", true},
	{"flt", true},
	{"m3d", true},
	{"mix", true},
};

MilesFiles_t MilesFilesTable[] =
{
	{"mss32.dll", 349696, 1817711331},
	{"mssa3d.m3d", 83456, 1812642892},
	{"mssds3d.m3d", 70656, 2704596484},
	{"mssdsp.flt", 93696, 3364819387},
	{"mssdx7.m3d", 80896, 236402185},
	{"msseax.m3d", 103424, 3195814903},
	{"mssmp3.asi", 125952, 1219814613},
	{"mssrsx.m3d", 354816, 550946743},
	{"msssoft.m3d", 67072, 4284421368},
	{"mssvoice.asi", 197120, 1407967464},
};

bool checkMilesDir(const string szDirName)
{
	bool HasHack = false;

	char szDirNamePath[MAX_PATH];
	sprintf(szDirNamePath, "%s\\*", szDirName.c_str());

	WIN32_FIND_DATA f;
	HANDLE h = FindFirstFile(szDirNamePath, &f);

	if (h == INVALID_HANDLE_VALUE) { return HasHack; }

	do
	{
		if (HasHack)
			break;
		const char * name = f.cFileName;

		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { continue; }

		if (f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			// do nothing if it's a folder
		}
		else
		{
			// start processing file
			PrintMe(1, "starting %s", name);
			std::string sName(name);
			std::string sPathName(szDirName+"/"+name);
			PrintMe(1, "path %s", sPathName.c_str());
			// lower file name
			std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);
			// file or symlink; check for asi flt m3d mix exploit
			string sNameExt = CFileNameHelper::GetExtension(sName);
			PrintMe(1, "ext %s", sNameExt.c_str());
			// workaround instead of std::find
			bool isMilesFile = false, isMilesUnique = false;
			for (MilesExten_t *i1=std::begin(MilesExtenTable), *e1=std::end(MilesExtenTable); i1<e1; i1++)
			{
				PrintMe(1, "is %s ? %s", i1->ExtName.c_str(), sNameExt.c_str());
				if (!sNameExt.compare(0, i1->ExtName.length(), i1->ExtName))
				{
					isMilesFile = true;
					isMilesUnique = i1->IsUni;
				}
			}
			if (isMilesFile)
			{
				PrintMe(1, "verify %s -> %s", sName.c_str(), sNameExt.c_str());
				bool isMilesFound = false;
				for (MilesFiles_t *i1=std::begin(MilesFilesTable), *e1=std::end(MilesFilesTable); i1<e1; i1++)
				{
					if (!i1->sFileName.compare(sName))
					{
						PrintMe(1, "found %s==%s", i1->sFileName.c_str(), sName.c_str());
						DWORD dwCrc32 = GetFileCRC32(sPathName.c_str());
						// assert(dwCrc32);
						DWORD dwFileSize = f.nFileSizeLow;
						if (i1->stSize!=dwFileSize)
						{
							PrintMe(1, "wrong size %u==%u", i1->stSize, dwFileSize);
							HasHack = true;
							PrintMe(0, "wrong size %u for %s", dwFileSize, sPathName.c_str());
							return HasHack;
						}
						else if (i1->dwCRC32 != dwCrc32)
						{
							PrintMe(1, "wrong crc32 %u==%u", i1->dwCRC32, dwCrc32);
							HasHack = true;
							PrintMe(0, "wrong crc32 %u for %s", dwCrc32, sPathName.c_str());
							return HasHack;
						}
						PrintMe(1, "right size %u==%u", i1->stSize, dwFileSize);
						PrintMe(1, "right crc32 %u==%u", i1->dwCRC32, dwCrc32);
						isMilesFound = true;
						break;
					}
				}
				// only mss32.dll is checked, and there's no need to check the others
				if (!isMilesFound && isMilesUnique)
				{
					PrintMe(1, "not found %s", sName.c_str());
					HasHack = true;
					PrintMe(0, "ambiguous file for %s", sPathName.c_str());
					return HasHack;
				}
				PrintMe(1, "skipping file(%s) hack(%u) found(%u) uni(%u)", sName.c_str(), HasHack, isMilesFound, isMilesUnique);
			}
		}

	} while (FindNextFile(h, &f));
	FindClose(h);
	return HasHack;
}

bool __CheckMilesFiles()
{
	PrintMe(1, "__CheckMilesFiles processing . and .\\miles");
	if (checkMilesDir(".") || checkMilesDir(".\\miles"))
		return false;
	return true;
}
#endif

char gs_szErrorString[512] = "";

void ApplicationSetErrorString(const char* szErrorString)
{
	strcpy(gs_szErrorString, szErrorString);
}

bool CheckPythonLibraryFilenames()
{
	for (int i = 0; *sc_apszPythonLibraryFilenames[i] != '\n'; ++i)
	{
		std::string stFilename = "lib\\";
		stFilename += sc_apszPythonLibraryFilenames[i];

		if (_access(stFilename.c_str(), 0) != 0)
		{
			return false;
		}

		MoveFile(stFilename.c_str(), stFilename.c_str());
	}

	return true;
}

struct ApplicationStringTable
{
	HINSTANCE m_hInstance;
	std::map<DWORD, std::string> m_kMap_dwID_stLocale;
} gs_kAppStrTable;

void ApplicationStringTable_Initialize(HINSTANCE hInstance)
{
	gs_kAppStrTable.m_hInstance=hInstance;
}

const std::string& ApplicationStringTable_GetString(DWORD dwID, LPCSTR szKey)
{
	char szBuffer[512];
	char szIniFileName[256];
	char szLocale[256];

	::GetCurrentDirectory(sizeof(szIniFileName), szIniFileName);
	if(szIniFileName[lstrlen(szIniFileName)-1] != '\\')
		strcat(szIniFileName, "\\");
	strcat(szIniFileName, "metin2client.dat");

	strcpy(szLocale, LocaleService_GetLocalePath());
	if(strnicmp(szLocale, "locale/", strlen("locale/")) == 0)
		strcpy(szLocale, LocaleService_GetLocalePath() + strlen("locale/"));
	::GetPrivateProfileString(szLocale, szKey, NULL, szBuffer, sizeof(szBuffer)-1, szIniFileName);
	if(szBuffer[0] == '\0')
		LoadString(gs_kAppStrTable.m_hInstance, dwID, szBuffer, sizeof(szBuffer)-1);
	if(szBuffer[0] == '\0')
		::GetPrivateProfileString("en", szKey, NULL, szBuffer, sizeof(szBuffer)-1, szIniFileName);
	if(szBuffer[0] == '\0')
		strcpy(szBuffer, szKey);

	std::string& rstLocale=gs_kAppStrTable.m_kMap_dwID_stLocale[dwID];
	rstLocale=szBuffer;

	return rstLocale;
}

const std::string& ApplicationStringTable_GetString(DWORD dwID)
{
	char szBuffer[512];

	LoadString(gs_kAppStrTable.m_hInstance, dwID, szBuffer, sizeof(szBuffer)-1);
	std::string& rstLocale=gs_kAppStrTable.m_kMap_dwID_stLocale[dwID];
	rstLocale=szBuffer;

	return rstLocale;
}

const char* ApplicationStringTable_GetStringz(DWORD dwID, LPCSTR szKey)
{
	return ApplicationStringTable_GetString(dwID, szKey).c_str();
}

const char* ApplicationStringTable_GetStringz(DWORD dwID)
{
	return ApplicationStringTable_GetString(dwID).c_str();
}

////////////////////////////////////////////

int Setup(LPSTR lpCmdLine); // Internal function forward

bool PackInitialize(const char * c_pszFolder)
{
	if (_access(c_pszFolder, 0) != 0)
		return true;

	std::string stFolder(c_pszFolder);
	stFolder += "/";

	std::string stFileName(stFolder);
	stFileName += "Index";

	CMappedFile file;
	LPCVOID pvData;

#ifndef ENABLE_DEBUG_PACK_UNPACKED
	if (!file.Create(stFileName.c_str(), &pvData, 0, 0))
	{
		LogBoxf("FATAL ERROR! File not exist: %s", stFileName.c_str());
		TraceError("FATAL ERROR! File not exist: %s", stFileName.c_str());
		return true;
	}

	CMemoryTextFileLoader TextLoader;
	TextLoader.Bind(file.Size(), pvData);

	bool bPackFirst = TRUE;

	const std::string& strPackType = TextLoader.GetLineString(0);

	if (strPackType.compare("FILE") && strPackType.compare("PACK"))
	{
		TraceError("Pack/Index has invalid syntax. First line must be 'PACK' or 'FILE'");
		return false;
	}

#endif

#ifndef ENABLE_DEBUG_PACK_UNPACKED // @warme601 _DISTRIBUTE -> NDEBUG
	Tracef("Note: PackFirst mode enabled. [pack]\n");
#else
	bool bPackFirst = FALSE;
	Tracef("Note: PackFirst mode not enabled. [file]\n");
#endif

	CTextFileLoader::SetCacheMode();
#if defined(USE_RELATIVE_PATH)
	CEterPackManager::Instance().SetRelativePathMode();
#endif
	CEterPackManager::Instance().SetCacheMode();
	CEterPackManager::Instance().SetSearchMode(bPackFirst);

	CSoundData::SetPackMode();


#ifndef ENABLE_DEBUG_PACK_UNPACKED
	std::string strPackName, strTexCachePackName;
	for (DWORD i = 1; i < TextLoader.GetLineCount() - 1; i += 2)
	{
		const std::string & c_rstFolder = TextLoader.GetLineString(i);
		const std::string & c_rstName = TextLoader.GetLineString(i + 1);

		strPackName = stFolder + c_rstName;
		strTexCachePackName = strPackName + "_texcache";

		CEterPackManager::Instance().RegisterPack(strPackName.c_str(), c_rstFolder.c_str());
		CEterPackManager::Instance().RegisterPack(strTexCachePackName.c_str(), c_rstFolder.c_str());
	}
#ifndef __EXTRA_CYTHON__
	CEterPackManager::Instance().RegisterRootPack((stFolder + std::string("root")).c_str());
#endif
#endif
	return true;
}

#include "AntiMacro.h"
bool RunMainScript(CPythonLauncher& pyLauncher, const char* lpCmdLine)
{
	initpack();
	initdbg();
	initime();
	initgrp();
	initgrpImage();
	initgrpText();
	initwndMgr();
	/////////////////////////////////////////////
	initudp();
	initapp();
	initsystemSetting();
	initchr();
	initchrmgr();
	initPlayer();
	initItem();
	initNonPlayer();
	initTrade();
#ifdef ENABLE_IKASHOP_RENEWAL
	initikashop();
#endif
	initChat();
	initTextTail();
	initnet();
	initMiniMap();
	initProfiler();
	initEvent();
	initeffect();
	initfly();
	initsnd();
	initeventmgr();
	initshop();
	initskill();
	initcaptcha();
	initquest();
	initBackground();
	initMessenger();
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	initAcce();
#endif
	initsafebox();
	initguild();
	initServerStateChecker();
	initplayersettingsmodule();
#ifdef __USE_CYTHON__
	// don't add this line if you're implementing cython via .pyd:
#ifndef __CYTHON_PYD__
	initrootlibManager();
#ifdef __EXTRA_CYTHON__
	// don't add this line if you're implementing cython via .pyd:
	inituiscriptlibManager();
	initliblibManager();
#endif
#endif

#endif

#ifndef _DEBUG
	//StartAntiCheat();
#endif
	AntiMacro::GetInstance()->Initialize();

    PyObject * builtins = PyImport_ImportModule("__builtin__");
#ifdef NDEBUG // @warme601 _DISTRIBUTE -> NDEBUG
	PyModule_AddIntConstant(builtins, "__DEBUG__", 0);
#else
	PyModule_AddIntConstant(builtins, "__DEBUG__", 1);
#endif
#ifdef __USE_CYTHON__
	PyModule_AddIntConstant(builtins, "__USE_CYTHON__", 1);
#else
	PyModule_AddIntConstant(builtins, "__USE_CYTHON__", 0);
#endif
#ifdef __EXTRA_CYTHON__
	PyModule_AddIntConstant(builtins, "__USE_EXTRA_CYTHON__", 1);
#else
	PyModule_AddIntConstant(builtins, "__USE_EXTRA_CYTHON__", 0);
#endif

	// RegisterCommandLine
	{
		std::string stRegisterCmdLine;

		const char * loginMark = "-cs";
		const char * loginMark_NonEncode = "-ncs";
		const char * seperator = " ";

		std::string stCmdLine;
		const int CmdSize = 3;
		std::vector<std::string> stVec;
		SplitLine(lpCmdLine,seperator,&stVec);
		if (CmdSize == stVec.size() && stVec[0]==loginMark)
		{
			char buf[MAX_PATH];
			base64_decode(stVec[2].c_str(),buf);
			stVec[2] = buf;
			string_join(seperator,stVec,&stCmdLine);
		}
		else if (CmdSize <= stVec.size() && stVec[0]==loginMark_NonEncode)
		{
			stVec[0] = loginMark;
			string_join(" ",stVec,&stCmdLine);
		}
		else
			stCmdLine = lpCmdLine;

		PyModule_AddStringConstant(builtins, "__COMMAND_LINE__", stCmdLine.c_str());
	}
	{
		std::vector<std::string> stVec;
		SplitLine(lpCmdLine," " ,&stVec);

		if (stVec.size() != 0 && "--pause-before-create-window" == stVec[0])
			system("pause");

#ifdef ENABLE_DAEMONPROTECTION
		DWORD dwCrc32, dwSize;
		if (!((dwCrc32 = GetFileCRC32(DPDLL_FILENAME))==DPDLL_CRC32))
		{
			TraceError("dpdll wrong crc32 %d", dwCrc32);
			return false;
		}
		if (!((dwSize = GetFileSize(DPDLL_FILENAME))==DPDLL_FILESIZE))
		{
			TraceError("dpdll wrong size %d", dwSize);
			return false;
		}
		if (!LoadLibraryA(DPDLL_FILENAME))
		{
			TraceError("dpdll not loaded");
			return false;
		}
#endif

#ifdef __USE_CYTHON__
		PyObject* r;
		r = PyImport_ImportModule("rootlib");
		if (r == NULL) {
			TraceError("RunMain Error");
			PyErr_Print();
			Py_Finalize();
			exit(1);
		}
		else {
			PyObject* moduleImportFunc = PyObject_GetAttrString(r, "moduleImport");
			if (moduleImportFunc == NULL || !PyCallable_Check(moduleImportFunc)) {
				TraceError("'moduleImport' not found or not callable in 'rootlib'.");
				PyErr_Print();
				Py_Finalize();
				exit(1);
			}
			else
			{
				PyObject* args = PyTuple_Pack(1, PyUnicode_FromString("system"));
				PyObject* result = PyObject_CallObject(moduleImportFunc, args);

				if (result == NULL)
				{
					TraceError("Error while calling 'rootlib.moduleImport('system')'");
					PyErr_Print();
					Py_Finalize();
					exit(1);
				}
				else
				{
					Py_DECREF(r);
					Py_DECREF(moduleImportFunc);
					Py_DECREF(args);
					Py_DECREF(result);
				}
			}
		}
#else
		if (!pyLauncher.RunFile("system.py"))
		{
			TraceError("RunMain Error");
			return false;
		}
#endif

	}

	return true;
}

bool Main(HINSTANCE hInstance, LPSTR lpCmdLine)
{
#ifdef LOCALE_SERVICE_YMIR
	extern bool g_isScreenShotKey;
	g_isScreenShotKey = true;
#endif

	DWORD dwRandSeed=time(NULL)+DWORD(GetCurrentProcess());
	srandom(dwRandSeed);
	srand(random());

	SetLogLevel(1);

#ifdef LOCALE_SERVICE_VIETNAM_MILD
	extern BOOL USE_VIETNAM_CONVERT_WEAPON_VNUM;
	USE_VIETNAM_CONVERT_WEAPON_VNUM = true;
#endif

	if (_access("perf_game_update.txt", 0)==0)
	{
		DeleteFile("perf_game_update.txt");
	}

	if (_access("newpatch.exe", 0)==0)
	{
		system("patchupdater.exe");
		return false;
	}

	ilInit();

	if (!Setup(lpCmdLine))
		return false;

#ifdef _DEBUG
	OpenConsoleWindow();
	OpenLogFile(true); // true == uses syserr.txt and log.txt
#else
	OpenLogFile(false); // false == uses syserr.txt only
#endif

	static CLZO				lzo;
	static CEterPackManager	EterPackManager;

	if (!PackInitialize("pack"))
	{
		LogBox("Pack Initialization failed. Check log.txt file..");
		return false;
	}

	if(LocaleService_LoadGlobal(hInstance))
		SetDefaultCodePage(LocaleService_GetCodePage());

#if defined(ENABLE_PYLIB_CHECK) && !defined(__USE_CYTHON__)
	if (!__CheckPyLibFiles())
		return false;
#endif
#ifdef ENABLE_MILES_CHECK
	if (!__CheckMilesFiles())
		return false;
#endif
	CPythonApplication * app = new CPythonApplication;

	EterPackManager.SetTextureViewType(CPythonSystem::instance().GetTextureViewType());

	app->Initialize(hInstance);

	bool ret=false;
	{
		CPythonLauncher pyLauncher;
		CPythonExceptionSender pyExceptionSender;
		SetExceptionSender(&pyExceptionSender);

		if (pyLauncher.Create())
		{
			ret=RunMainScript(pyLauncher, lpCmdLine);
		}

		//ProcessScanner_ReleaseQuitEvent();

		app->Clear();

		timeEndPeriod(1);
		pyLauncher.Clear();
	}

	app->Destroy();
	delete app;

	return ret;
}

HANDLE CreateMetin2GameMutex()
{
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
	sa.nLength				= sizeof(sa);
	sa.lpSecurityDescriptor	= NULL;
	sa.bInheritHandle		= FALSE;

	return CreateMutex(&sa, FALSE, "Metin2GameMutex");
}

void DestroyMetin2GameMutex(HANDLE hMutex)
{
	if (hMutex)
	{
		ReleaseMutex(hMutex);
		hMutex = NULL;
	}
}

void __ErrorPythonLibraryIsNotExist()
{
	LogBoxf("FATAL ERROR!! Python Library file not exist!");
}

bool __IsTimeStampOption(LPSTR lpCmdLine)
{
	const char* TIMESTAMP = "/timestamp";
	return (strncmp(lpCmdLine, TIMESTAMP, strlen(TIMESTAMP))==0);
}

void __PrintTimeStamp()
{
#ifdef	_DEBUG
	if (__IS_TEST_SERVER_MODE__)
		LogBoxf("METIN2 BINARY TEST DEBUG VERSION %s  ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);
	else
		LogBoxf("METIN2 BINARY DEBUG VERSION %s ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);

#else
	if (__IS_TEST_SERVER_MODE__)
		LogBoxf("METIN2 BINARY TEST VERSION %s  ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);
	else
		LogBoxf("METIN2 BINARY DISTRIBUTE VERSION %s ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);
#endif
}

bool __IsLocaleOption(LPSTR lpCmdLine)
{
	return (strcmp(lpCmdLine, "--locale") == 0);
}

bool __IsLocaleVersion(LPSTR lpCmdLine)
{
	return (strcmp(lpCmdLine, "--perforce-revision") == 0);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_LEAK_CHECK_DF );
	//_CrtSetBreakAlloc( 110247 );
#endif

	ApplicationStringTable_Initialize(hInstance);

	LocaleService_LoadConfig("");
	SetDefaultCodePage(LocaleService_GetCodePage());

#if defined(CHECK_LATEST_DATA_FILES)
	if (!CheckLatestFiles())
		return 0;
#endif

	bool bQuit = false;
	bool bAuthKeyChecked = false;
	int nArgc = 0;
	PCHAR* szArgv = CommandLineToArgv( lpCmdLine, &nArgc );

	for( int i=0; i < nArgc; i++ ) {
		if(szArgv[i] == 0)
			continue;
		if (__IsLocaleVersion(szArgv[i]))
		{
			char szModuleName[MAX_PATH];
			char szVersionPath[MAX_PATH];
			GetModuleFileName(NULL, szModuleName, sizeof(szModuleName));
			sprintf(szVersionPath, "%s.version", szModuleName);
			FILE* fp = fopen(szVersionPath, "wt");
			if (fp)
			{
				extern int METIN2_GET_VERSION();
				fprintf(fp, "r%d\n", METIN2_GET_VERSION());
				fclose(fp);
			}
			bQuit = true;
		} else if (__IsLocaleOption(szArgv[i]))
		{
			FILE* fp=fopen("locale.txt", "wt");
			fprintf(fp, "service[%s] code_page[%d]",
				LocaleService_GetName(), LocaleService_GetCodePage());
			fclose(fp);
			bQuit = true;
		} else if (__IsTimeStampOption(szArgv[i]))
		{
			__PrintTimeStamp();
			bQuit = true;
		} else if ((strcmp(szArgv[i], "--force-set-locale") == 0))
		{
			if (nArgc <= i + 2)
			{
				MessageBox(NULL, "Invalid arguments", ApplicationStringTable_GetStringz(IDS_APP_NAME, "APP_NAME"), MB_ICONSTOP);
				goto Clean;
			}

			const char* localeName = szArgv[++i];
			const char* localePath = szArgv[++i];

			LocaleService_ForceSetLocale(localeName, localePath);
		}
		else if (strcmp(szArgv[i], "--config") == 0)
		{
			if (nArgc <= i + 1)
			{
				MessageBox(NULL, "Invalid arguments", ApplicationStringTable_GetStringz(IDS_APP_NAME, "APP_NAME"), MB_ICONSTOP);
				goto Clean;
			}

			const char* configFileName = szArgv[++i];
			SetGameConfigFileName(configFileName);
		}
	}

	if(bQuit)
		goto Clean;

#if defined(NEEDED_COMMAND_ARGUMENT)
	if (strstr(lpCmdLine, NEEDED_COMMAND_ARGUMENT) == 0) {
		MessageBox(NULL, ApplicationStringTable_GetStringz(IDS_ERR_MUST_LAUNCH_FROM_PATCHER, "ERR_MUST_LAUNCH_FROM_PATCHER"), ApplicationStringTable_GetStringz(IDS_APP_NAME, "APP_NAME"), MB_ICONSTOP);
			goto Clean;
	}
#endif

#if defined(NEEDED_COMMAND_CLIPBOARD)
	{
		CHAR szSecKey[256];
		CPostIt cPostIt( "VOLUME1" );

		if( cPostIt.Get( "SEC_KEY", szSecKey, sizeof(szSecKey) ) == FALSE ) {
			MessageBox(NULL, ApplicationStringTable_GetStringz(IDS_ERR_MUST_LAUNCH_FROM_PATCHER, "ERR_MUST_LAUNCH_FROM_PATCHER"), ApplicationStringTable_GetStringz(IDS_APP_NAME, "APP_NAME"), MB_ICONSTOP);
			goto Clean;
		}
		if( strstr(szSecKey, NEEDED_COMMAND_CLIPBOARD) == 0 ) {
			MessageBox(NULL, ApplicationStringTable_GetStringz(IDS_ERR_MUST_LAUNCH_FROM_PATCHER, "ERR_MUST_LAUNCH_FROM_PATCHER"), ApplicationStringTable_GetStringz(IDS_APP_NAME, "APP_NAME"), MB_ICONSTOP);
			goto Clean;
		}
		cPostIt.Empty();
	}
#endif

	//WebBrowser_Startup(hInstance);

//#ifndef ENABLE_PYLIB_CHECK
//	if (!
//		CheckPythonLibraryFilenames())
//	{
//		__ErrorPythonLibraryIsNotExist();
//		goto Clean;
//	}
//#endif

	Main(hInstance, lpCmdLine);

	//WebBrowser_Cleanup();

	::CoUninitialize();

	if(gs_szErrorString[0])
		MessageBox(NULL, gs_szErrorString, ApplicationStringTable_GetStringz(IDS_APP_NAME, "APP_NAME"), MB_ICONSTOP);

Clean:
	SAFE_FREE_GLOBAL(szArgv);

	return 0;
}

#if GrannyProductMinorVersion==4 || GrannyProductMinorVersion==7
static void GrannyError(granny_log_message_type Type,
						granny_log_message_origin Origin,
						char const *Error,
						void *UserData)
{
	TraceError("GRANNY: %s", Error);
}
#elif GrannyProductMinorVersion==9 || GrannyProductMinorVersion==8
static void GrannyError(granny_log_message_type Type,
						granny_log_message_origin Origin,
						char const*  File,
						granny_int32x Line,
						char const *Error,
						void *UserData)
{
	//Origin==GrannyFileReadingLogMessage for granny run-time tag& revision warning (Type==GrannyWarningLogMessage)
	//Origin==GrannyControlLogMessage for miss track_group on static models as weapons warning (Type==GrannyWarningLogMessage)
	//Origin==GrannyMeshBindingLogMessage for miss bone ToSkeleton on new ymir models error (Type==GrannyErrorLogMessage)
	// if (Type == GrannyWarningLogMessage)
	if (Origin==GrannyFileReadingLogMessage || Origin==GrannyControlLogMessage || Origin==GrannyMeshBindingLogMessage)
		return;
	TraceError("GRANNY: %s(%d): ERROR: %s --- [%d] %s --- [%d] %s", File, Line, Error, Type, GrannyGetLogMessageTypeString(Type), Origin, GrannyGetLogMessageOriginString(Origin));
}
#elif GrannyProductMinorVersion==11
static void GrannyError(granny_log_message_type Type,
						granny_log_message_origin Origin,
						char const*  File,
						granny_int32x Line,
						char const *Error,
						void *UserData)
{
}
#else
#error "unknown granny version"
#endif

int Setup(LPSTR lpCmdLine)
{
	TIMECAPS tc;
	UINT wTimerRes;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
		return 0;

	wTimerRes = MINMAX(tc.wPeriodMin, 1, tc.wPeriodMax);
	timeBeginPeriod(wTimerRes);

	granny_log_callback Callback;
    Callback.Function = GrannyError;
    Callback.UserData = 0;
    GrannySetLogCallback(&Callback);
	return 1;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
