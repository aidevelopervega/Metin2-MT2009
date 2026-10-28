// vim:ts=4 sw=4
#include "stdafx.h"
#include "ClientManager.h"
#include "Main.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"
#include "GuildManager.h"

void CClientManager::GuildCreate([[maybe_unused]] CPeer * peer, uint64_t dwGuildID)
{
	sys_log(0, "GuildCreate %llu", static_cast<unsigned long long>(dwGuildID));
	ForwardPacket(HEADER_DG_GUILD_LOAD, &dwGuildID, sizeof(dwGuildID));

	CGuildManager::instance().Load(dwGuildID);
}

void CClientManager::GuildChangeGrade([[maybe_unused]] CPeer* peer, TPacketGuild* p)
{
	sys_log(0, "GuildChangeGrade %llu %llu", static_cast<unsigned long long>(p->dwGuild), static_cast<unsigned long long>(p->dwInfo));
	ForwardPacket(HEADER_DG_GUILD_CHANGE_GRADE, p, sizeof(TPacketGuild));
}

void CClientManager::GuildAddMember([[maybe_unused]] CPeer* peer, TPacketGDGuildAddMember * p)
{
	CGuildManager::instance().TouchGuild(p->dwGuild);
	sys_log(0, "GuildAddMember %llu %llu", static_cast<unsigned long long>(p->dwGuild), static_cast<unsigned long long>(p->dwPID));

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery),
			"INSERT INTO guild_member%s VALUES(%llu, %llu, %d, 0, 0)",
			GetTablePostfix(), static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwGuild), p->bGrade);

	auto pmsg_insert(CDBManager::instance().DirectQuery(szQuery));

	snprintf(szQuery, sizeof(szQuery),
			"SELECT pid, grade, is_general, offer, level, job, name FROM guild_member%s, player%s WHERE guild_id = %llu and pid = id and pid = %llu",
			GetTablePostfix(), GetTablePostfix(), static_cast<unsigned long long>(p->dwGuild), static_cast<unsigned long long>(p->dwPID));

	auto pmsg(CDBManager::instance().DirectQuery(szQuery));
	if (pmsg->Get()->uiNumRows == 0)
	{
		sys_err("Query failed when getting guild member data %s", pmsg->stQuery.c_str());
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);

	if (!row[0] || !row[1])
		return;

	TPacketDGGuildMember dg;

	dg.dwGuild = p->dwGuild;
	str_to_number(dg.dwPID, row[0]);
	str_to_number(dg.bGrade, row[1]);
	str_to_number(dg.isGeneral, row[2]);
	str_to_number(dg.dwOffer, row[3]);
	str_to_number(dg.bLevel, row[4]);
	str_to_number(dg.bJob, row[5]);
	strlcpy(dg.szName, row[6], sizeof(dg.szName));

	ForwardPacket(HEADER_DG_GUILD_ADD_MEMBER, &dg, sizeof(TPacketDGGuildMember));
}

void CClientManager::GuildRemoveMember([[maybe_unused]] CPeer* peer, TPacketGuild* p)
{
	sys_log(0, "GuildRemoveMember %llu %llu", static_cast<unsigned long long>(p->dwGuild), static_cast<unsigned long long>(p->dwInfo));

	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_member%s WHERE pid=%llu and guild_id=%llu",
			GetTablePostfix(), static_cast<unsigned long long>(p->dwInfo), static_cast<unsigned long long>(p->dwGuild));
	CDBManager::instance().AsyncQuery(szQuery);

	// @fixme202 new_+withdraw_time
	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO quest%s (dwPID, szName, szState, lValue) VALUES(%llu, 'guild_manage', 'new_withdraw_time', %u)",
			GetTablePostfix(), static_cast<unsigned long long>(p->dwInfo), static_cast<unsigned int>(GetCurrentTime()));
	CDBManager::instance().AsyncQuery(szQuery);

	ForwardPacket(HEADER_DG_GUILD_REMOVE_MEMBER, p, sizeof(TPacketGuild));
}

void CClientManager::GuildSkillUpdate([[maybe_unused]] CPeer* peer, TPacketGuildSkillUpdate* p)
{
	sys_log(0, "GuildSkillUpdate %d", p->amount);
	ForwardPacket(HEADER_DG_GUILD_SKILL_UPDATE, p, sizeof(TPacketGuildSkillUpdate));
}

void CClientManager::GuildExpUpdate(CPeer* peer, TPacketGuildExpUpdate* p)
{
	sys_log(0, "GuildExpUpdate %d", p->amount);
	ForwardPacket(HEADER_DG_GUILD_EXP_UPDATE, p, sizeof(TPacketGuildExpUpdate), 0, peer);
}

void CClientManager::GuildChangeMemberData(CPeer* peer, TPacketGuildChangeMemberData* p)
{
	sys_log(0, "GuildChangeMemberData %llu %u %d %d", static_cast<unsigned long long>(p->pid), p->offer, p->level, p->grade);
	ForwardPacket(HEADER_DG_GUILD_CHANGE_MEMBER_DATA, p, sizeof(TPacketGuildChangeMemberData), 0, peer);
}

void CClientManager::GuildDisband([[maybe_unused]] CPeer* peer, TPacketGuild* p)
{
	sys_log(0, "GuildDisband %llu", static_cast<unsigned long long>(p->dwGuild));

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild%s WHERE id=%llu", GetTablePostfix(), static_cast<unsigned long long>(p->dwGuild));
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_grade%s WHERE guild_id=%llu", GetTablePostfix(), static_cast<unsigned long long>(p->dwGuild));
	CDBManager::instance().AsyncQuery(szQuery);

	// @fixme401 (withdraw -> new_disband)_time
	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO quest%s (dwPID, szName, szState, lValue) SELECT pid, 'guild_manage', 'new_disband_time', %u FROM guild_member%s WHERE guild_id = %llu",
			GetTablePostfix(), static_cast<unsigned int>(GetCurrentTime()), GetTablePostfix(), static_cast<unsigned long long>(p->dwGuild));
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_member%s WHERE guild_id=%llu", GetTablePostfix(), static_cast<unsigned long long>(p->dwGuild));
	CDBManager::instance().AsyncQuery(szQuery);

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM guild_comment%s WHERE guild_id=%llu", GetTablePostfix(), static_cast<unsigned long long>(p->dwGuild));
	CDBManager::instance().AsyncQuery(szQuery);

	ForwardPacket(HEADER_DG_GUILD_DISBAND, p, sizeof(TPacketGuild));
}

const char* __GetWarType(int n)
{
	switch (n)
	{
		case 0 :
			return "Field";
		case 1 :
			return "Theater";
		case 2 :
			return "CTF"; //Capture The Flag
		default :
			return "Wrong number";
	}
}

void CClientManager::GuildWar([[maybe_unused]] CPeer* peer, TPacketGuildWar* p)
{
	switch (p->bWar)
	{
		case GUILD_WAR_SEND_DECLARE:
			sys_log(0, "GuildWar: GUILD_WAR_SEND_DECLARE type(%s) guild(%llu - %llu)",  __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().AddDeclare(p->bType, p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_REFUSE:
			sys_log(0, "GuildWar: GUILD_WAR_REFUSE type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);
			break;
			/*
			   case GUILD_WAR_WAIT_START:
			   CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);

			   if (!CGuildManager::instance().WaitStart(p))
			   p->bWar = GUILD_WAR_CANCEL;

			   break;
			   */

		case GUILD_WAR_WAIT_START:
			sys_log(0, "GuildWar: GUILD_WAR_WAIT_START type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			[[fallthrough]];
		case GUILD_WAR_RESERVE:
			if (p->bWar != GUILD_WAR_WAIT_START)
				sys_log(0, "GuildWar: GUILD_WAR_RESERVE type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);

			if (!CGuildManager::instance().ReserveWar(p))
				p->bWar = GUILD_WAR_CANCEL;
			else
				p->bWar = GUILD_WAR_RESERVE;

			break;

		case GUILD_WAR_ON_WAR:
			sys_log(0, "GuildWar: GUILD_WAR_ON_WAR type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().RemoveDeclare(p->dwGuildFrom, p->dwGuildTo);
			CGuildManager::instance().StartWar(p->bType, p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_OVER:
			sys_log(0, "GuildWar: GUILD_WAR_OVER type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().RecvWarOver(p->dwGuildFrom, p->dwGuildTo, p->bType, p->lWarPrice);
			break;

		case GUILD_WAR_END:
			sys_log(0, "GuildWar: GUILD_WAR_END type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().RecvWarEnd(p->dwGuildFrom, p->dwGuildTo);
			return;

		case GUILD_WAR_CANCEL :
			sys_log(0, "GuildWar: GUILD_WAR_CANCEL type(%s) guild(%llu - %llu)", __GetWarType(p->bType), static_cast<unsigned long long>(p->dwGuildFrom), static_cast<unsigned long long>(p->dwGuildTo));
			CGuildManager::instance().CancelWar(p->dwGuildFrom, p->dwGuildTo);
			break;
	}

	ForwardPacket(HEADER_DG_GUILD_WAR, p, sizeof(TPacketGuildWar));
}

void CClientManager::GuildWarScore([[maybe_unused]] CPeer* peer, TPacketGuildWarScore * p)
{
	CGuildManager::instance().UpdateScore(p->dwGuildGainPoint, p->dwGuildOpponent, p->lScore, p->lBetScore);
}

void CClientManager::GuildChangeLadderPoint(TPacketGuildLadderPoint* p)
{
	sys_log(0, "GuildChangeLadderPoint Recv %llu %d", static_cast<unsigned long long>(p->dwGuild), p->lChange);
	CGuildManager::instance().ChangeLadderPoint(p->dwGuild, p->lChange);
}

void CClientManager::GuildUseSkill(TPacketGuildUseSkill* p)
{
	sys_log(0, "GuildUseSkill Recv %llu %d", static_cast<unsigned long long>(p->dwGuild), p->dwSkillVnum);
	CGuildManager::instance().UseSkill(p->dwGuild, p->dwSkillVnum, p->dwCooltime);
	SendGuildSkillUsable(p->dwGuild, p->dwSkillVnum, false);
}

void CClientManager::SendGuildSkillUsable(uint64_t guild_id, DWORD dwSkillVnum, bool bUsable)
{
	sys_log(0, "SendGuildSkillUsable Send %llu %d %s", static_cast<unsigned long long>(guild_id), dwSkillVnum, bUsable?"true":"false");

	TPacketGuildSkillUsableChange p;

	p.dwGuild = guild_id;
	p.dwSkillVnum = dwSkillVnum;
	p.bUsable = bUsable;

	ForwardPacket(HEADER_DG_GUILD_SKILL_USABLE_CHANGE, &p, sizeof(TPacketGuildSkillUsableChange));
}

void CClientManager::GuildChangeMaster(TPacketChangeGuildMaster* p)
{
	if (CGuildManager::instance().ChangeMaster(p->dwGuildID, p->idFrom, p->idTo) == true)
	{
		TPacketChangeGuildMaster packet;
		packet.dwGuildID = p->dwGuildID;
		packet.idFrom = 0;
		packet.idTo = 0;

		ForwardPacket(HEADER_DG_ACK_CHANGE_GUILD_MASTER, &packet, sizeof(packet));
	}
}
