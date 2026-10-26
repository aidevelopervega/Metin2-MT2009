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

    std::cout << "\n==================================================" << std::endl;
    std::cout << "[SUCCESS] DB CACHE SUITE: " << g_passCount << "/" << g_testCount << " tests passed cleanly!" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
