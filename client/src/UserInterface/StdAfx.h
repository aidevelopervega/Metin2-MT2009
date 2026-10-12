#pragma once

#pragma warning(disable:4702)
#pragma warning(disable:4100)
#pragma warning(disable:4201)
#pragma warning(disable:4511)
#pragma warning(disable:4663)
#pragma warning(disable:4018)
#pragma warning(disable:4245)

#if _MSC_VER >= 1400
//if don't use below, time_t is 64bit
#define _USE_32BIT_TIME_T
#endif
#include <iterator>
#include "../eterLib/StdAfx.h"
#include "../eterPythonLib/StdAfx.h"
#include "../gameLib/StdAfx.h"
#include "../scriptLib/StdAfx.h"
#include "../milesLib/StdAfx.h"
#include "../EffectLib/StdAfx.h"
#include "../PRTerrainLib/StdAfx.h"
#include "../SpeedTreeLib/StdAfx.h"

#ifndef __D3DRM_H__
#define __D3DRM_H__
#endif

#include <dshow.h>
#include <qedit.h>

#include "Locale.h"

#include "GameType.h"
extern DWORD __DEFAULT_CODE_PAGE__;

#define APP_NAME	"Metin 2"

void initudp();
void initapp();
void initime();
void initsystemSetting();
void initchr();
void initchrmgr();
void initChat();
void initTextTail();
void initime();
void initItem();
void initNonPlayer();
void initnet();
void initPlayer();
void initSectionDisplayer();
void initServerStateChecker();
void initTrade();
void initMiniMap();
void initProfiler();
void initEvent();
void initeffect();
void initsnd();
void initeventmgr();
void initBackground();
void initwndMgr();
void initshop();
void initpack();
void initskill();
void initfly();
void initquest();
void initsafebox();
void initguild();
void initMessenger();
void initcaptcha();
#ifdef ENABLE_IKASHOP_RENEWAL
void initikashop();
#endif
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
void initAcce();
#endif
void initplayersettingsmodule();

extern const std::string& ApplicationStringTable_GetString(DWORD dwID);
extern const std::string& ApplicationStringTable_GetString(DWORD dwID, LPCSTR szKey);

extern const char* ApplicationStringTable_GetStringz(DWORD dwID);
extern const char* ApplicationStringTable_GetStringz(DWORD dwID, LPCSTR szKey);

extern void ApplicationSetErrorString(const char* szErrorString);
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

enum EIkashop {
//#ifdef ENABLE_IKASHOP_RENEWAL
//	OFFLINE_SHOP_NAME_MAX_LEN = 15 + CHARACTER_NAME_MAX_LEN +1,
//#ifdef ENABLE_CHEQUE_SYSTEM
//	YANG_PER_CHEQUE = 100'000'000,
//#endif
//#ifdef EXTEND_IKASHOP_PRO
//	OFFLINESHOP_NOTIFICATION_FORMAT_LEN = 40,
//#endif
//#endif
};
// Files shared by GameCore.top
