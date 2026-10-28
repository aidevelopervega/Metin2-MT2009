#ifndef __INC_CLIENTMANAGER_H__
#define __INC_CLIENTMANAGER_H__

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "../../common/stl.h"
#include "../../common/building.h"

#include "Peer.h"
#include "DBManager.h"
#include "LoginData.h"
#ifdef ENABLE_IKASHOP_RENEWAL
#include <rapidjson/rapidjson.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <fmt/fmt.h>
#include "../../common/json_helper.h"
#include "IkarusShopCache.h"
#endif

#define ENABLE_PROTO_FROM_DB
#ifdef ENABLE_IKASHOP_RENEWAL
inline std::optional<rapidjson::Document> str_to_json(const char* str) {
	if (!str || str[0] == 0)
		return std::nullopt;

	rapidjson::Document ret;
	ret.Parse(str);

	if (ret.HasParseError()) {
		sys_err("Json Document has parse errors : %d (str %s)", (int)ret.GetParseError(), str);
		return std::nullopt;
	}

	return ret;
}
#endif

class CPlayerTableCache;
class CItemCache;
class CItemPriceListTableCache;

class CPacketInfo
{
    public:
	void Add(int header);
	void Reset();

	std::map<int, int> m_map_info;
};

size_t CreatePlayerSaveQuery(char * pszQuery, size_t querySize, TPlayerTable * pkTab);

class CClientManager : public CNetBase, public singleton<CClientManager>
{
    public:
	typedef std::list<CPeer *>			TPeerList;
	typedef boost::unordered_map<uint64_t, CPlayerTableCache *> TPlayerTableCacheMap;
	typedef boost::unordered_map<uint64_t, CItemCache *> TItemCacheMap;
	typedef boost::unordered_set<CItemCache *, boost::hash<CItemCache*> > TItemCacheSet;
	typedef boost::unordered_map<uint64_t, TItemCacheSet *> TItemCacheSetPtrMap;
	typedef boost::unordered_map<uint64_t, CItemPriceListTableCache*> TItemPriceListCacheMap;
	typedef boost::unordered_map<WORD, BYTE> TChannelStatusMap;

	// MYSHOP_PRICE_LIST

	typedef std::pair< DWORD, uint64_t >		TItemPricelistReqInfo;
	// END_OF_MYSHOP_PRICE_LIST

	class ClientHandleInfo
	{
	    public:
		DWORD	dwHandle;
		uint64_t	account_id;
		uint64_t	player_id;
		BYTE	account_index;
		char	login[LOGIN_MAX_LEN + 1];
		char	safebox_password[SAFEBOX_PASSWORD_MAX_LEN + 1];
		char	ip[MAX_HOST_LENGTH + 1];

		TAccountTable * pAccountTable;
		TSafeboxTable * pSafebox;

		ClientHandleInfo(DWORD argHandle, uint64_t dwPID = 0)
		{
		    dwHandle = argHandle;
		    pSafebox = NULL;
		    pAccountTable = NULL;
		    player_id = dwPID;
		    account_id = 0;
		    account_index = 0;
		    memset(login, 0, sizeof(login));
		    memset(safebox_password, 0, sizeof(safebox_password));
		    memset(ip, 0, sizeof(ip));
		};

		ClientHandleInfo(DWORD argHandle, uint64_t dwPID, uint64_t accountId)
		{
		    dwHandle = argHandle;
		    pSafebox = NULL;
		    pAccountTable = NULL;
		    player_id = dwPID;
			account_id = accountId;
			account_index = 0;
			memset(login, 0, sizeof(login));
			memset(safebox_password, 0, sizeof(safebox_password));
			memset(ip, 0, sizeof(ip));
		};

		~ClientHandleInfo()
		{
		    if (pSafebox)
			{
				delete pSafebox;
				pSafebox = NULL;
			}
		}
	};

	public:
	CClientManager();
	~CClientManager();

	bool	Initialize();
	time_t	GetCurrentTime();

	void	MainLoop();
	void	Quit();

	void	GetPeerP2PHostNames(std::string& peerHostNames);
	void	SetTablePostfix(const char* c_pszTablePostfix);
	void	SetPlayerIDStart(uint64_t iIDStart);
	uint64_t	GetPlayerIDStart() { return m_iPlayerIDStart; }

	int		GetPlayerDeleteLevelLimit() { return m_iPlayerDeleteLevelLimit; }

	void	SetChinaEventServer(bool flag) { m_bChinaEventServer = flag; }
	bool	IsChinaEventServer() { return m_bChinaEventServer; }

	DWORD	GetUserCount();

	void	SendAllGuildSkillRechargePacket();
	void	SendTime();

	CPlayerTableCache *	GetPlayerCache(uint64_t id);
	void			PutPlayerCache(TPlayerTable * pNew);

	void			CreateItemCacheSet(uint64_t dwID);
	TItemCacheSet *		GetItemCacheSet(uint64_t dwID);
	void			FlushItemCacheSet(uint64_t dwID);

	CItemCache *		GetItemCache(uint64_t id);
	void			PutItemCache(TPlayerItem * pNew, bool bSkipQuery = false);
	bool			DeleteItemCache(uint64_t id);

	void			UpdatePlayerCache();
	void			UpdateItemCache();

	// MYSHOP_PRICE_LIST

	CItemPriceListTableCache*	GetItemPriceListCache(uint64_t dwID);

	void			PutItemPriceListCache(const TItemPriceListTable* pItemPriceList);

	void			UpdateItemPriceListCache(void);
	// END_OF_MYSHOP_PRICE_LIST

	void			SendGuildSkillUsable(uint64_t guild_id, DWORD dwSkillVnum, bool bUsable);

	void			SetCacheFlushCountLimit(int iLimit);

	template <class Func>
	Func		for_each_peer(Func f);

	CPeer *		GetAnyPeer();

	void			ForwardPacket(BYTE header, const void* data, int size, BYTE bChannel = 0, CPeer * except = NULL);

	void			SendNotice(const char * c_pszFormat, ...);

	// @fixme203 directly GetCommand instead of strcpy
	char*			GetCommand(char* str, char* command);
	void			ItemAward(CPeer * peer, char* login);

	int			AnalyzeQueryResult(SQLMsg * msg);
	void			AnalyzeFailedQuery(SQLMsg * msg);
	int			AnalyzeErrorMsg(CPeer * peer, SQLMsg * msg);
	void		FlushPlayerCacheSet(uint64_t pid);
	uint64_t	GetMaxCachedItemIDInRange(uint64_t dwMin, uint64_t dwMax) const;
	void		QUERY_PLAYER_LOAD(CPeer * peer, DWORD dwHandle, TPlayerLoadPacket * packet);
	void		InsertLoginData(CLoginData * pkLD);
	CLoginData *	GetLoginDataByAID(uint64_t dwAID);
	bool		InitializeSpecialShopTable();
	void		QUERY_BOOT(CPeer * peer, TPacketGDBoot * p);
	int		GetSpecialShopTableSize() const { return m_iSpecialShopTableSize; }
	const TSpecialShopTable * GetSpecialShopTable() const { return m_pSpecialShopTable; }
	size_t		GetSpecialShopMetaCount() const { return m_map_SpecialShopMetaData.size(); }
	void		SetTestSpecialShopTable(const TSpecialShopTable * pTables, int iCount)
	{
		if (m_pSpecialShopTable)
		{
			delete [] m_pSpecialShopTable;
			m_pSpecialShopTable = NULL;
		}
		m_iSpecialShopTableSize = 0;
		m_map_SpecialShopMetaData.clear();
		if (pTables && iCount > 0)
		{
			m_pSpecialShopTable = new TSpecialShopTable[static_cast<size_t>(iCount)];
			memcpy(m_pSpecialShopTable, pTables, sizeof(TSpecialShopTable) * static_cast<size_t>(iCount));
			m_iSpecialShopTableSize = iCount;
			for (int i = 0; i < iCount; ++i)
				GetSpecialShopMetaCache(pTables[i].vnum);
		}
	}

    protected:
	void	Destroy();

    private:
	bool		InitializeTables();
	bool		InitializeShopTable();
	bool		InitializeMobTable();
	bool		InitializeItemTable();
	bool		InitializeQuestItemTable();
	bool		InitializeSkillTable();
	bool		InitializeRefineTable();
	bool		InitializeOfflineShopLimitTable();
	bool		InitializeBanwordTable();
	bool		InitializeItemAttrTable();
	bool		InitializeItemRareTable();
	bool		InitializeLandTable();
	bool		InitializeObjectProto();
	bool		InitializeObjectTable();
	bool		InitializeMonarch();
	bool		InitializeItemShop();
	bool		InitializeCraftingTable();
	bool		InitializeQuestRewardTable();
	bool		InitializeNameReservationTable();
	bool		InitializeSpecialShopItemTable();

	bool		MirrorMobTableIntoDB();
	bool		MirrorItemTableIntoDB();

	void		AddPeer(socket_t fd);
	void		RemovePeer(CPeer * pPeer);
	CPeer *		GetPeer(IDENT ident);

	int		Process();

        void            ProcessPackets(CPeer * peer);

	CLoginData *	GetLoginData(DWORD dwKey);
	CLoginData *	GetLoginDataByLogin(const char * c_pszLogin);

	void		DeleteLoginData(CLoginData * pkLD);

	bool		InsertLogonAccount(const char * c_pszLogin, DWORD dwHandle, const char * c_pszIP);
	bool		DeleteLogonAccount(const char * c_pszLogin, DWORD dwHandle);
	bool		FindLogonAccount(const char * c_pszLogin);

	void		GuildCreate(CPeer * peer, uint64_t dwGuildID);
	void		GuildSkillUpdate(CPeer * peer, TPacketGuildSkillUpdate* p);
	void		GuildExpUpdate(CPeer * peer, TPacketGuildExpUpdate* p);
	void		GuildAddMember(CPeer * peer, TPacketGDGuildAddMember* p);
	void		GuildChangeGrade(CPeer * peer, TPacketGuild* p);
	void		GuildRemoveMember(CPeer * peer, TPacketGuild* p);
	void		GuildChangeMemberData(CPeer * peer, TPacketGuildChangeMemberData* p);
	void		GuildDisband(CPeer * peer, TPacketGuild * p);
	void		GuildWar(CPeer * peer, TPacketGuildWar * p);
	void		GuildWarScore(CPeer * peer, TPacketGuildWarScore * p);
	void		GuildChangeLadderPoint(TPacketGuildLadderPoint* p);
	void		GuildUseSkill(TPacketGuildUseSkill* p);
	void		GuildDepositMoney(TPacketGDGuildMoney* p);
	void		GuildWithdrawMoney(CPeer* peer, TPacketGDGuildMoney* p);
	void		GuildWithdrawMoneyGiveReply(TPacketGDGuildMoneyWithdrawGiveReply* p);
	void		GuildWarBet(TPacketGDGuildWarBet * p);
	void		GuildChangeMaster(TPacketChangeGuildMaster* p);

	void		SetGuildWarEndTime(uint64_t guild_id1, uint64_t guild_id2, time_t tEndTime);

	void		QUERY_LOGIN(CPeer * peer, DWORD dwHandle, SLoginPacket* data);
	void		QUERY_LOGOUT(CPeer * peer, DWORD dwHandle, const char *);

	void		RESULT_LOGIN(CPeer * peer, SQLMsg *msg);

	void		RESULT_COMPOSITE_PLAYER(CPeer * peer, SQLMsg * pMsg, DWORD dwQID);
	void		RESULT_PLAYER_LOAD(CPeer * peer, MYSQL_RES * pRes, ClientHandleInfo * pkInfo);
	void		RESULT_ITEM_LOAD(CPeer * peer, MYSQL_RES * pRes, DWORD dwHandle, uint64_t dwPID);
	void		RESULT_QUEST_LOAD(CPeer * pkPeer, MYSQL_RES * pRes, DWORD dwHandle, uint64_t dwPID);
	// @fixme402 (RESULT_AFFECT_LOAD +dwRealPID)
	void		RESULT_AFFECT_LOAD(CPeer * pkPeer, MYSQL_RES * pRes, DWORD dwHandle, uint64_t dwRealPID);
	void		RESULT_SPECIAL_FLAG_LOAD(CPeer* pkPeer, MYSQL_RES* pRes, DWORD dwHandle, uint64_t dwPID);

	// PLAYER_INDEX_CREATE_BUG_FIX
	void		RESULT_PLAYER_INDEX_CREATE(CPeer *pkPeer, SQLMsg *msg);
	// END_PLAYER_INDEX_CREATE_BUG_FIX

	// MYSHOP_PRICE_LIST

	void		RESULT_PRICELIST_LOAD(CPeer* peer, SQLMsg* pMsg);

	void		RESULT_PRICELIST_LOAD_FOR_UPDATE(SQLMsg* pMsg);
	// END_OF_MYSHOP_PRICE_LIST

	void		QUERY_PLAYER_SAVE(CPeer * peer, DWORD dwHandle, TPlayerTable*);

	void		__QUERY_PLAYER_CREATE(CPeer * peer, DWORD dwHandle, TPlayerCreatePacket *);
	void		__QUERY_PLAYER_DELETE(CPeer * peer, DWORD dwHandle, TPlayerDeletePacket *);
	void		__RESULT_PLAYER_DELETE(CPeer * peer, SQLMsg* msg);

	void		QUERY_PLAYER_COUNT(CPeer * pkPeer, TPlayerCountPacket *);

	void		QUERY_ITEM_SAVE(CPeer * pkPeer, const char * c_pData);
	void		QUERY_ITEM_DESTROY(CPeer * pkPeer, const char * c_pData);
	void		QUERY_ITEM_FLUSH(CPeer * pkPeer, const char * c_pData);

	void		QUERY_QUEST_SAVE(CPeer * pkPeer, TQuestTable *, DWORD dwLen);
	void		QUERY_SPECIAL_FLAG_SAVE(CPeer* pkPeer, TSpecialFlagTable* pTable, DWORD dwLen);
	void		QUERY_ADD_AFFECT(CPeer * pkPeer, TPacketGDAddAffect * p);
	void		QUERY_REMOVE_AFFECT(CPeer * pkPeer, TPacketGDRemoveAffect * p);

	void		QUERY_SAFEBOX_LOAD(CPeer * pkPeer, DWORD dwHandle, TSafeboxLoadPacket *, bool bMall);
	void		QUERY_SAFEBOX_SAVE(CPeer * pkPeer, TSafeboxTable * pTable);
	void		QUERY_SAFEBOX_CHANGE_SIZE(CPeer * pkPeer, DWORD dwHandle, TSafeboxChangeSizePacket * p);
	void		QUERY_SAFEBOX_CHANGE_PASSWORD(CPeer * pkPeer, DWORD dwHandle, TSafeboxChangePasswordPacket * p);

	void		RESULT_SAFEBOX_LOAD(CPeer * pkPeer, SQLMsg * msg);
	void		RESULT_SAFEBOX_CHANGE_SIZE(CPeer * pkPeer, SQLMsg * msg);
	void		RESULT_SAFEBOX_CHANGE_PASSWORD(CPeer * pkPeer, SQLMsg * msg);
	void		RESULT_SAFEBOX_CHANGE_PASSWORD_SECOND(CPeer * pkPeer, SQLMsg * msg);

	void		QUERY_MOVE_CHANNEL(CPeer* pkPeer, DWORD dwHandle, TMoveChannel*);

	void		QUERY_EMPIRE_SELECT(CPeer * pkPeer, DWORD dwHandle, TEmpireSelectPacket * p);
	void		QUERY_SETUP(CPeer * pkPeer, DWORD dwHandle, const char * c_pData);

	void		SendPartyOnSetup(CPeer * peer);

	void		QUERY_FLUSH_CACHE(CPeer * pkPeer, const char * c_pData);

	void		QUERY_PARTY_CREATE(CPeer * peer, TPacketPartyCreate* p);
	void		QUERY_PARTY_DELETE(CPeer * peer, TPacketPartyDelete* p);
	void		QUERY_PARTY_ADD(CPeer * peer, TPacketPartyAdd* p);
	void		QUERY_PARTY_REMOVE(CPeer * peer, TPacketPartyRemove* p);
	void		QUERY_PARTY_STATE_CHANGE(CPeer * peer, TPacketPartyStateChange* p);
	void		QUERY_PARTY_SET_MEMBER_LEVEL(CPeer * peer, TPacketPartySetMemberLevel* p);

	void		QUERY_RELOAD_PROTO();

	void		QUERY_CHANGE_NAME(CPeer * peer, DWORD dwHandle, TPacketGDChangeName * p);
	void		GetPlayerFromRes(TPlayerTable * player_table, MYSQL_RES* res);

	void		QUERY_LOGIN_KEY(CPeer * pkPeer, TPacketGDLoginKey * p);

	void		AddGuildPriv(TPacketGiveGuildPriv* p);
	void		AddEmpirePriv(TPacketGiveEmpirePriv* p);
	void		AddCharacterPriv(TPacketGiveCharacterPriv* p);

	void		MoneyLog(TPacketMoneyLog* p);

	void		QUERY_AUTH_LOGIN(CPeer * pkPeer, DWORD dwHandle, TPacketGDAuthLogin * p);

	void		QUERY_LOGIN_BY_KEY(CPeer * pkPeer, DWORD dwHandle, TPacketGDLoginByKey * p);
	void		RESULT_LOGIN_BY_KEY(CPeer * peer, SQLMsg * msg);

	void		ChargeCash(const TRequestChargeCash * p);
	void		UseCodeVoucher(const TRequestUseCodeVoucher * p, CPeer* peer);

	void		LoadEventFlag();
	void		SetEventFlag(TPacketSetEventFlag* p);
	void		SendEventFlagsOnSetup(CPeer* peer);

	void		MarriageAdd(TPacketMarriageAdd * p);
	void		MarriageUpdate(TPacketMarriageUpdate * p);
	void		MarriageRemove(TPacketMarriageRemove * p);

	void		WeddingRequest(TPacketWeddingRequest * p);
	void		WeddingReady(TPacketWeddingReady * p);
	void		WeddingEnd(TPacketWeddingEnd * p);

	// MYSHOP_PRICE_LIST

	void		MyshopPricelistUpdate(const TItemPriceListTable* pPacket); // @fixme403 (TPacketMyshopPricelistHeader to TItemPriceListTable)

	void		MyshopPricelistRequest(CPeer* peer, DWORD dwHandle, uint64_t dwPlayerID);
	// END_OF_MYSHOP_PRICE_LIST

	// Building
	void		CreateObject(TPacketGDCreateObject * p);
	void		DeleteObject(DWORD dwID);
	void		UpdateLand(const TPacketGDUpdateLand * p);

	// BLOCK_CHAT
	void		BlockChat(TPacketBlockChat * p);
	// END_OF_BLOCK_CHAT

    private:
	int					m_looping;
	socket_t				m_fdAccept;
	TPeerList				m_peerList;

	CPeer *					m_pkAuthPeer;

	// LoginKey, LoginData pair
	typedef boost::unordered_map<DWORD, CLoginData *> TLoginDataByLoginKey;
	TLoginDataByLoginKey			m_map_pkLoginData;

	// Login LoginData pair
	typedef boost::unordered_map<std::string, CLoginData *> TLoginDataByLogin;
	TLoginDataByLogin			m_map_pkLoginDataByLogin;

	// AccountID LoginData pair
	typedef boost::unordered_map<uint64_t, CLoginData *> TLoginDataByAID;
	TLoginDataByAID				m_map_pkLoginDataByAID;

	typedef boost::unordered_map<std::string, CLoginData *> TLogonAccountMap;
	TLogonAccountMap			m_map_kLogonAccount;

	uint64_t				m_iPlayerIDStart;
	int					m_iPlayerDeleteLevelLimit;
	int					m_iPlayerDeleteLevelLimitLower;
	bool					m_bChinaEventServer;

	std::vector<TMobTable>			m_vec_mobTable;
	std::vector<TItemTable>			m_vec_itemTable;
	std::map<DWORD, TItemTable *>		m_map_itemTableByVnum;

	int					m_iShopTableSize;
	TShopTable *				m_pShopTable;

	int					m_iRefineTableSize;
	TRefineTable*				m_pRefineTable;

	std::vector<TSkillTable>		m_vec_skillTable;
	std::vector<TBanwordTable>		m_vec_banwordTable;
	std::vector<TNameReservationTable>		m_vec_nameReservationTable;
	std::vector<TItemAttrTable>		m_vec_itemAttrTable;
	std::vector<TItemAttrTable>		m_vec_itemRareTable;

	std::vector<TOfflineShopLimit>		m_vec_offlineShopLimitTable;

	std::vector<building::TLand>		m_vec_kLandTable;
	std::vector<building::TObjectProto>	m_vec_kObjectProto;
	std::map<DWORD, building::TObject *>	m_map_pkObjectTable;

	std::vector<TItemShopItem>		m_vec_itemShopItemTable;
	std::vector<TCraftingItem>		m_vec_CraftingTable;
	std::vector<TQuestRewardItem>		m_vec_QuestRewardTable;

	int					m_iSpecialShopTableSize;
	TSpecialShopTable *		m_pSpecialShopTable;
	std::vector<TSpecialShopItem>		m_vec_SpecialShopItemTable;

	bool					m_bShutdowned;
	bool					m_bMaintenance;

	std::map<DWORD, TSpecialShopMetaData>	m_map_SpecialShopMetaData;

	TPlayerTableCacheMap			m_map_playerCache;

	TItemCacheMap				m_map_itemCache;
	TItemCacheSetPtrMap			m_map_pkItemCacheSetPtr;

	// MYSHOP_PRICE_LIST

	TItemPriceListCacheMap m_mapItemPriceListCache;
	// END_OF_MYSHOP_PRICE_LIST

	TChannelStatusMap m_mChannelStatus;

	struct TPartyInfo
	{
	    BYTE bRole;
	    BYTE bLevel;

		TPartyInfo() :bRole(0), bLevel(0)
		{
		}
	};

	typedef std::map<uint64_t, TPartyInfo>	TPartyMember;
	typedef std::map<uint64_t, TPartyMember>	TPartyMap;
	typedef std::map<BYTE, TPartyMap>	TPartyChannelMap;
	TPartyChannelMap m_map_pkChannelParty;

	typedef std::map<std::string, long>	TEventFlagMap;
	TEventFlagMap m_map_lEventFlag;

	BYTE					m_bLastHeader;
	int					m_iCacheFlushCount;
	int					m_iCacheFlushCountLimit;

    private :
	TItemIDRangeTable m_itemRange;

    public :
	bool InitializeNowItemID();
	uint64_t GetItemID();
	uint64_t GainItemID();
	TItemIDRangeTable GetItemRange() { return m_itemRange; }
#ifdef ENABLE_IKASHOP_RENEWAL
public:
	// forwarding
	void SendIkarusShopTable(CPeer* peer);
	//booting
	bool InitializeIkarusShopTable(uint64_t owner=0);
	// shops
	bool InitializeIkarusShopShopTable(uint64_t owner=0);
	bool InitializeIkarusShopShopItemsTable(uint64_t owner=0);
	// offers
	bool InitializeIkarusShopOfferTable();
	// auctions
	bool InitializeIkarusShopAuctionTable();
	bool InitializeIkarusShopAuctionOfferTable();

	//packets exchanging
	void RecvIkarusShopPacket(CPeer* peer, const char* data);
	bool RecvIkarusShopBuyItemPacket(CPeer* peer, const char* data);
	bool RecvIkarusShopLockBuyItem(CPeer* peer, const char* data);
	bool RecvIkarusShopEditItemPacket(const char* data);
	bool RecvIkarusShopRemoveItemPacket(CPeer* peer, const char* data);
	bool RecvIkarusShopAddItemPacket(const char* data);

	bool RecvIkarusShopForceSoftClose(CPeer* peer, const char* data);
	bool RecvIkarusShopForceHardClose(CPeer* peer, const char* data);
	bool RecvIkarusShopCreateNew(const char* data);
	bool RecvIkarusShopChangeName(const char* data);

	bool RecvIkarusShopOfferCreate(const char* data);
	bool RecvIkarusShopOfferNotified(const char* data);
	bool RecvIkarusShopOfferAccepted(const char* data);
	bool RecvIkarusShopOfferCancel(const char* data);

	bool RecvIkarusShopSafeboxGetItem(CPeer* peer, const char* data);
	bool RecvIkarusShopSafeboxGetValutes(const char* data);
	bool RecvIkarusShopSafeboxAddItem(const char* data);
	bool RecvIkarusShopSafeboxGetItemFeedback(const char* data);

	//AUCTION
	bool RecvIkarusShopAuctionCreate(const char* data);
	bool RecvIkarusShopAuctionAddOffer(const char* data);
	bool RecvIkarusShopAuctionCancel(const char* data);
	bool RecvIkarusShopAuctionOfferSeen(const char* data);

#ifdef EXTEND_IKASHOP_PRO
	bool RecvIkarusShopNotificationSeen(const char* data);
	bool RecvIkarusShopRestoreDuration(const char* data);
#ifdef ENABLE_IKASHOP_ENTITIES
	bool RecvIkarusShopMoveShopEntity(const char* data);
#endif
#endif
	bool RecvIkarusShopLockedSlotState(const char* data);
	bool RecvIkarusShopOwnerOnline(const char* data);
	bool RecvIkarusShopLoadExpiredShop(const char* data);

	bool SendIkarusShopBuyItemPacket(uint64_t dwOwner, uint64_t dwGuest, uint64_t dwItem, CPeer* requester);
	bool SendIkarusShopBuyLockedItemPacket(CPeer* peer, uint64_t dwOwner, uint64_t dwGuest, uint64_t dwItem);
	bool SendIkarusShopEditItemPacket(uint64_t dwOwner, uint64_t dwItem, const ikashop::TPriceInfo& price);
	bool SendIkarusShopRemoveItemPacket(uint64_t dwOwner, uint64_t dwItem, CPeer* requester = nullptr);
	bool SendIkarusShopAddItemPacket(uint64_t dwOwner, uint64_t dwItemID, const ikashop::TShopItem& rInfo);

	bool SendIkarusShopForceClose(uint64_t dwOwnerID);
	bool SendIkarusShopCreateNew(const ikashop::TShopInfo& shop, const ikashop::TShopItem* items);
	bool SendIkarusShopChangeName(uint64_t dwOwnerID, const char* szName);
	bool SendIkarusShopShopExpired(uint64_t dwOwnerID);

	bool SendIkarusShopOfferCreate(const ikashop::TOfferInfo& offer);
	bool SendIkarusShopOfferNotified(uint64_t dwOwnerID, uint64_t dwOfferID);
	bool SendIkarusShopOfferAccepted(uint64_t dwOwnerID, uint64_t dwOfferID);

	//ikashop-updated 05/08/19
	void SendIkarusShopOfferCancel(uint64_t dwOwnerID, uint64_t dwOfferID, bool isRemovingItem);

	void SendIkarusShopSafeboxAddItem(uint64_t dwOwnerID, uint64_t dwItem, const ikashop::TShopPlayerItem& item);
	void SendIkarusShopSafeboxAddValutes(uint64_t dwOwnerID, const ikashop::TValutesInfo& valute);

	//patch 08-03-2020
	void SendIkarusShopSafeboxExpiredItem(uint64_t dwOwnerID, uint64_t itemID);
	void SendIkarusShopSafeboxGetItemConfirm(CPeer* peer, uint64_t dwOwnerID, uint64_t itemID);

	//AUCTION
	void SendIkarusShopAuctionCreate(const ikashop::TAuctionInfo& auction);
	void SendIkarusShopAuctionAddOffer(const ikashop::TAuctionOfferInfo& offer);
	void SendIkarusShopAuctionExpired(uint64_t dwOwnerID);

	void IkarusShopResultQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo);
	void IkarusShopResultAddItemQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo, bool sendResponse=true);
	void IkarusShopResultCreateShopQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo);
	void IkarusShopResultOfferAddQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo);
	void IkarusShopResultSafeboxAddItemQuery(CPeer* peer, SQLMsg* msg, CQueryInfo* pQueryInfo);


	void IkarusShopDurationProcess();
	void IkarusShopExpiredShop(uint64_t dwID
	);
	void IkarusShopExpiredAuction(uint64_t dwID);
	void IkarusShopLoadShopSafebox(CPeer* peer ,uint64_t dwID);
#ifdef EXTEND_IKASHOP_PRO
	void IkarusShopLoadNotifications(CPeer* peer, uint64_t owner);
	void AppendIkarusShopNotification(ikashop::ENotificationType type, uint64_t owner, DWORD what, const std::string& who, const std::string& format);
#endif
	//patch 08-03-2020
	void IkarusShopExpiredAuctionItem(uint64_t dwOwnerID);

	//updated 15-01-2020
	bool IsUsingIkarusShopSystem(uint64_t dwID);

	//patch offerlist loading fix
	void RemoveOfferOnShopItem(uint64_t dwOwnerID, uint64_t dwItemID);
	void RemoveOfferOnShop(uint64_t dwOwnerID);

#ifdef ENABLE_IKASHOP_LOGS
	void IkarusShopLog(uint64_t who, uint64_t itemid, std::string_view what, uint64_t shopOwner, const std::string& extra = "", DWORD vnum = 0, DWORD count = 0, int64_t yang = 0, int cheque = 0);
#endif

private:
	ikashop::CShopCache			m_offlineshopShopCache;
	ikashop::CSafeboxCache		m_offlineshopSafeboxCache;
	ikashop::COfferCache		m_offlineshopOfferCache;
	ikashop::CAuctionCache		m_offlineshopAuctionCache;
#endif

	//BOOT_LOCALIZATION
    public:

	bool InitializeLocalization();

    private:
	std::vector<tLocale> m_vec_Locale;
	//END_BOOT_LOCALIZATION
	//ADMIN_MANAGER

	bool __GetAdminInfo(const char *szIP, std::vector<tAdminInfo> & rAdminVec);
	bool __GetHostInfo(std::vector<std::string> & rIPVec);
	//END_ADMIN_MANAGER

	//RELOAD_ADMIN
	void ReloadAdmin(CPeer * peer, TPacketReloadAdmin * p);
	//END_RELOAD_ADMIN
	void ReloadItemShop();
	void BreakMarriage(CPeer * peer, const char * data);

	struct TLogoutPlayer
	{
	    uint64_t	pid;
	    time_t	time;

	    bool operator < (const TLogoutPlayer & r)
	    {
		return (pid < r.pid);
	    }
	};

	typedef boost::unordered_map<uint64_t, TLogoutPlayer*> TLogoutPlayerMap;
	TLogoutPlayerMap m_map_logout;

	void InsertLogoutPlayer(uint64_t pid);
	void DeleteLogoutPlayer(uint64_t pid);
	void UpdateLogoutPlayer();
	void UpdateItemCacheSet(uint64_t pid);


	//MONARCH
	void Election(CPeer * peer, DWORD dwHandle, const char * p);
	void Candidacy(CPeer * peer, DWORD dwHandle, const char * p);
	void AddMonarchMoney(CPeer * peer, DWORD dwHandle, const char * p);
	void TakeMonarchMoney(CPeer * peer, DWORD dwHandle, const char * p);
	void ComeToVote(CPeer * peer, DWORD dwHandle, const char * p);
	void RMCandidacy(CPeer * peer, DWORD dwHandle, const char * p);
	void SetMonarch(CPeer * peer, DWORD dwHandle, const char * p);
	void RMMonarch(CPeer * peer, DWORD dwHandle, const char * p);

	void DecMonarchMoney(CPeer * peer, DWORD dwHandle, const char * p);
	//END_MONARCH

	void ChangeMonarchLord(CPeer* peer, DWORD dwHandle, TPacketChangeMonarchLord* info);

	void SendSpareItemIDRange(CPeer* peer);

	void UpdateHorseName(TPacketUpdateHorseName* data, CPeer* peer);
	void AckHorseName(uint64_t dwPID, CPeer* peer);
	void SetLoginPremiumPass(CPeer* peer, TPacketSetPremiumPass* data);
	void DeleteLoginKey(TPacketDC *data);
	void ResetLastPlayerID(const TPacketNeedLoginLogInfo* data);
	//delete gift notify icon
	void DeleteAwardId(TPacketDeleteAwardID* data);
	void UpdateChannelStatus(TChannelStatus* pData);
	void RequestChannelStatus(CPeer* peer, DWORD dwHandle);
	void Maintenance(TPacketGDMaintenance* data);

	TSpecialShopMetaData GetSpecialShopMetaCache(DWORD shop_vnum);
	void RecvSpecialShopSeed(CPeer* peer, TPacketGDSpecialShopSeed* data);
	void RecvUsage(CPeer* peer, TPacketGDUsage* data);
#ifdef ENABLE_PROTO_FROM_DB
	public:
	bool		InitializeMobTableFromDB();
	bool		InitializeItemTableFromDB();
	protected:
	bool		bIsProtoReadFromDB;
#endif
};

template<class Func>
Func CClientManager::for_each_peer(Func f)
{
    TPeerList::iterator it;
    for (it = m_peerList.begin(); it!=m_peerList.end();++it)
    {
	f(*it);
    }
    return f;
}
#endif
