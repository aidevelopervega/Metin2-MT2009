
class CItemShopManager : public singleton<CItemShopManager>
{
	typedef std::set<DWORD> PLAYER_SET;
public:
	struct PlayerAccountBalance {
		int64_t dwDragonCoins = 0;
		int64_t dwDragonMarks = 0;
	};

	enum ECurrency {
		CURRENCY_DRAGON_COIN = 1,
		CURRENCY_DRAGON_MARK = 2,
	};

	enum ETransactionState {
		TX_STATE_INITIATED = 0,
		TX_STATE_CHARGED = 1,
		TX_STATE_DELIVERED = 2,
		TX_STATE_REFUNDED = 3,
		TX_STATE_NEEDS_MANUAL_REVIEW = 4,
	};


	CItemShopManager();
	virtual ~CItemShopManager();

	bool	Initialize(TItemShopItem* pTable, int iSize);
	void	Destroy();

	bool IsProperItem(TItemShopItem item) { return item.dwIndex > 0; }
	bool IsItemExist(int index);
	bool IsAuctionExist(int index);
	TItemShopItem GetItem(int index);
	TItemShopItem GetTimeAuctionItem(int index);
	int64_t GetItemPrice(TItemShopItem item);
	BYTE GetItemCurrency(TItemShopItem item);
	bool IsItemPromotion(TItemShopItem item);
	bool IsItemTimeAuction(TItemShopItem item);
	bool IsTimeAuctionActive(TItemShopItem item);
	bool CheckIfCanBuyTimeAuctionAndUpdate(TItemShopItem item, LPCHARACTER buyer, DWORD quantity=1);
	bool CanSendItem(LPCHARACTER buyer, TItemShopItem item);
	bool CanSendTimeAuction(TItemShopItem item);
	void SendBalance(LPCHARACTER ch, PlayerAccountBalance balance);
	void SendBalance(LPCHARACTER ch);
	void SendHappyHour(LPCHARACTER ch);
	void SendLatestTimeAuction(LPCHARACTER ch);
	void SendItemShopData(LPCHARACTER ch);
	void SendAuctionData(LPCHARACTER ch);
	void SendAuctionUpdate(LPCHARACTER ch, DWORD index);
	PlayerAccountBalance GetPlayerAccountBalance(LPCHARACTER ch);

	bool CheckBalance(LPCHARACTER ch, BYTE chargeType, int64_t amount, PlayerAccountBalance& currentBalance, bool addMarks = true);
	bool AddCash(LPCHARACTER ch, BYTE chargeType, int64_t amount, bool checkBalance = true, int64_t customSecondAmount = -1);
	bool BuyItem(LPCHARACTER ch, int itemIndex, int quantity, uint64_t requestId = 0);
	uint64_t LookupTransactionByRequestId(DWORD aid, uint64_t requestId, std::string* outState = nullptr);
	bool PersistInventoryItemSynchronously(LPITEM item);
	LPITEM GiveItem(LPCHARACTER ch, int itemIndex, DWORD itemVnum, DWORD itemCount, bool* outDeliveredToAward = nullptr, uint64_t* outAwardInsertId = nullptr, uint64_t txId = 0, DWORD slotIndex = 0);
	void RefreshItemShop(LPCHARACTER ch);
	void OpenItemShop(LPCHARACTER ch, bool refresh = false);
	void CloseItemShop(LPCHARACTER ch);
	void SendUseCodeVoucher(LPCHARACTER ch, const char * code);
	void RecvUseCodeVoucher(LPCHARACTER ch, BYTE bResponse, const char * code);

	void OpenAuctionTab(LPCHARACTER ch);
	void CloseAuctionTab(LPCHARACTER ch);

	void RefreshAuctionViewers(DWORD dwAuctionIndex);

	bool AddVIP(LPCHARACTER ch, BYTE premiumType, DWORD durationInHours);

	void UpdateAuctionBuyAmount(DWORD dwItemIndex, DWORD dwBuyAmount);

	bool EnsureTransactionJournalSchema();
	int RecoverIncompleteTransactions();
	uint64_t GetLastTransactionId() const { return m_ullLastTxId; }
	void SetCrashAfterChargeForTest(bool bEnable) { m_bCrashAfterChargeForTest = bEnable; }
	void SetHardExitAfterChargeForTest(bool bEnable) { m_bHardExitAfterChargeForTest = bEnable; }
	void SetCrashAfterDeliveryBeforeJournalForTest(bool bEnable) { m_bCrashAfterDeliveryBeforeJournalForTest = bEnable; }
	void SetCrashAfterGiveItemBeforeSqlPersistForTest(bool bEnable) { m_bCrashAfterGiveItemBeforeSqlPersistForTest = bEnable; }
	void SetHardExitAfterGiveItemBeforeSqlPersistForTest(bool bEnable) { m_bHardExitAfterGiveItemBeforeSqlPersistForTest = bEnable; }
	void SetHardExitAfterClaimRecoveringForTest(bool bEnable) { m_bHardExitAfterClaimRecoveringForTest = bEnable; }

private:
	std::map<DWORD, TItemShopItem> m_map_ItemShopItems;
	std::map<DWORD, TItemShopItem> m_map_ItemShopTimeAuctions;

	PLAYER_SET	m_map_AuctionViewer;

	uint64_t m_ullLastTxId = 0;
	bool m_bCrashAfterChargeForTest = false;
	bool m_bHardExitAfterChargeForTest = false;
	bool m_bCrashAfterDeliveryBeforeJournalForTest = false;
	bool m_bCrashAfterGiveItemBeforeSqlPersistForTest = false;
	bool m_bHardExitAfterGiveItemBeforeSqlPersistForTest = false;
	bool m_bHardExitAfterClaimRecoveringForTest = false;

	DWORD GetTimeAuctionProgress(TItemShopItem item);
	void GetPacketDataFromItem(struct TItemShopItemClient& pack, int& i, TItemShopItem item);
};

