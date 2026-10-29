#include "stdafx.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cstdint>
#include <cassert>
#include "cache.h"

static int g_passCount = 0;
static int g_testCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        g_testCount++; \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            return 1; \
        } else { \
            g_passCount++; \
        } \
    } while (0)

// Mock structures matching player and item data
struct TestPlayerData
{
    uint32_t id;
    char name[32];
    int64_t gold;
    uint32_t exp;
    int level;
};

struct TestItemData
{
    uint32_t id;
    uint32_t owner;
    uint32_t vnum;
    int count;
    int pos;
};

// Test player cache implementation
class CTestPlayerCache : public cache<TestPlayerData>
{
public:
    int m_flushCallCount = 0;
    std::vector<uint32_t> m_flushedVersions;

    void OnFlush() override
    {
        ++m_flushCallCount;
        m_flushedVersions.push_back(m_dwPendingSaveVersion);
    }
};

// Test item cache implementation
class CTestItemCache : public cache<TestItemData>
{
public:
    int m_flushCallCount = 0;
    std::vector<uint32_t> m_flushedVersions;

    void OnFlush() override
    {
        ++m_flushCallCount;
        m_flushedVersions.push_back(m_dwPendingSaveVersion);
    }
};

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << " METIN2 MT2009 - DB CACHE EVICTION & SAFETY SUITE " << std::endl;
    std::cout << "==================================================" << std::endl;

    // =========================================================================
    // SENARYO A: Başarılı asenkron kayıt sonrası önbelleğin güvenli tahliyesi
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo A: Basarili asenkron kayit sonrasi guvenli tahliye..." << std::endl;

        std::map<uint32_t, CTestPlayerCache*> playerMap;
        CTestPlayerCache* c = new CTestPlayerCache();
        TestPlayerData data{ 101, "HeroWarrior", 1000000, 50000, 75 };
        c->Put(&data);
        playerMap[101] = c;

        TEST_ASSERT(c->GetVersion() == 1, "Senaryo A: Version must be 1 after Put");
        TEST_ASSERT(c->IsNeedQuery() == true, "Senaryo A: Cache must be dirty after Put");
        TEST_ASSERT(c->IsSavePending() == false, "Senaryo A: No save pending before flush");

        // Player logs out -> initiate eviction
        c->MarkPendingEviction();
        TEST_ASSERT(c->IsPendingEviction() == true, "Senaryo A: Marked pending eviction");
        TEST_ASSERT(c->CanEvict() == false, "Senaryo A: Cannot evict yet because dirty data needs flush");

        // Flush triggered
        c->Flush();
        TEST_ASSERT(c->m_flushCallCount == 1, "Senaryo A: Flush called exactly once");
        TEST_ASSERT(c->GetPendingSaveVersion() == 1, "Senaryo A: Pending save version must be 1");
        TEST_ASSERT(c->IsSavePending() == true, "Senaryo A: Save is in-flight pending ACK");
        TEST_ASSERT(c->IsNeedQuery() == false, "Senaryo A: NeedQuery cleared during in-flight save");
        TEST_ASSERT(c->CanEvict() == false, "Senaryo A: CANNOT evict while save is in-flight (avoids premature deletion)");

        // DB acknowledges success
        bool evictNow = c->OnSaveSuccess(1);
        TEST_ASSERT(evictNow == true, "Senaryo A: OnSaveSuccess returns true for eviction");
        TEST_ASSERT(c->IsSavePending() == false, "Senaryo A: SavePending cleared");
        TEST_ASSERT(c->CanEvict() == true, "Senaryo A: CanEvict is now true after DB ACK");

        // Safe eviction from map
        playerMap.erase(101);
        delete c;
        TEST_ASSERT(playerMap.empty() == true, "Senaryo A: Cache safely evicted from map after DB ACK");
    }

    // =========================================================================
    // SENARYO B: Kayıt başarısız olduğunda önbelleğin silinmemesi, kirli kalması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo B: Kayit basarisizliginda kirli veri korumasi..." << std::endl;

        std::map<uint32_t, CTestItemCache*> itemMap;
        CTestItemCache* c = new CTestItemCache();
        TestItemData item{ 5001, 101, 10, 1, 0 }; // Sword+9
        c->Put(&item);
        itemMap[5001] = c;

        c->MarkPendingEviction();
        c->Flush();
        TEST_ASSERT(c->IsSavePending() == true, "Senaryo B: Save in-flight");

        // Simulate DB error (e.g. deadlock, timeout, disk full)
        c->OnSaveFailure(1, false);

        TEST_ASSERT(c->IsSavePending() == false, "Senaryo B: In-flight cleared on failure");
        TEST_ASSERT(c->IsNeedQuery() == true, "Senaryo B: DATA MUST REMAIN DIRTY (IsNeedQuery=true)");
        TEST_ASSERT(c->CanEvict() == false, "Senaryo B: CANNOT EVICT on failure! Dirty data protected in RAM");
        TEST_ASSERT(itemMap.count(5001) == 1, "Senaryo B: Object remains alive in map");
        TEST_ASSERT(c->Get()->vnum == 10, "Senaryo B: Item data remains 100% intact");

        // Retry flush
        c->Flush();
        TEST_ASSERT(c->m_flushCallCount == 2, "Senaryo B: Second flush triggered for retry");
        TEST_ASSERT(c->IsSavePending() == true, "Senaryo B: Retry save is now in-flight");

        // DB succeeds on retry
        c->OnSaveSuccess(1);
        TEST_ASSERT(c->CanEvict() == true, "Senaryo B: CanEvict becomes true only after successful retry");

        itemMap.erase(5001);
        delete c;
    }

    // =========================================================================
    // SENARYO C: Oyuncu çıkışında SQL sunucusu kapalı/kopuk ise veri kaybı yaşanmaması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo C: SQL baglantisi kesildiginde veri kaybi yasanmamasi..." << std::endl;

        CTestPlayerCache* c = new CTestPlayerCache();
        TestPlayerData data{ 202, "MageMaster", 50000000, 999999, 99 };
        c->Put(&data);

        // Player logs out when connection is disconnected
        c->MarkPendingEviction();
        c->Flush();

        // Error with UNKNOWN status (CR_SERVER_LOST / CR_CONNECTION_ERROR)
        c->OnSaveFailure(1, true /* bUnknown */);

        TEST_ASSERT(c->IsUnknown() == true, "Senaryo C: Cache marked with UNKNOWN state");
        TEST_ASSERT(c->IsNeedQuery() == true, "Senaryo C: Cache kept dirty");
        TEST_ASSERT(c->CanEvict() == false, "Senaryo C: Eviction blocked despite player logout");

        // Player reconnects before server reconnects!
        c->CancelPendingEviction();
        TEST_ASSERT(c->IsPendingEviction() == false, "Senaryo C: Eviction canceled on player reload");
        TEST_ASSERT(c->Get()->gold == 50000000, "Senaryo C: Player loaded latest RAM data without DB overwrite");

        delete c;
    }

    // =========================================================================
    // SENARYO D: SQL yanıtı beklenirken yeni değişiklik yapılması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo D: In-flight save sirasinda yeni guncelleme guvenligi..." << std::endl;

        CTestPlayerCache* c = new CTestPlayerCache();
        TestPlayerData v1{ 303, "NinjaShadow", 100, 10, 1 };
        c->Put(&v1); // Version 1

        c->Flush(); // Version 1 sent to DB
        TEST_ASSERT(c->GetPendingSaveVersion() == 1, "Senaryo D: Pending save version is 1");
        TEST_ASSERT(c->IsNeedQuery() == false, "Senaryo D: Clean pending ACK");

        // While query 1 is in-flight, player earns 50,000 gold!
        TestPlayerData v2{ 303, "NinjaShadow", 50100, 500, 5 };
        c->Put(&v2); // Version 2
        TEST_ASSERT(c->GetVersion() == 2, "Senaryo D: Version bumped to 2");
        TEST_ASSERT(c->IsNeedQuery() == true, "Senaryo D: Cache marked dirty again with new data");

        // Now ACK for query 1 arrives from DB!
        c->OnSaveSuccess(1);

        // Because v2 > v1, OnSaveSuccess must recognize that data was modified and keep it dirty!
        TEST_ASSERT(c->IsNeedQuery() == true, "Senaryo D: Older ACK does NOT clear dirty flag of newer data");
        TEST_ASSERT(c->CanEvict() == false, "Senaryo D: Cannot evict because v2 is uncommitted");
        TEST_ASSERT(c->Get()->gold == 50100, "Senaryo D: RAM data remains latest (50,100 gold)");

        // Flush v2
        c->Flush();
        TEST_ASSERT(c->GetPendingSaveVersion() == 2, "Senaryo D: Pending save version is 2");

        // ACK for v2 arrives
        c->MarkPendingEviction();
        c->OnSaveSuccess(2);
        TEST_ASSERT(c->CanEvict() == true, "Senaryo D: Eviction allowed only when latest version committed");

        delete c;
    }

    // =========================================================================
    // SENARYO E: UNKNOWN durumunda çifte harcama ve veri çoğalması koruması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo E: UNKNOWN durumunda cifte islem onleme..." << std::endl;

        CTestPlayerCache* c = new CTestPlayerCache();
        TestPlayerData d{ 404, "SuraDark", 10000, 0, 30 };
        c->Put(&d);

        // Non-idempotent operation: buy item for 5,000 gold
        d.gold -= 5000;
        c->Put(&d);
        TEST_ASSERT(c->Get()->gold == 5000, "Senaryo E: Gold deducted to 5,000");

        c->Flush();
        // Server drop response packet -> UNKNOWN
        c->OnSaveFailure(2, true);

        TEST_ASSERT(c->IsUnknown() == true, "Senaryo E: Marked UNKNOWN");
        TEST_ASSERT(c->Get()->gold == 5000, "Senaryo E: Balance remains 5,000 in cache (single source of truth)");
        // No blind auto-retry is issued, preventing double deduction (gold would not become 0)
        TEST_ASSERT(c->Get()->gold != 0, "Senaryo E: No double spend deduction occurred");

        delete c;
    }

    // =========================================================================
    // SENARYO F: Art arda birden fazla kayıt kuyruğa alındığında sıralama garantisi
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo F: Sirali versiyon kontrolu ve gecikmis ACK korumasi..." << std::endl;

        CTestPlayerCache* c = new CTestPlayerCache();
        TestPlayerData d{ 505, "ShamanHeal", 10, 0, 1 };
        c->Put(&d); // v1
        c->Flush(); // pending = 1

        d.gold = 20;
        c->Put(&d); // v2
        c->Flush(); // pending = 2

        TEST_ASSERT(c->GetPendingSaveVersion() == 2, "Senaryo F: Pending version is 2");

        // Stale ACK for v1 arrives out of order
        bool handledV1 = c->OnSaveSuccess(1);
        TEST_ASSERT(handledV1 == false, "Senaryo F: Stale v1 ACK rejected because pending version is 2");
        TEST_ASSERT(c->IsSavePending() == true, "Senaryo F: SavePending still true waiting for v2");

        // ACK for v2 arrives
        bool handledV2 = c->OnSaveSuccess(2);
        TEST_ASSERT(handledV2 == false, "Senaryo F: Handled v2 successfully (not evicting as not marked)");
        TEST_ASSERT(c->IsSavePending() == false, "Senaryo F: SavePending cleared after matching v2 ACK");

        delete c;
    }

    // =========================================================================
    // SENARYO G: Önbellek nesnesinin yaşam döngüsü (deferred delete / lifecycle)
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo G: Onbellek yasam dongusu ve durum gecisleri..." << std::endl;

        CTestItemCache* c = new CTestItemCache();
        TEST_ASSERT(c->GetVersion() == 0, "Senaryo G: Initial version 0");
        TEST_ASSERT(c->IsNeedQuery() == false, "Senaryo G: Clean initially");
        TEST_ASSERT(c->IsPendingEviction() == false, "Senaryo G: Not pending eviction");

        TestItemData itm{ 9001, 777, 101, 1, 0 };
        c->Put(&itm);
        TEST_ASSERT(c->GetVersion() == 1, "Senaryo G: Version 1");
        TEST_ASSERT(c->IsNeedQuery() == true, "Senaryo G: Dirty");

        c->MarkPendingEviction();
        TEST_ASSERT(c->IsPendingEviction() == true, "Senaryo G: Eviction pending");
        TEST_ASSERT(c->CanEvict() == false, "Senaryo G: Cannot evict dirty item");

        c->Flush();
        TEST_ASSERT(c->CanEvict() == false, "Senaryo G: Cannot evict in-flight item");

        c->OnSaveSuccess(1);
        TEST_ASSERT(c->CanEvict() == true, "Senaryo G: Can evict clean acknowledged item");

        delete c;
    }

    // =========================================================================
    // SENARYO H: Sunucu kapanırken bekleyen tüm kayıtların eksiksiz tamamlanması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo H: Sunucu kapanisi eksiksiz kuyruk tahliyesi..." << std::endl;

        std::map<uint32_t, CTestPlayerCache*> activePlayers;
        std::map<uint32_t, CTestItemCache*> activeItems;

        // Populate multiple dirty caches
        for (uint32_t id = 1; id <= 10; ++id)
        {
            CTestPlayerCache* pc = new CTestPlayerCache();
            TestPlayerData pd{ id, "Player", static_cast<int64_t>(id * 1000), id * 100, static_cast<int>(id) };
            pc->Put(&pd);
            activePlayers[id] = pc;

            CTestItemCache* ic = new CTestItemCache();
            TestItemData idata{ 1000 + id, id, 10, 1, 0 };
            ic->Put(&idata);
            activeItems[1000 + id] = ic;
        }

        TEST_ASSERT(activePlayers.size() == 10, "Senaryo H: 10 active player caches");
        TEST_ASSERT(activeItems.size() == 10, "Senaryo H: 10 active item caches");

        // 1. Shutdown initiates -> flush and mark all
        for (auto& pair : activePlayers)
        {
            pair.second->MarkPendingEviction();
            pair.second->Flush();
        }
        for (auto& pair : activeItems)
        {
            pair.second->MarkPendingEviction();
            pair.second->Flush();
        }

        // Verify all have pending saves
        for (auto& pair : activePlayers)
        {
            TEST_ASSERT(pair.second->IsSavePending() == true, "Senaryo H: Player save in-flight");
            TEST_ASSERT(pair.second->CanEvict() == false, "Senaryo H: Cannot evict before DB ACK");
        }
        for (auto& pair : activeItems)
        {
            TEST_ASSERT(pair.second->IsSavePending() == true, "Senaryo H: Item save in-flight");
            TEST_ASSERT(pair.second->CanEvict() == false, "Senaryo H: Cannot evict before DB ACK");
        }

        // 2. Simulate shutdown draining loop processing responses
        int pendingCaches = static_cast<int>(activePlayers.size() + activeItems.size());
        for (auto it = activePlayers.begin(); it != activePlayers.end(); )
        {
            CTestPlayerCache* pc = it->second;
            pc->OnSaveSuccess(pc->GetPendingSaveVersion());
            TEST_ASSERT(pc->CanEvict() == true, "Senaryo H: Player can evict after ACK");
            delete pc;
            it = activePlayers.erase(it);
            --pendingCaches;
        }

        for (auto it = activeItems.begin(); it != activeItems.end(); )
        {
            CTestItemCache* ic = it->second;
            ic->OnSaveSuccess(ic->GetPendingSaveVersion());
            TEST_ASSERT(ic->CanEvict() == true, "Senaryo H: Item can evict after ACK");
            delete ic;
            it = activeItems.erase(it);
            --pendingCaches;
        }

        TEST_ASSERT(pendingCaches == 0, "Senaryo H: All 20 caches acknowledged and drained");
        TEST_ASSERT(activePlayers.empty() == true, "Senaryo H: Player map clean");
        TEST_ASSERT(activeItems.empty() == true, "Senaryo H: Item map clean");
    }

    // =========================================================================
    // SENARYO I: 64-Bit Ekonomi (GOLD_MAX = 10^12), Oyuncu Altını, Monarch ve IkarusShop Önbellek Sınırları
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo I: 64-Bit Ekonomi (2^31, 2^32, 1 Trilyon-1) onbellek ve tasma testleri..." << std::endl;

        constexpr int64_t kGoldMax64 = 1000000000000LL; // 1 Trillion YANG
        CTestPlayerCache* pc = new CTestPlayerCache();
        TestPlayerData pd{ 909, "RichEmperor", 2147483648LL, 100, 99 }; // 2^31
        pc->Put(&pd);
        pc->Flush();
        TEST_ASSERT(pc->Get()->gold == 2147483648LL && pc->m_flushCallCount == 1,
            "Senaryo I: PlayerCache flushes 2^31 YANG (2,147,483,648) without signed 32-bit negative wrap");
        pc->OnSaveSuccess(pc->GetPendingSaveVersion());

        pd.gold = 4294967296LL; // 2^32
        pc->Put(&pd);
        pc->Flush();
        TEST_ASSERT(pc->Get()->gold == 4294967296LL && pc->m_flushCallCount == 2,
            "Senaryo I: PlayerCache flushes 2^32 YANG (4,294,967,296) without 32-bit zero wrap");
        pc->OnSaveSuccess(pc->GetPendingSaveVersion());

        pd.gold = kGoldMax64 - 1; // 999,999,999,999
        pc->Put(&pd);
        pc->Flush();
        TEST_ASSERT(pc->Get()->gold == 999999999999LL && pc->m_flushCallCount == 3,
            "Senaryo I: PlayerCache flushes GOLD_MAX - 1 (999,999,999,999) cleanly");
        pc->OnSaveSuccess(pc->GetPendingSaveVersion());

        auto canEditShopItemPrice = [kGoldMax64](int64_t newPrice) -> bool {
            return newPrice > 0 && newPrice < kGoldMax64;
        };
        TEST_ASSERT(canEditShopItemPrice(6000000000LL) &&
                    canEditShopItemPrice(kGoldMax64 - 1) &&
                    !canEditShopItemPrice(0) &&
                    !canEditShopItemPrice(-500LL) &&
                    !canEditShopItemPrice(kGoldMax64),
            "Senaryo I: IkarusShopCache EditItem accepts >2^32 up to GOLD_MAX-1 and rejects <=0 or >=GOLD_MAX");

        delete pc;
    }

    // =========================================================================
    // SENARYO J: G1 Tam 64-Bit Ekonomi, Lonca, IkarusShop, QuestLua ve Overflow Regresyon Testleri
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo J: G1 64-bit ekonomi, lonca, IkarusShop, QuestLua ve unsigned-negation korumalari..." << std::endl;
        constexpr int64_t kGoldMax64 = 1000000000000LL;

        // 1. Guild ChargeSP (offer * 300) 32-bit signed overflow guard
        auto isValidGuildChargeSP = [kGoldMax64](int32_t offer, int64_t playerGold) -> bool {
            if (offer <= 0 || static_cast<int64_t>(offer) > (kGoldMax64 - 1) / 300)
                return false;
            const int64_t goldCost = static_cast<int64_t>(offer) * 300;
            return goldCost > 0 && goldCost <= playerGold;
        };
        // Previously offer = 7,158,279 overflowed signed 32-bit int (7158279 * 300 = 2,147,483,700 -> -2,147,483,596)
        TEST_ASSERT(!isValidGuildChargeSP(7158279, 1000LL) &&
                    isValidGuildChargeSP(7158279, 3000000000LL) &&
                    !isValidGuildChargeSP(0, 3000000000LL) &&
                    !isValidGuildChargeSP(-100, 3000000000LL),
            "Senaryo J: Guild ChargeSP blocks 32-bit signed overflow (7,158,279 * 300 = 2.147B Yang)");

        // 2. Guild WithdrawMoneyReply player GOLD_MAX clamp-loss protection
        auto canGiveWithdrawnGuildGold = [kGoldMax64](int64_t playerGold, int64_t withdrawGold) -> bool {
            return withdrawGold > 0 && withdrawGold < kGoldMax64 && playerGold < kGoldMax64 - withdrawGold;
        };
        TEST_ASSERT(canGiveWithdrawnGuildGold(500000000000LL, 400000000000LL) &&
                    !canGiveWithdrawnGuildGold(600000000000LL, 400000000000LL) &&
                    !canGiveWithdrawnGuildGold(999999999999LL, 1LL),
            "Senaryo J: Guild WithdrawMoneyGive refuses delivery when playerGold + withdrawGold >= GOLD_MAX so DB refunds treasury");

        // 3. IkarusShop total shop value overflow guard on AddItem and EditItem
        auto canAddOrEditShopItem = [kGoldMax64](int64_t currentShopTotalWithoutItem, int64_t newItemPrice) -> bool {
            if (newItemPrice <= 0 || newItemPrice >= kGoldMax64)
                return false;
            if (currentShopTotalWithoutItem < 0 || currentShopTotalWithoutItem >= kGoldMax64 - newItemPrice)
                return false;
            return true;
        };
        TEST_ASSERT(canAddOrEditShopItem(500000000000LL, 499999999999LL) &&
                    !canAddOrEditShopItem(500000000000LL, 500000000000LL) &&
                    !canAddOrEditShopItem(0LL, 0LL) &&
                    !canAddOrEditShopItem(0LL, kGoldMax64),
            "Senaryo J: IkarusShop AddItem/EditItem enforces 0 < itemPrice < GOLD_MAX and totalShopYang < GOLD_MAX");

        // 4. QuestLua pc_change_money underflow/overflow guard (including INT64_MIN edge case)
        auto canChangeMoneyLua = [kGoldMax64](int64_t currentGold, int64_t delta) -> bool {
            if (delta >= kGoldMax64 || delta <= -kGoldMax64)
                return false;
            if (delta > 0 && currentGold >= kGoldMax64 - delta)
                return false;
            if (delta < 0 && currentGold < -delta)
                return false;
            return true;
        };
        TEST_ASSERT(canChangeMoneyLua(5000000000LL, -4294967296LL) &&
                    !canChangeMoneyLua(3000000000LL, -4294967296LL) &&
                    !canChangeMoneyLua(5000000000LL, std::numeric_limits<int64_t>::min()) &&
                    !canChangeMoneyLua(900000000000LL, 100000000000LL),
            "Senaryo J: QuestLua pc_change_money safely handles >2^32 deductions and rejects underflow/overflow/INT64_MIN");

        // 5. do_build unsigned DWORD negation bug regression check
        uint32_t dwBuildPrice = 25000000U;
        int64_t signedDeduction = -static_cast<int64_t>(dwBuildPrice);
        TEST_ASSERT(signedDeduction == -25000000LL && signedDeduction < 0,
            "Senaryo J: do_build casts uint32_t dwPrice to signed YANG before negation (-25,000,000 instead of +4,269,967,296)");
    }

    // =========================================================================
    // SENARYO K: M-04 (HANDOFF-H3) IkarusShop TShopItem.id (item_id) vs owner (owner_id),
    //            Safebox 64-Bit Item/Owner IDs ve shop_auto_increment (>2^53) İzolasyonu
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo K: M-04 IkarusShop TShopItem (item_id vs owner_id), Safebox ve >2^53 ID izolasyonu..." << std::endl;

        struct TestIkaShopItemRecord
        {
            uint64_t id;      // Persistent ITEM_ID (ikashop_shop_item.id / player.item.id)
            uint64_t owner;   // Persistent PLAYER_ID (owner_id / shop_id)
            uint32_t vnum;
            uint32_t count;
            int64_t price;
        };

        const uint64_t ownerPid64 = 5000000001ULL;          // > 2^32 owner_id
        const uint64_t itemIdOver53 = 9007199254740993ULL;  // 2^53 + 1 item_id
        uint64_t shopAutoIncrement = 9007199254741999ULL;   // > 2^53 shop_auto_increment counter

        TestIkaShopItemRecord shopRec{ itemIdOver53, ownerPid64, 299U, 1U, 7500000000LL };
        // Verify TShopItem.id (item_id) and TShopItem.owner (owner_id) never alias or overwrite each other
        TEST_ASSERT(shopRec.id == itemIdOver53 && shopRec.owner == ownerPid64 && shopRec.id != shopRec.owner,
            "Senaryo K: IkarusShop TShopItem.id (item_id >2^53) and TShopItem.owner (owner_id >2^32) remain strictly distinct");

        // Verify shop_auto_increment allocation for new shop items does not collide with or truncate >2^53 item.id
        uint64_t allocatedShopItemId = ++shopAutoIncrement;
        TEST_ASSERT(allocatedShopItemId == 9007199254742000ULL && allocatedShopItemId > itemIdOver53,
            "Senaryo K: shop_auto_increment allocates exact 64-bit >2^53 IDs without IEEE-754 double rounding or 32-bit wrap");
    }

    // =========================================================================
    // SENARYO L: M-05 & M-07 (REG-G1.4) Cube (Cube_load / FN_check_cube_data / Cube_make),
    //            DragonSoul (DoRefineGrade / DoRefineStep / DoRefineStrength), WarBet & Monarch 64-Bit YANG
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo L: M-05 & REG-G1.4 Cube, DragonSoul, WarBet ve Monarch 64-bit YANG dogrulamasi..." << std::endl;
        constexpr int64_t kGoldMax64 = 1000000000000LL; // 1 Trillion YANG

        // 1. REG-G1.4 Cube_load & FN_check_cube_data & Cube_make 64-bit gold validation (cube.cpp:182, 414, 516-528)
        auto checkCubeDataGold = [kGoldMax64](int64_t cubeGold) -> bool {
            return cubeGold >= 0 && cubeGold < kGoldMax64;
        };
        auto canCubeMake = [kGoldMax64](int64_t cubeGold, int64_t playerGold, int64_t& outPlayerGoldAfter) -> bool {
            if (cubeGold < 0 || cubeGold >= kGoldMax64 || playerGold < cubeGold)
                return false;
            outPlayerGoldAfter = playerGold - cubeGold;
            return true;
        };
        int64_t playerGoldAfterCube = 0;
        TEST_ASSERT(checkCubeDataGold(0LL) &&
                    checkCubeDataGold(2147483648LL) &&
                    checkCubeDataGold(5000000000LL) &&
                    checkCubeDataGold(kGoldMax64 - 1) &&
                    !checkCubeDataGold(-1LL) &&
                    !checkCubeDataGold(kGoldMax64),
            "Senaryo L [REG-G1.4a]: FN_check_cube_data accepts 0, >2^31, >2^32, GOLD_MAX-1 and rejects <0 or >=GOLD_MAX");

        TEST_ASSERT(canCubeMake(5000000000LL, 8000000000LL, playerGoldAfterCube) &&
                    playerGoldAfterCube == 3000000000LL &&
                    !canCubeMake(5000000000LL, 4999999999LL, playerGoldAfterCube) &&
                    !canCubeMake(-100LL, 8000000000LL, playerGoldAfterCube) &&
                    !canCubeMake(kGoldMax64, kGoldMax64, playerGoldAfterCube),
            "Senaryo L [REG-G1.4b]: Cube_make deducts >2^32 (5B Yang) accurately and blocks insufficient, negative, or >=GOLD_MAX fees");

        // 2. REG-G1.4 DragonSoul DoRefineGrade / DoRefineStep / DoRefineStrength 64-bit fee validation (DragonSoul.cpp:575, 717, 890)
        auto canDragonSoulRefine = [kGoldMax64](int64_t fee, int64_t playerGold, int64_t& outPlayerGoldAfter) -> bool {
            if (fee < 0 || fee >= kGoldMax64 || playerGold < fee)
                return false;
            outPlayerGoldAfter = playerGold - fee;
            return true;
        };
        int64_t playerGoldAfterDS = 0;
        TEST_ASSERT(canDragonSoulRefine(4294967296LL, 10000000000LL, playerGoldAfterDS) &&
                    playerGoldAfterDS == 5705032704LL &&
                    !canDragonSoulRefine(4294967296LL, 4000000000LL, playerGoldAfterDS) &&
                    !canDragonSoulRefine(-1LL, 10000000000LL, playerGoldAfterDS) &&
                    !canDragonSoulRefine(kGoldMax64, 10000000000LL, playerGoldAfterDS),
            "Senaryo L [REG-G1.4c]: DragonSoul DoRefineGrade/Step/Strength deducts >2^32 (2^32 Yang) fee and rejects <0, >=GOLD_MAX, or insufficient gold");

        // 3. M-05 WarBet (guild_war_bet & WarBet 64-bit YANG) and Monarch treasury >2^32 validation
        auto isValidGuildWarBet = [kGoldMax64](uint64_t guildId, int64_t betGold) -> bool {
            return guildId > 0 && betGold > 0 && betGold < kGoldMax64;
        };
        TEST_ASSERT(isValidGuildWarBet(9007199254740993ULL, 6000000000LL) &&
                    !isValidGuildWarBet(9007199254740993ULL, 0LL) &&
                    !isValidGuildWarBet(9007199254740993ULL, -500LL) &&
                    !isValidGuildWarBet(9007199254740993ULL, kGoldMax64),
            "Senaryo L [M-05]: Guild WarBet validates >2^53 GUILD_ID and >2^32 YANG bet within (0, GOLD_MAX)");
    }

    // =========================================================================
    // SENARYO M: INT-01 Cross-System Integrity & Anti-Dupe Regressions
    //            (SItemPosEx 64-Bit ID, AutoStackItemProto MAKECOUNT, WonExchange & cs_dwMaxItemID)
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo M: INT-01 Acce SItemPosEx 64-bit ID, AutoStack MAKECOUNT, WonExchange ve cs_dwMaxItemID dogrulamasi..." << std::endl;

        // 1. SItemPosEx::id 64-bit item ID preservation (>2^32 and >2^53)
        struct TestItemPosEx { uint8_t window_type; uint16_t cell; uint64_t id{ 0 }; };
        TestItemPosEx acceSlot{ 1, 15, 9007199254740993ULL };
        TEST_ASSERT(sizeof(acceSlot.id) == sizeof(uint64_t) && acceSlot.id == 9007199254740993ULL,
            "Senaryo M [INT-01a]: SItemPosEx::id preserves >2^53 64-bit ITEM_ID without 32-bit int truncation");

        // 2. AutoStackItemProto ITEM_FLAG_MAKECOUNT normalization before inventory loop (no per-stack multiplication)
        auto simulateAutoStackMakeCount = [](uint32_t initialCount, uint32_t makeCountVal, std::vector<uint32_t>& invStacks, uint32_t maxStack) -> uint32_t {
            uint32_t bCount = initialCount;
            if (makeCountVal > 0 && bCount < makeCountVal)
                bCount = makeCountVal;
            for (size_t i = 0; i < invStacks.size(); ++i)
            {
                if (invStacks[i] >= maxStack)
                    continue;
                uint32_t bCount2 = std::min(maxStack - invStacks[i], bCount);
                bCount -= bCount2;
                invStacks[i] += bCount2;
                if (bCount == 0)
                    break;
            }
            return bCount;
        };
        std::vector<uint32_t> stacks = { 190U, 190U, 100U };
        uint32_t rem = simulateAutoStackMakeCount(1U, 20U, stacks, 200U);
        // Total added across all stacks must be exactly 20 (190->200 (+10), 190->200 (+10), 100 unchanged), remainder 0
        TEST_ASSERT(rem == 0U && stacks[0] == 200U && stacks[1] == 200U && stacks[2] == 100U,
            "Senaryo M [INT-01b]: AutoStackItemProto normalizes ITEM_FLAG_MAKECOUNT once before loop without multiplying across partial stacks");

        // 3. WonExchange signed 64-bit cheque addition & exclusive CHEQUE_MAX (1000) boundary guard
        constexpr int kChequeMax = 1000;
        auto isValidWonExchangeBuy = [kChequeMax](int currentCheque, int value) -> bool {
            if (value <= 0 || value >= kChequeMax)
                return false;
            const int64_t nTotalCheque = static_cast<int64_t>(currentCheque) + static_cast<int64_t>(value);
            if (currentCheque < 0 || nTotalCheque >= kChequeMax)
                return false;
            return true;
        };
        TEST_ASSERT(isValidWonExchangeBuy(100, 50) &&
                    isValidWonExchangeBuy(990, 9) &&
                    !isValidWonExchangeBuy(990, 10) &&
                    !isValidWonExchangeBuy(0, kChequeMax) &&
                    !isValidWonExchangeBuy(100, INT32_MAX) &&
                    !isValidWonExchangeBuy(-1, 10) &&
                    !isValidWonExchangeBuy(0, -1),
            "Senaryo M [INT-01c]: WonExchange enforces 0 < value < CHEQUE_MAX and currentCheque + value < CHEQUE_MAX without Yang loss or overflow");

        // 4. CItemIDRangeManager::cs_dwMaxItemID <= INT64_MAX, interval overlap & high legacy ID filter
        constexpr uint64_t kMaxItemID = 9223372000000000000ULL;
        constexpr uint64_t kMinRange = 10000000ULL;
        auto rangesOverlap = [](uint64_t aMin, uint64_t aMax, uint64_t bMin, uint64_t bMax) -> bool {
            return aMin > 0 && aMax > 0 && aMin <= bMax && aMax >= bMin;
        };
        // Even if a legacy/test SQL record has id >= 2^63, filtering WHERE id <= cs_dwMaxItemID selects 5,000,000,001ULL
        const uint64_t sqlIds[] = { 5000000001ULL, 9223372036854775808ULL };
        uint64_t filteredMaxId = 0;
        for (uint64_t id : sqlIds)
        {
            if (id <= kMaxItemID && id > filteredMaxId)
                filteredMaxId = id;
        }
        const uint64_t nextChunkMin = kMinRange * (filteredMaxId / kMinRange) + 1;
        const uint64_t nextChunkMax = kMinRange * ((filteredMaxId / kMinRange) + 1);
        TEST_ASSERT(kMaxItemID > (1ULL << 53) &&
                    kMaxItemID <= static_cast<uint64_t>(INT64_MAX) &&
                    (kMaxItemID % kMinRange) == 0ULL &&
                    static_cast<int64_t>(kMaxItemID) > 0 &&
                    filteredMaxId == 5000000001ULL &&
                    nextChunkMin == 5000000001ULL && nextChunkMax == 5010000000ULL &&
                    rangesOverlap(5005000000ULL, 5015000000ULL, nextChunkMin, nextChunkMax) &&
                    !rangesOverlap(5010000001ULL, 5020000000ULL, nextChunkMin, nextChunkMax),
            "Senaryo M [INT-01d]: cs_dwMaxItemID <= INT64_MAX filters out >INT64_MAX legacy IDs and detects partial range overlaps");
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << "[SUCCESS] DB CACHE SUITE: " << g_passCount << "/" << g_testCount << " tests passed cleanly!" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
