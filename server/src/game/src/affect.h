#ifndef __INC_AFFECT_H
#define __INC_AFFECT_H

class CAffect
{
	public:
		DWORD	dwType;
		BYTE    bApplyOn;
		long    lApplyValue;
		DWORD   dwFlag;
		long	lDuration;
		long	lSPCost;
		DWORD	dwItemVnum;

		static CAffect* Acquire();
		static void Release(CAffect* p);
};

extern void SendAffectAddPacket(LPDESC d, CAffect * pkAff);

// AFFECT_DURATION_BUG_FIX
enum AffectVariable
{
	INFINITE_AFFECT_DURATION = 60 * 365 * 24 * 60 * 60
};
// END_AFFECT_DURATION_BUG_FIX

#endif

