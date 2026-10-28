#include "stdafx.h"
#include "ItemIDRangeManager.h"
#include "Main.h"
#include "DBManager.h"
#include "ClientManager.h"
#include "Peer.h"

CItemIDRangeManager::CItemIDRangeManager()
{
	m_listData.clear();
}

void CItemIDRangeManager::Build()
{
	uint64_t dwMin = 0;
	uint64_t dwMax = 0;
	TItemIDRangeTable range;

	for (uint64_t i = 0; ; ++i)
	{
		dwMin = cs_dwMinimumRange * (i + 1) + 1;
		dwMax = cs_dwMinimumRange * (i + 2);

		if (dwMax >= cs_dwDefaultPrebuildMaxItemID)
			break;

		if (CClientManager::instance().GetItemRange().dwMin <= dwMin &&
			CClientManager::instance().GetItemRange().dwMax >= dwMax)
		{
			continue;
		}

		if (BuildRange(dwMin, dwMax, range) == true)
		{
			m_listData.emplace_back(range);
		}
	}

	// Also build 64-bit ranges above 2^32 if high item IDs exist in DB or active range
	char szQuery[256];
	snprintf(szQuery, sizeof(szQuery), "SELECT MAX(id) FROM item%s", GetTablePostfix());
	uint64_t ullGlobalMaxID = CClientManager::instance().GetItemRange().dwMax;
	auto pMsg = CDBManager::instance().DirectQuery(szQuery);
	if (pMsg && pMsg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		uint64_t dbMax = 0;
		if (row && row[0])
			str_to_number(dbMax, row[0]);
		if (dbMax > ullGlobalMaxID)
			ullGlobalMaxID = dbMax;
	}

	if (ullGlobalMaxID >= cs_dwDefaultPrebuildMaxItemID)
	{
		const uint64_t baseChunk = ullGlobalMaxID / cs_dwMinimumRange;
		for (uint64_t offset = 0; offset < 16; ++offset)
		{
			dwMin = cs_dwMinimumRange * (baseChunk + offset) + 1;
			dwMax = cs_dwMinimumRange * (baseChunk + offset + 1);
			if (dwMax >= cs_dwMaxItemID)
				break;
			if (CClientManager::instance().GetItemRange().dwMin <= dwMin &&
				CClientManager::instance().GetItemRange().dwMax >= dwMax)
			{
				continue;
			}
			if (BuildRange(dwMin, dwMax, range) == true)
			{
				m_listData.emplace_back(range);
			}
		}
	}
}

struct FCheckCollision
{
	bool hasCollision;
	TItemIDRangeTable range;

	FCheckCollision(TItemIDRangeTable data)
	{
		hasCollision = false;
		range = data;
	}

	void operator() (CPeer* peer)
	{
		if (hasCollision == false)
		{
			hasCollision = peer->CheckItemIDRangeCollision(range);
		}
	}
};

TItemIDRangeTable CItemIDRangeManager::GetRange()
{
	TItemIDRangeTable ret;
	ret.dwMin = 0;
	ret.dwMax = 0;
	ret.dwUsableItemIDMin = 0;

	if (m_listData.size() > 0)
	{
		while (m_listData.size() > 0)
		{
			ret = m_listData.front();
			m_listData.pop_front();

			FCheckCollision f(ret);
			f = CClientManager::instance().for_each_peer(f);

			if (f.hasCollision == false) return ret;
		}
	}

	for (int i = 0; i < 10; ++i)
		sys_err("ItemIDRange: NO MORE ITEM ID RANGE");

	return ret;
}

bool CItemIDRangeManager::BuildRange(uint64_t dwMin, uint64_t dwMax, TItemIDRangeTable& range)
{
	char szQuery[1024];
	uint64_t dwItemMaxID = 0;
	MYSQL_ROW row;

	snprintf(szQuery, sizeof(szQuery), "SELECT MAX(id) FROM item%s WHERE id >= %llu and id <= %llu",
			GetTablePostfix(), static_cast<unsigned long long>(dwMin), static_cast<unsigned long long>(dwMax));

	auto pMsg = CDBManager::instance().DirectQuery(szQuery);
	if (pMsg != NULL)
	{
		if (pMsg->Get()->uiNumRows > 0)
		{
			row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			str_to_number(dwItemMaxID, row[0]);
		}
	}

	const uint64_t dwCachedMaxID = CClientManager::instance().GetMaxCachedItemIDInRange(dwMin, dwMax);
	if (dwCachedMaxID > dwItemMaxID)
		dwItemMaxID = dwCachedMaxID;

	if (dwItemMaxID == 0)
		dwItemMaxID = dwMin;
	else
		dwItemMaxID++;

	if ((dwMax < dwItemMaxID) || (dwMax - dwItemMaxID < cs_dwMinimumRemainCount))
	{
		sys_log(0, "ItemIDRange: Build %llu ~ %llu start: %llu\tNOT USE remain count is below %llu",
					static_cast<unsigned long long>(dwMin), static_cast<unsigned long long>(dwMax),
				static_cast<unsigned long long>(dwItemMaxID), static_cast<unsigned long long>(cs_dwMinimumRemainCount));
	}
	else
	{
		range.dwMin = dwMin;
		range.dwMax = dwMax;
		range.dwUsableItemIDMin = dwItemMaxID;

		snprintf(szQuery, sizeof(szQuery), "SELECT COUNT(*) FROM item%s WHERE id >= %llu AND id <= %llu",
				GetTablePostfix(), static_cast<unsigned long long>(range.dwUsableItemIDMin), static_cast<unsigned long long>(range.dwMax));

		pMsg = CDBManager::instance().DirectQuery(szQuery);

		if (pMsg != NULL)
		{
			if (pMsg->Get()->uiNumRows > 0)
			{
				DWORD count = 0;
				row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				str_to_number(count, row[0]);

				if (count > 0)
				{
					sys_err("ItemIDRange: Build: %llu ~ %llu\thave a item",
							static_cast<unsigned long long>(range.dwUsableItemIDMin), static_cast<unsigned long long>(range.dwMax));
					return false;
				}
				else
				{
					sys_log(0, "ItemIDRange: Build: %llu ~ %llu start:%llu",
							static_cast<unsigned long long>(range.dwMin), static_cast<unsigned long long>(range.dwMax),
							static_cast<unsigned long long>(range.dwUsableItemIDMin));
					return true;
				}
			}
		}
	}

	return false;
}

void CItemIDRangeManager::UpdateRange(uint64_t dwMin, uint64_t dwMax)
{
	TItemIDRangeTable range;

	if (BuildRange(dwMin, dwMax, range) == true)
	{
		m_listData.emplace_back(range);
	}
}
