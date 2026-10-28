#include "stdafx.h"
#include "desc_client.h"
#include "item_manager.h"

int touch(const char *path)
{
	FILE	*fp;

	if ( !(fp = fopen(path, "a")) )
	{
		sys_err("touch failed");
		return (-1);
	}

	fclose(fp);
	return 0;
}

uint64_t ITEM_MANAGER::GetNewID()
{
	assert(m_dwCurrentID != 0);

	if ( m_dwCurrentID >= m_ItemIDRange.dwMax )
	{
		if ( m_ItemIDSpareRange.dwMin == 0 || m_ItemIDSpareRange.dwMax == 0 || m_ItemIDSpareRange.dwUsableItemIDMin == 0 )
		{
			for ( int i=0; i < 10; i++ ) sys_err("ItemIDRange: FATAL ERROR!!! no more item id");
			touch(".killscript");
			thecore_shutdown();
			return 0;
		}
		else
		{
			sys_log(0, "ItemIDRange: First Range is full. Change to SpareRange %llu ~ %llu %llu",
					static_cast<unsigned long long>(m_ItemIDSpareRange.dwMin),
					static_cast<unsigned long long>(m_ItemIDSpareRange.dwMax),
					static_cast<unsigned long long>(m_ItemIDSpareRange.dwUsableItemIDMin));

			db_clientdesc->DBPacket(HEADER_GD_REQ_SPARE_ITEM_ID_RANGE, 0, &m_ItemIDRange.dwMax, sizeof(uint64_t));

			SetMaxItemID(m_ItemIDSpareRange);

			m_ItemIDSpareRange.dwMin = 0;
			m_ItemIDSpareRange.dwMax = 0;
			m_ItemIDSpareRange.dwUsableItemIDMin = 0;
		}
	}

	return (m_dwCurrentID++);
}

bool ITEM_MANAGER::SetMaxItemID(TItemIDRangeTable range)
{
	m_ItemIDRange = range;

	if ( m_ItemIDRange.dwMin == 0 || m_ItemIDRange.dwMax == 0 || m_ItemIDRange.dwUsableItemIDMin == 0 )
	{
		for ( int i=0; i < 10; i++ ) sys_err("ItemIDRange: FATAL ERROR!!! ITEM ID RANGE is not set.");
		touch(".killscript");
		thecore_shutdown();
		return false;
	}

	m_dwCurrentID = range.dwUsableItemIDMin;

	sys_log(0, "ItemIDRange: %llu ~ %llu %llu",
			static_cast<unsigned long long>(m_ItemIDRange.dwMin),
			static_cast<unsigned long long>(m_ItemIDRange.dwMax),
			static_cast<unsigned long long>(m_dwCurrentID));

	return true;
}

bool ITEM_MANAGER::SetMaxSpareItemID(TItemIDRangeTable range)
{
	if ( range.dwMin == 0 || range.dwMax == 0 || range.dwUsableItemIDMin == 0 )
	{
		for ( int i=0; i < 10; i++ ) sys_err("ItemIDRange: FATAL ERROR!!! Spare ITEM ID RANGE is not set");
		return false;
	}

	m_ItemIDSpareRange = range;

	sys_log(0, "ItemIDRange: New Spare ItemID Range Recv %llu ~ %llu %llu",
			static_cast<unsigned long long>(m_ItemIDSpareRange.dwMin),
			static_cast<unsigned long long>(m_ItemIDSpareRange.dwMax),
			static_cast<unsigned long long>(m_ItemIDSpareRange.dwUsableItemIDMin));

	return true;
}
