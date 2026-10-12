#include "StdAfx.h"
#include "PythonSystem.h"
#include "PythonApplication.h"

#define DEFAULT_VALUE_ALWAYS_SHOW_NAME		1 // group by default

static std::string gs_strGameConfigFileName = "game1.cfg";

void SetGameConfigFileName(const char* c_szFileName)
{
	gs_strGameConfigFileName = c_szFileName;
}

void CPythonSystem::SetInterfaceHandler(PyObject * poHandler)
{
//	if (m_poInterfaceHandler)
//		Py_DECREF(m_poInterfaceHandler);

	m_poInterfaceHandler = poHandler;

//	if (m_poInterfaceHandler)
//		Py_INCREF(m_poInterfaceHandler);
}

void CPythonSystem::DestroyInterfaceHandler()
{
	m_poInterfaceHandler = NULL;
}

std::string CPythonSystem::GetLanguage()
{
	return m_Config.language;
}

void CPythonSystem::SetLanguage(const char* language)
{
	m_Config.language = language;
}

void CPythonSystem::SaveWindowStatus(int iIndex, int iVisible, int iMinimized, int ix, int iy, int iHeight)
{
	m_WindowStatus[iIndex].isVisible	= iVisible;
	m_WindowStatus[iIndex].isMinimized	= iMinimized;
	m_WindowStatus[iIndex].ixPosition	= ix;
	m_WindowStatus[iIndex].iyPosition	= iy;
	m_WindowStatus[iIndex].iHeight		= iHeight;
}

void CPythonSystem::GetDisplaySettings()
{
	memset(m_ResolutionList, 0, sizeof(TResolution) * RESOLUTION_MAX_NUM);
	m_ResolutionCount = 0;

	LPDIRECT3D9 lpD3D = CPythonGraphic::Instance().GetD3D();

	D3DADAPTER_IDENTIFIER9 d3dAdapterIdentifier;
	D3DDISPLAYMODE d3ddmDesktop;

	lpD3D->GetAdapterIdentifier(0, 0, &d3dAdapterIdentifier);
	lpD3D->GetAdapterDisplayMode(0, &d3ddmDesktop);

	DWORD dwNumAdapterModes = lpD3D->GetAdapterModeCount(0, d3ddmDesktop.Format);

	for (UINT iMode = 0; iMode < dwNumAdapterModes; iMode++)
	{
		D3DDISPLAYMODE DisplayMode;
		lpD3D->EnumAdapterModes(0, d3ddmDesktop.Format, iMode, &DisplayMode);
		DWORD bpp = 0;

		if (DisplayMode.Width < 800 || DisplayMode.Height < 600)
			continue;

		if (DisplayMode.Format == D3DFMT_R5G6B5)
			bpp = 16;
		else if (DisplayMode.Format == D3DFMT_X8R8G8B8)
			bpp = 32;
		else
			continue;

		int check_res = false;

		for (int i = 0; !check_res && i < m_ResolutionCount; ++i)
		{
			if (m_ResolutionList[i].bpp != bpp ||
				m_ResolutionList[i].width != DisplayMode.Width ||
				m_ResolutionList[i].height != DisplayMode.Height)
				continue;

			int check_fre = false;

			for (int j = 0; j < m_ResolutionList[i].frequency_count; ++j)
			{
				if (m_ResolutionList[i].frequency[j] == DisplayMode.RefreshRate)
				{
					check_fre = true;
					break;
				}
			}

			if (!check_fre)
				if (m_ResolutionList[i].frequency_count < FREQUENCY_MAX_NUM)
					m_ResolutionList[i].frequency[m_ResolutionList[i].frequency_count++] = DisplayMode.RefreshRate;

			check_res = true;
		}

		if (!check_res)
		{
			if (m_ResolutionCount < RESOLUTION_MAX_NUM)
			{
				m_ResolutionList[m_ResolutionCount].width			= DisplayMode.Width;
				m_ResolutionList[m_ResolutionCount].height			= DisplayMode.Height;
				m_ResolutionList[m_ResolutionCount].bpp				= bpp;
				m_ResolutionList[m_ResolutionCount].frequency[0]	= DisplayMode.RefreshRate;
				m_ResolutionList[m_ResolutionCount].frequency_count	= 1;

				++m_ResolutionCount;
			}
		}
	}
}

int	CPythonSystem::GetResolutionCount()
{
	return m_ResolutionCount;
}

int CPythonSystem::GetFrequencyCount(int index)
{
	if (index >= m_ResolutionCount)
		return 0;

    return m_ResolutionList[index].frequency_count;
}

bool CPythonSystem::GetResolution(int index, OUT DWORD *width, OUT DWORD *height, OUT DWORD *bpp)
{
	if (index >= m_ResolutionCount)
		return false;

	*width = m_ResolutionList[index].width;
	*height = m_ResolutionList[index].height;
	*bpp = m_ResolutionList[index].bpp;
	return true;
}

bool CPythonSystem::GetFrequency(int index, int freq_index, OUT DWORD *frequncy)
{
	if (index >= m_ResolutionCount)
		return false;

	if (freq_index >= m_ResolutionList[index].frequency_count)
		return false;

	*frequncy = m_ResolutionList[index].frequency[freq_index];
	return true;
}

int	CPythonSystem::GetResolutionIndex(DWORD width, DWORD height, DWORD bit)
{
	DWORD re_width, re_height, re_bit;
	int i = 0;

	while (GetResolution(i, &re_width, &re_height, &re_bit))
	{
		if (re_width == width)
			if (re_height == height)
				if (re_bit == bit)
					return i;
		i++;
	}

	return 0;
}

int	CPythonSystem::GetFrequencyIndex(int res_index, DWORD frequency)
{
	DWORD re_frequency;
	int i = 0;

	while (GetFrequency(res_index, i, &re_frequency))
	{
		if (re_frequency == frequency)
			return i;

		i++;
	}

	return 0;
}

void CPythonSystem::SetResolution(DWORD width, DWORD height, DWORD bpp)
{
	m_Config.width = width;
	m_Config.height = height;
	if (bpp > 0)
		m_Config.bpp = bpp;
}

void CPythonSystem::SetScaledResolution(DWORD width, DWORD height)
{
	m_Config.scaled_width = width;
	m_Config.scaled_height = height;
}

void CPythonSystem::SetCustomResolution(DWORD width, DWORD height)
{
	m_Config.custom_width = width;
	m_Config.custom_height = height;
}

void CPythonSystem::SetScaleInterface(float scale)
{
	m_Config.scale_interface = scale;
}

void CPythonSystem::SetScaleInterface(bool isScale)
{
	m_Config.bScaleInterface = isScale;
}

DWORD CPythonSystem::GetWidth()
{
	if (IsWindowed() && m_Config.custom_width > 0)
		return m_Config.custom_width;

	return m_Config.width;
}

DWORD CPythonSystem::GetHeight()
{
	if (IsWindowed() && m_Config.custom_height > 0)
		return m_Config.custom_height;

	DWORD height = m_Config.height;
	if (IsWindowed())
		height -= m_Config.windowHeightDifference;

	return height;
}

DWORD CPythonSystem::GetCustomWidth()
{
	return m_Config.custom_width;
}

DWORD CPythonSystem::GetCustomHeight()
{
	return m_Config.custom_height;
}

DWORD CPythonSystem::GetBPP()
{
	return m_Config.bpp;
}
DWORD CPythonSystem::GetFrequency()
{
	return m_Config.frequency;
}

bool CPythonSystem::IsNoSoundCard()
{
	return m_Config.bNoSoundCard;
}

bool CPythonSystem::IsSoftwareCursor()
{
	return m_Config.is_software_cursor;
}

float CPythonSystem::GetMusicVolume()
{
	return m_Config.music_volume;
}

int CPythonSystem::GetSoundVolume()
{
	return m_Config.voice_volume;
}

void CPythonSystem::SetMusicVolume(float fVolume)
{
	m_Config.music_volume = fVolume;
}

void CPythonSystem::SetSoundVolumef(float fVolume)
{
	m_Config.voice_volume = int(5 * fVolume);
}

int CPythonSystem::GetDistance()
{
	return m_Config.iDistance;
}

int CPythonSystem::GetShadowLevel()
{
	return m_Config.iShadowLevel;
}

void CPythonSystem::SetShadowLevel(unsigned int level)
{
	m_Config.iShadowLevel = MIN(level, 7);
	CPythonBackground::instance().RefreshShadowLevel();
}

int CPythonSystem::GetTextureViewType()
{
	return m_Config.iTextureViewType;
}

void CPythonSystem::SetTextureViewType(unsigned int type)
{
	m_Config.iTextureViewType = type;
}

int CPythonSystem::GetScaledWidth()
{
	if (!IsScaleMode())
		return GetWidth();

	return m_Config.width / m_Config.scale_interface;
}

int CPythonSystem::GetScaledHeight()
{
	if (!IsScaleMode())
		return GetHeight();

	return m_Config.height / m_Config.scale_interface;
}

float CPythonSystem::GetScaleInterface()
{
	if (!IsScaleMode())
		return 1.0f;

	return m_Config.scale_interface;
}

bool CPythonSystem::IsScaleMode()
{
	return m_Config.bScaleInterface && m_Config.scale_interface > 1.0;
}

int CPythonSystem::GetAntiAliasingLevel()
{
	return m_Config.iAliasingLevel;
}

void CPythonSystem::SetAntiAliasingLevel(unsigned int level)
{
	m_Config.iAliasingLevel = MIN(level, 3);
}

const char* CPythonSystem::GetFontName()
{
	return m_Config.fontName.c_str();
}

int CPythonSystem::GetFontSize()
{
	return m_Config.fontSize;
}

std::string CPythonSystem::GetFont()
{
	return m_Config.fontName + ":" + std::to_string(m_Config.fontSize);
}

void CPythonSystem::SetFont(const char* fontName, int size)
{
	m_Config.fontName = fontName;
	m_Config.fontSize = size;
}


int CPythonSystem::IsSaveID()
{
	return m_Config.isSaveID;
}

const char * CPythonSystem::GetSaveID()
{
	return m_Config.SaveID;
}

bool CPythonSystem::isViewCulling()
{
	return m_Config.is_object_culling;
}

void CPythonSystem::SetSaveID(int iValue, const char * c_szSaveID)
{
	if (iValue != 1)
		return;

	m_Config.isSaveID = iValue;
	strncpy(m_Config.SaveID, c_szSaveID, sizeof(m_Config.SaveID) - 1);
}

CPythonSystem::TConfig * CPythonSystem::GetConfig()
{
	return &m_Config;
}

void CPythonSystem::SetConfig(TConfig * pNewConfig)
{
	m_Config = *pNewConfig;
}

void CPythonSystem::SetDefaultConfig()
{
	memset(&m_Config, 0, sizeof(TConfig));

	m_Config.width				= 1024;
	m_Config.height				= 768;
	m_Config.scaled_width = 0;
	m_Config.scaled_height = 0;
	m_Config.bpp				= 32;

	m_Config.custom_width = 0;
	m_Config.custom_height = 0;

	m_Config.windowHeightDifference = 0;
	m_Config.windowScaleHeightDifference = 0;

	m_Config.bScaleInterface = false;
	m_Config.scale_interface = 1.0f;

	m_Config.bWindowMode			= WINDOW_MODE_WINDOWED;

	m_Config.is_software_cursor	= false;
	m_Config.is_object_culling	= true;
	m_Config.iDistance			= 3;

	m_Config.gamma				= 3;
	m_Config.music_volume		= 1.0f;
	m_Config.voice_volume		= 5;

	m_Config.bDecompressDDS		= 0;
	m_Config.bSoftwareTiling	= 0;
	m_Config.iShadowLevel		= 7;
	m_Config.bViewChat			= true;
	m_Config.bAlwaysShowName	= DEFAULT_VALUE_ALWAYS_SHOW_NAME;
	m_Config.bShowDamage		= ShowDamageTarget;
	m_Config.bShowSalesText		= true;
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
	m_Config.bShowMobAIFlag		= true;
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
	m_Config.bShowMobLevel		= true;
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
	m_Config.shopRangeView = 5000.0f;
#endif
#endif
	m_Config.iAliasingLevel = 2;
	m_Config.bShowFloatingText = 1;
	m_Config.iQuestTextDelay = 0;
	m_Config.bShowEmpireFlag = true;
	m_Config.bNightMode = 2; // auto
	m_Config.shop_render_distance = 1.0f;
	m_Config.fontName = std::string("Tahoma");
	m_Config.fontSize = 12;
	m_Config.iTextureViewType = 0;
	m_Config.language = std::string("pl");
}

bool CPythonSystem::IsWindowed()
{
	return m_Config.bWindowMode == WINDOW_MODE_WINDOWED;
}

bool CPythonSystem::IsFullscreen()
{
	return m_Config.bWindowMode == WINDOW_MODE_FULLSCREEN;
}

bool CPythonSystem::IsFullscreenWindowed()
{
	return m_Config.bWindowMode == WINDOW_MODE_FULLSCREEN_WINDOWED;
}

BYTE CPythonSystem::GetWindowMode()
{
	return m_Config.bWindowMode;
}

bool CPythonSystem::IsViewChat()
{
	return m_Config.bViewChat;
}

void CPythonSystem::SetViewChatFlag(int iFlag)
{
	m_Config.bViewChat = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsAlwaysShowName()
{
	return m_Config.bAlwaysShowName == 2;
}

bool CPythonSystem::IsGroupShowName()
{
	return m_Config.bAlwaysShowName == DEFAULT_VALUE_ALWAYS_SHOW_NAME;
}

BYTE CPythonSystem::GetShowName()
{
	return m_Config.bAlwaysShowName;
}

void CPythonSystem::SetShowNameFlag(int iFlag)
{
	m_Config.bAlwaysShowName = iFlag;
}

BYTE CPythonSystem::GetShowDamage()
{
	return m_Config.bShowDamage;
}

void CPythonSystem::SetShowDamageFlag(int iFlag)
{
	m_Config.bShowDamage = iFlag;
}

bool CPythonSystem::IsShowSalesText()
{
	return m_Config.bShowSalesText;
}

void CPythonSystem::SetShowSalesTextFlag(int iFlag)
{
	m_Config.bShowSalesText = iFlag == 1 ? true : false;
}

#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
bool CPythonSystem::IsShowMobAIFlag()
{
	return m_Config.bShowMobAIFlag;
}

void CPythonSystem::SetShowMobAIFlagFlag(int iFlag)
{
	m_Config.bShowMobAIFlag = iFlag == 1 ? true : false;
}
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
bool CPythonSystem::IsShowMobLevel()
{
	return m_Config.bShowMobLevel;
}

void CPythonSystem::SetShowMobLevelFlag(int iFlag)
{
	m_Config.bShowMobLevel = iFlag == 1 ? true : false;
}
#endif

bool CPythonSystem::IsAutoTiling()
{
	if (m_Config.bSoftwareTiling == 0)
		return true;

	return false;
}

void CPythonSystem::SetSoftwareTiling(bool isEnable)
{
	if (isEnable)
		m_Config.bSoftwareTiling=1;
	else
		m_Config.bSoftwareTiling=2;
}

bool CPythonSystem::IsSoftwareTiling()
{
	if (m_Config.bSoftwareTiling==1)
		return true;

	return false;
}

bool CPythonSystem::IsUseDefaultIME()
{
	return m_Config.bUseDefaultIME;
}

bool CPythonSystem::IsShrinkMap()
{
	return m_Config.bIsShrinkMap;
}

void CPythonSystem::SetShrinkMap(bool state)
{
	m_Config.bIsShrinkMap = state;
}

BYTE CPythonSystem::GetNightMode()
{
	return m_Config.bNightMode;
}

void CPythonSystem::SetNightMode(BYTE bMode)
{
	m_Config.bNightMode = bMode;
}

void CPythonSystem::SetWindowMode(int iFlag)
{
	m_Config.bWindowMode = iFlag;
}

bool CPythonSystem::IsShowEmpireFlag()
{
	return m_Config.bShowEmpireFlag;
}

int CPythonSystem::GetQuestTextDelay()
{
	return m_Config.iQuestTextDelay;
}

void CPythonSystem::SetQuestTextDelay(int delay)
{
	m_Config.iQuestTextDelay = delay;
}

BYTE CPythonSystem::GetShowFloatingText()
{
	return m_Config.bShowFloatingText;
}

void CPythonSystem::SetShowFloatingText(int iFlag)
{
	m_Config.bShowFloatingText = iFlag;
}

bool CPythonSystem::LoadConfig()
{
	FILE * fp = NULL;

	if (NULL == (fp = fopen(gs_strGameConfigFileName.c_str(), "rt")))
	{
		FixScreenResolution();
		return false;
	}

	char buf[256];
	char command[256];
	char value[256];

	while (fgets(buf, 256, fp))
	{
		if (sscanf(buf, " %s %s\n", command, value) == EOF)
			break;

		if (!stricmp(command, "WIDTH"))
			m_Config.width = atoi(value);
		else if (!stricmp(command, "HEIGHT"))
			m_Config.height = atoi(value);
		else if (!stricmp(command, "BPP"))
			m_Config.bpp = atoi(value);
		else if (!stricmp(command, "FREQUENCY"))
			m_Config.frequency = atoi(value);
		else if (!stricmp(command, "CUSTOM_WIDTH"))
			m_Config.custom_width = atoi(value);
		else if (!stricmp(command, "CUSTOM_HEIGHT"))
			m_Config.custom_height = atoi(value);
		else if (!stricmp(command, "SOFTWARE_CURSOR"))
			m_Config.is_software_cursor = atoi(value) ? true : false;
		else if (!stricmp(command, "OBJECT_CULLING"))
			m_Config.is_object_culling = atoi(value) ? true : false;
		else if (!stricmp(command, "VISIBILITY"))
			m_Config.iDistance = atoi(value);
		else if (!stricmp(command, "MUSIC_VOLUME")) {
			if (strchr(value, '.') == 0) { // Old compatiability
				m_Config.music_volume = pow(10.0f, (-1.0f + (((float)atoi(value)) / 5.0f)));
				if (atoi(value) == 0)
					m_Config.music_volume = 0.0f;
			}
			else
				m_Config.music_volume = atof(value);
		}
		else if (!stricmp(command, "VOICE_VOLUME"))
			m_Config.voice_volume = (char)atoi(value);
		else if (!stricmp(command, "GAMMA"))
			m_Config.gamma = atoi(value);
		else if (!stricmp(command, "IS_SAVE_ID"))
			m_Config.isSaveID = atoi(value);
		else if (!stricmp(command, "SAVE_ID"))
			strncpy(m_Config.SaveID, value, 20);
		else if (!stricmp(command, "WINDOWED"))
		{
			m_Config.bWindowMode = atoi(value);
		}
		else if (!stricmp(command, "USE_DEFAULT_IME"))
			m_Config.bUseDefaultIME = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SOFTWARE_TILING"))
			m_Config.bSoftwareTiling = atoi(value);
		else if (!stricmp(command, "SHADOW_LEVEL"))
			m_Config.iShadowLevel = atoi(value);
		else if (!stricmp(command, "DECOMPRESSED_TEXTURE"))
			m_Config.bDecompressDDS = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "NO_SOUND_CARD"))
			m_Config.bNoSoundCard = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "VIEW_CHAT"))
			m_Config.bViewChat = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "ALWAYS_VIEW_NAME"))
			m_Config.bAlwaysShowName = atoi(value);
		else if (!stricmp(command, "SHOW_DAMAGE"))
			m_Config.bShowDamage = atoi(value);
		else if (!stricmp(command, "SHOW_SALESTEXT"))
			m_Config.bShowSalesText = atoi(value) == 1 ? true : false;
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
		else if (!stricmp(command, "SHOW_MOBAIFLAG"))
			m_Config.bShowMobAIFlag = atoi(value) == 1 ? true : false;
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
		else if (!stricmp(command, "SHOW_MOBLEVEL"))
			m_Config.bShowMobLevel = atoi(value) == 1 ? true : false;
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
		else if (!stricmp(command, "SHOP_RANGE_VIEW"))
			m_Config.shopRangeView = std::stof(value);
#endif
#endif
		else if (!stricmp(command, "SCALE_INTERFACE"))
			m_Config.scale_interface = atof(value);
		else if (!stricmp(command, "IS_SCALE_INTERFACE"))
			m_Config.bScaleInterface = atoi(value) ? true : false;
		else if (!stricmp(command, "ANTIALIASING_LEVEL"))
			m_Config.iAliasingLevel = atoi(value);
		else if (!stricmp(command, "MAP_SHRINK"))
			m_Config.bIsShrinkMap = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SHOW_FLOATING_TEXT"))
			m_Config.bShowFloatingText = atoi(value);
		else if (!stricmp(command, "SHOW_EMPIRE_FLAG"))
			m_Config.bShowEmpireFlag = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "NIGHT_MODE"))
			m_Config.bNightMode = atoi(value);
		else if (!stricmp(command, "SHOP_RENDER_DISTANCE"))
			m_Config.shop_render_distance = atof(value);
		else if (!stricmp(command, "FONT_NAME"))
			m_Config.fontName = value;
		else if (!stricmp(command, "FONT_SIZE"))
			m_Config.fontSize = atoi(value);
		else if (!stricmp(command, "TEXTURE_VIEW_TYPE"))
			m_Config.iTextureViewType = atoi(value);
		else if (!stricmp(command, "QUEST_TEXT_DELAY"))
			m_Config.iQuestTextDelay = MINMAX(0, atoi(value), 10);
		else if (!stricmp(command, "LANGUAGE"))
			m_Config.language = value;
	}

	FixScreenResolution();

	m_OldConfig = m_Config;

	fclose(fp);

//	Tracef("LoadConfig: Resolution: %dx%d %dBPP %dHZ Software Cursor: %d, Music/Voice Volume: %d/%d Gamma: %d\n",
//		m_Config.width,
//		m_Config.height,
//		m_Config.bpp,
//		m_Config.frequency,
//		m_Config.is_software_cursor,
//		m_Config.music_volume,
//		m_Config.voice_volume,
//		m_Config.gamma);
	return true;
}

void CPythonSystem::FixScreenResolution()
{
	if (m_Config.bWindowMode == WINDOW_MODE_WINDOWED)
	{
		unsigned screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
		unsigned screen_height = GetSystemMetrics(SM_CYFULLSCREEN);

		if (m_Config.width > screen_width)
			m_Config.width = screen_width;

		if (m_Config.scaled_width > screen_width)
			m_Config.scaled_width = screen_width;

		if (m_Config.height > screen_height) //Fix
		{
			int config_height = m_Config.height;
			int difference = (config_height - screen_height) + 7;
			m_Config.windowHeightDifference = difference;
		}

		if (m_Config.scaled_height > screen_height)
		{
			int config_height = m_Config.scaled_height;
			int difference = (config_height - screen_height) + 7;
			m_Config.windowScaleHeightDifference = difference;
		}

		if (m_Config.custom_width > screen_width)
			m_Config.custom_width = screen_width;

		if (m_Config.custom_height > screen_height)
			m_Config.custom_height = screen_height;
	}
	else if (m_Config.bWindowMode == WINDOW_MODE_FULLSCREEN_WINDOWED)
	{
		unsigned screen_width = GetSystemMetrics(SM_CXSCREEN);
		unsigned screen_height = GetSystemMetrics(SM_CYSCREEN);

		if (m_Config.width != screen_width)
			m_Config.width = screen_width;

		if (m_Config.height != screen_height)
			m_Config.height = screen_height;

		m_Config.windowHeightDifference = 0;
		m_Config.windowScaleHeightDifference = 0;
	}

	m_Config.scaled_width = m_Config.width / m_Config.scale_interface;
	m_Config.scaled_height = m_Config.height / m_Config.scale_interface;
}

bool CPythonSystem::SaveConfig()
{
	FILE *fp;

	if (NULL == (fp = fopen(gs_strGameConfigFileName.c_str(), "wt")))
		return false;

	fprintf(fp, "WIDTH						%d\n"
				"HEIGHT						%d\n"
				"CUSTOM_WIDTH				%d\n"
				"CUSTOM_HEIGHT				%d\n"
				"BPP						%d\n"
				"FREQUENCY					%d\n"
				"SCALED_WIDTH				%d\n"
				"SCALED_HEIGHT				%d\n"
				"SCALE_INTERFACE			%.1f\n"
				"IS_SCALE_INTERFACE			%d\n"
				"SOFTWARE_CURSOR			%d\n"
				"OBJECT_CULLING				%d\n"
				"VISIBILITY					%d\n"
				"MUSIC_VOLUME				%.3f\n"
				"VOICE_VOLUME				%u\n"
				"GAMMA						%d\n"
				"IS_SAVE_ID					%d\n"
				"SAVE_ID					%s\n"
				"DECOMPRESSED_TEXTURE		%d\n"
				"WINDOWED					%d\n",
				m_Config.width,
				m_Config.height,
				m_Config.custom_width,
				m_Config.custom_height,
				m_Config.bpp,
				m_Config.frequency,
				m_Config.scaled_width,
				m_Config.scaled_height,
				m_Config.scale_interface,
				m_Config.bScaleInterface,
				m_Config.is_software_cursor ? 1 : 0,
				m_Config.is_object_culling ? 1 : 0,
				m_Config.iDistance,
				m_Config.music_volume,
				m_Config.voice_volume,
				m_Config.gamma,
				m_Config.isSaveID,
				m_Config.SaveID,
				m_Config.bDecompressDDS ? 1 : 0,
				m_Config.bWindowMode);

	if (m_Config.bViewChat == 0)
		fprintf(fp, "VIEW_CHAT				%d\n", m_Config.bViewChat);
	if (m_Config.bAlwaysShowName != DEFAULT_VALUE_ALWAYS_SHOW_NAME)
		fprintf(fp, "ALWAYS_VIEW_NAME		%u\n", m_Config.bAlwaysShowName);
	if (m_Config.bShowSalesText == 0)
		fprintf(fp, "SHOW_SALESTEXT		%d\n", m_Config.bShowSalesText);
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
	if (m_Config.bShowMobAIFlag == 0)
		fprintf(fp, "SHOW_MOBAIFLAG		%d\n", m_Config.bShowMobAIFlag);
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
	if (m_Config.bShowMobLevel == 0)
		fprintf(fp, "SHOW_MOBLEVEL		%d\n", m_Config.bShowMobLevel);
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
	fprintf(fp, "SHOP_RANGE_VIEW		%f\n", m_Config.shopRangeView);
#endif
#endif

	if (m_Config.bIsShrinkMap == 1)
		fprintf(fp, "MAP_SHRINK		%d\n", m_Config.bIsShrinkMap);
	if (m_Config.bShowFloatingText)
		fprintf(fp, "SHOW_FLOATING_TEXT		%d\n", m_Config.bShowFloatingText);
	if (m_Config.shop_render_distance < 1)
		fprintf(fp, "SHOP_RENDER_DISTANCE		%.2f\n", m_Config.shop_render_distance);
	if (m_Config.bNightMode >= 0)
		fprintf(fp, "NIGHT_MODE		%d\n", m_Config.bNightMode);
	if (m_Config.bShowEmpireFlag < 1)
		fprintf(fp, "SHOW_EMPIRE_FLAG    %d\n", m_Config.bShowEmpireFlag);
	if (m_Config.iQuestTextDelay != 10)
		fprintf(fp, "QUEST_TEXT_DELAY		%d\n", m_Config.iQuestTextDelay);

	fprintf(fp, "USE_DEFAULT_IME		%d\n", m_Config.bUseDefaultIME);
	fprintf(fp, "SOFTWARE_TILING		%d\n", m_Config.bSoftwareTiling);
	fprintf(fp, "SHADOW_LEVEL			%d\n", m_Config.iShadowLevel);
	fprintf(fp, "ANTIALIASING_LEVEL	%d\n", m_Config.iAliasingLevel);
	fprintf(fp, "FONT_NAME				%s\n", m_Config.fontName.c_str());
	fprintf(fp, "FONT_SIZE				%d\n", m_Config.fontSize);
	fprintf(fp, "TEXTURE_VIEW_TYPE		%d\n", m_Config.iTextureViewType);
	fprintf(fp, "SHOW_DAMAGE		%d\n", m_Config.bShowDamage);
	fprintf(fp, "LANGUAGE				%s\n", m_Config.language.c_str());
	fprintf(fp, "\n");

	fclose(fp);
	return true;
}

bool CPythonSystem::LoadInterfaceStatus()
{
	FILE * File;
	File = fopen("interface.cfg", "rb");

	if (!File)
		return false;

	fread(m_WindowStatus, 1, sizeof(TWindowStatus) * WINDOW_MAX_NUM, File);
	fclose(File);
	return true;
}

void CPythonSystem::SaveInterfaceStatus()
{
	if (!m_poInterfaceHandler)
		return;

	PyCallClassMemberFunc(m_poInterfaceHandler, "OnSaveInterfaceStatus", Py_BuildValue("()"));

	FILE * File;

	File = fopen("interface.cfg", "wb");

	if (!File)
	{
		TraceError("Cannot open interface.cfg");
		return;
	}

	fwrite(m_WindowStatus, 1, sizeof(TWindowStatus) * WINDOW_MAX_NUM, File);
	fclose(File);
}

bool CPythonSystem::isInterfaceConfig()
{
	return m_isInterfaceConfig;
}

const CPythonSystem::TWindowStatus & CPythonSystem::GetWindowStatusReference(int iIndex)
{
	return m_WindowStatus[iIndex];
}

void CPythonSystem::ApplyConfig()
{
	if (m_OldConfig.gamma != m_Config.gamma)
	{
		float val = 1.0f;

		switch (m_Config.gamma)
		{
			case 0: val = 0.4f;	break;
			case 1: val = 0.7f; break;
			case 2: val = 1.0f; break;
			case 3: val = 1.2f; break;
			case 4: val = 1.4f; break;
		}

		CPythonGraphic::Instance().SetGamma(val);
	}

	if (m_OldConfig.is_software_cursor != m_Config.is_software_cursor)
	{
		if (m_Config.is_software_cursor)
			CPythonApplication::Instance().SetCursorMode(CPythonApplication::CURSOR_MODE_SOFTWARE);
		else
			CPythonApplication::Instance().SetCursorMode(CPythonApplication::CURSOR_MODE_HARDWARE);
	}

	m_OldConfig = m_Config;

	ChangeSystem();
}

void CPythonSystem::ChangeSystem()
{
	// Shadow
	/*
	if (m_Config.is_shadow)
		CScreen::SetShadowFlag(true);
	else
		CScreen::SetShadowFlag(false);
	*/
	CSoundManager& rkSndMgr = CSoundManager::Instance();
	/*
	float fMusicVolume;
	if (0 == m_Config.music_volume)
		fMusicVolume = 0.0f;
	else
		fMusicVolume= (float)pow(10.0f, (-1.0f + (float)m_Config.music_volume / 5.0f));
		*/
	rkSndMgr.SetMusicVolume(m_Config.music_volume);

	/*
	float fVoiceVolume;
	if (0 == m_Config.voice_volume)
		fVoiceVolume = 0.0f;
	else
		fVoiceVolume = (float)pow(10.0f, (-1.0f + (float)m_Config.voice_volume / 5.0f));
	*/
	rkSndMgr.SetSoundVolumeGrade(m_Config.voice_volume);
}

void CPythonSystem::Clear()
{
	SetInterfaceHandler(NULL);
}

CPythonSystem::CPythonSystem()
{
	memset(&m_Config, 0, sizeof(TConfig));
	m_poInterfaceHandler = NULL;
	SetDefaultConfig();
	LoadConfig();

	ChangeSystem();

	if (LoadInterfaceStatus())
		m_isInterfaceConfig = true;
	else
		m_isInterfaceConfig = false;
}

CPythonSystem::~CPythonSystem()
{
	assert(m_poInterfaceHandler==NULL && "CPythonSystem MUST CLEAR!");
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
