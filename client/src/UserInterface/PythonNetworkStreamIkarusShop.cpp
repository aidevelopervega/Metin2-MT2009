#include "StdAfx.h"
#ifdef ENABLE_IKASHOP_RENEWAL
#include "PythonNetworkStream.h"
#include "Packet.h"
#include "PythonIkarusShop.h"
#include "PythonMiniMap.h"

bool CPythonNetworkStream::RecvOfflineshopPacket()
{
	TPacketGCNewOfflineshop pack;
	if(!Recv(sizeof(pack), &pack))
		return false;

	switch (pack.subheader)
	{
		case ikashop::SUBHEADER_GC_SHOP_OPEN:
			return RecvOfflineshopShopOpen();

		case ikashop::SUBHEADER_GC_SHOP_OPEN_OWNER:
			return RecvOfflineshopShopOpenOwner();

		case ikashop::SUBHEADER_GC_SHOP_OPEN_OWNER_NO_SHOP:
			return RecvOfflineshopShopOpenOwnerNoShop();

		case ikashop::SUBHEADER_GC_SHOP_POSITION:
			return RecvOfflineShopPosition();

		case ikashop::SUBHEADER_GC_SHOP_EXPIRED_GUESTING:
			return RecvOfflineshopShopExpiredGuesting();

		case ikashop::SUBHEADER_GC_SHOP_REMOVE_ITEM_OWNER:
			return RecvOfflineshopShopRemoveItem(true);

		case ikashop::SUBHEADER_GC_SHOP_REMOVE_ITEM_GUEST:
			return RecvOfflineshopShopRemoveItem(false);

		case ikashop::SUBHEADER_GC_SHOP_EDIT_ITEM_GUEST:
			return RecvOfflineshopShopGuestEditItem();

		case ikashop::SUBHEADER_GC_SHOP_EDIT_ITEM_OWNER:
			return RecvOfflineshopShopOwnerEditItem();

		case ikashop::SUBHEADER_GC_SHOP_FILTER_RESULT:
			return RecvOfflineshopShopFilterResult();

		case ikashop::SUBHEADER_GC_OFFER_LIST:
			return RecvOfflineshopOfferList();

		case ikashop::SUBHEADER_GC_SHOP_SAFEBOX_REFRESH:
			return RecvOfflineshopShopSafeboxRefresh();

		case ikashop::SUBHEADER_GC_SHOP_SAFEBOX_REMOVE_ITEM:
			return RecvOfflineshopShopSafeboxRemoveItem();

		case ikashop::SUBHEADER_GC_SHOP_SAFEBOX_ADD_ITEM:
			return RecvOfflineshopShopSafeboxAddItem();

		case ikashop::SUBHEADER_GC_AUCTION_LIST:
			return RecvOfflineshopAuctionList();

		case ikashop::SUBHEADER_GC_OPEN_MY_AUCTION:
			return RecvOfflineshopOpenMyAuction();

		case ikashop::SUBHEADER_GC_OPEN_MY_AUCTION_NO_AUCTION:
			return RecvOfflineshopOpenMyAuctionNoAuction();

		case ikashop::SUBHEADER_GC_OPEN_AUCTION:
			return RecvOfflineshopOpenAuction();

		case ikashop::SUBHEDAER_GC_POPUP_MESSAGE:
			return RecvOfflineshopPopupMessage();

#ifdef ENABLE_IKASHOP_ENTITIES
		case ikashop::SUBHEADER_GC_INSERT_SHOP_ENTITY:
			return RecvOfflineshopInsertEntity();

		case ikashop::SUBHEADER_GC_REMOVE_SHOP_ENTITY:
			return RecvOfflineshopRemoveEntity();
#endif

		case ikashop::SUBHEADER_GC_BOARD_COUNTERS:
			return RecvOfflineshopBoardCounters();

#ifdef EXTEND_IKASHOP_PRO
		case ikashop::SUBHEADER_GC_NOTIFICATION_LIST:
			return RecvOfflineshopNotificationList();
#endif

		case ikashop::SUBHEADER_GC_SEARCH_RESULT_DELETE:
			return RecvOfflineshopSearchResultDelete();

		case ikashop::SUBHEADER_GC_NOTIFY_ITEM_SOLD:
			return RecvOfflineshopNotifyItemSold();

		case ikashop::SUBHEADER_GC_SHOP_SEARCH:
			return RecvOfflineshopSearchItem();

		default:
			TraceError("UNKNOWN OFFLINESHOP SUBHEADER : %d ",pack.subheader);
			return false;
	}
}

bool CPythonNetworkStream::RecvOfflineshopShopOpen()
{
	ikashop::TSubPacketGCShopOpen subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<ikashop::TShopItem> items;

	items.resize(subpack.shop.count);
	if(subpack.shop.count > 0)
		if(!Recv(sizeof(items[0])*items.size(), items.data()))
			return false;

	CPythonIkarusShop::Instance().OpenShop(subpack.vid, subpack.shop, items, subpack.isOwnerOnline);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopOpenOwner()
{
	ikashop::TSubPacketGCShopOpenOwner subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<ikashop::TShopItem> items;
	items.resize(subpack.shop.count);
	
	if(subpack.shop.count!=0)
		if(!Recv(sizeof(items[0]) * subpack.shop.count, items.data()))
			return false;

	CPythonIkarusShop::Instance().OpenShopOwner(subpack.shop, items, subpack.tax);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopOpenOwnerNoShop()
{
	CPythonIkarusShop::instance().OpenShopOwnerNoShop();
	return true;
}

bool CPythonNetworkStream::RecvOfflineShopPosition()
{
	ikashop::TSubPacketGCShopPosition subpack;
	if (!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().ShowMyShopPosition(subpack.vid, subpack.x, subpack.y);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopExpiredGuesting()
{
	ikashop::TSubPacketGCShopExpiredGuesting subpack;
	if(!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().ShopExpiredGuesting(subpack.ownerid);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopRemoveItem(bool owner)
{
	ikashop::TSubPacketGCShopRemoveItem subpack;
	if(!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().ShopRemoveItem(owner, subpack.itemid);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopGuestEditItem()
{
	ikashop::TSubPacketGCShopEditItem subpack;
	if (!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().ShopGuestEditItem(subpack.itemid, subpack.price);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopOwnerEditItem()
{
	ikashop::TSubPacketGCShopEditItem subpack;
	if (!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().ShopOwnerEditItem(subpack.itemid, subpack.price);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopFilterResult()
{
	ikashop::TSubPacketGCShopFilterResult subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<ikashop::TSearchResultInfo> items(static_cast<size_t>(subpack.count));
	if(subpack.count != 0)
		if(!Recv(items.size() * sizeof(items[0]), items.data()))
			return false;

	CPythonIkarusShop::Instance().ShopFilterResult(items);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopOfferList() //offlineshop-updated 03/08/19
{
	ikashop::TSubPacketGCShopOfferList subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	
	std::vector<ikashop::TOfferListElement> vec;
	vec.resize(subpack.offercount);

	if (subpack.offercount != 0)
		if (!Recv(sizeof(ikashop::TOfferListElement)*subpack.offercount, &vec[0]))
			return false;

	CPythonIkarusShop::instance().OfferListReceived(vec);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopSafeboxRefresh()
{
	ikashop::TSubPacketGCShopSafeboxRefresh subpack;
	if(!Recv(sizeof(subpack), &subpack))
		return false;

	std::vector<ikashop::TShopPlayerItem> items;
	items.resize(subpack.itemcount);

	if(subpack.itemcount != 0)
		if (!Recv(sizeof(ikashop::TShopPlayerItem) * items.size(), &items[0]))
			return false;

	CPythonIkarusShop::instance().SafeboxRefresh(subpack.valute, items);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopSafeboxRemoveItem()
{
	ikashop::TSubPacketGCShopSafeboxRemoveItem subpack{};
	if(!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().SafeboxRemoveItem(subpack.itemid);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopShopSafeboxAddItem()
{
	ikashop::TSubPacketGCShoppingSafeboxAddItem subpack{};
	if (!Recv(sizeof(subpack), &subpack))
		return false;

	CPythonIkarusShop::instance().SafeboxAddItem(subpack.item);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopAuctionList()
{
	ikashop::TSubPacketGCAuctionList subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<ikashop::TAuctionListElement> vector;
	vector.resize(subpack.count);

	if(subpack.count!=0)
		if(!Recv(sizeof(vector[0]) * vector.size(), &vector[0]))
			return false;

	CPythonIkarusShop::instance().AuctionList(vector);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopOpenMyAuction()
{
	ikashop::TSubPacketGCAuctionOpen subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<ikashop::TAuctionOfferInfo> offers;
	offers.resize(subpack.offercount);

	if(subpack.offercount!= 0)
		if(!Recv(sizeof(offers[0]) * offers.size() , &offers[0]))
			return false;
	CPythonIkarusShop::instance().MyAuctionOpen(subpack.auction, offers);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopOpenMyAuctionNoAuction()
{
	CPythonIkarusShop::instance().OpenMyAuctionNoAuction();
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopOpenAuction()
{
	ikashop::TSubPacketGCAuctionOpen subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	std::vector<ikashop::TAuctionOfferInfo> offers;
	offers.resize(subpack.offercount);

	if(subpack.offercount!= 0)
		if(!Recv(sizeof(offers[0]) * offers.size() , &offers[0]))
			return false;

	CPythonIkarusShop::instance().AuctionOpen(subpack.auction, offers);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopPopupMessage()
{
	// receiving string
	ikashop::TSubPacketGCPopupMessage pack;
	if (!Recv(sizeof(pack), &pack))
		return false;

	// safely terminating string
	pack.localeString[pack.MESSAGE_LEN-1] = 0;

	CPythonIkarusShop::instance().PopupMessage(pack.localeString);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopSearchResultDelete()
{
	// getting itemid
	DWORD itemid = 0;
	if (!Recv(sizeof(itemid), &itemid))
		return false;

	CPythonIkarusShop::instance().DeleteSearchResult(itemid);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopNotifyItemSold()
{
	ikashop::TSubPacketGCShopNotifyItemSold packet;
	if (!Recv(sizeof(packet), &packet))
		return false;

	// vnum, count, price
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "OnMyShopBuy", Py_BuildValue("(iii)",
		packet.itemVnum, packet.count, packet.price));
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopSearchItem()
{
	ikashop::TSubPacketGCShopSearchItem packet;
	if (!Recv(sizeof(packet), &packet))
		return false;

	std::vector<ikashop::TSubPacketGCShopSearchItemShop> buffer;
	buffer.resize(packet.itemCount);

	if (!Recv(sizeof(ikashop::TSubPacketGCShopSearchItemShop) * packet.itemCount, buffer.data()))
		return false;

	CPythonIkarusShop::instance().ClearFoundShopMap();

	for (auto foundShop : buffer)
	{
		//Tracenf("pokazuje shopa vid %d x %d y %d", foundShop.shopVid, foundShop.x, foundShop.y);
		CPythonIkarusShop::instance().ShowFoundShopPosition(foundShop.shopVid, foundShop.x, foundShop.y);
	}

	return true;
}

bool CPythonNetworkStream::RecvOfflineshopBoardCounters()
{
	ikashop::TSubPacketGCBoardCounters subpack;
	if (!Recv(sizeof(subpack), &subpack))
		return false;

#ifdef EXTEND_IKASHOP_PRO
	CPythonIkarusShop::Instance().RefreshBoardCounters(subpack.safebox, subpack.auction, subpack.offers, subpack.notification);
#else
	CPythonIkarusShop::Instance().RefreshBoardCounters(subpack.safebox, subpack.auction, subpack.offers);
#endif
	return true;
}

#ifdef EXTEND_IKASHOP_PRO
bool CPythonNetworkStream::RecvOfflineshopNotificationList()
{
	ikashop::TSubPacketGCNotificationList subpack;
	if (!Recv(sizeof(subpack), &subpack))
		return false;

	std::vector<ikashop::TNotificationInfo> notifications;
	notifications.resize(subpack.count);

	if(subpack.count != 0)
		if(!Recv(notifications.size() * sizeof(ikashop::TNotificationInfo), notifications.data()))
			return false;

	CPythonIkarusShop::Instance().SetNotificationList(notifications);
	return true;
}
#endif

void CPythonNetworkStream::AppendIkarusShopAdBoard(DWORD dwVID, const char * szShopName)
{
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME],
		"BINARY_OfflineShop_Appear",
		Py_BuildValue("(is)", dwVID, szShopName)
	);
}

void CPythonNetworkStream::RemoveIkarusShopAdBoard(DWORD dwVID)
{
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME],
		"BINARY_OfflineShop_Disappear",
		Py_BuildValue("(i)", dwVID)
	);
}

void CPythonNetworkStream::ToggleIkarusShopAdBoardVisibility(DWORD dwVID, bool state)
{
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME],
		"BINARY_OfflineShop_ToggleVisibility",
		Py_BuildValue("(ii)", dwVID, state)
	);
}

#ifdef ENABLE_IKASHOP_ENTITIES
bool CPythonNetworkStream::RecvOfflineshopInsertEntity()
{
	ikashop::TSubPacketGCInsertShopEntity subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	CPythonIkarusShop::Instance().InsertEntity(subpack.vid, subpack.type, subpack.name, subpack.ownerName, subpack.x, subpack.y, subpack.z);
	AppendIkarusShopAdBoard(subpack.vid, subpack.name);
	return true;
}

bool CPythonNetworkStream::RecvOfflineshopRemoveEntity()
{
	ikashop::TSubPacketGCRemoveShopEntity subpack;
	if(!Recv(sizeof(subpack) , &subpack))
		return false;

	CPythonIkarusShop::Instance().RemoveEntity(subpack.vid);
	RemoveIkarusShopAdBoard(subpack.vid);

	if (CPythonIkarusShop::instance().GetFoundMyShopVID() == subpack.vid)
	{
		CPythonBackground::Instance().DeleteTargetEffect(OFFLINESHOP_MYSHOP_TARGET_INDEX);
		CPythonMiniMap::instance().DeleteTarget(OFFLINESHOP_MYSHOP_TARGET_INDEX);
	}
	
	if (CPythonIkarusShop::instance().IsFoundShopFromSearchItem(subpack.vid))
	{
		CPythonBackground::instance().DeleteTargetEffect(CPythonIkarusShop::instance().GetFoundShopFromSearchTargetId(subpack.vid));
	}

	return true;
}

void CPythonNetworkStream::SendIkarusShopOnClickShopEntity(DWORD dwPickedShopVID)
{
	if (CPythonIkarusShop::instance().GetFoundMyShopVID() == dwPickedShopVID)
	{
		CPythonBackground::Instance().DeleteTargetEffect(OFFLINESHOP_MYSHOP_TARGET_INDEX);
		CPythonMiniMap::instance().DeleteTarget(OFFLINESHOP_MYSHOP_TARGET_INDEX);
	}
	else if (CPythonIkarusShop::instance().IsFoundShopFromSearchItem(dwPickedShopVID))
	{
		CPythonIkarusShop::instance().SetFoundShopViewed(dwPickedShopVID);
	}

	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_CLICK_ENTITY;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGShopClickEntity);

	ikashop::TSubPacketCGShopClickEntity subpack;
	subpack.vid = dwPickedShopVID;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ", pack.subheader);
}

void CPythonNetworkStream::SendIkarusFindMyShop()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_FIND_MY_SHOP;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}

#ifdef EXTEND_IKASHOP_PRO
void CPythonNetworkStream::SendIkarusShopMoveShopEntity()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_MOVE_SHOP_ENTITY;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}
#endif
#endif

void CPythonNetworkStream::SendIkarusShopReopenShop(const char* name, BYTE bTimeIndex)
{
	TPacketCGNewOfflineShop pack{};
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_REOPEN;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGShopReopen);

	ikashop::TSubPacketCGShopReopen subpack;
	strncpy(subpack.name, name, OFFLINE_SHOP_NAME_MAX_LEN);
	subpack.bTimeIndex = bTimeIndex;

	if (!Send(pack))
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopForceCloseShop()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_FORCE_CLOSE;
	pack.size		= sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopRequestShopList()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_REQUEST_SHOPLIST;
	pack.size		= sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopOpenShop(DWORD dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_OPEN;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGShopOpen);

	ikashop::TSubPacketCGShopOpen subpack;
	subpack.ownerid = dwOwnerID;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopOpenShopOwner()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_OPEN_OWNER;
	pack.size		= sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}


void CPythonNetworkStream::SendIkarusShopBuyItem(DWORD dwOwnerID, DWORD dwItemID, bool isSearch, long long TotalPriceSeen) //patch seen price check
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_BUY_ITEM;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGShopBuyItem);

	ikashop::TSubPacketCGShopBuyItem subpack;
	subpack.itemid = dwItemID;
	subpack.ownerid = dwOwnerID;
	subpack.searching = isSearch;

	//patch seen price check
	subpack.seenprice = TotalPriceSeen;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopAddItem(ikashop::TShopItemInfo& itemInfo)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_ADD_ITEM;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGAddItem);

	ikashop::TSubPacketCGAddItem subpack;
	subpack.pos = itemInfo.pos;
	subpack.price = itemInfo.price;
	subpack.destpos = itemInfo.destpos;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopRemoveItem(DWORD dwItemID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_REMOVE_ITEM;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGRemoveItem);

	ikashop::TSubPacketCGRemoveItem subpack;
	subpack.itemid	= dwItemID;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopRemoveAllItem()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_REMOVE_ALL_ITEM;
	pack.size = sizeof(pack);

	ikashop::TSubPacketCGRemoveItem subpack;
	subpack.itemid = 0;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendOfflineShopEditItem(DWORD dwItemID, const ikashop::TPriceInfo& price)
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_EDIT_ITEM;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGEditItem);

	ikashop::TSubPacketCGEditItem subpack;
	subpack.itemid = dwItemID;
	subpack.price = price;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusSearchItem(DWORD itemVnum, int socket0)
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_SEARCH;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGShopSearchItem);

	ikashop::TSubPacketCGShopSearchItem subpack{};
	subpack.itemVnum = itemVnum;
	subpack.socket0 = socket0;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusRequestEdit(bool state)
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_REQUEST_EDIT;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGShopRequestEdit);

	ikashop::TSubPacketCGShopRequestEdit subpack{};
	subpack.bIsEdit = state;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopFilterRequest(const ikashop::TFilterInfo& filter)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_FILTER_REQUEST;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGFilterRequest);

	ikashop::TSubPacketCGFilterRequest subpack;
	subpack.filter = filter;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopSearchFillRequest()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_SEARCH_FILL_REQUEST;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopOfferCreate(const ikashop::TOfferInfo& offer)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_OFFER_CREATE;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGOfferCreate);

	ikashop::TSubPacketCGOfferCreate subpack;
	subpack.offer = offer;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopOfferAccept(DWORD dwOfferID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_OFFER_ACCEPT;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGOfferAccept);

	ikashop::TSubPacketCGOfferAccept subpack;
	subpack.offerid	= dwOfferID;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopOfferCancel(DWORD dwOfferID, DWORD dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_OFFER_CANCEL;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGOfferCancel);

	ikashop::TSubPacketCGOfferCancel subpack;
	subpack.offerid	= dwOfferID;
	subpack.ownerid	= dwOwnerID;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopOfferListRequest()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_REQUEST_OFFER_LIST;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopSafeboxOpen()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_SAFEBOX_OPEN;
	pack.size		= sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopSafeboxGetItem(DWORD dwItemID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_SAFEBOX_GET_ITEM;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGShopSafeboxGetItem);

	ikashop::TSubPacketCGShopSafeboxGetItem subpack;
	subpack.itemid = dwItemID;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if(!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack ",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopSafeboxGetValutes()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_SAFEBOX_GET_VALUTES;
	pack.size = sizeof(pack);

	if (!Send(pack))
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
}

void CPythonNetworkStream::SendIkarusShopSafeboxClose()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_SHOP_SAFEBOX_CLOSE;
	pack.size		= sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopAuctionListRequest()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_AUCTION_LIST_REQUEST;
	pack.size		= sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopAuctionOpen(DWORD dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_AUCTION_OPEN_REQUEST;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGAuctionOpenRequest);

	ikashop::TSubPacketCGAuctionOpenRequest subpack;
	subpack.ownerid = dwOwnerID;


	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
	
	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopAuctionAddOffer(DWORD dwOwnerID, const ikashop::TPriceInfo& price)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_AUCTION_ADD_OFFER;
	pack.size		= sizeof(pack) + sizeof(ikashop::TSubPacketCGAuctionAddOffer);

	ikashop::TSubPacketCGAuctionAddOffer subpack;
	subpack.ownerid = dwOwnerID;
	subpack.price = price;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopAuctionExitFrom(DWORD dwOwnerID)
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_EXIT_FROM_AUCTION;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGAuctionExitFrom);

	ikashop::TSubPacketCGAuctionExitFrom subpack;
	subpack.ownerid = dwOwnerID;
	
	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopAuctionCreate(const TItemPos& pos, const ikashop::TPriceInfo& price)
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader	= ikashop::SUBHEADER_CG_CREATE_AUCTION;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGAuctionCreate);

	ikashop::TSubPacketCGAuctionCreate subpack;
	subpack.pos = pos;
	subpack.startprice = price;

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopChangeName(const char* stName)
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_SHOP_CHANGE_NAME;
	pack.size = sizeof(pack) + sizeof(ikashop::TSubPacketCGChangeName);

	ikashop::TSubPacketCGChangeName subpack;
	strncpy(subpack.name, stName, sizeof(subpack.name));

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}

	if (!Send(subpack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - subpack", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopMyAuctionOpen()
{
	TPacketCGNewOfflineShop pack{};
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_MY_AUCTION_OPEN_REQUEST;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopMyAuctionClose()
{
	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_MY_AUCTION_CLOSE;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusShopMyAuctionCancel()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_MY_AUCTION_CANCEL;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusMyShopCloseBoard()
{
	if(m_strPhase != "Game")
		return;

	TPacketCGNewOfflineShop pack;
	pack.header	= HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_CLOSE_MY_SHOP_BOARD;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack",pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusOffersListClose()
{
	if (m_strPhase != "Game")
		return;

	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_CLOSE_OFFER_LIST;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusCloseShopGuestBoard()
{
	if (m_strPhase != "Game")
		return;

	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_CLOSE_SHOP_GUEST;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}

#ifdef EXTEND_IKASHOP_PRO
void CPythonNetworkStream::SendIkarusNotificationListRequest()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_NOTIFICATION_LIST_REQUEST;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}

void CPythonNetworkStream::SendIkarusNotificationListClose()
{
	TPacketCGNewOfflineShop pack;
	pack.header = HEADER_CG_NEW_OFFLINESHOP;
	pack.subheader = ikashop::SUBHEADER_CG_NOTIFICATION_LIST_CLOSE;
	pack.size = sizeof(pack);

	if (!Send(pack))
	{
		TraceError("CANNOT SEND OFFLINESHOP PACKET : SUBHEADER %d - pack", pack.subheader);
		return;
	}
}
#endif


#endif

// Files shared by GameCore.top
