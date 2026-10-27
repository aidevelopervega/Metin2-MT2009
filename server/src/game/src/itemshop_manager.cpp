#include "stdafx.h"
#include "config.h"
#include "utils.h"
#include "constants.h"
#include "locale_service.h"
#include "text_file_loader.h"
#include "itemshop_manager.h"
#include "item_manager.h"
#include "questmanager.h"
#include "char.h"
#include "item.h"
#include "log.h"
#include "desc.h"
#include "db.h"
#include "packet.h"
#include "desc_client.h"
#include "buffer_manager.h"
#include "p2p.h"
#include "char_manager.h"
#include "../../common/PulseManager.h"

CItemShopManager::CItemShopManager()
{
}

CItemShopManager::~CItemShopManager()
{
	Destroy();
}

bool CItemShopManager::Initialize(TItemShopItem* pTable, int iSize)
{
	m_map_ItemShopItems.clear();
	m_map_ItemShopTimeAuctions.clear();
	m_map_AuctionViewer.clear();

	TItemShopItem* t = pTable;

	for (int i = 0; i < iSize; ++i, ++t)
	{
		sys_log(0, "mamy item %d o vnumie %d promotionPrice %lld", t->dwIndex, t->dwVnum, static_cast<long long>(t->promotion.dwPrice));
		TItemShopItem item;
		thecore_memcpy(&item, t, sizeof(TItemShopItem));

		if (IsItemTimeAuction(item))
		{
			m_map_ItemShopTimeAuctions.insert(std::make_pair(item.dwIndex, item));
		}
		else
		{
			m_map_ItemShopItems.insert(std::make_pair(item.dwIndex, item));
		}
	}

	// Refresh auction buy amount
	std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().DirectQuery("SELECT item_index, buy_count FROM itemshop_time_auction"));
	if (pkMsg && pkMsg->uiSQLErrno == 0 && pkMsg->Get() && pkMsg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(pkMsg->Get()->pSQLResult)) != NULL)
		{
			int col = 0;
			DWORD item_index = 0;
			DWORD buy_count = 0;
			str_to_number(item_index, row[col++]);
			str_to_number(buy_count, row[col++]);
			UpdateAuctionBuyAmount(item_index, buy_count);
		}
	}

	if (EnsureTransactionJournalSchema())
	{
		const int recovered = RecoverIncompleteTransactions();
		if (recovered > 0)
		{
			sys_log(0, "ItemShop: Recovered %d incomplete transaction(s) from itemshop_tx_journal", recovered);
		}
	}

	return true;
}

void CItemShopManager::Destroy()
{
	m_map_ItemShopItems.clear();
	m_map_ItemShopTimeAuctions.clear();
}

bool CItemShopManager::IsItemExist(int index)
{
	if (index < 0)
		return false;

	auto it = m_map_ItemShopItems.find(static_cast<DWORD>(index));
	return it != m_map_ItemShopItems.end();
}

bool CItemShopManager::IsAuctionExist(int index)
{
	if (index < 0)
		return false;

	auto it = m_map_ItemShopTimeAuctions.find(static_cast<DWORD>(index));
	return it != m_map_ItemShopTimeAuctions.end();
}

TItemShopItem CItemShopManager::GetItem(int index)
{
	if (index < 0)
		return TItemShopItem{};

	const DWORD dwIndex = static_cast<DWORD>(index);
	if (IsItemExist(index))
		return m_map_ItemShopItems[dwIndex];
	else if (IsAuctionExist(index)) {
		return m_map_ItemShopTimeAuctions[dwIndex];
	}
	return TItemShopItem{};
}

int64_t CItemShopManager::GetItemPrice(TItemShopItem item)
{
	if (IsProperItem(item))
	{
		if (IsItemPromotion(item))
			return item.promotion.dwPrice;

		return item.dwPrice;
	}
	return 0;
}

BYTE CItemShopManager::GetItemCurrency(TItemShopItem item)
{
	if (IsProperItem(item))
		return item.bCurrency;
	return 0;
}

bool CItemShopManager::IsItemPromotion(TItemShopItem item)
{
	if (IsProperItem(item))
	{
		int64_t start_time = item.promotion.dwStartTime;
		int64_t end_time = item.promotion.dwEndTime;
		int64_t current_time = get_global_time();
		return current_time >= start_time && current_time < end_time;
	}
	return false;
}

bool CItemShopManager::IsItemTimeAuction(TItemShopItem item)
{
	if (IsProperItem(item))
	{
		int64_t start_time = item.timeAuction.dwStartTime;
		int64_t end_time = item.timeAuction.dwEndTime;
		return start_time > 0 && end_time > 0;
	}
	return false;
}

bool CItemShopManager::IsTimeAuctionActive(TItemShopItem item)
{
	if (IsProperItem(item))
	{
		int64_t start_time = item.timeAuction.dwStartTime;
		int64_t end_time = item.timeAuction.dwEndTime;
		int64_t current_time = get_global_time();
		return current_time >= start_time && current_time < end_time;
	}
	return false;
}

bool CItemShopManager::CheckIfCanBuyTimeAuctionAndUpdate(TItemShopItem item, LPCHARACTER buyer, DWORD quantity)
{
	if (!buyer)
		return false;

	if (!buyer->GetDesc())
		return false;

	if (IsItemTimeAuction(item))
	{
		if (!IsTimeAuctionActive(item))
			return false;

		const uint64_t item_count_u64 = static_cast<uint64_t>(item.dwCount) * static_cast<uint64_t>(quantity);
		if (item.dwCount == 0 || item_count_u64 == 0 || item_count_u64 > static_cast<uint64_t>(INT32_MAX))
			return false;
		const DWORD item_count = static_cast<DWORD>(item_count_u64);

		// account limit start (enforced for both finite and infinite global stock auctions)
		const DWORD MAX_BUY_PER_ACCOUNT = item.timeAuction.dwAccountLimit;
		std::string auction_buy_flag;
		std::string auction_buy_time_flag;
		long long buyer_buy_count = 0;
		if (MAX_BUY_PER_ACCOUNT > 0)
		{
			auction_buy_flag = (item.dwIndex < 100000U)
				? ("itemshop_auction_buy_" + std::to_string(item.dwIndex))
				: ("is_auc_buy_" + std::to_string(item.dwIndex));
			auction_buy_time_flag = (item.dwIndex < 100000U)
				? ("itemshop_auction_time_" + std::to_string(item.dwIndex))
				: ("is_auc_time_" + std::to_string(item.dwIndex));
			buyer_buy_count = buyer->GetAccountFlag(auction_buy_flag);
			const long long buyer_buy_time = buyer->GetAccountFlag(auction_buy_time_flag);
			if (buyer_buy_count < 0 || buyer_buy_time <= 0 || static_cast<long long>(get_global_time()) > buyer_buy_time)
			{
				buyer_buy_count = 0;
			}

			if (static_cast<uint64_t>(buyer_buy_count) >= static_cast<uint64_t>(MAX_BUY_PER_ACCOUNT))
			{
				buyer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_TIME_AUCTION_ACCOUNT_LIMIT"), (int)item.timeAuction.dwAccountLimit);
				return false;
			}

			if (static_cast<uint64_t>(buyer_buy_count) + item_count_u64 > static_cast<uint64_t>(MAX_BUY_PER_ACCOUNT))
			{
				buyer->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_TIME_AUCTION_PURCHASE_WOULD_EXCEED_LIMIT"));
				return false;
			}
		}
		// account limit end

		bool isInfinite = item.timeAuction.dwMaxBuyAmount == 0;
		if (!isInfinite) {
			if (item.timeAuction.dwCurrentBuyAmount >= item.timeAuction.dwMaxBuyAmount)
				return false;

			if (item_count_u64 > static_cast<uint64_t>(item.timeAuction.dwMaxBuyAmount))
				return false;

			std::unique_ptr<SQLMsg> pkUpdateMsg(DBManager::instance().DirectQuery(
				"UPDATE itemshop_time_auction SET buy_count = buy_count + %u "
				"WHERE item_index = %u AND buy_count + %u <= %u",
				static_cast<unsigned int>(item_count),
				static_cast<unsigned int>(item.dwIndex),
				static_cast<unsigned int>(item_count),
				static_cast<unsigned int>(item.timeAuction.dwMaxBuyAmount)));
			if (!pkUpdateMsg || pkUpdateMsg->uiSQLErrno != 0 || !pkUpdateMsg->Get() || pkUpdateMsg->Get()->uiAffectedRows != 1)
				return false;

			DWORD new_buy_count = item.timeAuction.dwCurrentBuyAmount + item_count;
			std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().DirectQuery(
				"SELECT buy_count FROM itemshop_time_auction WHERE item_index=%u",
				static_cast<unsigned int>(item.dwIndex)));
			if (pkMsg && pkMsg->uiSQLErrno == 0 && pkMsg->Get() && pkMsg->Get()->uiNumRows >= 1)
			{
				MYSQL_ROW row = mysql_fetch_row(pkMsg->Get()->pSQLResult);
				if (row && row[0])
					str_to_number(new_buy_count, row[0]);
			}

			DWORD current_progress = GetTimeAuctionProgress(item);
			item.timeAuction.dwCurrentBuyAmount = new_buy_count;
			DWORD new_progress = GetTimeAuctionProgress(item);
			buyer->ChatDebug("%d != %d", current_progress, new_progress);
			if (current_progress != new_progress)
			{
				buyer->ChatDebug("wysylam!");
				TPacketGGItemShopAuctionUpdate p;
				p.bHeader = HEADER_GG_ITEMSHOP_AUCTION_UPDATE;
				p.dwItemIndex = item.dwIndex;
				p.dwBuyAmount = item.timeAuction.dwCurrentBuyAmount;

				P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGItemShopAuctionUpdate));
				UpdateAuctionBuyAmount(item.dwIndex, item.timeAuction.dwCurrentBuyAmount);
			}
		}

		if (MAX_BUY_PER_ACCOUNT > 0)
		{
			const long long updatedCount = buyer_buy_count + static_cast<long long>(item_count);
			const long long updatedEndTime = static_cast<long long>(item.timeAuction.dwEndTime);
			buyer->SetAccountFlag(auction_buy_flag, updatedCount);
			buyer->SetAccountFlag(auction_buy_time_flag, updatedEndTime);
			if (buyer->HasPlayerData())
			{
				buyer->playerData->SaveSpecialFlag(buyer->GetPlayerID(), buyer->GetAID());
			}
		}

		return true;
	}

	return false;
}

void CItemShopManager::UpdateAuctionBuyAmount(DWORD dwItemIndex, DWORD dwBuyAmount)
{
	if (IsAuctionExist(static_cast<int>(dwItemIndex))) {
		auto item = m_map_ItemShopTimeAuctions[dwItemIndex];
		item.timeAuction.dwCurrentBuyAmount = dwBuyAmount;
		m_map_ItemShopTimeAuctions[dwItemIndex] = item;
		RefreshAuctionViewers(item.dwIndex);
	}
}

bool CItemShopManager::CanSendItem(LPCHARACTER buyer, TItemShopItem item)
{
	TItemTable* pkItemTable = ITEM_MANAGER::instance().GetTable(item.dwVnum);
	switch (buyer->GetRaceNum())
	{
	case MAIN_RACE_WARRIOR_M:
	case MAIN_RACE_ASSASSIN_M:
	case MAIN_RACE_SURA_M:
	case MAIN_RACE_SHAMAN_M:
	{
		if (IS_SET(pkItemTable->dwAntiFlags, ITEM_ANTIFLAG_MALE))
			return false;
	}
	break;
	case MAIN_RACE_WARRIOR_W:
	case MAIN_RACE_ASSASSIN_W:
	case MAIN_RACE_SURA_W:
	case MAIN_RACE_SHAMAN_W:
	{
		if (IS_SET(pkItemTable->dwAntiFlags, ITEM_ANTIFLAG_FEMALE))
			return false;
	}
	break;
	}

	if (buyer->GetJob() == JOB_WARRIOR && IS_SET(pkItemTable->dwAntiFlags, ITEM_ANTIFLAG_WARRIOR))
		return false;
	else if (buyer->GetJob() == JOB_ASSASSIN && IS_SET(pkItemTable->dwAntiFlags, ITEM_ANTIFLAG_ASSASSIN))
		return false;
	else if (buyer->GetJob() == JOB_SURA && IS_SET(pkItemTable->dwAntiFlags, ITEM_ANTIFLAG_SURA))
		return false;
	else if (buyer->GetJob() == JOB_SHAMAN && IS_SET(pkItemTable->dwAntiFlags, ITEM_ANTIFLAG_SHAMAN))
		return false;

	return true;
}

bool CItemShopManager::CanSendTimeAuction(TItemShopItem item)
{
	int64_t start_time = item.timeAuction.dwStartTime;
	int64_t end_time = item.timeAuction.dwEndTime;
	int64_t current_time = get_global_time();

	if (current_time < start_time)
	{
		int64_t deltaTime = start_time - current_time;
		return deltaTime < 86400; // 1 day
	}

	if (current_time > end_time) {
		int64_t deltaTime = current_time - end_time;
		return deltaTime < 3600; // 1 hour
	}

	return true;
}

DWORD CItemShopManager::GetTimeAuctionProgress(TItemShopItem item)
{
	if (item.timeAuction.dwMaxBuyAmount < 1)
		return 999; // infinite

	if (item.timeAuction.dwCurrentBuyAmount >= item.timeAuction.dwMaxBuyAmount)
		return 0;

	DWORD amount_left = item.timeAuction.dwMaxBuyAmount - item.timeAuction.dwCurrentBuyAmount;
	return amount_left * 100 / item.timeAuction.dwMaxBuyAmount;
}

void CItemShopManager::SendBalance(LPCHARACTER ch, PlayerAccountBalance balance)
{
	if (!ch)
		return;

	LPDESC desc = ch->GetDesc();
	if (!desc)
		return;

	TPacketGCItemShop pack;
	pack.header = HEADER_GC_ITEMSHOP;
	pack.subheader = SUBHEADER_ITEMSHOP_BALANCE;

	TPacketGCItemShopBalance pack2;
	memset(&pack2, 0, sizeof(pack2));

	ch->ChatDebug("current balance dc: %lld, dm: %lld", static_cast<long long>(balance.dwDragonCoins), static_cast<long long>(balance.dwDragonMarks));
	pack2.dwDragonCoins = balance.dwDragonCoins;
	pack2.dwDragonMarks = balance.dwDragonMarks;

	pack.size = sizeof(pack) + sizeof(pack2);
	ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCItemShop));
	ch->GetDesc()->Packet(&pack2, sizeof(TPacketGCItemShopBalance));
}

void CItemShopManager::SendBalance(LPCHARACTER ch)
{
	auto balance = GetPlayerAccountBalance(ch);
	return SendBalance(ch, balance);
}

void CItemShopManager::SendHappyHour(LPCHARACTER ch)
{
	int value = quest::CQuestManager::instance().GetEventFlag("happy_hour_value");
	int dwEndTime = quest::CQuestManager::instance().GetEventFlag("happy_hour_end_time");

	if (value < 1)
		return;

	if (dwEndTime <= 0 || get_global_time() > static_cast<DWORD>(dwEndTime))
		return;

	if (!ch)
		return;

	LPDESC desc = ch->GetDesc();
	if (!desc)
		return;

	TPacketGCItemShop pack;
	pack.header = HEADER_GC_ITEMSHOP;
	pack.subheader = SUBHEADER_ITEMSHOP_HAPPY_HOUR;

	TPacketGCItemShopHappyHour pack2;
	memset(&pack2, 0, sizeof(pack2));

	pack2.dwHappyHourValue = static_cast<DWORD>(value);
	pack2.dwEndTime = dwEndTime;

	pack.size = sizeof(pack) + sizeof(pack2);
	ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCItemShop));
	ch->GetDesc()->Packet(&pack2, sizeof(TPacketGCItemShopHappyHour));
}

void CItemShopManager::SendLatestTimeAuction(LPCHARACTER ch)
{
	if (!ch || !ch->GetDesc())
		return;

	int64_t latestEndTime = 0;
	for (auto it : m_map_ItemShopTimeAuctions)
	{
		auto item = it.second;
		if (CanSendTimeAuction(item))
		{
			if (item.timeAuction.dwEndTime > latestEndTime)
				latestEndTime = item.timeAuction.dwEndTime;
		}
	}

	if (latestEndTime > 0)
	{
		TPacketGCItemShop pack;
		pack.header = HEADER_GC_ITEMSHOP;
		pack.subheader = SUBHEADER_ITEMSHOP_TIME_AUCTION;

		TPacketGCItemShopLatestTimeAuction pack2;
		memset(&pack2, 0, sizeof(pack2));
		pack2.dwEndTime = latestEndTime;

		pack.size = sizeof(pack) + sizeof(pack2);
		ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCItemShop));
		ch->GetDesc()->Packet(&pack2, sizeof(TPacketGCItemShopLatestTimeAuction));
	}
}

void CItemShopManager::SendItemShopData(LPCHARACTER ch)
{
	if (!ch)
		return;

	LPDESC desc = ch->GetDesc();
	if (!desc)
		return;

	size_t itemCount = 0;
	for (const auto& pair : m_map_ItemShopItems)
		if (CanSendItem(ch, pair.second))
			itemCount++;

	TPacketGCItemShop pack{};
	pack.header = HEADER_GC_ITEMSHOP;
	pack.subheader = SUBHEADER_ITEMSHOP_REFRESH;
	pack.size = static_cast<int>(sizeof(pack) + sizeof(TPacketGCItemShopRefresh) + sizeof(TItemShopItem) * itemCount);

	TPacketGCItemShopRefresh subpack{};
	memset(&subpack, 0, sizeof(subpack));
	subpack.count = static_cast<WORD>(itemCount);

	TEMP_BUFFER buff;
	buff.write(&pack, sizeof(pack));
	buff.write(&subpack, sizeof(subpack));

	for (const auto& pair : m_map_ItemShopItems) {
		auto& item = pair.second;
		if (CanSendItem(ch, item)) {
			//ch->ChatDebug("sending item vnum: %d count: %d", item.dwVnum, item.dwCount);
			buff.write(&item, sizeof(TItemShopItem));
		}
	}

	desc->Packet(buff.read_peek(), buff.size());
	ch->ChatDebug("%d items sent", static_cast<int>(itemCount));
}

void CItemShopManager::SendAuctionData(LPCHARACTER ch)
{
	if (!ch)
		return;

	LPDESC desc = ch->GetDesc();
	if (!desc)
		return;

	size_t itemCount = 0;
	for (const auto& pair : m_map_ItemShopTimeAuctions)
		if (CanSendTimeAuction(pair.second))
			itemCount++;

	TPacketGCItemShop pack{};
	pack.header = HEADER_GC_ITEMSHOP;
	pack.subheader = SUBHEADER_ITEMSHOP_OPEN_AUCTIONS;
	pack.size = static_cast<int>(sizeof(pack) + sizeof(TPacketGCItemShopRefresh) + sizeof(TItemShopItem) * itemCount);

	TPacketGCItemShopRefresh subpack{};
	memset(&subpack, 0, sizeof(subpack));
	subpack.count = static_cast<WORD>(itemCount);

	TEMP_BUFFER buff;
	buff.write(&pack, sizeof(pack));
	buff.write(&subpack, sizeof(subpack));

	for (const auto& pair : m_map_ItemShopTimeAuctions) {
		auto& item = pair.second;
		if (CanSendTimeAuction(item)) {
			TItemShopItem sendItem{};
			memcpy(&sendItem, &item, sizeof(TItemShopItem));
			sendItem.timeAuction.dwMaxBuyAmount = 0;
			sendItem.timeAuction.dwCurrentBuyAmount = GetTimeAuctionProgress(item);
			//ch->ChatDebug("sending auction item vnum: %d count: %d", sendItem.dwVnum, sendItem.dwCount);
			buff.write(&sendItem, sizeof(TItemShopItem));
		}
	}

	desc->Packet(buff.read_peek(), buff.size());
	ch->ChatDebug("%d time auctions sent", static_cast<int>(itemCount));
}

void CItemShopManager::SendAuctionUpdate(LPCHARACTER ch, DWORD index)
{
	if (!ch)
		return;

	LPDESC desc = ch->GetDesc();
	if (!desc)
		return;

	auto it = m_map_ItemShopTimeAuctions.find(index);
	if (it == m_map_ItemShopTimeAuctions.end())
		return;
	
	auto item = it->second;
	if (!CanSendTimeAuction(item))
		return;

	TPacketGCItemShop pack{};
	pack.header = HEADER_GC_ITEMSHOP;
	pack.subheader = SUBHEADER_ITEMSHOP_UPDATE_AUCTION;
	pack.size = sizeof(pack) + sizeof(TItemShopUpdateAuctionItem);

	TItemShopUpdateAuctionItem auctionPacket{};
	memset(&auctionPacket, 0, sizeof(auctionPacket));
	auctionPacket.dwIndex = item.dwIndex;
	auctionPacket.dwProgress = GetTimeAuctionProgress(item);
	auctionPacket.dwStartTime = item.timeAuction.dwStartTime;
	auctionPacket.dwEndTime = item.timeAuction.dwEndTime;
	auctionPacket.dwAccountLimit = item.timeAuction.dwAccountLimit;

	TEMP_BUFFER buff;
	buff.write(&pack, sizeof(pack));
	buff.write(&auctionPacket, sizeof(TItemShopUpdateAuctionItem));

	desc->Packet(buff.read_peek(), buff.size());
}

CItemShopManager::PlayerAccountBalance CItemShopManager::GetPlayerAccountBalance(LPCHARACTER ch)
{
	auto balance = PlayerAccountBalance();
	if (!ch)
		return balance;

	if (!ch->GetDesc())
		return balance;

	DWORD account_id = ch->GetDesc()->GetAccountTable().id;
	std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().AccountQuery("SELECT cash, cash_mark FROM account WHERE id=%u", static_cast<unsigned int>(account_id)));
	if (!pkMsg || pkMsg->uiSQLErrno != 0 || !pkMsg->Get() || pkMsg->Get()->uiNumRows < 1)
		return balance;

	if (pkMsg->Get()->uiNumRows > 1)
	{
		sys_err("MORE THAN 1 account with id %u ???? FATAL ERROR!!!!! ", static_cast<unsigned int>(account_id));
		return balance;
	}

	MYSQL_ROW row = mysql_fetch_row(pkMsg->Get()->pSQLResult);
	if (!row)
		return balance;
	long long rawCoins = 0;
	long long rawMarks = 0;
	str_to_number(rawCoins, row[0]);
	str_to_number(rawMarks, row[1]);
	if (rawCoins < 0) rawCoins = 0;
	if (rawMarks < 0) rawMarks = 0;
	balance.dwDragonCoins = static_cast<int64_t>(rawCoins);
	balance.dwDragonMarks = static_cast<int64_t>(rawMarks);

	return balance;
}

void CItemShopManager::RefreshItemShop(LPCHARACTER ch)
{
	if (!ch)
		return;

	ch->ChatDebug("refresh itemshop");
	SendBalance(ch);
	SendHappyHour(ch);
	SendLatestTimeAuction(ch);
	SendItemShopData(ch);

	if (ch->HasPlayerData())
		ch->playerData->m_isRefreshItemShopOnOpen = false;
}

void CItemShopManager::OpenItemShop(LPCHARACTER ch, bool refresh)
{
	if (!ch)
		return;

	if (ch->IsBusy())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_CONTINUE_WHEN_BUSY"));
		return;
	}

	if (ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_WHILE_DEAD"));
		return;
	}

	if (!ch->HasPlayerData())
		return;

	if (!refresh && ch->playerData->m_isRefreshItemShopOnOpen)
		refresh = true;

	ch->ChatDebug("open itemshop");
	ch->playerData->SetItemShopBrowse(true);
	if (refresh) {
		RefreshItemShop(ch);
	}
	ch->ChatPacket(CHAT_TYPE_COMMAND, "itemshop_open");
}

void CItemShopManager::CloseItemShop(LPCHARACTER ch)
{
	if (!ch)
		return;

	if (!ch->HasPlayerData())
		return;

	ch->ChatDebug("close itemshop");
	ch->playerData->SetItemShopBrowse(false);
	CloseAuctionTab(ch);
}

void CItemShopManager::SendUseCodeVoucher(LPCHARACTER ch, const char* code)
{
	if (!ch || !ch->GetDesc() || !code || !db_clientdesc || db_clientdesc->GetSocket() == INVALID_SOCKET)
		return;

	if (ch->HasPlayerData() && !ch->playerData->IsItemShopBrowse())
		return;

	if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::ItemShopBuy, std::chrono::milliseconds(2000)))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NEXT_ACTION_WAIT_NO_FORMAT"));
		return;
	}

	char szCode[CODE_VOUCHER_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szCode, sizeof(szCode), code, std::strlen(code));

	ch->ChatDebug("checkCode %s", szCode);

	TRequestUseCodeVoucher packet;
	packet.dwPID = ch->GetPlayerID();
	packet.dwAID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(packet.code, szCode, sizeof(packet.code));

	db_clientdesc->DBPacketHeader(HEADER_GD_REQUEST_USE_VOUCHER, 0, sizeof(TRequestUseCodeVoucher));
	db_clientdesc->Packet(&packet, sizeof(packet));
}

void CItemShopManager::RecvUseCodeVoucher(LPCHARACTER ch, BYTE bResponse, const char* code)
{
	if (!ch)
		return;

	if (!ch->GetDesc())
		return;

	TPacketGCItemShop pack{};
	pack.header = HEADER_GC_ITEMSHOP;
	pack.subheader = SUBHEADER_ITEMSHOP_USE_VOUCHER;
	pack.size = sizeof(pack) + sizeof(TPacketGCItemShopUseCodeVoucher);

	TPacketGCItemShopUseCodeVoucher pack2{};
	pack2.bResponse = bResponse;
	ch->GetDesc()->BufferedPacket(&pack, sizeof(TPacketGCItemShop));
	ch->GetDesc()->Packet(&pack2, sizeof(TPacketGCItemShopUseCodeVoucher));

	if (bResponse == USE_VOUCHER_SUCCESS)
	{
		SendBalance(ch);

		LogManager::Instance().VoucherCodeLog(ch, code);
	}
}

void CItemShopManager::OpenAuctionTab(LPCHARACTER ch)
{
	if (!ch)
		return;

	m_map_AuctionViewer.emplace(ch->GetPlayerID());
	SendAuctionData(ch);
}

void CItemShopManager::CloseAuctionTab(LPCHARACTER ch)
{
	m_map_AuctionViewer.erase(ch->GetPlayerID());
}

void CItemShopManager::RefreshAuctionViewers(DWORD auctionIndex)
{
	for (auto& guest : m_map_AuctionViewer)
	{
		if (auto ch = CHARACTER_MANAGER::Instance().FindByPID(guest))
		{
			SendAuctionUpdate(ch, auctionIndex);
		}
	}
}

bool CItemShopManager::CheckBalance(LPCHARACTER ch, BYTE chargeType, int64_t amount, PlayerAccountBalance& currentBalance, bool addMarks)
{
	if (amount == INT64_MIN || currentBalance.dwDragonCoins < 0 || currentBalance.dwDragonMarks < 0)
		return false;

	if (chargeType == ERequestCharge_Cash)
	{
		if (amount < 0)
		{
			const uint64_t cost = static_cast<uint64_t>(-amount);
			if (cost > static_cast<uint64_t>(currentBalance.dwDragonCoins))
			{
				if (ch)
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_NO_DRAGON_COINS"));
				return false;
			}
			if (addMarks && static_cast<uint64_t>(currentBalance.dwDragonMarks) + cost > static_cast<uint64_t>(INT64_MAX))
				return false;

			currentBalance.dwDragonCoins -= static_cast<int64_t>(cost);
			if (addMarks)
				currentBalance.dwDragonMarks += static_cast<int64_t>(cost);
		}
		else
		{
			const uint64_t credit = static_cast<uint64_t>(amount);
			if (static_cast<uint64_t>(currentBalance.dwDragonCoins) + credit > static_cast<uint64_t>(INT64_MAX))
				return false;
			currentBalance.dwDragonCoins += static_cast<int64_t>(credit);
		}
	}
	else if (chargeType == ERequestCharge_Marks)
	{
		if (amount < 0)
		{
			const uint64_t cost = static_cast<uint64_t>(-amount);
			if (cost > static_cast<uint64_t>(currentBalance.dwDragonMarks))
			{
				if (ch)
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_NO_DRAGON_MARKS"));
				return false;
			}
			currentBalance.dwDragonMarks -= static_cast<int64_t>(cost);
		}
		else
		{
			const uint64_t credit = static_cast<uint64_t>(amount);
			if (static_cast<uint64_t>(currentBalance.dwDragonMarks) + credit > static_cast<uint64_t>(INT64_MAX))
				return false;
			currentBalance.dwDragonMarks += static_cast<int64_t>(credit);
		}
	}
	else
	{
		return false;
	}
	return true;
}

bool CItemShopManager::AddCash(LPCHARACTER ch, BYTE chargeType, int64_t amount, bool checkBalance, int64_t customSecondAmount)
{
	if (NULL == ch || NULL == ch->GetDesc() || NULL == db_clientdesc || db_clientdesc->GetSocket() == INVALID_SOCKET)
	{
		return false;
	}
	if (chargeType != ERequestCharge_Cash && chargeType != ERequestCharge_Marks)
		return false;
	if (amount == INT64_MIN)
		return false;
	if (customSecondAmount < -1)
		return false;

	if (checkBalance) {
		PlayerAccountBalance balance = GetPlayerAccountBalance(ch);
		if (!CheckBalance(ch, chargeType, amount, balance))
			return false;
	}

	const DWORD aid = ch->GetDesc()->GetAccountTable().id;
	if (amount == 0)
	{
		std::unique_ptr<SQLMsg> pkCheck(DBManager::instance().AccountQuery(
			"SELECT id FROM account WHERE id = %u LIMIT 1",
			static_cast<unsigned int>(aid)));
		if (!pkCheck || pkCheck->uiSQLErrno != 0 || !pkCheck->Get() || pkCheck->Get()->uiNumRows != 1)
			return false;
	}
	else if (chargeType == ERequestCharge_Cash)
	{
		if (amount < 0)
		{
			const uint64_t cost = static_cast<uint64_t>(-amount);
			const uint64_t secondCredit = (customSecondAmount >= 0) ? static_cast<uint64_t>(customSecondAmount) : cost;
			std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().AccountQuery(
				"UPDATE account SET cash = cash - %llu, cash_mark = cash_mark + %llu "
				"WHERE id = %u AND cash >= %llu AND cash_mark >= 0 "
				"AND cash_mark <= %lld - %llu LIMIT 1",
				static_cast<unsigned long long>(cost),
				static_cast<unsigned long long>(secondCredit),
				static_cast<unsigned int>(aid),
				static_cast<unsigned long long>(cost),
				static_cast<long long>(INT64_MAX),
				static_cast<unsigned long long>(secondCredit)));
			if (!pkMsg || pkMsg->uiSQLErrno != 0 || !pkMsg->Get() || pkMsg->Get()->uiAffectedRows != 1)
			{
				if (pkMsg && pkMsg->uiSQLErrno == 0 && pkMsg->Get() && pkMsg->Get()->uiAffectedRows == 0)
				{
					PlayerAccountBalance latest = GetPlayerAccountBalance(ch);
					if (latest.dwDragonCoins < static_cast<int64_t>(cost))
						ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_NO_DRAGON_COINS"));
				}
				return false;
			}
		}
		else
		{
			const uint64_t credit = static_cast<uint64_t>(amount);
			std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().AccountQuery(
				"UPDATE account SET cash = cash + %llu "
				"WHERE id = %u AND cash >= 0 "
				"AND cash <= %lld - %llu LIMIT 1",
				static_cast<unsigned long long>(credit),
				static_cast<unsigned int>(aid),
				static_cast<long long>(INT64_MAX),
				static_cast<unsigned long long>(credit)));
			if (!pkMsg || pkMsg->uiSQLErrno != 0 || !pkMsg->Get() || pkMsg->Get()->uiAffectedRows != 1)
				return false;
		}
	}
	else if (chargeType == ERequestCharge_Marks)
	{
		if (amount < 0)
		{
			const uint64_t cost = static_cast<uint64_t>(-amount);
			std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().AccountQuery(
				"UPDATE account SET cash_mark = cash_mark - %llu "
				"WHERE id = %u AND cash_mark >= %llu LIMIT 1",
				static_cast<unsigned long long>(cost),
				static_cast<unsigned int>(aid),
				static_cast<unsigned long long>(cost)));
			if (!pkMsg || pkMsg->uiSQLErrno != 0 || !pkMsg->Get() || pkMsg->Get()->uiAffectedRows != 1)
			{
				if (pkMsg && pkMsg->uiSQLErrno == 0 && pkMsg->Get() && pkMsg->Get()->uiAffectedRows == 0)
				{
					PlayerAccountBalance latest = GetPlayerAccountBalance(ch);
					if (latest.dwDragonMarks < static_cast<int64_t>(cost))
						ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_NO_DRAGON_MARKS"));
				}
				return false;
			}
		}
		else
		{
			const uint64_t credit = static_cast<uint64_t>(amount);
			std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().AccountQuery(
				"UPDATE account SET cash_mark = cash_mark + %llu "
				"WHERE id = %u AND cash_mark >= 0 "
				"AND cash_mark <= %lld - %llu LIMIT 1",
				static_cast<unsigned long long>(credit),
				static_cast<unsigned int>(aid),
				static_cast<long long>(INT64_MAX),
				static_cast<unsigned long long>(credit)));
			if (!pkMsg || pkMsg->uiSQLErrno != 0 || !pkMsg->Get() || pkMsg->Get()->uiAffectedRows != 1)
				return false;
		}
	}

	if (checkBalance)
		SendBalance(ch);

	return true;
}

bool CItemShopManager::EnsureTransactionJournalSchema()
{
	std::unique_ptr<SQLMsg> pkProbe(DBManager::instance().AccountQuery(
		"SELECT 1 FROM itemshop_tx_journal LIMIT 1"));
	if (pkProbe && pkProbe->uiSQLErrno == 0)
		return true;

	std::unique_ptr<SQLMsg> pkCreate(DBManager::instance().AccountQuery(
		"CREATE TABLE IF NOT EXISTS `itemshop_tx_journal` ("
		"  `tx_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
		"  `aid` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `pid` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `login` varchar(30) NOT NULL DEFAULT '',"
		"  `item_index` int(11) NOT NULL DEFAULT 0,"
		"  `vnum` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `quantity` int(11) NOT NULL DEFAULT 1,"
		"  `item_count` int(11) unsigned NOT NULL DEFAULT 1,"
		"  `charge_type` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `cash_delta` bigint(20) NOT NULL DEFAULT 0,"
		"  `mark_delta` bigint(20) NOT NULL DEFAULT 0,"
		"  `money_before_buy` bigint(20) NOT NULL DEFAULT 0,"
		"  `is_vip` tinyint(1) NOT NULL DEFAULT 0,"
		"  `vip_type` tinyint(3) unsigned NOT NULL DEFAULT 0,"
		"  `vip_hours` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `is_time_auction` tinyint(1) NOT NULL DEFAULT 0,"
		"  `auction_reserved_count` int(11) unsigned NOT NULL DEFAULT 0,"
		"  `state` enum('INITIATED','CHARGED','DELIVERED','REFUNDED','NEEDS_MANUAL_REVIEW') NOT NULL DEFAULT 'INITIATED',"
		"  `delivered_item_ids` varchar(255) NOT NULL DEFAULT '',"
		"  `created_at` datetime NOT NULL DEFAULT current_timestamp(),"
		"  `updated_at` datetime NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),"
		"  PRIMARY KEY (`tx_id`),"
		"  KEY `idx_itemshop_tx_state` (`state`),"
		"  KEY `idx_itemshop_tx_aid` (`aid`),"
		"  KEY `idx_itemshop_tx_pid` (`pid`)"
		") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC"));
	return (pkCreate && pkCreate->uiSQLErrno == 0);
}

int CItemShopManager::RecoverIncompleteTransactions()
{
	std::unique_ptr<SQLMsg> pkSelect(DBManager::instance().AccountQuery(
		"SELECT tx_id, aid, pid, item_index, vnum, quantity, item_count, charge_type, "
		"cash_delta, mark_delta, money_before_buy, is_vip, is_time_auction, auction_reserved_count, state "
		"FROM itemshop_tx_journal "
		"WHERE state IN ('INITIATED', 'CHARGED') "
		"ORDER BY tx_id ASC"));
	if (!pkSelect || pkSelect->uiSQLErrno != 0 || !pkSelect->Get())
		return -1;

	if (pkSelect->Get()->uiNumRows == 0)
		return 0;

	struct IncompleteTxRow
	{
		uint64_t txId = 0;
		DWORD aid = 0;
		DWORD pid = 0;
		int itemIndex = 0;
		DWORD vnum = 0;
		int quantity = 0;
		DWORD itemCount = 0;
		BYTE chargeType = 0;
		int64_t cashDelta = 0;
		int64_t markDelta = 0;
		int64_t moneyBeforeBuy = 0;
		int isVip = 0;
		int isTimeAuction = 0;
		DWORD auctionReservedCount = 0;
		std::string state;
	};

	std::vector<IncompleteTxRow> pendingRows;
	pendingRows.reserve(pkSelect->Get()->uiNumRows);

	MYSQL_ROW row = nullptr;
	while ((row = mysql_fetch_row(pkSelect->Get()->pSQLResult)) != nullptr)
	{
		int col = 0;
		IncompleteTxRow tx{};
		if (row[col]) str_to_number(tx.txId, row[col]);
		++col;
		if (row[col]) str_to_number(tx.aid, row[col]);
		++col;
		if (row[col]) str_to_number(tx.pid, row[col]);
		++col;
		if (row[col]) str_to_number(tx.itemIndex, row[col]);
		++col;
		if (row[col]) str_to_number(tx.vnum, row[col]);
		++col;
		if (row[col]) str_to_number(tx.quantity, row[col]);
		++col;
		if (row[col]) str_to_number(tx.itemCount, row[col]);
		++col;
		if (row[col]) str_to_number(tx.chargeType, row[col]);
		++col;
		if (row[col]) str_to_number(tx.cashDelta, row[col]);
		++col;
		if (row[col]) str_to_number(tx.markDelta, row[col]);
		++col;
		if (row[col]) str_to_number(tx.moneyBeforeBuy, row[col]);
		++col;
		if (row[col]) str_to_number(tx.isVip, row[col]);
		++col;
		if (row[col]) str_to_number(tx.isTimeAuction, row[col]);
		++col;
		if (row[col]) str_to_number(tx.auctionReservedCount, row[col]);
		++col;
		if (row[col]) tx.state = row[col];
		pendingRows.push_back(tx);
	}

	int recoveredCount = 0;
	for (const IncompleteTxRow& tx : pendingRows)
	{
		if (tx.state == "INITIATED")
		{
			std::unique_ptr<SQLMsg> pkAbort(DBManager::instance().AccountQuery(
				"UPDATE itemshop_tx_journal SET state = 'REFUNDED', delivered_item_ids = 'ABORTED_BEFORE_CHARGE' "
				"WHERE tx_id = %llu AND state = 'INITIATED' LIMIT 1",
				static_cast<unsigned long long>(tx.txId)));
			if (pkAbort && pkAbort->uiSQLErrno == 0 && pkAbort->Get() && pkAbort->Get()->uiAffectedRows == 1)
			{
				if (tx.isTimeAuction != 0 && tx.auctionReservedCount > 0 && tx.itemIndex > 0)
				{
					DBManager::instance().DirectQuery(
						"UPDATE itemshop_time_auction SET buy_count = CASE WHEN buy_count >= %u THEN buy_count - %u ELSE 0 END "
						"WHERE item_index = %u",
						static_cast<unsigned int>(tx.auctionReservedCount),
						static_cast<unsigned int>(tx.auctionReservedCount),
						static_cast<unsigned int>(tx.itemIndex));
				}
				++recoveredCount;
			}
			continue;
		}

		if (tx.state != "CHARGED")
			continue;

		// Check if item_award already received the items for this tx_id right before crash
		uint64_t awardRows = 0;
		uint64_t awardSumCount = 0;
		uint64_t awardTakenRows = 0;
		if (tx.isVip == 0)
		{
			std::unique_ptr<SQLMsg> pkAwardCheck(DBManager::instance().DirectQuery(
				"SELECT COUNT(*), COALESCE(SUM(count), 0), "
				"COALESCE(SUM(CASE WHEN taken_time IS NOT NULL THEN 1 ELSE 0 END), 0) "
				"FROM item_award WHERE why = 'ITEMSHOP_TX_%llu'",
				static_cast<unsigned long long>(tx.txId)));
			if (pkAwardCheck && pkAwardCheck->uiSQLErrno == 0 && pkAwardCheck->Get() && pkAwardCheck->Get()->uiNumRows == 1)
			{
				MYSQL_ROW aRow = mysql_fetch_row(pkAwardCheck->Get()->pSQLResult);
				if (aRow)
				{
					if (aRow[0]) str_to_number(awardRows, aRow[0]);
					if (aRow[1]) str_to_number(awardSumCount, aRow[1]);
					if (aRow[2]) str_to_number(awardTakenRows, aRow[2]);
				}
			}
		}

		// Subcase A: Full item_award delivery had already completed before crash -> finalize as DELIVERED (0 double-delivery, 0 false refund)
		if (tx.isVip == 0 && tx.itemCount > 0 && awardSumCount >= static_cast<uint64_t>(tx.itemCount))
		{
			std::unique_ptr<SQLMsg> pkFinalize(DBManager::instance().AccountQuery(
				"UPDATE itemshop_tx_journal SET state = 'DELIVERED', delivered_item_ids = 'RECOVERED_AWARD' "
				"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
				static_cast<unsigned long long>(tx.txId)));
			if (pkFinalize && pkFinalize->uiSQLErrno == 0 && pkFinalize->Get() && pkFinalize->Get()->uiAffectedRows == 1)
				++recoveredCount;
			continue;
		}

		// Subcase B: Partial item_award row was already claimed by the player -> mark NEEDS_MANUAL_REVIEW
		if (awardRows > 0 && awardTakenRows > 0)
		{
			std::unique_ptr<SQLMsg> pkReview(DBManager::instance().AccountQuery(
				"UPDATE itemshop_tx_journal SET state = 'NEEDS_MANUAL_REVIEW', delivered_item_ids = 'PARTIAL_AWARD_TAKEN' "
				"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
				static_cast<unsigned long long>(tx.txId)));
			if (pkReview && pkReview->uiSQLErrno == 0 && pkReview->Get() && pkReview->Get()->uiAffectedRows == 1)
				++recoveredCount;
			continue;
		}

		// Subcase C: Clean up any untaken partial item_award rows before refunding
		if (awardRows > 0)
		{
			std::unique_ptr<SQLMsg> pkCleanAward(DBManager::instance().DirectQuery(
				"DELETE FROM item_award WHERE why = 'ITEMSHOP_TX_%llu' AND taken_time IS NULL",
				static_cast<unsigned long long>(tx.txId)));
			if (!pkCleanAward || pkCleanAward->uiSQLErrno != 0)
			{
				DBManager::instance().AccountQuery(
					"UPDATE itemshop_tx_journal SET state = 'NEEDS_MANUAL_REVIEW', delivered_item_ids = 'PARTIAL_AWARD_DELETE_FAILED' "
					"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
					static_cast<unsigned long long>(tx.txId));
				continue;
			}
		}

		// Perform atomic, idempotent refund on AccountDB
		std::unique_ptr<SQLMsg> pkBegin(DBManager::instance().AccountQuery("START TRANSACTION"));
		if (!pkBegin || pkBegin->uiSQLErrno != 0)
			continue;

		std::unique_ptr<SQLMsg> pkClaimRefund(DBManager::instance().AccountQuery(
			"UPDATE itemshop_tx_journal SET state = 'REFUNDED', delivered_item_ids = 'CRASH_REFUNDED' "
			"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
			static_cast<unsigned long long>(tx.txId)));
		if (!pkClaimRefund || pkClaimRefund->uiSQLErrno != 0 || !pkClaimRefund->Get() || pkClaimRefund->Get()->uiAffectedRows != 1)
		{
			DBManager::instance().AccountQuery("ROLLBACK");
			continue;
		}

		bool refundAccountOk = false;
		if (tx.chargeType == ERequestCharge_Cash)
		{
			const uint64_t refundCash = (tx.cashDelta < 0) ? static_cast<uint64_t>(-tx.cashDelta) : 0ULL;
			const uint64_t rollbackMarks = (tx.markDelta > 0) ? static_cast<uint64_t>(tx.markDelta) : 0ULL;
			std::unique_ptr<SQLMsg> pkRefundAcc(DBManager::instance().AccountQuery(
				"UPDATE account SET cash = cash + %llu, "
				"cash_mark = CASE WHEN cash_mark >= %llu THEN cash_mark - %llu ELSE 0 END "
				"WHERE id = %u LIMIT 1",
				static_cast<unsigned long long>(refundCash),
				static_cast<unsigned long long>(rollbackMarks),
				static_cast<unsigned long long>(rollbackMarks),
				static_cast<unsigned int>(tx.aid)));
			refundAccountOk = (pkRefundAcc && pkRefundAcc->uiSQLErrno == 0 && pkRefundAcc->Get() && pkRefundAcc->Get()->uiAffectedRows == 1);
		}
		else if (tx.chargeType == ERequestCharge_Marks)
		{
			const uint64_t refundMarks = (tx.markDelta < 0) ? static_cast<uint64_t>(-tx.markDelta) : 0ULL;
			std::unique_ptr<SQLMsg> pkRefundAcc(DBManager::instance().AccountQuery(
				"UPDATE account SET cash_mark = cash_mark + %llu WHERE id = %u LIMIT 1",
				static_cast<unsigned long long>(refundMarks),
				static_cast<unsigned int>(tx.aid)));
			refundAccountOk = (pkRefundAcc && pkRefundAcc->uiSQLErrno == 0 && pkRefundAcc->Get() && pkRefundAcc->Get()->uiAffectedRows == 1);
		}

		if (!refundAccountOk)
		{
			DBManager::instance().AccountQuery("ROLLBACK");
			DBManager::instance().AccountQuery(
				"UPDATE itemshop_tx_journal SET state = 'NEEDS_MANUAL_REVIEW', delivered_item_ids = 'ACCOUNT_REFUND_FAILED' "
				"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
				static_cast<unsigned long long>(tx.txId));
			sys_err("[CRITICAL][ITEMSHOP_RECOVERY_MANUAL_REVIEW] Failed to refund account for tx_id=%llu (aid=%u, pid=%u)",
				static_cast<unsigned long long>(tx.txId),
				static_cast<unsigned int>(tx.aid),
				static_cast<unsigned int>(tx.pid));
			continue;
		}

		std::unique_ptr<SQLMsg> pkCommit(DBManager::instance().AccountQuery("COMMIT"));
		if (!pkCommit || pkCommit->uiSQLErrno != 0)
		{
			DBManager::instance().AccountQuery("ROLLBACK");
			continue;
		}

		if (tx.isTimeAuction != 0 && tx.auctionReservedCount > 0 && tx.itemIndex > 0)
		{
			DBManager::instance().DirectQuery(
				"UPDATE itemshop_time_auction SET buy_count = CASE WHEN buy_count >= %u THEN buy_count - %u ELSE 0 END "
				"WHERE item_index = %u",
				static_cast<unsigned int>(tx.auctionReservedCount),
				static_cast<unsigned int>(tx.auctionReservedCount),
				static_cast<unsigned int>(tx.itemIndex));
		}

		++recoveredCount;
	}

	return recoveredCount;
}

bool CItemShopManager::BuyItem(LPCHARACTER ch, int itemIndex, int quantity)
{
	if (!ch || !ch->GetDesc() || !db_clientdesc || db_clientdesc->GetSocket() == INVALID_SOCKET)
		return false;

	if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::ItemShopBuy, std::chrono::milliseconds(1000)))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NEXT_ACTION_WAIT_NO_FORMAT"));
		return false;
	}

	if (!ch->CanHandleItem())
		return false;

	if (ch->HasPlayerData() && !ch->playerData->IsItemShopBrowse())
		return false;

	if (quantity < 1 || quantity > 10)
		return false;

	TItemShopItem item = GetItem(itemIndex);
	if (!IsProperItem(item))
		return false;

	if (ch->GetLevel() < item.bMinLevel)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_LOW_LEVEL_FOR_ITEM"), item.bMinLevel);
		return false;
	}

	TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(item.dwVnum);
	if (!itemTable) {
		ch->ChatDebug("item vnum (%d) not found", item.dwVnum);
		return false;
	}

	DWORD PREMIUM_TOKEN_VNUM = 72199;
	bool isVIPEffect = itemTable->bType == ITEM_USE && itemTable->bSubType == USE_VIP && item.dwVnum != PREMIUM_TOKEN_VNUM;
	if (ch->IsActivePremiumSubscription() && isVIPEffect)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("BUY_VIP_WITH_SUBSCRIPTION_ACTIVE_ERROR"));
		return false;
	}

	if (isVIPEffect)
	{
		if (itemTable->alValues[0] < 0 || itemTable->alValues[0] >= PREMIUM_MAX_NUM || itemTable->alValues[1] <= 0)
			return false;
	}

	if (IS_SET(itemTable->dwAntiFlags, ITEM_ANTIFLAG_STACK) || !IS_SET(itemTable->dwFlags, ITEM_FLAG_STACKABLE))
	{
		quantity = 1;
	}

	const int64_t rawUnitPrice = GetItemPrice(item);
	if (rawUnitPrice < 0 || static_cast<uint64_t>(rawUnitPrice) > static_cast<uint64_t>(INT64_MAX) / static_cast<uint64_t>(quantity))
	{
		sys_err("ItemShop::BuyItem price overflow blocked (pid=%u, itemIndex=%d, unitPrice=%lld, quantity=%d)",
			static_cast<uint32_t>(ch->GetPlayerID()), itemIndex,
			static_cast<long long>(rawUnitPrice), quantity);
		return false;
	}
	const uint64_t unitPrice = static_cast<uint64_t>(rawUnitPrice);
	const uint64_t totalPriceU64 = unitPrice * static_cast<uint64_t>(quantity);
	const int64_t llPrice = static_cast<int64_t>(totalPriceU64);

	const uint64_t totalItemCountU64 = static_cast<uint64_t>(item.dwCount) * static_cast<uint64_t>(quantity);
	if (!isVIPEffect && (item.dwCount == 0 || totalItemCountU64 == 0 || totalItemCountU64 > static_cast<uint64_t>(INT32_MAX)))
	{
		sys_err("ItemShop::BuyItem invalid item count blocked (pid=%u, itemIndex=%d, dwCount=%u, quantity=%d)",
			static_cast<uint32_t>(ch->GetPlayerID()), itemIndex,
			static_cast<uint32_t>(item.dwCount), quantity);
		return false;
	}

	PlayerAccountBalance balance = GetPlayerAccountBalance(ch);
	int64_t money_before_buy = 0;

	ERequestChargeType chargeType;
	if (item.bCurrency == CURRENCY_DRAGON_COIN)
	{
		chargeType = ERequestCharge_Cash;
		money_before_buy = balance.dwDragonCoins;
	}
	else
	{
		chargeType = ERequestCharge_Marks;
		money_before_buy = balance.dwDragonMarks;
	}

	bool isItemDragonScroll = itemTable->dwVnum >= 80014 && itemTable->dwVnum <= 80019;

	const int betaServer = (g_dwClientVersion < 1000000) ? quest::CQuestManager::instance().GetEventFlag("beta_server") : 0;
	if (betaServer > 0 && betaServer < 4)
	{
		if (isItemDragonScroll)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "BETA: Zakup kuponow dostepny od etapu 3+.");
			return false;
		}
	}


	if (NULL == ch->GetDesc() || NULL == db_clientdesc || db_clientdesc->GetSocket() == INVALID_SOCKET)
		return false;

	int64_t dragonMarkAmount = 0;
	if (isItemDragonScroll && item.bCurrency == CURRENCY_DRAGON_COIN)
	{
		dragonMarkAmount = llPrice - static_cast<int64_t>(itemTable->alValues[0]);
		if (dragonMarkAmount < 0)
			dragonMarkAmount = 0;
		else if (dragonMarkAmount > llPrice)
			dragonMarkAmount = llPrice;
	}

	if (!CheckBalance(ch, chargeType, -llPrice, balance, !(isItemDragonScroll && item.bCurrency == CURRENCY_DRAGON_COIN)))
		return false;

	if (isItemDragonScroll && item.bCurrency == CURRENCY_DRAGON_COIN)
	{
		if (static_cast<uint64_t>(balance.dwDragonMarks) + static_cast<uint64_t>(dragonMarkAmount) > static_cast<uint64_t>(INT64_MAX))
			return false;
		balance.dwDragonMarks += dragonMarkAmount;
	}

	const bool isTimeAuction = IsItemTimeAuction(item);
	const bool isAuctionInfinite = isTimeAuction && (item.timeAuction.dwMaxBuyAmount == 0);
	bool auctionReservedInDB = false;
	DWORD auctionItemCount = 0;
	std::string auction_buy_flag;
	std::string auction_buy_time_flag;
	long long buyer_buy_count = 0;

	if (isTimeAuction)
	{
		if (!IsTimeAuctionActive(item))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_AUCTION_NOT_AVAILABLE"));
			return false;
		}

		if (item.dwCount == 0 || totalItemCountU64 == 0 || totalItemCountU64 > static_cast<uint64_t>(INT32_MAX))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_AUCTION_NOT_AVAILABLE"));
			return false;
		}
		auctionItemCount = static_cast<DWORD>(totalItemCountU64);

		const DWORD MAX_BUY_PER_ACCOUNT = item.timeAuction.dwAccountLimit;
		if (MAX_BUY_PER_ACCOUNT > 0)
		{
			auction_buy_flag = (item.dwIndex < 100000U)
				? ("itemshop_auction_buy_" + std::to_string(item.dwIndex))
				: ("is_auc_buy_" + std::to_string(item.dwIndex));
			auction_buy_time_flag = (item.dwIndex < 100000U)
				? ("itemshop_auction_time_" + std::to_string(item.dwIndex))
				: ("is_auc_time_" + std::to_string(item.dwIndex));
			buyer_buy_count = ch->GetAccountFlag(auction_buy_flag);
			const long long buyer_buy_time = ch->GetAccountFlag(auction_buy_time_flag);
			if (buyer_buy_count < 0 || buyer_buy_time <= 0 || static_cast<long long>(get_global_time()) > buyer_buy_time)
			{
				buyer_buy_count = 0;
			}

			if (static_cast<uint64_t>(buyer_buy_count) >= static_cast<uint64_t>(MAX_BUY_PER_ACCOUNT))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_TIME_AUCTION_ACCOUNT_LIMIT"), (int)item.timeAuction.dwAccountLimit);
				return false;
			}

			if (static_cast<uint64_t>(buyer_buy_count) + totalItemCountU64 > static_cast<uint64_t>(MAX_BUY_PER_ACCOUNT))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_TIME_AUCTION_PURCHASE_WOULD_EXCEED_LIMIT"));
				return false;
			}
		}

		if (!isAuctionInfinite)
		{
			if (item.timeAuction.dwCurrentBuyAmount >= item.timeAuction.dwMaxBuyAmount ||
				totalItemCountU64 > static_cast<uint64_t>(item.timeAuction.dwMaxBuyAmount))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_AUCTION_NOT_AVAILABLE"));
				return false;
			}

			std::unique_ptr<SQLMsg> pkAuctionReserve(DBManager::instance().DirectQuery(
				"UPDATE itemshop_time_auction SET buy_count = buy_count + %u "
				"WHERE item_index = %u AND buy_count + %u <= %u",
				static_cast<unsigned int>(auctionItemCount),
				static_cast<unsigned int>(item.dwIndex),
				static_cast<unsigned int>(auctionItemCount),
				static_cast<unsigned int>(item.timeAuction.dwMaxBuyAmount)));
			if (!pkAuctionReserve || pkAuctionReserve->uiSQLErrno != 0 || !pkAuctionReserve->Get() || pkAuctionReserve->Get()->uiAffectedRows != 1)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_AUCTION_NOT_AVAILABLE"));
				return false;
			}
			auctionReservedInDB = true;
		}
	}

	auto rollbackAuctionInDB = [&]() {
		if (auctionReservedInDB)
		{
			bool auctionRbOk = false;
			for (int attempt = 0; attempt < 2 && !auctionRbOk; ++attempt)
			{
				std::unique_ptr<SQLMsg> pkAuctionRollback(DBManager::instance().DirectQuery(
					"UPDATE itemshop_time_auction SET buy_count = CASE WHEN buy_count >= %u THEN buy_count - %u ELSE 0 END "
					"WHERE item_index = %u",
					static_cast<unsigned int>(auctionItemCount),
					static_cast<unsigned int>(auctionItemCount),
					static_cast<unsigned int>(item.dwIndex)));
				auctionRbOk = (pkAuctionRollback && pkAuctionRollback->uiSQLErrno == 0 && pkAuctionRollback->Get() && pkAuctionRollback->Get()->uiAffectedRows == 1);
			}
			if (!auctionRbOk)
			{
				sys_err("[CRITICAL][ITEMSHOP_ROLLBACK_FAILED] Failed to rollback itemshop_time_auction (pid=%u, aid=%u, itemIndex=%u, auctionItemCount=%u)",
					static_cast<unsigned int>(ch->GetPlayerID()),
					static_cast<unsigned int>(ch->GetDesc()->GetAccountTable().id),
					static_cast<unsigned int>(item.dwIndex),
					static_cast<unsigned int>(auctionItemCount));
			}
			auctionReservedInDB = false;
		}
	};

	const DWORD aid = ch->GetDesc()->GetAccountTable().id;
	const char* rawLogin = ch->GetDesc()->GetAccountTable().login;
	char szEscapedLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szEscapedLogin, sizeof(szEscapedLogin), rawLogin, std::strlen(rawLogin));

	const int64_t cashDelta = (chargeType == ERequestCharge_Cash) ? -llPrice : 0;
	const int64_t markDelta = (chargeType == ERequestCharge_Cash)
		? ((isItemDragonScroll && item.bCurrency == CURRENCY_DRAGON_COIN) ? dragonMarkAmount : llPrice)
		: -llPrice;
	const BYTE vipType = isVIPEffect ? static_cast<BYTE>(itemTable->alValues[0]) : 0;
	const DWORD vipHours = isVIPEffect ? static_cast<DWORD>(itemTable->alValues[1]) : 0;
	const DWORD journalItemCount = isVIPEffect ? 0U : static_cast<DWORD>(totalItemCountU64);

	std::unique_ptr<SQLMsg> pkInitJournal(DBManager::instance().AccountQuery(
		"INSERT INTO itemshop_tx_journal "
		"(aid, pid, login, item_index, vnum, quantity, item_count, charge_type, cash_delta, mark_delta, money_before_buy, "
		"is_vip, vip_type, vip_hours, is_time_auction, auction_reserved_count, state, delivered_item_ids, created_at, updated_at) "
		"VALUES (%u, %u, '%s', %d, %u, %d, %u, %u, %lld, %lld, %lld, %d, %u, %u, %d, %u, 'INITIATED', '', NOW(), NOW())",
		static_cast<unsigned int>(aid),
		static_cast<unsigned int>(ch->GetPlayerID()),
		szEscapedLogin,
		itemIndex,
		static_cast<unsigned int>(item.dwVnum),
		quantity,
		static_cast<unsigned int>(journalItemCount),
		static_cast<unsigned int>(chargeType),
		static_cast<long long>(cashDelta),
		static_cast<long long>(markDelta),
		static_cast<long long>(money_before_buy),
		isVIPEffect ? 1 : 0,
		static_cast<unsigned int>(vipType),
		static_cast<unsigned int>(vipHours),
		isTimeAuction ? 1 : 0,
		static_cast<unsigned int>(auctionReservedInDB ? auctionItemCount : 0U)));
	if (!pkInitJournal || pkInitJournal->uiSQLErrno != 0 || !pkInitJournal->Get() || pkInitJournal->Get()->ullInsertID == 0)
	{
		sys_err("ItemShop::BuyItem failed to record INITIATED journal entry (pid=%u, aid=%u, itemIndex=%d)",
			static_cast<unsigned int>(ch->GetPlayerID()), static_cast<unsigned int>(aid), itemIndex);
		rollbackAuctionInDB();
		return false;
	}
	const uint64_t txId = pkInitJournal->Get()->ullInsertID;
	m_ullLastTxId = txId;

	ch->ChatDebug("Buy item index %d (quantity %d) with price %lld (tx_id=%llu)",
		itemIndex, quantity, static_cast<long long>(llPrice), static_cast<unsigned long long>(txId));

	std::unique_ptr<SQLMsg> pkBeginTx(DBManager::instance().AccountQuery("START TRANSACTION"));
	if (!pkBeginTx || pkBeginTx->uiSQLErrno != 0)
	{
		DBManager::instance().AccountQuery(
			"UPDATE itemshop_tx_journal SET state = 'REFUNDED', delivered_item_ids = 'TX_BEGIN_FAILED' "
			"WHERE tx_id = %llu AND state = 'INITIATED' LIMIT 1",
			static_cast<unsigned long long>(txId));
		rollbackAuctionInDB();
		return false;
	}

	bool paymentOk = false;
	if (isItemDragonScroll && item.bCurrency == CURRENCY_DRAGON_COIN)
		paymentOk = AddCash(ch, chargeType, -llPrice, false, dragonMarkAmount);
	else
		paymentOk = AddCash(ch, chargeType, -llPrice, false);

	if (paymentOk)
	{
		std::unique_ptr<SQLMsg> pkMarkCharged(DBManager::instance().AccountQuery(
			"UPDATE itemshop_tx_journal SET state = 'CHARGED' "
			"WHERE tx_id = %llu AND state = 'INITIATED' LIMIT 1",
			static_cast<unsigned long long>(txId)));
		if (!pkMarkCharged || pkMarkCharged->uiSQLErrno != 0 || !pkMarkCharged->Get() || pkMarkCharged->Get()->uiAffectedRows != 1)
			paymentOk = false;
	}

	if (paymentOk)
	{
		std::unique_ptr<SQLMsg> pkCommitTx(DBManager::instance().AccountQuery("COMMIT"));
		if (!pkCommitTx || pkCommitTx->uiSQLErrno != 0)
			paymentOk = false;
	}

	if (!paymentOk)
	{
		DBManager::instance().AccountQuery("ROLLBACK");
		DBManager::instance().AccountQuery(
			"UPDATE itemshop_tx_journal SET state = 'REFUNDED', delivered_item_ids = 'CHARGE_ABORTED' "
			"WHERE tx_id = %llu AND state = 'INITIATED' LIMIT 1",
			static_cast<unsigned long long>(txId));
		rollbackAuctionInDB();
		SendBalance(ch);
		return false;
	}

	if (m_bCrashAfterChargeForTest)
	{
		sys_log(0, "ItemShop::BuyItem simulated crash immediately after CHARGED (tx_id=%llu)",
			static_cast<unsigned long long>(txId));
		return false;
	}

	std::string itemId = "null";
	bool deliveryOk = true;
	std::vector<LPITEM> createdItems;
	std::vector<uint64_t> awardInsertIds;

	if (isVIPEffect)
	{
		BYTE premiumType = static_cast<BYTE>(itemTable->alValues[0]);
		DWORD durationInHours = static_cast<DWORD>(itemTable->alValues[1]);
		deliveryOk = AddVIP(ch, premiumType, durationInHours);
	}
	else
	{
		DWORD itemVnum = item.dwVnum;
		DWORD itemCount = static_cast<DWORD>(totalItemCountU64);
		DWORD maxI = 0;
		DWORD r = 0;
		DWORD max_item_stack = (itemTable->dwMaxStack > 0)
			? std::min<DWORD>(itemTable->dwMaxStack, static_cast<DWORD>(INT32_MAX))
			: static_cast<DWORD>(ITEM_MAX_COUNT);
		if (itemCount > max_item_stack)
		{
			maxI = itemCount / max_item_stack;
			r = itemCount - maxI * max_item_stack;
		}
		else
		{
			r = itemCount;
		}

		for (DWORD i = 0; i < maxI; i++) {
			ch->ChatDebug("add item %u %u", itemVnum, max_item_stack);
			bool deliveredToAward = false;
			uint64_t awardInsertId = 0;
			LPITEM newItem = GiveItem(ch, itemIndex, itemVnum, max_item_stack, &deliveredToAward, &awardInsertId, txId);
			if (newItem)
			{
				createdItems.push_back(newItem);
				itemId = std::to_string(newItem->GetID());
			}
			else if (deliveredToAward)
			{
				if (awardInsertId > 0)
					awardInsertIds.push_back(awardInsertId);
			}
			else
			{
				deliveryOk = false;
				break;
			}
		}

		if (deliveryOk && r > 0) {
			ch->ChatDebug("add item %u %u", itemVnum, r);
			bool deliveredToAward = false;
			uint64_t awardInsertId = 0;
			LPITEM newItem = GiveItem(ch, itemIndex, itemVnum, r, &deliveredToAward, &awardInsertId, txId);
			if (newItem)
			{
				createdItems.push_back(newItem);
				itemId = std::to_string(newItem->GetID());
			}
			else if (deliveredToAward)
			{
				if (awardInsertId > 0)
					awardInsertIds.push_back(awardInsertId);
			}
			else
			{
				deliveryOk = false;
			}
		}
	}

	if (!deliveryOk)
	{
		for (LPITEM createdItem : createdItems)
		{
			if (createdItem)
				ITEM_MANAGER::instance().RemoveItem(createdItem, "ITEMSHOP_ROLLBACK");
		}
		for (uint64_t awardId : awardInsertIds)
		{
			bool awardDelOk = false;
			for (int attempt = 0; attempt < 2 && !awardDelOk; ++attempt)
			{
				std::unique_ptr<SQLMsg> pkAwardDelete(DBManager::instance().DirectQuery(
					"DELETE FROM item_award WHERE id = %llu AND taken_time IS NULL",
					static_cast<unsigned long long>(awardId)));
				awardDelOk = (pkAwardDelete && pkAwardDelete->uiSQLErrno == 0 && pkAwardDelete->Get() && pkAwardDelete->Get()->uiAffectedRows == 1);
			}
			if (!awardDelOk)
			{
				sys_err("[CRITICAL][ITEMSHOP_ROLLBACK_FAILED] Failed to delete partial item_award row (pid=%u, aid=%u, awardId=%llu)",
					static_cast<unsigned int>(ch->GetPlayerID()),
					static_cast<unsigned int>(ch->GetDesc()->GetAccountTable().id),
					static_cast<unsigned long long>(awardId));
			}
		}

		bool refundOk = false;
		for (int attempt = 0; attempt < 2 && !refundOk; ++attempt)
		{
			std::unique_ptr<SQLMsg> pkBeginRefund(DBManager::instance().AccountQuery("START TRANSACTION"));
			if (!pkBeginRefund || pkBeginRefund->uiSQLErrno != 0)
				continue;

			std::unique_ptr<SQLMsg> pkClaimRefund(DBManager::instance().AccountQuery(
				"UPDATE itemshop_tx_journal SET state = 'REFUNDED', delivered_item_ids = 'DELIVERY_FAILED_REFUNDED' "
				"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
				static_cast<unsigned long long>(txId)));
			if (!pkClaimRefund || pkClaimRefund->uiSQLErrno != 0 || !pkClaimRefund->Get() || pkClaimRefund->Get()->uiAffectedRows != 1)
			{
				DBManager::instance().AccountQuery("ROLLBACK");
				continue;
			}

			bool accUpdated = false;
			if (chargeType == ERequestCharge_Cash)
			{
				const uint64_t refundCash = static_cast<uint64_t>(llPrice);
				const uint64_t rollbackMarks = (isItemDragonScroll && item.bCurrency == CURRENCY_DRAGON_COIN)
					? static_cast<uint64_t>(dragonMarkAmount)
					: refundCash;
				std::unique_ptr<SQLMsg> pkRefund(DBManager::instance().AccountQuery(
					"UPDATE account SET cash = cash + %llu, "
					"cash_mark = CASE WHEN cash_mark >= %llu THEN cash_mark - %llu ELSE 0 END "
					"WHERE id = %u LIMIT 1",
					static_cast<unsigned long long>(refundCash),
					static_cast<unsigned long long>(rollbackMarks),
					static_cast<unsigned long long>(rollbackMarks),
					static_cast<unsigned int>(aid)));
				accUpdated = (pkRefund && pkRefund->uiSQLErrno == 0 && pkRefund->Get() && pkRefund->Get()->uiAffectedRows == 1);
			}
			else if (chargeType == ERequestCharge_Marks)
			{
				const uint64_t refundMarks = static_cast<uint64_t>(llPrice);
				std::unique_ptr<SQLMsg> pkRefund(DBManager::instance().AccountQuery(
					"UPDATE account SET cash_mark = cash_mark + %llu WHERE id = %u LIMIT 1",
					static_cast<unsigned long long>(refundMarks),
					static_cast<unsigned int>(aid)));
				accUpdated = (pkRefund && pkRefund->uiSQLErrno == 0 && pkRefund->Get() && pkRefund->Get()->uiAffectedRows == 1);
			}

			if (accUpdated)
			{
				std::unique_ptr<SQLMsg> pkCommitRefund(DBManager::instance().AccountQuery("COMMIT"));
				refundOk = (pkCommitRefund && pkCommitRefund->uiSQLErrno == 0);
			}
			if (!refundOk)
			{
				DBManager::instance().AccountQuery("ROLLBACK");
			}
		}

		if (!refundOk)
		{
			DBManager::instance().AccountQuery(
				"UPDATE itemshop_tx_journal SET state = 'NEEDS_MANUAL_REVIEW', delivered_item_ids = 'DELIVERY_AND_REFUND_FAILED' "
				"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
				static_cast<unsigned long long>(txId));
			sys_err("[CRITICAL][ITEMSHOP_ROLLBACK_FAILED] Account refund failed after delivery failure (pid=%u, aid=%u, itemIndex=%d, vnum=%u, qty=%d, price=%lld, chargeType=%d, moneyBefore=%lld)",
				static_cast<unsigned int>(ch->GetPlayerID()),
				static_cast<unsigned int>(aid),
				itemIndex,
				static_cast<unsigned int>(item.dwVnum),
				quantity,
				static_cast<long long>(llPrice),
				static_cast<int>(chargeType),
				static_cast<long long>(money_before_buy));
			LogManager::instance().Query(
				"INSERT INTO itemshop (pid, account_id, item_index, vnum, count, price, currency, item_id, date, money_before_buy) "
				"VALUES (%u, %u, %d, %u, %d, %lld, %d, NULL, NOW(), %lld)",
				ch->GetPlayerID(),
				aid,
				-itemIndex,
				item.dwVnum,
				0,
				static_cast<long long>(llPrice),
				static_cast<int>(chargeType) + 1,
				static_cast<long long>(money_before_buy));
		}

		rollbackAuctionInDB();
		SendBalance(ch);
		return false;
	}

	for (LPITEM createdItem : createdItems)
	{
		if (createdItem)
			ITEM_MANAGER::instance().FlushDelayedSave(createdItem);
	}

	if (m_bCrashAfterDeliveryBeforeJournalForTest)
	{
		sys_log(0, "ItemShop::BuyItem simulated crash after delivery before DELIVERED journal update (tx_id=%llu)",
			static_cast<unsigned long long>(txId));
		return true;
	}

	std::string deliveredSummary = isVIPEffect ? "VIP" : itemId;
	if (!awardInsertIds.empty())
	{
		deliveredSummary = "AWARD:" + std::to_string(awardInsertIds.front());
	}
	DBManager::instance().AccountQuery(
		"UPDATE itemshop_tx_journal SET state = 'DELIVERED', delivered_item_ids = '%s' "
		"WHERE tx_id = %llu AND state = 'CHARGED' LIMIT 1",
		deliveredSummary.c_str(),
		static_cast<unsigned long long>(txId));

	if (isTimeAuction)
	{
		const DWORD MAX_BUY_PER_ACCOUNT = item.timeAuction.dwAccountLimit;
		if (MAX_BUY_PER_ACCOUNT > 0)
		{
			const long long updatedCount = buyer_buy_count + static_cast<long long>(auctionItemCount);
			const long long updatedEndTime = static_cast<long long>(item.timeAuction.dwEndTime);
			ch->SetAccountFlag(auction_buy_flag, updatedCount);
			ch->SetAccountFlag(auction_buy_time_flag, updatedEndTime);
			if (ch->HasPlayerData())
			{
				ch->playerData->SaveSpecialFlag(ch->GetPlayerID(), ch->GetAID());
			}
		}
	}

	if (auctionReservedInDB)
	{
		DWORD new_buy_count = item.timeAuction.dwCurrentBuyAmount + auctionItemCount;
		std::unique_ptr<SQLMsg> pkAuctionSelect(DBManager::instance().DirectQuery(
			"SELECT buy_count FROM itemshop_time_auction WHERE item_index=%u",
			static_cast<unsigned int>(item.dwIndex)));
		if (pkAuctionSelect && pkAuctionSelect->uiSQLErrno == 0 && pkAuctionSelect->Get() && pkAuctionSelect->Get()->uiNumRows >= 1)
		{
			MYSQL_ROW row = mysql_fetch_row(pkAuctionSelect->Get()->pSQLResult);
			if (row && row[0])
				str_to_number(new_buy_count, row[0]);
		}

		DWORD current_progress = GetTimeAuctionProgress(item);
		item.timeAuction.dwCurrentBuyAmount = new_buy_count;
		DWORD new_progress = GetTimeAuctionProgress(item);
		ch->ChatDebug("%d != %d", current_progress, new_progress);
		if (current_progress != new_progress)
		{
			ch->ChatDebug("wysylam!");
			TPacketGGItemShopAuctionUpdate p;
			p.bHeader = HEADER_GG_ITEMSHOP_AUCTION_UPDATE;
			p.dwItemIndex = item.dwIndex;
			p.dwBuyAmount = item.timeAuction.dwCurrentBuyAmount;

			P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGItemShopAuctionUpdate));
			UpdateAuctionBuyAmount(item.dwIndex, item.timeAuction.dwCurrentBuyAmount);
		}
	}

	SendBalance(ch);

	const long long llLogPrice = static_cast<long long>(llPrice);
	LogManager::instance().Query(
		"INSERT INTO itemshop (pid, account_id, item_index, vnum, count, price, currency, item_id, date, money_before_buy) "
		"VALUES (%u, %u, %d, %u, %d, %lld, %d, %s, NOW(), %lld)",
		ch->GetPlayerID(),
		ch->GetDesc()->GetAccountTable().id,
		itemIndex,
		item.dwVnum,
		quantity,
		llLogPrice,
		static_cast<int>(chargeType) + 1,
		itemId.c_str(),
		static_cast<long long>(money_before_buy));
	return true;
}

LPITEM CItemShopManager::GiveItem(LPCHARACTER ch, int itemIndex, DWORD itemVnum, DWORD itemCount, bool* outDeliveredToAward, uint64_t* outAwardInsertId, uint64_t txId)
{
	(void)itemIndex;
	if (outDeliveredToAward)
		*outDeliveredToAward = false;
	if (outAwardInsertId)
		*outAwardInsertId = 0;

	if (!ch || !ch->GetDesc())
		return nullptr;

	if (itemCount == 0 || itemCount > static_cast<DWORD>(INT32_MAX))
	{
		sys_err("ItemShop::GiveItem invalid itemCount=%u (max=%d, pid=%u, vnum=%u)",
			static_cast<unsigned int>(itemCount),
			INT32_MAX,
			static_cast<unsigned int>(ch->GetPlayerID()),
			static_cast<unsigned int>(itemVnum));
		return nullptr;
	}

	if (ch->HasSlotForItem(itemVnum) && db_clientdesc && db_clientdesc->GetSocket() != INVALID_SOCKET)
	{
		LPITEM pkItem = ch->AutoGiveItem(itemVnum, static_cast<ITEM_COUNT>(itemCount), -1, true, true);
		if (pkItem)
		{
			if (pkItem->GetOwner() != ch || pkItem->GetID() == 0)
			{
				ITEM_MANAGER::instance().RemoveItem(pkItem, "ITEMSHOP_UNOWNED_FALLBACK");
				pkItem = nullptr;
			}
			else
			{
				return pkItem;
			}
		}
	}

	const char* rawLogin = ch->GetDesc()->GetAccountTable().login;
	char szEscapedLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szEscapedLogin, sizeof(szEscapedLogin), rawLogin, std::strlen(rawLogin));

	char szWhy[64];
	if (txId > 0)
		std::snprintf(szWhy, sizeof(szWhy), "ITEMSHOP_TX_%llu", static_cast<unsigned long long>(txId));
	else
		strlcpy(szWhy, "ITEMSHOP_BUY", sizeof(szWhy));

	std::unique_ptr<SQLMsg> pkAwardMsg(DBManager::instance().DirectQuery(
		"INSERT INTO item_award (login, vnum, count, given_time, why, mall) VALUES ('%s', %u, %u, NOW(), '%s', %u)",
		szEscapedLogin, static_cast<unsigned int>(itemVnum), static_cast<unsigned int>(itemCount), szWhy, 1u));
	if (!pkAwardMsg || pkAwardMsg->uiSQLErrno != 0 || !pkAwardMsg->Get() || pkAwardMsg->Get()->uiAffectedRows != 1)
	{
		sys_err("ItemShop::GiveItem failed to insert into item_award (pid=%u, vnum=%u, count=%u)",
			static_cast<unsigned int>(ch->GetPlayerID()),
			static_cast<unsigned int>(itemVnum),
			static_cast<unsigned int>(itemCount));
		return nullptr;
	}

	if (outDeliveredToAward)
		*outDeliveredToAward = true;
	if (outAwardInsertId)
		*outAwardInsertId = pkAwardMsg->Get()->ullInsertID;

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_ITEM_AWARD"));
	return nullptr;
}

bool CItemShopManager::AddVIP(LPCHARACTER ch, BYTE premiumType, DWORD durationInHours)
{
	if (!ch)
		return false;

	if (!ch->GetDesc())
		return false;

	if (premiumType >= PREMIUM_MAX_NUM || durationInHours == 0)
		return false;

	DWORD aid = ch->GetDesc()->GetAccountTable().id;

	std::string premiumColumnStr[PREMIUM_MAX_NUM] = {
		"silver_expire",
		"gold_expire",
		"safebox_expire",
		"autoloot_expire",
		"fish_mind_expire",
		"marriage_fast_expire",
		"money_drop_rate_expire",
		"shop_expire",
		"premium_expire",
	};
	std::string column = premiumColumnStr[premiumType];

	std::string query = "UPDATE account SET " + column + " = CASE "
		"WHEN " + column + " <= NOW() THEN DATE_ADD(NOW(), INTERVAL " + std::to_string(durationInHours) + " HOUR) "
		"ELSE DATE_ADD(" + column + ", INTERVAL " + std::to_string(durationInHours) + " HOUR) "
		"END "
		"WHERE id = " + std::to_string(aid) + ";";

	std::unique_ptr<SQLMsg> pkMsg(DBManager::instance().AccountQuery("%s", query.c_str()));
	if (!pkMsg || pkMsg->uiSQLErrno != 0 || !pkMsg->Get() || pkMsg->Get()->uiAffectedRows == 0)
	{
		sys_err("ItemShop::AddVIP failed to update account (aid=%u, premiumType=%u, hours=%u)",
			static_cast<unsigned int>(aid), static_cast<unsigned int>(premiumType), static_cast<unsigned int>(durationInHours));
		return false;
	}

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ITEMSHOP_BUY_VIP_INFO"));
	return true;
}

