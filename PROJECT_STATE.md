# METİN2 MT2009 — PROJE DURUM TAKİBİ (PROJECT_STATE.md)

Bu belge, projenin anlık teknik durumunu, doğrulanmış bulgularını ve bir sonraki mikro görevini gösteren **tek doğruluk kaynağıdır (Single Source of Truth)**.

---

## 1. GİT VE VERSİYON DURUMU

* **Geçerli Dal (Branch):** `dev/windows-gcc-baseline`
* **Uzak Depo (Remote):** `https://github.com/x64bit-source/Metin2-MT2009.git` (Private)
* **Senkronizasyon:** Git geçmişi ve commit doğrulamaları doğrudan `git` komutları üzerinden izlenmektedir.
* **Yerel Dal Koruması:** `.githooks/` kaynak kancaları dallardan bağımsız kalıcı `.git/hooks` dizinine kurulmuş olup, `core.hooksPath = D:/mt22009/.git/hooks` mutlak yoluyla yapılandırılmıştır. `main` dalına geçilse dahi `pre-commit` ve `pre-push` koruması kesintisiz aktiftir. Kancalar `.githooks/install.ps1` ve `.githooks/install.sh` betikleriyle otomatik dağıtılabilir durumdadır.

---

## 2. MEVCUT KAYNAK KODUN DOĞRULANMIŞ DURUMU

* **Altyapı:** martysama v5.x / GameCore tabanlı C++20 MT2009 kaynak kodları.
* **Bileşenler:**
  * Sunucu: `game`, `db`, `libthecore`, `libsql`, `libgame`, `libpoly`, `liblua` (5.0 aktif).
  * İstemci: `Source Client` (Visual Studio 2022 / v143 uyumlu C++20 istemci kaynak kodları).
  * Veritabanı: `sql/` temiz şema tabloları.

---

## 3. DERLEME VE TEST DURUMU

* **Windows Geliştirme Ortamı:**
  * **Derleyici:** MinGW-w64 GCC 16.2.0 (`x86_64-w64-mingw32`, UCRT runtime, POSIX threads) — **KURULDU VE DOĞRULANDI**.
  * **İnşa Araçları:** CMake 4.4.2 ve Ninja 1.13.2 — **KURULDU VE DOĞRULANDI**.
  * **C++20 & 64-bit Doğrulaması:** C++20 `<concepts>`, `<ranges>` ve 64-bit işaretçi boyutu (8 bayt) test programıyla yerel olarak doğrulanmıştır.
* **Modül Doğrulama Durumu (Windows GCC x64 Baseline):**
  * **libpoly:**
    * **Kaynak Dosyalar:** `Base.cc`, `Symbol.cc`, `SymTable.cc`, `Poly.cc` (Başlık: `Poly.h`).
    * **Derleme Bayrakları:** `-std=c++20 -Wall -Wextra -O2 -pipe -fno-exceptions -fno-rtti -I../../Extern/include`.
    * **Sonuç:** 0 hata, 0 uyarı.
    * **Otomatik Regresyon Testi:** `tests/test_libpoly.cpp` -> **24/24 test başarılı**. (Dinamik değişken formülleri, min/max/sqrt fonksiyonları, 0 ile çarpım, negatif sayılar, sözdizimi hata yakalama ve bellek temizliği doğrulandı).
  * **libgame:**
    * **Kaynak Dosyalar:** `src/grid.cc`, `src/attribute.cc`, `src/targa.cc` (Başlıklar: `grid.h`, `attribute.h`, `targa.h`).
    * **Bağımlılıklar:** `libthecore/include/memcpy.h`, `libthecore/include/stdafx.h`, `common/stl.h`, Boost başlıkları.
    * **Kök Neden Düzeltmeleri:**
      * `libthecore/include/typedef.h` dosyasında `struct timezone` yapısı `#ifndef _TIMEZONE_DEFINED` ile sarılarak MinGW CRT `<time.h>` çakışması kod seviyesinde kalıcı olarak çözüldü. Geçici `-D_TIMEZONE_DEFINED` bayrağı tamamen kaldırıldı.
      * `libthecore/include/stdafx.h` dosyasında `S_ISDIR` ve `PATH_MAX` makroları `#ifndef` ile sarılarak derleyici yeniden tanımlama uyarıları tamamen sıfırlandı.
    * **Derleme Bayrakları:** `-std=c++20 -Wall -Wextra -O2 -pipe -fno-exceptions -fno-rtti` (Hiçbir geçici bayrak veya uyarı bastırma kullanılmamıştır).
    * **Sonuç:** 0 hata, 0 uyarı.
    * **Otomatik Regresyon Testi:** `tests/test_libgame.cpp` -> **28/28 test başarılı**. (`CGrid` hücre yerleşimi, taşma sınırları, negatif indeksler, temizleme; `CAttribute` bitmask atama/kaldırma, sınır ötesi koordinat güvenliği, CopyRow; `CTargaImage` satır işaretçileri ve başlık doğrulaması).
  * **Otomatik Test Altyapısı & Güvenilirlik Doğrulaması:**
    * `tests/run_tests.ps1` (Windows UCRT64 GCC) ve `tests/run_tests.sh` (FreeBSD/POSIX) betikleri Git deposuna entegre edildi.
    * **Kontrollü Negatif Testler:** Test altyapısının sahte başarı üretmediğini kanıtlamak için kasıtlı test hatası ve sözdizimi derleme hatası enjekte edilerek test edilmiş; betiğin süreci anında durdurup çıkış kodu 1 (`exit 1`) döndürdüğü doğrulanmıştır.
    * **Temiz Ortam Doğrulaması:** Temiz derleme ortamında (`tests/build/` silinerek) baştan derlenen regresyon testleri 0 hata, 0 uyarı ile 52/52 başarılı sonuçlanmıştır.
    * **Geçici Bayrak Kontrolü:** Derleme sürecinde `-D_TIMEZONE_DEFINED` geçici bayrağının bulunmadığı, düzeltmenin [typedef.h](file:///d:/mt22009/mt2009%20-%20FULL%20SOURCE/Server%20Source/Server/libthecore/include/typedef.h) ve [stdafx.h](file:///d:/mt22009/mt2009%20-%20FULL%20SOURCE/Server%20Source/Server/libthecore/include/stdafx.h) içinde kök nedenden sağlandığı doğrulanmıştır.
    * **thecore_memcpy Analizi ve Gerçek Bağlantı Durumu:**
      * `Server Source/Server/libgame` altındaki hiçbir üretim kodu (`grid.cc`, `attribute.cc`, `targa.cc` vb.) değiştirilmemiştir; orijinal kod tabanı korunmaktadır.
      * Windows tarafında `memcpy.h` başlığı `#define thecore_memcpy memcpy` makrosunu kullandığı için MinGW CRT fonksiyonuna bağlanmaktadır.
      * FreeBSD tarafında `memcpy.h` başlığı `extern void *(*thecore_memcpy)(...)` sembolünü bildirmekte; gerçek gövde `libthecore/src/memcpy.c` içerisinde yer almaktadır.
      * [tests/test_libgame.cpp](file:///d:/mt22009/tests/test_libgame.cpp) içerisindeki `thecore_memcpy = memcpy;` tanımı **yalnızca izole birim testinin** henüz derlenmemiş `libthecore.a` bağımlılığı olmadan FreeBSD üzerinde bağlanabilmesi (link) için eklenmiş bir test yönlendiricisidir (mock/stub). Üretim kodunda kesinlikle yer almamaktadır.
      * **Doğrulama Notu:** Gerçek `thecore_memcpy` implementasyonunun `libthecore.a` üzerinden fiziksel bağlanması henüz test edilmemiştir ve bu entegrasyon bir sonraki **`libthecore`** aşamasında doğrulanacaktır.
  * **GitHub Actions CI ve Tetikleme Mimarisi:**
    * `.github/workflows/freebsd-gcc-test.yml` iş akışında `paths` filtresi yalnızca `'.github/workflows/freebsd-gcc-test.yml'` dosyasına sınırlandırılmıştır.
    * Bu sayede depoya yapılan normal commit ve push işlemleri GitHub Actions'ı tetiklemez, gereksiz sanal makine başlatmaz ve kota/süre tüketimini engeller.
    * **Gelecekteki Kontrollü Test Koşu Yöntemi:**
      1. **Windows Testleri:** Geliştirme sırasında yerel olarak `powershell -ExecutionPolicy Bypass -File tests/run_tests.ps1` ile anında ve sınırsız çalıştırılır.
      2. **FreeBSD CI Testleri:** Önemli kaynak kod değişiklikleri tamamlanıp Windows'ta doğrulandıktan sonra, ilgili commit ile birlikte `.github/workflows/freebsd-gcc-test.yml` dosyasındaki revizyon başlığı (`# Rev X`) güncellenerek bilinçli ve kontrollü olarak tetiklenir.
  * **libthecore:**
    * **Kaynak Dosyalar:** `DES_table.c`, `buffer.c`, `des.c`, `fdwatch.c`, `gost.c`, `hangul.c`, `heart.c`, `kstbl.c`, `log.c`, `main.c`, `memcpy.c`, `signal.c`, `socket.c`, `tea.c`, `utils.c`, `xdirent.c`, `xgetopt.c`, `xmd5.c`.
    * **Kök Neden Düzeltmeleri:**
      * `log.h`: `sys_err(fmt, ...)` makrosu `##__VA_ARGS__` ile güncellenerek argümansız çağrılardaki sözdizimi hatası (`_sys_err(..., )`) kalıcı olarak giderildi.
      * `socket.h` / `socket.c`: `socket_tcp_bind`, `socket_udp_bind`, `socket_bind` fonksiyonları 64-bit güvenli `socket_t` türüne geçirildi. Hata kontrolleri `INVALID_SOCKET` ile eşitlendi; 64-bit Windows işaretsiz soket karşılaştırmalarındaki `-Wtype-limits` ve `-Wsign-compare` uyarıları giderildi. Bind aşamasında IP parametresi boşsa `INADDR_ANY` yedeklemesi sağlanarak `-Wunused-parameter` sıfırlandı.
      * `utils.h` / `utils.c`: `thecore_msleep(DWORD)` fonksiyon bildirimi `utils.h` içerisine eklendi. `(void)dummy`, `(void)who`, `(void)line` ile kullanılmayan parametre uyarıları giderildi.
      * `signal.c`: `signal_timer_enable` için `(void)timeout_seconds`, FreeBSD sinyal işleyicileri (`reap`, `checkpointing`, `hupsig`, `usrsig`) için `(void)sig` eklenerek derleyici uyarıları tamamen giderildi.
      * `log.c`: `_sys_err` içinde `(void)line` eklendi.
      * `fdwatch.c`: `(void)oneshot`, `(void)fdw`, `(void)fd` eklendi.
      * `memcpy.c`: `thecore_memcpy` fonksiyon işaretçisi `#ifdef thecore_memcpy #undef thecore_memcpy #endif` korumasıyla tüm platformlarda statik kütüphaneden (`libthecore.a`) dışa aktarılacak şekilde yapılandırıldı.
      * `test_libgame.cpp`: `thecore_memcpy` geçici test yönlendiricisi kaldırıldı; `test_libgame` doğrudan gerçek `libthecore.a` kütüphanesine bağlandı.
    * **Derleme Bayrakları:** `-std=c++20 -Wall -Wextra -O2 -pipe -fno-exceptions -fno-rtti`.
    * **Sonuç:** 0 hata, 0 uyarı.
    * **Otomatik Regresyon Testi:** `tests/test_libthecore.cpp` -> **97/97 test başarılı**. (`thecore_memcpy` bellek bütünlüğü/sıfır bayt/işaretçi dönüşü; `buffer` dinamik büyüme/peek/read/reset/delete; TEA, DES, GOST simetrik şifreleme ve çözme; `number`/`fnumber` sınır ve rastgele aralıkları; `timediff`/`timeadd`/`get_dword_time`/`get_float_time` zamanlayıcı ilerlemesi; `strlcpy`/`strlcat`/`trim_and_lower`/`lower_string` metin işleme; `fdwatch` soket kaydı/seçimi/temizliği; `socket_tcp_bind` yerel soket bağlama; `heart` nabız zamanlayıcısı).
  * **libsql:**
    * **Kaynak Dosyalar:** `AsyncSQL.cpp`, `Semaphore.cpp`, `Statement.cpp`, `Tellwait.cpp` (Başlıklar: `AsyncSQL.h`, `Semaphore.h`, `Statement.h`, `Tellwait.h`, `libsql.h`).
    * **Kök Neden Düzeltmeleri:**
      * `AsyncSQL.h`: `SQLResult` yapısına yerel 64-bit alanlar eklendi (`ullNumRows`, `ullAffectedRows`, `ullInsertID`). Mevcut 32-bit alanlar (`uiNumRows`, `uiAffectedRows`, `uiInsertID`) geriye uyumluluk ve `db`/`game` içerisindeki `(uint32_t)-1` hata kontrollerini (`ClientManagerPlayer.cpp:1191` vb.) bozmamak adına korundu ve tam hizalandı. `bool Reconnect()` eklendi, `IsConnected() const` yapıldı.
      * `AsyncSQL.cpp`: Yapıcıda (`CAsyncSQL`) `m_mtxQuery` ve `m_mtxResult` kritik kesit nesneleri `std::make_unique` ile baştan tahsis edildi; böylece `bNoThread=true` veya `Setup` çağrılmadan önce kuyruk işlemlerinde oluşan `CRITICAL_SECTION NULL` çökmesi giderildi. `Destroy()` içinde bellek sızıntısı üreten `.release()` çağrıları `.reset()` ile değiştirildi.
      * `AsyncSQL.cpp`: Windows `Quit()` içinde `::CloseHandle(m_hThread)` eklenerek thread kernel nesnesi sızıntısı kalıcı olarak giderildi. `INVALID_HANDLE_VALUE` ve null kontrolleri ile ardışık `Quit()` ve `Destroy()` çağrılarının tam idempotent olması sağlandı.
      * `AsyncSQL.cpp`: `AsyncSQLThread` içinde ilk veritabanı bağlantısı (`Connect()`) başarısız olduğunda thread'in sessizce sonlanarak sorguları RAM'de sonsuza dek terk etmesi engellendi; 1 saniyeden 5 saniyeye kadar kademeli backoff ve `IsEnd()` bayrağını dinleyen 100ms aralıklı duyarlı durdurma döngüsü kuruldu.
      * `AsyncSQL.h` / `AsyncSQL.cpp`: `AsyncQuery` fire-and-forget sorgularında bağlantı kopması durumunda düşen sorguların sessizce silinmesi (`delete p;`) önlendi; `m_queue_failed` kuyruğu, `PushFailedQuery`, `PopFailedQuery` ve `CountFailedQuery` API'si eklenerek başarısız sorguların saklanması sağlandı. En eski sorgunun körlemesine silinmesi (sessiz veri kaybı) tamamen kaldırıldı; eleman sayısı ve bellek kullanımı (64 MB failed / 128 MB query) çift sınırı getirildi. Sınır aşıldığında `m_bFailedQueueOverflow` bayrağı ile açık geri basınç aktif hale getirildi; `SetFailedQueryCallback` reaktif geri çağırma mimarisi eklendi.
      * `AsyncSQL.cpp`: `m_queue_query` kuyruğuna 100.000 eleman ve 128 MB bellek tavan sınırı eklendi; veritabanı uzun süre kapalı kaldığında sorguların kabul/reddi kontrollü hale getirilerek `CR_CONNECTION_ERROR` ile dönmesi sağlandı.
      * `AsyncSQL.h` / `AsyncSQL.cpp`: `SanitizeQueryForLog` filtresi ile SQL hata/yavaş/tahliye loglarında parola (`PASSWORD('...')`, `password='...'`), token ve kişisel verilerin sızması kesin olarak engellendi.
      * `AsyncSQL.h`: `SQLResult` içinde kimlik ve etkilenen satır alanlarında sessiz satürasyon yasaklandı; `ullInsertID > UINT32_MAX` durumunda `uiInsertID = 0` (hata) ve `uiSQLErrno = 1264` (`ER_WARN_DATA_OUT_OF_RANGE`) atanarak sahte kimlik ve veri çakışması kesin olarak engellendi. `ullAffectedRows > UINT32_MAX` durumunda `uiAffectedRows = (uint32_t)-1` hata belirteci atandı. `HasValidInsertID()`, `HasValidAffectedRows()`, `HasValidNumRows()`, `IsOverflow()`, `IsError()`, `IsEmpty()` yardımcı fonksiyonları eklenerek hata durumunun boş sonuçla çakışması önlendi.
      * `AsyncSQL.cpp`: Tüm kuyruk ve sayaç fonksiyonlarına (`PushResult`, `PopResult`, `PushFailedQuery`, `PopFailedQuery`, `CountFailedQuery`, `PushQuery`, `PeekQuery`, `PopQuery`, `CountQuery`, `CountResult`) mutex null kontrolleri ve thread-safe senkronizasyon eklendi; `Destroy()` sonrasında oluşabilecek `0xC0000005` (Access Violation) çökmesi tamamen ortadan kaldırıldı.
      * `Tellwait.h` / `Tellwait.cpp` & `libthecore/include/signal.h`: `libthecore/include/signal.h` başlığına `#include_next <signal.h>` eklenerek yerel başlığın sistem `<signal.h>` başlığını gölgelemesi (shadowing) kök nedeninden çözüldü. `Tellwait.cpp` dosyasına `<sys/types.h>`, `<unistd.h>`, `<signal.h>` eklenerek FreeBSD 15.1 POSIX sinyal fonksiyonları (`sigemptyset`, `sigaddset`, `sigprocmask`, `sigsuspend`, `kill`) ve `-Wunused-parameter` uyarısı tam olarak giderildi.
    * **Makefile:** `clang++` ve `-m32` yerine `CXX ?= g++`, `-std=c++20` ve standart nesne kuralı (`%.o: %.cpp`) tanımlandı.
    * **Derleme Bayrakları:** `-std=c++20 -Wall -Wextra -O2 -pipe -fno-exceptions -fno-rtti`.
    * **Sonuç:** 0 hata, 0 uyarı.
    * **Otomatik Regresyon Testi:** `tests/test_libsql.cpp` -> **141/141 test başarılı**. (`CSemaphore` temel işlemler/iş parçacıkları arası devir/çoklu sayaç/yeniden ilklendirme; `CAsyncSQL` kuyruk işlemleri FIFO/sonuç kuyruğu/sayaçlar; `EscapeString` sınır ve taşma güvenliği; `SQLResult` 64-bit ID/satır, satürasyon ve `(uint32_t)-1` hata eşleşmesi; `SQLMsg` çoklu sonuç gezimi; `CAsyncSQL` bağlantı yaşam döngüsü, yeniden bağlanma, `Quit`/`Destroy` handle güvenliği; bağlantı yeniden denemesi, kademeli backoff ve kontrollü durdurma; başarısız fire-and-forget sorgularının saklanması, bellek sınırı ve reaktif callback teslimatı; SQL log parola/token temizliği; 64-bit hata vs boş sonuç ayrımı; çoklu nesne iş parçacığı izolasyonu; MariaDB 12.3 snapshot isolation sözleşmesi; MySQL 8.4 açık reconnect sözleşmesi; DB tarafı `cache<T>` hata anında kirli veri koruması (`SetNeedQuery`); çoklu SQL slotu `PopFailedQuery` kuyruk tahliyesi; bağlantı kopmasında UNKNOWN işlem durumu koruması; sunucu kapanışında bekleyen sorguların tam tahliyesi; Canlı veritabanı entegrasyonu dürüstçe `[SKIP]` olarak raporlanmaktadır).
  * **Toplu Regresyon Testi:**
    * `libpoly`: **24/24 test başarılı**
    * `libthecore`: **97/97 test başarılı**
    * `libgame`: **28/28 test başarılı** (Gerçek `libthecore.a` ile bağlı)
    * `libsql`: **141/141 test başarılı**
    * **Toplam:** **290/290 test başarılı (%100)**.
  * **Gerçek Canlı Veritabanı Entegrasyon Testleri (Aşama 11.2 - tests/test_live_db.cpp):**
    * **Hedef Motorlar ve Kararlı Sürümler:**
      * **Oracle MySQL 8.4 LTS:** Sürüm 8.4.11 GA (Birincil MySQL hedefi).
      * **MariaDB 12.3 LTS:** Sürüm 12.3.3 GA (Birincil MariaDB hedefi).
      * **MariaDB 11.4 LTS:** Sürüm 11.4.13 GA (Geriye dönük uyumluluk hedefi).
    * **64-Bit Connector & MinGW Entegrasyonu:**
      * MariaDB Connector/C 3.4.10 64-bit DLL'den `gendef` + `dlltool` ile üretilen yerel `libmariadb.a` import kütüphanesi bağlandı.
      * Oracle MySQL C API 8.4.11 64-bit DLL'den `gendef` + `dlltool` ile üretilen yerel `libmysql.a` import kütüphanesi bağlandı.
      * Her iki bağlayıcı da MinGW-w64 UCRT GCC 16.2.0 (C++20) ile 0 hata, 0 uyarı ile derlendi ve bağlandı.
      * Eski 32-bit `Extern/lib/libmysqlclient.a` (`ELF32-i386`) kesinlikle kullanılmamıştır ve devre dışı tutulmuştur.
    * **Windows amd64 İzole Canlı Test Sonuçları (Port 3307 & 3308):**
      * **MariaDB 11.4.13 LTS (Port 3307):** **73/73 test başarılı (%100)**.
      * **MariaDB 12.3.3 LTS (Port 3307):** **73/73 test başarılı (%100)**.
      * **Oracle MySQL 8.4.11 LTS (Port 3308):** **73/73 test başarılı (%100)**.
      * **Toplam Canlı Doğrulama:** **219/219 kontrol başarılı (%100)**.
      * **Doğrulanan 12 Canlı Test Paketi ve İşlevleri:**
        1. Gerçek TCP/IP soket el sıkışması ve kimlik doğrulama.
        2. MySQL 8.4 `caching_sha2_password` varsayılan kimlik doğrulama protokolü.
        3. Katı SQL Modu (`STRICT_TRANS_TABLES`, `NO_ZERO_DATE`) ve geçersiz tarihlerin (`0000-00-00 00:00:00`) reddedilmesi.
        4. CRUD işlemleri, otomatik artan 64-bit ID (`mysql_insert_id`) ve etkilenen satır sayıları (`mysql_affected_rows`).
        5. SQL enjeksiyon koruması (`CAsyncSQL::EscapeString`).
        6. ACID Transaction garantisi: `START TRANSACTION`, `ROLLBACK` ile geri alma ve `COMMIT` ile kalıcı yazma.
        7. Kısıt ihlalleri: `ER_DUP_ENTRY` (1062) ve sözdizimi hatası `ER_PARSE_ERROR` (1064).
        8. Türkçe karakter ve ANSI codepage bütünlüğü (`latin1`, `cp1250`).
        9. Çok iş parçacıklı asenkron kuyruk: Arka plan `AsyncSQLThread` iş parçacığı üzerinden `ReturnQuery` ve `PopResult` FIFO sonuç teslimatı.
        10. 64-bit taşma göstergeleri ve boş sonuç (`IsEmpty()`) ile hata (`IsError()`) ayrımı.
        11. TLS / SSL Bağlantı Güvenliği & CA Sözleşmesi: Aktif TLSv1.3 şifreleme müzakeresi (`Ssl_cipher != NONE`), güvenilen CA ile başarılı el sıkışma, yetkisiz/sahte CA ile bağlantının istemci seviyesinde kesin reddi (`errno 2026`).
        12. Önceden Kesilmiş Soket Koruması ve Körlemesine Retry Yasağı: Kapalı/kopuk sokette `DirectQuery` çağrısının çökmeden güvenli `CR_SERVER_GONE_ERROR` (2006) döndürmesi, `IsError() == true` garantisi, veri bütünlüğü ve körlemesine tekrar sorgu gönderilmemesi.
        13. Gerçek TCP Ağ Arıza Enjeksiyonu ve Ağ Kesintisi Doğrulaması (`tests/tcp_fault_proxy.hpp`):
            * **Senaryo 11.A (Sorgu Yürütülürken Hat Kesintisi / In-Flight Mid-Query Severance):** `SELECT SLEEP(2)` sunucuda işlenirken TCP proxy üzerinden soket anında (`SO_LINGER(1, 0)` RST) koparılmış; istemcide `IsError() == true`, `CR_SERVER_LOST` (2013) / `CR_SERVER_GONE_ERROR` (2006) hata kodu korunmuş, çökme oluşmamış ve sunucunun sonraki sorguları kabul etmeye devam ettiği kanıtlanmıştır.
            * **Senaryo 11.B (Sunucuda Commit Sonrası Yanıt Paketinin Düşürülmesi / Post-Execution Dropped Response -> UNKNOWN):** Non-idempotent `UPDATE test_player_live SET gold = gold + 500` sunucuda commit edildikten sonra sunucunun OK yanıt paketi TCP proxy tarafından istemciye ulaşmadan yakalanıp düşürülmüş ve soket koparılmıştır. İstemcide hata üretilerek işlemin UNKNOWN durumunda kaldığı, veritabanında güncellemenin fiilen uygulandığı (`initial + 500`), ve `libsql` tarafından asla körlemesine `ResendQuery` / duplicate retry yapılmadığı (altının `initial + 1000` olmadığı) kesin olarak kanıtlanmıştır.
        14. Soket yeniden bağlanması (`Reconnect()`) ve temiz şema tahliyesi (`Destroy()`).
    * **Test Çalıştırıcı Betikleri:** `tests/run_live_tests.ps1` (Windows) ve `tests/run_live_tests.sh` (FreeBSD/POSIX) eklendi. Test parolaları loglarda maskelenmekte (`[PROTECTED / NOT LOGGED]`), yalnızca `127.0.0.1` üzerinden çalıştırılmaktadır.
    * **FreeBSD amd64 Canlı DB ve CI Durumu (GitHub Actions Rev 17 / Commit fd0c8482):**
      * FreeBSD 15.1-RELEASE amd64 resmî pkg deposunda 3 motorun da ikili (binary) paketlerinin mevcut olduğu kanıtlandı: `mariadb114-server-11.4.13`, `mariadb123-server-12.3.3`, `mysql84-server-8.4.11_4`.
      * Servislerin FreeBSD `mysql` kullanıcısı haklarıyla başlatılması (`chown -R mysql:mysql`, 750/644/640) ve başlık/kütüphane arama sırasının izole edilmesiyle (`-I/usr/local/include` önceliği) daemon ve istemci uyumsuzlukları kalıcı olarak çözüldü.
      * Rev 16 (Run 38140566854): MariaDB 11.4, MariaDB 12.3 ve MySQL 8.4 için 168/168 canlı kontrol + 290/290 regresyon testi %100 başarıyla tamamlandı.
      * Rev 17 (Commit fd0c8482): TCP fault injection (17 ek kontrol ile toplam 73 kontrol/motor, genel toplam 219 kontrol) entegrasyonu sağlandı.
  * **Platform Doğrulama Özeti:**
    * **Windows amd64:** MinGW-w64 GCC 16.2.0 ile 290/290 birim testi + 219/219 canlı veritabanı kontrolü (Suite 1-12) tam doğrulandı (%100).
    * **FreeBSD amd64:** GitHub Actions CI iş akışı (Rev 16: Run 38140566854; Rev 17: Commit fd0c8482) ile FreeBSD 15.1-RELEASE amd64 + GCC 14.2.0 ortamında 3 izole matris koşusunda 290/290 birim testi ve canlı veritabanı kontrolleri sürdürülmektedir.
* **Mevcut Makefile Durumu:**
  * Mevcut Makefile dosyaları FreeBSD 32-bit (`clang++`, `-m32`) yapılandırmasındadır; CMake dönüşümü henüz başlatılmamıştır.

---

## 4. BİLİNEN KRİTİK RİSKLER

1. **LP64 (FreeBSD) vs LLP64 (Windows) Uyumsuzluğu:**
   * `packet.h` ve `tables.h` içerisinde `long` kullanımı mevcuttur.
   * FreeBSD amd64 üzerinde `sizeof(long) = 8 bayt` olurken, Windows 32-bit istemcide 4 bayttır. Paket kaymasını önlemek için tüm `long` paket alanlarının `int32_t` ile sabitlenmesi gereklidir.
2. **Harici Kütüphane Mimarisi:**
   * `Server Source/Extern/lib` altındaki 5 adet kütüphane (`libmysqlclient.a`, `libcryptopp.a`, `libIL.a`, `libILU.a`, `libILUT.a`) FreeBSD 32-bit ELF'tir. Windows amd64 GCC için uyumlu kütüphanelerin temin edilmesi gerekmektedir.
3. **Windows Sistem/Sinyal Farklılıkları:**
   * `signal.c` içerisinde Windows için sinyal işleme stub durumundadır; Windows'ta sunucu konsol/geliştirme modunda interaktif çalışmaktadır.
4. **Çıkış Yapan Karakterin Bellek Önbelleğinden Silinmesi ve Veri Kaybı Riski (Logout Cache Flush / Deletion Race) — [ÇÖZÜLDÜ]:**
   * `cache<T>`, `CPlayerTableCache`, `CItemCache`, `ClientManager` ve `ClientManagerPlayer` üzerinde gecikmeli tahliye (deferred cache eviction / generation versioning) mimarisi kuruldu.
   * `FlushPlayerCacheSet`, `FlushItemCacheSet` ve `QUERY_FLUSH_CACHE` çağrılarında nesneler hemen silinmeyip `MarkPendingEviction()` ile işaretlenmekte; yalnızca veritabanından başarılı onay (`OnSaveSuccess`) geldiğinde tahliye edilmektedir.
   * Başarısızlık veya bağlantı kopmasında (`OnSaveFailure`, `AnalyzeFailedQuery`) kirli durum (`m_bNeedQuery = true`) korunmakta; nesneler RAM'de saklanmakta; oyuncu tekrar bağlandığında (`QUERY_PLAYER_LOAD`) önbellek tahliyesi iptal edilerek en güncel RAM verisiyle devam edilmektedir (Senaryo A-H, 120 test ile %100 doğrulandı).

---

## 5. GELECEK AŞAMA KABUL KRİTERLERİ: KALICI MYSQL VE MARIADB ÇİFT MOTOR UYUMLULUĞU

Bu kriterler, `libsql`, `db`, `game` ve `sql/` şemaları üzerinde geliştirme yapılacağı zaman karşılanması **zorunlu başarı ve kabul şartlarını** belirler:

1. **Çift Motor Zorunluluğu:**
   * Proje, mevcut en güncel kararlı **MySQL** ve **MariaDB** sürümlerinin **HER İKİSİNİ DE** gerçek anlamda ve eksiksiz desteklemelidir. Yalnızca birini desteklemek yeterli değildir.
2. **Kapsam:**
   * `libsql` bağlantı katmanı ve tüm ilgili istemci kütüphaneleri.
   * `db` sunucusundaki veritabanı işlemleri ve sorgular.
   * `game` sunucusundaki veritabanı kullanan bütün sistemler.
   * Mevcut tüm SQL dosyaları, tablo şemaları, indeksler ve başlangıç verileri.
   * Gerekli diğer sunucu kaynak kodları, yapılandırmalar ve veri dönüşüm mekanizmaları.
3. **Kesin Kabul Kriterleri ve Kurallar:**
   * **SQL Mode Dokunulmazlığı:** SQL mode (`STRICT_TRANS_TABLES`, `NO_ZERO_DATE`, `ONLY_FULL_GROUP_BY` vb.) devre dışı bırakılamaz veya uyumluluk sağlamak için gevşetilemez.
   * **Hata Gizleme Yasağı:** `INSERT IGNORE`, `UPDATE IGNORE` ve benzeri hatayı gizleyen yaklaşımlar çözüm kabul edilmez. SQL hataları sessizce yok sayılamaz.
   * **Kök Neden Çözümü:** Uyumsuzlukların kök nedenleri kalıcı şekilde düzeltilmelidir. Uyarı veya hataları gizlemek için geçici hileler uygulanmamalıdır.
   * **Açık Uyumluluk Katmanı:** MySQL ve MariaDB arasındaki gerçek sözdizimi veya davranış farkları gerektiğinde açık ve test edilebilir uyumluluk katmanlarıyla çözülmelidir.
   * **Veri Tipi ve Şema Standartları:** Veri tipleri, `PRIMARY KEY`, indeksler, `AUTO_INCREMENT`, `NULL`, `DEFAULT`, tarih/saat alanları, karakter setleri (`utf8mb4`), collation ve transaction davranışları titizlikle incelenmeli ve tam uyumlu hale getirilmelidir.
   * **Veri Güvenliği ve Geri Dönüş Planı:** Mevcut tablolar ve kullanıcı verileri korunmalıdır. Veri kaybı doğurabilecek dönüşümler öncesinde doğrulanmış yedek ve geri dönüş planı olmalıdır.
   * **Çift Senaryo Doğrulaması:** Hem sıfırdan kurulum hem de mevcut veritabanını yükseltme (migration) test edilmelidir.
   * **Bütünsel Değerlendirme:** SQL dosyaları ile `libsql`, `db` ve `game` kaynak kodları birlikte değerlendirilmelidir.
   * **Platform Uyumluluğu:** Windows amd64 ve FreeBSD amd64 GCC uyumluluğu korunmalıdır.
   * **Gerçek Test Doğrulaması:** İki veritabanında da (güncel MySQL ve güncel MariaDB) gerçek testler geçmeden ilgili aşama tamamlanmış sayılmaz. Testlerde veri bütünlüğü, karakter/eşya kayıtları, normal ve hatalı sorgular, bağlantı kopmaları ve transaction sorunları doğrulanmalıdır.
   * **Resmî Hedef Sürümler (2026 Doğrulaması):**
     * **Oracle MySQL:**
       * **MySQL 8.4 LTS:** Nisan 2024 GA (Premier Destek: Nisan 2029, Uzatılmış Destek: Nisan 2032).
       * **MySQL 9.7 LTS:** Nisan 2026 GA (2026 yılı güncel LTS sürümü).
       * **MySQL 26.7 Innovation:** Temmuz 2026 GA (CalVer YY.M takvimli güncel kararlı üretim sürümü).
       * (Not: Eski MySQL 8.0 desteği Nisan 2026'da sona ermiştir).
     * **MariaDB Foundation:**
       * **MariaDB 11.4 LTS:** Mayıs 2024 GA (Destek: Mayıs 2029).
       * **MariaDB 12.3 LTS:** Mayıs 2026 GA (Destek: Haziran 2029, güncel LTS kararlı sürüm 12.3.3).
       * **MariaDB 13.0 Stable:** Eylül 2026 GA (Rolling kararlı sürüm 13.0.2).
       * **MariaDB 10.11 LTS:** Geriye dönük desteklenen LTS sürümü (Destek: Şubat 2028).
   * **Körlemesine Dönüşüm Yasağı ve Veri Koruma Standartları:**
     * **Karakter Seti (Charset/Collation):** Tablolar körlemesine `utf8mb4`'e çevrilemez. Metin2 32-bit Windows istemcisi sabit uzunluklu ANSI codepage (`char name[24]` vb.) gönderdiğinden, sunucu `CONFIG` locale (`g_stLocale`: `latin1`, `cp1250`) yapısı korunmalıdır.
     * **Sıfır Tarihler (Zero-Date):** `account.shop_expire`, `player.change_empire.last_time`, `log.log_commands.date` ve `BanManager.cpp:34` sıfır tarihleri körlemesine `NULL` yapılmaz; `UNIX_TIMESTAMP`, `str_to_number` ve tablo kullanım anlamları korunarak katı moda (`NO_ZERO_DATE`) uyumlu hale getirilir.
     * **Depolama Motoru (Storage Engine):** `log.sql` içindeki MyISAM tabloları mevcut verilerde zorla dönüştürülmez; yeni kurulumlar ve mevcut veriler için veri kaybı üretmeyen, geri alınabilir (rollback destekli) migration stratejisi uygulanır.

---

## 6. TAMAMLANAN GELİŞTİRME AŞAMALARI

* [x] **Aşama 1 (Analiz):** Kaynak kod, modül, mimari ve platform bağımlılıklarının salt okunur tam analizi.
* [x] **Aşama 2 (Güvenlik & Sürüm Kontrolü):**
  * Özel `.gitignore` yapılandırması (100 MB üzeri dosyalar ve hassas veriler filtrelendi).
  * İlk saf kaynak kod commit'inin oluşturulması.
  * Private GitHub deposuna ilk başarılı push işleminin yapılması.
* [x] **Aşama 3 (Hafıza Sistemi):** `AGENTS.md` ve `PROJECT_STATE.md` yönetim belgelerinin oluşturulup depoya eklenmesi.
* [x] **Aşama 4 (Windows GCC Ortamı):** MinGW-w64 UCRT64 GCC, CMake ve Ninja araç zincirinin kurulması ve C++20 64-bit derleme testinin başarıyla tamamlanması.
* [x] **Aşama 5 (İlk Modül Doğrulaması - libpoly):** `libpoly` modülünün kaynak kodlara dokunulmadan Windows amd64 GCC 16.2.0 (C++20) ile derlenmesi, statik kütüphanesinin (`libpoly.a`) üretilmesi ve matematiksel formül çalışma testinin doğrulanması.
* [x] **Aşama 6 (İkinci Modül Doğrulaması - libgame):** `libgame` modülünün kaynak kodlara dokunulmadan Windows amd64 GCC 16.2.0 (C++20) ile derlenmesi, `libgame.a` kütüphanesinin üretilmesi ve `CGrid`, `CAttribute`, `CTargaImage` fonksiyonel testlerinin doğrulanması.
* [x] **Aşama 7 (Kalıcı Düzeltmeler ve Regresyon Test Paketi):**
  * `AGENTS.md` dosyasına katı "sorunlar çözülmeden sonraki modüle geçilmez" anayasa kuralı eklendi.
  * `struct timezone` ve `S_ISDIR` / `PATH_MAX` çakışmaları kök nedeninden düzeltildi, tüm geçici bayraklar kaldırıldı.
  * `tests/` altında `test_libpoly.cpp` ve `test_libgame.cpp` ile 52/52 başarılı otomatik test paketi ve `run_tests.ps1`/`run_tests.sh` çalıştırıcıları kuruldu.
* [x] **Aşama 8 (FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması):**
  * GitHub Actions ve `vmactions/freebsd-vm@c46abacb49f09938ca4e1702d15d836285d694cc` ile güvenli, izole FreeBSD 15.1-RELEASE amd64 ortamı hazırlandı.
  * Derleyici `gcc14` (`/usr/local/bin/g++14`) olarak sabitlendi; Clang kullanımı engellendi.
  * `tests/run_tests.sh` POSIX boşluk kaçışları ve `test_libgame.cpp` üzerindeki FreeBSD `thecore_memcpy` linker sembolü çözümlendi.
  * `libpoly` (24/24) ve `libgame` (28/28) toplam 52/52 test FreeBSD 15.1 amd64 üzerinde 0 hata, 0 uyarı ile doğrulandı.
* [x] **Aşama 9 (libthecore Modülü ve Entegrasyon):**
  * `libthecore` 18 kaynak dosyası incelendi; `log.h`, `socket.h`, `socket.c`, `utils.h`, `utils.c`, `signal.c`, `log.c`, `fdwatch.c`, `memcpy.c` kök neden düzeltmeleri tamamlandı.
  * `test_libthecore.cpp` (97 test) oluşturuldu; Windows amd64 GCC 16.2.0 üzerinde `libpoly` (24) + `libthecore` (97) + `libgame` (28) toplam 149/149 test 0 hata ve 0 uyarı ile doğrulandı.
  * `test_libgame` içerisindeki geçici `thecore_memcpy` yönlendiricisi kaldırıldı; gerçek `libthecore.a` bağlantısı doğrulandı.
  * FreeBSD 15.1 amd64 GCC 14 ortamında CI koşusu (Run 38099052441) ile 149/149 test, 0 derleme hatası ve 0 derleyici uyarısı ile tam doğrulandı.
* [x] **Aşama 10 (libsql Modülü Teknik Envanter ve Çift Motor Analizi):**
  * `libsql` kaynak dosyaları (`AsyncSQL.cpp/h`, `Semaphore.cpp/h`, `Statement.cpp/h`, `Tellwait.cpp/h`, `libsql.h`, `stdafx.h`, `Makefile`) eksiksiz incelendi.
  * `Extern/lib/libmysqlclient.a` dosyasının FreeBSD 32-bit `elf32-i386` olduğu tespit edildi; 64-bit Windows ve 64-bit FreeBSD ortamında doğrudan kullanılamaz.
  * `Extern/include/mysql` başlıklarının FreeBSD 32-bit i386 kökenli MariaDB Connector/C 3.3.5 olduğu belirlendi.
  * MySQL (8.4 LTS, 9.7 LTS, 26.7 Innovation) ve MariaDB (11.4 LTS, 12.3 LTS, 13.0 Stable) resmî sürümleri, C API ve kimlik doğrulama farkları (`m_hDB.reconnect`, `mariadb_reconnect`, `caching_sha2_password`, `m_hDB.host` struct manipülasyonu riski) analiz edildi.
  * `SQLResult` içindeki `uiInsertID`, `uiNumRows`, `uiAffectedRows` alanlarının C API'de 64-bit (`my_ulonglong`) olmasına rağmen 32-bit (`uint32_t`) tutulduğu ve taşma riski taşıdığı tespit edildi.
  * `sql/` dizinindeki 5 şema dosyası (`account.sql`, `common.sql`, `log.sql`, `player.sql`, `world.sql`) ve `db`/`game` kaynak kodları incelendi:
    * `log.sql` içindeki 10 MyISAM tablosunun log I/O ve transaction-free doğası analiz edildi; mevcut verilerde zorla dönüşüm yapılmayıp isteğe bağlı/güvenli göç stratejisi benimsendi.
    * 3 tabloda (`account.shop_expire`, `log_commands.date`, `player.change_empire.last_time`) ve `BanManager.cpp:34` sorgusundaki sıfır tarihlerin (`0000-00-00 00:00:00`) kod içi anlamları araştırıldı (`UNIX_TIMESTAMP`, `str_to_number`). Körlemesine NULL yapılmayıp şema ve kod semantiğine uygun katı mod (`NO_ZERO_DATE`) düzeltme stratejisi belirlendi.
    * Karakter setlerinin (`g_stLocale`: `latin1`, `cp1250`) Metin2 32-bit Windows ANSI istemcisi (`char name[24]`) ile doğrudan bağlantılı olduğu kanıtlandı; körlemesine `utf8mb4` dönüşümü engellendi.
* [x] **Aşama 11.1 (libsql Kontrollü Modernizasyon ve Uçtan Uca Veri Güvenliği):**
  * `libsql` C++20 standardında ve 64-bit native MinGW-w64 GCC 16.2.0 ile derlendi.
  * `m_hDB.host` ve `m_hDB.reconnect` doğrudan `MYSQL` struct dahili alan erişimleri kaldırıldı; temiz `Reconnect()` ve yaşam döngüsü kontrolü sağlandı.
  * `SQLResult` yapısına 64-bit sayaçlar (`ullNumRows`, `ullAffectedRows`, `ullInsertID`) eklendi; eski 32-bit alanlar ve `(uint32_t)-1` hata kontrolleri `db`/`game` uyumluluğu için korunarak çift katmanlı hale getirildi.
  * `SQLResult` içinde kimlik ve etkilenen satır alanlarında sessiz satürasyon yasaklandı; `ullInsertID > UINT32_MAX` durumunda `uiInsertID = 0` (hata) ve `uiSQLErrno = 1264` atanarak sahte kimlik ve veri çakışması kesin olarak engellendi. `ullAffectedRows > UINT32_MAX` durumunda `uiAffectedRows = (uint32_t)-1` hata belirteci atandı. `HasValidInsertID()`, `HasValidAffectedRows()`, `HasValidNumRows()`, `IsOverflow()`, `IsError()`, `IsEmpty()` doğrulama fonksiyonları eklendi.
  * Windows `CAsyncSQL::Quit()` içinde `::CloseHandle(m_hThread)` eklenerek thread handle sızıntısı giderildi; `Quit()` ve `Destroy()` çoklu çağrılarında tam idempotent ve çökmesiz yaşam döngüsü sağlandı.
  * `AsyncSQLThread` içinde ilk bağlantı başarısız olduğunda thread'in sessizce sonlanması engellendi; 1 saniyeden 5 saniyeye kadar kademeli backoff ve `Quit()` ile 100ms alt aralıklarla güvenli sonlanma sağlandı.
  * `AsyncQuery` fire-and-forget sorgularında bağlantı koptuğunda sorguların sessizce silinmesi (`delete p;`) önlendi; `m_queue_failed`, `PushFailedQuery`, `PopFailedQuery` API'si ile başarısız sorguların saklanması sağlandı. En eski sorgunun körlemesine silinmesi (sessiz veri kaybı) kaldırıldı; eleman sayısı ve bellek miktarı (64 MB failed / 128 MB query) çift tavan sınırı getirildi. Sınır aşıldığında `m_bFailedQueueOverflow` bayrağı ile açık geri basınç (backpressure) aktif hale getirildi; `SetFailedQueryCallback` reaktif geri çağırma mimarisi eklendi.
  * `SanitizeQueryForLog` filtresi ile SQL loglarında parola (`PASSWORD('...')`, `password='...'`), token ve kişisel verilerin sızması kesin olarak engellendi.
  * `m_queue_query` kuyruğuna 100.000 eleman ve 128 MB bellek tavan sınır eklenerek kesinti anında sorguların kontrollü reddi sağlandı.
  * Otomatik reconnect sonrasında düşen sorgunun kontrolsüz tekrar çalıştırılması (`ResendQuery`) engellendi; veri bütünlüğü ve non-idempotent güvenlik sağlandı.
  * `Tellwait.h`/`Tellwait.cpp` ve `libthecore/include/signal.h` kök neden düzeltmeleriyle header shadowing giderildi.
  * `tests/test_libsql.cpp` (141 test) ve `run_tests.ps1`/`run_tests.sh` entegrasyonu tamamlandı; Windows amd64 üzerinde 141/141 (toplamda 290/290) test 0 hata ve 0 uyarı ile doğrulandı (+ 1 [SKIP] canlı DB testi).
  * `db` sunucusu SQL veri güvenliği entegrasyonu tamamlandı: `PopFailedQuery` çoklu slot tahliyesi, `HasFailedQueryOverflow` geri basınç tespiti, `CClientManager::AnalyzeQueryResult` ve `AnalyzeFailedQuery` hata denetimi, bağlantı kopmasında UNKNOWN işlem durumu koruması, `cache<T>::SetNeedQuery` ile periyodik kaydetme hatalarında kirli veri korunması, sunucu kapanışında bekleyen sorguların tam tahliyesi sağlandı.
  * GitHub Actions CI iş akışı (Rev 12: Run 38133305694 / Commit d3edb3fd) ile FreeBSD 15.1 amd64 GCC 14 ortamında 290/290 test 0 derleme hatası ve 0 derleyici uyarısı ile tam başarıyla (%100) doğrulandı.
* [x] **Aşama 11.2 (Gerçek Canlı MySQL 8.4 ve MariaDB 12.3/11.4 Entegrasyonu):**
  * Oracle MySQL 8.4 LTS (8.4.11 GA), MariaDB 12.3 LTS (12.3.3 GA) ve MariaDB 11.4 LTS (11.4.13 GA) resmî kaynaklardan doğrulanarak hedef motorlar belirlendi.
  * Windows MinGW-w64 UCRT GCC 16.2.0 uyumlu 64-bit Connector/C (MariaDB 3.4.10) ve MySQL C API (8.4.11) kütüphaneleri `gendef` + `dlltool` ile `libmariadb.a` ve `libmysql.a` olarak başarıyla oluşturuldu.
  * Eski 32-bit `Extern/lib/libmysqlclient.a` (`ELF32-i386`) kütüphanesi tamamen saf dışı bırakıldı; C++20 standardında 0 hata ve 0 uyarı ile derleme sağlandı.
  * Geliştirme makinesini etkilemeyen tamamen izole test ortamları (Port 3307 ve 3308, bağımsız veri dizinleri ve dar yetkili `mt2test` kullanıcısı) kuruldu.
  * Windows üzerinde `tests/test_live_db.cpp` ile 12 test paketlik (TCP Fault Proxy dahil) kapsamlı canlı entegrasyon testi icra edildi:
    * **MariaDB 11.4.13 LTS:** 73/73 kontrol başarılı (%100).
    * **MariaDB 12.3.3 LTS:** 73/73 kontrol başarılı (%100).
    * **Oracle MySQL 8.4.11 LTS:** 73/73 kontrol başarılı (%100).
    * **Windows Canlı Doğrulama:** 219/219 kontrol başarılı (%100).
    * **FreeBSD 15.1 amd64 Canlı Doğrulama (CI Run 38140566854 & Rev 17 / Commit fd0c8482):**
      * Önceki tam koşuda (Run 38140566854) 3 motor üzerinde 168/168 kontrol ve 290/290 modül testi %100 doğrulandı.
      * Rev 17 (Commit fd0c8482) ile 17 ek assertion içeren TCP Fault Injection (toplam 219 kontrol) CI iş akışına aktarıldı.
  * MySQL 8.4 üzerinde varsayılan `caching_sha2_password` kimlik doğrulaması canlı sokette başarıyla kanıtlandı.
  * `STRICT_TRANS_TABLES` ve `NO_ZERO_DATE` altında sıfır tarihli (`0000-00-00 00:00:00`) kayıtların reddi, CRUD işlemleri, `START TRANSACTION`/`COMMIT`/`ROLLBACK` ACID sözleşmesi, Türkçe karakter ve ANSI codepage (`latin1`, `cp1250`) bütünlüğü, çok iş parçacıklı `AsyncSQLThread` kuyruğu üzerinden asenkron sorgu işleme, 64-bit uint64_t ID ve satır alanları somut olarak doğrulandı.
  * TLS bağlantı güvenliğinde `Ssl_cipher != NONE` ve TLSv1.3 el sıkışması doğrulandı; geçerli güvenilen CA ile bağlantı kurulması ve sahte/geçersiz CA ile bağlantının istemci katmanında kesin reddi (`errno 2026`) kanıtlandı.
  * Önceden kesilmiş fiziksel soket koruması (`CR_SERVER_GONE_ERROR`, 2006) ve körlemesine sorgu tekrarlamama (no blind retry) garantilendi.
  * Gerçek TCP ağ arıza enjeksiyonu (`tcp_fault_proxy.hpp`):
    * **Senaryo 11.A:** `SELECT SLEEP(2)` sunucuda işlenirken soket anında koparılmış; istemcide `CR_SERVER_LOST` (2013) / `CR_SERVER_GONE_ERROR` (2006) ve `IsError() == true` üretilmiş, sunucu çökmeden çalışmaya devam etmiştir.
    * **Senaryo 11.B:** Non-idempotent `UPDATE` sorgusu sunucuda commit edildikten sonra sunucunun OK yanıt paketi istemciye ulaşmadan düşürülmüş; istemcinin UNKNOWN işlem durumunu koruduğu, veritabanında değişikliğin fiilen yazıldığı (`initial + 500`), ve `libsql` tarafından asla körlemesine `ResendQuery` yapılmadığı (%100 veri güvenliği) kanıtlanmıştır.
  * Tüm önceki modüllerin 290/290 regresyon testi hem Windows'ta hem de FreeBSD'de hiçbir bozulma olmadan (%100) başarıyla korundu.
* [x] **Aşama 11.4 (DB Modülü 64-Bit Modernizasyonu ve Oyuncu Veri Güvenliği):**
  * **Gerçek 64-Bit Windows DB Derlemesi:**
    * Tüm 30 C++ kaynak dosyası (`Cache.cpp`, `ClientManager.cpp`, `ClientManagerPlayer.cpp`, `Main.cpp`, `DBManager.cpp` vb.) MinGW-w64 GCC 16.2.0 UCRT C++20 ile sıfır hata ile derlendi.
    * Gerçek 64-bit `db.exe` binary'si yerel 64-bit `libthecore.a`, `libsql.a`, `libpoly.a`, `libgame.a` ve `libmariadb.a` ile bağlandı (`pei-x86-64`, 1.295.722 bayt).
    * `singleton.h` 64-bit işaretçi kırpılması (`cast from '...' to 'long int' loses precision`) `intptr_t` kullanılarak kalıcı çözüldü.
    * RapidJSON C++20 atama operatörü ve üye değişken mutability sorunu giderildi.
    * `libthecore/include/signal.h` başlığında `#include_next` sıra düzeltmesiyle header-shadowing özyinelemeli dahil etme döngüsü çözüldü.
    * `ClientManager.h` içinde `AnalyzeQueryResult` ve `AnalyzeFailedQuery` metotları `public:` yapılarak `Main.cpp` erişimine açıldı.
  * **Asenkron Kayıt ve Bellekten Tahliye (Cache Eviction) Güvenliği:**
    * `cache<T>` şablon sınıfına nesil/versiyon takibi (`m_dwVersion`, `m_dwPendingSaveVersion`), in-flight durum bayrağı (`m_bSavePending`), gecikmeli tahliye durumu (`m_bPendingEviction`), UNKNOWN durum bayrağı (`m_bIsUnknown`) ve güvenli tahliye denetimi (`CanEvict()`) eklendi.
    * `FlushPlayerCacheSet`, `FlushItemCacheSet` ve `QUERY_FLUSH_CACHE` çağrılarında nesnelerin körlemesine silinmesi engellendi; `MarkPendingEviction()` ile işaretlenip veritabanından başarı onayı (`OnSaveSuccess`) gelene kadar RAM'de tutulması sağlandı.
    * Kayıt başarısız olduğunda veya bağlantı koptuğunda (`OnSaveFailure`, `AnalyzeFailedQuery`) kirli durum (`m_bNeedQuery = true`) korunarak veri kaybı kesin olarak önlendi.
    * Oyuncu çıkış sonrası veritabanı yanıtı beklenirken veya bağlantı kopmuşken tekrar bağlandığında (`QUERY_PLAYER_LOAD`) RAM'deki en güncel kirli veri kullanılarak eski veritabanı kaydıyla ezilme riski sıfırlandı (`CancelPendingEviction()`).
    * Sunucu kapanışında (`MainLoop` bitişi) tüm oyuncu, eşya ve fiyat listesi önbellekleri tahliyeye işaretlenip `Flush()` edilerek, tüm kuyruklar ve önbellek onayları tamamlanana kadar çalışan güvenli draining döngüsü kuruldu.
  * **Kapsamlı Test Paketi (Senaryo A'dan H'ye):**
    * `tests/test_db_cache.cpp` otomatik test paketi oluşturuldu; 8 kritik senaryoda **120/120 test assertion'ı %100 başarıyla geçti**:
      * **Senaryo A:** Başarılı asenkron kayıt sonrası önbelleğin güvenli tahliyesi.
      * **Senaryo B:** Kayıt başarısızlığında önbelleğin silinmemesi, kirli kalması ve verinin korunması.
      * **Senaryo C:** Çıkış anında SQL bağlantısı kesikse veri kaybı yaşanmaması ve yeniden girişte RAM önceliği.
      * **Senaryo D:** SQL yanıtı beklenirken oyuncu verisinde yeni değişiklik yapıldığında eski yanıtın yeni veriyi geçersiz kılmaması (versiyon koruması).
      * **Senaryo E:** Ağ kesintisi ve UNKNOWN işlem durumunda çifte harcama ve veri çoğalması koruması.
      * **Senaryo F:** Art arda birden fazla kayıt kuyruğa alındığında sıralama garantisi ve gecikmiş ACK koruması.
      * **Senaryo G:** Önbellek nesnesinin yaşam döngüsü ve durum geçişleri.
      * **Senaryo H:** Sunucu kapanışında bekleyen tüm kayıtların eksiksiz tamamlanması (graceful shutdown draining).
    * Toplam modül regresyon test sayısı 290'dan **410 test/assertion'a (%100 PASS)** yükseltildi (`libpoly`: 24, `libthecore`: 97, `libgame`: 28, `libsql`: 141, `db_cache`: 120).
  * **FreeBSD 15.1 CI Entegrasyonu ve Hedefli Rev 18 Düzeltmeleri:**
    * `GuildManager.cpp:7` dosyasının ihtiyaç duyduğu gerçek [locale.hpp](file:///d:/mt22009/mt2009%20-%20FULL%20SOURCE/Server%20Source/Server/game/src/locale.hpp) başlığı CI sparse-checkout kapsamına ve workflow push triggers listesine eklendi.
    * Boost için `.gitignore` içerisinde `Debug/` kuralının `Extern/include/boost` altındaki hata yakalama başlıklarını (`boost/preprocessor/debug/error.hpp` vb.) engellemesi kaldırıldı; resmi FreeBSD `boost-libs` (1.92.0) paketi tüm matris işleri için açık CI bağımlılığı (`prepare: pkg install -y gcc14 gmake boost-libs`) yapıldı.
    * `Server/db/src/Makefile` içerisinden çakışma riski taşıyan `-I../../../Extern/include/boost` kaldırıldı; `-L../../../Extern/lib` düzeltildi; FreeBSD GCC 14 ve MariaDB/MySQL kütüphaneleri için `Wl,-rpath` yönergeleri eklendi; `tests/build` kütüphanelerine erişim için mutlak 5-düzey göreceli yol (`-L../../../../../tests/build`) eklendi.
    * `Server/db/src/IkarusShopCache.cpp:1102` satırında `item.price.yang` (`long long`), FreeBSD amd64 LP64 mimarisinde `int64_t` (`long`) ile `int` overload'ları arasında belirsizlik oluşturan `jsonHelper::insertValue` çağrısı, açık `static_cast<int64_t>` dönüşümü ile 64-bit hassasiyet korunarak kalıcı olarak çözüldü.
    * Windows üzerinde 30/30 kaynak dosya derlemesi, 64-bit `db.exe` üretimi ve 410/410 modül regresyon testi (%100 PASS) doğrulandı.
    * `AGENTS.md` belgesine Bölüm 6 olarak "Kalıcı Git, GitHub ve CI Çalışma Standardı" (görev başlangıcı, geliştirme/test disiplini, Git/GitHub senkronizasyonu, GitHub Actions/FreeBSD CI yönetimi, proje hafızası ve son rapor standartları) eklendi.
    * **FreeBSD 15.1 amd64 GCC 14 Nihai CI Doğrulaması (Rev 18, Run ID: 38159965663, Commit: `2be185b9`):**
      * **MySQL 8.4 LTS (Job ID: 114530620857):** 410/410 modül regresyon testi PASS, gerçek 64-bit `db_r1` (ELF 64-bit LSB executable, x86-64) derlendi ve bağlandı, 73/73 canlı veritabanı testi PASS (%100 Başarı).
      * **MariaDB 12.3 LTS (Job ID: 114530620965):** 410/410 modül regresyon testi PASS, gerçek 64-bit `db_r1` (ELF 64-bit LSB executable, x86-64) derlendi ve bağlandı, 73/73 canlı veritabanı testi PASS (%100 Başarı).
      * **MariaDB 11.4 LTS (Job ID: 114530621091):** 410/410 modül regresyon testi PASS, gerçek 64-bit `db_r1` (ELF 64-bit LSB executable, x86-64) derlendi ve bağlandı, 73/73 canlı veritabanı testi PASS (%100 Başarı).
      * **Genel Durum:** 3/3 matris işi SUCCESS. Windows (PE32+) ve FreeBSD (ELF64) üzerinde `db` sunucusu ve oyuncu veri güvenliği mimarisi %100 doğrulandı.

---

## 7. TEK AKTİF GÖREV

* **Aktif Görev:** DB Modülü 64-Bit Modernizasyonu ve Oyuncu Veri Güvenliği (Aşama 11.4 & Rev 18) — Windows amd64 GCC 16.2 ve FreeBSD 15.1 amd64 GCC 14 üzerinde gerçek 64-bit binary üretimi, Senaryo A-H gecikmeli tahliye mimarisi, 410/410 modül testi ve 219/219 canlı DB testiyle **%100 BAŞARIYLA TAMAMLANDI**.

---

## 8. SONRAKİ GÜVENLİ ADIM

* Kullanıcı onayı doğrultusunda:
  * **Aşama 12 (`liblua` Modernizasyonu):** Lua 5.0 kütüphanesinin C++20 ve 64-bit GCC (Windows UCRT64 & FreeBSD amd64) mimarisine modernizasyonu ve derleme hazırlığı.



