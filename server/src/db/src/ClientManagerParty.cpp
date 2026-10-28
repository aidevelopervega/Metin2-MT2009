// vim:ts=4 sw=4
#include "stdafx.h"
#include "ClientManager.h"
#include "Config.h"
#include "DBManager.h"
#include "QID.h"

void CClientManager::QUERY_PARTY_CREATE(CPeer* peer, TPacketPartyCreate* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];

	if (pm.find(p->dwLeaderPID) == pm.end())
	{
		pm.emplace(p->dwLeaderPID, TPartyMember());
		ForwardPacket(HEADER_DG_PARTY_CREATE, p, sizeof(TPacketPartyCreate), peer->GetChannel(), peer);
		sys_log(0, "PARTY Create [%llu]", static_cast<unsigned long long>(p->dwLeaderPID));
	}
	else
	{
		sys_err("PARTY Create - Already exists [%llu]", static_cast<unsigned long long>(p->dwLeaderPID));
	}
}

void CClientManager::QUERY_PARTY_DELETE(CPeer* peer, TPacketPartyDelete* p)
{
	TPartyMap& pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY Delete - Non exists [%llu]", static_cast<unsigned long long>(p->dwLeaderPID));
		return;
	}

	pm.erase(it);
	ForwardPacket(HEADER_DG_PARTY_DELETE, p, sizeof(TPacketPartyDelete), peer->GetChannel(), peer);
	sys_log(0, "PARTY Delete [%llu]", static_cast<unsigned long long>(p->dwLeaderPID));
}

void CClientManager::QUERY_PARTY_ADD(CPeer* peer, TPacketPartyAdd* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY Add - Non exists [%llu]", static_cast<unsigned long long>(p->dwLeaderPID));
		return;
	}

	if (it->second.find(p->dwPID) == it->second.end())
	{
		it->second.emplace(p->dwPID, TPartyInfo());
		ForwardPacket(HEADER_DG_PARTY_ADD, p, sizeof(TPacketPartyAdd), peer->GetChannel(), peer);
		sys_log(0, "PARTY Add [%llu] to [%llu]", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID));
	}
	else
		sys_err("PARTY Add - Already [%llu] in party [%llu]", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID));
}

void CClientManager::QUERY_PARTY_REMOVE(CPeer* peer, TPacketPartyRemove* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY Remove - Non exists [%llu] cannot remove [%llu]", static_cast<unsigned long long>(p->dwLeaderPID), static_cast<unsigned long long>(p->dwPID));
		return;
	}

	itertype(it->second) pit = it->second.find(p->dwPID);

	if (pit != it->second.end())
	{
		it->second.erase(pit);
		ForwardPacket(HEADER_DG_PARTY_REMOVE, p, sizeof(TPacketPartyRemove), peer->GetChannel(), peer);
		sys_log(0, "PARTY Remove [%llu] to [%llu]", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID));
	}
	else
		sys_err("PARTY Remove - Cannot find [%llu] in party [%llu]", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID));
}

void CClientManager::QUERY_PARTY_STATE_CHANGE(CPeer* peer, TPacketPartyStateChange* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY StateChange - Non exists [%llu] cannot state change [%llu]", static_cast<unsigned long long>(p->dwLeaderPID), static_cast<unsigned long long>(p->dwPID));
		return;
	}

	itertype(it->second) pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		sys_err("PARTY StateChange - Cannot find [%llu] in party [%llu]", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID));
		return;
	}

	if (p->bFlag)
		pit->second.bRole = p->bRole;
	else
		pit->second.bRole = 0;

	ForwardPacket(HEADER_DG_PARTY_STATE_CHANGE, p, sizeof(TPacketPartyStateChange), peer->GetChannel(), peer);
	sys_log(0, "PARTY StateChange [%llu] at [%llu] from %d %d", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID), p->bRole, p->bFlag);
}

void CClientManager::QUERY_PARTY_SET_MEMBER_LEVEL(CPeer* peer, TPacketPartySetMemberLevel* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	itertype(pm) it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY SetMemberLevel - Non exists [%llu] cannot level change [%u] [%llu]", static_cast<unsigned long long>(p->dwLeaderPID), p->bLevel, static_cast<unsigned long long>(p->dwPID));
		return;
	}

	itertype(it->second) pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		sys_err("PARTY SetMemberLevel - Cannot find [%llu] in party [%llu]", static_cast<unsigned long long>(p->dwPID), static_cast<unsigned long long>(p->dwLeaderPID));
		return;
	}

	pit->second.bLevel = p->bLevel;

	ForwardPacket(HEADER_DG_PARTY_SET_MEMBER_LEVEL, p, sizeof(TPacketPartySetMemberLevel), peer->GetChannel());
	sys_log(0, "PARTY SetMemberLevel pid [%llu] level %d", static_cast<unsigned long long>(p->dwPID), p->bLevel);
}
