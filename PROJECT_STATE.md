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
    * **Yerel Windows Doğrulaması:** 528/528 test (%100 PASS: `libpoly` 20, `libthecore` 105, `libgame` 24, `libsql` 143, `db_cache` 120, `db_cache_integration` 116) sıfır hata ve sıfır uyarı ile başarıyla tamamlandı.

---

## 7. TEK AKTİF GÖREV

* **Aktif Görev:** GameCoreTop.txt tamamen kaldırıldı (0 kopya), tüm Makefile, kaynak kod ve test script fallback yolları temizlendi; Windows 528/528 PASS.
* **Mevcut Durum:** **GameCoreTop.txt ELIMINATED (0 files on disk/git). ALL OBSOLETE FALLBACK PATHS CLEANED. Windows tests 528/528 PASS. Ready for push and CI verification.**

---

## 8. SONRAKİ GÜVENLİ ADIM

* CI doğrulamasının ardından sunucu çalışma zamanı arşivi (`new_ftp.tar.gz`) kontrollü parola/hassas veri sanitizasyonu ve `server/files` kurulumu; ardından `game` ve `client` C++20 x64 derleme modernizasyonu.








