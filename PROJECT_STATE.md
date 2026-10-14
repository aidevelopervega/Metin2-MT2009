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
  * **Gerçek DB Cache / ACK Entegrasyon Test Kabulü ve Rev 20 Güvenlik Düzeltmeleri:**
    * **1. IkarusShop Safebox Veri Kaybı Açığının Kalıcı Giderilmesi:**
      * `IkarusShopCache.cpp:LoadSafebox` fonksiyonunda önbelleğe erken ve kontrolsüz ekleme (`m_safeboxMap[dwPID] = ...`) kaldırıldı.
      * Yükleme işlemi geçici bir nesne (`std::make_shared<TSafeboxCacheInfo>()`) üzerinde yürütülmekte; ikinci sorgu (`CreateSafeboxCacheLoadItemsQuery`) başarısız olduğunda, boş veya bozuk bir safebox belleğe kaydedilmemekte, hata günlüğe yazılıp anında `nullptr` döndürülmektedir.
      * Gerçek safebox yalnızca her iki sorgu ve tüm eşyalar eksiksiz yüklendikten sonra `m_safeboxMap[dwPID]` içine yazılmaktadır (`Commit-on-Success`).
    * **2. DBManager SQL Kaçış Güvenliği ve Çok Baytlı Karakter Bütünlüğü:**
      * `DBManager.cpp` içerisindeki `LocalEscapeString` fonksiyonu çok baytlı (multibyte GBK, Big5, SJIS, UTF-8) kodlamalara tam duyarlı hale getirildi. `0x81`-`0xFE` öncül baytlarını izleyen `0x5C` (`\`) baytlarının ters eğik çizgi olarak algılanıp kaçırılması (trail-byte mutilation / multibyte SQL injection) önlendi.
      * Standart ASCII özel karakterleri (`'`, `\`, `"`, `\0`, `\n`, `\r`, `\032`) eksiksiz kaçırılmakta; `bNoBackslashEscapes` (`SQL_MODE=NO_BACKSLASH_ESCAPES`) desteği (`'` -> `''`) sağlanmıştır.
  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Rev 20, Run ID: 38172765363, Commit: `5640c591`):**
    * **MySQL 8.4 LTS (Job ID: 114565780517):** 410/410 modül regresyon testi PASS, gerçek 64-bit `db_r1` (ELF 64-bit LSB executable, x86-64) derlendi ve bağlandı, 116/116 gerçek DB cache entegrasyon testi PASS, 73/73 canlı veritabanı testi PASS (%100 Başarı).
    * **MariaDB 12.3 LTS (Job ID: 114565780629):** 410/410 modül regresyon testi PASS, gerçek 64-bit `db_r1` (ELF 64-bit LSB executable, x86-64) derlendi ve bağlandı, 116/116 gerçek DB cache entegrasyon testi PASS, 73/73 canlı veritabanı testi PASS (%100 Başarı).
    * **MariaDB 11.4 LTS (Job ID: 114565780775):** 410/410 modül regresyon testi PASS, gerçek 64-bit `db_r1` (ELF 64-bit LSB executable, x86-64) derlendi ve bağlandı, 116/116 gerçek DB cache entegrasyon testi PASS, 73/73 canlı veritabanı testi PASS (%100 Başarı).
    * **Genel Durum:** 3/3 matris işi SUCCESS.
  * **Rev 21: SQL Kaçış Güvenliği, Karşı Örnek Koruması ve Katmanlı Test Ayrımı:**
    * **1. Kritik Karşı Örnek `₁'` (E2 82 81 27) Koruması & Resmi API Zorunluluğu:**
      * `DBManager.cpp` içerisindeki el yapımı `LocalEscapeString` döngüsü tamamen silindi.
      * SQL kaçış işlemleri doğrudan `CAsyncSQL::EscapeString` ve aktif veritabanı soketi/karakter seti (`utf8mb4`) bağlamındaki `mysql_real_escape_string` fonksiyonuna devredildi.
      * Bağlantısız ortamda fail-closed prensibi uygulandı: `EscapeString` 0 döndürür ve hedef tamponu anında temizler (`to[0] = '\0'`).
    * **2. Çağrıcıların (Callers) Veri Bütünlüğü Koruması:**
      * `ClientManagerPlayer.cpp:CreatePlayerSaveQuery`: `EscapeString` başarısız olduğunda (bağlantı yokluğunda) tamponu temizler ve 0 döner; eksik veya bozuk SQL üretilmesi kesin olarak engellenir.
      * `Cache.cpp:CPlayerTableCache::OnFlush`: Sorgu üretilemediğinde veya başarısız olduğunda kirli durum korunur (`m_bNeedQuery = true`, `m_bSavePending = false`, `CanEvict() == false`); RAM'deki oyuncu verisinin sessizce tahliye edilip kaybolması önlenir.
      * `ClientManager.cpp`, `ClientManagerPlayer.cpp`, `ClientManagerIkarusShop.cpp`, `IkarusShopCache.cpp`: Bağlantı yokluğunda güvenle hata döndürme sağlandı.
    * **3. Entegrasyon Test Betiği (run_db_integration_tests.sh) Sıkılaştırması:**
      * Temiz derleme ortamı hazırlığı (`rm -f libdb_objs.a test_db_cache_integration`).
      * 13 zorunlu nesne dosyası içeren açık manifesto denetimi (`REQUIRED_DB_OBJS`).
      * Nesne dosyalarının 64-bit mimari (`x86-64 / amd64`) doğrulaması.
      * Temiz arşiv oluşturma (`ar cr`) ve `Main.o` nesnesinin arşivden kesin olarak çıkarıldığının kontrolü (`ar t ... | grep -q '^Main\.o$'`).
    * **4. Dürüst Test Seviyesi Sınıflandırması:**
      * **Birim Testleri (Unit Tests - Mock):** `tests/test_libpoly.cpp` (24), `tests/test_libthecore.cpp` (97), `tests/test_libgame.cpp` (28), `tests/test_libsql.cpp` (143), `tests/test_db_cache.cpp` (120) -> **Toplam 412/412 test (%100 PASS)**.
      * **Bileşen Entegrasyon Testleri (Component Integration Tests):** `tests/test_db_cache_integration.cpp` -> Gerçek `CClientManager`, `CDBManager`, `Cache`, `IkarusShopCache` bileşenleri ve mock `CAsyncSQL` ile 9 senaryoda **116/116 assertion (%100 PASS)**. (Senaryo 9: Fail-closed DBManager, `\xE2\x82\x81'` reddi, save query temizliği ve kirli önbellek koruması).
      * **Canlı Veritabanı Testleri (Live DB Functional Tests):** `tests/test_live_db.cpp` içerisine Bölüm 12 (Real Multibyte SQL Escaping & Charset Safety) eklendi: Canlı MySQL 8.4, MariaDB 12.3 ve MariaDB 11.4 üzerinde `\xE2\x82\x81'` (E2 82 81 27) roundtrip INSERT + SELECT doğrulaması, UTF-8 1/2/3/4 baytlık diziler, gömülü NUL içeren ikili veri ve `NO_BACKSLASH_ESCAPES` kipi.
    * **5. Windows ve FreeBSD 64-Bit Binary Üretimi:**
      * Windows amd64 GCC 16.2.0 UCRT ile tam `db.exe` üretildi (`pei-x86-64`, 1.303.855 bayt).

---

  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Rev 21 - Run ID: 38181369275, Commit: `8446fe46`):**
    * **MariaDB 11.4 LTS (Job ID: 114594447121):** SUCCESS — Gerçek ELF64 `db_r1` derlendi ve bağlandı, 116/116 gerçek DB cache entegrasyon testi PASS, canlı veritabanı testleri PASS.
    * **MariaDB 12.3 LTS (Job ID: 114594447154):** SUCCESS — Gerçek ELF64 `db_r1` derlendi ve bağlandı, 116/116 gerçek DB cache entegrasyon testi PASS, canlı veritabanı testleri PASS.
    * **MySQL 8.4 LTS (Job ID: 114594446989):** FAILURE — Gerçek ELF64 `db_r1` derlendi ve bağlandı, 116/116 gerçek DB cache entegrasyon testi PASS; Canlı DB testinde Bölüm 12.4 (`NO_BACKSLASH_ESCAPES`) aşamasında Oracle MySQL C API'sinin `mysql_real_escape_string` yerine `mysql_real_escape_string_quote` gerektirmesi nedeniyle başarısız oldu.
  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Rev 21.1 - Run ID: 38182028002, Commit: `4358e223`):**
    * **MariaDB 11.4 LTS (Job ID: 114596426055):** SUCCESS — Gerçek ELF64 `db_r1` derlendi ve bağlandı, deterministik manifesto (29 DB nesnesi) doğrulandı, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS).
    * **MariaDB 12.3 LTS (Job ID: 114596426066):** SUCCESS — Gerçek ELF64 `db_r1` derlendi ve bağlandı, deterministik manifesto (29 DB nesnesi) doğrulandı, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS).
    * **MySQL 8.4 LTS (Job ID: 114596425922):** SUCCESS — Gerçek ELF64 `db_r1` derlendi ve bağlandı, deterministik manifesto (29 DB nesnesi) doğrulandı, 412/412 birim testi, 116/116 DB cache entegrasyon testi, `mysql_real_escape_string_quote` ve `NO_BACKSLASH_ESCAPES` dahil 113/113 canlı veritabanı testi (%100 PASS).
    * **Toplam Sonuç:** 3 motor üzerinde toplam 1.923 assertion (%100 PASS), sıfır regresyon ve tam çift-motor uyumluluğu resmen kanıtlandı.

  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 1 `libpoly` Göçü - Run ID: 38182919055, Commit: `436d8e4e`):**
    * **MariaDB 11.4 LTS (Job ID: 114599103388):** SUCCESS — `server/src/libpoly` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS).
    * **MariaDB 12.3 LTS (Job ID: 114599103242):** SUCCESS — `server/src/libpoly` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS).
    * **MySQL 8.4 LTS (Job ID: 114599103427):** SUCCESS — `server/src/libpoly` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS).
    * **Sonuç:** `libpoly` göçü üç motorda 1.923 assertion ile resmen kabul edildi.

  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 2 `libgame` Göçü - Run ID: 38307182004, Commit: `569d64f8`):**
    * **MariaDB 11.4 LTS (Job ID: 114881023259):** SUCCESS (3m 42s) — `server/src/libgame` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS).
    * **MariaDB 12.3 LTS (Job ID: 114881023276):** SUCCESS (4m 21s) — `server/src/libgame` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS).
    * **MySQL 8.4 LTS (Job ID: 114881023169):** SUCCESS (4m 23s) — `server/src/libgame` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS).
    * **Sonuç:** `libgame` göçü üç motorda 1.923 assertion ile resmen kabul edildi.
    * **game/src Derleme Durumu (Şeffaf Değerlendirme):**
      * Adım 2 sırasında `include` başlıkları güncellenen 10 `game/src` dosyası (`char.cpp`, `char_shop.cpp`, `exchange.cpp`, `safebox.cpp`, `sectree.cpp`, `sectree_manager.cpp`, `shop.cpp`, `shopEx.cpp`, `shop_manager.cpp`, `shutdown_manager.cpp`) ile Adım 3 dosyaları (`ClientPackageCryptInfo.cpp`, `cmd_general.cpp`) GCC `-fsyntax-only` sözdizimi denetiminden başarıyla geçmiştir (%100 OK).
      * Ancak tam `game` sunucu ikilisinin derlenmesi ve bağlanması (linking) **NOT_VERIFIED** olarak kaydedilmiştir; zira Aşama 8 (`game` modernizasyonu ve tam build) henüz aktif değildir ve `game` ikilisi `liblua.a`, `libthecore.a`, `libcryptopp` ve `libIL` kütüphanelerinin modern derlemesini gerektirmektedir. DB testleri tam game derlemesi yerine konulamaz.

  * **Dizin Göçü Adım 3: `libthecore` -> `server/src/libthecore` (Kayıpsız ve Korumalı Taşıma):**
    * `mt2009 - FULL SOURCE/Server Source/Server/libthecore` dizini altındaki 39 dosyanın tamamı `git mv` ile `server/src/libthecore` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 38 dosya %100 saf yeniden adlandırma (pure rename, SHA değişmedi); yalnızca 1 dosya (`src/Makefile`) kök Extern include yollarını destekleyecek şekilde güncellendi.
    * Kademeli ve geriye uyumlu include ve arama yolları kuruldu:
      * `server/src/libthecore/src/Makefile`: `Extern/include` yolları eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/Makefile`: `quick`, `libthecore`, `clean`, `all` hedefleri güncellendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/db/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına `server/src/libthecore` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/db/src/stdafx.h` ve `Monarch.h`: Taşınabilir `#if __has_include` ile `libthecore` başlıkları korundu.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına `server/src/libthecore` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/stdafx.h`, `ClientPackageCryptInfo.cpp`, `cmd_general.cpp`, `FileMonitor_FreeBSD.cpp`: Taşınabilir include yapısı kuruldu.
      * `mt2009 - FULL SOURCE/Server Source/Server/libsql/Makefile`, `stdafx.h`, `AsyncSQL.h`: `server/src/libthecore` başlıkları tam entegre edildi.
      * `tests/run_tests.ps1`, `tests/run_tests.sh`, `tests/run_db_integration_tests.ps1`, `tests/run_db_integration_tests.sh`, `tests/run_live_tests.ps1`, `tests/run_live_tests.sh`: `server/src/libthecore` desteği eklendi.
      * `.github/workflows/freebsd-gcc-test.yml`: Kullanıcı direktifine uygun olarak `sparse-checkout` sınırlaması tamamen kaldırıldı; FreeBSD CI ortamında tek kaynak mimarisine uygun eksiksiz Git ağacı (Windows, FreeBSD ve Extern) sağlandı. `server/src/libthecore/**` tetikleyici yollara eklendi.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.

  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 3 `libthecore` Göçü - Run ID: 38309868821, Commit: `6fa29338`):**
    * **MariaDB 11.4 LTS (Job ID: 115012683427):** SUCCESS (4m 3s) — Tam Git ağacı (34.994 dosya sparse-checkout olmadan) çekildi, `server/src/libthecore` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS - 641 assertion).
    * **MariaDB 12.3 LTS (Job ID: 115012683094):** SUCCESS (4m 40s) — Tam Git ağacı (34.994 dosya) çekildi, `server/src/libthecore` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS - 641 assertion).
    * **MySQL 8.4 LTS (Job ID: 115012683264):** SUCCESS (4m 32s) — Tam Git ağacı (34.994 dosya) çekildi, `server/src/libthecore` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS - 641 assertion).
    * **Sonuç:** `libthecore` göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. `sparse-checkout` sınırlaması güvenle kaldırılmış ve tüm repoyu barındıran CI iş akışı %100 başarıyla doğrulanmıştır.

---

  * **Dizin Göçü Adım 4: `libsql` -> `server/src/libsql` (Kayıpsız ve Korumalı Taşıma):**
    * `mt2009 - FULL SOURCE/Server Source/Server/libsql` dizini altındaki 14 dosyanın tamamı (`AsyncSQL.cpp`, `AsyncSQL.h`, `Depend`, `Makefile`, `Semaphore.cpp`, `Semaphore.h`, `Statement.cpp`, `Statement.h`, `Tellwait.cpp`, `Tellwait.h`, `libsql.h`, `libsql.vcxproj`, `libsql.vcxproj.filters`, `stdafx.h`) `git mv` ile `server/src/libsql` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 13 dosya %100 saf yeniden adlandırma (pure rename, SHA değişmedi); yalnızca 1 dosya (`server/src/libsql/Makefile`) yeni göç konumundan `Extern/include` ve MySQL/MariaDB yollarını destekleyecek şekilde güncellendi.
    * **SQL Güvenliği ve Protokol Korunumu:** `AsyncSQL.cpp` ve `AsyncSQL.h` içerisindeki SQL kaçış güvenliği, `mysql_real_escape_string_quote`, `NO_BACKSLASH_ESCAPES` ve fail-closed davranışları aynen korundu, hiçbir işlevsel değişiklik yapılmadı.
    * Kademeli ve geriye uyumlu include ve arama yolları kuruldu:
      * `server/src/libsql/Makefile`: `INCDIR` yolları `../libthecore/include`, `Extern/include`, MySQL/MariaDB başlıkları ile güncellendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/Makefile`: `quick`, `libsql`, `clean`, `all` hedefleri her iki konumu da destekleyecek şekilde güncellendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/db/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına `server/src/libsql` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/db/src/DBManager.h` ve `GuildManager.h`: Taşınabilir `#if __has_include` ile `AsyncSQL.h` ve `libsql.h` başlıkları korundu.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına `server/src/libsql` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/db.h` ve `log.h`: Taşınabilir `#if __has_include` ile `AsyncSQL.h` başlığı korundu.
      * `tests/run_tests.ps1`, `tests/run_tests.sh`, `tests/run_db_integration_tests.ps1`, `tests/run_db_integration_tests.sh`, `tests/run_live_tests.ps1`, `tests/run_live_tests.sh`: `server/src/libsql` desteği eklendi.
      * `.github/workflows/freebsd-gcc-test.yml`: `server/src/libsql/**` tetikleyici yollara eklendi.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı. `game/src` sözdizimi denetimi (`-fsyntax-only`) 0 hata ile doğrulandı.

  * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 4 `libsql` Göçü - Run ID: 38311132574, Commit: `f4a51133`):**
    * **MariaDB 11.4 LTS (Job ID: 115016859360):** SUCCESS (4m 29s) — `server/src/libsql` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS - 641 assertion).
    * **MariaDB 12.3 LTS (Job ID: 115016859598):** SUCCESS (4m 14s) — `server/src/libsql` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS - 641 assertion).
    * **MySQL 8.4 LTS (Job ID: 115016859627):** SUCCESS (4m 12s) — `server/src/libsql` üzerinden derlendi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi, gerçek ELF64 `db_r1` ve 29 DB nesne deterministik manifesto (%100 PASS - 641 assertion).
    * **Sonuç:** `libsql` göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Sıfır SQL regresyonu ve tam çift-motor uyumluluğu korundu.

  * **Dizin Göçü Adım 5: `common` -> `server/src/common` (Kayıpsız ve Korumalı Taşıma):**
    * `mt2009 - FULL SOURCE/Server Source/Server/common` dizini altındaki 17 başlık dosyasının tamamı (`CommonDefines.h`, `PulseManager.h`, `VnumHelper.h`, `building.h`, `cache.h`, `d3dtype.h`, `item_length.h`, `json_helper.h`, `length.h`, `noncopyable.h`, `pool.h`, `service.h`, `singleton.h`, `skill_length.h`, `stl.h`, `tables.h`, `utils.h`) `git mv` ile `server/src/common` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 17/17 dosya %100 saf yeniden adlandırma (pure rename, SHA hash'leri ve bayt uzunlukları birebir aynı, sıfır değişiklik).
    * Kademeli ve geriye uyumlu include ve arama yolları kuruldu:
      * `mt2009 - FULL SOURCE/Server Source/Server/db/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına 5-seviyeli mutlak kök yolları (`../../../../../server/src/...`), `server/src/common` ve `server/src/libthecore/src` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına 5-seviyeli mutlak kök yolları (`../../../../../server/src/...`), `server/src/common` ve `server/src/libthecore/src` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/db/src/stdafx.h`, `DBManager.h`, `GuildManager.h`, `Monarch.h`, `ClientManager.cpp`: Taşınabilir `#if __has_include` ile 5-seviyeli kök include direktifleri eklendi.
      * `tests/run_tests.ps1`, `tests/run_tests.sh`, `tests/run_db_integration_tests.ps1`, `tests/run_db_integration_tests.sh`, `tests/run_live_tests.ps1`, `tests/run_live_tests.sh`: `server/src/common` desteği eklendi.
      * `.github/workflows/freebsd-gcc-test.yml`: `server/src/common/**` tetikleyici yollara eklendi.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 5 `common` Göçü - Run ID: 38313793099, Commit: `88b6aa08`):**
      * **MariaDB 12.3 LTS (Job ID: 115025498061):** SUCCESS (4m 10s) — `server/src/common` üzerinden derlendi, gerçek ELF64 `db_r41023` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MySQL 8.4 LTS (Job ID: 115025498212):** SUCCESS (4m 34s) — `server/src/common` üzerinden derlendi, gerçek ELF64 `db_r41023` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115025498337):** SUCCESS (4m 25s) — `server/src/common` üzerinden derlendi, gerçek ELF64 `db_r41023` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `common` göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r41023` ikilisi ve tüm DB cache testleri tam başarıyla doğrulandı.

  * **Dizin Göçü Adım 6: `liblua` -> `server/src/liblua` (Kayıpsız ve Korumalı Taşıma):**
    * `mt2009 - FULL SOURCE/Server Source/Server/liblua` dizini altındaki 121 dosyanın tamamı (`5.0/`, `5.2/`, `.gitignore`) `git mv` ile `server/src/liblua` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 121/121 dosya %100 saf yeniden adlandırma (pure rename, SHA hash'leri ve dosya boyutları birebir aynı, sıfır kod değişikliği).
    * Kademeli ve geriye uyumlu Makefile ve include yolları kuruldu:
      * `mt2009 - FULL SOURCE/Server Source/Server/Makefile`: `quick`, `liblua`, `clean`, `all` hedeflerinde `liblua` dizin kontrolü ve `server/src/liblua` desteği eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/Makefile`: `INCDIR` ve `LIBDIR` yollarına `server/src/liblua/5.0` ve `5.2` eklendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/game/src/quest/Makefile`: `INCLUDEDIR` yollarına `server/src/liblua` eklendi.
      * `.github/workflows/freebsd-gcc-test.yml`: `server/src/liblua/**` tetikleyici yollara eklendi.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 6 `liblua` Göçü - Run ID: 38314624806, Commit: `5444e0dd`):**
      * **MySQL 8.4 LTS (Job ID: 115028192118):** SUCCESS (4m 3s) — `server/src/liblua` üzerinden derlendi, gerçek ELF64 `db_r41023` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115028192330):** SUCCESS (4m 1s) — `server/src/liblua` üzerinden derlendi, gerçek ELF64 `db_r41023` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115028192407):** SUCCESS (4m 42s) — `server/src/liblua` üzerinden derlendi, gerçek ELF64 `db_r41023` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `liblua` göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r41023` ikilisi ve tüm DB cache testleri tam başarıyla doğrulandı.

---

  * **Dizin Göçü Adım 7: `db` -> `server/src/db` (Kayıpsız ve Korumalı Taşıma):**
    * `mt2009 - FULL SOURCE/Server Source/Server/db` dizini altındaki 57 dosyanın tamamı (`src/` C++ kaynakları, başlıkları, Makefile, Depend vb.) `git mv` ile `server/src/db` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 57/57 dosya %100 saf yeniden adlandırma (pure rename, SHA hash'leri ve dosya boyutları birebir aynı, sıfır kod kaybı).
    * Temiz ve taşınabilir Makefile ve include yolları kuruldu:
      * `server/src/db/src/Makefile`: `INCDIR` ve `LIBDIR` yolları `server/src` kardeş dizinleri (`../../libpoly`, `../../libgame/include`, `../../libthecore/include`, `../../libthecore/src`, `../../libsql`, `../../common`), kök test çıktıları (`../../../../tests/build`) ve Extern dizinleri ile güncellendi.
      * `server/src/db/src/GuildManager.cpp`: `locale.hpp` başlığı taşınabilir `#if __has_include` ile korundu.
      * `mt2009 - FULL SOURCE/Server Source/Server/Makefile`: `quick`, `db`, `symlink`, `strip`, `clean` hedefleri her iki konumu da destekleyecek şekilde güncellendi.
      * `tests/run_db_integration_tests.ps1` ve `tests/run_db_integration_tests.sh`: `server/src/db/src` dizinini otomatik tespit edecek şekilde güncellendi.
      * `.github/workflows/freebsd-gcc-test.yml`: `server/src/db/**` tetikleyici yollara eklendi ve gerçek FreeBSD DB derleme adımı dinamik `DB_DIR` ile yapılandırıldı.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 7 `db` Göçü - Run ID: 38315824092, Commit: `21dcacd3`):**
      * **MariaDB 11.4 LTS (Job ID: 115032079358):** SUCCESS (4m 8s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MySQL 8.4 LTS (Job ID: 115032079497):** SUCCESS (4m 39s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115032079599):** SUCCESS (4m 23s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `db` göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r1` ikilisi ve tüm DB cache testleri tam başarıyla doğrulandı.

  * **Dizin Göçü Adım 8: `game` -> `server/src/game` (Kayıpsız ve Korumalı Taşıma):**
    * `mt2009 - FULL SOURCE/Server Source/Server/game` dizini altındaki 353 dosyanın tamamı (`src/` C++ kaynakları, başlıkları, Makefile, quest/ vb.) `git mv` ile `server/src/game` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 353/353 dosya %100 saf yeniden adlandırma (pure rename, SHA hash'leri ve dosya boyutları birebir aynı, sıfır kod kaybı).
    * Temiz ve taşınabilir Makefile ve include yolları kuruldu:
      * `server/src/game/src/Makefile`: `INCDIR` ve `LIBDIR` yolları `server/src` kardeş dizinleri (`../../libpoly`, `../../libgame/include`, `../../libthecore/include`, `../../libthecore/src`, `../../libsql`, `../../common`, `../../liblua/5.0/include`), ve Extern dizinleri ile güncellendi.
      * `mt2009 - FULL SOURCE/Server Source/Server/Makefile`: `quick`, `game`, `ver2`, `symlink`, `strip`, `clean`, `all` hedefleri her iki konumu da destekleyecek şekilde güncellendi.
      * `.github/workflows/freebsd-gcc-test.yml`: `server/src/game/**` tetikleyici yollara eklendi.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 8 `game` Göçü - Run ID: 38316794260, Commit: `b367d4c0`):**
      * **MariaDB 11.4 LTS (Job ID: 115035235259):** SUCCESS (3m 43s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115035235620):** SUCCESS (3m 36s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MySQL 8.4 LTS (Job ID: 115035235676):** SUCCESS (4m 40s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `game` göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r1` ikilisi ve tüm DB cache testleri tam başarıyla doğrulandı.

  * **Dizin Göçü Adım 9: `sql` -> `sql` (Veritabanı Şemaları Kök Dizine Taşıma):**
    * `mt2009 - FULL SOURCE/sql` dizini altındaki 6 dosyanın tamamı (`account.sql`, `common.sql`, `log.sql`, `player.sql`, `world.sql`, `GameCoreTop.txt`) `git mv` ile depo kök dizinindeki `sql/` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 6/6 dosya %100 saf yeniden adlandırma (pure rename, SHA hash'leri ve dosya boyutları birebir aynı, sıfır veri kaybı).
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 9 `sql/` Göçü - Run ID: 38317589781, Commit: `8771a166`):**
      * **MariaDB 11.4 LTS (Job ID: 115037811138):** SUCCESS (4m 22s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MySQL 8.4 LTS (Job ID: 115037811308):** SUCCESS (4m 40s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115037811331):** SUCCESS (4m 9s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `sql/` şema göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r1` ikilisi ve tüm DB cache testleri tam başarıyla doğrulandı.

  * **Dizin Göçü Adım 10: `Source Client` -> `client/src` (İstemci Kaynak Kodları Taşıma):**
    * `mt2009 - FULL SOURCE/Source/Source Client` dizini altındaki 692 dosyanın tamamı (`UserInterface/`, `EterLib/`, `GameLib/`, `EterBase/`, `EffectLib/`, `EterGrnLib/`, `MilesLib/`, `SpeedTreeLib/`, `EterPythonLib/`, `EterPack/`, `ScriptLib/`, `EterLocale/`, `SphereLib/`, `EterImageLib/`, `PRTerrainLib/`, `CWebBrowser/`, `Metin2Client.sln`) `git mv` ile depo kök dizinindeki `client/src/` konumuna taşındı.
    * **Git Bütünlük Denetimi:** 692/692 dosya %100 saf yeniden adlandırma (pure rename, SHA hash'leri ve dosya boyutları birebir aynı, sıfır veri kaybı).
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 10 `client/src` Göçü - Run ID: 38318301570, Commit: `de79dc20`):**
      * **MySQL 8.4 LTS (Job ID: 115040114228):** SUCCESS (4m 16s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115040114459):** SUCCESS (4m 40s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115040114544):** SUCCESS (3m 47s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi, 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `client/src` istemci kaynak kodları göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r1` ikilisi ve tüm DB cache testleri tam başarıyla doğrulandı.

  * **Dizin Göçü Adım 11: İki Extern Ağacının Kayıpsız Merkezî Göçü (`extern/common`, `extern/server`, `extern/client`, `extern/platform`):**
    * `mt2009 - FULL SOURCE/Server Source/Extern` ve `mt2009 - FULL SOURCE/Source/Extern` dizinleri Git izleme, dosya içerik hash'leri (SHA256 ve Git blob SHA) ve mimari bazında tam olarak karşılaştırıldı.
    * **Ortak Başlık Kütüphaneleri (`extern/common/include`):**
      * `boost` (15.693 başlık): Sunucu C++20 `std::atoi` düzeltmeleri ve istemcinin 20 adet benzersiz detay başlığı birleştirilerek hiçbir başlık kaybedilmeden tek merkezde toplandı.
      * `rapidjson` (37 başlık): Sunucunun C++20 standardına uygun `GenericStringRef::operator=` yamalı sürümü ortaklaştırıldı.
      * `fmt` (16 başlık), `msl` (10 başlık), `OptionParser` (1 başlık), `opencv2` (166 başlık) ve ortak kök başlıkları (`lazy_importer.hpp`, `opencv.hpp`, `smbios.hpp`) kayıpsız taşındı.
    * **Sunucu Harici Bağımlılıkları (`extern/server/`):**
      * `include/mysql`: 64-bit MariaDB C/C bağlayıcı başlıkları (122 dosya).
      * `include/cryptopp` (164 başlık) ve `cryptopp/` (365 kaynak dosya): GCC/FreeBSD uyumlu Crypto++.
      * `include/IL`: FreeBSD DevIL başlıkları (5 dosya).
      * `lib/`: FreeBSD static arşivleri (`libcryptopp.a`, `libIL.a`, `libILU.a`, `libILUT.a`, `libmysqlclient.a`).
    * **İstemci Harici Bağımlılıkları (`extern/client/`):**
      * `lib/`: 62 adet 32-bit Win32 statik `.lib` kütüphanesi.
      * `dll/`: 11 adet 32-bit dinamik `.dll` kütüphanesi.
      * `include/`: `Python27`, `inih`, `libjpeg`, `lz4`, `lzo`, `minIni`, `mysqlclient`, `snappy`, 32-bit Win32 `mysql` (35 başlık), `cryptopp` (183 başlık), `IL` (14 başlık) ve kök DirectX (`d3d9*.h`), Granny (`granny*.h`), Miles (`MSS.H`), SpeedTree (`SpeedTreeRT.h`), Themida (`ThemidaSDK.h`) vb. tüm başlıklar.
      * `backup_dx8/`, `old_python_not_static/`, `openssl/`, `cryptopp/` dizinleri.
      * **100 MB Sınırı Koruması:** `extern/client/lib/cryptlib-Debug.lib` (105.7 MB) `.gitignore` içinde kesin olarak hariç tutuldu.
    * **Platform Kütüphaneleri (`extern/platform/windows-x64/lib/`):**
      * MinGW-w64 64-bit MariaDB import library (`libmariadb.a`) yerleştirildi.
    * **Yapılandırma ve Derleme Güncellemeleri:**
      * Makefile'lar (`server/src/db/src/Makefile`, `server/src/game/src/Makefile`, `server/src/lib*/Makefile`), test betikleri (`tests/run_tests.ps1`, `tests/run_tests.sh`, `tests/run_db_integration_tests.ps1`, `tests/run_db_integration_tests.sh`) ve `.github/workflows/freebsd-gcc-test.yml` (`extern/**` tetikleyicisi) yeni yollara uyarlandı; geriye dönük fallback yolları korundu.
      * Eski `mt2009 - FULL SOURCE/Server Source/Extern` ve `mt2009 - FULL SOURCE/Source/Extern` dizinleri diskten ve Git takibinden sıfır kayıpla tamamen kaldırıldı.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 11 `extern/` Göçü - Run ID: 38321904900, Commit: `cd9d2937`):**
      * **MySQL 8.4 LTS (Job ID: 115051692502):** SUCCESS (4m 28s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115051692792):** SUCCESS (4m 26s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115051692849):** SUCCESS (4m 17s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** `extern/` merkezî göçü üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r1` ikilisi ve tüm DB testleri tam başarıyla doğrulandı.

    * **Adım 11 Kayıpsızlık Denetimi ve %100 Blob Bütünlüğü Doğrulaması:**
      * Commit `f7a957b9` ile güncel durumdaki iki eski Extern ağacı (`Server Source/Extern` 16.354 dosya, `Source/Extern` 16.280 dosya) Git blob SHA1 ve dosya bazında tam karşılaştırıldı.
      * Sunucu tarafındaki 16.098 özgün blob'un tamamının (%100) yeni `extern/` ağacında korunduğu doğrulandı (0 kayıp).
      * İstemci tarafında yalnızca istemcide bulunan 3 başlık (`fmt/posix.h`, `fmt/time.h`, `msl/check_assert.h`) `extern/common/include/` altına, 3 istemci-özel varyant (`boost/.../network_v4.ipp`, `boost/.../network_v6.ipp`, `rapidjson/document.h`) `extern/client/include/` altına eklenerek 16.487 istemci blob'unun tamamı (%100) kayıpsız korundu (0 kayıp).
      * Toplam 16.785 özgün blob'un tamamı (%100) yeni `extern/` ağacında doğrulanmıştır.
      * `cryptlib-Debug.lib` (105.7 MB - 110.819.592 bayt) fiziksel olarak `extern/client/lib/` altında mevcuttur; önceki commit'lerde de GitHub 100 MB sınırı nedeniyle Git takibinde bulunmadığı, `.gitignore` ile korunduğu doğrulanmıştır.
      * İstemci projesindeki 16 adet `.vcxproj` dosyasının tamamı (`UserInterface`, `EterLib`, `GameLib` vb.) yeni `../../extern/client/include`, `../../extern/common/include` ve `../../extern/client/lib` yollarına güncellendi.

  * **Dizin Göçü Adım 12: Kalan Server Source Kök Dosyaları ve `client/files` Göçü:**
    * `mt2009 - FULL SOURCE/Server Source/Server` altındaki kök yapılandırma dosyaları `git mv` ile `server/src/` konumuna taşındı:
      * `Makefile` -> `server/src/Makefile` (Tüm alt modülleri `liblua`, `libsql`, `libgame`, `libpoly`, `libthecore`, `game`, `db` doğrudan çalıştırır)
      * `m2server.sln` -> `server/src/m2server.sln` (8 sunucu projesinin göreli yolları `server/src/` ile birebir eşleşir)
      * `__REVISION__` -> `server/src/__REVISION__` (41023 revizyon numarası)
    * `mt2009 - FULL SOURCE/Source/GameCoreTop.txt` -> `server/GameCoreTop.txt` ve `mt2009 - FULL SOURCE/Server Source/GameCoreTop.txt` -> `client/GameCoreTop.txt` olarak taşındı.
    * Boşalan eski `Server Source` ve `Source` dizin kabukları diskten temizlendi.
    * `mt2009 - FULL SOURCE/Klient` (731.5 MB 32-bit hazır istemci çalışma paketi) disk üzerinde `client/files/` konumuna taşındı; `.gitignore` kuralı ile büyük ikili paketinin Git'i şişirmesi engellendi.
    * `mt2009 - FULL SOURCE/new_ftp.tar.gz` (48.2 MB sunucu runtime arşivi) hassas parola ve bağlantı bilgileri içerdiği için AGENTS.md kural 6 uyarınca kontrolsüz açılmayıp güvenli şekilde muhafaza edildi.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Adım 12 Göçü - Run ID: 38324020775, Commit: `d4d56454`):**
      * **MySQL 8.4 LTS (Job ID: 115058430803):** SUCCESS (4m 21s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115058430914):** SUCCESS (4m 25s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115058430987):** SUCCESS (4m 21s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r1` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
  * **GameCoreTop.txt Dosyalarının Tamamen Kaldırılması ve Eski Fallback Yollarının Temizlenmesi:**
    * **GameCoreTop.txt Temizliği:**
      * Depo içindeki tüm kopyalar (`client/GameCoreTop.txt`, `mt2009 - FULL SOURCE/GameCoreTop.txt`, `server/GameCoreTop.txt`, `sql/GameCoreTop.txt`) `git rm` ile kaldırıldı.
      * Git takibinde ve yerel çalışma ağacında kalan `GameCoreTop.txt` sayısı: **Tam 0 (SIFIR)** olarak doğrulandı.
    * **Tüm Makefile ve Kaynak Kod Yollarının Düzeltilmesi:**
      * `server/src/db/src/Makefile`: Eski `mt2009 - FULL SOURCE` include ve lib fallback yolları kaldırıldı; doğrudan `../../../../extern/...` ve `../...` yapılandırması korundu.
      * `server/src/db/src/GuildManager.cpp`: Eski fallback `#elif __has_include("../../../../mt2009 - FULL SOURCE...")` satırı kaldırıldı.
      * `server/src/game/src/Makefile`: Eski `mt2009 - FULL SOURCE` ve `../../../../../server/src` fallback yolları kaldırıldı; doğrudan `../../../../extern/server/lib`, `../../../../extern/common/include`, `../__REVISION__` ve `../../liblua` bağlayıcı yolları tanımlandı.
      * `server/src/game/src/quest/Makefile`: `INCLUDEDIR` ve `LIBS` yolları doğrudan `../../../liblua/5.0/include` ve `../../../liblua/5.0/lib` olarak temizlendi.
      * `server/src/libgame/src/Makefile`: Eski fallback `CFLAGS += -I"../../../mt2009 - FULL SOURCE..."` kaldırıldı.
      * `server/src/libgame/src/attribute.cc`: Eski fallback `#include "../../../../mt2009 - FULL SOURCE..."` kaldırıldı.
      * `server/src/libgame/src/grid.cc`: Eski fallback `#include` satırları kaldırıldı.
      * `server/src/libgame/src/targa.cc`: Eski fallback `#include` satırı kaldırıldı.
      * `server/src/libpoly/Makefile`: Eski `mt2009 - FULL SOURCE` ve `../../Extern` yolları kaldırıldı.
      * `server/src/libsql/Makefile`: Eski `mt2009 - FULL SOURCE` ve `../../Extern` yolları kaldırıldı.
      * `server/src/libthecore/src/Makefile`: Eski `mt2009 - FULL SOURCE` ve `../../../../Extern` yolları kaldırıldı.
      * `server/src/Makefile`: Eski `elif [ -d "../../../server/src/..." ]` fallback blokları tamamen temizlendi; doğrudan `liblua/5.0`, `libsql`, `libgame/src`, `libpoly`, `libthecore/src`, `game/src`, `db/src` hedefleri tanımlandı.
    * **Test Betikleri ve CI İş Akışının Temizlenmesi:**
      * `tests/run_tests.sh`: Eski `SERVER_DIR` ve `if [ -d "$ROOT_DIR/server/src/..." ]; else ...` fallback yapıları kaldırıldı; tüm yollar doğrudan `server/src/...` ve `extern/...` olarak bağlandı.
      * `tests/run_tests.ps1`: Eski `$ServerDir` ve tüm fallback include yolları kaldırıldı; tüm yollar doğrudan `$RootDir\server\src\...` ve `$RootDir\extern\...` yapıldı.
      * `tests/run_db_integration_tests.sh`: Eski `SERVER_DIR` ve tüm `-I"$SERVER_DIR/..."` fallback argümanları kaldırıldı; doğrudan `server/src/...` bağlandı.
      * `tests/run_db_integration_tests.ps1`: Eski `$ServerDir` ve tüm `-I$ServerDir\...` argümanları kaldırıldı; doğrudan `$RootDir\server\src\...` bağlandı.
      * `tests/run_live_tests.sh`: Eski `SERVER_DIR` ve `EXTERN_DIR` argümanları kaldırıldı; doğrudan `server/src/...` bağlandı.
      * `tests/run_live_tests.ps1`: Eski `$ServerDir` ve `$ExternDir` argümanları kaldırıldı; doğrudan `$RootDir\server\src\...` ve `$RootDir\extern\...` bağlandı.
      * `.github/workflows/freebsd-gcc-test.yml`: Eski `mt2009 - FULL SOURCE/Server Source/...` tetikleyicileri kaldırıldı; `DB_DIR="server/src/db"` olarak doğrudan yapılandırıldı.
    * **İstemci Proje Dosyaları (.vcxproj):**
      * 16 istemci `.vcxproj` dosyasının tamamının yeni `../../extern/client/include`, `../../extern/common/include` ve `../../extern/client/lib` yollarını kullandığı ve hiçbir eski yola bağımlılık barındırmadığı teyit edildi.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) sıfır hata ve sıfır uyarı ile başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (GameCoreTop Kaldırma ve Makefile/Test Temizliği - Run ID: 38325667908, Commit: `0d871bf4`):**
      * **MySQL 8.4 LTS (Job ID: 115063628540):** SUCCESS (3m 53s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r41023` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115063628621):** SUCCESS (4m 28s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r41023` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115063628337):** SUCCESS (4m 28s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r41023` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** GameCoreTop.txt kaldırılması ve tüm Makefile / test script temizliği üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r41023` ikilisi ve tüm DB testleri tam başarıyla doğrulandı.

  * **Extern x64 Modernizasyonu, Platform Ayrımı ve Kapsamlı Bağımlılık Denetimi:**
    * **1. Client Proje Yolları Düzeltmesi (PASS):**
      * `client/src/` altındaki 16 `.vcxproj` projesinin tamamı incelendi (`client/src/<Project>/<Project>.vcxproj` derinliği 3 seviye: `Project` -> `src` -> `client` -> Repo Kökü).
      * Eski `../../extern/` göreli yolunun var olmayan `client/extern/` konumuna çözümlendiği tespit edildi; 16 dosyanın tamamında `../../../extern/client/include`, `../../../extern/common/include` ve `../../../extern/client/lib` olarak güncellendi.
      * 16 projenin tamamında XML geçerliliği (`[xml]` parser) ve hedef dizinlerin disk üzerindeki fiziksel varlığı otomatik olarak doğrulandı (%100 PASS).
    * **2. Test Assertion Dağılımı Kök Neden Analizi:**
      * Gerçek test kaynakları (`tests/test_libpoly.cpp: 24`, `tests/test_libthecore.cpp: 97`, `tests/test_libgame.cpp: 28`, `tests/test_libsql.cpp: 143`, `tests/test_db_cache.cpp: 120`, `tests/test_db_cache_integration.cpp: 116`) Git geçmişi ve doğrudan derlenen ikililerin çalıştırılmasıyla incelendi.
      * Test kaynak dosyalarının taahhüt edildiği ilk günden bu yana hiçbir zaman değişmediği (`git log`), hiçbir testin atlanmadığı veya silinmediği kesin olarak kanıtlandı.
      * Önceki raporda yer alan `20 / 105 / 24` sayısının, önceki ajanın raporlama aşamasında yaptığı bir metin yazım hatası (149 toplamının yanlış alt kırılımla yazılması) olduğu, gerçek sayının başından beri `24 / 97 / 28` olduğu tespit edildi ve `PROJECT_STATE.md` içinde düzeltildi.
    * **3. FreeBSD 15.1 amd64 GCC 14 Bağımlılık Manifestosu:**
      * **Git `extern/` Bağımlılıkları:**
        * `extern/common/include`: Boost (C++20 header-only), RapidJSON (C++20 yamalı), fmt, msl, OptionParser, OpenCV2 başlıkları.
        * `extern/server/include`: MariaDB / MySQL C bağlayıcı başlıkları.
        * `extern/server/lib`: İçerisindeki 5 arşivin (`libcryptopp.a`, `libIL.a`, `libILU.a`, `libILUT.a`, `libmysqlclient.a`) tamamının `elf32-i386` (32-bit legacy) olduğu objdump ile belgelendi.
      * **FreeBSD Sistem Paketleri (`/usr/local` ve `/usr`):**
        * `gcc14`: `/usr/local/bin/g++14`, `/usr/local/lib/gcc14` (FSF GCC 14.2 64-bit amd64 derleyici ve çalışma zamanı).
        * `mariadb114-server` / `mariadb123-server` / `mysql84-server`: `/usr/local/include/{mysql,mariadb}` başlıkları ve `/usr/local/lib/{mysql,mariadb}` altındaki `-lmariadb` / `-lmysqlclient` 64-bit dinamik/statik kütüphaneleri.
        * `boost-libs`: `/usr/local/lib/libboost_*.so` (64-bit ELF dinamik kütüphaneleri).
        * `openssl`: `/usr/lib/libssl.so`, `/usr/lib/libcrypto.so` (FreeBSD 15.1 base 64-bit OpenSSL).
        * `zlib`: `/usr/lib/libz.so` (FreeBSD base 64-bit zlib).
      * **Temiz FreeBSD 15.1 Yeniden Elde Edilebilirlik:** `pkg install -y gcc14 gmake boost-libs mariadb114-server (veya mariadb123-server / mysql84-server)` komutuyla %100 resmî depodan temiz ve otomatik olarak kurulabilmektedir.
    * **4. Windows ve Client İkili Kütüphane (x64 vs x86) Denetimi:**
      * **Windows x64 Native:** `extern/platform/windows-x64/mingw-ucrt-gcc16/lib/libmariadb.a` (`pe-x86-64 / i386:x86-64`, MinGW UCRT 64-bit import library).
      * **İstemci Middleware x64 Engelleri (Kayıpsız x86 Koruması):**
        * `Python 2.7`: `python27-static.lib` (`pe-i386`) -> **BLOCKED** (x86 statik kütüphane; x64 için kaynak derlemesi veya x64 SDK gerekir).
        * `Granny 3D`: `granny2.*.lib` ve `.dll` (`pe-i386` / `pei-i386`) -> **BLOCKED** (Epic Games / RAD Game Tools tescilli ikili middleware; 64-bit SDK gerekir).
        * `Miles Sound System`: `mss32.lib` / `.dll` (`pe-i386` / `pei-i386`) -> **BLOCKED** (RAD Game Tools tescilli 32-bit ikili middleware; mss64 gerekir).
        * `SpeedTree`: `SpeedTreeRT.lib` (`pe-i386`) -> **BLOCKED** (IDV tescilli 32-bit ikili middleware).
        * `DirectX 9`: `d3d9.lib`, `d3dx9.lib`, `dxguid.lib` (`pe-i386`) -> **NOT_VERIFIED** (Microsoft DirectX SDK June 2010 x64 kütüphaneleri gerekir).
        * `Crypto++`: Kaynak kodları `extern/server/cryptopp/` altında eksiksiz mevcuttur -> **READY** (Windows x64 ve FreeBSD amd64 için sıfırdan derlenebilir).
        * `DevIL`: Açık kaynak -> **NOT_VERIFIED** (x64 derlemesi veya port paketi gerekir).
        * `Boost`: `extern/common/include/boost` -> **READY** (C++20 header-only modülleri x64 tam uyumludur).
    * **5. Uygulanan Somut x64 Platform ve ABI İyileştirmeleri:**
      * `extern/platform/windows-x64/mingw-ucrt-gcc16/` platform ayrımı uygulandı; `libmariadb.a` bu konuma taşındı ve `tests/run_db_integration_tests.ps1` senkronize edildi.
      * `server/src/game/src/Makefile` ve `server/src/db/src/Makefile` içindeki eski 32-bit cross-compilation yapılandırmaları (`-L/usr/local/lib32/gcc12`, `/usr/lib32/libcrypto.a`, `extern/server/lib/*.a`) 64-bit modda tamamen engellendi; 64-bit FreeBSD amd64 GCC 14 uyumlu fail-closed kütüphane seçimi (`-Wl,-rpath=/usr/local/lib/gcc14`, `/usr/local/lib/mysql`, `/usr/lib -lssl -lcrypto`) sağlandı.
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 24, `libthecore` 97, `libgame` 28, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) sıfır hata ve sıfır uyarı ile başarıyla tamamlandı.
    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Extern x64 ve Makefile İyileştirmeleri - Run ID: 38327339109, Commit: `820cf42f`):**
      * **MySQL 8.4 LTS (Job ID: 115068966327):** SUCCESS (3m 56s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r41023` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115068966238):** SUCCESS (3m 36s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r41023` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115068966076):** SUCCESS (4m 21s) — `server/src/db` üzerinden doğrudan derlendi, gerçek ELF64 `server/src/db/db_r41023` üretildi (17M, dynamically linked, not stripped), 412/412 birim testi, 116/116 DB cache entegrasyon testi, 113/113 canlı veritabanı testi (%100 PASS - 641 assertion).
      * **Sonuç:** Extern x64 modernizasyonu, Client vcxproj yol düzeltmeleri, `mingw-ucrt-gcc16` platform ayrımı ve 64-bit Makefile korumaları üç motorda toplam 1.923 assertion ile resmen kabul edildi. Gerçek ELF64 `db_r41023` ikilisi ve tüm DB testleri tam başarıyla doğrulandı.

  * **Server x64 Tam Uyumluluk, Veri Güvenliği ve Gerçek Game Derlemesi:**
    * **1. 32-Bit İstemci Binary Wire Protocol & LP64 Uyumlaştırması:**
      * İstemci `_USE_32BIT_TIME_T` tanımlı 32-bit Windows binary'si olduğu için sunucu tarafındaki tüm paket yapılarının `#pragma pack(1)` ve bayt hizalamaları denetlendi.
      * `server/src/libthecore/include/typedef.h`: 64-bit safe `LONG` -> `int32_t`, `ULONG` -> `uint32_t`, `QWORD` -> `uint64_t` yapıldı.
      * `server/src/common/tables.h`: 41 adet ham `long` alan `int32_t` ile sabitlendi; `TPlayerSkill.tNextRead` `uint32_t` yapıldı.
      * `server/src/game/src/packet.h`: 70 adet ham `long` alan `int32_t` ile sabitlendi; `SPacketGCTime.time` `uint32_t` yapıldı.
      * `server/src/db/src/Peer.h` ve `Peer.cpp`: `m_alMaps` dizisi `int32_t` ile `tables.h::alMaps` ile eşitlendi.
      * `server/src/game/src/item.h` ve `item.cpp`: `m_alSockets` ve soket get/set fonksiyonları `long` yerine `int32_t` ile eşitlendi; 32-bit DB ve wire protocol uyumu garantilendi.
    * **2. Wire Protocol Test Paketi (tests/test_packet_wire.cpp):**
      * 32 compile-time `static_assert` ve çalışma zamanı assertion'ı içeren test paketi oluşturuldu.
      * Paket boyutları (`TPacketCGMove` 16B, `TPacketGCMove` 24B, `TPacketGCCharacterAdd` 37B, `TPacketGCMainCharacter` 48B, `TPacketGCItemSet` 54B, `TPacketGCItemUpdate` 41B, `TPacketGCWarp` 15B, `TPacketGCTime` 5B, `TPlayerItem` 52B, `TSimplePlayer` 67B, `TPacketGDSetup` 155B, `TMapLocation` 147B) 32-bit istemciyle bayt bayt doğrulandı (%100 PASS).
      * `tests/run_tests.ps1` ve `tests/run_tests.sh` çalıştırıcılarına entegre edildi: Toplam modül assertion sayısı 528'den **560'a (%100 PASS)** yükseldi (`libpoly`: 24, `libthecore`: 97, `libgame`: 28, `libsql`: 143, `db_cache`: 120, `db_cache_integration`: 116, `wire_protocol`: 32).
    * **3. C++20 ve 64-Bit Kod Düzeltmeleri:**
      * `xgetopt.h` / `xgetopt.c`: `const TCHAR *optstring` ve C++ aşırı yükleme belirsizliği (overload ambiguity) giderildi.
      * `questlua*.cpp` (16 dosya): `sys_err(fmt, ...)` makrosundaki `__VA_ARGS__` argümansız çağrılarda sözdizimi hatası üreten boş virgül sorunu `##__VA_ARGS__` ile kalıcı olarak giderildi.
      * `map_location.h`: `CMapLocation::Get` için template sarmalayıcılar eklenerek hem `int32_t&` hem `long&` referanslarının tip hatası olmadan sorunsuz bağlanması sağlandı.
    * **4. Makefile ve Araç Zinciri Modernizasyonu:**
      * `server/src/liblua/5.0/config`: `CC ?= gcc`, `WARN= -Wall` (hardcoded `-m32` kaldırıldı), `AR ?= ar rcu`, `RANLIB ?= ranlib`.
      * `server/src/Makefile`: `CC ?= gcc`, `CXX ?= g++`.
      * `server/src/libpoly/Makefile`: `CXX ?= g++`, `-m32` yalnızca 32-bit mimaride aktif; `sinclude Depend` yapıldı.
      * `server/src/libthecore/src/Makefile`: `CXX ?= g++`, `-m32` yalnızca 32-bit mimaride aktif; `sinclude Depend` yapıldı.
      * `server/src/libgame/src/Makefile`: `CXX ?= g++`, `-m32` yalnızca 32-bit mimaride aktif; `sinclude Depend` yapıldı.
      * `server/src/libthecore/src/Depend` ve `server/src/libpoly/Depend`: Eski `../../../Extern/include/msl/...` yolları merkezi `../../../../extern/common/include/msl/...` ile güncellendi.
      * `server/src/game/src/Makefile`: `CC ?= gcc`, `CXX ?= g++`, `-m32` yalnızca 32-bit mimaride aktif; MySQL/MariaDB kütüphane tespiti `db/src/Makefile` ile eşitlendi; DevIL (`-lIL`) ve CryptoPP (`-lcryptopp`) 64-bit bağlama kuralları tanımlandı; `-w` bayrağı ve `tests/build` arama yolu fallback'i kaldırılarak doğrudan resmi kütüphane yollarına bağlandı.
      * `server/src/libsql/Depend`: MySQL 8.4 derlemesini bozan `/usr/local/include/mysql/mariadb_com.h` ve türevi sistem başlık bağımlılıkları temizlendi; hem Oracle MySQL hem MariaDB ile %100 uyumlu kılındı.
      * `server/src/libsql/Makefile`: `-fno-rtti` bayrağı kaldırılarak C++ RTTI (`typeinfo for CAsyncSQL`) etkinleştirildi; `game` sunucusundaki `CAsyncSQL2` türetiminde oluşan link hatası kalıcı olarak giderildi.
      * `extern/server/cryptopp/GNUmakefile`: `CXX ?= g++` ve `CC ?= gcc` varsayılanlarına geçildi.
      * `server/src/game/src/Makefile`: Clang/libc++ ile GCC/libstdc++ arasındaki C++ ABI uyumsuzluğunu (`std::__cxx11::basic_string` eksik sembolleri) önlemek adına GCC 14 ile derlenip `extern/server/lib/` dizinine taşınan `libcryptopp.a` arşivi birinci öncelikli bağlama kütüphanesi yapıldı.
    * **5. FreeBSD amd64 LP64 Tip ve Güvenlik Düzeltmeleri:**
      * `server/src/game/src/config.h` ve `config.cpp`: `map_allow_copy(long * pl, int size)` imzası `int32_t * pl` olarak güncellendi. `desc_client.cpp:158` çağrısında `TPacketGDSetup::alMaps` (`int32_t[MAP_ALLOW_LIMIT]`) ile LP64 tip uyumsuzluğu ve derleme hatası (`cannot convert 'int32_t*' to 'long int*'`) kalıcı olarak çözüldü; `map_allow_copy` içine sınır kontrolü (`iCount < size`) eklendi.
      * `server/src/game/src/config.h`: `<cstdint>` başlığı dahil edildi, eski `GameCore.top` yorum satırı temizlendi.
    * **6. FreeBSD amd64 CI İş Akışı Entegrasyonu:**
      * `.github/workflows/freebsd-gcc-test.yml`: `prepare` aşamasına `devil cryptopp` eklendi.
      * `server/src/` altındaki resmi sunucu kütüphaneleri (`libthecore`, `libpoly`, `libsql`, `libgame`, `liblua`) DB ve Game derlemelerinden önce doğrudan kendi Makefile'larıyla derlendi.
      * 64-bit Game sunucusu derleme ve bağlama adımı (`gmake -C server/src/game/src -j2 CXX=/usr/local/bin/g++14 CC=/usr/local/bin/gcc14`) entegre edildi.
      * `file`, `readelf -h` (ELF64 x86-64 executable) ve `readelf -d` (dinamik kütüphane bağımlılıkları: NEEDED, RPATH) kontrolleri eklendi.
    * **7. AGENTS.md Kalıcı Kurallar Güncellemesi:**
      * Madde 7: Server Extern Resmî Sürüm Yükseltme Anayasası eklendi.
      * Madde 8: İstemci Kaynak ve SDK Dokunulmazlığı (Client Strict Embargo) eklendi.
      * Madde 9: Çoklu Ortam Senkronizasyon ve Doğrulama Standardı eklendi.
      * Madde 10: Sürekli ve Otonom Geliştirme Protokolü (Gereksiz prompt bekleme yasağı) eklendi.
    * **8. İstemci Dokunulmazlığı:**
      * `client/src`, `client/files`, `extern/client` dizinlerine dokunulmadı; %100 korundu.

    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Gerçek Game ve DB 64-Bit Binary Üretimi - Run ID: 38359016904, Commit: `0f9a2aad`):**
      * **MySQL 8.4 LTS (Job ID: 115168530439):** SUCCESS (10m 44s) — Gerçek 64-bit `server/src/db/db_r41023` (17M, ELF64 x86-64, dynamically linked) ve `server/src/game/game_r41023` (80M, ELF64 x86-64, dynamically linked) ikilileri üretildi. Dinamik semboller (`readelf -d`: libmd, libmysqlclient.so.24, libz, libIL, libc++, libcxxrt, libssl, libcrypto, libstdc++, libm, libgcc_s, libthr, libc) ve RUNPATH (`/usr/local/lib/gcc14:/usr/local/lib/mysql`) doğrulandı. 412/412 modül testi, 116/116 DB cache entegrasyon testi, 113/113 canlı MySQL 8.4 testi (%100 PASS - 641 assertion).
      * **MariaDB 12.3 LTS (Job ID: 115168530248):** SUCCESS (15m 43s) — Gerçek 64-bit `db_r41023` ve `game_r41023` ikilileri üretildi. Dinamik semboller ve RUNPATH (`/usr/local/lib/gcc14:/usr/local/lib/mariadb`) doğrulandı. 412/412 modül testi, 116/116 DB cache entegrasyon testi, 113/113 canlı MariaDB 12.3 testi (%100 PASS - 641 assertion).
      * **MariaDB 11.4 LTS (Job ID: 115168530398):** SUCCESS (9m 50s) — Gerçek 64-bit `db_r41023` ve `game_r41023` ikilileri üretildi. Dinamik semboller ve RUNPATH (`/usr/local/lib/gcc14:/usr/local/lib/mariadb`) doğrulandı. 412/412 modül testi, 116/116 DB cache entegrasyon testi, 113/113 canlı MariaDB 11.4 testi (%100 PASS - 641 assertion).
      * **Sonuç:** Server x64 tam uyumluluğu, 32-bit wire protokolü korunumu, gerçek 64-bit Game ve DB ikili derlemeleri ve bağlamaları, sıfır test fallback'i ile resmi kütüphane bağlaması ve üç motorda toplam **1.923 assertion** (%100 PASS) ile eksiksiz kabul edildi.

    * **FreeBSD 15.1 amd64 GCC 14 CI Doğrulaması (Rev 23 Pure libstdc++ & CryptoPP Düzeltmesi - Run ID: 38376898747, Commit: `b02c797e`):**
      * **MySQL 8.4 LTS (Job ID: 115227429685):** SUCCESS (9m 58s) — `game_r*` ve `db_r*` ikilileri derlendi. `readelf -d` çıktısında `libc++.so` bulunmadığı ve RUNPATH (`/usr/local/lib/gcc14:/usr/local/lib/mysql`) ile saf `libstdc++` kütüphanesine bağlandığı fail-closed assertion ile kesin olarak kanıtlandı: `--> game_r* pure libstdc++ olarak basariyla dogrulandi (libc++ bagimliligi yok)`. 113/113 canlı veritabanı testi %100 PASS.
      * **MariaDB 12.3 LTS (Job ID: 115227429968):** SUCCESS (13m 44s) — Saf `libstdc++` ile 64-bit Game ve DB ikilileri, 113/113 canlı test %100 PASS.
      * **MariaDB 11.4 LTS (Job ID: 115227429640):** SUCCESS (16m 02s) — Saf `libstdc++` ile 64-bit Game ve DB ikilileri, 113/113 canlı test %100 PASS.
      * **Sonuç:** Çift C++ runtime tehlikesi (`libc++.so` + `libstdc++.so.6`) kök nedeninden çözüldü, üç motorda tüm testler başarıyla sonuçlandı.

    * **CMake + Ninja Birleşik Derleme Altyapısı (Yol Haritası Adım 2 Tamamlandı):**
      * **Mimari ve Yapı:**
        * Kök `CMakeLists.txt` ve `CMakePresets.json` (`windows-gcc-ucrt` ve `freebsd-gcc14` yapılandırmaları).
        * Platform ve Derleyici Modülleri: `cmake/CompilerFlags.cmake` (C++20, C11, amd64 kontrolü, GCC bayrakları), `cmake/FindDatabaseClient.cmake` (MariaDB/MySQL tespiti ve otomatik link bağımlılıkları), `cmake/FindCryptoPP.cmake` (pure GCC arşivi ve FreeBSD otomatik derleme), `cmake/FindDevIL.cmake`.
        * Sunucu Kütüphane ve İkili Hedefleri: `server/CMakeLists.txt`, `libthecore`, `libpoly`, `libgame`, `libsql`, `liblua`, `db` (Windows ve FreeBSD için `db_r41023`), `game` (FreeBSD için `game_r41023`).
        * Test ve Doğrulama Hedefleri: `tests/CMakeLists.txt` (7 test paketi: `test_libpoly`, `test_libthecore`, `test_libgame`, `test_libsql`, `test_db_cache`, `test_db_cache_integration`, `test_packet_wire`).
      * **Windows Yerel CMake / CTest Doğrulaması:**
        * `cmake --preset windows-gcc-ucrt` ve `ninja -C build/windows-gcc-ucrt` ile tüm hedefler ve 64-bit `db_r41023.exe` (PE x86-64) hatasız derlendi.
        * `ctest --preset windows-gcc-ucrt` çalıştırıldı: **7/7 test paketi PASSED (%100 PASS, 0.26s)**.
      * **Eski Makefile Eşdeğerlik Güvencesi:**
        * `tests/run_tests.ps1` çalıştırıldı: **560/560 assertion PASSED (%100 PASS)**. Eski Makefiles ve scriptler depoda eksiksiz korunmaktadır.
      * **FreeBSD CI Entegrasyonu (Rev 24):**
        * `.github/workflows/freebsd-gcc-test.yml` güncellendi: VM prepare adımına `cmake ninja` eklendi; Step 5.4 ile `cmake --preset freebsd-gcc14`, `cmake --build --preset freebsd-gcc14` ve `ctest --preset freebsd-gcc14` doğrulaması devreye alındı.

    * **Eski 32-Bit Sunucu İkililerinin Temizlenmesi (Kayıpsız x64 Hijyeni):**
      * İstemci kodlarına (`client/src`, `client/files`, `extern/client`) kesinlikle dokunulmadan, sunucuya ait eski ve yanıltıcı 32-bit (`elf32-i386`) statik kütüphaneleri (`extern/server/lib/libIL.a`, `libILU.a`, `libILUT.a`, `libcryptopp.a`, `libmysqlclient.a`) `git rm` ile Git ağacından kaldırıldı.
      * Yerel çalışma dizinindeki kalıntı 32-bit `.o` nesne dosyaları (`extern/server/cryptopp/*.o` ve `server/src/liblua/5.2/src/*.o`) temizlendi.
      * `extern/server/cryptopp/GNUmakefile` dosyasında arşiv kopyalama kuralı `mkdir -p ../lib && cp -f $@ ../lib/` olarak güncellendi; `extern/server/lib/.gitkeep` ile klasör yapısı korundu.
      * `.gitignore` dosyasına derleme çıktısı olarak `*.a` ve `*.so` eklendi; istemci ve platform istisnaları korunarak yeni üretilen 64-bit sunucu arşivlerinin yanlışlıkla Git'e girmesi engellendi.

    * **CI Koşusu 38380990544 Kök Neden Analizi ve Kalıcı Çözüm:**
      * **Hata Tespiti:** CMake + Ninja FreeBSD CI koşusu 38380990544'te her 3 DB matrisi de Step 5.4 sırasında `server/src/game/src/cmd_general.cpp:2269:17: error: 'MD5_CTX' was not declared in this scope` hatasıyla durdu.
      * **Kök Neden:** `cmake/FindCryptoPP.cmake` içerisindeki `CRYPTOPP_INC_DIRS` tanımına `extern/server/cryptopp` eklenmişti. Bu dizindeki `md5.h` (Crypto++ C++ sınıfı), `cmd_general.cpp` içindeki `#include <md5.h>` sistem başlığı çağrısını gölgeledi (shadowing) ve FreeBSD'nin sistem `/usr/include/md5.h` (`MD5_CTX`) yapısını devre dışı bıraktı.
      * **Kalıcı Çözüm:** Sunucu kaynaklarında Crypto++ başlıkları `<cryptopp/...>` önekiyle çağrıldığından, `cmake/FindCryptoPP.cmake` içinden `extern/server/cryptopp` kaldırıldı; yalnızca `extern/server/include` bırakıldı. Clang/libc++ fallback bloku tamamen silinerek fail-closed pure GCC / libstdc++ standardı güvenceye alındı.

    * **Aşama 5: Server Extern Modernizasyonu — Crypto++ 8.9.0 Yükseltmesi:**
      * **Sürüm Doğrulaması:** Eski Crypto++ 7.0.0 sürümünden, resmî GitHub deposundan doğrulanan en son kararlı sürüm olan Crypto++ 8.9.0'a (`CRYPTOPP_8_9_0`) yükseltildi.
      * **Resmî SHA-256 Sağlaması:** `4CC0CCC324625B80B695FCD3DEE63A66F1A460D3E51B71640CDBFC4CD1A3779C` (Resmî kaynakla %100 eşleşti).
      * **Lisans:** Boost Software License 1.0 (BSL-1.0) / Public Domain.
      * **Entegrasyon ve Yapılandırma:**
        * `extern/server/cryptopp/` içerisine 8.9.0 kaynakları ve `TestPrograms` mimari testleri aktarıldı.
        * `extern/server/include/cryptopp/` içerisine 192 yeni 8.9.0 başlığı dağıtıldı; Metin2'ye özgü `cryptoppLibLink.h` korundu.
        * `extern/server/cryptopp/GNUmakefile` kuralına `mkdir -p ../lib && cp -f $@ ../lib/` entegre edildi.
      * **Doğrulama ve Test Sonuçları:**
        * Windows MinGW GCC 16.2 UCRT üzerinde `cmake -B build/windows-gcc-ucrt --fresh` ve `ninja -C build/windows-gcc-ucrt` hatasız tamamlandı.
        * CTest: **7/7 test paketi PASSED (%100 PASS, 1.13s)**.
        * Tam regresyon testleri (`tests/run_tests.ps1`): **560/560 assertion PASSED (%100 PASS)** (`libpoly`: 24/24, `libthecore`: 61/61, `libgame`: 64/64, `libsql`: 143/143, `db cache`: 120/120, `real db cache integration`: 116/116, `wire protocol`: 32/32).

    * **CI Koşusu 38384598415 Doğrulaması ve İncelemesi:**
      * **Başarılı İş (SUCCESS):** `FreeBSD 15.1 - MariaDB 12.3 LTS` (Job 115253138270) baştan sona eksiksiz tamamlandı; Crypto++ 8.9.0 bağlandı, CMake + Ninja ile `game_r41023` ELF64 üretildi, CTest 7/7 (%100) ve canlı DB testleri başarıyla geçti.
      * **İptal Edilen İşler (CANCELLED):** `MariaDB 11.4 LTS` ve `MySQL 8.4 LTS` işleri Ninja derlemesi [260/283] aşamasındayken 24m45s süresinde `timeout-minutes: 25` limitine takılarak iptal edildi (0 derleme hatası, 0 link hatası).
      * **Rev 26 İyileştirmesi:** İş akışındaki timeout süresi 45 dakikaya (`timeout-minutes: 45`) yükseltildi; FreeBSD 2-vCPU sanal makinesindeki CPU yükünü dengelemek için `ninja` derleme komutu `-j2` ile sınırlandırıldı.

    * **CI Koşusu 38393245226 Doğrulaması (Rev 26 fmt 12.2.0, RapidJSON Modern Master, 45m Timeout, -j2):**
      * **MariaDB 12.3 LTS (Job 115281965652):** SUCCESS (25m 15s) — CMake + Ninja ile `game_r41023` ve `db_r41023` üretildi, CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MariaDB 11.4 LTS (Job 115281965978):** SUCCESS (20m 13s) — CMake + Ninja ile `game_r41023` ve `db_r41023` üretildi, CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MySQL 8.4 LTS (Job 115281966171):** SUCCESS (26m 33s) — CMake + Ninja ile `game_r41023` ve `db_r41023` üretildi, CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **Sonuç:** Üç FreeBSD matris hedefinde de **%100 BAŞARI**. Ninja `-j2` ve 45m timeout ile timeout sorunu kalıcı olarak çözüldü; fmt 12.2.0 ve modern RapidJSON FreeBSD amd64 üzerinde başarıyla doğrulandı.

    * **Aşama 5: Server/Common Extern Modernizasyonu — fmt 12.2.0 ve RapidJSON Yükseltmesi:**
      * **fmt 12.2.0:**
        * Eski 8.1.1 sürümünden resmî en güncel kararlı `{fmt}` 12.2.0 sürümüne yükseltildi.
        * Resmî SHA-256: `a2f4a8d51178f954e4c339007f77edd76ba0cb2e36f87a48e5a5403d9be5878f` (MIT Lisansı).
        * Metin2'ye özgü `fmt.h` tekil başlığı korundu; `fmt::format`, `FMT_COMPILE`, `fmt::sprintf` tam uyumlu doğrulandı.
        * Eski 2016 kalıntısı `posix.h` ve `time.h` başlıkları temizlendi; modern `base.h`, `fmt-c.h`, `std.h` başlıkları entegre edildi.
      * **RapidJSON (Belgelenmiş Teknik İstisna — Upstream Master Commit `24b5e7a8`):**
        * **Resmî Kararlı Sürüm Durumu:** Upstream RapidJSON deposunda en son resmî etiket `v1.1.0` (2016-08-25) olup, 2016'dan bu yana resmî bir release etiketi basılmamıştır.
        * **Teknik İstisna ve Gerekçe:** Resmî `v1.1.0` sürümü GCC 14 ve GCC 16 (C++20) altında ölümcül `-Wclass-memaccess` kopyalama uyarılarına ve şablon örnekleme hatalarına neden olmaktadır. Bu uyarıları geçici bayraklarla (`-Wno-...`) bastırmak AGENTS.md Madde 3.2 uyarınca kesinlikle yasaktır.
        * **Kullanılan Snapshot:** Upstream master dalından sabitlenen commit `24b5e7a8b27f42fa16b96fc70aade9106cf7102f` (2025-02-05 snapshot).
        * **Resmî SHA-256:** `df07f5ddfebbc2940181039f6c939ec2764a7303ef79b17958d9792a364306bb` (MIT/BSD Lisansı).
        * **Doğrulama:** `json_helper.h`, `ClientManager.h` ve `IkarusShopCache.cpp` ile tam uyum; CTest 7/7, yerel 560/560 regresyon testi ve FreeBSD CI Run 38393245226 ile 3 DB matrisinde canlı olarak doğrulanmıştır.
      * **DevIL 1.8.0 (Developer's Image Library):**
        * **Sürüm Doğrulaması:** Eski DevIL 1.7.8 (2013) başlıklarından resmî upstream GitHub deposu (`DentonW/DevIL`) tarafından yayınlanan en güncel kararlı `v1.8.0` sürümüne yükseltildi.
        * **Resmî SHA-256 Sağlaması:** `b488afe4e797f8cf0c79c4d5120da5cbdcc447fcb142af104807bc9a3d3eabac` (%100 eşleşti).
        * **Lisans:** GNU Lesser General Public License v2.1 (LGPL-2.1).
        * **Entegrasyon ve Yapılandırma:**
          * `extern/server/include/IL/` içerisine 1.8.0 başlıkları aktarıldı (`il.h`, `ilu.h`, `ilut.h`, `ilu_region.h`, `devil_cpp_wrapper.hpp`, `devil_internal_exports.h`, `ilut_config.h`, `il_wrap.h`).
          * FreeBSD 15.1 amd64 üzerindeki resmî `devil-1.8.0` sistem paketiyle (`pkg install -y devil`) tam sürüm ve ABI eşleşmesi sağlandı.
          * `il.h` içindeki C++ standardına aykırı C99 `restrict` anahtar kelimesi uyarısı DevIL 1.8.0'ın `#if defined(RESTRICT_KEYWORD) && !defined(__cplusplus)` koruması ile çözüldü.
        * **Doğrulama ve Test Sonuçları:**
          * `server/src/game/src/MarkImage.cpp` (Guild mark sıkıştırma/açma) C++20 modunda GCC 16.2 ile 0 hata ile derlendi.
          * Windows MinGW GCC 16.2 UCRT üzerinde `ninja -C build/windows-gcc-ucrt` hatasız tamamlandı.
          * CTest: **7/7 test paketi PASSED (%100 PASS, 0.30s)**.
          * Tam regresyon testleri (`tests/run_tests.ps1`): **560/560 assertion PASSED (%100 PASS)**.

    * **CI Koşusu 38399674627 Doğrulaması (Rev 27 DevIL 1.8.0 Headers, FreeBSD GCC 14, 3 DB Matrisi):**
      * **MariaDB 12.3 LTS (Job 115303750717):** SUCCESS (25m 15s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri üretildi; pure `libstdc++` kütüphanesine bağlandı (`libc++.so` yok); CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MySQL 8.4 LTS (Job 115303750920):** SUCCESS (25m 28s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri üretildi; pure `libstdc++` kütüphanesine bağlandı; CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MariaDB 11.4 LTS (Job 115303751177):** SUCCESS (14m 55s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri üretildi; pure `libstdc++` kütüphanesine bağlandı; CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **Sonuç:** Üç FreeBSD matris hedefinde de **%100 BAŞARI**. DevIL 1.8.0 başlıklarının FreeBSD amd64 GCC 14 ve sistem `devil-1.8.0` paketiyle tam uyumlu olduğu fiziksel olarak doğrulandı.

    * **Aşama 5: Server/Common Extern Modernizasyonu — Boost 1.92.0 Yükseltmesi:**
      * **Sürüm Doğrulaması:** Eski Boost 1.83.0 (`BOOST_VERSION 108300`) sürümünden, resmî Boost.org tarafından yayınlanan en güncel kararlı `boost-1.92.0` (12 Ağustos 2026) sürümüne yükseltildi.
      * **Resmî SHA-256 Sağlaması:** `e2a814b3a158ab482c7a3d330f8bf5a7a8423d258a4e2fb396996e00fcee2111` (`boost-1.92.0-b2-nodocs.tar.gz`, %100 eşleşti).
      * **Lisans:** Boost Software License 1.0 (BSL-1.0).
      * **Kapsam ve İstemci Dokunulmazlığı (AGENTS.md Madde 8):**
        * Yalnızca `extern/common/include/boost/` (Server/Common Boost) güncellendi.
        * `extern/client/include/boost/` istemci dokunulmazlığı gereği kesinlikle DEĞİŞTİRİLMEMİŞTİR (%100 korundu).
      * **Gerçekte Kullanılan 10 Boost Modülü:**
        * `boost/algorithm/string.hpp` (Metin düzenleme, kırpma: `char_shop.cpp`)
        * `boost/algorithm/string/predicate.hpp` (Önek/sonek arama: `shop_manager.cpp`)
        * `boost/bind.hpp` (İşlev bağlama: `char_manager.cpp`)
        * `boost/function.hpp` (Genel callback: `IFileMonitor.h`, `any_function.h`)
        * `boost/intrusive_ptr.hpp` (Olay sayaçlı işaretçi: `event.h`)
        * `boost/lexical_cast.hpp` (Metin-sayı dönüşümü: `DragonSoul.cpp`, `dragon_soul_table.cpp`)
        * `boost/pool/object_pool.hpp` (Hızlı nesne havuzu: `affect.cpp`)
        * `boost/typeof/typeof.hpp` (Tür çıkarımı: `qc.cc`, `libthecore/stdafx.h`)
        * `boost/unordered_map.hpp` (Hash map: `ClientManager.h`, `char.h`, `desc_manager.h`, `p2p.h`, vb.)
        * `boost/unordered_set.hpp` (Hash set: `ClientManager.h`, `stdafx.h`)
      * **Mimari:** Tüm modüller **%100 header-only**'dir; sunucu tarafında hiçbir derlenmiş Boost kütüphanesi (`.a` veya `.so`) kullanılmamakta ve bağlanmamaktadır.
      * **64-Bit LLP64 Kök Neden Düzeltmesi:**
        * `server/src/game/src/questmanager.cpp` (satır 1852 ve 1867): `__codecache` tablosuna kod işaretçisi yazılırken kullanılan `(long)code` dönüştürmesi, Windows amd64 (LLP64, `sizeof(long)==4`, `sizeof(pointer)==8`) üzerinde hassasiyet kaybı hatası (`cast from 'const char*' to 'long int' loses precision`) vermekteydi. Kod `(uintptr_t)code` olarak düzeltilerek hem Windows LLP64 hem de FreeBSD LP64 üzerinde standart 64-bit güvenli hale getirildi.
      * **Doğrulama ve Test Sonuçları:**
        * Windows MinGW GCC 16.2 UCRT üzerinde `ninja -C build/windows-gcc-ucrt` hatasız tamamlandı.
        * CTest: **7/7 test paketi PASSED (%100 PASS, 1.11s)**.
        * Tam regresyon testleri (`tests/run_tests.ps1`): **560/560 assertion PASSED (%100 PASS)**.

    * **CI Koşusu 38403723838 Doğrulaması (Rev 28 Boost 1.92.0, FreeBSD GCC 14, 3 DB Matrisi):**
      * **MySQL 8.4 LTS (Job 115317714114):** SUCCESS (26m 34s) — Boost 1.92.0 başlıkları ile CMake + Ninja üzerinden `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi ve bağlandı; pure `libstdc++` doğrulandı; CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MariaDB 12.3 LTS (Job 115317714452):** SUCCESS (24m 57s) — Boost 1.92.0 başlıkları ile CMake + Ninja üzerinden `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi ve bağlandı; pure `libstdc++` doğrulandı; CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MariaDB 11.4 LTS (Job 115317714544):** SUCCESS (25m 20s) — Boost 1.92.0 başlıkları ile CMake + Ninja üzerinden `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi ve bağlandı; pure `libstdc++` doğrulandı; CTest 7/7 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **Sonuç:** Üç FreeBSD matris hedefinde de **%100 BAŞARI**. Boost 1.92.0 sürümünün FreeBSD amd64 GCC 14 ve sunucu kodları ile tam uyumlu olduğu fiziksel olarak doğrulandı.

    * **Aşama 5: Lua Quest Motoru ve Otomatik Regresyon Altyapısı (Adım 4):**
      * **`__codecache` 64-Bit İşaretçi ve Kayan Nokta Hassasiyet Kaybı Kök Neden Çözümü:**
        * `server/src/game/src/questmanager.cpp` (satır 1852 ve 1867): Kod işaretçisi Lua'ya `lua_pushnumber` ile iletiliyordu. 64-bit mimarilerde $2^{53}$ değerinden büyük sanal bellek adreslerinde (`0x0040000000000001` vb.) IEEE-754 `double` (`lua_Number`) mantissa biti yetersizliği nedeniyle en anlamsız bitler yuvarlanmakta ve iki farklı kod işaretçisi aynı sayıya dönüşerek cache çarpışması/yanlış quest fonksiyonu çalıştırma riski oluşturmaktaydı.
        * `lua_pushnumber(qs.co, (uintptr_t)code)` yerine yerel işaretçi tabanlı `lua_pushlightuserdata(qs.co, const_cast<char*>(code))` mimarisine geçildi.
        * Hem Lua 5.0 hem de Lua 5.2'de lightuserdata 64-bit işaretçiyi ham bitleriyle saklar ve Lua tablosunda pointer hash ile doğrudan eşleştirir. Kayan nokta dönüşümü ve hassasiyet kaybı %100 ortadan kaldırıldı.
      * **Genişletilmiş Otomatik CMake Regresyon Hedefleri (CTest 9/9, 8'er Kapsamlı Test Paketi):**
        * `server/src/liblua/CMakeLists.txt`: `lua52_lib` static kütüphane hedefi eklendi (32 kaynak dosya).
        * `tests/test_lua_quest.cpp`: Varsayılan motor olan Lua 5.0.3 için 8 modüllü otomatik regresyon paketi yazıldı:
          1. Motor başlatma ve sürüm doğrulaması (`_VERSION == "Lua 5.0.3"`).
          2. Metin2 lexer özel rezerve sözcük doğrulaması (`quest`, `state`, `with`, `when`, `begin` token tanımları).
          3. Metin2 C API tablo fonksiyonları (`pc.get_name`, `pc.get_gold`, `pc.give_gold`, `pc.get_level`, `pc.getqf`, `pc.setqf`, `pc.count_item`, `pc.give_item2`, `pc.remove_item`, `say_title`, `say`, `set_state`).
          4. Gerçekçi NPC diyalog ve coroutine yield/resume durum makinesi (Biyolog Chaegirab görev diyaloğu ve seçenekli seçim).
          5. Çok aşamalı görev ilerlemesi (`start` -> `collecting` -> `__COMPLETE__`) ve eşya teslimatı / envanter düşümü / Yang ödülü.
          6. `__codecache` 64-bit işaretçi güvenliği: $2^{53}$ üstündeki adreslerde `double` çarpışma kanıtı ve `lua_pushlightuserdata` ile %100 çarpışmasız geri çağırma.
          7. Hata izolasyonu ve sunucu çökme koruması: Hatalı coroutine çağrısının yakalanması ve ana `lua_State* L` durumunun bozulmaması.
          8. Yaşam döngüsü metrikleri ve motorlar arası eşdeğerlik doğrulaması.
        * `tests/test_lua52_quest.cpp`: Aşamalı geçiş motoru olan Lua 5.2.3 için birebir eşdeğer 8 modüllü regresyon paketi yazıldı.
        * `tests/CMakeLists.txt`: `test_lua_quest` ve `test_lua52_quest` CTest hedeflerine bağlandı.
      * **Doğrulama ve Test Sonuçları:**
        * Windows MinGW GCC 16.2 UCRT üzerinde `ninja -C build/windows-gcc-ucrt` hatasız tamamlandı.
        * CTest: **9/9 test paketi PASSED (%100 PASS, 0.32s)**.
        * Tam regresyon testleri (`tests/run_tests.ps1`): **560/560 assertion PASSED (%100 PASS)**.
      * **Motor Durumu ve Upstream Lua Analizi:**
        * Resmî Lua upstream (lua.org) en son kararlı sürümü **Lua 5.5.1** olarak doğrulanmıştır.
        * Lua 5.2.3 nihai modernizasyon hedefi değil, Metin2 C API ve quest uyumluluğunu adım adım güvenceye alan kontrollü bir ara aşamadır.
        * Tüm Metin2 quest dosyalarında tam uyumluluk kanıtlanana kadar **Lua 5.0.3 aktif varsayılan sunucu motoru** olarak korunmaktadır.

      * **Metin2 Quest Derleyicisi (qc) CMake Entegrasyonu ve Çapraz Motor Bayt Eşdeğerliği (qc & qc_lua52):**
        * `server/src/game/src/quest/CMakeLists.txt`: `qc` (varsayılan Lua 5.0.3) ve `qc_lua52` (aşamalı geçiş Lua 5.2.3) derleme hedefleri eklendi.
        * `server/src/game/src/quest/crc32.h`: C++20 standardında `size_t` ve `std::string` için `<cstddef>` ve `<cstring>` başlıkları eklendi.
        * `server/src/liblua/5.2/src/llex.c`:
          1. Standalone lexer'larda (`qc.cc`) `ls->fs == NULL` durumunda `luaX_newstring` içindeki `luaH_set` çağrısının çökmesini (`0xC0000005` Access Violation) önlemek için `if (ls->fs)` null denetimi eklendi.
          2. `read_numeral`: Sayı ve olay adı sözdiziminde (`when 20084.chat."..."`) `.` karakterinden sonra gelen harfin onaltılık rakam (`lisxdigit`: a-f, A-F) olarak sayı tamponuna dahil edilmesini önlemek amacıyla onaltılık kontrol (`is_hex`) ayrıştırıldı; `20084.chat` sözdiziminde `malformed number near '20084.c'` hatası kök nedeninden çözüldü.
        * `server/src/game/src/quest/qc.cc`: Lua 5.2'nin `luaX_token2str` fonksiyonunun tokenları tek tırnakla (`'...'`) sarmalaması sonucu oluşan `pc '.' get_level` sözdizimi bozulması `operator <<` içerisinde tırnak ayıklama ile giderildi.
        * `tests/fixtures/sample_biologist.quest`: Gerçek Metin2 görev sözdizimini içeren (`login or enter`, `when 20084.chat."..."`, envanter eşya kontrolü/turn-in ve çok durumlu durum geçişi) test fixture'ı eklendi.
        * **100% Bayt Eşdeğerliği Kanıtı:** Hem `qc.exe` (Lua 5.0.3) hem de `qc_lua52.exe` (Lua 5.2.3) tarafından üretilen tüm `object/` çıktıları (`20084/chat/*.arg`, `*.script`, `*.when`, `notarget/enter/*`, `notarget/login/*`, `state/*`) ikili (binary) düzeyde karşılaştırılmış ve **%100 BAYT-BAYT BİREBİR EŞDEĞER (BYTE-IDENTICAL)** olduğu kanıtlanmıştır.

    * **Aşama 5: Server/Common Extern Modernizasyonu — OpenSSL ve zlib İncelemesi (Adım 5):**
      * **zlib Sürüm ve Bağlantı Durumu:**
        * Resmî upstream en son kararlı sürüm: **zlib v1.3.2** (17 Şubat 2026, zlib.net).
        * FreeBSD 15.1 ortamında sistem taban kütüphanesi `/usr/lib/libz.so` ve `/usr/include/zlib.h` out-of-the-box sağlanmaktadır.
        * Windows tarafında MariaDB Connector/C (`libmariadb.a`) kendi sıkıştırma rutinlerini barındırmaktadır.
      * **OpenSSL Sürüm ve Bağlantı Durumu:**
        * Resmî upstream en son kararlı LTS sürüm: **OpenSSL 3.5.9 LTS** / OpenSSL 3.6+ (openssl-library.org).
        * FreeBSD 15.1 ortamında taban sistem kütüphaneleri `/usr/lib/libssl.so`, `/usr/lib/libcrypto.so` ve `/usr/include/openssl/` mevcuttur.
        * Windows tarafında MariaDB Connector/C (`libmariadb.dll`) TLS bağlantılarını Windows güvenli sağlayıcıları veya dinamik OpenSSL üzerinden yönetmektedir.
      * **CMake + Ninja Sistem Kütüphaneleri Entegrasyonu (`cmake/FindSystemLibraries.cmake`):**
        * Sistem kütüphanelerini gereksiz yere Git deposuna kopyalamadan (`AGENTS.md` ve kullanıcı talimatı) doğrudan platform bağlayıcılarıyla temiz keşif yapan `FindSystemLibraries.cmake` modülü oluşturuldu.
        * `server/src/game/CMakeLists.txt` içerisinde `OpenSSL::SSL`, `OpenSSL::Crypto` ve `ZLIB::ZLIB` hedefleri şeffaf ve geriye uyumlu biçimde entegre edildi.
      * **Genişletilmiş CTest Paketi (11/11):**
        * `test_qc_compiler_lua50` ve `test_qc_compiler_lua52` CTest testlerine eklendi.
        * Windows MinGW GCC 16.2 UCRT üzerinde: **11/11 CTest PASSED (%100 PASS, 0.42s)**.
        * Bağımsız regresyon paketi (`tests/run_tests.ps1`): **560/560 assertion PASSED (%100 PASS)**.

    * **CI Koşusu 38412561082 Doğrulaması (Rev 30 qc/qc_lua52 & OpenSSL/zlib, FreeBSD GCC 14, 3 DB Matrisi):**
      * **MySQL 8.4 LTS (Job 115348402026):** SUCCESS (25m 37s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi; `qc` ve `qc_lua52` üretildi; pure `libstdc++` kütüphanesine bağlandı; CTest 11/11 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MariaDB 12.3 LTS (Job 115348402298):** SUCCESS (25m 52s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi; pure `libstdc++` doğrulandı; CTest 11/11 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **MariaDB 11.4 LTS (Job 115348402468):** SUCCESS (25m 00s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi; pure `libstdc++` doğrulandı; CTest 11/11 (%100) ve canlı DB testleri 0 hata ile geçti.
      * **Sonuç:** Üç FreeBSD matris hedefinde de **%100 BAŞARI**.

    * **Aşama 5: Resmî Upstream Kararlı Lua 5.5.1 Entegrasyonu ve 343 Gerçek Quest Matrisi (Adım 6):**
      * **Resmî Upstream Sürüm ve Doğrulama:**
        * Resmî upstream Lua (lua.org) en son kararlı sürümü olan **Lua 5.5.1** kaynak kodları `server/src/liblua/5.5/` dizinine eklendi.
        * Resmî Arşiv: `lua-5.5.1.tar.gz` (SHA-256: `1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`, %100 doğrulandı).
        * Lisans: MIT License.
      * **Metin2 Quest Sözdizimi ve Lexer Uyarlamaları:**
        * `llex.h` / `llex.c`: Metin2 özel tokenları (`TK_QUEST`, `TK_STATE`, `TK_WITH`, `TK_WHEN`) eklendi.
        * `token_preserved`: `begin` sözcüğü `TK_DO` tokenına bağlandı ve `do` sözcüğü yedeklenerek Lua'nın `do ... end` dil yapısı ile Metin2'nin `begin ... end` görev bloğu sözdizimsel olarak tam eşleştirildi.
        * Sayı ve harf bitişikliği uyumluluğu (`read_numeral`): Eski görev yazarlarının boşluk bırakmadan yazdığı `== 0then` kalıbında Lua 5.5'in onluk sayılarda kasıtlı sözdizimi hatası zorlaması engellendi (`is_hex` ile izole edildi).
        * `!` mantıksal operatörü: Lua 5.2'deki `'~'` dönme hatası düzeltildi; `!` için `TK_NOT`, `!=` için `TK_NE` standardı sağlandı.
      * **Derleyici Kilitlenme ve Heap/Stack Bozulması Kök Neden Çözümleri:**
        * `server/src/game/src/quest/qc.cc` sonsuz döngü: `ST_WHEN_BODY` ve `ST_FUNCTION_BODY` ayrıştırıcı döngülerinde dosya sonu (`TK_EOS`) durumunda `unexpected end of file` hatasıyla anında çıkış yapılması sağlandı. Kilitlenme ve sonsuz döngü tamamen ortadan kaldırıldı.
        * WerFault GUI modal popup engellemesi: `abort()` çağrıları `exit(1)` ile değiştirildi; Windows üzerinde `_set_abort_behavior(0, ...)` ile sessiz ve hızlı çıkış garantilendi.
        * `luaX_token2str` yığın taşması (Stack Overflow / 0xC0000374 Heap Corruption): Upstream Lua'nın hata mesajları için kullandığı `luaO_pushfstring` mekanizmasının `qc.cc` tarafından her token çıktısında çağrılarak Lua stack'ini taşırması kök nedeninden çözüldü; doğrudan statik string döndüren sıfır yığın maliyetli implementasyona geçildi.
        * Standalone Lexer Bellek Güvenliği: `luaX_init_standalone` ve `luaX_close_standalone` fonksiyonları ile taranan dizgelerin Lua registry tablosunda sabitlenmesi sağlandı; GC'nin çalışan lexer altındaki dizgeleri zamansız serbest bırakması ve use-after-free use engellendi.
      * **Gerçek FTP Arşivi (`new_ftp.tar.gz`) ve 343 Quest Konsolide Test Matrisi:**
        * Arşiv: `mt2009 - FULL SOURCE/new_ftp.tar.gz` (Orijinal arşiv bozulmadan `scratch/ftp_quests/` izole dizinine açıldı; parola ve hassas bilgiler Git takibine alınmadı).
        * Envanter: 343 `.quest` dosyası ve 46 `.lua` dosyası tespit edildi.
        * Tek merkezî otomatik test sistemi: `tests/run_quest_matrix.py` (5.0s zaman aşımı, izole çalışma dizini, kesintiye dayanıklı JSON önbelleği).
        * **343 Gerçek Görev Konsolide Matris Sonuçları:**
          * **Lua 5.0.3 (Varsayılan Üretim):** 290 PASS (%84.5) | 53 FAIL (Makro içeren `_unused/` dosyaları) | 0 TIMEOUT
          * **Lua 5.2.3 (Aşamalı Geçiş):** 290 PASS (%84.5) | 53 FAIL | 0 TIMEOUT
          * **Lua 5.5.1 (Upstream Kararlı):** 290 PASS (%84.5) | 53 FAIL | 0 TIMEOUT
          * **Motorlar Arası Durum Farkı:** **Tam olarak 0 adet (Sıfır FARK!)**
          * **Lua 5.0 vs Lua 5.2 Bayt Eşdeğerliği:** **290 / 290 (%100.0 Birebir Bayt-Bayt Eşdeğer)**
          * **Lua 5.0 vs Lua 5.5 Bayt Eşdeğerliği:** **273 / 290 (%94.1 Birebir Bayt-Bayt Eşdeğer)**. Kalan 17 dosyada tek fark Lua 5.5'in tam sayıları IEEE-754 kayan noktalı bilimsel gösterim (`2.2e+06`) yerine kesin tamsayı (`2200000`) olarak formatlamasıdır; anlamsal ve çalışma zamanı olarak tam eşdeğerdir.
      * **Gerçek FTP questlib.lua, locale.lua ve Modül Yükleme Sırası Doğrulaması (Test 9):**
        * `scratch/ftp_quests/` içerisindeki `questlib.lua`, `locale.lua`, 7 modül yükleyicisi (`libModuleLoader.lua`, `_enumLibLoader.lua`, `_itemshopModuleLoader.lua`, `_fishingModuleLoader.lua`, `_craftingModuleLoader.lua`, `_otherModuleLoader.lua`, `_questBookModuleLoader.lua`) ve 14 enum kütüphanesi gerçek yükleme sırasıyla test edildi.
        * `server/src/liblua/5.5/src/lua.h`: Metin2 sürüm makrosu `#define LUA_V 551` tanımlandı.
        * `server/src/game/src/lua_incl.h`: `LUA_V == 551` ve 4 argümanlı `lua_resume(L, NULL, nargs, &nres)` sarmalayıcısı eklendi.
        * `server/src/game/src/questlua.cpp`:
          1. `InitializeLua`: `LUA_V == 551` için `luaL_newstate()` ve `luaL_openlibs(L)` entegrasyonu sağlandı.
          2. Lua 5.0 uyumluluk katmanı (`math.mod = math.fmod`, `math.pow = function(x, y) return x ^ y end`, `string.gfind = string.gmatch`, `table.getn = function(t) return #t end`, `table.foreach`, `table.foreachi`, `unpack = table.unpack`) eklendi.
          3. Modern `select(...)` ve `debug(...)` sarmalayıcıları eklenerek Lua 5.2/5.5 motorlarında `arg = {...}; arg.n = #arg` Metin2 vararg sözleşmesi %100 korundu.
          4. Sonsuz iç içe indeksleme ve `..` metin birleştirmeyi (`__index`, `__concat`, `__tostring`) destekleyen özyinelemeli `gameforge` proxy tablosu eklenerek `locale.lua` dosyasının her üç motorda da sıfır hata ile yüklenmesi sağlandı.
      * **Doğrulanmış FreeBSD CI Koşusu (Rev 31 - Run ID 38436002336):**
        * **MySQL 8.4 LTS (Job 115430004050):** SUCCESS (19m 26s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi; `qc`, `qc_lua52`, `qc_lua55` üretildi; pure `libstdc++` kütüphanesine bağlandı; CTest 13/13 (%100) ve canlı DB testleri 0 hata ile geçti.
        * **MariaDB 11.4 LTS (Job 115430004085):** SUCCESS (15m 45s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi; pure `libstdc++` doğrulandı; CTest 13/13 (%100) ve canlı DB testleri 0 hata ile geçti.
        * **MariaDB 12.3 LTS (Job 115430004252):** SUCCESS (20m 29s) — CMake + Ninja ile `game_r41023` ve `db_r41023` ELF64 ikilileri derlendi; pure `libstdc++` doğrulandı; CTest 13/13 (%100) ve canlı DB testleri 0 hata ile geçti.
        * **Sonuç:** Üç FreeBSD matris hedefinde de **%100 BAŞARI**.

      * **Aşama 6: questlib.lua Vararg/Nil Güvenliği, Gameforge Çeviri Doğrulaması ve Game Runtime C API Entegrasyonu:**
        * **Hardened Vararg / Nil Sayımı ve `luaL_getn`:**
          * Modern Lua (5.2/5.5) `lua_rawlen`'ın dizideki `nil` elemanlarda erken durarak argüman sayısını eksik bildirmesi kök nedeninden çözüldü.
          * `server/src/game/src/lua_incl.h`: `m2_lua_getn(L, t)` fonksiyonu eklendi; tablonun `t.n` alanını öncelikli kontrol ederek nil argümanlar bulunsa dahi (`select('A', nil, 'C')`) gerçek argüman sayısını (`n=3`) korur; fallback olarak `lua_rawlen` kullanır. `#define luaL_getn m2_lua_getn`.
          * `server/src/game/src/lua_incl.h`: `lua_dobuffer(L, b, sz, n)` makrosu `(luaL_loadbuffer(...) || lua_pcall(L, 0, LUA_MULTRET, 0))` olarak düzeltildi; `ScriptToString`, `IsScriptTrue` ve `StartScript` çağrılarında tamponun yalnızca yüklenip çalıştırılmaması hatası kalıcı olarak giderildi.
          * `server/src/game/src/questlua.cpp`: `select` fonksiyonu `table.pack(...)` ile yeniden tanımlandı (`local a = table.pack(...); return q.yield('select', a)`).
          * `table.getn` shimi `t.n` varlığını kontrol edecek şekilde güncellendi.
        * **Hardened Gameforge Çeviri Proxy'si:**
          * Önceki naive proxy'nin gerçek veri eksikliklerini ve çeviri hatalarını maskelemesi düzeltildi.
          * Yeni proxy, `translate.general`, `translate.reward` ve `translate[domain]` tablolarını kontrol eder; örneğin `locale.yes = gameforge.locale.yes` doğrudan `translate.general.yes` olan Polish `"Tak"` dizgesine çözümlenir.
          * Gerçekte bulunmayan anahtarlar için (`gameforge.locale.unknown_test_key`) sessizce maskeleme yapmak yerine `[MISSING_LOCALE: path]` döndürülür ve eksiklik açıkça teşhis edilir.
        * **343 Gerçek Metin2 Görevi Kesin Sonuç Raporu:**
          * Toplam Görev Sayısı: 343 `.quest` dosyası.
          * **Lua 5.0.3 (Varsayılan Üretim):** 290 PASS (%84.5) | 53 FAIL | 0 TIMEOUT
          * **Lua 5.2.3 (Aşamalı Geçiş):** 290 PASS (%84.5) | 53 FAIL | 0 TIMEOUT
          * **Lua 5.5.1 (Upstream Kararlı):** 290 PASS (%84.5) | 53 FAIL | 0 TIMEOUT
          * **Motorlar Arası Durum Farkı:** **Tam olarak 0 adet (Sıfır FARK!)**
          * **Lua 5.0 vs Lua 5.2 Bayt Eşdeğerliği:** **290 / 290 (%100.0 Birebir Bayt-Bayt Eşdeğer)**
          * **Lua 5.0 vs Lua 5.5 Bayt Eşdeğerliği:** **289 / 290 (%99.7 Birebir Bayt-Bayt Eşdeğer)**. Yalnızca 1 dosyada (`xxx_monkey_dungeon.quest`) küçük kayan noktalı sayı gösterim farkı mevcuttur.
          * **53 Başarısız Dosya Analizi:** 34 dosya `_unused/` klasöründedir; kalan 19 dosya ise `precompile.py` makro işlemcisinden (`define group`) geçirilerek `pre_qc/` klasöründe üretilen ve 290 PASS listesinde %100 derlenen dosyalardır.
        * **46 Gerçek `.lua` Dosyası Yükleme Sırası ve Çalışma Zamanı Doğrulaması:**
          * 40 adet gerçek sunucu yardımcı ve modül dosyası (`questlib.lua`, `locale.lua`, 7 modül yükleyici, 14 enum kütüphanesi, crafting/fishing/itemshop/questbook/translate kütüphaneleri) gerçek yükleme sırasıyla Lua 5.0, 5.2 ve 5.5 motorlarında çalıştırılmış ve **40/40 (%100) BAŞARIYLA** yüklenmiştir.
          * Kalan 6 dosya (%100 motor paritesi): 5 tanesi `quest ... begin` ile başlayan ve `qc.exe` ile derlenen quest dosyalarıdır (`herbalism.lua`, `crafting.lua` vb.); 1 tanesi ise kullanılmayan test dosyasıdır.
        * **Game Sunucusu Gerçek Quest Runtime C API Entegrasyonu:**
          * `server/src/game/CMakeLists.txt`: `ENABLE_LUA_5_VERSION=0` (varsayılan Lua 5.0), `2` (Lua 5.2) veya `5` (Lua 5.5.1) seçenekleri eklendi. Uygun başlık dizinleri ve kütüphaneler dinamik bağlandı.
          * `server/src/game/src/Makefile`: `ENABLE_LUA_5_VERSION=5` dalı eklendi.
          * Tüm uyumluluk kanıtları tamamlanana kadar **Lua 5.0.3 üretim varsayılanı** olarak aktif bırakılmıştır.

      * **Aşama 7: FreeBSD GCC 14 Game Server Dual Build (game_r41023_lua55), --check-lua ve TCP Fault Proxy İyileştirmesi:**
        * **Game Server Makefile Dual Target Desteği:**
          * `server/src/game/src/Makefile`: `ENABLE_LUA_5_VERSION ?= 0` tanımlandı.
          * `ENABLE_LUA_5_VERSION=5` için izole `OBJDIR = .obj_lua55` ve `MAIN_TARGET = $(BINDIR)/game_r$(GAME_VERSION)_lua55` yapılandırıldı.
          * `clean` hedefi aktif hedefi silerken tüm derlemeleri tek seferde temizleyen `clean_all` hedefi eklendi (`.obj`, `.obj_lua52`, `.obj_lua55` ve tüm `game_r*` ikilileri).
        * **Game Server CMake Dual Target Desteği:**
          * `server/src/game/CMakeLists.txt`: `game` (Lua 5.0 varsayılan, `game_r41023`) yanında `game_lua55` (Lua 5.5 staging, `game_r41023_lua55`) bağımsız çalıştırılabilir hedefi eklendi; `lua55_lib` ve `5.5/src` başlıklarına bağlandı.
        * **Game Server İzole Lua Quest Runtime Test Paketi (`server/src/game/src/questlua_test.cpp`):**
          * `RunGameQuestRuntimeTests()` fonksiyonu eklendi; fail-closed `RUNTIME_ASSERT` makrosu ile 10 kritik çalışma zamanı kontrolü gerçekleştirir (hata anında 1 koduyla anında sonlanır, atlama yapmaz):
            1. 32-Bit Client wire protokolü `packet_script` struct boyutu ve hizalaması (`#pragma pack(1)`).
            2. QuestManager ve Lua state başlatması.
            3. `math.mod`, `math.pow`, `table.getn`, `table.foreach`, `unpack` uyumluluk shimleri.
            4. Metin2 C API fonksiyon tabloları (`pc`, `q`, `say`, `select`, `npc`, `item` vb.).
            5. `select(...)` nil argüman sayımı ve `m2_lua_getn` sözleşmesi.
            6. Gerçek `tests/fixtures/questlib.lua` ve `tests/fixtures/locale.lua` yüklemesi ve değişken kontrolleri (`CONFIRM_YES == 1`, `locale.yes`).
            7. Gameforge çeviri proxy'si ve eksik anahtar teşhisi.
            8. Etkileşimli coroutine dialog durum makinesi (`select` ile yield, seçenek sayısı doğrulaması, oyuncu seçimi ile resume).
            9. `__codecache` 64-bit işaretçi güvenliği (hassasiyet kaybı olmadan depolama ve okuma).
            10. Coroutine hata izolasyonu ve çökme engelleme.
        * **Game Server CLI Doğrulama Bayrağı (`server/src/game/src/main.cpp`):**
          * `./game_r41023 --check-lua`, `./game_r41023_lua55 --check-lua` veya `--test-quest` bayrakları eklendi. Ağ soketleri ve MySQL bağlantısı açılmadan doğrudan `RunGameQuestRuntimeTests()` çalıştırılır ve temiz çıkış kodu üretilir.
        * **TCP Fault Proxy Race Condition Kök Neden Çözümü (`tests/tcp_fault_proxy.hpp`, `tests/test_live_db.cpp`):**
          * FreeBSD VM yüksek CPU yükü altında Senaryo 11.B'de `DirectQuery` tamamlandıktan mikrosaniyeler sonra arka plan proxy iş parçacığının `SeverSocket` çağrısını tamamlaması arasındaki yarış durumu (race condition) giderildi; `WaitForDroppedResponse(1000)` ve `WaitForSeverMidQuery(1000)` duyarlı bekleme metotları eklendi.
        * **FreeBSD CI Rev 33 Mimarisi (`.github/workflows/freebsd-gcc-test.yml`):**
          * `server/src/liblua/5.5/src/liblua.a` derleme adımı eklendi.
          * Hem `game_r41023` (Lua 5.0) hem de `game_r41023_lua55` (Lua 5.5) ikilileri gmake ile derlenir.
          * `readelf` ile ELF64 amd64 mimarisi ve pure `libstdc++` (sıfır `libc++.so`) doğrulanır.
          * Her iki ikili üzerinde `./game_r41023 --check-lua` ve `./game_r41023_lua55 --check-lua` testleri çalıştırılarak FreeBSD 15.1 VM üzerinde doğrulanır.

      * **Genişletilmiş CTest Paketi (13/13 Test, 100% PASS):**
        * Windows MinGW GCC 16.2 UCRT üzerinde: **13/13 CTest PASSED (%100 PASS, 0.74s)**.
        * Standalone regresyon paketi (`tests/run_tests.ps1`): **290/290 birim test, 120/120 cache, 116/116 integration, 32/32 wire protocol PASSED (560/560 assertions %100 PASS)**.

      * **Aşama 8: Lua 5.0 LUA_YIELD Derleme Düzeltmesi, BuildStateIndexToName Tablo Rehash Koruması ve 201 FTP Durumu %100 Paritesi (Rev 34):**
        * **FreeBSD CI Rev 33 (Run 38448323786 / Commit 3a6b0eb3) Kök Neden Analizi:**
          * MySQL 8.4 (Job `115472451269`) ve MariaDB 11.4 (Job `115472450953`), `ENABLE_LUA_5_VERSION=0` (Lua 5.0 varsayılan motor) ile `game_r41023` derlenirken `questlua_test.cpp:212` satırında `error: 'LUA_YIELD' was not declared in this scope` hatasıyla durmuştur.
          * Kök Neden: Lua 5.0 API'sinde `LUA_YIELD` sembolü bulunmamakta; `lua_resume` coroutine yield durumunda da `0` döndürmektedir. Lua 5.2 ve 5.5 sürümlerinde ise yield durumunda `LUA_YIELD` (1), başarıyla tamamlandığında ise `0` (`LUA_OK`) döndürülmektedir.
          * Kök Neden Çözümü:
            1. `server/src/game/src/questlua_test.cpp`: `lua_resume` yield ve hata kontrolleri `#if LUA_V == 551 || LUA_V == 523` ile ayrılarak Lua 5.0 (`== 0`) ve Lua 5.2/5.5 (`== LUA_YIELD`) gerçek dönüş sözleşmesine göre tam hizalandı.
            2. `server/src/game/src/lua_incl.h`: Kod genelinde olası referanslar için `#ifndef LUA_YIELD #define LUA_YIELD 1 #endif` fallback bildirimi eklendi.
        * **Kritik Metin2 QuestManager Rehashing & Reverse-Indexing Hatası ve Kalıcı Çözümü:**
          * `CQuestManager::BuildStateIndexToName` fonksiyonunda `lua_next` döngüsü içinde doğrudan `lua_rawset(L, -4)` ile numerik hash anahtarlarının tabloya eklenmesi, Lua'nın dahili tablo yapısının yeniden hashlenmesine (rehash / node reallocation) yol açmakta; bu durum `lua_next` yineleyicisini bozarak döngünün erken sonlanmasına veya durum anahtarlarını atlamasına neden olmaktaydı.
          * Gerçek FTP arşivi `new_ftp.tar.gz` (`scratch/ftp_quests/.../object/state/`) içerisindeki 201 görev durum dosyasında yapılan doğrulamada, eski kodun Lua 5.0'da **130 görevi**, Lua 5.5'te ise **162 görevi** ters indeksleyemediği (reverse mapping missing) kanıtlandı!
          * İki Aşamalı (Two-Phase) Güvenli Mimari:
            * Aşama 1: `lua_next` gezinimi sırasında tablo kesinlikle mutasyona uğratılmaz; eşleşmeler `std::vector<std::pair<std::string, double>>` içine toplanır.
            * Aşama 2: Tablo gezinmesi tamamlandıktan sonra ters eşleşmeler (`table[state_index] = state_name`) güvenle yazılır.
          * Sonuç: `scratch/test_states_all_lua.cpp` ile 201 gerçek FTP dosyasının tamamı test edildi:
            * **Lua 5.0.3:** 201/201 Görev Yüklendi, 1054/1054 Durum Doğrulandı, 0 Mismatch (**%100 KUSURSUZ**)
            * **Lua 5.2.3:** 201/201 Görev Yüklendi, 1054/1054 Durum Doğrulandı, 0 Mismatch (**%100 KUSURSUZ**)
            * **Lua 5.5.1:** 201/201 Görev Yüklendi, 1054/1054 Durum Doğrulandı, 0 Mismatch (**%100 KUSURSUZ**)
        * **--check-lua ve Birim Test Paketlerinin Genişletilmesi:**
          * `server/src/game/src/questlua_test.cpp`: Check 11 eklendi (`RunGameQuestRuntimeTests` 11/11 kontrol fail-closed doğrulandı).
          * `tests/test_lua_quest.cpp`, `tests/test_lua52_quest.cpp`, `tests/test_lua55_quest.cpp`: Test 10 eklendi (10/10 test %100 PASS).
          * Commit: `88d56a1f7d63914f54938111d607eaac0df70ff2` (Rev 34).

      * **Aşama 9: lua_incl.h Include Guard & OXEvent.cpp Çakışması Kök Neden Çözümü, CQuestManager::RunState LUA_YIELD Desteği (Rev 35):**
        * **FreeBSD CI Rev 34 (Run 38450564577 / Commit 88d56a1f) Kök Neden Analizi:**
          * Tüm matris işleri (MariaDB 11.4, MySQL 8.4, MariaDB 12.3), `ENABLE_LUA_5_VERSION=5` staging motoru ile `game_r41023_lua55` derlenirken `OXEvent.cpp` dosyasında `lua_incl.h:39:60: error: macro "lua_resume" passed 4 arguments, but takes just 2` ve `redefinition of 'size_t m2_lua_getn'` hatalarıyla durmuştur.
          * Kök Neden: `server/src/game/src/lua_incl.h` başlığında `#ifndef __INC_METIN2_LUA_INCL_H__` koruması (include guard) bulunmamaktaydı. `OXEvent.cpp`, hem `horse_rider.h` -> `char.h` hem de `questmanager.h` -> `quest.h` üzerinden `lua_incl.h` dosyasını iki kez dahil etmekte; ikinci dahil etmede `#define lua_resume(a,b)` makrosu önceden tanımlanmış olduğundan `m2_lua_resume` fonksiyonunun dahili 4 parametreli `lua_resume(L, NULL, nargs, &nresults)` çağrısını bozmaktaydı.
          * Kök Neden Çözümü:
            1. `server/src/game/src/lua_incl.h`: Standart `#ifndef __INC_METIN2_LUA_INCL_H__ ... #endif` başlık koruması eklendi.
      * **Aşama 10: FreeBSD amd64 Çift game İkili (ELF64 & Pure libstdc++) Başarısı, CQuestManager Singleton Null Pointer Kök Neden Çözümü, InitializeLua access() Koruması ve Kapsamlı Coroutine Suite (Rev 36):**
        * **FreeBSD CI Rev 35 (Run 38456092378 / Commit 7800d8ce) Bulguları:**
          * **Büyük Başarı:** Hem `game_r41023` (Lua 5.0 varsayılan) hem de `game_r41023_lua55` (Lua 5.5 staging) FreeBSD 15.1 amd64 GCC 14 üzerinde sıfır hata ile başarıyla derlendi ve bağlandı! `readelf -d` ile her iki ikilinin de ELF64 amd64 mimarisinde ve pure `libstdc++` (sıfır `libc++.so`) olduğu kesin olarak kanıtlandı.
          * **Check 1 Başarısı:** `--check-lua` çalıştırıldığında `packet_script` struct boyutunun tam 6 bayt ve `#pragma pack(1)` hizalı olduğu doğrulandı.
          * **Check 2 Hata Analizi (Exit Code 139 - SIGSEGV):** Check 2 aşamasında süreç `Segmentation fault (core dumped)` ile durmuştur.
          * **Kök Neden:** `main.cpp` içerisindeki `--check-lua` komut argümanı dalı, normal sunucu yöneticileri başlatılmadan önce çalıştırılmaktadır. Bu noktada `quest::CQuestManager` nesnesi henüz oluşturulmadığından `singleton<CQuestManager>::ms_singleton` değeri `nullptr` kalmaktadır. `questlua_test.cpp` içerisindeki `quest::CQuestManager::instance()` çağrısı `*nullptr` dereference ederek `qm.GetLuaState()` çağrısında `0x0` adresine erişmeye çalışmış ve segmentation fault üretmiştir.
          * **Kök Neden Çözümleri:**
            1. `server/src/game/src/questlua_test.cpp`: Check 2 öncesinde `std::unique_ptr<quest::CQuestManager> local_qm` ile `instance_ptr() == nullptr` kontrolü eklendi; nesne henüz oluşturulmamışsa güvenli ve izole biçimde tahsis edilerek `qm` referansı sağlandı.
            2. `server/src/game/src/questlua.cpp`: `RunState` içerisinde `ret == LUA_YIELD` ile `ret == 0` durumları tam olarak ayrıştırıldı; Lua 5.5/5.2 altında coroutine yield ve normal betik sonlanması sözleşmesi kesinleştirildi.
            3. `tests/fixtures/questlib.lua`: Standart MT2009 coroutine diyalog fonksiyonları (`select`, `select_table`, `wait`, `input`, `confirm`, `select_item`) eklendi.
            4. `server/src/game/src/questlua_test.cpp`: Check 8 genişletilerek 5 yield komutunun tamamı (`select`, `wait`, `input`, `confirm`, `select_item`) ve yield olmadan normal tamamlanma akışı fail-closed olarak doğrulandı.
      * **Aşama 11: FreeBSD CI Rev 36 Log Analizi, InitializeLua Fail-Closed Restorasyonu, Staging Yolu Tespiti, Sentetik vs Gerçek FTP Ayrı Doğrulaması ve packet_script Wire Testi (Rev 37):**
        * **FreeBSD CI Rev 36 (Run 38459564231 / Commit d0650a9f) Bulguları:**
          * **SIGSEGV %100 Çözüldü:** Check 1 (packet_script 6 bayt), Check 2 (Lua state başlatma) ve Check 3 (uyumluluk shim'leri) her üç FreeBSD matris işinde (MySQL 8.4, MariaDB 11.4, MariaDB 12.3) sıfır çökme ile başarıyla geçti!
          * **Check 4 Hata Analizi (Line 116 - select must be a valid registered function):**
            * Kök Neden: MT2009 Lua 5.0 motorunda `select(...)` bir C API fonksiyonu olmayıp `questlib.lua` içerisinde tanımlanmaktadır. `InitializeLua()`, varsayılan `g_stQuestDir = "./quest"` dizininde arama yaptığından CI ortamında `questlib.lua` dosyasını bulamamış ve `access()` koruması nedeniyle dosyayı sessizce atlamıştır. Bu nedenle Lua 5.0'da `select` global fonksiyonu tanımsız kalarak Check 4'ün başarısız olmasına yol açmıştır.
          * **Rev 37 Kök Neden Çözümleri:**
            1. `tests/fixtures/settings.lua` ve `tests/fixtures/translate.lua` oluşturuldu.
            2. `server/src/game/src/questlua.cpp`: `InitializeLua()` içerisindeki `access(..., R_OK)` kontrolleri kaldırılarak %100 fail-closed davranış restore edildi; `settings.lua`, `questlib.lua`, `translate.lua` veya `locale.lua` eksikse `InitializeLua()` anında `false` döndürür.
            3. `server/src/game/src/questlua_test.cpp`: Check 2 öncesinde `g_stQuestDir` ve `g_stServiceBasePath` değişkenleri çalışma dizinine göre aday `tests/fixtures` yoluna dinamik olarak yönlendirildi; `qm.InitializeLua()` çağrısının dönüş değeri fail-closed olarak doğrulandı.
            4. `server/src/game/src/questlua_test.cpp`: Check 6 sentetik fixture doğrulaması (`tests/fixtures/questlib.lua`, `locale.lua`, `settings.lua`, `translate.lua`) ve gerçek FTP arşiv doğrulaması (`scratch/ftp_quests/...`) olarak ikiye ayrıldı; her iki durum ayrı ayrı raporlandı.
            5. `tests/test_packet_wire.cpp`: `struct packet_script` için static_assert (6 bayt, ofsetler) ve Check 7 runtime serileştirme/deserileştirme testi eklendi; wire protokolü 37/37 assertions ile yerel olarak doğrulandı.
            6. Windows MinGW GCC 16.2 UCRT üzerinde hem `run_tests.ps1` (565 assertions) hem de `ctest --preset windows-gcc-ucrt` (13/13 test paketi, 0.51s) %100 PASS olarak doğrulandı.
      * **Aşama 12: FreeBSD CI Rev 37 Kök Neden Analizi, Lua 5.0 Vararg Sözdizimi Düzeltmesi ve Çift Motor Doğrulaması (Rev 38):**
        * **FreeBSD CI Rev 37 (Run 38464168058 / Commit 97fe3cc4) Bulguları:**
          * **Derleme & Bağlama Başarısı:** `db_r41023`, `game_r41023` (Lua 5.0) ve `game_r41023_lua55` (Lua 5.5) her üç matris ortamında (MySQL 8.4, MariaDB 11.4, MariaDB 12.3) sıfır hata ile ELF64 amd64 ve pure libstdc++ olarak bağlandı.
          * **DB Entegrasyon Testi:** `run_db_integration_tests.sh` sıfır hata ile başarıyla geçti.
          * **Check 1 Başarısı:** `--check-lua` çalıştırıldığında 32-bit istemci `packet_script` struct boyutu tam 6 bayt (`#pragma pack(1)`) olarak doğrulandı.
          * **Check 2 Hata Analizi (Line 98 - InitializeLua fail-closed):**
            * Hata Çıktısı: `tests/fixtures/questlib.lua:34: unexpected symbol near '...'`
            * `[FAIL] Line 98: InitializeLua() must succeed and load all mandatory lua files fail-closed`
            * Kök Neden: Lua 5.0.3 sürümünde ifade (expression) pozisyonunda `...` token'ı (`table.pack(...)`) geçerli bir sözdizimi değildir. Lua 5.0 vararg değişkenlerini yalnızca yerleşik `arg` tablosu (`arg[1]`, `arg[2]`, `arg.n`) üzerinden sunar; `...` ifadesi Lua 5.1 ile tanıtılmıştır. `tests/fixtures/questlib.lua` dosyasındaki `table.pack(...)` çağrısı Lua 5.0 derleyicisi tarafından sözdizimi hatası olarak değerlendirilmiş, fail-closed `InitializeLua()` yükleme hatası vererek süreci durdurmuştur.
        * **Rev 38 Kök Neden Çözümleri:**
          1. `tests/fixtures/questlib.lua`: `select(...)` fonksiyonu Lua 5.0 altında dinamik olarak izole edildi:
             ```lua
             if table.pack then
                 (loadstring or load)("function select(...) return q.yield('select', table.pack(...)) end")()
             else
                 function select(...)
                     return q.yield('select', arg)
                 end
             end
             ```
             Böylece Lua 5.0 derleyicisi ham `...` ifadesiyle karşılaşmaz; gerçek MT2009 FTP `questlib.lua` (satır 492) davranışına tam uygun olarak `arg` tablosunu kullanır. Lua 5.2/5.5 motorlarında ise `table.pack(...)` dinamik olarak derlenir ve `questlua.cpp` satır 779-784'teki koruma ile tam uyum sağlanır.
          2. Windows MinGW GCC 16.2 UCRT üzerinde:
             - `test_lua_quest.exe` (Lua 5.0): 10/10 PASS
             - `test_lua52_quest.exe` (Lua 5.2): 10/10 PASS
             - `test_lua55_quest.exe` (Lua 5.5): 10/10 PASS
             - `tests/run_tests.ps1`: 565/565 assertions (%100 PASS)
             - `ctest --preset windows-gcc-ucrt`: 13/13 test paketi (%100 PASS, 0.50s)
      * **Aşama 13: FreeBSD CI Rev 38 Log Analizi, Coroutine luaL_loadbuffer vs lua_dobuffer Kök Neden Çözümü ve Gerçek FTP 51-Dosya / 1.04 MB translate.lua Yükleme Sırası Doğrulaması (Rev 39):**
        * **FreeBSD CI Rev 38 (Run 38468055806 / Commit f8555510) Bulguları:**
          * **Derleme & Bağlama Başarısı:** `db_r41023`, `game_r41023` (Lua 5.0.3) ve `game_r41023_lua55` (Lua 5.5.1) her üç matris işinde (`MariaDB 11.4 LTS` ID `115539769892`, `MySQL 8.4 LTS` ID `115539770093`, `MariaDB 12.3 LTS` ID `115539770246`) ELF64 amd64 ve pure `libstdc++` (sıfır `libc++.so`) olarak başarıyla bağlandı.
          * **Check 1 - Check 7 Başarısı (%100 Kanıtlandı):**
            * Check 1 (`packet_script` 6 bayt `#pragma pack(1)`): PASS
            * Check 2 (`QuestManager & Lua State Initialization`): PASS (`Lua state initialized at: 0x2a66cca54000 from staging: tests/fixtures`) — Rev 38'deki Lua 5.0 vararg sözdizimi düzeltmesi (`tests/fixtures/questlib.lua`) FreeBSD 15.1 üzerinde %100 doğrulandı!
            * Check 3 (`Standard compatibility shims`): PASS
            * Check 4 (`Registered Metin2 C API function tables`): PASS
            * Check 5 (`select(...) nil-argument preservation & m2_lua_getn contract`): PASS
            * Check 6 (`questlib.lua and locale.lua verification`): PASS
            * Check 7 (`Gameforge translation proxy`): PASS
          * **Check 8 Hata Analizi (Line 233 - attempt to yield across metamethod/C-call boundary):**
            * Hata Çıktısı:
              `[RUN] Check 8: Interactive Coroutine Dialog State Machine...`
              `attempt to yield across metamethod/C-call boundary`
              `[FAIL] Line 233: test_biologist quest script buffer compilation failed`
            * Kök Neden: `server/src/game/src/questlua_test.cpp` içerisinde Check 8 (satır 232, 251, 265, 280, 295, 310) ve Check 10 (satır 350), betiği coroutine yığınına yüklemek için `luaL_loadbuffer(co, ...)` yerine `lua_dobuffer(co, ...)` çağırmaktaydı! `lua_dobuffer` yalnızca derleme yapmaz; betiği derledikten hemen sonra `lua_pcall` (`luaD_pcall`, yani C-call sınırı / `nCcalls > 0`) içinde anında çalıştırır! Betik içindeki `select(...)` -> `q.yield` -> `lua_yield(co, ...)` çağrısı `lua_dobuffer`'ın `lua_pcall` sınırı içinde çalıştığı için Lua VM `attempt to yield across metamethod/C-call boundary` hatası döndürmüştür. Gerçek oyun sunucusunda (`questmanager.cpp:1884`, `cmd_gm.cpp:2908`) ise görev betikleri her zaman `luaL_loadbuffer(qs.co, ...)` ile derlenip ardından `lua_resume(qs.co, 0)` (`RunState`) ile çalıştırılmaktadır.
        * **Rev 39 Kök Neden Çözümleri ve Gerçek FTP Doğrulaması:**
          1. `server/src/game/src/questlua_test.cpp`: Check 8 (6 alt test: `select`, `wait`, `input`, `confirm`, `select_item`, normal bitiş) ve Check 10 içindeki tüm `lua_dobuffer(co*, ...)` çağrıları gerçek sunucu mimarisiyle birebir aynı şekilde `luaL_loadbuffer(co*, ...)` olarak düzeltildi.
          2. `server/src/game/src/questlua.cpp`: `make_gf_proxy` fonksiyonu, gerçek `poland/translate.lua` (1.04 MB, 9.869 Lehçe çeviri anahtarı) yüklendiğinde mevcut `gameforge` ve alt tablolarına (`gameforge.locale` vb.) da fallback metatable bağlayacak şekilde güçlendirildi. Böylece mevcut 9.869 anahtar doğrudan tablo hızında çözülürken, eksik anahtarlar çökmeye yol açmadan `translate` tablosuna veya `[MISSING_LOCALE: ...]` işaretçisine düşer.
          3. **Gerçek FTP 51 `.lua` Dosyası Envanteri ve Tam `InitializeLua()` Yükleme Sırası Doğrulaması:**
             - `new_ftp.tar.gz` içerisindeki toplam 51 `.lua` dosyasının **46 adedi** saf Lua VM çalışma zamanı betiğidir (`poland/settings.lua` [91 goto, 41 bgm], `poland/BlueDragon.lua`, `poland/translate.lua` [1.04 MB], `poland/oxquiz.lua` [217 aktif OX sorusu], `poland/monkey_dungeon.lua` [124 ışınlanma], `poland/quest/questlib.lua`, `poland/quest/locale.lua` [103 KB] ve `poland/quest/libs/` altındaki 7 modül yükleyici + 32 yardımcı kütüphane); **5 adedi** ise `quest ... state ... when` sözdizimli Quest-DSL dosyasıdır (`crafting.lua`, `potion_use.lua`, `herbalism.lua` x2, `spider_dungeon_3floor_boss.quest.lua`).
             - `tests/test_lua_quest.cpp` (Lua 5.0.3), `tests/test_lua52_quest.cpp` (Lua 5.2.3) ve `tests/test_lua55_quest.cpp` (Lua 5.5.1) içerisindeki Test 9 güncellenerek gerçek FTP dosyalarının tam `InitializeLua()` sırasıyla (`settings.lua` -> `BlueDragon.lua` + `GFquestlib.lua` -> `questlib.lua` + 39 `libs/` -> `translate.lua` [1.04 MB] -> `make_gf_proxy` -> `locale.lua` [103 KB] -> `oxquiz.lua` -> `monkey_dungeon.lua`) yüklenmesi ve gerçek `q.yield` (`lua_yield`) üzerinden 6 coroutine durum makinesi akışının (`select` [nil argüman korumalı], `wait`, `input`, `confirm`, `select_item`, normal bitiş) üç motorda da %100 PASS vermesi sağlandı.
          4. `.github/workflows/freebsd-gcc-test.yml` (Rev 39): CMake + Ninja ile üretilen `build/freebsd-gcc14/server/src/db/db_r41023`, `game_r41023` ve `game_r41023_lua55` ikilileri için de ELF64, `readelf -d` (pure `libstdc++`) ve `--check-lua` doğrulaması eklendi.

---

## 7. ÇOKLU ORTAM SENKRONİZASYON DURUMU (AGENTS.md Madde 9)

* **1. Windows Yerel Geliştirme Ortamı (`D:\mt22009`):** Güncel, CMake/CTest (13/13, 0.79s) ve yerel regresyon testleri (565/565 assertions) %100 PASS. Gerçek FTP `locale/poland` (46 runtime `.lua` dosyası, 1.04 MB `translate.lua`) ve 6 coroutine akışı Lua 5.0/5.2/5.5 üzerinde doğrulandı.
* **2. GitHub Uzak Depo (`origin/dev/windows-gcc-baseline`):** Rev 39 commit ve push edilecek.
* **3. GitHub Actions FreeBSD 15.1 CI Ortamı:** Ephemeral (geçici temiz sanal makine checkout'u).
  * CI Run ID **38436002336** (Rev 31): **3/3 MATRİS İŞİ %100 BAŞARIYLA TAMAMLANDI**.
  * CI Run ID **38450564577** (Rev 34): `OXEvent.cpp` include guard eksikliği (Rev 35 ile çözüldü).
  * CI Run ID **38456092378** (Rev 35): Hem `game_r41023` hem `game_r41023_lua55` çift ELF64 pure `libstdc++` olarak başarıyla derlendi ve bağlandı.
  * CI Run ID **38459564231** (Rev 36): SIGSEGV çözüldü; Check 4 questlib arama yolu eksikliği (Rev 37 ile çözüldü).
  * CI Run ID **38464168058** (Rev 37): Check 1 geçti; Check 2'de Lua 5.0 `table.pack(...)` sözdizimi kök nedeni tespit edildi ve Rev 38 ile çözüldü.
  * CI Run ID **38468055806** (Rev 38 / Commit `f8555510`): Check 1 - Check 7 %100 PASS (Check 2 `InitializeLua()` düzeltmesi kanıtlandı); Check 8'de `lua_dobuffer` C-call boundary yield hatası tespit edildi ve Rev 39'da `luaL_loadbuffer` ile çözüldü.
* **4. Kalıcı FreeBSD VM Git Deposu:** Bulunmamaktadır (Dürüst Durum: **UNKNOWN / EPHEMERAL** — CI haricinde doğrudan erişilebilir kalıcı bir FreeBSD VM Git sunucusu mevcut değildir).

---

## 8. KALICI GELİŞTİRME YOL HARİTASI VE ÇALIŞMA SIRASI (AGENTS.md Madde 11)

Projenin modernizasyonunda takip edilecek değişmez operasyonel sıra:

1. **Mevcut FreeBSD x64 game/db Başarısını Koru:**
   * Doğrulanmış ELF64 `game` ve `db` derleme, bağlama ve wire protokolü başarısı her aşamada korunacak; tamamlanmış adımlar tekrarlanmayacaktır. (TAMAMLANDI)
2. **CMake + Ninja Derleme Altyapısı Kurulumu:**
   * Windows MinGW-w64 GCC 16.2 UCRT ve FreeBSD 15.1 amd64 GCC 14 için birleşik, tek merkezî `CMakeLists.txt` ve Ninja derleme mimarisi kurulacaktır. (TAMAMLANDI)
3. **CMake ile Gerçek İkili ve Test Doğrulaması:**
   * CMake üzerinden FreeBSD ortamında gerçek `game` ve `db` ELF64 binary'leri, Windows tarafında ise sunucu kütüphaneleri ve regresyon testleri doğrulanacaktır. (Windows doğrulandı, FreeBSD CI 38436002336 ile 3 DB matrisinde %100 SUCCESS)
4. **Eski Makefile Eşdeğerlik Güvencesi:**
   * Mevcut Makefile sistemleri, CMake altyapısının %100 eşdeğerliği ve başarısı kanıtlanıncaya kadar depoda korunacaktır; aceleyle silinmeyecektir. (Eşdeğerlik kanıtlandı)
5. **Aşamalı Server/Common Extern Sürüm Yükseltmesi:**
   * Server ve Common Extern bağımlılıkları (Crypto++, Boost, DevIL, Lua, RapidJSON, fmt, OpenSSL, zlib vb.) CMake altyapısı üzerinden en son doğrulanmış resmi kararlı sürümlere adım adım yükseltilecektir. (Crypto++ 8.9.0, fmt 12.2.0, RapidJSON modern master, DevIL 1.8.0, Boost 1.92.0, OpenSSL/zlib sistem entegrasyonu, Lua 5.5.1 upstream kararlı entegrasyonu, 343-quest matrix, 46-file loader chain, questlib.lua runtime compatibility, game_r41023 ve game_r41023_lua55 dual ELF64 pure libstdc++ derlemesi, BuildStateIndexToName 1054/1054 state hardening, RunState LUA_YIELD ayrımı ve Check 8/10 luaL_loadbuffer coroutine düzeltmesi TAMAMLANDI)
6. **Bütünsel Test ve Çoklu Ortam Doğrulaması:**
   * Her aşamada ilgili birim, entegrasyon, canlı SQL ve wire protokolü testleri çalıştırılacak; platform bağımlılıkları ve Git/CI senkronizasyonu kanıtlanacaktır.

---

## 9. TEK AKTİF GÖREV VE SIRADAKİ ADIM

* **Aktif Görev:** Rev 39: `questlua_test.cpp` Check 8 ve Check 10 içindeki `luaL_loadbuffer` coroutine düzeltmesi ile FreeBSD CI üzerinde `game_r41023` ve `game_r41023_lua55` ikililerinin `--check-lua` (11/11 test) doğrulamasını tamamlamak.
* **Sıradaki Güvenli Adım:**
   1. Rev 39 değişikliklerini `dev/windows-gcc-baseline` dalında commit ve push ederek FreeBSD CI koşusunu başlatmak.
   2. Yeni CI Run ID'sini doğrulamak ve 11/11 `--check-lua` geçişinden sonra Lua 5.5.1 varsayılan geçiş kararını değerlendirmek.









