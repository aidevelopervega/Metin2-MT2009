#include "StdAfx.h"
#include "GameType.h"

std::string g_strResourcePath = "d:/ymir work/";
std::string g_strImagePath = "d:/ymir work/ui/";

std::string g_strGuildSymbolPathName = "mark/10/";

// DEFAULT_FONT
static std::string gs_strDefaultFontName = "Tahoma:12.fnt";
static std::string gs_strDefaultItalicFontName = "Tahoma:12i.fnt";
static std::string gs_strDefaultLargeFontName = "";

static CResource* gs_pkDefaultFont = NULL;
static CResource* gs_pkDefaultItalicFont = NULL;
static CResource* gs_pkDefaultLargeFont = NULL;


static bool gs_isReloadDefaultFont = false;

void DefaultFont_Startup()
{
	gs_pkDefaultFont = NULL;
	gs_pkDefaultLargeFont = NULL;
}

void DefaultFont_Cleanup()
{
	if (gs_pkDefaultFont)
		gs_pkDefaultFont->Release();

	if (gs_pkDefaultLargeFont)
		gs_pkDefaultLargeFont->Release();
}

std::string GetDefaultFontName()
{
	return gs_strDefaultFontName;
}

std::string GetLargeFontName()
{
	return gs_strDefaultLargeFontName;
}

std::string GetDefaultFontNameOnly()
{
	size_t colonPos = gs_strDefaultFontName.find(':');
	return gs_strDefaultFontName.substr(0, colonPos);
}

std::string GetDefaultFontSizeOnly()
{
	size_t colonPos = gs_strDefaultFontName.find(':');
	//size_t dotPos = gs_strDefaultFontName.find('.');
	return gs_strDefaultFontName.substr(colonPos + 1);
}

int GetDefaultFontSizeNumber()
{
	return std::stoi(GetDefaultFontSizeOnly());
}

void DefaultFont_SetName(const char * c_szFontName, const char* c_szLargeFontName)
{
	gs_strDefaultFontName = c_szFontName;
	gs_strDefaultFontName += ".fnt";

	gs_strDefaultItalicFontName = c_szFontName;
	if (strchr(c_szFontName, ':'))
		gs_strDefaultItalicFontName += "i";
	gs_strDefaultItalicFontName += ".fnt";

	gs_strDefaultLargeFontName = c_szLargeFontName;
	gs_strDefaultLargeFontName += ".fnt";

	gs_isReloadDefaultFont = true;
}

bool ReloadDefaultFonts()
{
	CResourceManager& rkResMgr = CResourceManager::Instance();

	gs_isReloadDefaultFont = false;

	CResource* pkNewFont = rkResMgr.GetResourcePointer(gs_strDefaultFontName.c_str());
	pkNewFont->AddReference();
	if (gs_pkDefaultFont)
		gs_pkDefaultFont->Release();
	gs_pkDefaultFont = pkNewFont;

	CResource* pkNewItalicFont = rkResMgr.GetResourcePointer(gs_strDefaultItalicFontName.c_str());
	pkNewItalicFont->AddReference();
	if (gs_pkDefaultItalicFont)
		gs_pkDefaultItalicFont->Release();
	gs_pkDefaultItalicFont = pkNewItalicFont;

	CResource* pkNewLargeFont = rkResMgr.GetResourcePointer(gs_strDefaultLargeFontName.c_str());
	pkNewLargeFont->AddReference();
	if (gs_pkDefaultLargeFont)
		gs_pkDefaultLargeFont->Release();
	gs_pkDefaultLargeFont = pkNewLargeFont;

	return true;
}

CResource* DefaultFont_GetResource()
{
	if (!gs_pkDefaultFont || gs_isReloadDefaultFont)
		ReloadDefaultFonts();
	return gs_pkDefaultFont;
}

CResource* DefaultItalicFont_GetResource()
{
	if (!gs_pkDefaultItalicFont || gs_isReloadDefaultFont)
		ReloadDefaultFonts();
	return gs_pkDefaultItalicFont;
}

CResource* LargeFont_GetResource()
{
	if (!gs_pkDefaultLargeFont || gs_isReloadDefaultFont)
		ReloadDefaultFonts();
	return gs_pkDefaultLargeFont;
}

// END_OF_DEFAULT_FONT

void SetGuildSymbolPath(const char * c_szPathName)
{
	g_strGuildSymbolPathName = "mark/";
	g_strGuildSymbolPathName += c_szPathName;
	g_strGuildSymbolPathName += "/";
}

const char * GetGuildSymbolFileName(DWORD dwGuildID)
{
	return _getf("%s%03d.jpg", g_strGuildSymbolPathName.c_str(), dwGuildID);
}

BYTE c_aSlotTypeToInvenType[SLOT_TYPE_MAX] =
{
	RESERVED_WINDOW,		// SLOT_TYPE_NONE
	INVENTORY,				// SLOT_TYPE_INVENTORY
	RESERVED_WINDOW,		// SLOT_TYPE_SKILL
	RESERVED_WINDOW,		// SLOT_TYPE_EMOTION
	RESERVED_WINDOW,		// SLOT_TYPE_SHOP
	RESERVED_WINDOW,		// SLOT_TYPE_EXCHANGE_OWNER
	RESERVED_WINDOW,		// SLOT_TYPE_EXCHANGE_TARGET
	RESERVED_WINDOW,		// SLOT_TYPE_QUICK_SLOT
	RESERVED_WINDOW,
	RESERVED_WINDOW,		// SLOT_TYPE_PRIVATE_SHOP
	RESERVED_WINDOW,
	DRAGON_SOUL_INVENTORY,	// SLOT_TYPE_DRAGON_SOUL_INVENTORY
};

BYTE SlotTypeToInvenType(BYTE bSlotType)
{
	if (bSlotType >= SLOT_TYPE_MAX)
		return RESERVED_WINDOW;
	else
		return c_aSlotTypeToInvenType[bSlotType];
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
