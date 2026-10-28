#ifndef __INCLUDE_HEADER_OFFLINESHOP_CACHE__
#define __INCLUDE_HEADER_OFFLINESHOP_CACHE__

#ifdef ENABLE_IKASHOP_RENEWAL
#include <numeric>
namespace ikashop
{
	struct SQueryInfoCreateShop;

	class CShopCache
	{
	public:
		struct TShopCacheItemInfo : TShopItem {
			bool lock = false;
		};

		using ITEM_CACHE_HANDLE = std::shared_ptr<TShopCacheItemInfo>;
		using SHOPCACHE_MAP = std::map<uint64_t, ITEM_CACHE_HANDLE>;
		struct TShopCacheInfo 
		{
			DWORD duration;
			SHOPCACHE_MAP itemsmap;
			std::string name;
			std::string ownerName;
#ifdef ENABLE_IKASHOP_ENTITIES
			TShopSpawn spawn;
#endif
			BYTE unlockCount;
			bool isPremium;
		};
		using SHOP_HANDLE = std::shared_ptr<TShopCacheInfo>;

	public:
		CShopCache();
		~CShopCache();

		SHOP_HANDLE	Get(uint64_t dwOwnerID) const;
		bool AddItem(const TShopCacheItemInfo& rItem, const int queryType);
		bool RemoveItem(uint64_t dwOwnerID, uint64_t dwItemID);
		void DeleteItem(uint64_t owner, uint64_t itemid);
		bool SellItem(uint64_t dwOwnerID, uint64_t dwItemID, bool updateQuery = true);
		bool LockSellItem(uint64_t dwOwnerID, uint64_t dwItemID, long long TotalPriceSeen); //patch seen price check
		bool UnlockSellItem(uint64_t dwOwnerID, uint64_t dwItemID);//topatch
		bool EditItem(uint64_t dwOwnerID, uint64_t dwItemID, const TPriceInfo& rItemPrice);
		bool CloseShop(uint64_t dwOwnerID);

		bool CreateShop(uint64_t dwOwnerID, DWORD dwDuration, const char* szShopName, const char* szOwnerName
#ifdef ENABLE_IKASHOP_ENTITIES
			, const TShopSpawn& spawn, const TShopItem * item
#endif
			, BYTE unlockCount, bool isPremium
		);

		bool ChangeShopName(uint64_t dwOwnerID, const char* szName);

		// on boot
		bool PutItem(uint64_t dwOwnerID, uint64_t dwItemID, const TShopCacheItemInfo& rItem);
		bool PutShop(uint64_t dwOwnerID, DWORD dwDuration, const char* szShopName, const char* szOwnerName
#ifdef ENABLE_IKASHOP_ENTITIES
			, const TShopSpawn& spawn
#endif
			, BYTE unlockCount, bool isPremium
		);

		DWORD GetCount() const {return static_cast<DWORD>(m_shopsMap.size());}
		void EncodeCache(CPeer* peer) const;
		DWORD GetItemCount() const;

		void ShopDurationProcess();
		void UpdateDurationQuery(uint64_t dwOwnerID, const TShopCacheInfo& rShop );

		ITEM_CACHE_HANDLE GetItem(uint64_t owner, uint64_t item);

#ifdef ENABLE_IKASHOP_ENTITIES
#ifdef EXTEND_IKASHOP_PRO
		bool MoveShopEntity(uint64_t owner, const TShopSpawn& spawn);
#endif
#endif

		bool SetLockedSlotState(uint64_t owner, BYTE unlockCount, bool isPremium);

	private:
		std::map<uint64_t, SHOP_HANDLE> m_shopsMap;
	};


	class CSafeboxCache
	{
	public:
		struct TSafeboxCacheInfo {
			TValutesInfo valutes;
			std::map<uint64_t, TShopPlayerItem> itemsmap;
			std::map<uint64_t, TShopPlayerItem> lockedmap;
		};

		using SAFEBOX_HANDLE = std::shared_ptr<TSafeboxCacheInfo>;
		using CACHEMAP = std::map<uint64_t, SAFEBOX_HANDLE>;

	public:
		CSafeboxCache();
		~CSafeboxCache();

		SAFEBOX_HANDLE Get(uint64_t dwOwnerID) const;

		bool PutSafebox(uint64_t dwOwnerID, const TSafeboxCacheInfo& rSafebox);
		bool PutItem(uint64_t dwOwnerID, uint64_t dwItem, const TShopPlayerItem& item);

		bool DeleteItem(uint64_t dwOwner, uint64_t dwItemID);
		bool LockItem(uint64_t dwOwner, uint64_t dwItemID);
		bool RemoveLockedItem(uint64_t dwOwner, uint64_t dwItemID);
		bool UnlockItem(uint64_t dwOwner, uint64_t dwItemID);
		bool AddItem(uint64_t dwOwnerID, uint64_t dwItemID, const TShopPlayerItem& item);

		bool AddValutes(uint64_t dwOwnerID, const TValutesInfo& val);
		bool RemoveValutes(uint64_t dwOwnerID, const TValutesInfo& val);
		void AddValutesAsQuery(uint64_t dwOwnerID, const TValutesInfo& val);

		SAFEBOX_HANDLE CreateSafebox(uint64_t dwOwnerID);
		DWORD GetCount() const {return static_cast<DWORD>(m_safeboxMap.size());}
		DWORD GetItemCount() const;

		SAFEBOX_HANDLE LoadSafebox(uint64_t dwPID);

		//patch 08-03-2020
		void ItemExpirationProcess();

	private:
		CACHEMAP m_safeboxMap;

	};


	class COfferCache
	{
	public:
		using OFFER_HANDLE = std::shared_ptr<TOfferInfo>;
		using CACHEMAP = std::map<uint64_t, OFFER_HANDLE>;
		using FINDOFFERBYSHOP = std::map<uint64_t, std::vector<OFFER_HANDLE>>;

	public:
		COfferCache();
		~COfferCache();

		bool Puts(const TOfferInfo& rOffer);
		bool AddOffer(const TOfferInfo& rOffer);
		bool NotifiedOffer(uint64_t dwOfferID);
		bool AcceptOffer(uint64_t dwOfferID);
		bool CancelOffer(uint64_t dwOfferID, uint64_t dwOwnerID);
		bool RemoveOffersByShopOwner(uint64_t dwShopOwnerID);

		OFFER_HANDLE Get(uint64_t dwOfferID) const;
		std::vector<OFFER_HANDLE> GetOffersByShopOwner(uint64_t dwOwnerID) const;
		std::vector<OFFER_HANDLE> GetOffersByItemID(uint64_t dwOwnerID, uint64_t dwItemID) const;

		DWORD GetCount() {return static_cast<DWORD>(m_mapOffersByBuyer.size());}
		void EncodeCache(CPeer* peer) const;

	private:
		CACHEMAP m_mapOffersByBuyer;
		FINDOFFERBYSHOP m_findOffersByShop;
	};

	class CAuctionCache 
	{
	public:
		using AUCTION_HANDLE = std::shared_ptr<TAuctionInfo>;
		using AUCTION_OFFER_HANDLE = std::shared_ptr<TAuctionOfferInfo>;
		using AUCTION_OFFER_VEC = std::vector<AUCTION_OFFER_HANDLE>;
		using AUCTION_MAP = std::map<uint64_t, std::shared_ptr<TAuctionInfo>>;
		using AUCTION_OFFER_MAP = std::map<uint64_t, AUCTION_OFFER_VEC>;

	public:
		CAuctionCache();
		~CAuctionCache();

		bool PutsAuction(const TAuctionInfo& auction);
		bool PutsAuctionOffer(const TAuctionOfferInfo& auctionOffer);

		bool AddAuction(const TAuctionInfo& auction);
		bool AddOffer(const TAuctionOfferInfo& auctionOffer, bool quering = true);
		bool ExpiredAuction(uint64_t dwOwnerID);
		bool AuctionDurationProcess();

		AUCTION_OFFER_HANDLE GetBestBuyerOffer(uint64_t dwOwnerID);

		AUCTION_HANDLE Get(uint64_t dwOwnerID);
		AUCTION_OFFER_VEC GetOffers(uint64_t dwOwnerID);

		DWORD GetCount();
		DWORD GetOffersCount();

		void EncodeCache(CPeer* peer);

	private:
		void _UpdateDuration(const TAuctionInfo& auction);

	private:
		AUCTION_MAP m_mapAuction;
		AUCTION_OFFER_MAP m_mapAuctionOffer;
	};



	//QUERY INFO
	struct SQueryInfoCreateShop {
		uint64_t owner;
		DWORD duration;
		BYTE unlockCount;
		bool isPremium;
		std::string name;
		std::string ownerName;
#ifdef ENABLE_IKASHOP_ENTITIES
		TShopSpawn spawn;
#endif
		TShopItem items[SHOP_PLAYER_HOST_ITEM_MAX_NUM];
	};
	using SQueryInfoAddItem = CShopCache::TShopCacheItemInfo;
	using SQueryInfoSafeboxAddItem = TShopPlayerItem;
	using SQueryInfoOfferAdd = TOfferInfo;
}





#endif
#endif //__INCLUDE_HEADER_OFFLINESHOP_CACHE__
