// vim:ts=4 sw=4
#include "stdafx.h"
#include "ClientManager.h"

void CClientManager::UpdateHorseName(TPacketUpdateHorseName* data, CPeer* peer)
{
	if (!data || !peer)
		return;

	char rawName[CHARACTER_NAME_MAX_LEN + 1];
	strlcpy(rawName, data->szHorseName, sizeof(rawName));

	char szEscapedName[CHARACTER_NAME_MAX_LEN * 2 + 1];
	CDBManager::instance().EscapeString(szEscapedName, rawName, static_cast<unsigned long>(std::strlen(rawName)));

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO horse_name VALUES(%llu, '%s')", static_cast<unsigned long long>(data->dwPlayerID), szEscapedName);

	auto pmsg_insert(CDBManager::instance().DirectQuery(szQuery));

	ForwardPacket(HEADER_DG_UPDATE_HORSE_NAME, data, sizeof(TPacketUpdateHorseName), 0, peer);
}

void CClientManager::AckHorseName(uint64_t dwPID, CPeer* peer)
{
	if (!peer)
		return;

	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery), "SELECT name FROM horse_name WHERE id = %llu", static_cast<unsigned long long>(dwPID));

	auto pmsg(CDBManager::instance().DirectQuery(szQuery));

	TPacketUpdateHorseName packet{};
	packet.dwPlayerID = dwPID;

	if (!pmsg || pmsg->uiSQLErrno != 0 || !pmsg->Get() || pmsg->Get()->uiNumRows == 0)
	{
		memset(packet.szHorseName, 0, sizeof (packet.szHorseName));
	}
	else
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
		if (row && row[0])
			strlcpy(packet.szHorseName, row[0], sizeof(packet.szHorseName));
		else
			memset(packet.szHorseName, 0, sizeof(packet.szHorseName));
	}

	peer->EncodeHeader(HEADER_DG_ACK_HORSE_NAME, 0, sizeof(TPacketUpdateHorseName));
	peer->Encode(&packet, sizeof(TPacketUpdateHorseName));
}
