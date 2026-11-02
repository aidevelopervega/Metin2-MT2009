#ifndef _CRYPTOPPLIBLINK_H_
#define _CRYPTOPPLIBLINK_H_

#ifdef _DLL
#ifndef CRYPTOPP_IMPORTS
#define CRYPTOPP_IMPORTS
#endif
#endif

#if defined(CRYPTOPP_IMPORTS)
#include "dll.h"
#elif defined(_WIN64)
#pragma comment( lib, "cryptlib-x64.lib" )
#endif

#endif /* !_CRYPTOPPLIBLINK_H_ */
