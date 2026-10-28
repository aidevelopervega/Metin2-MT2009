#include "stdafx.h"
#include "horsename_manager.h"
#include "desc_client.h"
#include "char_manager.h"
#include "char.h"
#include "affect.h"
#include "utils.h"

CHorseNameManager::CHorseNameManager()
{
	m_mapHorseNames.clear();
}

const char* CHorseNameManager::GetHorseName(uint64_t dwPlayerID)
{
	std::map<uint64_t, std::string>::iterator iter;

	iter = m_mapHorseNames.find(dwPlayerID);

	if ( iter != m_mapHorseNames.end() )
	{
		return iter->second.c_str();
	}
	else
	{
		return NULL;
	}
}

void CHorseNameManager::UpdateHorseName(uint64_t dwPlayerID, const char* szHorseName, bool broadcast)
{
	if ( szHorseName == NULL )
	{
		sys_err("HORSE_NAME: NULL NAME (%llu)", static_cast<unsigned long long>(dwPlayerID));
		szHorseName = "";
	}

	sys_log(0, "HORSENAME: update %llu %s", static_cast<unsigned long long>(dwPlayerID), szHorseName);

	m_mapHorseNames[dwPlayerID] = szHorseName;

	if ( broadcast == true )
	{
		BroadcastHorseName(dwPlayerID, szHorseName);
	}
}

void CHorseNameManager::BroadcastHorseName(uint64_t dwPlayerID, const char* szHorseName)
{
	TPacketUpdateHorseName packet;
	packet.dwPlayerID = dwPlayerID;
	strlcpy(packet.szHorseName, szHorseName, sizeof(packet.szHorseName));

	db_clientdesc->DBPacket(HEADER_GD_UPDATE_HORSE_NAME, 0, &packet, sizeof(TPacketUpdateHorseName));
}
