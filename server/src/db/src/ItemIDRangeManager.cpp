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
	const TItemIDRangeTable& dbRange = CClientManager::instance().GetItemRange();

	for (uint64_t i = 0; ; ++i)
	{
		dwMin = cs_dwMinimumRange * (i + 1) + 1;
		dwMax = cs_dwMinimumRange * (i + 2);

		if (dwMax >= cs_dwDefaultPrebuildMaxItemID)
			break;

		if (dbRange.dwMin > 0 && dbRange.dwMax > 0 &&
			dbRange.dwMin <= dwMax && dbRange.dwMax >= dwMin)
		{
			continue;
		}

		if (BuildRange(dwMin, dwMax, range) == true)
		{
			m_listData.emplace_back(range);
		}
	}

	// Observe any pre-existing records above cs_dwMaxItemID (> INT64_MAX range) without modifying or reusing them
	char szQuery[256];
	snprintf(szQuery, sizeof(szQuery), "SELECT COUNT(*), MIN(id), MAX(id) FROM item%s WHERE id > %llu",
		GetTablePostfix(), static_cast<unsigned long long>(cs_dwMaxItemID));
	if (auto pHighMsg = CDBManager::instance().DirectQuery(szQuery))
	{
		if (pHighMsg->Get()->uiNumRows > 0)
		{
			MYSQL_ROW row = mysql_fetch_row(pHighMsg->Get()->pSQLResult);
			uint64_t highCount = 0, highMin = 0, highMax = 0;
			if (row && row[0]) str_to_number(highCount, row[0]);
			if (row && row[1]) str_to_number(highMin, row[1]);
			if (row && row[2]) str_to_number(highMax, row[2]);
			if (highCount > 0)
			{
				sys_log(0, "ItemIDRange: Preserving %llu existing item record(s) above cs_dwMaxItemID (%llu ~ %llu); new allocations stay <= %llu",
					static_cast<unsigned long long>(highCount),
					static_cast<unsigned long long>(highMin),
					static_cast<unsigned long long>(highMax),
					static_cast<unsigned long long>(cs_dwMaxItemID));
			}
		}
	}

	// Build 64-bit ranges above 2^32 when high item IDs (<= cs_dwMaxItemID) exist or 32-bit pool has fewer than 16 ranges
	snprintf(szQuery, sizeof(szQuery), "SELECT MAX(id) FROM item%s WHERE id <= %llu",
		GetTablePostfix(), static_cast<unsigned long long>(cs_dwMaxItemID));
	uint64_t ullGlobalMaxID = (dbRange.dwMax <= cs_dwMaxItemID) ? dbRange.dwMax : 0;
	auto pMsg = CDBManager::instance().DirectQuery(szQuery);
	if (pMsg && pMsg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		uint64_t dbMax = 0;
		if (row && row[0])
			str_to_number(dbMax, row[0]);
		if (dbMax > ullGlobalMaxID && dbMax <= cs_dwMaxItemID)
			ullGlobalMaxID = dbMax;
	}

	if (ullGlobalMaxID >= cs_dwDefaultPrebuildMaxItemID || m_listData.size() < 16)
	{
		const uint64_t min64Chunk = cs_dwDefaultPrebuildMaxItemID / cs_dwMinimumRange;
		const uint64_t maxChunk = cs_dwMaxItemID / cs_dwMinimumRange;
		uint64_t baseChunk = ullGlobalMaxID / cs_dwMinimumRange;
		if (baseChunk < min64Chunk)
			baseChunk = min64Chunk;
		if (baseChunk >= maxChunk && maxChunk > 0)
			baseChunk = maxChunk - 1;

		size_t built64 = 0;
		for (uint64_t offset = 0; offset < 16 && built64 < 16; ++offset)
		{
			dwMin = cs_dwMinimumRange * (baseChunk + offset) + 1;
			dwMax = cs_dwMinimumRange * (baseChunk + offset + 1);
			if (dwMax > cs_dwMaxItemID)
				break;
			if (dbRange.dwMin > 0 && dbRange.dwMax > 0 &&
				dbRange.dwMin <= dwMax && dbRange.dwMax >= dwMin)
			{
				continue;
			}
			if (BuildRange(dwMin, dwMax, range) == true)
			{
				m_listData.emplace_back(range);
				++built64;
			}
		}

		// Fallback scan from min64Chunk if baseChunk was at/near cs_dwMaxItemID (e.g., sparse high test ID)
		if (built64 < 16 && baseChunk > min64Chunk)
		{
			for (uint64_t chunk = min64Chunk; chunk < baseChunk && built64 < 16 && (chunk - min64Chunk) < 64; ++chunk)
			{
				dwMin = cs_dwMinimumRange * chunk + 1;
				dwMax = cs_dwMinimumRange * (chunk + 1);
				if (dwMax > cs_dwMaxItemID)
					break;
				if (dbRange.dwMin > 0 && dbRange.dwMax > 0 &&
					dbRange.dwMin <= dwMax && dbRange.dwMax >= dwMin)
				{
					continue;
				}
				if (BuildRange(dwMin, dwMax, range) == true)
				{
					m_listData.emplace_back(range);
					++built64;
				}
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

	if (m_listData.empty())
		Build();

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
	if (dwMin == 0 || dwMax == 0 || dwMin >= dwMax || dwMax > cs_dwMaxItemID)
		return false;

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
			if (row && row[0])
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
				uint64_t count = 0;
				row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				if (row && row[0])
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
	const TItemIDRangeTable& dbRange = CClientManager::instance().GetItemRange();
	if (dbRange.dwMin > 0 && dbRange.dwMax > 0 &&
		dbRange.dwMin <= dwMax && dbRange.dwMax >= dwMin)
	{
		return;
	}

	if (BuildRange(dwMin, dwMax, range) == true)
	{
		m_listData.emplace_back(range);
	}
}
