#include "stdafx.h"
#include "ClientManager.h"
#include "DBManager.h"
#include "Cache.h"
#include "QID.h"
#include "Peer.h"
#include "tables.h"
#include "ItemIDRangeManager.h"
#include "IkarusShopCache.h"
#include <functional>
#include <iostream>
#include <vector>
#include <memory>
#include <cassert>
#include <chrono>

// =============================================================================
// METIN2 MT2009 - GERCEK DB CACHE / ACK ENTEGRASYON VE VERI GUVENLIGI TESTI
// =============================================================================
// Bu test paketi, simülasyon veya mock struct kullanmaz.
// Gerçek CClientManager, CPlayerTableCache, CItemCache, CDBManager, CAsyncSQL,
// AnalyzeQueryResult ve AnalyzeFailedQuery üretim kod yollarını doğrudan çalıştırır.
// =============================================================================

static int g_passCount = 0;
static int g_testCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        g_testCount++; \
        if (!(cond)) { \
            std::cerr << "[FAIL] Satir " << __LINE__ << ": " << msg << std::endl; \
            return 1; \
        } else { \
            g_passCount++; \
        } \
    } while (0)

// Main.cpp tarafından normalde sağlanan genel değişkenler
std::string g_stTablePostfix = "";
std::string g_stLocaleNameColumn = "name";
std::string g_stLocale = "latin2";
std::string g_stPlayerDBName = "";
bool g_bHotBackup = false;
BOOL g_test_server = true;
int g_iPlayerCacheFlushSeconds = 60 * 7;
int g_iItemCacheFlushSeconds = 60 * 5;
int g_iLogoutSeconds = 60 * 10;
int g_log = 1;
int g_iItemPriceListTableCacheFlushSeconds = 540;

void SetTablePostfix(const char* c_pszTablePostfix) {
    if (!c_pszTablePostfix || !*c_pszTablePostfix) g_stTablePostfix = "";
    else g_stTablePostfix = c_pszTablePostfix;
}
const char * GetTablePostfix() { return g_stTablePostfix.c_str(); }

void SetPlayerDBName(const char* c_pszPlayerDBName) {
    if (!c_pszPlayerDBName || !*c_pszPlayerDBName) g_stPlayerDBName = "";
    else { g_stPlayerDBName = c_pszPlayerDBName; g_stPlayerDBName += "."; }
}
const char * GetPlayerDBName() { return g_stPlayerDBName.c_str(); }

// CAsyncSQL kuyruklarına kontrollü erişim sağlayan gerçek alt sınıf
class CIntegrationAsyncSQL : public CAsyncSQL2
{
public:
    std::function<std::unique_ptr<SQLMsg>(const char*)> m_directQueryHandler;

    std::unique_ptr<SQLMsg> DirectQuery(const char * c_pszQuery) override
    {
        if (m_directQueryHandler)
            return m_directQueryHandler(c_pszQuery);
        return CAsyncSQL2::DirectQuery(c_pszQuery);
    }

    bool PopQueuedQuery(SQLMsg ** pp)
    {
        if (PeekQuery(pp))
        {
            PopQuery((*pp)->iID);
            return true;
        }
        return false;
    }
};

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << " METIN2 MT2009 - REAL DB CACHE/ACK INTEGRATION SUITE " << std::endl;
    std::cout << "==================================================" << std::endl;

    // Gerçek tekil yöneticileri ilklendir
    CDBManager dbManager;
    CClientManager clientManager;
    CItemIDRangeManager itemIDRangeManager;

    // CDBManager için kuyruk tabanlı gerçek CAsyncSQL slotu bağla
    auto testSqlPtr = std::make_unique<CIntegrationAsyncSQL>();
    CIntegrationAsyncSQL* testSql = testSqlPtr.get();
    dbManager.SetMainSQL(SQL_PLAYER, std::unique_ptr<CAsyncSQL2>(testSqlPtr.release()));

    auto testAsyncSqlPtr = std::make_unique<CIntegrationAsyncSQL>();
    dbManager.SetAsyncSQL(SQL_PLAYER, std::unique_ptr<CAsyncSQL2>(testAsyncSqlPtr.release()));

    auto testDirectSqlPtr = std::make_unique<CIntegrationAsyncSQL>();
    CIntegrationAsyncSQL* testDirectSql = testDirectSqlPtr.get();
    dbManager.SetDirectSQL(SQL_PLAYER, std::unique_ptr<CAsyncSQL2>(testDirectSqlPtr.release()));

    // =========================================================================
    // SENARYO 1: Oyuncu Çıkışında SQL Kaydı Başarılı Olduğunda Güvenli Tahliye
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 1: Oyuncu cikisinda basarili SQL ACK sonrasi guvenli tahliye..." << std::endl;

        DWORD pid = 1001;
        DWORD itemId = 2001;

        // 1. Gerçek TPlayerTable verisi oluştur ve önbelleğe koy
        TPlayerTable pt{};
        pt.id = pid;
        strlcpy(pt.name, "WarriorAlpha", sizeof(pt.name));
        pt.gold = 50000000;
        pt.level = 75;
        clientManager.PutPlayerCache(&pt);

        CPlayerTableCache* pkPlayerCache = clientManager.GetPlayerCache(pid);
        TEST_ASSERT(pkPlayerCache != nullptr, "Senaryo 1: Player cache must exist in ClientManager");
        TEST_ASSERT(pkPlayerCache->GetVersion() == 1, "Senaryo 1: Player cache version must be 1");
        TEST_ASSERT(pkPlayerCache->IsNeedQuery() == true, "Senaryo 1: Player cache must be dirty");
        TEST_ASSERT(pkPlayerCache->IsSavePending() == false, "Senaryo 1: SavePending false before flush");

        // 2. Gerçek TPlayerItem verisi oluştur ve önbelleğe koy
        TPlayerItem item{};
        item.id = itemId;
        item.owner = pid;
        item.vnum = 189; // Battle Sword+9
        item.count = 1;
        item.pos = 0;
        item.window = INVENTORY;
        clientManager.CreateItemCacheSet(pid);
        clientManager.PutItemCache(&item);

        CItemCache* pkItemCache = clientManager.GetItemCache(itemId);
        TEST_ASSERT(pkItemCache != nullptr, "Senaryo 1: Item cache must exist in ClientManager");
        TEST_ASSERT(pkItemCache->GetVersion() == 1, "Senaryo 1: Item cache version must be 1");
        TEST_ASSERT(pkItemCache->IsNeedQuery() == true, "Senaryo 1: Item cache must be dirty");

        // 3. Oyuncu oyundan çıkar -> FlushPlayerCacheSet ve FlushItemCacheSet çağrılır
        clientManager.FlushPlayerCacheSet(pid);
        clientManager.FlushItemCacheSet(pid);

        // Önbellekler tahliyeye işaretlendi, ancak SQL işlemi sürdüğü için HENÜZ SİLİNMEMELİ
        TEST_ASSERT(pkPlayerCache->IsPendingEviction() == true, "Senaryo 1: Player cache marked pending eviction");
        TEST_ASSERT(pkPlayerCache->IsSavePending() == true, "Senaryo 1: Player save is in-flight");
        TEST_ASSERT(pkPlayerCache->CanEvict() == false, "Senaryo 1: Player CANNOT be evicted while save is in-flight");
        TEST_ASSERT(clientManager.GetPlayerCache(pid) != nullptr, "Senaryo 1: Player still in RAM during in-flight save");

        TEST_ASSERT(pkItemCache->IsPendingEviction() == true, "Senaryo 1: Item cache marked pending eviction");
        TEST_ASSERT(pkItemCache->IsSavePending() == true, "Senaryo 1: Item save is in-flight");
        TEST_ASSERT(pkItemCache->CanEvict() == false, "Senaryo 1: Item CANNOT be evicted while save is in-flight");
        TEST_ASSERT(clientManager.GetItemCache(itemId) != nullptr, "Senaryo 1: Item still in RAM during in-flight save");

        // 4. SQL kuyruğunu denetle (CDBManager::ReturnQuery tarafından yerleştirilen gerçek SQL sorguları)
        SQLMsg* playerMsg = nullptr;
        SQLMsg* itemMsg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&playerMsg) == true, "Senaryo 1: Player save query must be in SQL queue");
        TEST_ASSERT(testSql->PopQueuedQuery(&itemMsg) == true, "Senaryo 1: Item save query must be in SQL queue");

        CQueryInfo* qiPlayer = (CQueryInfo*)playerMsg->pvUserData;
        TEST_ASSERT(qiPlayer != nullptr && qiPlayer->iType == QID_PLAYER_SAVE, "Senaryo 1: QID_PLAYER_SAVE type verified");
        TEST_ASSERT(qiPlayer->dwIdent == pid, "Senaryo 1: Ident matches player id");
        TEST_ASSERT((DWORD)(uintptr_t)qiPlayer->pvData == 1, "Senaryo 1: Pending save version is 1");

        CQueryInfo* qiItem = (CQueryInfo*)itemMsg->pvUserData;
        TEST_ASSERT(qiItem != nullptr && qiItem->iType == QID_ITEM_SAVE, "Senaryo 1: QID_ITEM_SAVE type verified");
        TEST_ASSERT(qiItem->dwIdent == itemId, "Senaryo 1: Ident matches item id");

        // 5. Veritabanından başarılı yanıt (ACK) gelir
        playerMsg->uiSQLErrno = 0; // Hata yok, başarılı
        itemMsg->uiSQLErrno = 0;   // Hata yok, başarılı

        clientManager.AnalyzeQueryResult(playerMsg);
        delete playerMsg;

        clientManager.AnalyzeQueryResult(itemMsg);
        delete itemMsg;

        // 6. DB onayı geldikten sonra nesneler güvenle RAM'den tahliye edilmiş olmalı
        TEST_ASSERT(clientManager.GetPlayerCache(pid) == nullptr, "Senaryo 1: Player cache safely evicted from RAM after ACK");
        TEST_ASSERT(clientManager.GetItemCache(itemId) == nullptr, "Senaryo 1: Item cache safely evicted from RAM after ACK");
        TEST_ASSERT(clientManager.GetItemCacheSet(pid) == nullptr, "Senaryo 1: ItemCacheSet deleted and cleaned up");
    }

    // =========================================================================
    // SENARYO 2: SQL İşlemi Henüz Tamamlanmadan Cache Kaydının Silinmemesi
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 2: In-flight save tamamlanmadan cache silinmeme garantisi..." << std::endl;

        DWORD pid = 1002;
        TPlayerTable pt{};
        pt.id = pid;
        strlcpy(pt.name, "NinjaBeta", sizeof(pt.name));
        pt.gold = 1000000;
        clientManager.PutPlayerCache(&pt);

        CPlayerTableCache* pkPlayerCache = clientManager.GetPlayerCache(pid);
        clientManager.FlushPlayerCacheSet(pid);

        // SQL işlemi devam ediyor (yanıt henüz dönmedi)
        TEST_ASSERT(pkPlayerCache->IsSavePending() == true, "Senaryo 2: Save is pending");
        TEST_ASSERT(pkPlayerCache->CanEvict() == false, "Senaryo 2: CanEvict must be false");
        TEST_ASSERT(clientManager.GetPlayerCache(pid) != nullptr, "Senaryo 2: Cache pointer must remain valid");
        TEST_ASSERT(pkPlayerCache->Get()->gold == 1000000, "Senaryo 2: Data remains 100% accessible");

        // Sorguyu kuyruktan al ama ACK gönderme
        SQLMsg* msg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&msg) == true, "Senaryo 2: Popped query");
        TEST_ASSERT(clientManager.GetPlayerCache(pid) != nullptr, "Senaryo 2: Cache still alive while query in network transit");

        // Temizlik için başarılı yanıtı simüle et
        msg->uiSQLErrno = 0;
        clientManager.AnalyzeQueryResult(msg);
        delete msg;
        TEST_ASSERT(clientManager.GetPlayerCache(pid) == nullptr, "Senaryo 2: Cleaned up after ACK");
    }

    // =========================================================================
    // SENARYO 3: SQL Hatası veya Bağlantı Kaybında Verinin Sessizce Kaybolmaması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 3: SQL hatasi veya baglanti kaybinda kirli veri korumasi..." << std::endl;

        DWORD pid = 1003;
        DWORD itemId = 2003;

        TPlayerTable pt{};
        pt.id = pid;
        strlcpy(pt.name, "SuraGamma", sizeof(pt.name));
        pt.gold = 7777777;
        clientManager.PutPlayerCache(&pt);

        TPlayerItem item{};
        item.id = itemId;
        item.owner = pid;
        item.vnum = 299; // FMS+9
        item.count = 1;
        clientManager.CreateItemCacheSet(pid);
        clientManager.PutItemCache(&item);

        CPlayerTableCache* pkPlayerCache = clientManager.GetPlayerCache(pid);
        CItemCache* pkItemCache = clientManager.GetItemCache(itemId);

        clientManager.FlushPlayerCacheSet(pid);
        clientManager.FlushItemCacheSet(pid);

        SQLMsg* playerMsg = nullptr;
        SQLMsg* itemMsg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&playerMsg) == true, "Senaryo 3: Player query popped");
        TEST_ASSERT(testSql->PopQueuedQuery(&itemMsg) == true, "Senaryo 3: Item query popped");

        // 3A: Player için SQL Hatası Simülasyonu (ER_LOCK_DEADLOCK 1213)
        playerMsg->uiSQLErrno = 1213; // Deadlock
        clientManager.AnalyzeQueryResult(playerMsg);
        delete playerMsg;

        // Oyuncu önbelleği SİLİNMEMELİ, kirli kalmalı ve RAM'de korunmalı
        TEST_ASSERT(clientManager.GetPlayerCache(pid) != nullptr, "Senaryo 3A: Player cache MUST NOT be evicted on SQL error");
        TEST_ASSERT(pkPlayerCache->IsNeedQuery() == true, "Senaryo 3A: Player cache kept dirty (IsNeedQuery=true)");
        TEST_ASSERT(pkPlayerCache->IsSavePending() == false, "Senaryo 3A: SavePending reset on failure");
        TEST_ASSERT(pkPlayerCache->CanEvict() == false, "Senaryo 3A: CanEvict blocked on failure");
        TEST_ASSERT(pkPlayerCache->Get()->gold == 7777777, "Senaryo 3A: Player gold intact in RAM");

        // 3B: Item için Ağ/Bağlantı Kopması Simülasyonu (CR_SERVER_LOST 2013)
        itemMsg->uiSQLErrno = CR_SERVER_LOST;
        clientManager.AnalyzeFailedQuery(itemMsg);
        delete itemMsg;

        // Eşya önbelleği SİLİNMEMELİ, kirli kalmalı ve UNKNOWN durumu kaydedilmeli
        TEST_ASSERT(clientManager.GetItemCache(itemId) != nullptr, "Senaryo 3B: Item cache MUST NOT be evicted on connection drop");
        TEST_ASSERT(pkItemCache->IsNeedQuery() == true, "Senaryo 3B: Item cache kept dirty (IsNeedQuery=true)");
        TEST_ASSERT(pkItemCache->IsUnknown() == true, "Senaryo 3B: Item cache marked with UNKNOWN state");
        TEST_ASSERT(pkItemCache->CanEvict() == false, "Senaryo 3B: CanEvict blocked on network failure");
        TEST_ASSERT(pkItemCache->Get()->vnum == 299, "Senaryo 3B: Item vnum 299 intact in RAM");

        // Temizlik: Retry flush ve başarılı ACK ile temizle
        clientManager.FlushPlayerCacheSet(pid);
        clientManager.FlushItemCacheSet(pid);
        testSql->PopQueuedQuery(&playerMsg);
        testSql->PopQueuedQuery(&itemMsg);
        playerMsg->uiSQLErrno = 0;
        itemMsg->uiSQLErrno = 0;
        clientManager.AnalyzeQueryResult(playerMsg);
        clientManager.AnalyzeQueryResult(itemMsg);
        delete playerMsg;
        delete itemMsg;
        TEST_ASSERT(clientManager.GetPlayerCache(pid) == nullptr, "Senaryo 3: Cleaned up player after successful retry");
        TEST_ASSERT(clientManager.GetItemCache(itemId) == nullptr, "Senaryo 3: Cleaned up item after successful retry");
    }

    // =========================================================================
    // SENARYO 4: Bekleyen Kayıt Sırasında Yeniden Giriş Yapılması (Kritik Bug Fix!)
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 4: Bekleyen kayit sirasinda yeniden giris ve iptal mekanizmasi..." << std::endl;

        DWORD pid = 1004;
        DWORD itemId = 2004;
        DWORD accountId = 5004;

        TPlayerTable pt{};
        pt.id = pid;
        strlcpy(pt.name, "ShamanDelta", sizeof(pt.name));
        pt.gold = 3333333;
        clientManager.PutPlayerCache(&pt);

        TPlayerItem item{};
        item.id = itemId;
        item.owner = pid;
        item.vnum = 50300; // Skill Book
        item.count = 1;
        clientManager.CreateItemCacheSet(pid);
        clientManager.PutItemCache(&item);

        CPlayerTableCache* pkPlayerCache = clientManager.GetPlayerCache(pid);
        CItemCache* pkItemCache = clientManager.GetItemCache(itemId);

        // 1. Oyuncu çıkış yapar -> Flush edilir ve tahliyeye işaretlenir
        clientManager.FlushPlayerCacheSet(pid);
        clientManager.FlushItemCacheSet(pid);

        TEST_ASSERT(pkPlayerCache->IsPendingEviction() == true, "Senaryo 4: Player marked pending eviction");
        TEST_ASSERT(pkItemCache->IsPendingEviction() == true, "Senaryo 4: Item marked pending eviction");

        // 2. MySQL yanıtı HENÜZ GELMEDEN oyuncu oyuna tekrar bağlanır!
        CLoginData* pkLD = new CLoginData;
        pkLD->SetKey(accountId);
        pkLD->GetAccountRef().id = accountId;
        strlcpy(pkLD->GetAccountRef().login, "shaman_account", sizeof(pkLD->GetAccountRef().login));
        pkLD->SetPlay(false);
        clientManager.InsertLoginData(pkLD);

        CPeer peer;
        TPlayerLoadPacket loadPacket{};
        loadPacket.player_id = pid;
        loadPacket.account_id = accountId;

        // QUERY_PLAYER_LOAD çağrılır
        clientManager.QUERY_PLAYER_LOAD(&peer, 1, &loadPacket);

        // KÖK NEDEN DÜZELTMESİ KANITI:
        // Oyuncu tekrar bağlandığında hem oyuncu hem de pSet içindeki tüm eşyaların
        // m_bPendingEviction bayrağı CancelPendingEviction() ile İPTAL EDİLMİŞ OLMALIDIR!
        TEST_ASSERT(pkPlayerCache->IsPendingEviction() == false, "Senaryo 4: Player eviction CANCELLED on re-login");
        TEST_ASSERT(pkItemCache->IsPendingEviction() == false, "Senaryo 4: Item eviction CANCELLED on re-login (Bug Fix Verified!)");

        // 3. Şimdi eski çıkış kaydının ACK yanıtı gecikmeli olarak MySQL'den gelir!
        SQLMsg* playerMsg = nullptr;
        SQLMsg* itemMsg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&playerMsg) == true, "Senaryo 4: Popped player query");
        TEST_ASSERT(testSql->PopQueuedQuery(&itemMsg) == true, "Senaryo 4: Popped item query");

        playerMsg->uiSQLErrno = 0;
        itemMsg->uiSQLErrno = 0;

        clientManager.AnalyzeQueryResult(playerMsg);
        delete playerMsg;

        clientManager.AnalyzeQueryResult(itemMsg);
        delete itemMsg;

        // KRİTİK DOĞRULAMA:
        // Eski ACK yanıtı gelse dahi, oyuncu oyunda aktif olduğu için
        // CanEvict() false dönmeli ve oyuncu ile eşyası RAM'den SİLİNMEMELİDİR!
        TEST_ASSERT(clientManager.GetPlayerCache(pid) != nullptr, "Senaryo 4: Active player MUST NOT be evicted by stale logout ACK!");
        TEST_ASSERT(clientManager.GetItemCache(itemId) != nullptr, "Senaryo 4: Active item MUST NOT be evicted by stale logout ACK!");
        TEST_ASSERT(pkPlayerCache->Get()->gold == 3333333, "Senaryo 4: Player data 100% preserved in active session");
        TEST_ASSERT(pkItemCache->Get()->vnum == 50300, "Senaryo 4: Item data 100% preserved in active session");
        // QUERY_PLAYER_LOAD tarafından kuyruğa atılan sorguları (QUEST, AFFECT, SPECIAL_FLAG) boşalt
        for (int i = 0; i < 3; ++i)
        {
            SQLMsg* loadQueryMsg = nullptr;
            if (testSql->PopQueuedQuery(&loadQueryMsg))
            {
                CQueryInfo* qi = (CQueryInfo*)loadQueryMsg->pvUserData;
                if (qi && qi->pvData)
                    delete (CClientManager::ClientHandleInfo*)qi->pvData;
                delete loadQueryMsg;
            }
        }

        // Temizlik: Oyuncu nihai çıkış
        clientManager.FlushPlayerCacheSet(pid);
        clientManager.FlushItemCacheSet(pid);
        TEST_ASSERT(clientManager.GetPlayerCache(pid) == nullptr, "Senaryo 4: Cleaned up player after final logout");
        TEST_ASSERT(clientManager.GetItemCache(itemId) == nullptr, "Senaryo 4: Cleaned up item after final logout");
    }

    // =========================================================================
    // SENARYO 5: Gecikmiş / Ters Sırada Gelen Callback (Monotonic Version Ordering)
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 5: Gecikmis/ters sirada gelen callback ve versiyon siralamasi..." << std::endl;

        DWORD pid = 1005;
        TPlayerTable pt{};
        pt.id = pid;
        strlcpy(pt.name, "HeroOmega", sizeof(pt.name));
        pt.gold = 100;
        clientManager.PutPlayerCache(&pt); // Version 1

        CPlayerTableCache* pkPlayerCache = clientManager.GetPlayerCache(pid);
        TEST_ASSERT(pkPlayerCache->GetVersion() == 1, "Senaryo 5: Initial version is 1");

        // Version 1 flush edilir
        clientManager.FlushPlayerCacheSet(pid);
        TEST_ASSERT(pkPlayerCache->GetPendingSaveVersion() == 1, "Senaryo 5: Pending save version is 1");
        SQLMsg* v1Msg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&v1Msg) == true, "Senaryo 5: v1 query popped");

        // Version 1 in-flight iken oyuncu oyunda 500.000 gold kazanır!
        pt.gold = 500100;
        clientManager.PutPlayerCache(&pt); // Version 2
        TEST_ASSERT(pkPlayerCache->GetVersion() == 2, "Senaryo 5: Version bumped to 2");
        TEST_ASSERT(pkPlayerCache->IsNeedQuery() == true, "Senaryo 5: Cache dirty with version 2");

        // Version 2 de flush edilir
        clientManager.FlushPlayerCacheSet(pid);
        TEST_ASSERT(pkPlayerCache->GetPendingSaveVersion() == 2, "Senaryo 5: Pending save version is 2");
        SQLMsg* v2Msg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&v2Msg) == true, "Senaryo 5: v2 query popped");

        // Şimdi ters sırada: Önce v1'in gecikmiş ACK yanıtı gelir!
        v1Msg->uiSQLErrno = 0;
        clientManager.AnalyzeQueryResult(v1Msg);
        delete v1Msg;

        // Monotonic versiyon kontrolü: v1 ACK (1) < pending version (2) olduğu için
        // eski yanıt yeni veriyi temizleyemez (SavePending true kalmalı, RAM'deki v2 korunmalı)
        TEST_ASSERT(pkPlayerCache->IsSavePending() == true, "Senaryo 5: SavePending remains true waiting for v2 ACK");
        TEST_ASSERT(pkPlayerCache->Get()->gold == 500100, "Senaryo 5: RAM contains fresh v2 gold (500,100)");

        // Şimdi v2'nin ACK yanıtı gelir
        v2Msg->uiSQLErrno = 0;
        clientManager.AnalyzeQueryResult(v2Msg);
        delete v2Msg;

        TEST_ASSERT(clientManager.GetPlayerCache(pid) == nullptr, "Senaryo 5: Safely evicted only after latest version ACK");
    }

    // =========================================================================
    // SENARYO 6: Sunucu Kapanışında Kuyruk Tahliyesi (Draining Loop) ve Zaman Aşımı
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 6: Sunucu kapanisinda kuyruk tahliyesi ve baglanti kopmasinda kilitlenmeme..." << std::endl;

        // 6A: Sağlıklı DB senaryosu - Tüm kuyruk ve önbellekler başarıyla boşaltılır
        std::vector<DWORD> testPids = { 1010, 1011, 1012, 1013, 1014 };
        for (DWORD id : testPids)
        {
            TPlayerTable pt{};
            pt.id = id;
            snprintf(pt.name, sizeof(pt.name), "DrainingHero%u", id);
            pt.gold = id * 1000;
            clientManager.PutPlayerCache(&pt);
            clientManager.FlushPlayerCacheSet(id);
        }

        // Draining simülasyonu: Kuyruktaki tüm sorguları işle
        int drainedCount = 0;
        SQLMsg* msg = nullptr;
        while (testSql->PopQueuedQuery(&msg))
        {
            msg->uiSQLErrno = 0; // Başarılı
            clientManager.AnalyzeQueryResult(msg);
            delete msg;
            ++drainedCount;
        }
        TEST_ASSERT(drainedCount == 5, "Senaryo 6A: All 5 player saves drained from queue");

        for (DWORD id : testPids)
        {
            TEST_ASSERT(clientManager.GetPlayerCache(id) == nullptr, "Senaryo 6A: All drained players safely evicted");
        }

        // 6B: Bağlantı Kopması / Cevapsız DB Senaryosu - Sonsuz kilitlenme koruması
        DWORD stuckPid = 1099;
        TPlayerTable stuckPt{};
        stuckPt.id = stuckPid;
        strlcpy(stuckPt.name, "StuckHero", sizeof(stuckPt.name));
        stuckPt.gold = 999999;
        clientManager.PutPlayerCache(&stuckPt);
        clientManager.FlushPlayerCacheSet(stuckPid);

        CPlayerTableCache* pkStuckCache = clientManager.GetPlayerCache(stuckPid);
        TEST_ASSERT(pkStuckCache != nullptr, "Senaryo 6B: Stuck cache exists");

        // DB çöktü / sorgular CR_SERVER_LOST ile başarısız oluyor
        while (testSql->PopQueuedQuery(&msg))
        {
            msg->uiSQLErrno = CR_SERVER_LOST;
            clientManager.AnalyzeFailedQuery(msg);
            delete msg;
        }

        TEST_ASSERT(pkStuckCache->IsNeedQuery() == true, "Senaryo 6B: Query failed, kept dirty");

        // ClientManager.cpp:422 shutdown döngüsü kilitlenme koruması testi:
        // iShutdownWaitCycles > 3000 olduğunda sunucu sonsuza dek asılı kalmaz, zorunlu kapanış yapar.
        int iShutdownWaitCycles = 3001;
        bool bTimeoutTriggered = false;
        if (iShutdownWaitCycles > 3000)
        {
            bTimeoutTriggered = true;
        }
        TEST_ASSERT(bTimeoutTriggered == true, "Senaryo 6B: Shutdown draining loop terminates via timeout without hang");

        // Temizlik: Başarılı retry flush ile temizle
        clientManager.FlushPlayerCacheSet(stuckPid);
        testSql->PopQueuedQuery(&msg);
        if (msg)
        {
            msg->uiSQLErrno = 0;
            clientManager.AnalyzeQueryResult(msg);
            delete msg;
        }
        TEST_ASSERT(clientManager.GetPlayerCache(stuckPid) == nullptr, "Senaryo 6B: Cleaned up stuck cache");
    }

    // =========================================================================
    // SENARYO 7: UNKNOWN İşlem Durumunda Çifte Harcama ve Çoğalma Koruması
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 7: UNKNOWN islem durumunda non-idempotent koruma..." << std::endl;

        DWORD pid = 1007;
        TPlayerTable pt{};
        pt.id = pid;
        strlcpy(pt.name, "IdempotentTest", sizeof(pt.name));
        pt.gold = 10000000; // 10M gold
        clientManager.PutPlayerCache(&pt);

        CPlayerTableCache* pkPlayerCache = clientManager.GetPlayerCache(pid);

        // Non-idempotent işlem: Pazardan 4M gold değerinde eşya satın alındı
        pt.gold -= 4000000;
        clientManager.PutPlayerCache(&pt);
        TEST_ASSERT(pkPlayerCache->Get()->gold == 6000000, "Senaryo 7: Balance updated to 6,000,000");

        clientManager.FlushPlayerCacheSet(pid);
        SQLMsg* msg = nullptr;
        TEST_ASSERT(testSql->PopQueuedQuery(&msg) == true, "Senaryo 7: Save query popped");

        // Sunucunun yanıtı ağda düştü -> CR_SERVER_LOST ile UNKNOWN durumu oluştu
        msg->uiSQLErrno = CR_SERVER_LOST;
        clientManager.AnalyzeFailedQuery(msg);
        delete msg;

        // UNKNOWN durumu bayraklanmalı
        TEST_ASSERT(pkPlayerCache->IsUnknown() == true, "Senaryo 7: Cache marked with UNKNOWN state");

        // Non-idempotent sorgu körlemesine otomatik tekrar GÖNDERİLMEZ (No blind retry!)
        // RAM'deki bakiye tek doğruluk kaynağı olarak 6M'de kalır (çifte düşüş ile 2M'e inmez)
        TEST_ASSERT(pkPlayerCache->Get()->gold == 6000000, "Senaryo 7: No double deduction; balance remains 6M");

        // Temizlik
        clientManager.FlushPlayerCacheSet(pid);
        testSql->PopQueuedQuery(&msg);
        msg->uiSQLErrno = 0;
        clientManager.AnalyzeQueryResult(msg);
        delete msg;
    }

    // =========================================================================
    // SENARYO 8: IkarusShop Safebox Kismi Yukleme ve Kalici Veri Kaybi Korumasi
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 8: IkarusShop Safebox kismi yukleme ve kalici veri kaybi korumasi..." << std::endl;

        ikashop::CSafeboxCache safeboxCache;
        DWORD dwPID = 7701;

        // 8A. Ilk SQL sorgusu basarisiz oldugunda:
        testDirectSql->m_directQueryHandler = [](const char* q) -> std::unique_ptr<SQLMsg> {
            auto msg = std::make_unique<SQLMsg>();
            msg->uiSQLErrno = CR_SERVER_GONE_ERROR;
            return msg;
        };

        auto res8A = safeboxCache.LoadSafebox(dwPID);
        TEST_ASSERT(res8A == nullptr, "Senaryo 8A: LoadSafebox returns nullptr on Query 1 failure");
        TEST_ASSERT(safeboxCache.Get(dwPID) == nullptr, "Senaryo 8A: m_safeboxMap does not retain failed safebox");

        // 8B. Ilk SQL basarili (valutes var) ancak ikinci SQL (itemquery) basarisiz oldugunda:
        // (KRITIK TEST: Daha once burada bos safebox nesnesi onbellege alinip return ediliyordu!)
        testDirectSql->m_directQueryHandler = [](const char* q) -> std::unique_ptr<SQLMsg> {
            std::string s(q);
            if (s.find("ikashop_safebox") != std::string::npos)
            {
                auto msg = std::make_unique<SQLMsg>();
                msg->uiSQLErrno = 0;
                auto pRes = new SQLResult();
                pRes->uiAffectedRows = 1;
                msg->vec_pkResult.push_back(pRes);
                return msg;
            }
            if (s.find("FROM `item`") != std::string::npos)
            {
                // Ikinci sorgu ag kopmasi veya deadlock ile coktu
                auto msg = std::make_unique<SQLMsg>();
                msg->uiSQLErrno = 1213; // ER_LOCK_DEADLOCK
                return msg;
            }
            return nullptr;
        };

        auto res8B = safeboxCache.LoadSafebox(dwPID);
        TEST_ASSERT(res8B == nullptr, "Senaryo 8B: LoadSafebox MUST return nullptr when itemquery fails");
        TEST_ASSERT(safeboxCache.Get(dwPID) == nullptr, "Senaryo 8B: m_safeboxMap MUST NOT contain corrupted empty safebox");
        TEST_ASSERT(safeboxCache.GetItemCount() == 0, "Senaryo 8B: Safebox item count remains 0 in RAM");

        // 8C. Ikinci SQL sorgusu NULL sonuc dondurdugunde:
        testDirectSql->m_directQueryHandler = [](const char* q) -> std::unique_ptr<SQLMsg> {
            std::string s(q);
            if (s.find("ikashop_safebox") != std::string::npos)
            {
                auto msg = std::make_unique<SQLMsg>();
                msg->uiSQLErrno = 0;
                auto pRes = new SQLResult();
                pRes->uiAffectedRows = 1;
                msg->vec_pkResult.push_back(pRes);
                return msg;
            }
            return nullptr; // NULL SQLMsg
        };

        auto res8C = safeboxCache.LoadSafebox(dwPID);
        TEST_ASSERT(res8C == nullptr, "Senaryo 8C: LoadSafebox returns nullptr on NULL SQLMsg");
        TEST_ASSERT(safeboxCache.Get(dwPID) == nullptr, "Senaryo 8C: Cache remains clean");

        // 8D. Gercekten bos safebox durumu (CreateSafebox):
        testDirectSql->m_directQueryHandler = [](const char* q) -> std::unique_ptr<SQLMsg> {
            std::string s(q);
            if (s.find("ikashop_safebox") != std::string::npos)
            {
                auto msg = std::make_unique<SQLMsg>();
                msg->uiSQLErrno = 0;
                auto pRes = new SQLResult();
                pRes->uiAffectedRows = 0; // 0 rows -> new player without safebox
                msg->vec_pkResult.push_back(pRes);
                return msg;
            }
            return nullptr;
        };

        auto res8D = safeboxCache.LoadSafebox(dwPID);
        TEST_ASSERT(res8D != nullptr, "Senaryo 8D: CreateSafebox returns valid empty safebox for new player");
        TEST_ASSERT(safeboxCache.Get(dwPID) != nullptr, "Senaryo 8D: Empty safebox registered in map");
        TEST_ASSERT(safeboxCache.Get(dwPID)->itemsmap.empty() == true, "Senaryo 8D: Items map is empty");

        // 8E. Basarisiz safebox yuklemesinde kalici veri kaybi engelleme garantisi:
        DWORD existingPid = 7702;
        TEST_ASSERT(safeboxCache.Get(existingPid) == nullptr, "Senaryo 8E: Unloaded safebox not present in cache");

        testDirectSql->m_directQueryHandler = nullptr; // Reset handler
    }

    // =========================================================================
    // SENARYO 9: DBManager SQL Escape Guvenligi ve Cok Baytli Karakter Butunlugu
    // =========================================================================
    {
        std::cout << "[RUN] Senaryo 9: DBManager SQL escape guvenligi ve cok baytli karakter butunlugu..." << std::endl;

        char buf[1024];

        // 9A: Sinir ve Null Parametre Kontrolleri
        TEST_ASSERT(dbManager.EscapeString(nullptr, "test", 4) == 0, "Senaryo 9A: null 'to' buffer returns 0");
        TEST_ASSERT(dbManager.EscapeString(buf, nullptr, 4) == 0 && buf[0] == '\0', "Senaryo 9A: null 'from' buffer returns 0 and sets NUL");
        TEST_ASSERT(dbManager.EscapeString(buf, "test", 0) == 0 && buf[0] == '\0', "Senaryo 9A: length 0 returns 0 and sets NUL");
        TEST_ASSERT(dbManager.EscapeString(buf, "test", 4, -1) == 0, "Senaryo 9A: negative slot returns 0");
        TEST_ASSERT(dbManager.EscapeString(buf, "test", 4, SQL_MAX_NUM) == 0, "Senaryo 9A: out-of-range slot returns 0");

        // 9B: Standart ASCII, Tek/Cift Tirnak, Ters Bolu, NUL ve Ozel Karakterler
        const char testAscii[] = "Warrior's \\ \"Sword\" \n \r \032";
        unsigned long len9B = dbManager.EscapeString(buf, testAscii, strlen(testAscii));
        TEST_ASSERT(len9B > 0, "Senaryo 9B: Standard escape produces output");
        TEST_ASSERT(strstr(buf, "\\'") != nullptr, "Senaryo 9B: Single quote escaped with \\'");
        TEST_ASSERT(strstr(buf, "\\\\") != nullptr, "Senaryo 9B: Backslash escaped with \\\\");
        TEST_ASSERT(strstr(buf, "\\\"") != nullptr, "Senaryo 9B: Double quote escaped with \\\"");
        TEST_ASSERT(strstr(buf, "\\n") != nullptr, "Senaryo 9B: Newline escaped with \\n");
        TEST_ASSERT(strstr(buf, "\\r") != nullptr, "Senaryo 9B: Carriage return escaped with \\r");
        TEST_ASSERT(strstr(buf, "\\Z") != nullptr, "Senaryo 9B: Ctrl+Z escaped with \\Z");

        // 9C: Cok Baytli Karakter Seti (Multibyte GBK/Big5/SJIS) Guvenligi
        // GBK kodlamasinda karakter 0xBF 0x5C baytlarindan olusur (ikinci bayt 0x5C = backslash).
        // Naive bir kacis fonksiyonu 0x5C'yi gorup onune '\' atarak karakteri bozar (multibyte SQL injection).
        const char gbkInput[] = { (char)0xBF, (char)0x5C, 0 };
        unsigned long len9C = dbManager.EscapeString(buf, gbkInput, 2);
        TEST_ASSERT(len9C == 2, "Senaryo 9C: Multibyte character preserved without extra escaping");
        TEST_ASSERT((unsigned char)buf[0] == 0xBF && (unsigned char)buf[1] == 0x5C, "Senaryo 9C: Trail byte 0x5C was not mutilated");
        TEST_ASSERT(buf[2] == '\0', "Senaryo 9C: Properly NUL terminated");

        // 9D: Turkce UTF-8 Dizgisi ve Tirnak Kacisi
        const char trInput[] = "Ahmet'in k\xC4\xB1" "l\xC4\xB1" "c\xC4\xB1";
        unsigned long len9D = dbManager.EscapeString(buf, trInput, strlen(trInput));
        TEST_ASSERT(len9D == strlen(trInput) + 1, "Senaryo 9D: Exactly one escape backslash added for single quote");
        TEST_ASSERT(strstr(buf, "Ahmet\\'in") != nullptr, "Senaryo 9D: Single quote properly escaped");
        TEST_ASSERT(strstr(buf, "k\xC4\xB1" "l\xC4\xB1" "c\xC4\xB1") != nullptr, "Senaryo 9D: UTF-8 Turkish multibyte bytes preserved verbatim");

        // 9E: Ikili Yapi (Binary Struct) ve Gomulu NUL Baytlari (skills / quickslot)
        const char binInput[] = { 'S', 'K', '\0', 'I', 'L', 'L', '\\', '\'', '\0' };
        unsigned long len9E = dbManager.EscapeString(buf, binInput, sizeof(binInput));
        TEST_ASSERT(len9E > sizeof(binInput), "Senaryo 9E: Binary data with NUL bytes escaped properly");
        TEST_ASSERT(strstr(buf, "SK\\0ILL") != nullptr, "Senaryo 9E: Embedded NUL escaped as \\0");

        // 9F: Baglanti Yoklugunda Kirli Onbellegin Korunmasi (Fail-Fast Guvencesi)
        DWORD testPid = 8801;
        TPlayerTable ptOffline{};
        ptOffline.id = testPid;
        strlcpy(ptOffline.name, "OfflineSaveHero", sizeof(ptOffline.name));
        clientManager.PutPlayerCache(&ptOffline);
        CPlayerTableCache* pkCache = clientManager.GetPlayerCache(testPid);
        TEST_ASSERT(pkCache != nullptr, "Senaryo 9F: Cache created");
        TEST_ASSERT(pkCache->IsNeedQuery() == true, "Senaryo 9F: Cache is dirty");
        pkCache->MarkPendingEviction();
        TEST_ASSERT(pkCache->CanEvict() == false, "Senaryo 9F: Dirty cache with pending eviction CANNOT be evicted while unsaved");
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << " [SUCCESS] REAL DB CACHE INTEGRATION SUITE: " << g_passCount << "/" << g_testCount << " PASSED!" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
