#ifndef __INC_METiN_II_DBSERV_STDAFX_H__
#define __INC_METiN_II_DBSERV_STDAFX_H__

#if __has_include("../../../../../server/src/libthecore/include/stdafx.h")
#include "../../../../../server/src/libthecore/include/stdafx.h"
#elif __has_include("../../../../server/src/libthecore/include/stdafx.h")
#include "../../../../server/src/libthecore/include/stdafx.h"
#elif __has_include("../../libthecore/include/stdafx.h")
#include "../../libthecore/include/stdafx.h"
#else
#include <libthecore/include/stdafx.h>
#endif

#ifndef __WIN32__
#include <semaphore.h>
#else
#define isdigit iswdigit
#define isspace iswspace
#endif

#if __has_include("../../../../../server/src/common/length.h")
#include "../../../../../server/src/common/length.h"
#include "../../../../../server/src/common/tables.h"
#include "../../../../../server/src/common/singleton.h"
#include "../../../../../server/src/common/utils.h"
#include "../../../../../server/src/common/stl.h"
#include "../../../../../server/src/common/service.h"
#elif __has_include("../../common/length.h")
#include "../../common/length.h"
#include "../../common/tables.h"
#include "../../common/singleton.h"
#include "../../common/utils.h"
#include "../../common/stl.h"
#include "../../common/service.h"
#else
#include <common/length.h>
#include <common/tables.h>
#include <common/singleton.h>
#include <common/utils.h>
#include <common/stl.h>
#include <common/service.h>
#endif

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
