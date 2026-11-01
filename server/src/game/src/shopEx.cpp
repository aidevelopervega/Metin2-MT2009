#include "stdafx.h"
#if __has_include(<grid.h>)
#include <grid.h>
#elif __has_include("../../../../server/src/libgame/include/grid.h")
#include "../../../../server/src/libgame/include/grid.h"
#elif __has_include("../../libgame/include/grid.h")
#include "../../libgame/include/grid.h"
#else
#include "grid.h"
#endif
#include "constants.h"
#include "utils.h"
#include "config.h"
#include "shop.h"
#include "desc.h"
#include "desc_manager.h"
#include "char.h"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "log.h"
#include "db.h"
#include "questmanager.h"
#include "monarch.h"
#include "mob_manager.h"
#include "locale_service.h"
#include "desc_client.h"
#include "shopEx.h"
#include "group_text_parse_tree.h"

static_assert(SHOP_TAB_COUNT_MAX == 3, "SHOP_TAB_COUNT_MAX must be 3");
static_assert(SHOP_PLAYER_HOST_ITEM_MAX_NUM == 80, "SHOP_PLAYER_HOST_ITEM_MAX_NUM must be 80");
static_assert(sizeof(TPacketGCShopStartEx::TSubPacketShopTab{}.name) == SHOP_TAB_NAME_MAX + 1, "TSubPacketShopTab::name size mismatch");
#ifndef ENABLE_CHEQUE_SYSTEM
static_assert(sizeof(TPacketGCShopStartEx::TSubPacketShopTab) == 4034, "TSubPacketShopTab wire size must be 4034 bytes");
#endif

bool CShopEx::Create(DWORD dwVnum, DWORD dwNPCVnum)
{
	m_dwVnum = dwVnum;
	m_dwNPCVnum = dwNPCVnum;
	return true;
}

bool CShopEx::AddShopTable(TShopTableEx& shopTable)
{
	if (m_vec_shopTabs.size() >= SHOP_TAB_COUNT_MAX)
	{
		sys_err("CShopEx::AddShopTable : tab count exceeds max (%d)", SHOP_TAB_COUNT_MAX);
		return false;
	}

	for (itertype(m_vec_shopTabs) it = m_vec_shopTabs.begin(); it != m_vec_shopTabs.end(); it++)
	{
		const TShopTableEx& _shopTable = *it;
		if (0 != _shopTable.dwVnum && _shopTable.dwVnum == shopTable.dwVnum)
			return false;
		if (0 != _shopTable.dwNPCVnum && _shopTable.dwNPCVnum == shopTable.dwVnum)
			return false;
	}
	m_vec_shopTabs.emplace_back(shopTable);
	return true;
}

bool CShopEx::AddGuest(LPCHARACTER ch,DWORD owner_vid, bool bOtherEmpire)
{
	if (!ch || !ch->GetDesc())
		return false;

	if (ch->GetExchange())
		return false;

	if (ch->GetShop())
		return false;

	if (m_vec_shopTabs.empty() || m_vec_shopTabs.size() > SHOP_TAB_COUNT_MAX)
	{
		sys_err("CShopEx::AddGuest : invalid shop tab count (%zu)", m_vec_shopTabs.size());
		return false;
	}

	TPacketGCShop pack{};
	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_START_EX;

	TPacketGCShopStartEx pack2{};
	pack2.owner_vid = owner_vid;
	pack2.shop_tab_count = static_cast<BYTE>(m_vec_shopTabs.size());

	constexpr size_t kShopExTabBufferCapacity = sizeof(TPacketGCShopStartEx::TSubPacketShopTab) * SHOP_TAB_COUNT_MAX;
	char temp[kShopExTabBufferCapacity]{};
	char* buf = &temp[0];
	size_t size = 0;

	for (itertype(m_vec_shopTabs) it = m_vec_shopTabs.begin(); it != m_vec_shopTabs.end(); it++)
	{
		const TShopTableEx& shop_tab = *it;
		TPacketGCShopStartEx::TSubPacketShopTab pack_tab{};
		pack_tab.coin_type = shop_tab.coinType;

		const size_t nameLen = std::min<size_t>(shop_tab.name.size(), SHOP_TAB_NAME_MAX);
		if (nameLen > 0)
			memcpy(pack_tab.name, shop_tab.name.data(), nameLen);
		pack_tab.name[nameLen] = '\0';

		for (BYTE i = 0; i < SHOP_HOST_ITEM_MAX_NUM; i++)
		{
			pack_tab.items[i].vnum = shop_tab.items[i].vnum;
			pack_tab.items[i].count = shop_tab.items[i].count;
			switch(shop_tab.coinType)
			{
			case SHOP_COIN_TYPE_GOLD:
#ifdef ENABLE_NEWSTUFF
				if (bOtherEmpire && !g_bEmpireShopPriceTripleDisable) // no empire price penalty for pc shop
#else
				if (bOtherEmpire) // no empire price penalty for pc shop
#endif
					pack_tab.items[i].price = shop_tab.items[i].price * 3;
				else
					pack_tab.items[i].price = shop_tab.items[i].price;
				#ifdef ENABLE_CHEQUE_SYSTEM
				pack_tab.items[i].cheque = shop_tab.items[i].cheque;
				#endif
				break;
			case SHOP_COIN_TYPE_SECONDARY_COIN:
				pack_tab.items[i].price = shop_tab.items[i].price;
				break;
			}
			memset(pack_tab.items[i].aAttr, 0, sizeof(pack_tab.items[i].aAttr));
			memset(pack_tab.items[i].alSockets, 0, sizeof(pack_tab.items[i].alSockets));
		}

		if (size + sizeof(pack_tab) > sizeof(temp))
		{
			sys_err("CShopEx::AddGuest : tab buffer overflow prevented (size=%zu, tab_size=%zu, cap=%zu)",
				size, sizeof(pack_tab), sizeof(temp));
			return false;
		}

		memcpy(buf, &pack_tab, sizeof(pack_tab));
		buf += sizeof(pack_tab);
		size += sizeof(pack_tab);
	}

	constexpr size_t kHeaderSize = sizeof(pack) + sizeof(pack2);
	if (kHeaderSize + size > std::numeric_limits<WORD>::max())
	{
		sys_err("CShopEx::AddGuest : packet size overflow (%zu)", kHeaderSize + size);
		return false;
	}

	ch->SetShop(this);
	m_map_guest.emplace(ch, bOtherEmpire);

	pack.size = static_cast<WORD>(kHeaderSize + size);

	ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCShop));
	ch->GetDesc()->BufferedPacket(&pack2, sizeof(TPacketGCShopStartEx));
	ch->GetDesc()->Packet(temp, size);

	return true;
}

int CShopEx::Buy(LPCHARACTER ch, BYTE pos)
{
	BYTE tabIdx = pos / SHOP_PLAYER_HOST_ITEM_MAX_NUM;
	BYTE slotPos = pos % SHOP_PLAYER_HOST_ITEM_MAX_NUM;
	if (tabIdx >= GetTabCount() || slotPos >= SHOP_HOST_ITEM_MAX_NUM)
	{
		sys_log(0, "ShopEx::Buy : invalid position %d : %s", pos, ch->GetName());
		return SHOP_SUBHEADER_GC_INVALID_POS;
	}

	sys_log(0, "ShopEx::Buy : name %s pos %d", ch->GetName(), pos);

	GuestMapType::iterator it = m_map_guest.find(ch);

	if (it == m_map_guest.end())
		return SHOP_SUBHEADER_GC_END;

	TShopTableEx& shopTab = m_vec_shopTabs[tabIdx];
	TShopItemTable& r_item = shopTab.items[slotPos];

	if (r_item.price <= 0 || r_item.price >= GOLD_MAX)
	{
		LogManager::instance().HackLog("SHOP_BUY_GOLD_OVERFLOW", ch);
		return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
	}

	YANG dwPrice = r_item.price;

	switch (shopTab.coinType)
	{
	case SHOP_COIN_TYPE_GOLD:
#ifdef ENABLE_NEWSTUFF
		if (it->second && !g_bEmpireShopPriceTripleDisable)	// if other empire, price is triple
#else
		if (it->second)	// if other empire, price is triple
#endif
		{
			if (dwPrice > (GOLD_MAX - 1) / 3)
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
			dwPrice *= 3;
		}

		if (ch->GetGold() < dwPrice)
		{
			sys_log(1, "ShopEx::Buy : Not enough money : %s has %lld, price %lld",
				ch->GetName(), static_cast<long long>(ch->GetGold()), static_cast<long long>(dwPrice));
			return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
		}
		break;
	case SHOP_COIN_TYPE_SECONDARY_COIN:
		{
			if (dwPrice > static_cast<YANG>(std::numeric_limits<ITEM_COUNT>::max()))
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX;
			ITEM_COUNT count = ch->CountSpecifyTypeItem(ITEM_SECONDARY_COIN);
			if (static_cast<YANG>(count) < dwPrice)
			{
				sys_log(1, "ShopEx::Buy : Not enough myeongdojun : %s has %u, price %lld",
					ch->GetName(), static_cast<unsigned int>(count), static_cast<long long>(dwPrice));
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX;
			}
		}
		break;
	}

	LPITEM item;

	item = ITEM_MANAGER::instance().CreateItem(r_item.vnum, r_item.count);

	if (!item)
		return SHOP_SUBHEADER_GC_SOLD_OUT;

	int iEmptyPos = ch->GetEmptyInventoryEx(item);
	if (iEmptyPos < 0)
	{
		sys_log(1, "ShopEx::Buy : Inventory full : %s size %d", ch->GetName(), item->GetSize());
		M2_DESTROY_ITEM(item);
		return SHOP_SUBHEADER_GC_INVENTORY_FULL;
	}

	switch (shopTab.coinType)
	{
	case SHOP_COIN_TYPE_GOLD:
		ch->ChangeGold(-dwPrice);
		break;
	case SHOP_COIN_TYPE_SECONDARY_COIN:
		ch->RemoveSpecifyTypeItem(ITEM_SECONDARY_COIN, static_cast<ITEM_COUNT>(dwPrice));
		break;
	}

	item->AddToCharacter(ch, TItemPos(item->GetWindowInventoryEx(), iEmptyPos));

	ITEM_MANAGER::instance().FlushDelayedSave(item);
	LogManager::instance().ItemLog(ch, item, "BUY", item->GetName());

	if (item->GetVnum() >= 80003 && item->GetVnum() <= 80007)
	{
		LogManager::instance().GoldBarLog(ch->GetPlayerID(), item->GetID(), PERSONAL_SHOP_BUY, "");
	}

	DBManager::instance().SendMoneyLog(MONEY_LOG_SHOP, item->GetVnum(), -dwPrice);

	if (item)
		sys_log(0, "ShopEx: BUY: name %s %s(x %u):%llu price %lld",
			ch->GetName(), item->GetName(),
			static_cast<unsigned int>(item->GetCount()), static_cast<unsigned long long>(item->GetID()),
			static_cast<long long>(dwPrice));

	ch->Save();

	return (SHOP_SUBHEADER_GC_OK);
}
