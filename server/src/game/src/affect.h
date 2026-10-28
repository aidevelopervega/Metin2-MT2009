#ifndef __INC_AFFECT_H
#define __INC_AFFECT_H

#include <cstdint>

class CAffect
{
	public:
		DWORD	dwType;
		BYTE    bApplyOn;
		int32_t lApplyValue;
		DWORD   dwFlag;
		int64_t	lDuration;
		int64_t	lSPCost;
		DWORD	dwItemVnum;

		static CAffect* Acquire();
		static void Release(CAffect* p);
};

extern void SendAffectAddPacket(LPDESC d, CAffect * pkAff);

// AFFECT_DURATION_BUG_FIX
static constexpr int64_t INFINITE_AFFECT_DURATION = 60LL * 365 * 24 * 60 * 60;
// END_AFFECT_DURATION_BUG_FIX

#endif

