#include "StdAfx.h"
#include "PythonSystem.h"

PyObject * systemGetWidth(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetWidth());
}

PyObject * systemGetHeight(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetHeight());
}

PyObject * systemSetInterfaceHandler(PyObject* poSelf, PyObject* poArgs)
{
	PyObject* poHandler;
	if (!PyTuple_GetObject(poArgs, 0, &poHandler))
		return Py_BuildException();

	CPythonSystem::Instance().SetInterfaceHandler(poHandler);
	return Py_BuildNone();
}

PyObject * systemDestroyInterfaceHandler(PyObject* poSelf, PyObject* poArgs)
{
	CPythonSystem::Instance().DestroyInterfaceHandler();
	return Py_BuildNone();
}

PyObject * systemReserveResource(PyObject* poSelf, PyObject* poArgs)
{
	std::set<std::string> ResourceSet;
	CResourceManager::Instance().PushBackgroundLoadingSet(ResourceSet);
	return Py_BuildNone();
}

PyObject * systemisInterfaceConfig(PyObject* poSelf, PyObject* poArgs)
{
	int isInterfaceConfig = CPythonSystem::Instance().isInterfaceConfig();
	return Py_BuildValue("i", isInterfaceConfig);
}

PyObject * systemSaveWindowStatus(PyObject* poSelf, PyObject* poArgs)
{
	int iIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iIndex))
		return Py_BuildException();

	int iVisible;
	if (!PyTuple_GetInteger(poArgs, 1, &iVisible))
		return Py_BuildException();

	int iMinimized;
	if (!PyTuple_GetInteger(poArgs, 2, &iMinimized))
		return Py_BuildException();

	int ix;
	if (!PyTuple_GetInteger(poArgs, 3, &ix))
		return Py_BuildException();

	int iy;
	if (!PyTuple_GetInteger(poArgs, 4, &iy))
		return Py_BuildException();

	int iHeight;
	if (!PyTuple_GetInteger(poArgs, 5, &iHeight))
		return Py_BuildException();

	CPythonSystem::Instance().SaveWindowStatus(iIndex, iVisible, iMinimized, ix, iy, iHeight);
	return Py_BuildNone();
}

PyObject * systemGetWindowStatus(PyObject* poSelf, PyObject* poArgs)
{
	int iIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iIndex))
		return Py_BuildException();

	const CPythonSystem::TWindowStatus & c_rWindowStatus = CPythonSystem::Instance().GetWindowStatusReference(iIndex);
	return Py_BuildValue("iiiii", c_rWindowStatus.isVisible,
								  c_rWindowStatus.isMinimized,
								  c_rWindowStatus.ixPosition,
								  c_rWindowStatus.iyPosition,
								  c_rWindowStatus.iHeight);
}

PyObject * systemGetConfig(PyObject * poSelf, PyObject * poArgs)
{
	CPythonSystem::TConfig *tmp = CPythonSystem::Instance().GetConfig();

	int iRes = CPythonSystem::Instance().GetResolutionIndex(tmp->width, tmp->height, tmp->bpp);
	int iFrequency = CPythonSystem::Instance().GetFrequencyIndex(iRes, tmp->frequency);

	return Py_BuildValue("iiiiiiii",  iRes,
									  iFrequency,
									  tmp->is_software_cursor,
									  tmp->is_object_culling,
									  tmp->music_volume,
									  tmp->voice_volume,
									  tmp->gamma,
									  tmp->iDistance);
}

PyObject * systemSetSaveID(PyObject * poSelf, PyObject * poArgs)
{
	int iValue;
	if (!PyTuple_GetInteger(poArgs, 0, &iValue))
		return Py_BuildException();

	char * szSaveID;
	if (!PyTuple_GetString(poArgs, 1, &szSaveID))
		return Py_BuildException();

	CPythonSystem::Instance().SetSaveID(iValue, szSaveID);
	return Py_BuildNone();
}

PyObject * systemisSaveID(PyObject * poSelf, PyObject * poArgs)
{
	int value = CPythonSystem::Instance().IsSaveID();
	return Py_BuildValue("i", value);
}

PyObject * systemGetSaveID(PyObject * poSelf, PyObject * poArgs)
{
	const char * c_szSaveID = CPythonSystem::Instance().GetSaveID();
	return Py_BuildValue("s", c_szSaveID);
}

PyObject * systemGetMusicVolume(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("f", CPythonSystem::Instance().GetMusicVolume());
}

PyObject * systemGetSoundVolume(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetSoundVolume());
}

PyObject * systemSetMusicVolume(PyObject * poSelf, PyObject * poArgs)
{
	float fVolume;
	if (!PyTuple_GetFloat(poArgs, 0, &fVolume))
		return Py_BuildException();

	CPythonSystem::Instance().SetMusicVolume(fVolume);
	return Py_BuildNone();
}

PyObject * systemSetSoundVolumef(PyObject * poSelf, PyObject * poArgs)
{
	float fVolume;
	if (!PyTuple_GetFloat(poArgs, 0, &fVolume))
		return Py_BuildException();

	CPythonSystem::Instance().SetSoundVolumef(fVolume);
	return Py_BuildNone();
}

PyObject * systemIsSoftwareCursor(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsSoftwareCursor());
}

PyObject * systemSetViewChatFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetViewChatFlag(iFlag);

	return Py_BuildNone();
}

PyObject * systemIsViewChat(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsViewChat());
}

PyObject * systemSetShowNameFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetShowNameFlag(iFlag);

	return Py_BuildNone();
}

PyObject * systemSetShowDamageFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetShowDamageFlag(iFlag);

	return Py_BuildNone();
}

PyObject * systemSetShowSalesTextFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetShowSalesTextFlag(iFlag);

	return Py_BuildNone();
}

#ifdef WJ_SHOW_MOB_INFO
#include "PythonCharacterManager.h"
#include "PythonNonPlayer.h"
#include "PythonSystem.h"
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
PyObject * systemIsShowMobAIFlag(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsShowMobAIFlag());
}
PyObject * systemSetShowMobAIFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetShowMobAIFlagFlag(iFlag);

	for (CPythonCharacterManager::CharacterIterator it=CPythonCharacterManager::Instance().CharacterInstanceBegin(), ti=CPythonCharacterManager::Instance().CharacterInstanceEnd();
			it!=ti;
			++it)
	{
		CInstanceBase * pkInst = *it;
		if (pkInst && pkInst->IsEnemy())
			pkInst->MobInfoAiFlagRefresh();
	}
	return Py_BuildNone();
}
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
PyObject * systemIsShowMobLevel(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsShowMobLevel());
}
PyObject * systemSetShowMobLevel(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetShowMobLevelFlag(iFlag);

	for (CPythonCharacterManager::CharacterIterator it=CPythonCharacterManager::Instance().CharacterInstanceBegin(), ti=CPythonCharacterManager::Instance().CharacterInstanceEnd();
			it!=ti;
			++it)
	{
		CInstanceBase * pkInst = *it;
		if (pkInst && pkInst->IsEnemy())
			pkInst->MobInfoLevelRefresh();
	}
	return Py_BuildNone();
}
#endif
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
PyObject* systemGetShopRangeView(PyObject* poSelf, PyObject* poArgs)
{
	return PyFloat_FromDouble(CPythonSystem::instance().GetShopRangeView());
}

PyObject* systemSetShopRangeView(PyObject* poSelf, PyObject* poArgs)
{
	float view = 0.0f;
	if(!PyTuple_GetFloat(poArgs, 0, &view))
		return Py_BadArgument();

	CPythonSystem::instance().SetShopRangeView(view);
	return Py_BuildNone();
}
#endif
#endif

PyObject* systemGetShowName(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetShowName());
}

PyObject * systemIsAlwaysShowName(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsAlwaysShowName());
}

PyObject* systemIsGroupShowName(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsGroupShowName());
}

PyObject * systemGetShowDamage(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetShowDamage());
}

PyObject * systemIsShowSalesText(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsShowSalesText());
}

PyObject * systemSetConfig(PyObject * poSelf, PyObject * poArgs)
{
	int res_index;
	int width;
	int height;
	int bpp;
	int frequency_index;
	int frequency;
	int software_cursor;
	int shadow;
	int object_culling;
	int music_volume;
	int voice_volume;
	int gamma;
	int distance;

	if (!PyTuple_GetInteger(poArgs, 0, &res_index))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 1, &frequency_index))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 2, &software_cursor))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 3, &shadow))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 4, &object_culling))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 5, &music_volume))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 6, &voice_volume))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 7, &gamma))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 8, &distance))
		return Py_BuildException();

	if (!CPythonSystem::Instance().GetResolution(res_index, (DWORD *) &width, (DWORD *) &height, (DWORD *) &bpp))
		return Py_BuildNone();

	if (!CPythonSystem::Instance().GetFrequency(res_index,frequency_index, (DWORD *) &frequency))
		return Py_BuildNone();

	CPythonSystem::TConfig tmp;

	memcpy(&tmp, CPythonSystem::Instance().GetConfig(), sizeof(tmp));

	tmp.width				= width;
	tmp.height				= height;
	tmp.bpp					= bpp;
	tmp.frequency			= frequency;
	tmp.is_software_cursor	= software_cursor ? true : false;
	tmp.is_object_culling	= object_culling ? true : false;
	tmp.music_volume		= (char) music_volume;
	tmp.voice_volume		= (char) voice_volume;
	tmp.gamma				= gamma;
	tmp.iDistance			= distance;

	CPythonSystem::Instance().SetConfig(&tmp);
	return Py_BuildNone();
}

PyObject * systemApplyConfig(PyObject * poSelf, PyObject * poArgs)
{
	CPythonSystem::Instance().ApplyConfig();
	return Py_BuildNone();
}

PyObject * systemSaveConfig(PyObject * poSelf, PyObject * poArgs)
{
	int ret = CPythonSystem::Instance().SaveConfig();
	return Py_BuildValue("i", ret);
}

PyObject * systemGetResolutionCount(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetResolutionCount());
}

PyObject * systemGetFrequencyCount(PyObject * poSelf, PyObject * poArgs)
{
	int	index;

	if (!PyTuple_GetInteger(poArgs, 0, &index))
		return Py_BuildException();

	return Py_BuildValue("i", CPythonSystem::Instance().GetFrequencyCount(index));
}

PyObject * systemGetResolution(PyObject * poSelf, PyObject * poArgs)
{
	int	index;
	DWORD width = 0, height = 0, bpp = 0;

	if (!PyTuple_GetInteger(poArgs, 0, &index))
		return Py_BuildException();

	CPythonSystem::Instance().GetResolution(index, &width, &height, &bpp);
	return Py_BuildValue("iii", width, height, bpp);
}

PyObject * systemGetCurrentResolution(PyObject * poSelf, PyObject *poArgs)
{
	CPythonSystem::TConfig *tmp = CPythonSystem::Instance().GetConfig();
	return Py_BuildValue("iii", tmp->width, tmp->height, tmp->bpp);
}

PyObject * systemGetFrequency(PyObject * poSelf, PyObject * poArgs)
{
	int	index, frequency_index;
	DWORD frequency = 0;

	if (!PyTuple_GetInteger(poArgs, 0, &index))
		return Py_BuildException();

	if (!PyTuple_GetInteger(poArgs, 1, &frequency_index))
		return Py_BuildException();

	CPythonSystem::Instance().GetFrequency(index, frequency_index, &frequency);
	return Py_BuildValue("i", frequency);
}

PyObject * systemGetShadowLevel(PyObject * poSelf, PyObject * poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetShadowLevel());
}

PyObject * systemSetShadowLevel(PyObject * poSelf, PyObject * poArgs)
{
	int level;

	if (!PyTuple_GetInteger(poArgs, 0, &level))
		return Py_BuildException();

	if (level > 0)
		CPythonSystem::Instance().SetShadowLevel(level);

	return Py_BuildNone();
}

PyObject* systemSetShowFloatingText(PyObject* poSelf, PyObject* poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BuildException();

	CPythonSystem::Instance().SetShowFloatingText(iFlag);

	return Py_BuildNone();
}

PyObject* systemGetShowFloatingText(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetShowFloatingText());
}

PyObject* systemSetShrinkMap(PyObject* poSelf, PyObject* poArgs)
{
	bool bState;
	if (!PyTuple_GetBoolean(poArgs, 0, &bState))
		return Py_BuildException();

	CPythonSystem::Instance().SetShrinkMap(bState);

	return Py_BuildNone();
}

PyObject* systemIsShrinkMap(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsShrinkMap());
}

PyObject* systemSetResolution(PyObject* poSelf, PyObject* poArgs)
{
	int width;
	if (!PyTuple_GetInteger(poArgs, 0, &width))
		return Py_BuildException();

	int height;
	if (!PyTuple_GetInteger(poArgs, 1, &height))
		return Py_BuildException();

	int bpp;
	if (!PyTuple_GetInteger(poArgs, 2, &bpp))
		return Py_BuildException();

	if (width > 0 && height > 0)
		CPythonSystem::Instance().SetResolution(width, height, bpp);

	return Py_BuildNone();
}


PyObject* systemSetWindowed(PyObject* poSelf, PyObject* poArgs)
{
	int iWindowed;
	if (!PyTuple_GetInteger(poArgs, 0, &iWindowed))
		return Py_BuildException();

	CPythonSystem::Instance().SetWindowMode(iWindowed);

	return Py_BuildNone();
}

PyObject* systemIsWindowed(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsWindowed());
}

PyObject* systemGetScaledResolution(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("ii", CPythonSystem::Instance().GetScaledWidth(), CPythonSystem::Instance().GetScaledHeight());
}

PyObject* systemIsScaleMode(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsScaleMode());
}

PyObject* systemGetScaleInterface(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("f", CPythonSystem::Instance().GetScaleInterface());
}

PyObject* systemSetScaleInterface(PyObject* poSelf, PyObject* poArgs)
{
	float scale;
	if (!PyTuple_GetFloat(poArgs, 0, &scale))
		return Py_BuildException();

	CPythonSystem::Instance().SetScaleInterface(scale);

	return Py_BuildNone();
}

PyObject* systemSetIsScaleInterface(PyObject* poSelf, PyObject* poArgs)
{
	bool isScale;
	if (!PyTuple_GetBoolean(poArgs, 0, &isScale))
		return Py_BuildException();

	CPythonSystem::Instance().SetScaleInterface(isScale);

	return Py_BuildNone();
}

PyObject* systemSetFont(PyObject* poSelf, PyObject* poArgs)
{
	char* fontName;
	if (!PyTuple_GetString(poArgs, 0, &fontName))
		return Py_BuildException();

	int size;
	if (!PyTuple_GetInteger(poArgs, 1, &size))
		return Py_BuildException();

	if (size > 14)
		size = 14;

	CPythonSystem::Instance().SetFont(fontName, size);
	return Py_BuildNone();
}

PyObject* systemGetFont(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("s", CPythonSystem::Instance().GetFont().c_str());
}

PyObject* systemGetFontName(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("s", CPythonSystem::Instance().GetFontName());
}


PyObject* systemGetFontSize(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetFontSize());
}

PyObject* systemGetSamplingLevel(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetAntiAliasingLevel());
}

PyObject* systemSetSamplingLevel(PyObject* poSelf, PyObject* poArgs)
{
	int level;
	if (!PyTuple_GetInteger(poArgs, 0, &level))
		return Py_BuildException();

	CPythonSystem::Instance().SetAntiAliasingLevel(level);
	return Py_BuildNone();
}

PyObject* systemGetTextureViewType(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetTextureViewType());
}

PyObject* systemSetTextureViewType(PyObject* poSelf, PyObject* poArgs)
{
	int type;

	if (!PyTuple_GetInteger(poArgs, 0, &type))
		return Py_BuildException();

	CPythonSystem::Instance().SetTextureViewType(type);

	return Py_BuildNone();
}

PyObject* systemGetNightMode(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetNightMode());
}

PyObject* systemSetNightMode(PyObject* poSelf, PyObject* poArgs)
{
	int iMode;
	if (!PyTuple_GetInteger(poArgs, 0, &iMode))
		return Py_BuildException();

	CPythonSystem::Instance().SetNightMode(iMode);

	return Py_BuildNone();
}

PyObject* systemGetQuestTextDelay(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().GetQuestTextDelay());
}

PyObject* systemSetQuestTextDelay(PyObject* poSelf, PyObject* poArgs)
{
	int iDelay;
	if (!PyTuple_GetInteger(poArgs, 0, &iDelay))
		return Py_BuildException();

	CPythonSystem::Instance().SetQuestTextDelay(iDelay);

	return Py_BuildNone();
}

PyObject* systemGetLanguage(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("s", CPythonSystem::Instance().GetLanguage().c_str());
}

PyObject* systemSetLanguage(PyObject* poSelf, PyObject* poArgs)
{
	char* szLanguage;
	if (!PyTuple_GetString(poArgs, 0, &szLanguage))
		return Py_BuildException();

	CPythonSystem::Instance().SetLanguage(szLanguage);
	return Py_BuildNone();
}

PyObject* systemGetCustomResolution(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("ii", CPythonSystem::Instance().GetCustomWidth(), CPythonSystem::Instance().GetCustomHeight());
}

PyObject* systemSetCustomResolution(PyObject* poSelf, PyObject* poArgs)
{
	int width;
	if (!PyTuple_GetInteger(poArgs, 0, &width))
		return Py_BuildException();

	int height;
	if (!PyTuple_GetInteger(poArgs, 1, &height))
		return Py_BuildException();

	if (width >= 0 && height >= 0)
		CPythonSystem::Instance().SetCustomResolution(width, height);

	return Py_BuildNone();
}

PyObject* systemIsShopViewOnlyFound(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonSystem::Instance().IsShopViewOnlyFound());
}

PyObject* systemSetShopViewOnlyFound(PyObject* poSelf, PyObject* poArgs)
{
	bool state;
	if (!PyTuple_GetBoolean(poArgs, 0, &state))
		return Py_BuildException();

	CPythonSystem::Instance().SetShopViewOnlyFound(state);
	return Py_BuildNone();
}

void initsystemSetting()
{
	static PyMethodDef s_methods[] =
	{
		{ "GetWidth",					systemGetWidth,					METH_VARARGS },
		{ "GetHeight",					systemGetHeight,				METH_VARARGS },

		{ "SetInterfaceHandler",		systemSetInterfaceHandler,		METH_VARARGS },
		{ "DestroyInterfaceHandler",	systemDestroyInterfaceHandler,	METH_VARARGS },
		{ "ReserveResource",			systemReserveResource,			METH_VARARGS },

		{ "isInterfaceConfig",			systemisInterfaceConfig,		METH_VARARGS },
		{ "SaveWindowStatus",			systemSaveWindowStatus,			METH_VARARGS },
		{ "GetWindowStatus",			systemGetWindowStatus,			METH_VARARGS },

		{ "GetResolutionCount",			systemGetResolutionCount,		METH_VARARGS },
		{ "GetFrequencyCount",			systemGetFrequencyCount,		METH_VARARGS },

		{ "GetCurrentResolution",		systemGetCurrentResolution,		METH_VARARGS },

		{ "GetResolution",				systemGetResolution,			METH_VARARGS },
		{ "GetFrequency",				systemGetFrequency,				METH_VARARGS },

		{ "ApplyConfig",				systemApplyConfig,				METH_VARARGS },
		{ "SetConfig",					systemSetConfig,				METH_VARARGS },
		{ "SaveConfig",					systemSaveConfig,				METH_VARARGS },
		{ "GetConfig",					systemGetConfig,				METH_VARARGS },

		{ "SetSaveID",					systemSetSaveID,				METH_VARARGS },
		{ "isSaveID",					systemisSaveID,					METH_VARARGS },
		{ "GetSaveID",					systemGetSaveID,				METH_VARARGS },

		{ "GetMusicVolume",				systemGetMusicVolume,			METH_VARARGS },
		{ "GetSoundVolume",				systemGetSoundVolume,			METH_VARARGS },

		{ "SetMusicVolume",				systemSetMusicVolume,			METH_VARARGS },
		{ "SetSoundVolumef",			systemSetSoundVolumef,			METH_VARARGS },
		{ "IsSoftwareCursor",			systemIsSoftwareCursor,			METH_VARARGS },

		{ "SetViewChatFlag",			systemSetViewChatFlag,			METH_VARARGS },
		{ "IsViewChat",					systemIsViewChat,				METH_VARARGS },

		{ "SetShowNameFlag",			systemSetShowNameFlag,			METH_VARARGS },
		{ "IsAlwaysShowName",			systemIsAlwaysShowName,			METH_VARARGS },
		{ "IsGroupShowName",			systemIsGroupShowName,			METH_VARARGS },
		{ "GetShowName",				systemGetShowName,				METH_VARARGS },

		{ "SetShowDamageFlag",			systemSetShowDamageFlag,		METH_VARARGS },
		{ "GetShowDamage",				systemGetShowDamage,				METH_VARARGS },

		{ "SetShowSalesTextFlag",		systemSetShowSalesTextFlag,		METH_VARARGS },
		{ "IsShowSalesText",			systemIsShowSalesText,			METH_VARARGS },

		{ "GetShadowLevel",				systemGetShadowLevel,			METH_VARARGS },
		{ "SetShadowLevel",				systemSetShadowLevel,			METH_VARARGS },

#ifdef WJ_SHOW_MOB_INFO
		{ "IsShowMobAIFlag",			systemIsShowMobAIFlag,			METH_VARARGS },
		{ "SetShowMobAIFlag",			systemSetShowMobAIFlag,			METH_VARARGS },

		{ "IsShowMobLevel",				systemIsShowMobLevel,			METH_VARARGS },
		{ "SetShowMobLevel",			systemSetShowMobLevel,			METH_VARARGS },
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
		{"GetShopRangeView",			systemGetShopRangeView,			METH_VARARGS },
		{"SetShopRangeView",			systemSetShopRangeView,			METH_VARARGS },
#endif
#endif

		{ "SetShrinkMap",				systemSetShrinkMap,				METH_VARARGS },
		{ "IsShrinkMap",				systemIsShrinkMap,				METH_VARARGS },

		{ "SetShowFloatingText",		systemSetShowFloatingText,		METH_VARARGS },
		{ "GetShowFloatingText",		systemGetShowFloatingText,		METH_VARARGS },

		{ "SetResolution",				systemSetResolution,			METH_VARARGS },
		{ "SetWindowed",				systemSetWindowed,				METH_VARARGS },
		{ "IsWindowed",					systemIsWindowed,				METH_VARARGS },

		{ "IsScaleMode",				systemIsScaleMode,				METH_VARARGS },
		{ "GetScaledResolution",		systemGetScaledResolution,		METH_VARARGS },
		{ "SetScaleInterface",			systemSetScaleInterface,		METH_VARARGS },
		{ "SetIsScaleInterface",		systemSetIsScaleInterface,		METH_VARARGS },
		{ "GetScaleInterface",			systemGetScaleInterface,		METH_VARARGS },

		{ "SetFont",					systemSetFont,					METH_VARARGS },
		{ "GetFont",					systemGetFont,					METH_VARARGS },
		{ "GetFontName",				systemGetFontName,				METH_VARARGS },
		{ "GetFontSize",				systemGetFontSize,				METH_VARARGS },

		{ "GetSamplingLevel",			systemGetSamplingLevel,			METH_VARARGS },
		{ "SetSamplingLevel",			systemSetSamplingLevel,			METH_VARARGS },

		{ "GetTextureViewType",			systemGetTextureViewType,		METH_VARARGS },
		{ "SetTextureViewType",			systemSetTextureViewType,		METH_VARARGS },

		{ "GetNightMode",				systemGetNightMode,				METH_VARARGS },
		{ "SetNightMode",				systemSetNightMode,				METH_VARARGS },

		{ "GetQuestTextDelay",			systemGetQuestTextDelay,		METH_VARARGS },
		{ "SetQuestTextDelay",			systemSetQuestTextDelay,		METH_VARARGS },

		{ "GetLanguage",				systemGetLanguage,				METH_VARARGS },
		{ "SetLanguage",				systemSetLanguage,				METH_VARARGS },

		{ "SetCustomResolution",		systemSetCustomResolution,		METH_VARARGS },
		{ "GetCustomResolution",		systemGetCustomResolution,		METH_VARARGS },

		{ "SetShopViewOnlyFound",		systemSetShopViewOnlyFound,		METH_VARARGS },
		{ "IsShopViewOnlyFound",		systemIsShopViewOnlyFound,		METH_VARARGS },

		{ NULL,							NULL,							NULL }
	};

	PyObject * poModule = Py_InitModule("systemSetting", s_methods);

	PyModule_AddIntConstant(poModule, "RESOLUTION_MAX_NUM", CPythonSystem::RESOLUTION_MAX_NUM);
	PyModule_AddIntConstant(poModule, "WINDOW_STATUS",		CPythonSystem::WINDOW_STATUS);
	PyModule_AddIntConstant(poModule, "WINDOW_INVENTORY",	CPythonSystem::WINDOW_INVENTORY);
	PyModule_AddIntConstant(poModule, "WINDOW_ABILITY",		CPythonSystem::WINDOW_ABILITY);
	PyModule_AddIntConstant(poModule, "WINDOW_SOCIETY",		CPythonSystem::WINDOW_SOCIETY);
	PyModule_AddIntConstant(poModule, "WINDOW_JOURNAL",		CPythonSystem::WINDOW_JOURNAL);
	PyModule_AddIntConstant(poModule, "WINDOW_COMMAND",		CPythonSystem::WINDOW_COMMAND);

	PyModule_AddIntConstant(poModule, "WINDOW_QUICK",		CPythonSystem::WINDOW_QUICK);
	PyModule_AddIntConstant(poModule, "WINDOW_GAUGE",		CPythonSystem::WINDOW_GAUGE);
	PyModule_AddIntConstant(poModule, "WINDOW_MINIMAP",		CPythonSystem::WINDOW_MINIMAP);
	PyModule_AddIntConstant(poModule, "WINDOW_CHAT",		CPythonSystem::WINDOW_CHAT);

	PyModule_AddIntConstant(poModule, "SHOW_DAMAGE_NONE",		CPythonSystem::ShowDamageNone);
	PyModule_AddIntConstant(poModule, "SHOW_DAMAGE_TARGET",		CPythonSystem::ShowDamageTarget);
	PyModule_AddIntConstant(poModule, "SHOW_DAMAGE_ALL",		CPythonSystem::ShowDamageAll);
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
