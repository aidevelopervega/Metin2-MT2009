#include "stdafx.h"
#include "config.h"
#include "constants.h"
#include "monarch.h"
#include "char.h"
#include "sectree_manager.h"
#include "desc_client.h"


CMonarch::CMonarch()
{
	memset( &m_MonarchInfo, 0, sizeof(m_MonarchInfo) );

	Initialize();
}

CMonarch::~CMonarch()
{
}

bool CMonarch::Initialize()
{
	memset(m_PowerUp, 0, sizeof(m_PowerUp));
	memset(m_DefenseUp, 0, sizeof(m_DefenseUp));
	memset(m_PowerUpCT, 0, sizeof(m_PowerUpCT));
	memset(m_DefenseUpCT, 0, sizeof(m_DefenseUpCT));

	return 0;
}

struct FHealMyEmpire
{
	BYTE m_bEmpire;
	void operator () (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;

			if (ch->IsPC() && m_bEmpire == ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("\xb1\xba\xc1\xd6\xb0\xa1 \xb3\xbb\xb8\xb0 \xc3\xe0\xba\xb9\xc0\xb8\xb7\xce \xb8\xf0\xb5\xe7 \xbf\xa1\xb3\xca\xc1\xf6\xb0\xa1 \xb0\xa1\xb5\xe6 \xc3\xa4\xbf\xf6\xc1\xfd\xb4\xcf\xb4\xd9"));
				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
				ch->EffectPacket(SE_SPUP_BLUE);
				ch->EffectPacket(SE_HPUP_RED);
			}
		}
	}
};

int CMonarch::HealMyEmpire(LPCHARACTER ch, YANG price)
{
	if (price <= 0 || price > 2000000000)
		return 0;

	const int iPrice = static_cast<int>(price);
	BYTE Empire = ch->GetEmpire();
	DWORD pid = ch->GetPlayerID();

	sys_log(0, "HealMyEmpire[%d] pid:%u price %d", Empire, static_cast<unsigned int>(pid), iPrice);

	if (IsMonarch(pid, Empire) == 0)
	{
		if (!ch->IsGM())
		{
			ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("\xb1\xba\xc1\xd6\xc0\xc7 \xc0\xda\xb0\xdd\xc0\xbb \xb0\xa1\xc1\xf6\xb0\xed \xc0\xd6\xc1\xf6 \xbe\xca\xbd\xc0\xb4\xcf\xb4\xd9"));
			sys_err("No Monarch pid %u ", static_cast<unsigned int>(pid));
			return 0;
		}
	}

	if (!ch->IsMCOK(CHARACTER::MI_HEAL))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d \xc3\xca \xc8\xc4\xbf\xa1 \xb1\xba\xc1\xd6\xc0\xc7 \xc3\xe0\xba\xb9\xc0\xbb \xbb\xe7\xbf\xeb\xc7\xd2 \xbc\xf6 \xc0\xd6\xbd\xc0\xb4\xcf\xb4\xd9"), ch->GetMCLTime(CHARACTER::MI_HEAL));

		return 0;
	}

	if (!IsMoneyOk(iPrice, Empire))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("\xb1\xb9\xb0\xed\xbf\xa1 \xb5\xb7\xc0\xcc \xba\xce\xc1\xb7\xc7\xd5\xb4\xcf\xb4\xd9. \xc7\xf6\xc0\xe7 : %u \xc7\xca\xbf\xe4\xb1\xdd\xbe\xd7 : %u"), static_cast<unsigned int>(GetMoney(Empire)), static_cast<unsigned int>(iPrice));
		return 0;
	}

	int iMapIndex = ch->GetMapIndex();

	FHealMyEmpire f;
	f.m_bEmpire = Empire;
	SECTREE_MANAGER::instance().for_each(iMapIndex, f);


	SendtoDBDecMoney(iPrice, Empire, ch);


	ch->SetMC(CHARACTER::MI_HEAL);

	if (test_server)
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[TEST_ONLY]\xc7\xf6\xc0\xe7 \xb1\xb9\xb0\xed : %d "), GetMoney(Empire) - iPrice);
	return 1;
}

void CMonarch::SetMonarchInfo(TMonarchInfo * pInfo)
{
	memcpy(&m_MonarchInfo, pInfo, sizeof(TMonarchInfo));
}

bool CMonarch::IsMonarch(DWORD pid, BYTE bEmpire)
{
	if (bEmpire >= _countof(m_MonarchInfo.pid))
		return false;

	return (m_MonarchInfo.pid[bEmpire] == pid);
}

bool CMonarch::IsMoneyOk(int price, BYTE bEmpire)
{
	return (GetMoney(bEmpire) >= price);
}

bool CMonarch::SendtoDBAddMoney(YANG Money, BYTE bEmpire, LPCHARACTER ch)
{
	if (Money <= 0 || Money > 2000000000)
		return false;

	const YANG totalMoney = static_cast<YANG>(GetMoney(bEmpire)) + Money;
	if (totalMoney > 2000000000 || totalMoney < 0)
		return false;

	int nEmpire = bEmpire;
	int nMoney = static_cast<int>(Money);
	db_clientdesc->DBPacketHeader(HEADER_GD_ADD_MONARCH_MONEY, ch->GetDesc()->GetHandle(), sizeof(int) + sizeof(int));
	db_clientdesc->Packet(&nEmpire, sizeof(int));
	db_clientdesc->Packet(&nMoney, sizeof(int));
	return true;
}

bool CMonarch::SendtoDBDecMoney(int Money, BYTE bEmpire, LPCHARACTER ch)
{
	if (bEmpire >= _countof(m_MonarchInfo.money))
		return false;

	if (Money <= 0 || static_cast<YANG>(GetMoney(bEmpire)) - static_cast<YANG>(Money) < 0)
		return false;


	int nEmpire = bEmpire;

	db_clientdesc->DBPacketHeader(HEADER_GD_DEC_MONARCH_MONEY, ch->GetDesc()->GetHandle(), sizeof(int) + sizeof(int));
	db_clientdesc->Packet(&nEmpire, sizeof(int));
	db_clientdesc->Packet(&Money, sizeof(int));
	return true;
}

bool CMonarch::AddMoney(int Money, BYTE bEmpire)
{
	if (bEmpire >= _countof(m_MonarchInfo.money))
		return false;


	if (Money <= 0 || static_cast<YANG>(GetMoney(bEmpire)) + static_cast<YANG>(Money) > 2000000000)
		return false;

	m_MonarchInfo.money[bEmpire] += Money;
	return true;
}

bool CMonarch::DecMoney(int Money, BYTE bEmpire)
{
	if (bEmpire >= _countof(m_MonarchInfo.money))
		return false;

	if (Money <= 0 || static_cast<YANG>(GetMoney(bEmpire)) - static_cast<YANG>(Money) < 0)
		return false;

	m_MonarchInfo.money[bEmpire] -= Money;
	return true;
}

int CMonarch::GetMoney(BYTE bEmpire)
{
	if (bEmpire >= _countof(m_MonarchInfo.money))
		return 0;

	const YANG money = m_MonarchInfo.money[bEmpire];
	if (money < 0)
		return 0;
	if (money > 2000000000)
		return 2000000000;
	return static_cast<int>(money);
}

TMonarchInfo* CMonarch::GetMonarch()
{
	return &m_MonarchInfo;
}

DWORD CMonarch::GetMonarchPID(BYTE Empire)
{
	return Empire < _countof(m_MonarchInfo.pid) ? m_MonarchInfo.pid[Empire] : 0;
}

bool CMonarch::IsPowerUp(BYTE Empire)
{
	return Empire < _countof(m_PowerUp) ? m_PowerUp[Empire] : false;
}

bool CMonarch::IsDefenceUp(BYTE Empire)
{
	return Empire < _countof(m_DefenseUp) ? m_DefenseUp[Empire] : false;
}

bool CMonarch::CheckPowerUpCT(BYTE Empire)
{
	if (Empire >= _countof(m_PowerUpCT))
		return false;

	if (m_PowerUpCT[Empire] > thecore_pulse())
	{
		if (test_server)
			sys_log(0, "[TEST_ONLY] : CheckPowerUpCT CT%d Now%d 60sec %ld", m_PowerUpCT[Empire], thecore_pulse(), static_cast<long>(PASSES_PER_SEC(60 * 10)));
		return false;
	}

	return true;
}

bool CMonarch::CheckDefenseUpCT(BYTE Empire)
{
	if (Empire >= _countof(m_DefenseUpCT))
		return false;

	if (m_DefenseUpCT[Empire] > thecore_pulse())
	{
		if (test_server)
			sys_log(0, "[TEST_ONLY] : CheckPowerUpCT CT%d Now%d 60sec %ld", m_PowerUpCT[Empire], thecore_pulse(), static_cast<long>(PASSES_PER_SEC(60 * 10)));
		return false;
	}

	return true;
}

void CMonarch::PowerUp(BYTE Empire, bool On)
{
	if (Empire >= _countof(m_PowerUpCT))
		return;

	m_PowerUp[Empire] = On;


	m_PowerUpCT[Empire] = thecore_pulse() + PASSES_PER_SEC(60 * 10);
}

void CMonarch::DefenseUp(BYTE Empire, bool On)
{
	if (Empire >= _countof(m_DefenseUpCT))
		return;

	m_DefenseUp[Empire] = On;


	m_DefenseUpCT[Empire] = thecore_pulse() + PASSES_PER_SEC(60 * 10);
}

bool IsMonarchWarpZone (int map_idx)
{

	if (map_idx >= 10000)
		map_idx /= 10000;

	switch (map_idx)
	{
	case 301:
	case 302:
	case 303:
	case 304:

	case 351:
	case 352:
		return false;
	}

	return (map_idx != 208 && map_idx != 216 && map_idx != 217);
}
