#ifndef __INCLUDE_HEADER_OFFLINESHOP_MANAGER__
#define __INCLUDE_HEADER_OFFLINESHOP_MANAGER__

#ifdef ENABLE_IKASHOP_RENEWAL
#define SUBTYPE_NOSET 255
#define OFFLINESHOP_DURATION_UPDATE_TIME PASSES_PER_SEC(60)
#define OFFLINESHOP_AUCTION_RAISE_PERCENTAGE 10 //to raise the offer will be 10% more than best offer

#include "packet.h"
#include "ikarus_shop.h"

namespace ikashop
{
	enum SearchConstant 
	{
		// ITEM TYPES
		SEARCH_SHOP_TYPE_WEAPON = 1,
		SEARCH_SHOP_TYPE_ARMOR,
		SEARCH_SHOP_TYPE_JEWEL,
		SEARCH_SHOP_TYPE_COSTUME,
		SEARCH_SHOP_TYPE_PET,
		SEARCH_SHOP_TYPE_MOUNT,
		SEARCH_SHOP_TYPE_ALCHEMY,
		SEARCH_SHOP_TYPE_BOOK,
		SEARCH_SHOP_TYPE_STONE,
		SEARCH_SHOP_TYPE_MINERALS,
		SEARCH_SHOP_TYPE_FISH,

		// WEAPON SUBTYPES
		SEARCH_SHOP_SUBTYPE_WEAPON_SWORD = 1,
		SEARCH_SHOP_SUBTYPE_WEAPON_TWOHANDED,
		SEARCH_SHOP_SUBTYPE_WEAPON_BLADE,
		SEARCH_SHOP_SUBTYPE_WEAPON_DAGGER,
		SEARCH_SHOP_SUBTYPE_WEAPON_BOW,
		SEARCH_SHOP_SUBTYPE_WEAPON_BELL,
		SEARCH_SHOP_SUBTYPE_WEAPON_FAN,
#ifdef ENABLE_WOLFMAN_CHARACTER
		SEARCH_SHOP_SUBTYPE_WEAPON_CLAW,
#endif

		// ARMOR SUBTYPES
		SEARCH_SHOP_SUBTYPE_ARMOR_WARRIOR = 1,
		SEARCH_SHOP_SUBTYPE_ARMOR_SURA,
		SEARCH_SHOP_SUBTYPE_ARMOR_ASSASSIN,
		SEARCH_SHOP_SUBTYPE_ARMOR_SHAMAN,
#ifdef ENABLE_WOLFMAN_CHARACTER
		SEARCH_SHOP_SUBTYPE_ARMOR_WOLFMAN,
#endif

		// JEWEL SUBTYPES
		SEARCH_SHOP_SUBTYPE_JEWEL_HELM = 1,
		SEARCH_SHOP_SUBTYPE_JEWEL_SHIELD,
		SEARCH_SHOP_SUBTYPE_JEWEL_EAR,
		SEARCH_SHOP_SUBTYPE_JEWEL_NECK,
		SEARCH_SHOP_SUBTYPE_JEWEL_WRIST,
		SEARCH_SHOP_SUBTYPE_JEWEL_SHOES,

		// COSTUME SUBTYPES
		SEARCH_SHOP_SUBTYPE_COSTUME_BODY = 1,
		SEARCH_SHOP_SUBTYPE_COSTUME_WEAPON,
		SEARCH_SHOP_SUBTYPE_COSTUME_HAIR,
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
		SEARCH_SHOP_SUBTYPE_COSTUME_SASH,
#endif

		// PET SUBTYPES
		SEARCH_SHOP_SUBTYPE_PET_EGGS = 1,
		SEARCH_SHOP_SUBTYPE_PET_SEALS,

		// SASH GRADE TYPES
		SEARCH_SHOP_SASH_GRADE_VALUE0 = 1,
		SEARCH_SHOP_SASH_GRADE_VALUE1,
		SEARCH_SHOP_SASH_GRADE_VALUE2,
		SEARCH_SHOP_SASH_GRADE_VALUE3,

		// ALCHEMY GRADE TYPES
		SEARCH_SHOP_DRAGON_SOUL_GRADE_NORMAL = 1,
		SEARCH_SHOP_DRAGON_SOUL_GRADE_BRILLIANT,
		SEARCH_SHOP_DRAGON_SOUL_GRADE_RARE,
		SEARCH_SHOP_DRAGON_SOUL_GRADE_ANCIENT,
		SEARCH_SHOP_DRAGON_SOUL_GRADE_LEGENDARY,
	};


	enum EShopActionWeight 
	{
		SHOP_ACTION_WEIGHT_REQUEST_EDIT_ITEM = 1,
		SHOP_ACTION_WEIGHT_EDIT_ITEM = 5,
		SHOP_ACTION_WEIGHT_CREATE_NEW = 10,
		SHOP_ACTION_WEIGHT_FORCE_CLOSE_SHOP = 5,
		SHOP_ACTION_WEIGHT_OPEN_SHOP = 1,
		SHOP_ACTION_WEIGHT_OPEN_MY_SHOP = 1,
		SHOP_ACTION_WEIGHT_BUY_ITEM = 5,
		SHOP_ACTION_WEIGHT_MOVE_SHOP = 5,
		SHOP_ACTION_WEIGHT_ADD_ITEM = 5,
		SHOP_ACTION_WEIGHT_REMOVE_ITEM = 5,
		SHOP_ACTION_WEIGHT_REMOVE_ALL_ITEM = 400,
		SHOP_ACTION_WEIGHT_FILTER_REQUEST = 3,
		SHOP_ACTION_WEIGHT_CREATE_OFFER = 5,
		SHOP_ACTION_WEIGHT_ACCEPT_OFFER = 5,
		SHOP_ACTION_WEIGHT_CANCEL_OFFER = 5,
		SHOP_ACTION_WEIGHT_SAFEBOX_OPEN = 1,
		SHOP_ACTION_WEIGHT_SAFEBOX_GET_ITEM = 5,
		SHOP_ACTION_WEIGHT_SAFEBOX_GET_VALUTES = 5,
		SHOP_ACTION_WEIGHT_AUCTION_LIST_REQUEST = 2,
		SHOP_ACTION_WEIGHT_AUCTION_OPEN_REQUEST = 1,
		SHOP_ACTION_WEIGHT_MY_ACUTION_OPEN_REQUEST = 1,
		SHOP_ACTION_WEIGHT_MY_AUCTION_CANCEL = 5,
		SHOP_ACTION_WEIGHT_AUCTION_CREATE = 5,
		SHOP_ACTION_WEIGHT_AUCTION_ADD_OFFER = 5,
		SHOP_ACTION_WEIGHT_PRICE_AVERAGE_REQUEST = 1,
		SHOP_ACTION_WEIGHT_SHOP_DECORATION_USE = 5,
		SHOP_ACTION_WEIGHT_SHOP_MOVE_ITEM = 5,
		SHOP_ACTION_WEIGHT_SHOP_CHANGE_NAME = 10,
		SHOP_ACTION_WEIGHT_SEARCH_ITEM = 100,
	};

	enum EShopDistance {
		GUEST_INTERRACT_DISTANCE = 5000,
		OWNER_INTERRACT_DISTANCE = 1500,
	};


	class CShopManager : public singleton<CShopManager>
	{
	public:

		using ITEM_HANDLE = std::shared_ptr<ikashop::CShopItem>;
		using OFFER_HANDLE = std::shared_ptr<ikashop::TOfferInfo>;
		using SHOP_HANDLE = std::shared_ptr<ikashop::CShop>;
		using SAFEBOX_HANDLE = std::shared_ptr<ikashop::CShopSafebox>;
		using AUCTION_HANDLE = std::shared_ptr<CAuction>;

		using SHOP_LIMIT_MAP = std::map<DWORD, TOfflineShopLimit>;
		using SHOPMAP = std::map<uint64_t, SHOP_HANDLE>;
		using SAFEBOXMAP = std::map<uint64_t, SAFEBOX_HANDLE>;
		using OFFERSMAPBUYER = std::map<uint64_t, std::vector<OFFER_HANDLE>>;
		using OFFERSMAP = std::map<uint64_t, OFFER_HANDLE>;
		using SEARCHTIMEMAP = std::map<uint64_t, DWORD>;
		using AUCTIONMAP = std::map<uint64_t, AUCTION_HANDLE>;

#ifdef EXTEND_IKASHOP_PRO
		using NOTIFICATION_MAP = std::unordered_map<uint64_t, std::vector<TNotificationInfo>>;
#endif

#ifdef ENABLE_IKASHOP_ENTITIES
		using ENTITY_HANDLE = std::shared_ptr<ShopEntity>;
		using ENTITY_MAP = std::unordered_map<DWORD, ENTITY_HANDLE>;
#endif
		
		CShopManager();
		~CShopManager();

		//booting
		SHOP_HANDLE PutsNewShop(const TShopInfo& info);
		void PutsAuction(const TAuctionInfo& auction);
		void PutsAuctionOffer(const TAuctionOfferInfo& offer);

		void PrepareShopSearchFilters();

		SHOP_HANDLE GetShopByOwnerID(uint64_t pid);
		SAFEBOX_HANDLE GetShopSafeboxByOwnerID(uint64_t pid);
		AUCTION_HANDLE GetAuctionByOwnerID(uint64_t pid);

		//offers
		bool PutsNewOffer(OFFER_HANDLE pInfo);


#ifdef ENABLE_IKASHOP_ENTITIES
	public:
		void CreateShopEntity(SHOP_HANDLE shop);
		void DestroyShopEntity(SHOP_HANDLE shop);			 
		void EncodeInsertShopEntity(const ShopEntity& shop, LPCHARACTER ch);
		void EncodeRemoveShopEntity(const ShopEntity& shop, LPCHARACTER ch);

#ifdef EXTEND_IKASHOP_PRO
	private:
		void _UpdateEntity(SHOP_HANDLE shop);
#endif
#endif


	public:
//packets exchanging db
//ITEMS
/*db*/	void SendShopBuyDBPacket(uint64_t buyerid, uint64_t ownerid, uint64_t itemid, bool success);
/*db*/	void SendShopLockBuyItemDBPacket(LPCHARACTER buyer, uint64_t ownerid, ITEM_HANDLE item, long long seenprice); //patch seen price check
			 
/*db*/	bool RecvShopLockedBuyItemDBPacket(uint64_t buyerid, uint64_t ownerid, uint64_t itemid);
/*db*/	bool RecvShopBuyDBPacket(uint64_t buyerid, uint64_t ownerid, uint64_t itemid, bool requester);
			 
/*db*/	void SendShopEditItemDBPacket(uint64_t ownerid, uint64_t itemid, const TPriceInfo& price);
/*db*/	bool RecvShopEditItemDBPacket(uint64_t ownerid, uint64_t itemid, const TPriceInfo& price);
			 
			 
/*db*/	void SendShopRemoveItemDBPacket(uint64_t ownerid, uint64_t itemid);
/*db*/	bool RecvShopRemoveItemDBPacket(uint64_t ownerid, uint64_t itemid, bool requester);
			 
			 
/*db*/	void SendShopAddItemDBPacket(uint64_t ownerid, const TShopItem& iteminfo);
/*db*/	bool RecvShopAddItemDBPacket(uint64_t ownerid, const TShopItem& iteminfo);
			 
//SHOPS 	 
/*db*/	void SendShopForceHardCloseDBPacket(uint64_t pid);
/*db*/	void SendShopForceSoftCloseDBPacket(uint64_t pid, bool gm);
/*db*/	bool RecvShopForceCloseDBPacket(uint64_t pid);
/*db*/	bool RecvShopExpiredDBPacket(uint64_t pid);
			 
/*db*/	void SendShopCreateNewDBPacket(const TShopInfo&, const TShopItem*);
/*db*/	bool RecvShopCreateNewDBPacket(const TShopInfo&, const TShopItem*);
			 
/*db*/	void SendShopChangeNameDBPacket(uint64_t ownerid, const char* name);
/*db*/	bool RecvShopChangeNameDBPacket(uint64_t ownerid, const char* name);

/*db*/	void SendShopOwnerOnlineDBPacket(uint64_t ownerID, const bool ownerOnlineState);
/*db*/	bool RecvShopOwnerOnlineDBPacket(uint64_t ownerID, const bool ownerOnlineState);
			 
//OFFER		 
/*db*/	void SendShopOfferNewDBPacket(const TOfferInfo& offer, ITEM_HANDLE item);
/*db*/	bool RecvShopOfferNewDBPacket(const TOfferInfo& offer);
			 
/*db*/	void SendShopOfferNotifiedDBPacket(uint64_t offerid, uint64_t ownerid);
/*db*/	bool RecvShopOfferNotifiedDBPacket(uint64_t offerid, uint64_t ownerid);
			 
/*db*/	void SendShopOfferAcceptDBPacket(const TOfferInfo& offer);
/*db*/	bool RecvShopOfferAcceptDBPacket(uint64_t offerid, uint64_t ownerid);
			 
/*db*/	void SendShopOfferCancelDBPacket(const TOfferInfo& offer, uint64_t requester);
/*db*/	bool RecvShopOfferCancelDBPacket(uint64_t offerid, uint64_t ownerid, bool removing);
			 
/*db*/	void SendShopSafeboxGetItemDBPacket(uint64_t ownerid, uint64_t itemid);
/*db*/	void SendShopSafeboxGetValutesDBPacket(uint64_t ownerid, const TValutesInfo& valutes);
/*db*/  bool SendShopSafeboxAddItemDBPacket(uint64_t ownerid, const CShopItem& item);
/*db*/	bool RecvShopSafeboxAddItemDBPacket(uint64_t ownerid, uint64_t itemid, const TShopPlayerItem& item);
/*db*/	bool RecvShopSafeboxAddValutesDBPacket(uint64_t ownerid, const TValutesInfo& valute);
/*db*/	bool RecvShopSafeboxLoadDBPacket(uint64_t ownerid, const TValutesInfo& valute, const std::vector<TShopPlayerItem>& items);
//patch 08-03-2020
/*db*/  bool RecvShopSafeboxExpiredItemDBPacket(uint64_t ownerid, uint64_t itemid);
/*db*/	bool RecvShopSafeboxGetItemConfirm(uint64_t ownerid, uint64_t itemid);

/*db*/	void SendShopLoadExpiredShop(uint64_t ownerid);
			 
//AUCTION	 
/*db*/	void SendAuctionCreateDBPacket(const TAuctionInfo& auction);
/*db*/	void SendAuctionAddOfferDBPacket(const TAuctionOfferInfo& offer);
			 
/*db*/	bool RecvAuctionCreateDBPacket(const TAuctionInfo& auction);
/*db*/	bool RecvAuctionAddOfferDBPacket(const TAuctionOfferInfo& offer);
/*db*/	bool RecvAuctionExpiredDBPacket(uint64_t id);
/*db*/	bool RecvAuctionOfferSeenDBPacket(uint64_t ownerid, uint64_t buyerid, long long price);

#ifdef EXTEND_IKASHOP_PRO
/*db*/	bool RecvNotificationLoadDBPacket(uint64_t owner, const std::vector<TNotificationInfo>& notifications);
/*db*/	bool RecvNotificationForwardDBPacket(uint64_t owner, const TNotificationInfo& notification);
/*db*/	void SendShopRestoreDurationDBPacket(uint64_t owner);
/*db*/	bool RecvShopRestoreDurationDBPacket(uint64_t owner);
#ifdef ENABLE_IKASHOP_ENTITIES
/*db*/	void RecvMoveShopEntityDBPacket(uint64_t owner, const TShopSpawn& spawn);
#endif
#endif
		void SendShopLockedSlotStateDBPacket(uint64_t pid, BYTE unlockCount, bool isPremium);
		void RecvShopLockedSlotStateDBPacket(uint64_t ownerID, BYTE unlockCount, bool isPremium);

//packets echanging clients
//SHOPS
///*cli.*/bool RecvShopCreateNewClientPacket(LPCHARACTER ch); // UNUSED
/*cli.*/bool RecvShopReopenClientPacket(LPCHARACTER ch, const char * name, BYTE bTimeIndex);
/*cli.*/bool RecvShopForceCloseClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvShopOpenClientPacket(LPCHARACTER ch, uint64_t ownerid);
/*cli.*/bool RecvShopOpenMyShopClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvShopBuyItemClientPacket(LPCHARACTER ch, uint64_t ownerid, uint64_t itemid, bool searching, long long seenprice); //patch seen price check
#ifdef ENABLE_IKASHOP_ENTITIES
/*cli.*/bool RecvShopClickEntityClientPacket(LPCHARACTER ch, DWORD evid);
/*cli.*/bool RecvFindMyShopClientPacket(LPCHARACTER ch);
#ifdef EXTEND_IKASHOP_PRO
/*cli.*/bool RecvMoveShopEntityClientPacket(LPCHARACTER ch);
#endif
#endif

		bool SearchItemsByCategory(DWORD category, ikashop::CShopManager::SHOP_HANDLE shop);
		bool RecvShopSearchItemClientPacket(LPCHARACTER ch, DWORD itemVnum, int socket0);

/*cli.*/void SendSearchResultDeleteClientPacket(LPCHARACTER ch, uint64_t itemid);
/*cli.*/void SendShopOpenClientPacket(LPCHARACTER ch, CShop* shop);
/*cli.*/void SendShopOpenMyShopClientPacket(LPCHARACTER ch);
/*cli.*/void SendShopOpenMyShopNoShopClientPacket(LPCHARACTER ch);
/*cli.*/void SendShopExpiredGuesting(uint64_t ownerid, LPCHARACTER ch);
			 
/*cli.*/void SendShopForceClosedClientPacket(uint64_t ownerid);
/*cli.*/void SendMyShopPositionClientPacket(LPCHARACTER ch, DWORD shopVID, long x, long y);


		//ITEMS
		void ProcessAddItemToShop(TShopItem& sitem, LPCHARACTER ch, LPITEM item, WORD destPos, const TPriceInfo price);

/*cli.*/bool RecvShopAddItemClientPacket(LPCHARACTER ch, const TItemPos& item, const TPriceInfo& price, int destPos);
/*cli.*/bool RecvShopRemoveItemClientPacket(LPCHARACTER ch, uint64_t itemid);
/*cli.*/bool RecvShopRemoveAllItemClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvShopEditItemClientPacket(LPCHARACTER ch, uint64_t itemid, const TPriceInfo& price);
/*cli.*/bool RecvShopRequestEditClientPacket(LPCHARACTER ch, bool isEdit);
/*cli.*/bool RecvShopChangeNameClientPacket(LPCHARACTER ch, const char * szName);

		//FILTER
/*cli.*/bool RecvShopFilterRequestClientPacket(LPCHARACTER ch, const TFilterInfo& filter);
/*cli.*/bool RecvShopSearchFillRequestClientPacket(LPCHARACTER ch);
/*cli.*/void SendShopFilterResultClientPacket(LPCHARACTER ch, const std::vector<TSearchResultInfo>& items);


		//OFFERS
/*cli.*/bool RecvShopCreateOfferClientPacket(LPCHARACTER ch, TOfferInfo offer);
/*cli.*/bool RecvShopAcceptOfferClientPacket(LPCHARACTER ch, uint64_t offerid);
/*cli.*/bool RecvShopCancelOfferClientPacket(LPCHARACTER ch, uint64_t offerid, uint64_t ownerid);
/*cli.*/bool RecvOfferListRequestPacket(LPCHARACTER ch);

		
		//SAFEBOX
/*cli.*/bool RecvShopSafeboxOpenClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvShopSafeboxGetItemClientPacket(LPCHARACTER ch, uint64_t itemid);
/*cli.*/bool RecvShopSafeboxGetValutesClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvShopSafeboxCloseClientPacket(LPCHARACTER ch);
		

		//AUCTION
/*cli.*/bool RecvAuctionListRequestClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvAuctionOpenRequestClientPacket(LPCHARACTER ch, uint64_t ownerid);
/*cli.*/bool RecvMyAuctionOpenRequestClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvMyAuctionCloseClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvMyAuctionCancelClientPacket(LPCHARACTER ch);
/*cli.*/bool RecvAuctionCreateClientPacket(LPCHARACTER ch, const TPriceInfo& startprice, const TItemPos& pos);
/*cli.*/bool RecvAuctionAddOfferClientPacket(LPCHARACTER ch, uint64_t ownerid, const TPriceInfo& price);
/*cli.*/bool RecvAuctionExitFromAuction(LPCHARACTER ch, uint64_t ownerid);
			 
/*cli.*/void SendAuctionListClientPacket(LPCHARACTER ch, const std::vector<TAuctionListElement>& auctionVec);
/*cli.*/void SendAuctionOpenAuctionClientPacket(LPCHARACTER ch, const CAuction& auction); 
/*cli.*/void SendAuctionOpenMyAuctionNoAuctionClientPacket(LPCHARACTER ch);
			 
			 
/*cli.*/void SendShopSafeboxRefreshClientPacket(LPCHARACTER ch, const TValutesInfo& valute, const std::vector<ITEM_HANDLE>& vec);
			 
/*cli.*/void RecvCloseMyShopBoardClientPacket(LPCHARACTER ch);
/*cli.*/void RecvCloseOfferListClientPacket(LPCHARACTER ch);
/*cli.*/void RecvCloseShopGuestClientPacket(LPCHARACTER ch);

#ifdef EXTEND_IKASHOP_PRO
/*cli.*/void SendNotificationListClientPacket(LPCHARACTER ch);
#endif
/*cli.*/void SendBoardCountersClientPacket(LPCHARACTER ch);


/*cli.*/void SendPopupMessage(LPCHARACTER ch, const std::string& message);

		//other
		void UpdateShopsDuration();
		void UpdateAuctionsDuration();

		//search time map (checking to avoid search abouse)
		void ClearSearchTimeMap();
		bool CheckSearchCooldown(uint64_t pid);
		bool CheckOfferCooldown(uint64_t pid);

		bool CheckSafeboxSize(LPCHARACTER ch);

		bool OpenOfflineShop(LPCHARACTER ch, const char * shopSign, TShopItemTable* pItemTable, BYTE bItemCount, BYTE bTimeIndex);

		bool IsNearShop(LPCHARACTER ch, SHOP_HANDLE shop, long maxDistance=GUEST_INTERRACT_DISTANCE);

		void Destroy();

		bool ParseShopName(const char* name, std::string& output);

		bool CanOpenOnMap(LPCHARACTER ch);
		void LoadShopLimits(const TOfflineShopLimit* limits, WORD size);

		void NotifyOwnerItemSold(LPCHARACTER ch, DWORD item_vnum, ITEM_COUNT item_count, YANG price);
		 

	private:
		void _RemoveShopOffers(SHOP_HANDLE shop);

	private:
		SHOP_LIMIT_MAP m_mapLimitShops;
		SHOPMAP m_mapShops;
		SAFEBOXMAP m_mapSafeboxs;
		OFFERSMAPBUYER m_mapOffersByBuyer;
		OFFERSMAP m_mapOffer;

		LPEVENT m_eventShopDuration;
		SEARCHTIMEMAP m_searchCooldownMap;
		SEARCHTIMEMAP m_offerCooldownMap;
		AUCTIONMAP m_mapAuctions;
#ifdef ENABLE_IKASHOP_ENTITIES
		ENTITY_MAP m_entityByVID;
#endif
#ifdef EXTEND_IKASHOP_PRO
		NOTIFICATION_MAP m_notificationMap;
#endif

		struct SOfflineShopFilter {
			DWORD itemVnum;
			int socket0;
		};
		std::map<DWORD, std::vector<SOfflineShopFilter>> m_shopSearchFilters;
	};

	ikashop::CShopManager& GetManager();
}

#endif
#endif //__INCLUDE_HEADER_OFFLINESHOP_MANAGER__
