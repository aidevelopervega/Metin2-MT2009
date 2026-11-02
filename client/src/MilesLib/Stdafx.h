#ifndef __INC_MILESLIB_STDAFX_H__
#define __INC_MILESLIB_STDAFX_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef MT2_USE_MINIAUDIO
#define MT2_USE_MINIAUDIO 1
#endif

#include <windows.h>
#include <cstdint>

#pragma warning(disable:4786)
#pragma warning(disable:4100)

#include "SoundData.h"

#include "../eterBase/CRC32.h"
#include "../eterBase/Utils.h"
#include "../eterBase/Debug.h"

#include "../UserInterface/Locale_inc.h"

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
