#include "stdafx.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "db.h"
#include "guild_manager.h"
#include "marriage.h"
#include "../../common/CommonDefines.h"

int CHARACTER::ChangeEmpire(BYTE empire)
{
	if (GetEmpire() == empire)
		return 1;

	char szQuery[1024+1];
	DWORD dwAID;
	DWORD dwPID[PLAYER_PER_ACCOUNT];
	memset(dwPID, 0, sizeof(dwPID));

	{
		snprintf(szQuery, sizeof(szQuery),
				"SELECT id, pid1, pid2, pid3, pid4"
				#ifdef ENABLE_PLAYER_PER_ACCOUNT5
				", pid5"
				#endif
				" FROM player_index%s WHERE (pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u"
				#ifdef ENABLE_PLAYER_PER_ACCOUNT5
				" OR pid5=%u"
				#endif
				") AND empire=%u",
				get_table_postfix(), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()),
				#ifdef ENABLE_PLAYER_PER_ACCOUNT5
				static_cast<unsigned int>(GetPlayerID()),
				#endif
				static_cast<unsigned int>(GetEmpire()));
		auto msg(DBManager::instance().DirectQuery(szQuery));
		if (msg->Get()->uiNumRows == 0)
		{
			return 0;
		}

		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		str_to_number(dwAID, row[0]);
		str_to_number(dwPID[0], row[1]);
		str_to_number(dwPID[1], row[2]);
		str_to_number(dwPID[2], row[3]);
		str_to_number(dwPID[3], row[4]);
		#ifdef ENABLE_PLAYER_PER_ACCOUNT5
		str_to_number(dwPID[4], row[5]);
		#endif
	}

	const int loop = PLAYER_PER_ACCOUNT;

	{
		DWORD dwGuildID[PLAYER_PER_ACCOUNT];
		CGuild * pGuild[PLAYER_PER_ACCOUNT];
		std::unique_ptr<SQLMsg> pMsg;

		for (int i = 0; i < loop; ++i)
		{
			snprintf(szQuery, sizeof(szQuery), "SELECT guild_id FROM guild_member%s WHERE pid=%u", get_table_postfix(), static_cast<unsigned int>(dwPID[i]));

			pMsg = DBManager::instance().DirectQuery(szQuery);

			if (pMsg != NULL)
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

					str_to_number(dwGuildID[i], row[0]);

					pGuild[i] = CGuildManager::instance().FindGuild(dwGuildID[i]);

					if (pGuild[i] != NULL)
						return 2;
				}
				else
				{
					dwGuildID[i] = 0;
					pGuild[i] = NULL;
				}
			}
		}
	}

	{
		for (int i = 0; i < loop; ++i)
		{
			if (marriage::CManager::instance().IsEngagedOrMarried(dwPID[i]) == true)
				return 3;
		}
	}

	{
		snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET empire=%u WHERE (pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u"
#ifdef ENABLE_PLAYER_PER_ACCOUNT5
				" OR pid5=%u)"
#else
				")"
#endif
				" AND empire=%u",
				get_table_postfix(), static_cast<unsigned int>(empire), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()),
#ifdef ENABLE_PLAYER_PER_ACCOUNT5
			static_cast<unsigned int>(GetPlayerID()),
#endif
			static_cast<unsigned int>(GetEmpire()));

		auto msg(DBManager::instance().DirectQuery(szQuery));
		if (msg->Get()->uiAffectedRows > 0)
		{
			SetChangeEmpireCount();

			return 999;
		}
	}

	return 0;
}

int CHARACTER::GetChangeEmpireCount() const
{
	char szQuery[1024+1];
	DWORD dwAID = GetAID();

	if (dwAID == 0)
		return 0;

	snprintf(szQuery, sizeof(szQuery), "SELECT change_count FROM change_empire WHERE aid = %u", static_cast<unsigned int>(dwAID));

	auto pMsg = DBManager::instance().DirectQuery(szQuery);
	if (pMsg != NULL)
	{
		if (pMsg->Get()->uiNumRows == 0)
			return 0;

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		DWORD count = 0;
		str_to_number(count, row[0]);
		return count;
	}

	return 0;
}

void CHARACTER::SetChangeEmpireCount()
{
	char szQuery[1024+1];

	DWORD dwAID = GetAID();

	if (dwAID == 0) return;

	int count = GetChangeEmpireCount();

	if (count == 0)
	{
		count++;
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO change_empire VALUES(%u, %d, NOW())", static_cast<unsigned int>(dwAID), count);
	}
	else
	{
		count++;
		snprintf(szQuery, sizeof(szQuery), "UPDATE change_empire SET change_count=%d WHERE aid=%u", count, static_cast<unsigned int>(dwAID));
	}

	auto pmsg(DBManager::instance().DirectQuery(szQuery));
}

DWORD CHARACTER::GetAID() const
{
	char szQuery[1024+1];
	DWORD dwAID = 0;
	snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player_index%s WHERE (pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u"
			#ifdef ENABLE_PLAYER_PER_ACCOUNT5
			" OR pid5=%u"
			#endif
			") AND empire=%u",
			get_table_postfix(), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()), static_cast<unsigned int>(GetPlayerID()),
			#ifdef ENABLE_PLAYER_PER_ACCOUNT5
			static_cast<unsigned int>(GetPlayerID()),
			#endif
			static_cast<unsigned int>(GetEmpire()));

	auto pMsg = DBManager::instance().DirectQuery(szQuery);
	if (pMsg != NULL)
	{
		if (pMsg->Get()->uiNumRows == 0)
			return 0;

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		str_to_number(dwAID, row[0]);
		return dwAID;
	}
	return 0;
}

