#ifndef __INC_METIN_II_MONARCH_H__
#define __INC_METIN_II_MONARCH_H__

#include "../../common/tables.h"

class CMonarch : public singleton<CMonarch>
{
	public:
		CMonarch();
		virtual ~CMonarch();

		bool Initialize();

		int HealMyEmpire(LPCHARACTER ch, YANG price);
		void SetMonarchInfo(TMonarchInfo * pInfo);

		bool IsMonarch(uint64_t pid, BYTE bEmpire);
		bool IsMoneyOk(YANG price, BYTE bEmpire);
		bool SendtoDBAddMoney(YANG Money, BYTE bEmpire, LPCHARACTER ch);
		bool SendtoDBDecMoney(YANG Money, BYTE bEmpire, LPCHARACTER ch);

		bool AddMoney(YANG Money, BYTE bEmpire);
		bool DecMoney(YANG Money, BYTE bEmpire);
		YANG GetMoney(BYTE bEmpire);

		TMonarchInfo* GetMonarch();

		uint64_t GetMonarchPID(BYTE Empire);

		bool IsPowerUp(BYTE Empire);
		bool IsDefenceUp(BYTE Empire);

		int	GetPowerUpCT(BYTE Empire)
		{
			return Empire < _countof(m_PowerUpCT) ? m_PowerUpCT[Empire] : 0;
		}

		bool CheckPowerUpCT(BYTE Empire);
		bool CheckDefenseUpCT(BYTE Empire);

		int	GetDefenseUpCT(BYTE Empire)
		{
			return Empire < _countof(m_DefenseUpCT) ? m_DefenseUpCT[Empire] : 0;
		}

		void PowerUp(BYTE Empire, bool On);
		void DefenseUp(BYTE Empire, bool On);

	private:
		TMonarchInfo m_MonarchInfo;

		int m_PowerUp[4];
		int m_DefenseUp[4];

		int m_PowerUpCT[4];
		int m_DefenseUpCT[4];
};

bool IsMonarchWarpZone (int map_idx);

#endif
