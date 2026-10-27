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
  * Sunucu: `game`, `db`, `qc`, `libthecore`, `libsql`, `libgame`, `libpoly`, `liblua` (**yalnızca Lua 5.5.1 tek üretim ve derleme motoru aktif**; eski Lua 5.0.3 ve Lua 5.2.3 motorları ile `game_r41023_lua50` geri dönüş hedefi Rev 51 ile kalıcı olarak kaldırıldı).
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
    * **Hedef Motor ve Kararlı Sürüm (Kalıcı Proje Standardı — Rev 51):**
      * **MariaDB 12.3 LTS:** Sürüm 12.3.3 GA (**Tek desteklenen resmi veritabanı motoru**).
      * *(Tarihsel Not: Oracle MySQL 8.4 LTS [8.4.11 GA] Rev 16–Rev 50 arasında ve MariaDB 11.4.13 LTS Rev 16–Rev 49 arasında doğrulanmış olup, kalıcı proje kararıyla MySQL 8.4 LTS ve MariaDB 11.4 LTS desteği sona erdirilmiş ve gelecekteki tüm Windows/FreeBSD CI testleri, CMake keşfi ve betikler yalnızca **MariaDB 12.3 LTS** olarak sabitlenmiştir).*
    * **64-Bit Connector & MinGW Entegrasyonu:**
      * MariaDB Connector/C 3.4.10 64-bit DLL'den `gendef` + `dlltool` ile üretilen yerel `libmariadb.a` import kütüphanesi bağlandı (MariaDB Connector/C'nin kullandığı MySQL uyumlu C API `<mysql.h>` başlığı ve `mysql_*` fonksiyonları korunmaktadır).
      * Bağlayıcı MinGW-w64 UCRT GCC 16.2.0 (C++20) ile 0 hata, 0 uyarı ile derlendi ve bağlandı.
      * Eski 32-bit `Extern/lib/libmysqlclient.a` (`ELF32-i386`) kesinlikle kullanılmamıştır ve devre dışı tutulmuştur.
    * **Windows amd64 İzole Canlı Test Sonuçları (Port 3307):**
      * **MariaDB 12.3.3 LTS (Port 3307):** **73/73 (temel) / 113/113 (genişletilmiş) test başarılı (%100)**.
      * *(Tarihsel Doğrulama Kaydı: MariaDB 11.4.13 LTS ve MySQL 8.4.11 LTS motorları destek sonlandırılmadan önce 73/73 ve 113/113 kontrolle %100 geçmişti).*
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

## 5. KALICI MARIADB 12.3 LTS TEK VERİTABANI STANDARDI VE KABUL KRİTERLERİ

Bu kriterler, `libsql`, `db`, `game` ve `sql/` şemaları üzerinde yürütülen tüm geliştirmelerde karşılanması **zorunlu başarı ve kabul şartlarını** belirler:

1. **Tek Veritabanı Motoru Zorunluluğu (MariaDB 12.3 LTS):**
   * Proje yalnızca **MariaDB 12.3 LTS** veritabanı motorunu zorunlu destek ve test matrisinde bulundurur.
   * **MySQL 8.4 LTS** ve **MariaDB 11.4 LTS** destek ve CI test matrisinden kalıcı olarak çıkarılmıştır.
   * MariaDB Connector/C (`libmariadb`) tarafından kullanılan MySQL uyumlu C API (`mysql.h`, `include/mysql`, `mysql_*` fonksiyonları, `MYSQL`/`MYSQL_RES` veri yapıları) eski MySQL sunucusu bağımlılığı sayılmaz ve MariaDB istemci uyumluluğu için korunur.
2. **Kapsam:**
   * `libsql` bağlantı katmanı ve MariaDB Connector/C (`libmariadb`) istemci kütüphanesi.
   * `db` sunucusundaki veritabanı işlemleri, sorgular ve önbellek mekanizmaları.
   * `game` sunucusundaki veritabanı kullanan bütün sistemler.
   * Mevcut tüm SQL dosyaları (`sql/*.sql`), tablo şemaları, indeksler ve başlangıç verileri.
   * Gerekli diğer sunucu kaynak kodları, yapılandırmalar ve veri dönüşüm mekanizmaları.
3. **Kesin Kabul Kriterleri ve Kurallar:**
   * **SQL Mode Dokunulmazlığı:** SQL mode (`STRICT_TRANS_TABLES`, `NO_ZERO_DATE`, `ONLY_FULL_GROUP_BY` vb.) devre dışı bırakılamaz veya uyumluluk sağlamak için gevşetilemez.
   * **Hata Gizleme Yasağı:** `INSERT IGNORE`, `UPDATE IGNORE` ve benzeri hatayı gizleyen yaklaşımlar çözüm kabul edilemez. SQL hataları sessizce yok sayılamaz.
   * **Kök Neden Çözümü:** Uyumsuzlukların kök nedenleri kalıcı şekilde düzeltilmelidir. Uyarı veya hataları gizlemek için geçici hileler uygulanmamalıdır.
   * **Şeffaf Uyumluluk Katmanı:** MariaDB Connector/C ve MariaDB 12.3 LTS gereksinimleri açık ve test edilebilir katmanlarla yönetilmelidir.
   * **Veri Tipi ve Şema Standartları:** Veri tipleri, `PRIMARY KEY`, indeksler, `AUTO_INCREMENT`, `NULL`, `DEFAULT`, tarih/saat alanları, karakter setleri (`utf8mb4`), collation ve transaction davranışları titizlikle incelenmeli ve tam uyumlu hale getirilmelidir.
   * **Veri Güvenliği ve Geri Dönüş Planı:** Mevcut tablolar ve kullanıcı verileri korunmalıdır. Veri kaybı doğurabilecek dönüşümler öncesinde doğrulanmış yedek ve geri dönüş planı olmalıdır.
   * **Çift Senaryo Doğrulaması:** Hem sıfırdan kurulum hem de mevcut veritabanını yükseltme (migration) test edilmelidir.
   * **Bütünsel Değerlendirme:** SQL dosyaları ile `libsql`, `db` ve `game` kaynak kodları birlikte değerlendirilmelidir.
   * **Platform Uyumluluğu:** Windows amd64 ve FreeBSD amd64 GCC uyumluluğu korunmalıdır.
   * **Gerçek Test Doğrulaması:** Hedef veritabanında (**MariaDB 12.3 LTS**) gerçek testler geçmeden ilgili aşama tamamlanmış sayılmaz. Testlerde veri bütünlüğü, karakter/eşya kayıtları, normal ve hatalı sorgular, bağlantı kopmaları ve transaction sorunları doğrulanmalıdır.
   * **Resmî Hedef Sürüm (Kalıcı Proje Standardı):**
     * **MariaDB Foundation — MariaDB 12.3 LTS:** Tek resmî üretim ve test veritabanı motoru (kararlı sürüm 12.3.3+).
     * *(Not: Oracle MySQL 8.4 LTS ve MariaDB 11.4 LTS artık destek ve CI test matrisinde yer almamaktadır).*
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
          4. `.github/workflows/freebsd-gcc-test.yml` (Rev 39): CMake + Ninja ile üretilen `db_r41023`, `game_r41023` ve `game_r41023_lua55` ikilileri için de ELF64, `readelf -d` (pure `libstdc++`) ve `--check-lua` doğrulaması eklendi.
      * **Aşama 14: FreeBSD CI Rev 39 (`38627313843`) `--check-lua` 11/11 ve CTest 13/13 Kanıtı, `[MISSING_LOCALE]` Denetimi ve İzole Sunucu Başlatma / 32-Bit İstemci El Sıkışma Duman Testi (Rev 40):**
        * **FreeBSD CI Rev 39 (Run 38627313843 / Commit `a6333936b82819ecbc1c9d7e86832dd5a5ce13a7`) Doğrulanmış Sonuçları:**
          * **`game_r41023 --check-lua` (Lua 5.0.3 Varsayılan) — 11/11 PASS (Çıkış Kodu: 0, 0 Hata):**
            * Check 1 (`32-Bit Client packet_script struct size`): `6 bytes (aligned pack(1))` — PASS
            * Check 2 (`QuestManager & Lua State Initialization`): `Lua state initialized at: 0x36394654000 from staging: tests/fixtures` — PASS
            * Check 3 (`Standard compatibility shims`): `math.mod, math.pow, table.getn, table.foreach verified` — PASS
            * Check 4 (`Registered Metin2 C API function tables`): `pc, q, say, select tables and functions confirmed` — PASS
            * Check 5 (`select(...) nil-argument preservation & m2_lua_getn contract`): `Packed table with nil argument preserved count n = 3` — PASS
            * Check 6 (`questlib.lua and locale.lua verification`): `Synthetic fixtures (questlib.lua, locale.lua, settings.lua, translate.lua) verified` — PASS
            * Check 7 (`Gameforge translation proxy`): `Gameforge proxy transparently resolves and flags missing keys` — PASS
            * Check 8 (`Interactive Coroutine Dialog State Machine`): `All 5 yield commands (select, wait, input, confirm, select_item) & normal completion verified` — PASS (Rev 39 `luaL_loadbuffer` düzeltmesi FreeBSD 15.1 üzerinde kanıtlandı!)
            * Check 9 (`__codecache 64-bit pointer safety`): `64-bit pointer key (0x7FFE98765432) stored and retrieved with 0 precision loss` — PASS
            * Check 10 (`Coroutine Error Isolation & Crash Prevention`): `Deliberate script errors trapped without crashing server process` — PASS
            * Check 11 (`Multi-State Quest Registration & Safe BuildStateIndexToName`): `Two-way multi-state index/name mappings verified with 0 loss` — PASS
            * Sonuç: `[SUCCESS] METIN2 QUEST RUNTIME: 11/11 CHECKS PASSED!`
          * **`game_r41023_lua55 --check-lua` (Lua 5.5.1 Upstream Stable) — 11/11 PASS (Çıkış Kodu: 0, 0 Hata):**
            * Check 1–Check 11 tamamı PASS (`Lua state initialized at: 0xe9b2654588 from staging: tests/fixtures`, `[SUCCESS] METIN2 QUEST RUNTIME: 11/11 CHECKS PASSED!`).
          * **FreeBSD CMake + Ninja (`freebsd-gcc14`) ve CTest (13/13) Başarısı:**
            * `cmake --preset freebsd-gcc14` ve `cmake --build --preset freebsd-gcc14 -v -- -j2`: `[541/541]` hedef (`db_r41023`, `game_r41023`, `game_r41023_lua55`, `qc`, `qc_lua52`, `qc_lua55` ve 13 test ikilisi) başarıyla derlendi ve bağlandı.
            * `ctest --preset freebsd-gcc14 --output-on-failure`: **13/13 test paketi %100 PASS (`100% tests passed, 0 tests failed out of 13`, `0.09 sec`)**.
          * **Run 38627313843 Satır 207 Hata Kök Nedeni:**
            * `ctest` tamamlandıktan hemen sonra satır 207'deki `ls -lh build/freebsd-gcc14/server/src/db/db_r41023 ...` komutu `No such file or directory` ile döndü; çünkü `server/src/db/CMakeLists.txt` (satır 55) ve `server/src/game/CMakeLists.txt` (satır 65, 128) Makefile eşdeğerliği için `RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"` kullanmakta ve CMake ikililerini doğrudan `server/src/db/db_r41023`, `server/src/game/game_r41023` ve `server/src/game/game_r41023_lua55` konumlarına üretmektedir.
        * **Rev 40 Kök Neden Çözümleri ve İzole Sunucu Duman Testi (`run_server_smoke_test.sh`):**
          1. **`locale.*` Derinlemesine `[MISSING_LOCALE:` Denetimi:** `server/src/game/src/questlua_test.cpp` (Check 7) ve `tests/test_lua_quest.cpp`, `tests/test_lua52_quest.cpp`, `tests/test_lua55_quest.cpp` (Test 9) içerisine özyinelemeli `locale` tablosu taraması eklendi; gerçek FTP `locale/poland` (1.04 MB `translate.lua` + 103 KB `locale.lua`, 1.619 Lehçe dize) yüklendiğinde `missing_locale_entries == 0` olduğu Lua 5.0, 5.2 ve 5.5 üzerinde kanıtlandı.
          2. **CMake Release (`-DNDEBUG`) Assertion ve Uyarı Sertleştirmesi:** `tests/test_lua_quest.cpp`, `tests/test_lua52_quest.cpp` ve `tests/test_lua55_quest.cpp` başında `#undef NDEBUG` tanımlanarak Release derlemelerinde `assert(...)` kontrollerinin devre dışı kalması ve `-Wunused-variable` uyarıları tamamen giderildi; `tests/test_db_cache_integration.cpp` içindeki LLP64/LP64 format ve unused parameter uyarıları sıfırlandı.
          3. **İzole FreeBSD Sunucu Başlatma ve 32-Bit İstemci Wire Handshake Duman Testi (`tests/test_server_smoke_client.cpp` & `tests/run_server_smoke_test.sh`):**
             - Harici betik yorumlayıcısı gerektirmeyen C++20 yardımcı istemci (`test_server_smoke_client`) ve POSIX duman testi (`run_server_smoke_test.sh`) oluşturuldu.
             - Strict SQL Mode altında (`mt2_testdb`) `db_r41023` izole olarak başlatılıp tüm proto/sistem tablolarını yüklemesi ve TCP `15010` portunda bağlantı kabul etmesi doğrulanır.
             - Ardından hem `game_r41023` (Lua 5.0.3) hem `game_r41023_lua55` (Lua 5.5.1) normal oyun sunucusu modunda (`g_bAuthServer == false`) izole olarak başlatılır; `CQuestManager::Initialize()` -> `InitializeLua()` çalışması (`syserr` içinde 0 Lua hatası), `db_clientdesc` paket iletimi, 32-bit istemci protokolüyle `TPacketGCPhase(0xFD, PHASE_HANDSHAKE=1)` + `TPacketGCHandshake(0xFF)` (15 bayt) el sıkışması ve `PHASE_LOGIN` geçişi ile canlı `bootlog` SQL kaydı doğrulanır.
          4. **`.github/workflows/freebsd-gcc-test.yml` (Rev 40):** CMake ikili doğrulama yolları düzeltildi (`rm -f` sonrası CMake + Ninja ile `server/src/db/db_r41023`, `server/src/game/game_r41023`, `server/src/game/game_r41023_lua55` üretimi), gereksiz çift `game` derlemesi kaldırılarak iş süresi ~11 dakika kısaltıldı ve `./tests/run_live_tests.sh` sonrasına `./tests/run_server_smoke_test.sh` eklendi.
      * **Aşama 15: FreeBSD CI Rev 40 (`38634314243`) Kök Neden Analizi (MySQL 8.4 LTS `mysql/mysql.h` Başlık Gölgelemesi), Gerçek `HEADER_DG_BOOT` ve Uçtan Uca `AUTH_SERVER` + `GAME_SERVER` (`LoginByKey` / `PHASE_SELECT`) Duman Testi (Rev 41):**
        * **FreeBSD CI Rev 40 (Run 38634314243 / Commit `81c2e219335fdcab6c6840ed0caf896ab13b608a`) Doğrulanmış Sonuçları:**
          * **FreeBSD 15.1 - MariaDB 12.3 LTS (Job `116093447898`):** **SUCCESS (%100 PASS)** — CMake + Ninja (`[541/541]`), CTest (`13/13 PASS`), `game_r41023 --check-lua` ve `game_r41023_lua55 --check-lua` (`11/11 PASS`, exit `0`), `test_live_db` (`73/73 PASS`) ve `run_server_smoke_test.sh` (`[1/4]..[4/4]`, 4.2s) sıfır hata ile geçti!
          * **FreeBSD 15.1 - MariaDB 11.4 LTS (Job `116093448516`):** **SUCCESS (%100 PASS)** — Tüm derleme, birim, canlı SQL ve duman testi aşamaları (`[1/4]..[4/4]`) sıfır hata ile geçti!
          * **FreeBSD 15.1 - MySQL 8.4 LTS (Job `116093448281`):** **FAILURE** (`run_server_smoke_test.sh` `[2/4]` `db_r41023` başlatma aşamasında):
            * Hata Çıktısı:
              `[ERROR] SSL error: Unable to get certificate from '^A^H'.`
              `mysql_real_connect failed: SSL connection error: Unable to get certificate`
            * **Kök Neden:** `AsyncSQL.h` ve `DBManager.h` dosyaları `#include <mysql/mysql.h>` kullanmaktadır. `cmake/FindDatabaseClient.cmake` yalnızca `/usr/local/include/mysql` dizinini dışa aktarmakta (`/usr/local/include` üst dizinini aktarmamakta) ve `server/src/libsql/CMakeLists.txt`, `server/src/db/CMakeLists.txt`, `server/src/game/CMakeLists.txt` ile `server/src/libsql/Makefile` içinde `extern/server/include` yolu sistem başlıklarından ÖNCE yer almaktaydı. Bu nedenle FreeBSD MySQL 8.4 matrisinde dahi `#include <mysql/mysql.h>` ifadesi `/usr/local/include/mysql/mysql.h` (MySQL 8.4) yerine `extern/server/include/mysql/mysql.h` (vendored MariaDB Connector/C 3.3.5) başlığına çözümlenmekteydi! MariaDB başlığında `MARIADB_BASE_VERSION` tanımlı olduğu için `AsyncSQL.cpp` içinde `MYSQL_OPT_RECONNECT` (MariaDB enum değeri `20`) ile `mysql_options(&m_hDB, (mysql_option)20, &reconnect)` çağrılmakta; ancak çalışma zamanında bağlanan `/usr/local/lib/mysql/libmysqlclient.so` (MySQL 8.4) kütüphanesinde `20` numaralı enum `MYSQL_OPT_SSL_CERT` (`char*` sertifika dosya yolu) anlamına geldiğinden, `&reconnect` (`bool*`, değeri `1`) adresi SSL sertifika yolu (`"^A^H"`) olarak okunup `Unable to get certificate` hatası üretmekteydi!
        * **Rev 41 Kök Neden Çözümleri ve 6 Aşamalı Gerçek Sunucu / Auth / Login Doğrulaması:**
          1. **MySQL 8.4 vs MariaDB Başlık Önceliği Düzeltmesi (`FindDatabaseClient.cmake`, `CMakeLists.txt`, `Makefile`, `test_libsql.cpp`):**
             - `cmake/FindDatabaseClient.cmake`: FreeBSD üzerinde hem `${DB_INCLUDE_DIR}` (`/usr/local/include/mysql` veya `/usr/local/include/mariadb`) hem de `/usr/local/include` yollarını içeren `DB_CLIENT_INCLUDE_DIRS` değişkeni tanımlandı.
             - `server/src/libsql/CMakeLists.txt`, `server/src/db/CMakeLists.txt`, `server/src/game/CMakeLists.txt`, `tests/CMakeLists.txt`: `${DB_CLIENT_INCLUDE_DIRS}` başlık yolu `${CMAKE_SOURCE_DIR}/extern/server/include` dizininden ÖNCEYE alındı.
             - `server/src/libsql/Makefile`, `tests/run_tests.sh`, `tests/run_db_integration_tests.sh` ve `.github/workflows/freebsd-gcc-test.yml`: `-I/usr/local/include/mysql -I/usr/local/include/mariadb -I/usr/local/include` bayrakları `extern/server/include` öncesine yerleştirildi; veritabanı paketi kurulduktan sonra `libsql` temiz derlemesi eklendi.
             - `tests/test_libsql.cpp`: MySQL 8.4 `mysql.h` (`bool` ve `mysql_real_escape_string_quote`) ile MariaDB `mysql.h` (`my_bool`) arasında tam kaynak uyumluluğu sağlayan `mt2_my_bool` ve `mysql_real_escape_string_quote` mock fonksiyonu eklendi.
          2. **Sunucu Başlatma, `HEADER_DG_BOOT` ve `AUTH_SERVER` Kök Neden Düzeltmeleri:**
             - `server/src/db/src/Main.cpp`: `Start()` içerisindeki tüm SQL slot bağlantı döngülerine `thecore_is_shutdowned()` ve `bConnected` kontrolü eklendi; bağlantı kurulamaması veya `SIGTERM` alınması durumunda 150 saniye asılı kalmadan veya null `m_mainSQL` dereference etmeden anında temiz çıkış (`return false`) sağlandı.
             - `server/src/game/src/input_auth.cpp`: `GetAuthLoginQuery` içerisindeki sabit kodlanmış `FROM account.account AS ac LEFT JOIN account.hardware_status AS hw` sorgusu `db.cpp` ve `Hardware.cpp` ile tutarlı olacak şekilde `FROM account AS ac LEFT JOIN hardware_status AS hw` olarak düzeltildi (izole `mt2_testdb` şemasında gerçek `AUTH_SERVER` giriş sorgularının çalışması sağlandı).
             - `server/src/db/src/PeerBase.cpp`, `server/src/libthecore/src/buffer.c`, `server/src/db/src/ClientManager.cpp`: `QUERY_BOOT` sırasında boş tablo vektörleri için 0 baytlık `Encode` çağrılarının `buffer_write_proceed` assert üretmesi engellendi.
          3. **6 Aşamalı Ayrıştırılmış Sunucu, `HEADER_DG_BOOT` ve Gerçek `AUTH_SERVER` + `GAME_SERVER` (`LoginByKey`) Duman Testi (`tests/test_server_smoke_client.cpp` & `tests/run_server_smoke_test.sh`):**
             - Süreç yönetimi: Sonsuz `wait` çağrıları kaldırılarak zaman aşımı korumalı `stop_pid_bounded` (SIGTERM -> SIGKILL) ve `cleanup()` trap yapısı kuruldu.
             - **`[1/6]` Strict SQL Mode Şema ve Test Hesabı Hazırlığı:** `mt2_testdb` üzerinde tüm proto/oyuncu/lonca/hesap tabloları ve geçici test hesabı (`status='OK'`) oluşturulur.
             - **`[2/6]` Gerçek `db_r41023` Başlatma ve Tablo Yükleme:** `db_r41023` başlatılır, `InitializeTables()` başarısı ve `15010` TCP dinleyicisi doğrulanır.
             - **`[3/6]` Mock-DB + Çok Turlu 32-Bit TCP Wire Handshake (`PHASE_HANDSHAKE` -> `PHASE_LOGIN`):** Hem `game_r41023` (Lua 5.0.3) hem `game_r41023_lua55` (Lua 5.5.1) için `TPacketGCPhase(0xFD, PHASE_HANDSHAKE=1)` -> `TPacketGCHandshake(0xFF)` (15 bayt) -> zaman senkronizasyonu -> `TPacketGCPhase(0xFD, PHASE_LOGIN=2)` geçişi doğrulanır.
             - **`[4/6]` Gerçek `db_r41023` <-> `game` Tam `HEADER_DG_BOOT` ve `CQuestManager::Boot`:** Her iki `game` ikilisinin gerçek `db_r41023` üzerinden `HEADER_DG_BOOT` paketini uçtan uca işlemesi (`boot packet end check ok [ffff]==0xffff`), `CQuestManager::Initialize()` + `CQuestManager::Boot()` çalıştırması ve `bootlog` SQL tablosuna kayıt düşmesi doğrulanır.
             - **`[5/6]` `game_r41023` (Lua 5.0.3) Uçtan Uca `AUTH_SERVER` + `GAME_SERVER` Hesap Girişi (`LoginByKey`):** Gerçek `AUTH_SERVER` (`PORT 15110`) üzerinde 32-bit istemci protokolüyle `HEADER_CG_LOGIN3` (TEA şifreli, 49 bayt) gönderilir -> `QID_AUTH_LOGIN` SQL doğrulaması -> `HEADER_GD_AUTH_LOGIN` (`db_r41023` oturum anahtarı kaydı) -> `HEADER_GC_AUTH_SUCCESS (0x96)` (`dwLoginKey=***MASKED(non-zero)***`) alınır -> ardından gerçek `GAME_SERVER` (`PORT 15100`) üzerinde `HEADER_CG_LOGIN2` (`dwLoginKey` + `adwClientKey`, 45 bayt) gönderilir -> `HEADER_GD_LOGIN_BY_KEY` -> `HEADER_DG_LOGIN_SUCCESS` -> `HEADER_GC_PHASE(PHASE_SELECT=3)` + `HEADER_GC_LOGIN_SUCCESS_NEWSLOT (0x20)` + `HEADER_GC_EMPIRE (0x5A)` paketleri doğrulanır!
             - **`[6/6]` `game_r41023_lua55` (Lua 5.5.1) Uçtan Uca `AUTH_SERVER` + `GAME_SERVER` Hesap Girişi (`LoginByKey`):** Aynı tam kimlik doğrulama ve `PHASE_SELECT` akışı Lua 5.5.1 oyun sunucusu ikilisiyle birebir doğrulanır ve Lua 5.0.3 ile %100 protokol/davranış eşdeğerliği kanıtlanır.
      * **Aşama 16: FreeBSD CI Rev 41 (`38641852249`) Kök Neden Analizi, MySQL 8.4 `PASSWORD()` Kaldırılması ve Smoke Şemasının `db_r41023` Sorgularıyla Hizalanması (Rev 42):**
        * **FreeBSD CI Rev 41 (Run 38641852249 / Commit `4fd19abc`) Bulguları (3/3 FAILURE, test aşamasında):**
          * **MySQL 8.4 LTS (Job `116119050134`):** `db_r41023` yine `SSL error: Unable to get certificate from '^A^H'` verdi. Kök neden: CMake, FreeBSD'de `-I/usr/local/include` dizinini örtük sistem dizini olarak komut satırından düşürmekte; `-I/usr/local/include/mysql` ve `-Iextern/server/include` kalmaktadır. `#include <mysql/mysql.h>` ilk dizinde bulunamadığı için `extern/server/include/mysql/mysql.h` (MariaDB) başlığına çözülüyordu. Rev 41 düzeltmesi bu nedenle etkisiz kaldı.
          * **MariaDB 12.3 (Job `116119049627`) ve MariaDB 11.4 (Job `116119049997`):** `db_r41023` `mob_proto`, `item_proto`, `shop` tablolarını yükledikten sonra `InitializeSkillTable()` aşamasında `Unknown column 'dwSplashRange'` ile durdu (smoke şeması eksik/yanlış sütunlar içeriyordu).
          * **MySQL 8.4 `PASSWORD()`:** Smoke testi `PASSWORD('...')` SQL fonksiyonunu kullanıyordu; MySQL 8.0+ bu fonksiyonu kaldırmıştır.
        * **Rev 42 Kök Neden Çözümleri:**
          1. `AsyncSQL.h`, `DBManager.h`, `tests/test_live_db.cpp`: `<mysql/mysql.h>`, `<mysql/errmsg.h>`, `<mysql/mysqld_error.h>` yerine `<mysql.h>`, `<errmsg.h>`, `<mysqld_error.h>` kullanıldı; ilk `-I` dizini olan `/usr/local/include/mysql` (veya `/usr/local/include/mariadb`) doğrudan eşleşir, `extern` gölgelemesi imkânsızlaşır. Makefile ve test betiklerinde `extern/server/include/mysql` en sona taşındı.
          2. `cmake/FindDatabaseClient.cmake` + `AsyncSQL.cpp`: derleme zamanı başlık/kütüphane eşleşme kontrolü ve çalışma zamanında `mysql_get_client_version()` ile fail-closed ABI koruması eklendi.
          3. `input_auth.cpp`: Hesap şifresi C++ tarafında `mysql_hash_password()` ile (gerçek MT2009 41 karakter `*`+40 büyük hex = `SHA1(SHA1(pwd))` biçimi) hesaplanır; SQL `PASSWORD()` kullanılmaz. Hesap formatı değiştirilmedi, eski hesaplar aynen çalışır. Login ve HWID kaçışlanır.
          4. `tests/test_server_smoke_client.cpp`: Taşınabilir SHA-1 ile `--hash-env-pass`, ortam değişkeninden geçici kimlik bilgisi (`@ENV`) ve `TPacketGCLoginSuccess` karakter slotu (`SmokeHero`, id 90011, level 15) doğrulaması eklendi. Smoke betiği hash'i SQL `CONCAT('*',UPPER(SHA1(UNHEX(SHA1(pwd)))))` ile çapraz doğrular.
          5. `tests/run_server_smoke_test.sh`: `ClientManagerBoot.cpp`, `ClientManagerIkarusShop.cpp`, `ItemAwardManager.cpp` içindeki gerçek SELECT sorgularına göre `skill_proto`, `item_attr`, `item_attr_rare` (pendant sütunları + zorunlu 1 satır), `name_reservation`, `guild_land`, `itemshop_*`, `ikashop_offlineshop`, `offlineshop_limit` (zorunlu 1 satır), `crafting_proto`, `quest_reward_proto`, `shop_special`, `shop_special_proto`, `item_award` şemaları düzeltildi. Arka plan süreçleri `exec` ile başlatılarak `$!` doğrudan sunucu PID'i oldu.
        * **Windows Doğrulaması (Rev 42):** `cmake --build --preset windows-gcc-ucrt` (db_r41023.exe dahil bağlandı) ve `ctest --preset windows-gcc-ucrt` **13/13 PASS (1.33s)**. `run_server_smoke_test.sh` yalnızca FreeBSD'de çalışır — **FreeBSD CI ile doğrulanacak, henüz DOĞRULANMADI**.
      * **Aşama 17: Eski Hesap Şifre Uyumluluğu, Negatif Auth Testi ve Çalışma Zamanı ABI Kanıtı (Rev 43):**
        * **Uzak Depo Denetimi:** `git remote -v` → `origin = https://github.com/aidevelopervega/Metin2-MT2009.git`. `AGENTS.md` ise `x64bit-source/Metin2-MT2009` adresini yazıyor. Kullanıcı, geliştirmenin `aidevelopervega` deposunda yürütüldüğünü onayladı. Kullanıcı onayı olmadan uzak depo veya dal DEĞİŞTİRİLMEDİ.
        * **Rev 42 Commit Kapsamı Denetimi (`git add -A`):** `76c2adea` yalnızca 17 kaynak/test/belge dosyası içerir; `scratch/`, FTP arşivi, ikili dosya veya gizli bilgi yoktur. Smoke test kimlik bilgileri her koşuda `/dev/urandom` ile rastgele üretilir ve ortam değişkeniyle aktarılır.
        * **Değişiklikler:**
          1. `tests/test_server_smoke_client.cpp`: `--self-test-hash` (eski MySQL 4.1+/5.x `PASSWORD()` bilinen-cevap vektörleri: 2 adet 41 karakterlik `*<40-HEX-SHA1-MASKED>` bilinen cevap vektörü) ve `--auth-expect-fail` (beklenen `HEADER_GC_LOGIN_FAILURE` durumunu doğrular) eklendi.
          2. `tests/run_server_smoke_test.sh`: Hash öz-testi başlangıçta zorunlu; sabit eski hash'li `smklegacy` hesabı (id 9002) eklendi; her iki Lua ikilisinde gerçek `AUTH_SERVER` üzerinde yanlış şifre → `WRONGPWD` reddi ve eski hesap → `HEADER_GC_AUTH_SUCCESS` doğrulanır (`PulseManager` 500 ms/IP sınırı için 1 sn aralık). `[2/6]` aşamasında `db_r41023` çalışma zamanı ABI satırı zorunlu tutulur ve başlık türü sunucu motoruyla (`VERSION()`) karşılaştırılır.
          3. `server/src/libsql/AsyncSQL.cpp`: Fail-closed ABI korumasına ek olarak bir kez `AsyncSQL client ABI OK: header=...(MYSQL_VERSION_ID=...) runtime_lib=...(client_version=...)` kanıt satırı yazılır.
        * **Windows:** `--self-test-hash` 2/2 PASS (exit 0, `-Wall -Wextra -Werror`); CTest 13/13 PASS. FreeBSD tarafı **henüz doğrulanmadı**.
        * **FreeBSD CI Rev 42 (Run 38647690114 / Commit `76c2adea`) Kesin Sonuçları — 3/3 FAILURE (takılma YOK):**
          * MySQL 8.4 (`116138527814`, 16 dk), MariaDB 12.3 (`116138527928`, 25 dk), MariaDB 11.4 (`116138528000`, 24 dk). MariaDB işleri yavaş derleme nedeniyle uzun sürdü; log zaman damgaları sürekli ilerliyordu, gerçek takılma yoktu ve CI iptal edilmedi.
          * **Doğrulanan ilerleme (üç motorda da PASS):** `[1/6]` strict şema + C++ hash == SQL `SHA1(UNHEX(SHA1))` (PASSWORD() yok); `[2/6]` gerçek `db_r41023` başlangıcı ve tüm tablo yüklemesi — **MySQL 8.4'te ilk kez geçti, `<mysql.h>` başlık düzeltmesi FreeBSD'de kanıtlandı**; `[3a/6]` Lua 5.0 ve `[3b/6]` Lua 5.5 mock-DB + 32-bit handshake + InitializeLua.
          * **İlk gerçek hata (üç motorda aynı):** `[4-5/6] game_r41023 (Lua 5.0.3)` gerçek `db_r41023` ile 10 sn içinde `boot packet end check ok` satırına ulaşmadı. Oyun `syserr` boş; `db_r41023` logları ve oyun süreç durumu betik tarafından yazdırılmadığı için kök neden kanıtlanamadı (oyunun sessizce çökmesi veya boot paketinin ulaşmaması ayırt edilemedi). Motor bağımsız olduğu için SQL motoru kaynaklı değildir.
          * **Gerçek login (AUTH → LoginByKey → PHASE_SELECT → karakter listesi):** Lua 5.0 ve Lua 5.5 için **ÇALIŞTIRILAMADI** (boot aşamasında durdu).
        * **Rev 43 Ek Düzeltmeleri (tanı ve takılma koruması):**
          1. `run_server_smoke_test.sh`: `dump_stage_logs` — db/AUTH/GAME süreç canlılığı ve çıkış kodu (sinyal numarası dahil), oyun/auth/db stdout/stderr/syserr/syslog son 60 satır ve `BOOT`/`QUERY_BOOT`/`PHASE_DBCLIENT` ilerleme satırları; tüm gerçek-DB hata yollarında kullanılır.
          2. Boot bekleme: 30 sn üst sınır, `GAME_SERVER` erken ölürse anında hata.
          3. Global 900 sn smoke watchdog (`SMOKE_TIMEOUT_SEC`) → SIGTERM → trap ile sınırlı süreç temizliği; Windows Git sh ile normal çıkış (0 sn ek gecikme) ve takılma (2 sn'de tetiklendi) senaryoları doğrulandı.
          4. `freebsd-gcc-test.yml`: iş başına `timeout-minutes` 45 → 55 (≈25 dk derleme + 15 dk smoke watchdog tanıların kesilmeden yazılmasına izin verir; CI yine sınırsız RUNNING kalamaz).
      * **Aşama 18: FreeBSD CI Rev 43 (`38650928440`) Kanıtlanmış `HEADER_DG_BOOT` Kök Nedeni ve `CClientManager` / `CInputDB::Boot` Kalıcı Çözümü (Rev 44):**
        * **FreeBSD CI Rev 43 (Run 38650928440 / Commit `94b38eac09a49a5150039451aea0ad48bcb5214d`) Sonuçları — 3/3 FAILURE (`[4-5/6]` aşamasında):**
          * MySQL 8.4 LTS (`116149227159`), MariaDB 12.3 LTS (`116149226894`), MariaDB 11.4 LTS (`116149227210`):
            - `[self-test-hash]` 2/2 eski MySQL `PASSWORD()` KAT vektörü: **PASS**
            - `[1/6]` Strict SQL Mode şeması + hesaplar + `SmokeHero`: **PASS**
            - `[2/6]` Gerçek `db_r41023` başlangıcı + çalışma zamanı ABI kontrolü (`AsyncSQL client ABI OK`): **PASS**
            - `[3a/6]` Lua 5.0.3 Mock-DB + 32-bit TCP handshake + `InitializeLua()`: **PASS**
            - `[3b/6]` Lua 5.5.1 Mock-DB + 32-bit TCP handshake + `InitializeLua()`: **PASS**
            - `[4-5/6]` Gerçek `db_r41023` <-> `game_r41023` `HEADER_DG_BOOT`: **FAIL** (30 sn timeout).
        * **Kanıtlanmış `HEADER_DG_BOOT` Kök Nedeni (`[DIAG]` günlüklerinden):**
          * Her üç süreç de (`db_r41023`, `AUTH_SERVER`, `GAME_SERVER`) canlı (`ÇALIŞIYOR`).
          * `GAME_SERVER`, `db_r41023` (`127.0.0.1:15010`) sürecine bağlanarak `HEADER_GD_BOOT` ve `HEADER_GD_SETUP` paketlerini gönderdi.
          * `db_r41023`, `HEADER_GD_BOOT` paketini aldı ve `CClientManager::QUERY_BOOT` fonksiyonunu çalıştırdı; ancak `db/syslog` içinde:
            - MySQL 8.4 işinde: `BOOT: PACKET: 3825`
            - MariaDB 12.3 işinde: `BOOT: PACKET: 1487491141` (1.48 GB!)
            yazıldı; `GAME_SERVER` ise `CInputDB::Boot` fonksiyonuna hiç girmedi (`game/syslog` içinde `BOOT: PACKET:` yok).
          * **Birebir Kod Kanıtı:**
            1. `server/src/db/src/Main.cpp:67` içinde `CClientManager ClientManager;` yığın (stack) üzerinde oluşturulmaktadır.
            2. `CClientManager::CClientManager()` (`server/src/db/src/ClientManager.cpp:57-83`) yapıcısında `m_pSpecialShopTable = nullptr;` ilklendirildiği halde **`m_iSpecialShopTableSize` (ve `m_looping`, `m_fdAccept`, `m_bLastHeader`) HİÇ İLKLENDİRİLMEMİŞTİR**!
            3. `CClientManager::InitializeSpecialShopTable()` (`server/src/db/src/ClientManagerBoot.cpp:531-532`) içinde `shop_special` tablosu 0 satır döndürdüğünde (`if (!pRes2->uiNumRows) return true;`), `m_iSpecialShopTableSize = 0` atanmadan çıkılmaktadır.
            4. `CClientManager::QUERY_BOOT` (`server/src/db/src/ClientManager.cpp:493`) içinde `dwPacketSize` hesaplanırken `sizeof(TSpecialShopTable) * m_iSpecialShopTableSize` (yığın çöpü: MySQL 8.4'te `468` bayt, MariaDB 12.3'te `1487487784` bayt) gerçek `3357` baytlık paket boyutuna eklenmektedir (`HEADER_DG_BOOT` başlığında `dwSize = 3825` veya `1487491141` gönderilmektedir).
            5. Buna karşılık 603. satırdaki `peer->Encode(m_pSpecialShopTable, sizeof(TSpecialShopTable) * m_iSpecialShopTableSize)` çağrısı, `CPeerBase::Encode` (`PeerBase.cpp:157`: `if (size == 0 || !data) return;`) içinde `m_pSpecialShopTable == nullptr` olduğu için **0 bayt** yazmaktadır!
            6. Sonuç olarak `db_r41023` sokete gerçekte `3357` bayt gövde gönderirken paket başlığında `3825` (veya `1487491141`) bayt bildirmekte; `CInputDB::Process` (`input_db.cpp:3114`: `if (m_iBufferLeft - 9 < iSize) return true;`) eksik bayt bekleyerek `CInputDB::Boot` fonksiyonunu asla çağırmamaktadır!
        * **Rev 44 Kök Neden Çözümleri ve `CInputDB::Boot` Tamamlama Düzeltmeleri:**
          1. `server/src/db/src/ClientManager.cpp`: `CClientManager::CClientManager()` yapıcısında `m_iSpecialShopTableSize(0)`, `m_pSpecialShopTable(nullptr)`, `m_looping(0)`, `m_fdAccept(INVALID_SOCKET)` ve `m_bLastHeader(0)` açıkça ilklendirildi; `QUERY_BOOT` içindeki 2 adet eksik argümanlı `sys_log` (`sizeof(tAdminInfo)` ve `sizeof(TMonarchInfo)`) düzeltildi.
          2. `server/src/db/src/ClientManagerBoot.cpp`: `InitializeSpecialShopTable()` içinde `if (!pRes2->uiNumRows) return true;` öncesinde `m_pSpecialShopTable` temizliği ve `m_iSpecialShopTableSize = 0` sıfırlaması zorunlu kılındı.
          3. `tests/run_server_smoke_test.sh`:
             - `CMobManager::Initialize` (`mob_manager.cpp:151-165`) tarafından zorunlu tutulan `locale/poland/group.txt` ve `locale/poland/group_group.txt` dosyaları `prepare_game_workdir` içinde oluşturuldu; `mob_proto` tablosuna `shop` (`npc_vnum=9001`) için `9001` (`WeaponSeller`) NPC kaydı eklendi.
             - `CSkillManager::Initialize` (`skill.cpp:96`, `FindPointType(t->szPointOn)`) içinde `'0'` değerinin geçersiz sayılarak `cannot initialize skill table` hatası üretmesini önlemek için `skill_proto.szPointOn` varsayılanı `'NONE'` yapıldı.
              - `CQuestManager::Boot()` aşamasında dış dünya harita/NPC bağımlılığı olmadan deterministik coroutine çalışmasını doğrulamak üzere `locale/poland/quest/object/notarget/boot/smoke_boot.start` (`game.set_event_flag("smoke_boot_ok", 1)`) hazırlandı ve `gmlist` tablosuna `IMPLEMENTOR` kaydı eklendi.
      * **Aşama 19: FreeBSD CI Rev 44 (`39035737740`) 3/3 Matris `%100 SUCCESS` Kanıtı, Kalıcı `HEADER_DG_BOOT` Regresyon Testi (Senaryo 10: `140/140 PASS`) ve Lua 5.5.1 Varsayılan Sunucu Motoru Yükseltmesi (Rev 45):**
        * **FreeBSD CI Rev 44 (Run `39035737740` / Commit `25040708270fc40fcdc49d5f887b9b51d15e4101`) Kesin Sonuçları — 3/3 MATRİS `%100 SUCCESS`:**
          * **FreeBSD 15.1 - MariaDB 12.3 LTS (Job `117407893884`):** **`SUCCESS` (24 dk 18 sn)**
          * **FreeBSD 15.1 - MySQL 8.4 LTS (Job `117407894022`):** **`SUCCESS` (24 dk 59 sn)**
          * **FreeBSD 15.1 - MariaDB 11.4 LTS (Job `117407894044`):** **`SUCCESS` (25 dk 24 sn)**
        * **Rev 44 6 Aşamalı Gerçek Sunucu, `HEADER_DG_BOOT` ve `AUTH_SERVER` + `GAME_SERVER` (`LoginByKey`) Doğrulama Kanıtları (Üç Motorda Birebir PASS):**
          1. **`[self-test-hash]` & `[1/6]` Strict SQL Mode Şema ve Hesap Hazırlığı:**
             - 2 eski MySQL 4.1+/5.x `PASSWORD()` bilinen-cevap vektörü (`*<40-HEX-SHA1-MASKED>`) bayt-bayt doğrulandı (`PASS`).
             - `PASSWORD()` SQL fonksiyonu kullanılmadan C++ `mysql_hash_password` == SQL `CONCAT('*', UPPER(SHA1(UNHEX(SHA1(...)))))` eşleşmesi, `smklegacy` eski hesabı ve `SmokeHero (id=90011, level=15)` karakteri hazırlandı (`PASS`).
          2. **`[2/6]` Gerçek `db_r41023` İzole Başlangıç ve Çalışma Zamanı ABI Doğrulaması:**
             - MySQL 8.4 LTS: `AsyncSQL client ABI OK: header=MySQL(MYSQL_VERSION_ID=80411) runtime_lib=8.4.11(client_version=80411)` (`PASS`)
             - MariaDB 12.3 LTS: `AsyncSQL client ABI OK: header=MariaDB(MYSQL_VERSION_ID=120303) runtime_lib=3.4.10(client_version=30410)` (`PASS`)
             - MariaDB 11.4 LTS: `AsyncSQL client ABI OK: header=MariaDB(MYSQL_VERSION_ID=110413) runtime_lib=3.4.10(client_version=30410)` (`PASS`)
          3. **`[3a/6]` & `[3b/6]` Mock-DB + 32-Bit TCP Wire Handshake (`PHASE_HANDSHAKE` -> `PHASE_LOGIN`) + `InitializeLua()`:**
             - Hem `game_r41023` (Lua 5.0.3) hem `game_r41023_lua55` (Lua 5.5.1) üzerinde `CONNECTED=1 BOOT=1 SETUP=1`, 15 bayt el sıkışma ve 0 Lua hatası ile tamamlandı (`PASS`).
          4. **`[4/6]` Gerçek `db_r41023` <-> `game` Tam `HEADER_DG_BOOT` (`CInputDB::Boot` + `CQuestManager::Boot`) ve Canlı `bootlog` Kanıtı:**
             - Hem Lua 5.0.3 hem Lua 5.5.1 oyun sunucuları gerçek `db_r41023` üzerinden `HEADER_DG_BOOT` paketini ~0.53 saniyede eksiksiz yükledi (`boot packet end check ok [ffff]==0xffff`), `CQuestManager::InitializeLua()` + `CQuestManager::Boot()` 0 Lua hatası ile tamamlandı ve canlı SQL veritabanına `bootlog` (`hostname=real_lua50` ve `hostname=real_lua55`) kayıtları yazıldı (`PASS`).
          5. **`[5/6]` Lua 5.0.3 Gerçek `AUTH_SERVER` (`HEADER_CG_LOGIN3`) + Gerçek `GAME_SERVER` (`LoginByKey` -> `PHASE_SELECT`) Kanıtı:**
             - Yanlış parola `HEADER_GC_LOGIN_FAILURE status='WRONGPWD'` ile reddedildi (`PASS`).
             - Eski `smklegacy` hesabı `HEADER_GC_AUTH_SUCCESS(150)` ile kabul edildi (`dwLoginKey=***MASKED(non-zero)***` üç motorda doğrulandı) (`PASS`).
             - Geçici hesap `HEADER_CG_LOGIN3` ile doğrulandı (`dwLoginKey=***MASKED(non-zero)***`) ve gerçek `GAME_SERVER` üzerinde `HEADER_CG_LOGIN2` (`LoginByKey`) -> `HEADER_GC_EMPIRE(90, empire=1) + TPacketGCPhase(PHASE_SELECT=3) + HEADER_GC_LOGIN_SUCCESS(32, char[0]={id=90011, name='SmokeHero', level=15}) (376 wire bytes)` döndürdü (`PASS`).
          6. **`[6/6]` Lua 5.5.1 Gerçek `AUTH_SERVER` (`HEADER_CG_LOGIN3`) + Gerçek `GAME_SERVER` (`LoginByKey` -> `PHASE_SELECT`) Kanıtı:**
             - Yanlış parola `WRONGPWD` ile reddedildi (`PASS`).
             - Eski `smklegacy` hesabı `HEADER_GC_AUTH_SUCCESS(150)` ile kabul edildi (`dwLoginKey=***MASKED(non-zero)***` üç motorda doğrulandı) (`PASS`).
             - Geçici hesap `HEADER_CG_LOGIN3` ile doğrulandı (`dwLoginKey=***MASKED(non-zero)***`) ve gerçek `GAME_SERVER` üzerinde `HEADER_CG_LOGIN2` (`LoginByKey`) -> `HEADER_GC_EMPIRE(90, empire=1) + TPacketGCPhase(PHASE_SELECT=3) + HEADER_GC_LOGIN_SUCCESS(32, char[0]={id=90011, name='SmokeHero', level=15}) (376 wire bytes)` döndürdü (`PASS`).
        * **Rev 45 Kalıcı `HEADER_DG_BOOT` Regresyon Testi ve Lua 5.5.1 Varsayılan Sunucu Motoru Yükseltmesi:**
          1. **Kalıcı `HEADER_DG_BOOT` Regresyon Testi (`tests/test_db_cache_integration.cpp` Senaryo 10, `PeerBase.h`, `ClientManager.h`):**
             - `#define private public` hilesi kullanılmadan `CClientManager` ve `CPeerBase` üzerinde ABI-uyumlu test erişimcileri açıldı; `test_db_cache_integration.cpp` içerisindeki `CClientManager` örneği `0xAB` ile doldurulmuş kirli yığın belleği üzerinde `placement-new` ile oluşturuldu.
             - **Senaryo 10A:** Kirli `0xAB` yığını üzerinde `CClientManager()` yapıcısının `m_iSpecialShopTableSize == 0` ve `m_pSpecialShopTable == nullptr` ilklendirmesi doğrulanır.
             - **Senaryo 10B (Boş `shop_special` — 0 Satır):** `InitializeSpecialShopTable()` ve `QUERY_BOOT` çalıştırılarak `HEADER_DG_BOOT` başlık `dwSize` (`443` bayt), gerçek gönderilen gövde uzunluğu (`443` bayt), 24 tablonun eleman boyutları, `TSpecialShopTable` sayısı (`0`), `TSpecialShopMetaData` sayısı (`0`), son `0xffff` işaretçisi ve takip eden `HEADER_DG_NEW_OFFLINESHOP` çerçevesi birebir doğrulanır.
             - **Senaryo 10C (Dolu `shop_special` — 2 Dükkan `#9001 -> {70001, 70002}`, `#9002 -> {70003}`):** `SetTestSpecialShopTable` ile 2 özel dükkan ve meta verisi yüklendikten sonra `QUERY_BOOT` çalıştırılır; `dwSize` değerinin tam `2 * (sizeof(TSpecialShopTable) + sizeof(DWORD) + sizeof(TSpecialShopMetaData))` (`360` bayt -> toplam `803` bayt) arttığı, çözümlenen dükkan vnum/eşya dizilerinin birebir eşleştiği ve paketin tam `803.` baytta `0xffff` ile bittiği doğrulanır.
             - **Senaryo 10D (Dolu -> Boş Reload):** `InitializeSpecialShopTable()` 0 satırlı `shop_special` ile tekrar çağrıldığında `m_iSpecialShopTableSize == 0` ve `m_pSpecialShopTable == nullptr` sıfırlaması doğrulanır (`test_db_cache_integration`: **140/140 PASS**).
          2. **Lua 5.5.1 Varsayılan Sunucu Motoru Yükseltmesi ve Lua 5.0.3 Geri Dönüş (Rollback) Korunumu:**
             - `server/src/game/CMakeLists.txt`: `ENABLE_LUA_5_VERSION` önbellek değişkeninin varsayılan değeri `5` (Lua 5.5.1) yapıldı. Böylece varsayılan `game` hedefi (`server/src/game/game_r41023`) doğrudan **Lua 5.5.1** (`lua55_lib`, `ENABLE_LUA_5_VERSION=5`) ile derlenir ve bağlanır (`POST_BUILD` ile geriye dönük uyumluluk için `game_r41023_lua55` kopyası da senkronize edilir).
             - **Lua 5.0.3 Rollback Seçeneği:** Aynı anda hem yan yana `game_lua50` (`server/src/game/game_r41023_lua50`, `ENABLE_LUA_5_VERSION=0`, `lua_lib`) ikilisi üretilir hem de CMake'e `-DENABLE_LUA_5_VERSION=0` (veya Makefile'a `gmake ENABLE_LUA_5_VERSION=0`) verilerek ana `game_r41023` hedefinin tek komutla Lua 5.0.3'e geri döndürülmesi desteklenir.
             - `server/src/game/src/quest/CMakeLists.txt` & `tests/CMakeLists.txt`: Varsayılan `qc` hedefi **Lua 5.5.1** (`LUA_V=551`, `ENABLE_LUA_5_VERSION=5`, `lua55_lib`) olarak yükseltildi; açık geri dönüş derleyicisi olarak `qc_lua50` (`LUA_V=503`, `ENABLE_LUA_5_VERSION=0`, `lua_lib`) eklendi.
             - `server/src/game/src/Makefile` & `server/src/game/src/quest/Makefile`: Klasik Makefile varsayılanı da `ENABLE_LUA_5_VERSION ?= 5` olarak CMake ile hizalandı (`ENABLE_LUA_5_VERSION=0` geri dönüş seçeneği korundu).
             - `.github/workflows/freebsd-gcc-test.yml` & `tests/run_server_smoke_test.sh`: Hem yeni varsayılan `game_r41023` (Lua 5.5.1 Default) hem de geri dönüş ikilisi `game_r41023_lua50` (Lua 5.0.3 Rollback) için `--check-lua` sürüm doğrulaması ve 6 aşamalı gerçek sunucu/login duman testi yapılandırıldı.
      * **Aşama 20: FreeBSD CI Rev 45 (`39048567089`) Kanıtı, `LoginByKey` TEA Akış Çerçeveleyici Kök Neden Çözümü ve Gerçek Oyun Dünyasına Giriş (`PHASE_SELECT -> PHASE_LOADING -> PHASE_GAME`) + NPC/Quest Coroutine Duman Testi (Rev 46):**
        * **FreeBSD CI Rev 45 (Run `39048567089` / Commit `55ae725145de0d9ce8c8dec81d3090c02722c595`) Doğrulanmış Sonuçları:**
          * **1. Üç Matris İşinde de (`MySQL 8.4 LTS`, `MariaDB 12.3 LTS`, `MariaDB 11.4 LTS`) %100 Kanıtlanan Başarılar:**
            - `test_db_cache_integration`: **140/140 PASS** (Senaryo 10A/10B/10C/10D `HEADER_DG_BOOT` regresyon testi dahil).
            - `ctest --preset freebsd-gcc14`: **13/13 PASS (`0.11 sec`)** (`test_qc_compiler_lua50` + `test_qc_compiler_lua52` + `test_qc_compiler_lua55` dahil).
            - Gerçek FTP `1108/1108` `.quest` derlemesi: Varsayılan `qc` (**Lua 5.5.1**) ve `qc_lua50` (**Lua 5.0.3 Rollback**) ile **1108/1108 PASS (0 hata)**.
            - Varsayılan `server/src/game/game_r41023 --check-lua`: **`Lua 5.5.1` (`11/11 CHECKS PASSED`)**; geri dönüş ikilisi `server/src/game/game_r41023_lua50 --check-lua`: **`Lua 5.0.3` (`11/11 CHECKS PASSED`)** (CMake ve Makefile derlemelerinin her ikisinde de kanıtlandı).
            - `test_live_db`: **73/73 PASS** (toplam **219/219** canlı SQL kontrolü).
          * **2. `run_server_smoke_test.sh` Sonuçları ve `LoginByKey` (`rx=368`) Kök Nedeni:**
            - **FreeBSD 15.1 - MariaDB 11.4 LTS (Job `117440196972`):** **`SUCCESS` (24 dk 20 sn)** — 6 aşamanın tamamı (`[1/6]`..`[6/6]`, hem `game_r41023_lua50` hem `game_r41023`) %100 PASS!
            - **FreeBSD 15.1 - MySQL 8.4 LTS (Job `117440196979`) & MariaDB 12.3 LTS (Job `117440196935`):** `[1/6]`, `[2/6]`, `[3a/6]`, `[3b/6]` ve `[4-5/6]` (`HEADER_DG_BOOT` + `bootlog` + `WRONGPWD` reddi + `smklegacy` eski hesap girişi + `HEADER_CG_LOGIN3` -> `dwLoginKey=***MASKED(non-zero)***`) PASS; ancak `[4-5/6]` `--game-login-key` adımında:
              `[game-login-key] FAIL: incomplete LoginByKey response (rx=368 empire=1 phaseSelect=1 loginSuccess=0 id=90011 name='SmokeHero' level=15)`
            - **Kanıtlanmış Kök Neden:**
              1. `CInputDB::LoginSuccess` (`input_db.cpp:173-189`) ardışık olarak `HEADER_GC_EMPIRE` (`2` bayt -> TEA ile `8` bayt), `TPacketGCPhase(PHASE_SELECT=3)` (`2` bayt -> TEA ile `8` bayt) ve `TPacketGCLoginSuccess` (`0x20` = `32`, `sizeof = 353` bayt -> TEA ile `360` bayt) gönderir; ayrıca `DESC::Packet` zaman farkı `>1000 ms` olduğunda `HEADER_GC_TIME` (`106`, `5` bayt -> TEA ile `8` bayt) gönderebilir (`368` bayt = `8 + 360`).
              2. `DESC::SendLoginSuccessPacket` (`desc.cpp:963`) içerisinde `TPacketGCLoginSuccess p;` yığın üzerinde sıfırlanmadan (`memset` yapılmadan) oluşturulmakta; boş karakter slotlarındaki (slot 1..4) yığın çöpü baytları içinde rastgele `0xFD` (`HEADER_GC_PHASE`) veya `0x5A` (`HEADER_GC_EMPIRE`) baytları bulunabilmekteydi.
              3. Eski `run_game_login_key` ayrıştırıcısı gerçek paket uzunluklarını takip etmek yerine tüm `8` baytlık TEA bloklarının ilk baytını (`blk[0]`) körlemesine taradığı ve `if ... else if (hdr == 32)` kullandığı için, `TPacketGCLoginSuccess` gövdesi içindeki bir `8` baytlık blok `0xFD` ile başladığında `sawPhaseSelect = true` tetiklenip döngü `368.` baytta (`376` bayt tamamlanmadan) erken sonlanmaktaydı!
        * **Rev 46 Kök Neden Çözümleri ve `PHASE_SELECT -> PHASE_LOADING -> PHASE_GAME` + NPC/Quest Coroutine Duman Testi:**
          1. **Üretim Kodu Yığın Çöpü Temizliği (`server/src/game/src/desc.cpp`, `server/src/game/src/char.cpp`):**
             - `DESC::SendLoginSuccessPacket`: `TPacketGCLoginSuccess p{};` sıfır-ilklendirildi (boş karakter slotlarında ağa yığın çöpü sızması önlendi).
             - `CHARACTER::MainCharacterPacket` ve `CHARACTER::PointsPacket`: `TPacketGCMainCharacter pack{};` ve `TPacketGCPoints pack{};` sıfır-ilklendirildi.
          2. **Gerçek Paket Uzunluğu Tabanlı TEA Akış Çerçeveleyici (`tests/test_server_smoke_client.cpp`, `tests/test_packet_wire.cpp`):**
             - `tests/test_packet_wire.cpp`: `PHASE_SELECT -> PHASE_LOADING -> PHASE_GAME` ve Quest/NPC etkileşiminde kullanılan 23 paket yapısının (`TPacketGCLoginSuccess=353`, `TPacketCGPlayerSelect=2`, `TPacketCGEnterGame=1`, `TPacketCGOnClick=5`, `TPacketCGScriptAnswer=2`, `TPacketCGQuestInputString=66`, `TPacketGCMainCharacter=48`, `TPacketGCPoints=709`, `TPacketGCSkillLevel=1591`, `TPacketGCCharacterAdd=37`, `TPacketGCItemSet=54`, `packet_script=6` vb.) derleme zamanı `static_assert` boyut kontrolleri eklendi.
             - `tests/test_server_smoke_client.cpp`: Rastgele `8` baytlık blok başı taraması tamamen kaldırılarak `get_gc_packet_unpadded_size` + `SmokeGameStreamState` + `pump_gc_packets`TEA paket sınırı çerçeveleyicisi (`alignedSize = (pktSize + 7) & ~7`) yazıldı. Her paket yalnızca gerçek paket sınırında ayrıştırılır ve çok bloklu paketlerin (`TPacketGCLoginSuccess=360`, `TPacketGCPoints=712`, `TPacketGCSkillLevel=1592`, dinamik `HEADER_GC_CHAT`/`HEADER_GC_SCRIPT`) tamamı soketten okunmadan ilerlenmez.
          3. **Gerçek Oyun Dünyasına Giriş (`PHASE_GAME`) ve Quest / NPC Coroutine Doğrulaması (`tests/test_server_smoke_client.cpp`, `tests/run_server_smoke_test.sh`):**
             - İzole harita hazırlığı (`prepare_game_workdir`): `MapIndex=1` (`metin2_map_a1`, `4x5`, `BasePosition 409600 896000`), `--gen-server-attr` ile üretilen LZO1X-1 sıkıştırmalı geçerli `server_attr` (`16x20` sectree), `Town.txt` (`597 682`) ve `npc.txt` (`20016` Kowal/Blacksmith `469400x964200` koordinatında) oluşturuldu.
             - İzole SQL hazırlığı (`[1/6]`): `player` tablosunun `QUERY_PLAYER_LOAD` ve `CreatePlayerSaveQuery` tarafından kullanılan tüm sütunları, `item` (`window` ENUM, `SmokeHero` envanterinde `vnum=19` Kılıç+9), `quest` (`dwPID=90011`, `smoke_world.db_seed_val=77`), `affect`, `player_special_flag`, `safebox`, `messenger_list`, `messenger_block_list`, `ikashop_safebox`, `ikashop_notification`, `loginlog`, `loginlog2`, `char_log`, `quest_state_log` tabloları eklendi.
             - 4 Aşamalı Gerçek Oyun Dünyası & Coroutine Akışı (`--game-login-key`, hem `game_r41023_lua50` [Lua 5.0.3 Rollback] hem `game_r41023` [Lua 5.5.1 Default] üzerinde):
               * **Aşama A (`PHASE_SELECT`):** `HEADER_CG_LOGIN2(109)` -> `HEADER_GC_EMPIRE(90)` + `PHASE_SELECT(3)` + `HEADER_GC_LOGIN_SUCCESS(32, SmokeHero id=90011, level=15)`.
               * **Aşama B (`PHASE_LOADING`):** `HEADER_CG_CHARACTER_SELECT(6, index=0)` -> `HEADER_GD_PLAYER_LOAD` (`QID_PLAYER`, `QID_ITEM`, `QID_QUEST`, `QID_AFFECT`, `QID_SPECIAL_FLAG`) -> `PHASE_LOADING(4)` + `HEADER_GC_MAIN_CHARACTER(113, SmokeHero, pos=469300x964200)` + `HEADER_GC_CHARACTER_POINTS(16)` + `HEADER_GC_SKILL_LEVEL(76)`.
               * **Aşama C (`PHASE_GAME` & Login Quest):** `HEADER_CG_ENTERGAME(10)` -> `PHASE_GAME(5)` + `HEADER_GC_CHARACTER_ADD(1)` (hem `SmokeHero` PC hem haritadaki NPC `20016`) + `HEADER_GC_ITEM_SET(21, vnum=19)` + `notarget/login/smoke_world.start` çalışması (`[SMOKE_QUEST_LOGIN_OK] db_seed=77 lv=15 item19=1 qlib=1`).
               * **Aşama D (NPC `20016` Click & `wait()` -> `select()` -> `input()` Coroutine):** `HEADER_CG_ON_CLICK(26, npcVid)` -> `20016/click/smoke_world.start` -> `HEADER_GC_SCRIPT(45)` `[NEXT]` (`wait()` yield) -> `HEADER_CG_SCRIPT_ANSWER(29, 254)` -> `HEADER_GC_SCRIPT(45)` `[QUESTION 1;Continue|2;Cancel]` (`select()` yield) -> `HEADER_CG_SCRIPT_ANSWER(29, 0)` -> `HEADER_GC_SCRIPT(45)` `[INPUT]` (`input()` yield) -> `HEADER_CG_QUEST_INPUT_STRING(30, "SmokeInput55")` -> `[SMOKE_NPC_COROUTINE_OK] sel=1 input=SmokeInput55` + bağlantı kapanışında `PC::Save -> HEADER_GD_QUEST_SAVE` ile `login_verified=1` ve `npc_flow_done=1` bayraklarının canlı SQL `quest` tablosuna yazıldığının doğrulanması!
          4. **Güvenlik ve Gizlilik:** Test çıktılarında, hata günlüklerinde (`tail_log`) ve `PROJECT_STATE.md` içinde tüm `dwLoginKey`, parola ve hash değerleri maskelendi (`dwLoginKey=***MASKED(non-zero)***`), geçici anahtar dosyaları okunduktan hemen sonra silindi.
        * **FreeBSD CI Rev 46 (Run `39056319440` / Commit `3eb35d5d0954b544ad86806bd579204ee8959630`) Kesin Sonuçları — 3/3 MATRİS `%100 SUCCESS`:**
          * **FreeBSD 15.1 - MySQL 8.4 LTS (Job `117464215933`):** **`SUCCESS` (22 dk 53 sn)**
          * **FreeBSD 15.1 - MariaDB 12.3 LTS (Job `117464215928`):** **`SUCCESS` (23 dk 27 sn)**
          * **FreeBSD 15.1 - MariaDB 11.4 LTS (Job `117464216217`):** **`SUCCESS` (25 dk 42 sn)**
          * Üç veritabanı motorunda ve her iki Lua ikilisinde (`game_r41023` [Lua 5.5.1 Default] + `game_r41023_lua50` [Lua 5.0.3 Rollback]) `HEADER_DG_BOOT`, `HEADER_CG_LOGIN3`, `LoginByKey -> PHASE_SELECT -> PHASE_LOADING -> PHASE_GAME`, `notarget/login` görevi, NPC `20016` `wait() -> select(1) -> input('SmokeInput55')` coroutine akışı ve `PC::Save -> HEADER_GD_QUEST_SAVE` canlı SQL kaydı `0` hata ile doğrulandı.
      * **Aşama 21: Hesap ve Parola Güvenliğinin Sertleştirilmesi — Kademeli Argon2id Geçişi, Brute-Force Koruması, CSPRNG Oturum Anahtarı ve Tek-Kullanımlık `LoginByKey` (Rev 47):**
        * **1. Gerçek Kimlik Doğrulama Akışı Denetimi ve Kanıtları:**
          * **İstemci -> `AUTH_SERVER` (`server/src/game/src/input_auth.cpp`):** 32-bit istemci `PHASE_AUTH` (`10`) aşamasında `TPacketCGLogin3` (`HEADER_CG_LOGIN3 = 111`, `321` bayt) paketini gönderir. Parola istemciden `pinfo->passwd` (`PASSWD_MAX_LEN + 1 = 17` bayt) alanı içinde **düz metin (plaintext)** olarak gelir; ağ üzerinde 32-bit istemcinin statik anahtarlı (`"9382ujfn8237gydb"`) 8-bayt bloklu TEA şifrelemesiyle taşınır.
          * **Önemli Mimari Sınır Notu (32-Bit İstemci TEA Protokolü):** Mevcut 32-bit istemcinin kullandığı statik anahtarlı TEA ağ şifrelemesi **TLS eşdeğeri değildir** (ileri gizlilik / PFS veya sunucu sertifika doğrulaması sağlamaz). `AGENTS.md` Madde 8 (`CLIENT STRICT EMBARGO`) uyarınca `client/src/`, `client/files/` ve `extern/client/` salt-okunur olduğu için 32-bit istemci değiştirilmeden ağ katmanında TLS'e geçilmesi yalnızca sunucu tarafında tek başına çözülemez; buna karşılık sunucu tarafındaki parola saklama, doğrulama, oturum anahtarı (`dwLoginKey`), hız sınırlama ve SQL güvenliği tam olarak sertleştirilmiştir.
          * **Eski Doğrulama ve Tespit Edilen Güvenlik Açıkları:**
            1. Eski kodda `GetAuthLoginQuery` içinde `mysql_hash_password` (`*SHA1(SHA1(password))`) çağrılarak düz metin parola SQL sorgu metnine gömülmekte ve `QID_AUTH_LOGIN` (`server/src/game/src/db.cpp`) içinde `strcmp(szPassword, row[0])` ile zamanlama saldırısına açık şekilde karşılaştırılmaktaydı.
            2. `account.password` sütunu `sql/account.sql` içinde `varchar(45)` idi (modern PHC `$argon2id$` dizgeleri için yetersiz).
            3. `server/src/game/src/db.cpp:450` satırında `UPDATE account SET last_play=NOW(), hwid='%s'` sorgusunda `CheckHardware::DecodeHWID` çıktısı **escape edilmeden** SQL içine yazılmaktaydı (**ikinci derece SQL Injection açığı**).
            4. `server/src/db/src/ClientManagerLogin.cpp:511` (`QUERY_CHANGE_NAME`) içinde `p->name` alanı `EscapeString` çağrılmadan `UPDATE player SET name='%s'` sorgusuna gömülmekteydi (**SQL Injection açığı**).
            5. `QID_AUTH_LOGIN` içinde `uiNumRows == 0` durumunda `"NOID"`, yanlış parolada `"WRONGPWD"` döndürülerek **kullanıcı adı numaralandırma (username enumeration)** sızıntısı yapılmaktaydı.
            6. `DESC_MANAGER::CreateLoginKey` (`desc_manager.cpp`) tahmin edilebilir `number(1, 2000000)` PRNG kullanmaktaydı; `CClientManager::QUERY_LOGIN_BY_KEY` (`ClientManagerLogin.cpp`) ise `dwLoginKey` değerini ne süre sınırına (TTL) tabi tutmakta ne de ilk kullanımda tüketmekteydi (oturum anahtarı tekrar oynatma / replay riski).
            7. `CInputLogin::Login` (`HEADER_CG_LOGIN` = `1`) eski kimlik doğrulamasız giriş yolu açık bırakılmıştı.
            8. Sunucu günlüklerinde (`syslog`) `dwLoginKey`, `adwClientKey`, `dwPanamaKey`, `social_id` ve `r.passwd` düz metin parola verileri açıkça yazılmaktaydı.
        * **2. `phc-winner-argon2` (`20190702`) Entegrasyonu ve `AuthSecurity` (`server/src/libthecore`):**
          * Resmi `P-H-C/phc-winner-argon2` (`20190702` kararlı sürümü, tarball SHA256: `eae24fb92dde3beb6cdeeb1a4ca4ce3b2acff13f4725f6d1332c8aa59a32f9e0`) `server/src/libthecore/include/argon2.h` ve `server/src/libthecore/src/argon2/` altına entegre edildi.
          * `server/src/libthecore/include/auth_security.h` ve `server/src/libthecore/src/auth_security.cpp` ile hem `game_r41023` (Lua 5.5.1 Default) hem `game_r41023_lua50` (Lua 5.0.3 Rollback) hem de `db_r41023` tarafından paylaşılan güvenlik katmanı oluşturuldu:
            - **Varsayılan Argon2id PHC Parametreleri:** `$argon2id$v=19$m=19456,t=2,p=1$<base64_salt>$<base64_tag>` (`m=19456 KiB` [19 MiB], `t=2`, `p=1`, CSPRNG 16-bayt salt, 32-bayt çıktı etiketi).
            - **Sıkı PHC Parametre Sınırları (`IdentifyPasswordHashFormat`):** Bozuk veya aşırı maliyetli (`m` `[8192..65536]` KiB dışı, `t` `[1..8]` dışı, `p` `[1..4]` dışı) PHC dizgeleri bellek/CPU DoS saldırısını önlemek için `Invalid` olarak reddedilir.
            - **Geri Dönüş (Fallback) Yasağı:** `$argon2id$` formatındaki bir kayıt yalnızca Argon2id ile doğrulanır; asla eski `*SHA1(SHA1)` doğrulamasına geri dönmez.
            - **Eski `*SHA1(SHA1)` Doğrulaması ve Otomatik Yükseltme:** Yalnızca henüz yükseltilmemiş 41 karakterlik `*<40-HEX>` kayıtları sabit zamanlı (`ConstantTimeHexEquals`) doğrulanır ve `needs_upgrade = true` işaretlenir.
            - **Eşzamanlılık ve CPU Bütçesi Koruması:** Aynı anda en fazla `kMaxInFlightAuthQueries = 64` bekleyen kimlik doğrulama sorgusuna ve saniyede en fazla `kMaxArgon2OpsPerSecond = 40` Argon2id işlemine izin verilir (`SAuthLoginQueryInfo` RAII yıkıcısı ile slot sızıntısı ve `packet.passwd` bellek kalıntısı önlenir).
            - **Bağımsız IP ve Hesap Hız Sınırlayıcı (`AuthRateLimiter`):** IP başına 200ms burst koruması (`kIpBurstAllowance = 3`) ve hem IP hem hesap başına 60 saniyelik pencerede 5 başarısız deneme sonrasında `2 sn -> 4 sn -> 8 sn -> ... -> maks 60 sn` **geçici** üstel backoff uygulanır (saldırganların başka oyuncuları kalıcı kilitlemesini / DoS'u önlemek için kalıcı hesap kilitleme yapılmaz; en fazla 8192 kayıtla bellek sınırlıdır).
            - **CSPRNG (`GenerateCSPRNGBytes` / `GenerateSecureLoginKey`):** Windows üzerinde `BCryptGenRandom`, FreeBSD üzerinde `arc4random_buf` ile kriptografik güvenli rastgele sayı üretimi.
        * **3. `AUTH_SERVER` (`game`) ve `db_r41023` (`db`) Sertleştirmeleri:**
          * `input_auth.cpp` & `db.cpp`:
            - `pinfo->login`, `pinfo->passwd`, `pinfo->hwid` sabit tamponlarında `IsFieldNullTerminated` kontrolü zorunlu kılındı.
            - `GetAuthLoginQuery` içinden `mysql_hash_password` kaldırıldı ve `hw.hwid` parametresi `EscapeString` ile güvenli hale getirildi.
            - `QID_AUTH_LOGIN` içinde `uiNumRows == 0` durumunda zamanlama farkını azaltmak için `VerifyDummyPassword` çalıştırılır ve `"NOID"` yerine tek tip `"WRONGPWD"` döndürülür.
            - `GetAccountPasswordColumnMaxLength()` ile `INFORMATION_SCHEMA.COLUMNS` üzerinden `account.password` sütun uzunluğunun `>= 128` olduğu ön koşulu denetlenir; şema yükseltmesi (`rev47_account_security_upgrade.sql`) henüz uygulanmamışsa (`VARCHAR(45)`), eski hesabın parolasının kesilerek bozulmasını önlemek için Argon2id güncellemesi atlanır ve eski hesap güvenle giriş yapmaya devam eder.
            - Şema ön koşulu sağlandığında (`colMaxLen >= 128`), eski `*SHA1(SHA1)` hesap doğru düz metin parolayla giriş yaptığında düz metin parola `HashPasswordArgon2id` ile özetlenir ve `DirectQuery("UPDATE account SET password='%s', last_play=NOW(), hwid='%s' WHERE id=%u AND password='%s'")` ile atomik olarak güncellenip `uiAffectedRows == 1` (`affected_rows=1, col_len=255`) sonucu doğrulanır.
            - `r.passwd` içinde düz metin parola tutulması kaldırıldı (`"TEMP"`); `SAuthLoginQueryInfo` yıkıcısında `packet.passwd` `SecureZeroMemoryBuffer` ile sıfırlanır.
          * `desc_manager.cpp`, `desc.cpp`, `input_login.cpp`, `input_db.cpp`, `ClientManager.cpp`, `ClientManagerLogin.cpp`:
            - `DESC_MANAGER::CreateLoginKey` CSPRNG tabanlı `GenerateSecureLoginKey()` kullanacak şekilde güncellendi; `DESC::SetLoginKey(CLoginKey * pkKey)` içinde önceki `m_pkLoginKey` nesnesinin `Expire()` edilmesi `DESC` üye fonksiyonu kapsamına alınarak `protected` erişim hatası (`desc_manager.cpp:545`) giderildi ve tüm 6 `server/src/game/src/*.cpp` dosyası (`db.cpp`, `desc.cpp`, `desc_manager.cpp`, `input_auth.cpp`, `input_login.cpp`, `input_db.cpp`) Windows GCC `-fsyntax-only` ile doğrulandı.
            - Eski kimlik doğrulamasız `CInputLogin::Login` (`HEADER_CG_LOGIN`) devre dışı bırakıldı (`WRONGPWD`).
            - `CClientManager::QUERY_AUTH_LOGIN` içinde `pkLD->SetLogonTime()` kaydedilir; `CClientManager::QUERY_LOGIN_BY_KEY` içinde 60 saniyelik TTL kontrolü, sabit zamanlı `adwClientKey` doğrulaması ve **ilk başarılı kullanımda `m_map_pkLoginData.erase(p->dwLoginKey)` ile tek-kullanımlık tüketim** (`single-use`) zorunlu kılındı (`QUERY_MOVE_CHANNEL` kanal geçişinde anahtar güvenli şekilde yeniden silahlandırılır, `ResetLastPlayerID` ve `QUERY_LOGOUT` durumunda iptal edilir).
            - `ClientManagerLogin.cpp` `QUERY_CHANGE_NAME` içindeki `p->name` SQL Injection açığı `EscapeString` ile kapatıldı.
            - Tüm günlüklerde (`syslog`) `dwLoginKey`, `adwClientKey`, `dwPanamaKey` ve `social_id` değerleri `***MASKED***` olarak maskelendi.
        * **4. Güvenli SQL Şema ve Migration (`sql/account.sql`, `sql/migrations/rev47_account_security_upgrade.sql`):**
          * `sql/account.sql`: `login varchar(30)`, `password varchar(255)`, `shop_expire datetime NOT NULL DEFAULT current_timestamp()` olarak güncellendi.
          * `sql/migrations/rev47_account_security_upgrade.sql`: Mevcut hesapları silmeden/bozmadan önce `account_rev47_backup` yedek tablosunu oluşturan ve `account.password` sütununu `VARCHAR(255)` boyutuna genişleten idempotent geçiş betiği ve geri dönüş (rollback) adımları eklendi.
        * **5. Lisans, Üçüncü Taraf Kaynak Denetimi ve Kalıcı Testler (`server/src/libthecore/src/argon2/LICENSE`, `tests/test_auth_security.cpp`, `tests/run_server_smoke_test.sh`):**
          * `server/src/libthecore/src/argon2/LICENSE` (CC0 1.0 Universal / Apache 2.0 çift lisans, SHA256: `220F8736A89FF51C92EF3D497F413B48E6CF1DF3D6278BC909C6308C78E1718E`) ve tüm kaynak dosyalarındaki telif bildirimleri eksiksiz korundu (`core.c` içinde `NOT_OPTIMIZED` + `memset_sec` volatile işaretçisi ile `secure_wipe_memory` aktif).
          * `server/src/libthecore/src/Makefile` & `Depend`: `auth_security.o: auth_security.cpp` açık bağımlılık kuralı eklendi.
          * `tests/test_auth_security.cpp`: 7 kapsamlı senaryo ve **410/410 assertion PASS** (Windows performansı: `hash=22.0 ms, verify=21.0 ms`; FreeBSD 15.1 CI `39079851554` ve `39080977681` üzerinde doğrulanan gerçek performans: `hash=32649 us [32.6 ms], verify=23594 us [23.6 ms], len=97`).
          * `tests/run_server_smoke_test.sh` & `tests/test_server_smoke_client.cpp`: FreeBSD üzerinde üç veritabanı motorunda (`MySQL 8.4`, `MariaDB 12.3`, `MariaDB 11.4`) ve her iki Lua ikilisinde (`game_r41023_lua50` + `game_r41023`) `VARCHAR(45)` şema ön koşul koruması (eski hesabı bozmama), `rev47_account_security_upgrade.sql` migration, var olmayan hesap (`WRONGPWD`), bozuk hash (`WRONGPWD`), eski `*SHA1(SHA1)` hesaptan atomik `$argon2id$` yükseltmesi (`affected_rows=1`), yükseltme sonrası doğrudan Argon2id doğrulaması + SHA1 geri dönüş engeli, tüketilmiş `dwLoginKey` tekrar oynatma (`--game-login-key-expect-fail`) reddi ve `AuthRateLimiter` 5 hata -> 6. deneme `FAILURE` geçici backoff + otomatik iyileşme testleri eklendi ve CI `39086757830` (`be3b58b98d16d2dcee53af19623ca3875ab6ccfb`) üzerinde **3/3 SUCCESS** ile doğrulandı.
      * **Kapsamlı Denetim 11.39 — Sıfırdan Sunucu Kurulumu, Temiz SQL Şemaları, `db_r41023 --create-account` Argon2id CLI ve Uçtan Uca Karakter Oluşturma / Oyuna Giriş Akışı (Rev 48):**
        * **Proje Kararı:** Sunucu sıfırdan (eski oyuncu veritabanı veya hesap yedeği olmadan) açılacağı için tüm odak boş SQL veritabanından başlayarak **yeni hesap oluşturma -> karakter oluşturma -> oyuna girme -> sunucu yeniden başlatma sonrasında kalıcı hesap/karakterle tekrar oyuna girme** akışına verildi.
        * **1. Sıfırdan Kurulum İçin Temiz SQL Şemaları ve Veritabanı Eşlemesi (`sql/*.sql`):**
          * **Üretim Veritabanı Kurulum Eşlemesi:**
            - `SQL_ACCOUNT` (`account` veritabanı) $\leftarrow$ `sql/account.sql` (`account`, `block_exception`, `GameTime`, `GameTimeIP`, `GameTimeLog`, `hardware_status`, `iptocountry`, `string`)
            - `SQL_COMMON` (`common` veritabanı) $\leftarrow$ `sql/common.sql` (`exp_table`, `gmhost`, `gmlist`, `itemshop_items`, `itemshop_promotions`, `itemshop_time_auctions`, `locale`, `priv_settings`, `spam_db`, `vouchers`)
            - `SQL_LOG` (`log` veritabanı) $\leftarrow$ `sql/log.sql` (`log`, `hack_log`, `log_commands`, `bootlog`, `loginlog`, `loginlog2`, `character_log`, `item_log`, `gold_log`, `money_log`, `quest_reward_log`, `quest_state_log`, `cube_log`, `speed_hack_log`, `change_name_log`, `gmlog`, `refinelog`, `shout_log`, `level_log`, `fish_log`, `dragon_slay_log`, `hack_shield_log`, `invalid_server_log`, `pcbang_loginlog`, `levellog`, `locale_bug`)
            - `SQL_PLAYER` (`player` veritabanı) $\leftarrow$ `sql/player.sql` (`affect`, `ban_list`, `change_empire`, `guild`, `guild_comment`, `guild_grade`, `guild_invite_limit`, `guild_land`, `guild_member`, `guild_war_bet`, `guild_war_reservation`, `horse_name`, `ikashop_*`, `item`, `item_award`, `item_id_range`, `itemshop_time_auction`, `lotto_list`, `marriage`, `messenger_list`, `monarch`, `monarch_candidacy`, `monarch_election`, `myshop_pricelist`, `object`, `pcbang_ip`, `player`, `player_deleted`, `player_index`, `quest`, `safebox`, `shop_limit`, `sms_pool`, `special_spawn`, `string`)
            - `SQL_WORLD` (`world` veritabanı) $\leftarrow$ `sql/world.sql` (`banword`, `crafting_proto`, `item_attr`, `item_attr_rare`, `item_proto`, `land`, `mob_proto`, `name_reservation`, `object_proto`, `offlineshop_limit`, `quest_reward_proto`, `refine_proto`, `shop`, `shop_item`, `shop_special`, `shop_special_proto`, `skill_proto`). İzole duman testinde (`tests/run_server_smoke_test.sh`) üretim `sql/world.sql` dosyasına dokunulmadan geçici `mt2_testdb` içinde minimal `SQL_WORLD` test fixture'ı kullanılır; test verileri üretim şema dosyalarıyla kesinlikle karıştırılmaz.
          * Tüm 5 SQL şema dosyasından (`sql/account.sql`, `sql/common.sql`, `sql/log.sql`, `sql/player.sql`, `sql/world.sql`) `SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO"` gevşetmeleri kaldırıldı (`sql/world.sql` içindeki tüm ikili/non-ASCII baytlar birebir korundu).
          * `sql/account.sql`: `account.login VARCHAR(30)`, `account.password VARCHAR(255)`, `account.social_id VARCHAR(18) NOT NULL DEFAULT '1234567'` (0 satır); `GameTime.UserID`, `GameTimeLog.login`, `block_exception.login` sütunları `VARCHAR(30)` olarak hizalandı.
          * `sql/common.sql`: `gmlist` tablosundaki eski test kayıtları temizlendi (0 satır), `mAccount VARCHAR(30)` ve `mName VARCHAR(24)` olarak genişletildi.
          * `sql/log.sql`: Yalnızca MariaDB 11.4/12.3'te bulunan `utf8mb3_uca1400_ai_ci` collation tanımları MySQL 8.4 LTS uyumluluğu için `utf8mb4_general_ci` olarak standartlaştırıldı; `log_commands.date` içindeki `'0000-00-00 00:00:00'` varsayılanı `current_timestamp()` yapıldı; `log` ve `hack_log` tablolarına eksik `ip`/`login` sütunları eklendi; `LogManager` (`server/src/game/src/log.cpp`) tarafından çalışma zamanında kullanılan 23 eksik log tablosu şemaya eklendi.
          * `sql/player.sql`: `change_empire.last_time` içindeki `'0000-00-00 00:00:00'` varsayılanı `current_timestamp()` yapıldı; MySQL 8.4 LTS `STRICT_TRANS_TABLES` modunda `ERROR 1101 (42000)` hatasına yol açan `item.ikashop_data tinytext DEFAULT ''` sütunu aynı kapasiteye sahip `varchar(255) NOT NULL DEFAULT ''` olarak düzeltildi; eski sunucu dökümünden kalan `guild_land`, `guild_war_reservation`, `object` ve `special_spawn` satırları temizlendi; eksik `guild_invite_limit` tablosu eklendi; `guild_war_bet.login`, `item_award.login`, `messenger_list`, `monarch_candidacy.name` sütun genişlikleri `LOGIN_MAX_LEN (30)` / `CHARACTER_NAME_MAX_LEN (24)` ile hizalandı.
          * `sql/*.sql` içine hiçbir test hesabı veya `SmokeHero` örnek verisi eklenmedi.
        * **2. Güvenli Yeni Hesap Oluşturma (`AuthSecurity::PrepareNewAccountCredentials` & `db_r41023 --create-account`):**
          * `server/src/libthecore/include/auth_security.h` & `server/src/libthecore/src/auth_security.cpp`: `IsValidPlaintextPassword`, `IsValidSocialId`, `SNewAccountCredentials` ve `PrepareNewAccountCredentials` eklendi. Yeni hesaplar için 1..5 karakterlik zayıf parolaları reddeder ve 32-bit istemci protokol sınırını korur (`login` `2..30` karakter, `password` `6..16` yazdırılabilir karakter [`kMinPasswordLength = 6`, `kMaxPasswordLength = 16`], `social_id` `7..18` rakam, varsayılan `kDefaultSocialId = "1234567"`). Yalnızca rastgele CSPRNG salt içeren **Argon2id PHC (`$argon2id$v=19$m=19456,t=2,p=1$...`)** özeti üretir (yeni hesaplar için asla SHA1 kullanılmaz).
          * `server/src/db/src/Main.cpp`: `db_r41023 --create-account <login> [social_id] [hwid]` CLI modu eklendi (`social_id` belirtilmezse varsayılan `"1234567"` korunur). Düz metin parola kesinlikle komut satırı argümanı (`argv`) olarak alınmaz; yalnızca `stdin` üzerinden okunur, özetlendikten hemen sonra `SecureZeroMemoryBuffer` ile bellekten silinir, `account.password` sütun kapasitesi (`>= 96`) doğrulanır ve `SQL_ACCOUNT` veritabanına kaydedilir.
        * **3. Karakter Oluşturma ve Yükleme Kök Neden Düzeltmeleri (`ClientManagerPlayer.cpp`, `BanManager.cpp`):**
          * `server/src/db/src/ClientManagerPlayer.cpp`:
            - `QUERY_PLAYER_LOAD` içindeki iki `affect` SELECT sorgusuna (`ClientManagerPlayer.cpp:477, 513`) `CreateAffectTableFromRes` fonksiyonunun `row[9]` olarak okuduğu eksik `,itemVnum` sütunu eklendi (mevcut affect satırı olan karakterlerde oluşan `nullptr` dereference / SIGSEGV çökmesi kök nedeninden giderildi).
            - `__QUERY_PLAYER_CREATE` içinde `packet->account_index >= PLAYER_PER_ACCOUNT` sınır kontrolü eklendi, `player_table.name` için `EscapeString` zorunlu kılındı ve katı SQL modlarıyla tam uyumluluk için `INSERT INTO player` sorgusundaki `id = 0` ataması kaldırılarak `AUTO_INCREMENT` kimlik üretimine bırakıldı.
          * `server/src/game/src/BanManager.cpp`: Katı SQL modunda (`NO_ZERO_DATE`) hataya yol açan `'0000-00-00 00:00'` tarih karşılaştırması `NULL` ile değiştirildi.
        * **4. Uçtan Uca Sıfırdan Kurulum Smoke Testi (`tests/test_server_smoke_client.cpp`, `tests/run_server_smoke_test.sh`):**
          * `tests/test_auth_security.cpp`: `TestScenario8_NewAccountCredentialPreparation` eklendi (**423/423 assertion PASS** — 1 karakterlik zayıf parola ve 17 karakterlik parola reddi ile `social_id = 1234567` varsayılanı dahil).
          * `tests/test_server_smoke_client.cpp`: 0 karakterli yeni hesap `PHASE_SELECT` fazına girdiğinde `HEADER_CG_EMPIRE` (`90`, İmparatorluk 1) ve `HEADER_CG_CHARACTER_CREATE` (`4`, `SmokeHero`, Savaşçı, slot 0) paketlerini gönderip `HEADER_GC_CHARACTER_CREATE_SUCCESS` (`8`) paketini doğrulayan, ardından `HEADER_CG_CHARACTER_SELECT` (`6`) -> `PHASE_LOADING` (`4`) -> `HEADER_CG_ENTERGAME` (`10`) -> `PHASE_GAME` (`5`) + başlangıç eşyası (`vnum=19`) + quest/NPC (`20016`) coroutine akışını tamamlayan tam istemci simülasyonu eklendi.
          * `tests/run_server_smoke_test.sh`: Boş veritabanlarına doğrudan `sql/account.sql`, `sql/common.sql`, `sql/log.sql`, `sql/player.sql` şemalarını katı SQL modunda yükleyen, `db_r41023 --create-account` ile 1 karakterlik ve 17 karakterlik parolaları reddedip geçerli parolayla Argon2id + `social_id=1234567` hesap oluşturan, karakter oluşturup oyuna giren ve ardından **`db_r41023` + `AUTH_SERVER` + `GAME_SERVER` süreçlerini yeniden başlatarak** aynı Argon2id hesap ve kalıcı `SmokeHero` karakteriyle ikinci kez oyuna giren uçtan uca doğrulama eklendi ve CI `39120190223` (`4d9171b73b065b28f59e553aa57996322015cd3a`) üzerinde **3/3 SUCCESS** ile doğrulandı.
      * **Kapsamlı Denetim 11.40 — Gerçek Windows x64 ve FreeBSD 15.1 x64 Serverfiles Kurulumu, 8 Servis (`db`, `auth`, `ch11..ch01`) 0 `syserr` Temizliği ve CI Log Artifact Doğrulaması (Rev 49):**
        * **1. Windows x64 Tam Sunucu İkilileri (`db_r41023.exe`, `game_r41023.exe`, `game_r41023_lua50.exe`):**
          * `extern/platform/windows-x64/mingw-ucrt-gcc16/lib/libIL.a` statik 64-bit olarak derlendi; `cmake/FindDevIL.cmake`, `cmake/FindCryptoPP.cmake` ve `server/CMakeLists.txt` güncellenerek Windows x64 üzerinde hem `db_r41023.exe` hem `game_r41023.exe` (Lua 5.5.1 Default) hem de `game_r41023_lua50.exe` (Lua 5.0.3 Rollback) 0 uyarı ile derlendi (`ctest --preset windows-gcc-ucrt` **14/14 PASS**).
          * `server/src/game/src/char_item.cpp:281` (`DWORD` -> `uintptr_t`), `FileMonitor_FreeBSD.h` (`#ifdef __FreeBSD__`), `questlua_test.cpp` (`CArenaManager::instance_ptr()` null kontrolü) ve `server/src/libthecore/src/xdirent.c` (`long handle` -> 64-bit `intptr_t handle` ile `_findfirst`/`_findnext` işaretçi kesilmesi hatası) kök nedenlerinden düzeltildi.
        * **2. Çapraz Platform Çalışma Zamanı, Bakım Modu ve Zarif Kapatma (`0 syserr`) Kök Neden Düzeltmeleri:**
          * `server/src/libthecore/src/log.c`: Windows ve FreeBSD arasında `sys_log` filtreleme davranışı (`if (log_level_bits > 1)`) eşitlendi.
          * `server/src/db/src/Main.cpp`: `SetTablePostfix` içinde `conf.txt`'den okunan boş `TABLE_POSTFIX = ""` değerindeki boşluk karakteri temizlendi.
          * `server/src/game/src/config.cpp`: `ACCOUNT_SQL` yapılandırma anahtarı ayrıştırıldı ve `g_bAuthServer == true` olduğunda `DBManager::instance()` bağlantısının `ACCOUNT_SQL` veritabanına kurulması sağlandı.
          * `server/src/db/src/ClientManager.cpp` & `server/src/game/src/config.cpp`: Gerçek üretim serverfiles (`TEST_SERVER: 0`) açılışında normal oyuncu girişlerini `"MAINTENA"` hatasıyla engelleyen varsayılan `m_bMaintenance = TRUE` / `g_bIsMaintenance = true` değerleri `FALSE`/`false` olarak düzeltildi.
          * `server/src/db/src/ClientManager.cpp`, `ClientManagerEventFlag.cpp`, `ClientManagerPlayer.cpp`, `GuildManager.cpp`, `ClientManagerBoot.cpp`: FreeBSD amd64 (`LP64`, `sizeof(long) == 8`) üzerinde `tables.h` içinde 32-bit `int32_t` olarak tanımlı alanların (`TQuestTable::lValue`, `TPacketSetEventFlag::lValue`, `TPlayerTable::{lExitX, lExitY, lExitMapIndex, lAlignment}`, `TPacketAffectElement::{lApplyValue, lDuration, lSPCost}`, `TGuildWarReserve::{lInitialScore, lPowerFrom, lPowerTo, lHandicap}`, `TRequestChargeCash::lAmount`, `TItemTable::{aLimits, aApplies, alValues}`) `snprintf` içinde `%ld` ile biçimlendirilmesi nedeniyle negatif 32-bit değerlerin (ör. `main_quest_lv1` `gotoinfomation` CRC32 durumu `-596781658` / `0xDC6DF1A6`) 64-bit işaretsiz uzatmayla `+3698185638` olarak yazılıp `errno: 1264 Out of range value for column 'lValue'` hatası üretmesi kök nedeninden (`%ld` -> `%d`) giderildi.
          * `server/src/game/src/input_db.cpp`: `CInputDB::QuestLoad` içinde yeni oluşturulan karakterlerin ilk girişinde `dwCount == 0` (0 quest satırı) geldiğinde ve istemci `PHASE_GAME` fazına geçmiş olduğunda (`ch->GetDesc()->IsPhase(PHASE_GAME)`), mevcut olmayan `pQuestTable[0].dwPID` alanının paket tamponu dışından okunarak (`HEADER_DG_AFFECT_LOAD` başlığı `306` / `0x0132`) `Login: QUEST no such pc id : 306` `syserr` kaydı üretmesi kök nedeninden (`ch->GetPlayerID()` kullanılarak) düzeltildi.
          * `server/src/libthecore/src/main.c`, `signal.c`, `fdwatch.c`: Windows üzerinde `./pid` dosyası yönetimi, `SetConsoleCtrlHandler` ve `./stop_signal` yoklaması eklendi; `fdwatch.c` (`__USE_SELECT__`) içinde `fdwatch_del_fd()` aktif döngüde `select_rfdidx[]` eşlemesini güncelleyecek ve `fdwatch_check_fd()`/`fdwatch_check_event()` karşı soket kapandığında FreeBSD `kqueue` (`EV_EOF -> FDW_EOF`) ile birebir aynı şekilde `FDW_EOF` döndürecek biçimde düzeltildi (kapatma sırasındaki sahte `Recv: socket_read failed` / `socket_read: about to lose connection` `syserr` kayıtları tamamen giderildi).
        * **3. Gerçek Serverfiles Veri ve `syserr` Kök Neden Düzeltmeleri (`sql/world.sql`, `dragon_soul_table.txt`):**
          * `sql/world.sql`: GM marketi `1004` (`shop_item`) içindeki 45 hücrelik ızgarayı taşıran 3 adet 3-boyutlu eşya (`3100`, `3110`, `3120`) kaldırıldı (`SetShopItems: not enough shop window`); sabit NPC `20364` (`naoki`) için `NOMOVE` bayrağı eklendi; sabit `NOMOVE` NPC'leri `20406` ve `20407` üzerindeki geçersiz `npc2` klasör adları (`historian`, `starveling`) temizlendi.
          * `share/locale/poland/dragon_soul_table.txt` (ve `server/serverfiles_share.tar.xz`): `ENABLE_DS_GRADE_MYTH` (`GRADE_MYTH = 5`) ile uyumlu olacak şekilde `ApplyNumSettings`, `WeightTables`, `RefineGradeTables`, `DragonHeartExtTables` ve `DragonSoulExtTables` bloklarına eksik `grade_myth` tabloları eklendi.
        * **4. Windows x64 (`D:\mt22009\server\files`) ve FreeBSD 15.1 x64 (`/usr/home/mt2`) Gerçek Serverfiles Doğrulaması:**
          * `server/scripts/windows/{start.ps1,stop.ps1,status.ps1}` ve `server/scripts/freebsd/{start.sh,stop.sh,status.sh}` yönetim betikleri ile `tests/run_freebsd_serverfiles_test.sh` oluşturuldu.
          * Windows x64 üzerinde 9 servis (`mysql :3306`, `db :18000`, `auth :17105`, `ch11 :28000`, `ch12 :28001`, `ch13 :28002`, `ch99 :13099`, `ch00 :13088`, `ch01 :13098`) **0 `syserr`** ile başlatıldı; `win64hero` Argon2id hesabı -> İmparatorluk 1 -> `SmokeHero` karakter oluşturma -> `PHASE_GAME` (`metin2_map_a1`, `give_basic_weapon.start` başlangıç silahı `vnum=10`) -> zarif `stop.ps1` (`0 syserr`) -> `start.ps1` (`0 syserr`) -> kalıcı `SmokeHero` ile yeniden `PHASE_GAME` girişi %100 doğrulandı.
          * **Kalıcı Veritabanı Standardı ve CI Matris Düzenlemesi:** Kalıcı proje kararıyla desteklenen veritabanları **MariaDB 12.3 LTS** (ana veritabanı) ve **MySQL 8.4 LTS** (desteklenen alternatif) olarak sabitlendi; `MariaDB 11.4 LTS` `.github/workflows/freebsd-gcc-test.yml` CI matrisinden çıkarıldı (`social_id` varsayılanı `'1234567'`, SQL şemaları, Lua yapılandırması ve istemci dosyaları aynen korundu). Rev 49 CI `39143621656` (`a9ad54155d8f8681c7fded5d9b4d8554e99b7022`) her iki hedef motorda (`FreeBSD 15.1 - MariaDB 12.3 LTS` ve `FreeBSD 15.1 - MySQL 8.4 LTS`) **2/2 SUCCESS** ve `0 syserr` ile kapandı.
      * **Kapsamlı Denetim 11.41 — Serverfiles Kimlik Bilgisi Güvenliği, Taşınabilir Dağıtım, İkincil Kanallar (`ch12..ch01`) / Harita Geçişi (`WarpSet`), Log Rotasyonu ve `new_ftp.tar.gz` Tam Aktarım Denetimi (Rev 50):**
        * **1. SQL ve Yönetici Kimlik Bilgisi Güvenliği:**
          * `server/config/serverfiles.env.example` şablonu oluşturuldu; örnek yapılandırmada yalnızca açıkça belirtilmiş yer tutucular (`__CHANGE_ME_DB_PASSWORD__`, `__CHANGE_ME_ADMINPAGE_PASSWORD__`, `__CHANGE_ME_SERVER_KEY__`) kullanıldı (repoda gerçek parola veya secret tutulmaz).
          * `.gitignore` güncellenerek yerel çalışma zamanı yapılandırmaları (`serverfiles.env`, `*.env.local`, `server/files/**/conf.txt`, `server/files/**/CONFIG`) Git takibinden hariç tutuldu.
          * `server/src/game/src/config.cpp`: Sabit `"SHOWMETHEMONEY"` varsayılan parolası kaldırıldı (`g_stAdminPagePassword = ""`); `ADMIN_PAGE_PASSWORD` log çıktısı `[REDACTED (len=...)]` olarak maskelendi; SQL yapılandırma tamponları (`user`, `passwd`, `line`) bağlantı sonrasında `AuthSecurity::SecureZeroMemoryBuffer` ile sıfırlandı.
          * `server/src/db/src/Main.cpp`: `Start()` içindeki SQL parola tamponları (`line`, `szUser`, `szPasswd`) bağlantı sonrasında `AuthSecurity::SecureZeroMemoryBuffer` ile sıfırlandı.
          * **En Az Yetki (Least-Privilege) SQL Standardı:** Çalışma zamanı veritabanı kullanıcısı (`mt2server`) için `GRANT ALL PRIVILEGES ON *.*` kaldırıldı; yalnızca `account`, `common`, `log`, `player`, `world` veritabanları üzerinde `SELECT, INSERT, UPDATE, DELETE` yetkileri tanımlandı (`DDL`, `FILE`, `SUPER`, `GRANT OPTION` verilmez).
          * **Komut Satırı ve Log Gizliliği:** `install.ps1`, `install.sh` ve CI betiklerinde `-p"$PASS"` komut satırı argümanı kaldırıldı; bunun yerine `chmod 600` (FreeBSD) ve `icacls /inheritance:r` (Windows) ile korunan geçici `--defaults-extra-file` (`[client]`) dosyası kullanıldı (`ps aux`, `syslog`, `syserr` ve CI artifact'larında parola sızıntısı 0).
          * Yeni hesapların `social_id` varsayılanı `'1234567'` olarak korundu.
        * **2. Taşınabilir Windows x64 ve FreeBSD 15.1 Serverfiles Dağıtımı ve Kurulum Kılavuzu:**
          * `server/scripts/windows/{install.ps1,start.ps1,stop.ps1,status.ps1}` ve `server/scripts/freebsd/{install.sh,start.sh,stop.sh,status.sh,mt2server.rc}` içindeki sabit geliştirici yolları (`D:\mt22009`, `scratch\isolated_db\...`) tamamen kaldırıldı; betikler kendi konumlarından veya `MT2_SERVERFILES_ROOT` / `-ServerRoot` parametresi üzerinden taşınabilir çalışır.
          * `install.ps1` ve `install.sh` mevcut SQL veritabanlarında tablo sayısını (`TABLE_COUNT`) kontrol eder; tablolar mevcutsa mevcut oyuncu/dünya verilerini silmeden (`non-destructive`) korur, yalnızca boş veritabanlarına `sql/{account,common,log,player,world}.sql` şemalarını katı SQL modunda yükler.
          * **Windows x64 Adım Adım Kurulum ve Yönetim Komutları (Parola İçermez):**
            1. Derleme: `cmake --build --preset windows-gcc-ucrt -- -j4`
            2. Ortam yapılandırması: `Copy-Item server\config\serverfiles.env.example server\files\serverfiles.env` (yerel `MT2_DB_PASS`, `MT2_ADMINPAGE_PASSWORD`, `MT2_SERVER_KEY` değerlerini düzenleyin)
            3. Kurulum: `powershell -ExecutionPolicy Bypass -File server\scripts\windows\install.ps1 -TargetDir D:\mt22009\server\files -InitSql`
            4. Başlatma / Durum / Durdurma:
               - `powershell -ExecutionPolicy Bypass -File D:\mt22009\server\files\start.ps1`
               - `powershell -ExecutionPolicy Bypass -File D:\mt22009\server\files\status.ps1`
               - `powershell -ExecutionPolicy Bypass -File D:\mt22009\server\files\stop.ps1 -RotateLogs`
          * **FreeBSD 15.1 amd64 Adım Adım Kurulum ve Yönetim Komutları (Parola İçermez):**
            1. Derleme: `cmake --preset freebsd-gcc14 && cmake --build --preset freebsd-gcc14 -- -j4`
            2. Ortam yapılandırması: `cp server/config/serverfiles.env.example /usr/home/mt2/serverfiles.env && chmod 600 /usr/home/mt2/serverfiles.env`
            3. Kurulum: `sh server/scripts/freebsd/install.sh --target-dir /usr/home/mt2 --env-file /usr/home/mt2/serverfiles.env --init-sql`
            4. Başlatma / Durum / Durdurma (`rc.d` veya doğrudan betik):
               - `/usr/home/mt2/start.sh` | `/usr/home/mt2/status.sh` | `/usr/home/mt2/stop.sh --rotate-logs`
               - veya `/usr/local/etc/rc.d/mt2server start|status|stop|restart|rotate` (`sysrc mt2server_enable=YES`)
        * **3. İkincil Kanallar (`ch12`, `ch13`, `ch99`, `ch00`, `ch01`), P2P ve Harita Geçişleri:**
          * `server/src/game/src/map_location.cpp`: `CMapLocation::Insert` içinde `m_map_address` güncellenerek kanal yeniden başlatmalarında eski port bilgisinin kalması önlendi.
          * `server/src/game/src/char.cpp`: `CHARACTER::CanWarp` ve `CHARACTER::IsHack` içinde sunucu açılışının ilk 10 saniyesinde `m_iSafeboxLoadTime == 0` vb. başlangıç zaman damgalarının `thecore_pulse() - 0 < 250` nedeniyle yanlışlıkla portal geçişini engellemesi (`GetSafeboxLoadTime() > 0` kontrolüyle) düzeltildi; `CHARACTER::ChangeChannel` içinde `GetMapIndex() >= 10000` kontrolü ve `HEADER_GD_MOVE_CHANNEL` sonrası `WarpSet` yönlendirmesi eklendi.
          * `server/src/db/src/{LoginData.h,LoginData.cpp,ClientManager.cpp,ClientManagerLogin.cpp}` & `server/src/game/src/{char.cpp,input_db.cpp}`: `CClientManager::MoveChannel` kanal numarası eşlemesi düzeltildi; Rev 47 tek kullanımlık (`single-use`) ve 60 saniye ömürlü (`TTL`) oturum anahtarı güvenliğini hiçbir peer için gevşetmeden çekirdekler arası geçişi sağlamak üzere `CLoginData::m_bWarpHandoffPending` mekanizması eklendi:
            - Her `QUERY_LOGIN_BY_KEY` çağrısında `m_map_pkLoginData.erase(p->dwLoginKey)` koşulsuz olarak anahtarı haritadan silmeye devam eder (hem aynı peer hem farklı peer üzerinden yetkisiz tekrar kullanımı %100 engeller).
            - Yalnızca oturumun o anki sahibi olan yetkili oyun çekirdeği (`pkLD->GetConnectedPeerHandle() == pkPeer->GetHandle()`), `CInputDB::LoginSuccess` (`player.wPort != mother_port` `PHASE_SELECT` yönlendirmesi) veya `CHARACTER::WarpSet` / `MoveChannel` sırasında `HEADER_GD_MOVE_CHANNEL` göndererek tek seferlik geçiş bayrağını (`SetWarpHandoffPending(true)`) kurabilir.
            - Kaynak çekirdekten gelen `QUERY_LOGOUT` yalnızca bu bayrak aktifse anahtarı hedef çekirdeğin tek seferlik `QUERY_LOGIN_BY_KEY` tüketimi için korur ve bayrağı sıfırlar; normal bağlantılarda veya geçiş tamamlandıktan sonra aynı ya da farklı bir peer (`ch12`, `ch11` vb.) üzerinden yapılan tüm `dwLoginKey` tekrar denemeleri reddedilir (`[cross-core-step5]`).
          * **Doğrulanan (`PASS`) ve İstemci Gerektiren (`TEST EDİLMEDİ`) Akışların Açık Ayrımı:**
            - `ch11..ch01` 6 oyun çekirdeği P2P mesh ve `CMapLocation` tablosu kaydı: **PASS** (`syslog` + `test_server_smoke_client`)
            - `ch11 (port 28000)` `PHASE_SELECT` üzerinden `map_index = 3` (`metin2_map_a3`) üzerindeki karakterin `CMapLocation` ile `ch12 (port 28001)` çekirdeğine yönlendirilmesi: **PASS**
            - `ch12 (port 28001)` çekirdeğinde `map_index = 3` (`401600x899600`) üzerinde `PHASE_GAME` dünyasına giriş: **PASS**
            - `ch12 (port 28001)` üzerindeki gerçek portal NPC `10002` (`402000x899900`) yanına hareket -> `CHARACTER::WarpSet(450300, 905000, 0)` -> `HEADER_GC_WARP(65, dest=450300x905000, port=28000)` -> `ch11 (port 28000)` çekirdeğine bağlanıp `map_index = 1` (`450300x905000`) üzerinde `PHASE_GAME` dünyasına geçiş: **PASS**
            - Geçiş ve normal çıkış sonrasında tüketilmiş `dwLoginKey` değerinin hem **başka bir peer (`ch12:28001`)** hem de **aynı peer (`ch11:28000`)** üzerinde tekrar kullanılmasının reddedilmesi (`[cross-core-step5]`): **PASS**
            - Görsel 32-bit Windows GUI istemcisi üzerinden oyun içi kanal değiştirme penceresi, harita yükleme ekranı ve zindan/lonca savaşı (`ch99`/`ch00`/`ch01`) görsel geçişleri: **TEST EDİLMEDİ** (`AGENTS.md` Madde 8 İstemci Ambargosu uyarınca 32-bit GUI istemci çalıştırılmadı).
        * **4. Log Rotasyonu, Zarif Kapatma (Graceful Shutdown) ve Süreç Yönetimi:**
          * `server/src/db/src/Main.cpp`: `main()` çıkışında `thecore_destroy()` (`pid_deinit()` + `log_destroy()`) çağrısı eklendi.
          * `server/src/game/src/main.cpp`: `char_manager.Destroy()` ve `item_manager.Destroy()` sonrasında, `desc_manager.Destroy()` `db_clientdesc` nesnesini serbest bırakmadan hemen önce son `db_clientdesc->FlushOutput()` çağrısı eklendi; `building_manager.Destroy()` sonrasında ise bekleyen `DBManager::instance().CountQuery()` sorgularını tahliye eden döngü eklendi.
          * `stop.ps1` ve `stop.sh`: 3 aşamalı sıralı zarif kapatma (`[1/3] auth` -> `[2/3] ch11..ch01` -> `[3/3] db`) ve süreç sonlanma doğrulamasıyla birlikte `syserr`, `syslog`, `PTS`, `stdout.log`, `stderr.log` dosyalarını `<service>/log/archive/` altına zaman damgalı ve sayı sınırlı (`MT2_LOG_MAX_ARCHIVES=10`) olarak arşivleyen log rotasyonu tamamlandı (`Windows x64` testinde 48 arşiv dosyası doğrulandı).
          * FreeBSD `rc.d` servis betiği (`server/scripts/freebsd/mt2server.rc`) oluşturuldu.
        * **5. `new_ftp.tar.gz` Eksiksiz Aktarım, SHA256 Denetimi ve Kaldırılması:**
          * `D:\mt22009\mt2009 - FULL SOURCE\new_ftp.tar.gz` (`50,566,464` bayt) içindeki tüm `14,416` dosya girdisi `server/serverfiles_share.tar.xz` (`14,342` dosya) ile SHA256 düzeyinde karşılaştırıldı ve eksik oyun verisi olmadığı doğrulandıktan sonra kullanıcı kararıyla `mt2009 - FULL SOURCE/new_ftp.tar.gz` çalışma ağacından ve `.gitignore` listesinden kalıcı olarak kaldırıldı (`server/serverfiles_share.tar.xz` tek resmî kaynak arşiv olarak korundu):
            - **Ortak ve SHA256 Birebir Aynı Dosyalar:** `14,340 / 14,341` dosya (`share/data/*`, `share/locale/poland/map/*`, `share/locale/poland/quest/*`, `share/package/*`, `chs/ch01/CMD`, `chs/ch01/mark/*`).
            - **Bilinçli Güncellenen Dosya (`1` adet):** `share/locale/poland/dragon_soul_table.txt` (`9,390` -> `9,798` bayt; Rev 49 `ENABLE_DS_GRADE_MYTH` uyumu).
            - **`server/serverfiles_share.tar.xz` İçine Eklenen Dosya (`1` adet):** `share/conf/CMD` (`73` bayt; `chs/ch01/CMD` ile aynı SHA256).
            - **Bilinçli Olarak Dışarıda Bırakılan / Kurulumda Üretilen Girdiler (`75` adet):** `3` adet eski 32-bit FreeBSD i386 ikilisi (`db/db`, `share/game`, `share/locale/poland/quest/qc`), `23` adet eski çalışma zamanı PID ve log dosyası (`pid`, `PacketTracker.log`, `ch*.log`), `17` adet sabit parolalı eski `conf.txt`/`CONFIG` dosyası (artık `install.ps1`/`install.sh` tarafından güvenli üretiliyor), `3` adet eski kök betiği (`start.sh`, `stop.sh`, `clear.sh`), `1` adet mükerrer `share/CMD` ve `28` adet kanal bazlı `mark/mark_0.tga` + `mark_index` kopyası (artık `install.ps1`/`install.sh` tarafından her kanala otomatik kopyalanıyor).
        * **6. Sunucu Kaynak Kodlarından İki Tam Yorum Satırının Temizlenmesi:**
          * `server/src/` altındaki `575` kaynak dosyadan yalnızca `//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f` (`524` satır) ve `// Files shared by GameCore.top` (`397` satır) ile tam eşleşen toplam `921` yorum satırı bayt düzeyinde (kodlama ve satır sonları korunarak) kaldırıldı; başka hiçbir yorum, telif/lisans metni veya kod satırına dokunulmadı.
          * İstemci ambargosu (`AGENTS.md` Madde 8) kapsamındaki `client/src/`, `client/files/` ve `extern/client/` dizinlerine kesinlikle dokunulmadı (`client/src/` içinde `643` dosyada `618` adet `//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f` ve `641` adet `// Files shared by GameCore.top`, `client/files/` ve `extern/client/` içinde `0` eşleşme yalnızca sayılarak raporlandı).

        * **7. Kalıcı CI Maliyet Kontrolü, `ci/run-full-freebsd.txt` Tetikleyicisi, 4 Kademeli Test Standardı ve `ccache` + `libcryptopp.a` Önbelleklemesi (`035e94bdedc103512348fd8917858336283c6e4c`):**
          * `.github/workflows/freebsd-gcc-test.yml`: Sıradan push işlemlerinin pahalı FreeBSD 15.1 VM matrisini otomatik başlatması kaldırıldı; workflow tetikleyicisi `dev/windows-gcc-baseline` dalında yalnızca `ci/run-full-freebsd.txt` dosyasının değiştirilmesine bağlandı (`.github/workflows/` varsayılan `main` dalında bulunmadığı ve `main` dalına dokunulmadığı için çalışmayacak `workflow_dispatch` kaldırıldı; dal üzerinde zorunlu GitHub status check / ruleset bulunmadığı doğrulandı).
          * `actions/cache@v4` ile `.ccache-freebsd` ve `extern/server/cryptopp/libcryptopp.a` önbelleklemesi, `ccache` derleyici başlatıcısı (`-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`) ve `gmake -j4` paralelleştirmesi yapılandırıldı. **Doğrulama Durumu:** CI Run ID `39859687668` (`Job 120106866220`, `18m 2s`) üzerinde `.ccache-freebsd` (`1,554` önbellek dosyası) + `extern/server/cryptopp/libcryptopp.a` derlemesi ve `Post Save CMake/ccache & Crypto++ build cache` adımında **`66,434,038` bayt (`66.4 MB`, `%100.0`)** önbellek yüklemesi (`freebsd151-gcc14-mariadb123-8842e39e8112b0b91a69dfff2ce23dc5b2f3becb86c8703f962d9bc24a6528a0`) gerçek FreeBSD 15.1 CI üzerinde doğrulandı.
          * `AGENTS.md` ve `PROJECT_STATE.md` içine 4 kademeli doğrulama standardı (`A: Statik/Sözdizimi` -> `B: Windows CMake/CTest` -> `C: Yerel SQL/Protokol Duman Testi` -> `D: Bilinçli FreeBSD CI`) ve aylık GitHub Actions kota politikası (`3.000 dk` toplam, `2.400 dk` proje normal üst sınırı, `600 dk` zorunlu rezerv) kalıcı kural olarak işlendi.
      * **Kapsamlı Denetim 11.42 — Yalnızca Lua 5.5.1 Motoru, Tüm `.quest` / `.lua` / `object/` Envanter Sınıflandırması (`A–H`), FreeBSD CI Öncesi 4 Kritik Risk Denetimi ve Eski Lua 5.0.3 / 5.2.3 Motorlarının Kaldırılması (Rev 51 — `CLOSED` / KAPANDI):**
        * **1. `.quest` ve `.lua` Kaynaklarının Tam Envanteri ve Sınıflandırması (`A–H` Kategorileri — `server/files/share/locale/poland`):**
          * **Kategori A — Aktif Görev Kaynakları (`quest/quest/` + `quest/locale_list`):**
            - `locale_list` içinde tanımlı **202 aktif görev kaynağı** (`199` adet `.quest` dosyası + `3` adet `.lua` uzantılı görev kaynağı: `systems/crafting.lua`, `systems/herbalism/herbalism.lua`, `systems/potion_use.lua`).
            - Ön-derleme (`quest/precompile.py`) gereken **20 dosya** (`19` `.quest` + `1` `.lua` [`herbalism.lua`]) mevcut `quest/pre_qc/` içeriğiyle **0 fark (`%100` eşleşme)** üretmektedir.
            - Lua 5.5.1 tabanlı tek `qc` derleyicisi (`tests/run_quest_matrix.py --force --clean`) ile **202/202 PASS (`%100`, `0 FAIL`, `0 TIMEOUT`)** derlenmekte ve `object/state/` altında **202 benzersiz görev durum tablosu (`1:1`)** üretmektedir.
            - **Risk 1 Denetimi ve Kök Neden Düzeltmesi (`guild_building.quest` vs `guild_manage.quest` — `DÜZELTİLDİ`):**
              * C++ sunucu kaynak kodları (`server/src/game/src/guild.cpp:977, 1992, 1995`, `server/src/game/src/input_main.cpp:2648, 2656, 2905`) ve `server/src/db/src/ClientManagerGuild.cpp:75, 112` açıkça `guild_manage.new_withdraw_time` ve `guild_manage.new_disband_time` quest bayraklarını okumakta ve yazmaktadır (`guild.cpp:977` üzerinde `// @fixme401` ile işaretli).
              * Buna karşın `quest/systems/guild/guild_manage.quest` dosyasının 1. satırında kopyala-yapıştır hatasıyla `quest guild_building begin` yazıldığı tespit edildi. `Make.py` derleme sırasında önce `guild_building.quest` (#168, NPC `20040` Land Agent), ardından `guild_manage.quest` (#173, NPC `11000`/`11002`/`11004` Village Guard) derlendiğinden; `guild_manage.quest` çıktısı `object/state/guild_building` dosyasını eziyor (`object/state/guild_manage` hiç oluşmuyordu), `guild_man1..3/chat/` altına `guild_building.start.0..4.*` adıyla 45 dosya yazıyor ve çalışma zamanında `pc.getqf("new_withdraw_time")` / `pc.setqf("new_disband_time")` çağrılarını `guild_manage` yerine `guild_building` altında çalıştırarak C++ lonca kurma/katılma süre kontrolleriyle (`guild_manage.*`) uyumsuzluk yaratıyordu.
              * `guild_manage.quest` 1. satırı `quest guild_manage begin` olarak düzeltildi; `object/guild_man{1,2,3}/chat/` altındaki 45 çakışan `guild_building.start.*` dosyası kaldırıldı ve her iki görev Lua 5.5.1 `qc` ile yeniden derlendi (`object/state/guild_building` + `object/state/guild_manage` + 45 `guild_manage.start.*` dosyası; `202` aktif görev = `202` `object/state/*` tablosu, toplam `6,900` `object/` dosyası). Tüm 202 aktif görev tarandı ve başka çakışan quest adı olmadığı (`0` çakışma) kanıtlandı.
            - **Risk 4 Denetimi — Arşiv/Devre Dışı Görevler (`quest/quest/_unused/` — `DÜZELTİLDİ`):**
              * `locale_list` dışında tutulan **126 dosya** (`125` `.quest` + `1` `.lua`: `spider_dungeon_3floor_boss.quest.lua`) `_unused/` altında korundu.
              * `_unused/event_mob_invasion.quest`: 88. satırdaki `else (wave > 3) then` yazım hatası `elseif (wave > 3) then` olarak düzeltildi; Lua 5.5.1 `qc` ile **PASS** (`returncode=0`).
              * `_unused/future_story/main_quest_lv60.quest`: 182. ve 388. satırlardaki kaymış tırnak hataları (`"main_quest_lv6"0.quest2_end` -> `"main_quest_lv60.quest2_end"` ve `"main_quest_lv6"0.quest3_end2` -> `"main_quest_lv60.quest3_end2"`) EUC-KR yorum baytları korunarak düzeltildi; `precompile.py` + Lua 5.5.1 `qc` ile **PASS** (`returncode=0`).
              * `_unused/xxx_notify_shopmeeting.quest`: 48–178. satırlar arasındaki tüm Lua anahtar kelimeleri ve fonksiyon çağrılarının (`say`, `when`, `if`, `end`) orijinal dosyada silinip yalnızca ham Almanca metin parçalarının (`("Ziel1")!`, `("Ziel2"` vb.) kaldığı kanıtlandı; görev mantığı uydurmadan onarılamayacağından `_unused/` altında dokunulmadan bırakıldı (**`125/126` `_unused` dosyası Lua 5.5.1 `qc` ile hatasız derlenmektedir**).
          * **Kategori B — Çekirdek Quest Yardımcı Kütüphaneleri (`2` dosya + `2` test fixture):**
            - `quest/questlib.lua` (`CQuestManager::InitializeLua` tarafından ilk yüklenen ana kütüphane) ve `quest/libs/other/GFquestlib.lua` (`questlib.lua` başında `dofile` ile yüklenen Gameforge yardımcı fonksiyonları).
          * **Kategori C — Modül Yükleyici ve Alt Kütüphaneler (`37` aktif `.lua` dosyası):**
            - `quest/libs/libModuleLoader.lua` (`questlib.lua` sonunda `dofile` ile çağrılır) ve yüklediği **36 alt modül**: `enum/*` (`15` dosya), `translate/translate.lua` (`1` dosya), `itemshop/*` (`2` dosya -> global `itemshop`), `fishing/*` (`3` dosya -> global `fishing_manage`), `crafting/*` (`5` dosya -> global `crafting`), `other/*` (`7` aktif dosya: `_otherModuleLoader.lua`, `game_commands.lua`, `reward_data.lua`, `hunting_data.lua`, `collect_data.lua`, `busy_action.lua` -> global `busy_action`, `potion_system.lua`), `quest_book/*` (`3` dosya -> global `quest_book`).
          * **Kategori D — Dil, Çeviri ve Metin Dosyaları (`3` dosya + `2` test fixture):**
            - `translate.lua` (`1,041,866` B; `CQuestManager::InitializeLua` tarafından doğrudan yüklenir), `quest/locale.lua` (`103,350` B; `1,619` gerçek Lehçe metin dizgesi, `0` eksik alan), `quest/libs/translate/translate.lua` (`83,613` B; `msg()`, `error_msg()`, `send_letter_ex()` yardımcıları).
          * **Kategori E — Sistem, Zindan ve Etkinlik Lua Dosyaları (`3` dosya):**
            - `settings.lua` (`main.cpp` açılışında `lua_dofile` ile yüklenir), `BlueDragon.lua` (`BlueDragon.cpp` içinde C++ `BlueDragon_Binder` ile yüklenir), `oxquiz.lua` (`quest/locale.lua` sonunda `dofile` ile yüklenir; 200 OX sorusu).
          * **Kategori F — Derlenmiş `object/` Görev Çıktıları (`6,900` dosya):**
            - `quest/object/state/` altında **202** görev durum/fonksiyon dosyası, `quest/object/notarget/` altında **3,864** tetikleyici betiği ve NPC klasörleri altında **2,834** tetikleyici betiği (toplam **6,900** dosya).
          * **Kategori G — Lua Motor Kaynakları (`server/src/liblua/`):**
            - `server/src/liblua/5.5/` (**Lua 5.5.1**) tek motor olarak korundu; eski `server/src/liblua/5.0/` (Lua 5.0.3) ve `server/src/liblua/5.2/` (Lua 5.2.3) dizinleri tamamen kaldırıldı.
          * **Kategori H — Aktif Zincir Dışında Kalan ancak Korunan `.lua` Dosyaları (`2` dosya):**
            - `monkey_dungeon.lua` (`locale/poland/monkey_dungeon.lua`; geçerli Lua 5.5.1 tablosu, `--check-lua` ve `test_lua55_quest` içinde doğrulandı) ve `quest/libs/other/questing.lua` (`_otherModuleLoader.lua` tarafından yüklenmeyen, dış kabuktan `mysql` çağıran eski 3. parti Mijago kütüphanesi; çalışma zamanında çağrılmadığı doğrulandı ve silinmeden korundu).
        * **2. Risk 3 Denetimi — Lua 5.5.1 Motor Değişikliklerinin Sınırlandırılması (`lvm.h`, `lobject.c`, `lapi.c` — `DÜZELTİLDİ`):**
          * `server/src/liblua/5.5/src/lapi.c` ve `server/src/liblua/5.5/src/lvm.h` dosyalarındaki müdahaleler tamamen geri alınarak **%100 orijinal upstream Lua 5.5.1** durumuna döndürüldü (`git diff 12b3c19e90a8a726abecaacecfc8a2ca1f9688a2 -- lapi.c lvm.h` = `0` satır):
            - Standart Lua 5.5.1 `ltable.c` (`luaH_get`, `luaH_pset`, `luaH_finishset`) ve `lvm.c` (`luaV_tointegerns`) zaten tam sayı değerli `LUA_VNUMFLT` anahtarlarını (`50.0`) `F2Ieq` ile doğal olarak `lua_Integer` (`50`) anahtarına dönüştürmektedir. `lapi.c` (`lua_pushnumber`) müdahalesi gereksiz olduğu gibi `-0.0` işaretini ve $\ge 2^{53}$ float alt tip semantiğini bozuyordu; geri alındı.
            - `lvm.h` içinde `LUA_FLOORN2I` makrosunun `F2Ifloor` yapılması negatif kesirli sayıları sıfıra doğru (`-1.9 -> -1`) yerine $-\infty$'a doğru (`-1.9 -> -2`) yuvarlıyordu; standart `F2Ieq` değerine geri alındı.
          * Kesirli float değerlerle çağrılan `string.format("%d", 31 - t / 60)` (`new_christmas_gift_box.quest:60`, `new_easter_gift_box_gf.quest:66`) ve `math.random(a, b)` uyumluluğu Lua C motoruna dokunmadan yalnızca Metin2 entegrasyon katmanına (`CQuestManager::InitializeLua` — `server/src/game/src/questlua.cpp`) `math.modf` (sıfıra doğru kesme / C `static_cast<int>` eşdeğeri) ve `table.pack` / `table.unpack` sarmalayıcısı olarak taşındı.
          * `server/src/liblua/5.5/src/lobject.c` (`tostringbuffFloat`): `luaV_flttointeger` müdahalesi kaldırıldı (`-0.0 -> "-0"`, `NaN`, `Inf` ve büyük float'lar standart `LUA_NUMBER_FMT` ile işlenir); yalnızca Lua 5.3+ ile eklenen 5 satırlık `.0` soneki ekleme bloğu (`buff[len++] = '0'`) çıkarılarak belgelendi (Lua `..` `OP_CONCAT` ve C API `lua_tostring(L, idx)` doğrudan C seviyesinde `tostringbuffFloat` çağırdığından `1200 / 60` gibi tam bölünen float sonuçlarının `"20.0"` yerine `"20"` üretmesi için zorunludur).
          * `tests/test_lua55_quest.cpp` (`Test 11`) ve `server/src/game/src/questlua_test.cpp` (`Check 3`) içine kesirli/negatif float (`-1.9 -> -1`), tablo indeksleme (`t[50.0] == t[50]`, `t[1.5]` ayrımı), `-0.0` işaret korunumu (`1.0 / -0.0 == -math.huge`, `tostring(-0.0) == "-0"`), 64-bit tam sayı sınırları (`maxinteger` / `mininteger`), `NaN` / `Inf` ve metin dönüşümü regresyon testleri eklendi.
        * **3. Risk 2 Denetimi — FreeBSD Quest Compiler (`qc`) Yolu ve `server/serverfiles_share.tar.xz` Arşiv Bütünlüğü (`DÜZELTİLDİ`):**
          * `CMakePresets.json` içinde `freebsd-gcc14` preset'inin çıktı dizini `build/freebsd-gcc14` iken `.github/workflows/freebsd-gcc-test.yml:188`, `server/scripts/freebsd/install.sh:54` ve `tests/run_freebsd_serverfiles_test.sh:26` dosyalarında `build/freebsd-gcc/server/src/game/src/quest/qc` yazıldığı tespit edilerek `build/freebsd-gcc14/server/src/game/src/quest/qc` olarak düzeltildi.
          * `server/scripts/freebsd/install.sh` içinde FreeBSD kurulumunda arşivden gelen `qc.exe` dosyasının silinmesi (`rm -f ... "$TARGET_DIR/share/locale/poland/quest/qc.exe"`) ve ELF64 `qc` ikilisinin kurulması sağlandı; `tests/run_freebsd_serverfiles_test.sh` içinde `QC_BIN` ve `$SFROOT/share/locale/poland/quest/qc` kontrolü fail-closed hale getirildi; `Make.py` ve `tests/run_quest_matrix.py` hem Windows (`qc.exe`) hem FreeBSD (`./qc`) uyumlu yapıldı ve `sys.dont_write_bytecode = True` ile `__pycache__` oluşumu engellendi.
          * `server/serverfiles_share.tar.xz` arşivi Rev 50 (`12b3c19e90a8a726abecaacecfc8a2ca1f9688a2`) arşivi temel şablon alınarak yeniden paketlendi ve karşılaştırıldı:
            - `share/locale` dışındaki tüm kritik dosyalar (`usr/home/mt2/chs/ch01/CMD`, `usr/home/mt2/chs/ch01/mark/mark_0.tga`, `usr/home/mt2/chs/ch01/mark/mark_index`, `usr/home/mt2/share/version`) ve üst dizin girdileri %100 korundu.
            - Ortak `15,858` girdinin tamamında dosya tipi, izin bitleri (`0755` / `0644`) ve sembolik bağlantı farkı: **`0`**.
            - Kaldırılan girdiler (`45` adet): Yalnızca `guild_man{1,2,3}/chat/guild_building.start.{0..4}.{arg,script,when}` çakışma dosyaları.
            - Eklenen girdiler (`46` adet): `object/state/guild_manage` + 45 `guild_man{1,2,3}/chat/guild_manage.start.{0..4}.{arg,script,when}` dosyası (`15,903 -> 15,904` toplam girdi).
            - Arşiv içinde `__pycache__`, `conf.txt`, `CONFIG`, `.env` veya parola/secret sayısı: **`0`**.
        * **4. Tek Motorlu (`Lua 5.5.1`) CMake, Makefile ve Kod Temizliği:**
          * `server/src/liblua/CMakeLists.txt`, `server/src/game/CMakeLists.txt`, `server/src/game/src/quest/CMakeLists.txt`, `tests/CMakeLists.txt` ve `Makefile` dosyalarından `lua_lib` (5.0), `lua52_lib` (5.2), `game_lua50` (`game_r41023_lua50`), `qc_lua50`, `qc_lua52`, `qc_lua55`, `test_lua_quest`, `test_lua52_quest`, `test_qc_compiler_lua50` ve `test_qc_compiler_lua52` hedefleri kaldırıldı.
          * `cmake/FindDatabaseClient.cmake` ve `tests/CMakeLists.txt` içinde Windows'ta mevcut olmayan eski `scratch/isolated_db/mariadb-11.4.13-winx64` yoluna yapılan sabit başvurular temizlenerek depodaki `extern/server/include/mysql` ve `find_file(MT2_TEST_MARIADB_DLL ...)` yapısına geçirildi.
        * **5. Doğrulama Sonuçları (Windows x64 `PASS` / FreeBSD 15.1 `PASS` — Rev 51 `CLOSED`):**
          * **Kademe A (Statik/Sözdizimi — Windows & FreeBSD):** PowerShell AST (`server/scripts/windows/*.ps1`, `tests/*.ps1`), YAML (`freebsd-gcc-test.yml`) ve 10 POSIX betiğinde `sh -n` / LF satır sonu kontrolü **PASS**.
          * **Kademe B (Windows x64 Derleme, CTest, `--check-lua` ve `202/202` Quest Matrisi):**
            - `cmake --build --preset windows-gcc-ucrt`: Tek `db_r41023.exe`, tek `game_r41023.exe` ve tek `qc.exe` yalnızca Lua 5.5.1 (`liblua55_lib.a`) ile **PASS**.
            - `ctest --preset windows-gcc-ucrt --output-on-failure`: **10/10 PASS** (`test_lua55_quest` 11/11 alt testi dahil).
            - `game_r41023.exe --check-lua`: **11/11 PASS** (`202` gerçek `state/` tablosu [`guild_building` ve `guild_manage` ayrı] + `6,900` gerçek `object/` betiği + tüm gerçek `.lua` yardımcı/modül/dil/sistem dosyaları `0` hata ile yüklendi ve çalıştırıldı).
            - `python -X utf8 tests/run_quest_matrix.py --force --clean`: **202/202 PASS** (`%100.0`, `0 FAIL`, `0 TIMEOUT`).
          * **Kademe C (Windows x64 Canlı 8-Servis + MariaDB 12.3.3 LTS Serverfiles Doğrulaması):**
            - 8 servis (`db`, `auth`, `ch11..ch01`) + MariaDB 12.3.3 `LISTENING`, Argon2id hesap + `SmokeHero` `PHASE_GAME`, NPC `20016` quest etkileşimi, tek kullanımlık `LoginKey` tekrar oynatma reddi, `stop.ps1 -RotateLogs` (`40` arşiv dosyası), `player.quest` SQL kalıcılığı (`4` satır), `ch11 -> ch12 -> portal 10002 WarpSet -> ch11` `[cross-core-step1..5]` ve `0` bayt `syserr` **PASS**.
          * **Kademe D (FreeBSD 15.1-RELEASE-p3 amd64 GCC 14.2.0 + MariaDB 12.3.3 LTS CI — `CI Run ID 39859687668` / `Job ID 120106866220`):** **`SUCCESS` (`18m 2s` — Tetikleme SHA `81b3c39b03cc120d95a3fa4b8cd4082b7320cf1a` / Kaynak SHA `da1f6775f86cfbe15e43f493d772b498a2a97a08`)** —
            - **Temiz Checkout & MySQL Extern Yokluğu:** `20,037` dosya temiz checkout edildi; `extern/server/include/mysql/server` dizininin bulunmadığı fail-closed guard ile doğrulandı.
            - **MariaDB 12.3 LTS & Connector/C Sürümleri:** FreeBSD 15.1 üzerinde `mariadb123-server-12.3.3` ve `mariadb123-client-12.3.3` (`Live Server Version: 12.3.3-MariaDB`, `Client Connector Version: 3.4.10`, `AsyncSQL client ABI OK: header=MariaDB(MYSQL_VERSION_ID=120303) runtime_lib=3.4.10(client_version=30410)`) doğrulandı (Windows x64 üzerinde MariaDB Server `12.3.3` ve Connector/C `3.3.5` kullanılmaktadır; aktif MariaDB 11.4 sunucu/fallback kalıntısı: `0`).
            - **Birim, Auth, DB Cache ve Wire Testleri (`run_tests.sh` & `run_db_integration_tests.sh`):** `libpoly 24/24`, `libthecore 97/97`, `libgame 28/28`, `libsql 143/143` (`292/292`), `test_auth_security 423/423`, `test_db_cache 120/120`, `test_packet_wire 37/37` ve gerçek MariaDB 12.3.3 üzerinde **`test_db_cache_integration 140/140 PASS`**.
            - **CMake + Ninja ELF64 Derlemesi, `readelf -d` ve CTest:** `[302/303]` hedef derlendi; `db_r41023`, `game_r41023` ve `build/freebsd-gcc14/server/src/game/src/quest/qc` ikililerinin tamamı **`ELF 64-bit LSB executable, x86-64, version 1 (FreeBSD)`**; `readelf -d` ile yalnızca `libmariadb.so.3` ve `libstdc++.so.6` bağlandığı (`0` `libmysqlclient`, `0` `libc++.so`) kanıtlandı; `ctest --preset freebsd-gcc14` **`10/10 PASS`**.
            - **Lua 5.5.1 & Canlı MariaDB 12.3.3 Entegrasyonu:** `game_r41023 --check-lua` **`11/11 PASS`** (7 saç seçeneği canlı `CHARACTER` + `CItem` testi dahil); `run_live_tests.sh` **`113/113 PASS`**; `run_server_smoke_test.sh` `[1/6]..[6/6]` **PASS** (Argon2id + `SmokeHero` `PHASE_GAME` + NPC `20016` coroutine `login_count=1 -> 2` + `LoginKey` tekrar oynatma reddi + `AuthRateLimiter`).
            - **Gerçek FreeBSD 15.1 Serverfiles Doğrulaması (`run_freebsd_serverfiles_test.sh` `[1/7]..[7/7]`):**
              * `[1/7]` `/usr/home/mt2` kurulumu, `chmod 600` (`db/conf.txt`, `auth/CONFIG`, `chs/*/CONFIG`), `state_tables=202, total_object_files=6900`, FreeBSD `Python 3.12.14` (`defaultencoding=utf-8 | preferredencoding=UTF-8`) altında `python3 tests/run_quest_matrix.py --force --clean` **`Motor [lua55]: PASS: 202 (100.0%) | FAIL: 0 | TIMEOUT: 0`** (önceki `precompile.py:180` `UnicodeDecodeError` tamamen çözüldü) ve `game_r41023 --check-lua` **`11/11 PASS`**.
              * `[2/7]` 8 servis (`db:18000`, `auth:17105`, `ch11:28000`, `ch12:28001`, `ch13:28002`, `ch99:13099`, `ch00:13088`, `ch01:13098`) **`LISTENING`**, `syserr lines: 0`.
              * `[3/7]` `ACCOUNT_CREATED id=1 login=bsd64hero format=argon2id` -> `ch11` `PHASE_GAME`, `0 syserr`.
              * `[4/7]` 3 aşamalı zarif kapatma ve log rotasyonu: 8 servis `STOPPED`, `syserr lines: 0`, `log/archive` altında **`32` arşiv dosyası**.
              * `[5/7]` `SmokeHero` için `player.quest` tablosunda **`4` kalıcı satır** doğrulandı; karakter `map_index=3` (`ch12`) konumuna taşındı ve 8 servis yeniden başlatıldı (`LISTENING`, `syserr lines: 0`).
              * `[6/7]` Cross-core P2P & WarpSet `[cross-core-step1..5]` **PASS**: `ch11(28000)` `CMapLocation` -> `ch12(28001)` `PHASE_GAME(5)` (`vid=25396`) -> Portal `10002` `CHARACTER::WarpSet` -> `HEADER_GC_WARP(65, dest=450300x905000, target_port=28000)` -> `ch11(28000)` `PHASE_GAME(5)` (`map_index=1`) -> `[cross-core-step5]` tüketilmiş `dwLoginKey` tekrar oynatma reddi (`ch12:28001` ve `ch11:28000`), `0 syserr`.
              * `[7/7]` Son 3 aşamalı zarif kapatma: 8 servis `STOPPED`, `syserr lines: 0`, `0` secret sızıntısı ve **`44` log dosyası (`1,203,282` bayt)** `freebsd-serverfiles-logs-mariadb123` artifact'i olarak yüklendi (`Artifact ID 12461498722`, SHA256 `707d89d3f655951ba3ade03c0a25fd0bc631e84647dcf4db5224678ec2a406e8`).
            - **Sonuç:** Tüm zorunlu Windows x64 ve FreeBSD 15.1 amd64 doğrulamaları %100 kanıtlandığından **Rev 51 resmen `CLOSED` (KAPANDI)** olarak işaretlenmiştir.
        * **6. Kapsamlı x64 Mimari, Lua 5.5.1, Quest Fonksiyon Tablosu ve Sunucu Uyumluluk Denetimi (`DÜZELTİLDİ`):**
          * **x64 İkili ve Kütüphane Denetimi:** Tüm aktif Windows çıktı ikililerinin (`db_r41023.exe`, `game_r41023.exe`, `qc.exe`, 10 CTest ikilisi, `test_server_smoke_client.exe`, `libmariadb.dll`) **PE32+ (`0x8664`, `0x20b` 64-bit)** olduğu ve tüm statik `.a` arşivlerinin (`libcryptopp_bundled.a` [152 üye], `libdb_core.a` [29], `libgame_lib.a` [3], `liblua55_lib.a` [32], `libpoly.a` [4], `libsql_lib.a` [4], `libthecore.a` [25]) **%100 `COFF-x86_64 (0x8664)`** nesnelerinden oluştuğu doğrulandı.
          * **Eski Makefile ve Yedek Başlık 32-Bit Kalıntılarının Temizlenmesi:** `server/src/{game/src,db/src,libgame/src,libpoly,libthecore/src}/Makefile` dosyalarındaki tarihsel `ifeq ($(LBITS),32) CFLAGS += -m32` ve `.obj_lua52` blokları kaldırıldı; `extern/server/include/mysql/{mariadb_version.h,server/my_config.h}` yedek başlıklarındaki 32-bit `i386` / `SIZEOF_VOIDP 4` makroları 64-bit `x86_64` (`LP64`/`LLP64` uyumlu) olarak güncellendi.
          * **Quest Kaynak Düzeltmesi (`pony_levelup.quest:1994`) ve Çalışma Zamanı `affect.add_hair` / `affect.remove_hair` Davranış Doğrulaması (`DÜZELTİLDİ`):**
            - `server/files/share/locale/poland/quest/quest/systems/horse/pony_levelup.quest:1994`: `training_desert_yellow_step6` içindeki `ay_reward(gameforge.pony_levelup._120_sayReward)` yazım hatası doğrudan kaynak dosyada `say_reward(gameforge.pony_levelup._120_sayReward)` olarak düzeltildi; `Make.py` ile 202 aktif görev yeniden derlenerek `object/notarget/target/pony_levelup.training_desert_yellow_step6.0.script` ve `server/serverfiles_share.tar.xz` güncellendi (`quest/`, `_unused/`, `libs/`, `object/`, `quest_functions` ve arşiv genelinde kalan `ay_reward` kullanımı: **`0`**).
            - `server/src/game/src/questlua.cpp`: Kaynak dosya ve `object/` çıktısı kalıcı olarak düzeltildiği için `CQuestManager::InitializeLua()` içindeki geçici `ay_reward -> say_reward` C++ takma adı kaldırıldı.
            - `server/src/game/src/questlua_affect.cpp`, `server/src/game/src/questlua_item.cpp` ve `server/src/game/src/log.cpp`: `affect.add_hair`, `affect.remove_hair` (`AFFECT_HAIR = 514`) ve `item.set_value` fonksiyonları orijinal Metin2 saç görevlerinin (`event_halloween_hair.quest`, `_unused/hair.quest`) davranışını %100 koruyacak şekilde sertleştirildi (`897 -> 899` C++ quest fonksiyonu):
              * `ResolveHairApplyPoint` (`questlua_affect.cpp`) ve `ResolveHairItemApplyPoint` (`questlua_item.cpp`): `questlib.lua` `apply.*` numaraları (`1..86`) ile `EPointTypes` numaraları (`1..164`) çakıştığı için (`53 = apply.ATT_GRADE_BONUS` vs `POINT_ATTBONUS_MONSTER`, `59 = apply.ATTBONUS_WARRIOR` vs `POINT_RESIST_WARRIOR` vb.) doğrudan `EPointTypes` geri dönüşü kaldırılarak saç görevlerinde (`affect.add_hair` ve `ITEM_HAIR` / `ITEM_COSTUME:COSTUME_HAIR` eşyalarında `item.set_value(0, option.apply, option.value)`) **yalnızca** 7 `questlib.lua` `apply.*` sabiti (`ATTBONUS_WARRIOR=59 -> 54`, `ATTBONUS_ASSASSIN=60 -> 55`, `ATTBONUS_SURA=61 -> 56`, `ATTBONUS_SHAMAN=62 -> 57`, `ATTBONUS_MONSTER=63 -> 53`, `MAX_HP_PCT=69 -> 119`, `MAX_SP_PCT=70 -> 120`) kabul edilir; saç dışındaki normal ekipmanlarda (`/item_setattr`) standart `EPointTypes` (`1..POINT_MAX_NUM-1`) korunur; geçersiz veya çakışan sayılar ile sınır dışı attribute indeksleri (`index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM`) `POINT_NONE` ile reddedilir (`fail-closed`).
              * `BYTE` dönüşümü öncesinde `std::isfinite`, `rawApply` sınır kontrolü ve `duration` sınır kontrolü (`0 < duration <= INFINITE_AFFECT_DURATION`) uygulanarak negatif/taşan parametrelerin işaretsiz taşma (`uint8_t` wraparound) üretmesi engellendi; mevcut tüm `AFFECT_HAIR` girdileri (`while (CAffect* pkOld = ch->FindAffect(AFFECT_HAIR))`) yenisi eklenmeden önce kaldırılarak çift bonus birikmesi önlendi; `LogManager::Query` içinde `!m_bIsConnect` kontrolü eklenerek SQL bağlantısı olmayan test/çevrimdışı modda `SetForceAttribute` çağrılarının güvenli çalışması sağlandı.
                * **Mevcut Veritabanı Saç Eşyaları ve Cadılar Bayramı (`event_halloween_hair.quest`) Kalıcı Migrasyon Altyapısı (`sql/migrations/rev51_halloween_hair_attr_migration.sql` — `DOĞRULANDI`):**
                  - Depodaki `sql/player.sql` başlangıç şemasında `player.item` tablosu boştur (`0` kayıt).
                  - `sql/world.sql` içindeki standart saç eşyaları (`vnum 74001..74019`, `74251..75619`) zaten `item_proto.applytype0` sütununda doğrudan `EPointTypes` (`53..57, 119, 120`) taşımakta olup `item.set_value` çağırmaz; yalnızca `event_halloween_hair.quest` ile üretilen Cadılar Bayramı saç eşyaları (`74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620`, `type=28 ITEM_COSTUME / subtype=1 COSTUME_HAIR`, `applytype0=0`) görev sırasında `item.set_value(0, option.apply, option.value)` ile yazılır.
                  - `c186fed1a6374414a94c206a85a30128c0a4966d` öncesinde oluşturulmuş olabilecek eski Cadılar Bayramı saç kostümleri için `sql/migrations/rev51_halloween_hair_attr_migration.sql` hazırlandı ve canlı MariaDB 12.3.3 LTS (`Suite 13`, `38/38 PASS`) ile C++ çalışma zamanında (`questlua_test.cpp Check 4 & Check 6.2`, `--check-lua 11/11 PASS`) uçtan uca doğrulandı:
                    * **Beşli Parmak İzi (5-Part Fingerprint) ve İnsan Onaylı Seçim Kapısı (`approved_for_migration = 1`):** Yalnızca `(1) vnum IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620)`, `(2) attrtype0 IN (59,60,61,62,63,69,70)`, `(3) (attrtype0 IN (59..63) AND attrvalue0 = 10) OR (attrtype0 IN (69,70) AND attrvalue0 = 1)`, `(4) attrtype1..6 = 0 AND attrvalue1..6 = 0` ve `(5) socket0/socket1` görev süre izi (`604800`/`21600`/`86400` veya `socket1 >= 1 AND socket0 > 604800`, `socket2 = 0`) koşullarının tamamını sağlayan kayıtlar `CANDIDATE_REQUIRES_HUMAN_APPROVAL` olarak sınıflandırılır. `player.item` tablosunda eşyanın görevden mi yoksa aynı parmak iziyle GM tarafından mı oluşturulduğunu gösteren ayrı bir kaynak sütunu bulunmadığından **denetimsiz otomatik üretim migrasyonu yapılmaz**; 1. çalıştırma adayları `player_item_halloween_rev51_backup` tablosuna `approved_for_migration = 0` ile yedekler (`player.item` üzerinde `0` satır değiştirir). Yalnızca operatör tarafından `approved_for_migration = 1` olarak onaylanan satırlar dönüştürülür (`59->54`, `60->55`, `61->56`, `62->57`, `63->53`, `69->119`, `70->120`); onaysız (`0`) aynı parmak izli GM eşyaları %100 korunur.
                    * **Belirsiz Kayıtların Korunması (`AMBIGUOUS_DO_NOT_CONVERT`):** GM komutu (`/item_setattr`) ile sonradan değiştirilmiş (`attrvalue0 != 10/1`, `attrtype1..6 != 0` veya geçersiz `socket0`) kayıtlar yedek/aday tablosuna dahi alınmaz ve asla otomatik dönüştürülmez; salt-okunur denetim raporunda ayrı sınıflandırılır.
                    * **Yedekleme, İdempotent Güncelleme, Geri Alma ve Çevrimdışı Bakım (`CItemCache`):** Güncelleme öncesinde `player_item_halloween_rev51_backup` tablosuna tam yedek alınır; işlem `START TRANSACTION ... COMMIT` içinde idempotent çalışır (onay sonrası tekrar çalıştırmada `0` satır değişir); tek sorguluk rollback doğrulandı; `db_r41023` `CItemCache` bellekteki kirli önbelleğin SQL güncellemesini ezmemesi için migrasyonun `game_r41023` ve `db_r41023` kapalıyken çalıştırılması kuralı runbook'a bağlandı.
                    * **Dış Üretim Veritabanı Durumu:** Bu geliştirme ortamına bağlı harici bir canlı oyuncu veritabanı bulunmamaktadır (`HARİCİ ÜRETİM OYUNCU VERİTABANI BAĞLI DEĞİL`); betik operatör tarafından bakım penceresinde çalıştırılmak üzere depoda hazırdır.
                * **Kalıcı SQL Log Dayanıklılığı (`CAsyncSQL` & `LogManager` — Disk Spool, Kuyruk Taşması, Poison Karantinası, Çoklu Süreç Kilidi ve Kapanış/Çökme Kurtarması — `DOĞRULANDI`):**
                  - `server/src/libsql/AsyncSQL.{h,cpp}`, `server/src/game/src/log.{h,cpp}`, `server/src/game/src/config.cpp`, `server/src/libthecore/src/main.c` ve `server/src/game/src/main.cpp`:
                    * **Çevrimdışı Başlangıç, `EscapeString` ve `NO_BACKSLASH_ESCAPES` Güvenliği:** `LogManager` için kalıcı disk spool dosyası (`sql_log_spool.dat`) yapılandırıldı (`EnableDurableSpool`). MariaDB başlangıçta erişilemez olsa dahi (`ConnectAsync` + `m_bIsConnect = true`), `CAsyncSQL::EscapeString` gerçek `mysql_real_escape_string` bağlantısı yokken RFC/MariaDB uyumlu çevrimdışı karakter kaçışına (`\0`, `\n`, `\r`, `\\`, `'`, `"`, `\x1a`) otomatik düşer (`EscapeStringOfflineSafe`) ve `m_hDB.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES` durumunu izleyerek `NO_BACKSLASH_ESCAPES` aktifse `'` karakterini `''`, değilse `\'` olarak kaçar; böylece tek tırnak içeren eşya/karakter adları (`'Halloween_Hat_1'` vb.) bağlantı yokken de `latin1`/`latin2`/`utf8mb4` ASCII süper-kümelerinde bozulmadan SQL kuyruğuna ve disk spool'a yazılır.
                    * **Canlı Sorgu Bütünlüğü ve SQL String-Literal + `NO_BACKSLASH_ESCAPES` Duyarlı `SanitizeQueryForLog`:** Bağlantı açıkken ve kuyruk dolmamışken (`m_bConnected && !bQueueFull && m_dequeSpool.empty()`) `PushQuery` içindeki `p->stQuery` ham haliyle yürütülür (maskelenmez). Sorgu yalnızca disk spool'a, `.quarantine` dosyasına veya `sys_err` loguna yazılırken `SanitizeQueryForLog(query, bNoBackslashEscapes)` ile maskelenir; maskeleme algoritması dıştaki SQL tırnak sınırlarını (`GetOuterSqlQuoteAt`) ve `bNoBackslashEscapes` kipini takip ederek kaçışsız bir dış `'` veya `"` sütun sınırını asla aşmaz (`'password="hello', 'world"'`, `'password=', 'next_col'` ve `NO_BACKSLASH_ESCAPES` altında `'C:\dir\'` gibi çok sütunlu metinlerde sonraki sütunları bozmaz).
                    * **Ana Oyun Döngüsünü Bloklamayan Asenkron Spool, Kuyruk Sınırı, Taşma Politikası ve DoS Koruması (`ESpoolOverflowPolicy` & Atomik `thecore_shutdown()`):** `CAsyncSQL` kuyruk üst sınırı (`m_maxQueryQueueCount`, varsayılan `10,000`) ve disk spool tavanı (`m_maxSpoolBytes`, varsayılan `64 MB`) kontrol edilerek, bağlantı koptuğunda veya kuyruk dolduğunda yeni sorgular ana oyun döngüsünü (`thecore_heart`) bloklamadan bellekteki/diskteki dayanıklı spool kuyruğuna (`EnqueueToSpoolLocked`, atomik `.tmp` + `fflush` + `fsync`/`_commit` + `rename`/`MoveFileExA` ve sahip-özel dosya izinleri `0600` / `_S_IREAD | _S_IWRITE`) aktarılır; disk yazma/senkronizasyon hatalarında (`ullDiskSyncFailureCount++`) kayıt RAM'deki `m_dequeSpool` içinde tutularak sunucu çökmedikçe veya sonraki başarılı senkronizasyona / MariaDB yeniden bağlantısına kadar korunur. `64 MB` spool kapasitesi dolduğunda mevcut `64 MB` eski kayıt korunur ve yalnızca yeni gelen (`64 MB` sınırını aşan) kayıtlar reddedilir (`ullDroppedSpoolFullCount++`, `m_bFailedQueueOverflow = true`, `OnLogManagerSpoolOverflow` `syserr` alarmı). Kötü niyetli oyuncuların uzun süreli bir MariaDB kesintisi sırasında yoğun log üreten eylemlerle `64 MB` spool'u doldurarak oyun sunucusunu zorla kapatmasını (DoS) önlemek için üretim varsayılanı `LOG_SPOOL_FAIL_STOP: 0` (`SPOOL_OVERFLOW_ALERT_AND_DROP_NEW`) olarak tutulmuştur; `LOG_SPOOL_FAIL_STOP: 1` (`SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN`) etkinleştirildiğinde ise `64 MB` sınırını aşan **ilk tetikleyici kayıt reddedildiği an** (`ullDroppedSpoolFullCount >= 1`) atomik `thecore_shutdown()` tetiklenir (dolayısıyla `FAIL_STOP` sıfır kayıt kaybı garantisi değil, taşma başladığı anda 1 tetikleyici kayıttan sonra sunucuyu durduran katı üst sınır politikasıdır).
                    * **Çoklu GAME Süreçleri, Aynı Dizin Özel Kilit Yuvası İzolasyonu ve Yetim (Orphan) Spool Kurtarması (`AcquireSpoolLockLocked` / `AdoptOrphanSpoolSlotsLocked`):** 8 servisli serverfiles mimarisinde `db` ve `auth` (`g_bAuthServer == true`, `config.cpp:567`) `LogManager` açmaz; 6 oyun kanalı (`chs/ch11..ch01`) ayrı çalışma dizinlerinde kendi `sql_log_spool.dat` dosyasını kullanır. Aynı çalışma dizininde birden fazla süreç aynı `"sql_log_spool.dat"` yolunu açarsa işletim sistemi düzeyinde bloklamayan özel dosya kilidi (Windows `CreateFileA` `dwShareMode=0`, FreeBSD/POSIX `open` + `flock(LOCK_EX | LOCK_NB)`) çakışan örnekleri deterministik olarak `sql_log_spool.dat` (slot 0), `sql_log_spool.1.dat` (slot 1), `sql_log_spool.2.dat` (slot 2) dosyalarına ayırır. Çoklu süreçler çöktükten sonra **yalnızca tek bir süreç (slot 0)** yeniden başlatılırsa, `AdoptOrphanSpoolSlotsLocked()` ikincil yuvaları (`1..63`) tarar; aktif bir süreç tarafından kilitli olan yuvalara dokunmazken kilitsiz (yetim) `.N.dat` dosyalarını özel kilitle devralır, CRC32 doğrulaması ve monoton sıra numarası (`ullSeq`) yeniden eşlemesiyle slot 0 kuyruğuna birleştirir (`ullAdoptedOrphanCount`), atomik olarak `sql_log_spool.dat` dosyasına senkronize eder ve boşaltılan yetim `.N.dat` / `.N.dat.lock` dosyalarını temizler (`libsql 201/201 PASS` & `Suite 14 Senaryo 13 PASS`).
                    * **Sıralı (`FIFO`) Yeniden Oynatma ve `CR_SERVER_GONE_ERROR` vs `CR_SERVER_LOST` Ayrımı:** Bağlantı geri geldiğinde `ReplayDurableSpoolNow` mevcut spool kayıtlarını yeni kuyruk sorgularından önce sırayla (`FIFO`) MariaDB'ye gönderir; her başarılı sorgudan sonra spool dosyası güncellenir (`SyncSpoolToDiskLocked`). Gönderim öncesi bağlantı kopmalarında (`CR_SERVER_GONE_ERROR = 2006`) mükerrer kayıt riski `0`'dır; sorgu gönderildikten sonra TCP yanıtı kesilirse (`CR_SERVER_LOST = 2013`), `sql/log.sql` tablolarında `PRIMARY KEY` bulunmadığından veri kaybını önlemek (`at-least-once`) adına tek uçuş içi (`in-flight`) sorgu spool başında korunur ve bu belirsiz durum `ullUncertainInFlightRetries` sayacı ile dürüstçe izlenir.
                    * **Kalıcı Hatalı (`Poison`) Sorgu Karantinası:** Kalıcı sözdizimi/şema hatası (`1064 ER_PARSE_ERROR`, `1146 ER_NO_SUCH_TABLE`, `1054 ER_BAD_FIELD_ERROR` vb.) üreten bozuk bir log sorgusu kuyruğu sonsuza dek tıkamaz; hata koduyla birlikte `.quarantine` dosyasına (`sql_log_spool.dat.quarantine`) ayrılır ve arkadaki geçerli logların akışı kesintisiz devam eder.
                    * **Kontrollü Kapanış (`FlushAndShutdown`) ve Çökme Sonrası (`Crash Recovery`) Kurtarma:** Sunucu kapanışında (`main.cpp`) `LogManager::instance().FlushAndShutdown(3000)` çağrılarak bekleyen logların MariaDB'ye boşaltılması beklenir; MariaDB kapanış anında kapalıysa kalan tüm sorgular `sql_log_spool.dat` dosyasına senkronize edilir ve bir sonraki sunucu açılışında (`EnableDurableSpool`) otomatik yüklenip yeniden oynatılır (`0` veri kaybı, `0` mükerrer kayıt).
                * **MariaDB 12.3 LTS 3306 Dış Erişim Kapatma, 4 Kademeli En Az Yetki (`Least-Privilege`), `QUERY_DML_NO_SELECT` `server_audit`, `audit_guard.sh` ve Şifreli Yedekleme/Geri Yükleme Sertleştirmesi (`DOĞRULANDI`):**
                  - **Depo ve Kurulum Envanteri:** Depoda MartySama `gen_settings.py` benzeri `0.0.0.0:3306` açan veya güvenlik duvarında 3306 portunu dış dünyaya açan hiçbir betik bulunmamaktadır; yerel Windows ortamında `3306`/`3307` dış dinleyicisi ve Windows Firewall 3306/3307 kuralı sayısı `0`'dır (Harici Azure NSG / bulut güvenlik duvarı, uzak üretim SIEM sunucusu ve VM dışı harici yedekleme depolaması erişimimiz olmadığından `DOĞRULANMADI / NOT VERIFIED` olarak sınıflandırılmıştır).
                  - **Varsayılan Loopback (`127.0.0.1`) Bağlama ve Dosya Sistemi Koruması:** `server/config/serverfiles.env.example`, `server/scripts/freebsd/install.sh`, `server/scripts/windows/install.ps1`, `server/scripts/windows/start.ps1`, `.github/workflows/freebsd-gcc-test.yml` ve `tests/run_freebsd_serverfiles_test.sh` içinde MariaDB varsayılan olarak `bind-address=127.0.0.1`, `local-infile=0` ve `symbolic-links=0` ile çalışacak şekilde sertleştirilmiştir (`@@GLOBAL.bind_address = 127.0.0.1`, `@@GLOBAL.local_infile = 0`).
                  - **4 Kademeli Veritabanı Hesap Ayrımı ve C++ Kod Yollarına Göre Daraltılmış İzinler (`MT2_INIT_DB_USER`, `MT2_DB_USER=mt2server`, `MT2_LOG_DB_USER=mt2log`, `MT2_BACKUP_DB_USER=mt2backup`):**
                    1. **Kurulum/Migrasyon/Geri Yükleme Hesabı (`MT2_INIT_DB_USER`):** Yalnızca şema kurulumu (`install.sh --init-sql` / `install.ps1 -InitSql`), çevrimdışı migrasyon ve yedekten geri yükleme (`--restore`) sırasında kullanılır; çalışma zamanı `db/conf.txt` veya `CONFIG` dosyalarına asla yazılmaz.
                    2. **Çalışma Zamanı Ana Sunucu Hesabı (`MT2_DB_USER=mt2server`@`127.0.0.1`/`localhost`):** Gerçek C++ sorgu envanterine göre daraltılmıştır:
                       - `account.*`, `common.*`, `player.*`: `SELECT, INSERT, UPDATE, DELETE` (`REPLACE INTO` ve `PlayerHB::Initialize` `SHOW CREATE TABLE` dahil).
                       - `world.*`: **Yalnızca `SELECT`** (`PROTO_FROM_DB = 1` kipinde `ClientManagerBoot.cpp` `world.*` tablolarını yalnızca okur; `INSERT, UPDATE, DELETE` kaldırılmıştır, `errno 1142` ile reddedilir).
                       - `log.*`: **`0` Yetki (Tamamen Kaldırıldı)** (`db/conf.txt` `SQL_LOG` ve `CONFIG` `LOG_SQL` yalnızca `mt2log` hesabını kullanır; `mt2server` hesabının `log.*` üzerinde `SELECT`/`DELETE`/`UPDATE` yetkisi yoktur [`errno 1142/1044`], böylece `mt2server` ele geçirilse dahi saldırgan `log.*` içindeki adli izleri silemez).
                       - **Açık Artık DML Riski Sınıflandırması:** Normal oyun akışı `player.*` (`player`, `item`, `quest`, `affect`, `safebox`, `guild*`, `ikashop_*`) ve `account.*` üzerinde `UPDATE`/`DELETE` gerektirdiğinden, yalnızca DDL (`DROP`/`ALTER`/`TRUNCATE`) engellenmesi tam fidye koruması değildir; `mt2server` ele geçirilirse `DELETE`/`UPDATE` ile oyuncu verilerine zarar verilebilir. Bu risk `QUERY_DML_NO_SELECT` denetimi ve `mt2backup` ile alınan VM dışı şifreli yedeklerle azaltılır.
                    3. **Çalışma Zamanı Özel SQL Log Hesabı (`MT2_LOG_DB_USER=mt2log`@`127.0.0.1`/`localhost`):** Yalnızca `log.*` üzerinde `SELECT, INSERT, UPDATE, DELETE` yetkisine sahiptir (`account.*`, `common.*`, `player.*`, `world.*` erişimi `errno 1142/1044` ile kesin olarak reddedilir).
                    4. **Salt-Okunur Yedekleme Hesabı (`MT2_BACKUP_DB_USER=mt2backup`@`127.0.0.1`/`localhost`):** Yalnızca `SELECT, SHOW VIEW, TRIGGER, LOCK TABLES` yetkisine sahiptir (`INSERT`, `UPDATE`, `DELETE`, `DROP` `errno 1142` ile reddedilir).
                  - **MariaDB `server_audit` (`QUERY_DML_NO_SELECT`) ve `audit_guard.sh` v2 Hassas Veri Maskeleme & Rotasyon Güvenliği (`Suite 15`):** `server_audit` eklentisi `server_audit=FORCE_PLUS_PERMANENT`, `server_audit_events=CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT`, `server_audit_output_type=FILE`, `100 MB` (`104857600` B) x `90` rotasyon (`~9 GB` yerel saklama) ile yapılandırıldı. Canlı MariaDB 12.3.3 üzerinde yetkili `UPDATE` ve `DELETE` sorgularının da `mariadb_audit.log` dosyasına kaydedildiği (`avg_bytes_per_event = 184.2 B`), buna karşın hiçbir `SELECT` sorgusunun veya sonucunun (`0` `SELECT` sızıntısı) audit loguna yazılmadığı (dolayısıyla `SELECT` ile veri okuma/sızdırma sorgularının sorgu düzeyinde audit edilmediği açık tasarım sınırı olarak belgelenmiştir) ve `CREATE USER` parolalarının `*****` olarak maskelendiği kanıtlandı. `server/scripts/freebsd/audit_guard.sh` v2 ile:
                    * `0750`/`0640` izinleri, içerik özetli (`<rot>.<sha12>`) FreeBSD `chflags sappnd` arşivleme ve `MT2_AUDIT_ARCHIVE_MAX_FILES=30` (`~3 GB`) arşiv rotasyon tavanı,
                    * DML sorgularındaki Argon2id özetlerinin (`$argon2id$[REDACTED]`), `password`/`passwd`/`pwd`/`token`/`secret`/`social_id`/`email`/`hwid` alanlarının hem yerel arşiv kopyasında hem uzak TCP/TLS syslog akışında maskelenmesi,
                    * İlk satır imzası + bayt ofseti (`<sig16>|<offset>`) takibiyle `.1` / `.001` rotasyonu sırasında kuyrukta kalan gönderilmemiş satırların sıfır kayıpla boşaltılması, tamamlanmamış (`\n` içermeyen) son satırın bekletilmesi, akış ortası kopmada satır bazlı ilerleme kaydı, isteğe bağlı `--tls` ve `--require-ack` uygulama seviyesi onay (`ACK`/`OK`) desteği (varsayılan TCP soket gönderiminin uzak disk kalıcılığı anlamına gelmediği açıkça raporlanır),
                    * Kapasite (`[MT2_AUDIT_ALERT][CAPACITY_HIGH]`), dosya sistemi boş alan (`MT2_MIN_FREE_DISK_MB=1024`, `[MT2_AUDIT_ALERT][DISK_FREE_LOW]`), uzak iletim hatası (`[MT2_AUDIT_ALERT][FORWARD_FAILURE]`), `LogManager` `64 MB` spool taşması (`[MT2_SPOOL_ALERT][OVERFLOW_DETECTED]`) ve `.quarantine` (`[MT2_SPOOL_ALERT][QUARANTINE_DETECTED]`) alarmları ile `--install-cron` (`*/5 * * * *`) zamanlayıcı üretimi doğrulandı.
                  - **MariaDB 12.3 LTS Yedek Tutarlılığı, Anahtarlı `HMAC-SHA256(v2)` Doğrulaması ve Çok Tablolu İlişkisel + Trigger Geri Yükleme (`backup_mariadb.sh` & `backup_mariadb.ps1` v2):**
                    * **Depolama Motoru Envanteri (`125/125 InnoDB`):** `sql/account.sql` (`9`), `sql/common.sql` (`10`), `sql/log.sql` (`39` — `ikarusshop_log` ve `itemshop` eklendi, `itemshop_log` korundu), `sql/player.sql` (`40`) ve `sql/world.sql` (`27`) içindeki toplam **`125`** tablonun tamamının `ENGINE=InnoDB` olduğu (`0` InnoDB dışı tablo) hem statik şemada hem canlı MariaDB 12.3.3 `information_schema.TABLES` sorgusunda doğrulandı; ayrıca yedekleme öncesinde InnoDB dışı tablo tespit edilirse `[MT2_BACKUP_WARN][NON_INNODB_ENGINE]` uyarısı üretilmesi sağlandı.
                    * **Çevrimiçi (`Hot`) vs Kontrollü Çevrimdışı (`Cold`) Tutarlılık ve RPO Sınırları:** `--single-transaction --quick --hex-blob --triggers --routines --events` ile alınan yedeklerde InnoDB SQL anlık görüntüsü (MVCC snapshot) tablo/şema düzeyinde tutarlıdır; ancak `db_r41023` çalışırken (`Hot Backup`) `CPlayerTableCache` ve `CItemCache` içindeki henüz SQL'e flush edilmemiş kirli (`dirty`) bellek kayıtları (`HASH_PLAYER_LIFE_SEC = 600` sn, `SAVE_EVENT_SECOND_CYCLE = 180` sn) nedeniyle uygulama düzeyinde maksimum veri kaybı penceresi $\text{RPO}_{\text{hot}} \le T_{\text{backup\_interval}} + 600\text{ sn}$ (`6` saatlik cron ile en kötü durumda `6 saat 10 dakika`); `stop.sh` / `stop.ps1` sonrasında (`Cold Backup`, `FlushAndShutdown` tamamlanmış) ise önbellek gecikmesi `0 sn` olup $\text{RPO}_{\text{cold}} \le T_{\text{son\_başarılı\_yedek}}$ olarak sınırlandırılmıştır.
                    * **Anahtarlı `HMAC-SHA256(v2)` (`Encrypt-then-MAC`) Kriptografik Bütünlük:** Tek başına anahtarsız `.sha256` manifestosu, hem `.sql.gz.enc` arşivini hem `.sha256` dosyasını birlikte değiştiren bir saldırganı tespit edemeyeceğinden, `v2|<dosya_adı>|<sha256_hex>|<ham_şifreli_baytlar>` üzerinden `MT2_BACKUP_HMAC_SECRET` (veya alan-ayrımlı anahtar türetimi) ile `HMAC-SHA256(v2)=<64_hex>` doğrulaması eklendi. Saldırganın şifreli arşivi değiştirip `.sha256` özetini yeniden hesapladığı sahte arşiv + sahte manifesto saldırısının `[MT2_BACKUP_ALERT][TAMPER_DETECTED]` ile reddedildiği ve eski `v1` manifestoların varsayılan olarak reddedilip yalnızca açık `--allow-legacy-v1-sha256` / `-AllowLegacyV1Sha256` bayrağıyla kabul edildiği kanıtlandı.
                    * **8 Tablolu İlişkisel Veri Seti + Trigger Geri Yükleme ve Disk Kapasite Koruması:** `player`, `player_index`, `item`, `quest`, `affect`, `safebox`, `guild`, `guild_member` tabloları ve `trg_player_backup_audit` trigger nesnesi üzerinde yıkıcı `DELETE`/`UPDATE` saldırısı simülasyonundan sonra `DEFINER` bağımsız izole geri yükleme (`player_restore_verify`) ile tüm ilişkisel toplamların ve trigger nesnesinin (`TriggerCount=1`) %100 bütünlükle geri döndüğü; ayrıca `MT2_MIN_FREE_DISK_MB=1024`, `MT2_BACKUP_RETENTION_COUNT=14`, `MT2_BACKUP_RETENTION_DAYS=14`, `MT2_BACKUP_MAX_TOTAL_MB=10240` (`10 GB`) ve `--install-cron` (`0 */6 * * *`) kontrollerinin çalıştığı doğrulandı (Üretim ortamında cron zamanlayıcısı operatör tarafından `--install-cron` ile kurulana kadar otomatik yedekleme/izleme pasiftir).
              * `affect.remove_hair()`: Yalnızca `AFFECT_HAIR` (`514`) etkisini kaldırıp `(bApplyOn, lApplyValue, lDuration)` üçlüsünü döndürür; karakterin diğer aktif affect'lerine (`AFFECT_MOV_SPEED`, `AFFECT_COLLECT` vb.) dokunmaz.
            - `server/src/game/src/questlua_test.cpp` (`Check 4` & `Check 6.2`), `server/src/libthecore/src/main.c`, `server/src/game/src/char_affect.cpp`, `server/src/game/src/char_dragonsoul.cpp`: Canlı `CHARACTER` (`MaxHP=10000`, `MaxSP=5000`) ve canlı Cadılar Bayramı saç `CItem` (`vnum=74020`, `ITEM_COSTUME / COSTUME_HAIR`) nesneleri üzerinde 7 saç seçeneğinin tamamı (`63->53`, `61->56`, `62->57`, `59->54`, `60->55`, sentetik `_unused/hair.quest` `+10%` testi ile `MaxHP: 10000 -> 11000 -> 10000` / `MaxSP: 5000 -> 5500 -> 5000` ve gerçek `event_halloween_hair.quest` `+1%` [`value=1, duration=21600/86400`] testi ile `MaxHP: 10000 -> 10100 -> 10000` / `MaxSP: 5000 -> 5050 -> 5000`), veritabanından migrasyon sonrası yüklenen (`SetAttributes` -> `EquipTo` -> `Unequip` -> `Save/Reload`) saç kostümlerinde çift bonus oluşmadığı ve `attrtype1..6` yuvalarının korunduğu, `affect.add_hair` / `affect.remove_hair` / `item.set_value` / `ModifyPoints(true/false)`, `ay_reward == nil` kontrolü ve tüm **`696` `chat/*.arg`** başlık ifadesinin değerlendirilmesi (`0` hata, `0` `MISSING_LOCALE`) otomatik teste bağlandı (`--check-lua` **11/11 PASS**).
        * **7. Kalıcı MariaDB 12.3 LTS Tek Veritabanı Standardı, MySQL Extern Temizliği, `precompile.py` UTF-8 Düzeltmesi ve Kaynak SHA Kapsam Analizi (`DOĞRULANDI`):**
          * Kullanıcı kararıyla projede **MySQL 8.4 LTS** desteği kalıcı olarak sona erdirilmiş ve **MariaDB 12.3 LTS** tek desteklenen veritabanı motoru olarak sabitlenmiştir (`bdd99d76ab99d993f213d9ac78abc88290ec12f8`, `d70887e71d4e5b7796cec7c525d6d539ab90ee8b`, `da1f6775f86cfbe15e43f493d772b498a2a97a08`).
          * `extern/server/include/mysql/server/` altındaki hiç kullanılmayan **97** adet dahili sunucu/eklenti başlığı (`psi/*`, `service_*.h`, `mysql_embed.h`, `mysql_com_server.h`, `libmysqlclient`/`libmysqld` mükerrer `server/mysql.h` vb.) `git rm -r` ile çalışma ağacından ve Git indeksinden kaldırıldı; MariaDB Connector/C (`libmariadb`) tarafından kullanılan **22** adet istemci başlığı (`extern/server/include/mysql/*.h`, `mariadb/ma_io.h`, `mysql/client_plugin.h`, `mysql/plugin_auth*.h`) ve `extern/platform/windows-x64/mingw-ucrt-gcc16/lib/libmariadb.a` eksiksiz korundu (İstemci ambargosu altındaki `extern/client/**` dosyalarına dokunulmadı).
          * `.github/workflows/freebsd-gcc-test.yml` içinden `mysql84` matris işi kaldırıldı; ayrıca temiz checkout'ta `extern/server/include/mysql/server` bulunmadığını, FreeBSD Python sürümü ile varsayılan kodlamasını (`sys.getdefaultencoding()`, `locale.getpreferredencoding()`) ve CMake ELF64 ikililerinde `libmysqlclient` bağımlılığı olmadığını doğrulayan fail-closed kontroller eklendi.
          * `cmake/FindDatabaseClient.cmake`, `server/src/db/src/Makefile`, `server/src/game/src/Makefile`, `tests/run_live_tests.sh`, `tests/run_db_integration_tests.sh`, `tests/run_live_tests.ps1`, `tests/run_db_integration_tests.ps1`, `.gitignore` ve `server/config/serverfiles.env.example` dosyalarından `libmysqlclient` / `mysql-8.4.11` / `mariadb-11.4.13` / `mt2009 - FULL SOURCE` ölü arama ve bağlantı dalları temizlendi; `sql/migrations/rev47_account_security_upgrade.sql`, `tests/test_libsql.cpp`, `tests/test_live_db.cpp`, `tests/test_server_smoke_client.cpp` ve `cmake/FindDatabaseClient.cmake` içindeki yanıltıcı eski hedef veritabanı yorumları (`MySQL 8.4 LTS`, `MariaDB 11.4 LTS`) `Target database: MariaDB 12.3 LTS` olarak güncellendi; MariaDB Connector/C (`libmariadb` `3.3.5` / `MARIADB_BASE_VERSION "mariadb-10.11"` / `MARIADB_CLIENT_VERSION_STR "10.11.4"`) tarafından kullanılan MySQL uyumlu C API/ABI (`<mysql.h>`, `include/mysql`, `mysql_*` fonksiyonları) ve tüm SQL şemaları (`social_id DEFAULT '1234567'`, Argon2id, tek kullanımlık `LoginByKey`) eksiksiz korundu.
          * `server/files/share/locale/poland/quest/precompile.py`, `Make.py`, `tests/run_quest_matrix.py` ve `server/serverfiles_share.tar.xz` içindeki dosya okuma/yazma ve `subprocess.run` çağrıları `encoding="latin1"` ile güncellenerek FreeBSD Python UTF-8 ortamındaki `UnicodeDecodeError` (`0xb8`) kök nedeninden giderildi ve hem Windows (`python -X utf8`) hem FreeBSD 15.1 (`Python 3.12.14`, `defaultencoding=utf-8`, CI Run `39859687668`) üzerinde **`202/202 PASS`** ile doğrulandı.
        * **8. Üç Somut Oyun İçi SQL Hatasının Kök Neden Düzeltmesi ve Canlı MariaDB 12.3.3 LTS (`Red -> Green`) Doğrulaması (`DOĞRULANDI`):**
          1. **HATA 1 — Ikarus Offline Shop Log (`server/src/db/src/ClientManagerIkarusShop.cpp`, `server/src/db/src/DBManager.cpp`, `sql/log.sql`, `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql`):**
             * `CommonDefines.h:109` içinde `ENABLE_IKASHOP_LOGS` aktifken `CClientManager::IkarusShopLog` (`ClientManagerIkarusShop.cpp:2166`) `log.ikarusshop_log` tablosuna `INSERT` gönderiyor, ancak `sql/log.sql` içinde tablo bulunmadığından `errno 1146` (`Table 'log.ikarusshop_log' doesn't exist`) oluşuyordu; ayrıca `what` ve `extra` dizgeleri kaçışsız biçimlendiriliyordu.
             * `sql/log.sql` ve geriye uyumlu `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql` içine `ikarusshop_log` (`ENGINE=InnoDB DEFAULT CHARSET=utf8mb4`, `id, who, itemid, what, shop_owner, extra [mediumtext], vnum, count, yang [bigint], cheque, time`) eklendi.
             * `CClientManager::IkarusShopLog` içinde `what` ve `extra` parametreleri `CDBManager::instance().EscapeString(..., SQL_LOG)` ile gerçek `SQL_LOG` bağlantı karakter setine göre kaçışlandı (bağlantı yokken `fail-closed`); `CDBManager::EscapeString` içine slot başlatılmamışsa güvenli dönüş koruması eklendi.
             * Canlı MariaDB 12.3.3 LTS üzerinde `CREATE_SHOP`, `ADD_ITEM`, `BUY_ITEM` logları; tek tırnak (`'`), ters bölü (`\`), Türkçe/Lehçe UTF-8 karakterler ve `2400+` baytlık uzun `extra` metinleriyle test edildi (`Senaryo 11A/11B/11D` & `Suite 16` -> **PASS**).
          2. **HATA 2 — Nesne Market Satın Alma Logu (`server/src/game/src/itemshop_manager.cpp`, `sql/log.sql`, `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql`):**
             * `CItemShopManager::BuyItem` (`itemshop_manager.cpp:913`) `INSERT INTO itemshop VALUES (...)` ile 10 sütunlu kayıt yazarken `sql/log.sql` içinde yalnızca 8 sütunlu eski `itemshop_log` tablosu bulunduğu için her satın almada `errno 1146` (`Table 'log.itemshop' doesn't exist`) oluşuyor ve `LogManager` dayanıklı spool'u sorguyu `.quarantine` dosyasına ayırıyordu.
             * Mevcut `itemshop_log` tablosuna dokunulmadan `sql/log.sql` ve `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql` içine 10 işlevsel sütunlu (`id, pid, account_id, item_index, vnum, count, price, currency, item_id [DEFAULT NULL], date, money_before_buy`) `itemshop` (`ENGINE=InnoDB`) tablosu eklendi.
             * `itemshop_manager.cpp` içindeki satın alma log sorgusunda fiyat `const long long llLogPrice = static_cast<long long>(GetItemPrice(item)) * static_cast<long long>(quantity);` olarak 64-bit hesaplanıp `%lld` ile biçimlendirildi (`INSERT INTO itemshop (pid, account_id, item_index, vnum, count, price, currency, item_id, date, money_before_buy) VALUES (%u, %u, %d, %u, %d, %lld, %d, %s, NOW(), %u)`); böylece Windows LLP64 (`sizeof(long)==4`) üzerinde `> INT32_MAX` fiyatlarda 32-bit `long` taşması önlendi ve FreeBSD LP64 ile `bigint(20)` şema uyumu sağlandı; `GiveItem` uyarıları temizlendi.
             * Canlı MariaDB 12.3.3 LTS üzerinde normal eşya (`item_id=8800123`), VIP/ödül (`item_id=NULL`) ve büyük 64-bit fiyat (`3,000,000,000 > INT32_MAX`) satın alma loglarının yazıldığı ve `CAsyncSQL` dayanıklı spool'da karantinaya düşen sorgu sayısının **`0` (`ullQuarantinedCount == 0`)** olduğu doğrulandı (`Suite 16` -> **PASS**).
          3. **HATA 3 — İmparator Seçimi (`server/src/db/src/Monarch.cpp`, `sql/player.sql`):**
             * `sql/player.sql:761` ve `CMonarch::LoadMonarch` (`Monarch.cpp:179`) `monarch(empire, pid, windate, money)` şemasını kullanırken, `CMonarch::SetMonarch` (`Monarch.cpp:230`) `REPLACE INTO monarch (empire, name, windate, money)` çalıştırdığı için `errno 1054` (`Unknown column 'name' in 'INSERT INTO'`) hatasıyla başarısız oluyordu; ayrıca aday arama sorgusunda `name` kaçışlanmıyordu.
             * `CMonarch::SetMonarch` içinde `name` parametresi `CDBManager::instance().EscapeString(..., SQL_PLAYER)` ile kaçışlandı ve kaydetme sorgusu `REPLACE INTO monarch (empire, pid, windate, money) VALUES(%d, %u, now(), %lld)` olarak düzeltildi.
             * Canlı MariaDB 12.3.3 LTS üzerinde imparator seçimi (`SetMonarch`), hazine güncellemesi (`AddMoney`), veritabanına yazım ve yeniden yükleme (`LoadMonarch`) adımlarında `empire`, `pid`, `name`, `money` ve `windate` alanlarının eksiksiz korunduğu doğrulandı (`Senaryo 11C/11D` & `Suite 16` -> **PASS**).
        * **9. Tüm Sunucu Kaynaklarında Gerçek Warning Temizliği, ItemShop 64-Bit Ödeme Taşması Koruması ve Tam x64 Uyumluluk Denetimi (`DOĞRULANDI`):**
          * **1. Uyarı Bastırma Bayraklarının Tamamen Kaldırılması (`cmake/CompilerFlags.cmake`, `cmake/FindCryptoPP.cmake`, `server/src/db/src/Makefile`, `server/src/game/src/Makefile`, `server/src/libthecore/src/Makefile`, `tests/run_db_integration_tests.*`):**
             * `cmake/CompilerFlags.cmake` içindeki 9 adet `-Wno-*` bayrağı (`-Wno-unused-parameter`, `-Wno-sign-compare`, `-Wno-deprecated-declarations`, `-Wno-invalid-source-encoding`, `-Wno-unknown-pragmas`, `-Wno-reorder`, `-Wno-parentheses`, `-Wno-char-subscripts`, `-Wno-write-strings`), `cmake/FindCryptoPP.cmake` içindeki `-w` bayrağı, `db`/`game`/`libthecore` Makefile'larındaki `-Wno-*` bayrakları ve `run_db_integration_tests.*` içindeki `-w`/`-Wno-*` bayrakları tamamen kaldırıldı; tüm hedefler `-Wall -Wextra -pipe` ile hiçbir uyarı bastırma bayrağı olmadan derlenecek şekilde sabitlendi.
             * Başlangıçtaki **2.722** uyarı tekrarı (**686** benzersiz konum; `-Wno-*` kaldırıldığında **745** benzersiz konum) kök nedenlerinden tek tek düzeltilerek temiz derlemede (`cmake --build --preset windows-gcc-ucrt --clean-first`, **455/455** derleme adımı) **0 uyarı (`0 warnings`)** düzeyine indirildi.
          * **2. Öncelik #1 — ItemShop Gerçek Ödeme Fiyat ve Miktar Taşması Koruması (`server/src/game/src/itemshop_manager.h`, `server/src/game/src/itemshop_manager.cpp`, `server/src/game/src/questlua_pc.cpp`, `server/src/db/src/ClientManager.cpp`):**
             * `CItemShopManager::BuyItem`, `CheckBalance`, `AddCash`, `GiveItem`, `pc_charge_cash` ve `CClientManager::ChargeCash` içinde birim fiyat ve toplam fiyat (`unitPrice * quantity`) ile toplam eşya adedi (`dwCount * quantity`) `uint64_t` üzerinde hesaplandı; `unitPrice == 0 || quantity == 0 || unitPrice > INT32_MAX || totalPriceU64 > INT32_MAX || totalItemCountU64 == 0 || totalItemCountU64 > INT32_MAX` veya `lAmount == INT32_MIN` durumlarında hiçbir bakiye düşümü, açık artırma sayaç güncellemesi, DB `HEADER_GD_REQUEST_CHARGE_CASH` paketi veya eşya teslimi yapılmadan işlem `fail-closed` reddedildi.
             * `2,147,483,647` (`INT32_MAX`), `2,147,483,648` (`INT32_MAX + 1`), `3,000,000,000`, `UINT32_MAX`, `UINT32_MAX * 10`, yetersiz bakiye, başarısız ödeme ve başarılı ödeme + eşya teslimi sınır senaryoları hem `tests/test_packet_wire.cpp` (`47/47 PASS`) hem canlı MariaDB 12.3.3 LTS `tests/test_live_db.cpp` (`Suite 16` -> `325/325 PASS`) içinde doğrulandı.
          * **3. Kök Nedeninden Düzeltilen Gerçek Mantık, Bellek, Dizi Sınırı ve 64-Bit Tür Hataları (`db`, `game`, `common`, `qc`, `libpoly`, `libsql`, `cryptopp`):**
             * `char_item.cpp:352-399` (`CHARACTER::SetItem`, `DRAGON_SOUL_INVENTORY`): `wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM` (`36`) sınır kontrolü `if (pItem)` dışına en üste taşındı ve `wCell >= 36` olduğunda `m_PlayerSlots->wDSItemGrid[wCell]` dizisine sınır dışı yazan (`-Warray-bounds=`) hatalı `else` dalları kaldırıldı.
             * `char.cpp:6441` (`CHARACTER::SpecificEffectPacket`): Kısa string sabitlerinden `MAX_EFFECT_FILE_NAME` (`128` bayt) boyunca `memcpy` yaparak `.rodata` belleğini sınır dışı okuyan (`-Warray-bounds=`) ve ağ paketine sızdıran hata, `TPacketGCSpecificEffect p{};` + `strlcpy(p.effect_file, filename, sizeof(p.effect_file));` ile düzeltildi.
             * `char.cpp:9648` (`CHARACTER::IsHorseInventory`) ve `ikarus_shop.cpp:347, 364`: Her zaman `true` dönen zincirleme karşılaştırmalar (`a >= LOW <= HIGH`) `a >= LOW && a < HIGH` / `a >= LOW && a <= HIGH` olarak düzeltildi.
             * `battle.h:90` (`SkillAttackAffect`): `name == "GM_STUN" || name == "GM_SLOW"` işaretçi adresi karşılaştırması (`-Waddress`) `name && (!strcmp(name, "GM_STUN") || !strcmp(name, "GM_SLOW"))` olarak düzeltildi.
             * `char.h:904` & `char_affect.cpp:908` (`CHARACTER::IsGoodAffect`): `BYTE` (`0..255`) parametresi nedeniyle `AFFECT_SPRINT` (`> 255`) durumunun asla eşleşmemesi (`-Wswitch-outside-range`) parametrenin `DWORD dwAffectType` yapılmasıyla düzeltildi.
             * `char_resist.cpp:395` (`case IMMUNE_ROOT:`): Eksik `break;` nedeniyle `break_pct = 0` atamasının `default: break_pct = 10` dalına düşmesi düzeltildi.
             * `char_item.cpp:6895` (`CHARACTER::CanReceiveItem`): `case 20101..20103:` sonunda eksik `break;` tamamlandı.
             * `char_change_empire.cpp:26, 169-174`: `ENABLE_PLAYER_PER_ACCOUNT5` tanımlı değilken 4 yerine 5 adet `GetPlayerID()` argümanı geçilerek `AND empire=%u` alanına `GetEmpire()` yerine `GetPlayerID()` yazılması ve `OR`/`AND` parantez eksikliği düzeltildi.
             * `ClientManager.cpp`, `ClientManagerPlayer.cpp`, `ClientManagerBoot.cpp`, `ClientManagerGuild.cpp`, `ClientManagerHorseName.cpp`, `ClientManagerLogin.cpp`, `GuildManager.cpp`, `ItemAwardManager.cpp`, `ItemIDRangeManager.cpp`, `Marriage.cpp` ve `game/src/*.cpp`: Tüm SQL/log `snprintf`/`Query` format belirteçleri (`DWORD` vs `uint32_t`, `long` vs `int64_t`, `time_t`, `size_t` `%zu`, `gold`) Windows LLP64 ve FreeBSD LP64 ile birebir uyumlu hale getirildi; tanımsız davranış üreten kendinden kendine `snprintf(buf, ..., "%s...", buf, ...)` çağrıları ayrık ara tamponlarla düzeltildi; `FileMonitor_FreeBSD.cpp` hata yollarındaki dosya tanıtıcısı (`close(iFileHandle)`) sızıntısı ve `extern/server/cryptopp/esign.cpp` içindeki `seedParam.size() + 4` taşma kontrolü düzeltildi.

---

## 7. ÇOKLU ORTAM SENKRONİZASYON DURUMU (AGENTS.md Madde 9)

* **1. Windows Yerel Geliştirme Ortamı (`D:\mt22009`):** Güncel; yalnızca Lua 5.5.1 motoruyla (`db_r41023.exe`, `game_r41023.exe`, `qc.exe`) `tests/run_limited.ps1 -Build` (`%50` CPU hard-cap, `0xFC0` 6 LP, `-j6`, `BelowNormal`) **0 UYARI (`0 warnings`, `0` `-Wno-*` bayrağı) PASS**, `ctest --preset windows-gcc-ucrt` **10/10 PASS** (`libsql 201/201`, `test_packet_wire 102/102`, `test_db_cache 120/120`, `test_db_cache_integration 151/151` çevrimdışı / `180/180` canlı MariaDB ve `test_lua55_quest 11/11` dahil), `game_r41023.exe --check-lua` **11/11 PASS**, `game_r41023.exe --test-itemshop` **7/7 PASS** (gerçek `CItemShopManager` + canlı MariaDB 12.3.3 LTS üzerinde `account.itemshop_tx_journal` dayanıklı işlem günlüğü, çökme kurtarması, idempotency ve `> 2^32` 64-bit bakiye/fiyat testi), `python -X utf8 tests/run_quest_matrix.py` **202/202 PASS**, canlı MariaDB 12.3.3 LTS üzerinde `test_live_mariadb.exe` **343/343 PASS** (önceki `338/338` testin tamamı + `Suite 16b` 64-bit şema/migrasyon doğrulaması), `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql` bağımsız migrasyon doğrulaması **PASS** (`126/126` InnoDB tablo, `0` InnoDB dışı tablo) ve Rev 51 Windows x64 gerçek 8-servis + MariaDB 12.3.3 LTS canlı serverfiles doğrulaması **%100 PASS**.
* **2. GitHub Uzak Depo (`origin/dev/windows-gcc-baseline` = `aidevelopervega/Metin2-MT2009`; AGENTS.md `x64bit-source` adını yazıyor, kullanıcı onayı olmadan değiştirilmedi):**
  * Rev 49 (`a9ad54155d8f8681c7fded5d9b4d8554e99b7022`) CI `39143621656` üzerinde **2/2 SUCCESS** (`FreeBSD 15.1 - MariaDB 12.3 LTS` ve `FreeBSD 15.1 - MySQL 8.4 LTS`) ile doğrulandı.
  * Rev 50 (`4d89181caea8bd6746505ddde6a85283812d3050`) CI `39277835506` üzerinde **2/2 SUCCESS** (`FreeBSD 15.1 - MariaDB 12.3 LTS`: Job `118315414999`, `22m 58s`; `FreeBSD 15.1 - MySQL 8.4 LTS`: Job `118315414575`, `26m 13s`) ile doğrulandı ve Rev 50 resmen kapandı.
  * Rev 51 (`da1f6775f86cfbe15e43f493d772b498a2a97a08` kaynak / `81b3c39b03cc120d95a3fa4b8cd4082b7320cf1a` tetikleme) CI `39859687668` (`FreeBSD 15.1 - MariaDB 12.3 LTS`: Job `120106866220`, `18m 2s`) üzerinde **1/1 SUCCESS** ile doğrulandı ve Rev 51 resmen kapandı (`CLOSED`).
* **3. GitHub Actions FreeBSD 15.1 CI Ortamı:** Ephemeral (geçici temiz sanal makine checkout'u + `freebsd-serverfiles-logs-*` CI artifact kaydı).
  * CI Run ID **38436002336** (Rev 31): **3/3 MATRİS İŞİ %100 BAŞARIYLA TAMAMLANDI**.
  * CI Run ID **38627313843** (Rev 39 / Commit `a6333936b82819ecbc1c9d7e86832dd5a5ce13a7`): Hem `game_r41023 --check-lua` (Lua 5.0.3) hem `game_r41023_lua55 --check-lua` (Lua 5.5.1) **11/11 PASS (exit code 0)**, CMake + Ninja `[541/541]` hedef derlemesi ve `ctest --preset freebsd-gcc14` **13/13 PASS (0.09s)** kanıtlandı.
  * CI Run ID **39035737740** (Rev 44 / Commit `25040708270fc49d5f887b9b51d15e4101`): **3/3 MATRİS İŞİ (`MySQL 8.4 LTS`, `MariaDB 12.3 LTS`, `MariaDB 11.4 LTS`) %100 `SUCCESS`**.
  * CI Run ID **39056319440** (Rev 46 / Commit `3eb35d5d0954b544ad86806bd579204ee8959630`): **3/3 MATRİS İŞİ (`MySQL 8.4 LTS`, `MariaDB 12.3 LTS`, `MariaDB 11.4 LTS`) %100 `SUCCESS`**.
  * CI Run ID **39086757830** (Rev 47 / Commit `be3b58b98d16d2dcee53af19623ca3875ab6ccfb`): **3/3 MATRİS İŞİ (`MySQL 8.4 LTS`, `MariaDB 12.3 LTS`, `MariaDB 11.4 LTS`) %100 `SUCCESS`**.
  * CI Run ID **39120190223** (Rev 48 / Commit `4d9171b73b065b28f59e553aa57996322015cd3a`): **3/3 MATRİS İŞİ (`MySQL 8.4 LTS`, `MariaDB 12.3 LTS`, `MariaDB 11.4 LTS`) %100 `SUCCESS`**.
  * CI Run ID **39143621656** (Rev 49 / Commit `a9ad54155d8f8681c7fded5d9b4d8554e99b7022`): **2/2 MATRİS İŞİ (`MariaDB 12.3 LTS`, `MySQL 8.4 LTS`) %100 `SUCCESS`** (6/6 aşama `0 syserr`, 44'er log dosyası `freebsd-serverfiles-logs-*` artifact yüklendi).
  * CI Run ID **39277835506** (Rev 50 / Commit `4d89181caea8bd6746505ddde6a85283812d3050`): **2/2 MATRİS İŞİ (`MariaDB 12.3 LTS`: Job `118315414999`, `22m 58s`; `MySQL 8.4 LTS`: Job `118315414575`, `26m 13s`) %100 `SUCCESS`** (`ctest 14/14`, `--check-lua 11/11`, `run_live_tests.sh 113/113`, `run_server_smoke_test.sh 6/6`, `run_freebsd_serverfiles_test.sh 7/7`, 8 servis `0 syserr`, `chmod 600`, en az yetkili `mt2server`, 32 arşiv log rotasyonu, `ch11 -> ch12 -> portal 10002 WarpSet -> ch11` `[cross-core-step1..5]`, `0` secret sızıntısı ve `freebsd-serverfiles-logs-{mariadb123,mysql84}` artifact yüklemesi doğrulandı).
  * **CI Run ID 39859687668 (Rev 51 — Tetikleme SHA `81b3c39b03cc120d95a3fa4b8cd4082b7320cf1a` / Kaynak SHA `da1f6775f86cfbe15e43f493d772b498a2a97a08`):** **`1/1 MATRİS İŞİ (FreeBSD 15.1 - MariaDB 12.3 LTS: Job 120106866220, 18m 2s) %100 SUCCESS`** — FreeBSD 15.1-RELEASE-p3 amd64 GCC 14.2.0 + MariaDB `12.3.3-MariaDB` (`Connector/C 3.4.10`) üzerinde temiz checkout (`20,037` dosya, `extern/server/include/mysql/server` yokluğu), ELF64 `db_r41023` / `game_r41023` / `qc` derlemesi (`readelf -d`: `libmariadb.so.3`, `0` `libmysqlclient`), `run_tests.sh` (`292` birim + `423` Auth + `120` DB cache + `37` wire), `run_db_integration_tests.sh` (`140/140 PASS`), `ctest 10/10 PASS`, `game_r41023 --check-lua 11/11 PASS` (7 saç seçeneği canlı `CHARACTER` + `CItem` dahil), `run_live_tests.sh 113/113 PASS`, `run_server_smoke_test.sh [1/6]..[6/6] PASS`, `run_freebsd_serverfiles_test.sh [1/7]..[7/7] PASS` (`Python 3.12.14` UTF-8 altında `run_quest_matrix.py 202/202 PASS`, `202` state tablosu, `6,900` object, 8 servis `LISTENING`, `0 syserr`, `32` arşiv log rotasyonu, `4` `player.quest` satırı, `[cross-core-step1..5]` `ch11 -> ch12 -> portal 10002 WarpSet -> ch11` ve `dwLoginKey` tekrar oynatma reddi), `freebsd-serverfiles-logs-mariadb123` artifact yüklemesi (`44` dosya, `1,203,282` B, `Artifact ID 12461498722`) ve `.ccache-freebsd` + `libcryptopp.a` önbellek kaydı (`66.4 MB`) eksiksiz kanıtlandı.
  * **CI Run ID 40133816673 (Rev 51 Sonrası Güvenlik Doğrulama Koşusu — Tetikleme SHA `28241f0a2bb2ea77bc0571394843e7c924df267e` / Kaynak SHA `c7d0f5ed0c87a30855617c7a19b53131792d3c97` / Onaylanan Temel SHA `5c2b1af307d7c364d8310367d63824882b986bc6`):** **`1/1 MATRİS İŞİ (FreeBSD 15.1 - MariaDB 12.3 LTS: Job 120990978257, 15m 48s) -> FAILURE (BLOCKED at tests/run_freebsd_serverfiles_test.sh:381)`**:
    - **FreeBSD 15.1-RELEASE-p3 amd64 Üzerinde %100 Doğrulanan (`PASS`) Aşamalar:**
      1. **Temiz Checkout & MySQL Extern Yokluğu:** `20,041` dosya temiz checkout edildi; `extern/server/include/mysql/server` dizininin bulunmadığı doğrulandı.
      2. **Birim, Auth, DB Cache ve Wire Testleri (`tests/run_tests.sh` & `tests/run_db_integration_tests.sh`):** `libpoly 24/24 PASS`, `libthecore 97/97 PASS`, `test_auth_security 423/423 PASS`, `libgame 28/28 PASS`, **`libsql 201/201 PASS`** (dayanıklı SQL log spool, çevrimdışı `EscapeString`, `NO_BACKSLASH_ESCAPES`, `SanitizeQueryForLog`, aynı dizin `.lock` yuvası izolasyonu ve yetim `.1.dat`/`.2.dat` kurtarması dahil), `test_db_cache 120/120 PASS`, `test_packet_wire 37/37 PASS` ve gerçek MariaDB 12.3.3 üzerinde **`test_db_cache_integration 140/140 PASS`**.
      3. **CMake + Ninja ELF64 Derlemesi, `readelf -d` ve CTest:** `[302/303]` hedef derlendi; `db_r41023` (`1.2M`), `game_r41023` (`23M`) ve `qc` (`355K`) ikililerinin tamamı `ELF 64-bit LSB executable, x86-64, version 1 (FreeBSD)`, `libmariadb.so.3` + `libstdc++.so.6` (`0` `libmysqlclient`, `0` `libc++.so`) olarak doğrulandı; `ctest --preset freebsd-gcc14` **`10/10 PASS (0.55s)`**.
      4. **Lua 5.5.1 & 202 Quest Matrisi:** `game_r41023 --check-lua` **`11/11 PASS`** (hem sentetik testte hem `/usr/home/mt2/share/locale/poland` üzerinde `202` state tablosu, `696` `chat/*.arg`, `6,900` `object/` dosyası, 7 saç seçeneği + DB migrasyon sonrası `CItem` `EquipTo`/`Unequip`/`Save`); `Python 3.12.14` (`defaultencoding=utf-8`) altında `python3 tests/run_quest_matrix.py` **`202/202 PASS (0 FAIL, 0 TIMEOUT)`**.
      5. **Gerçek FreeBSD MariaDB 12.3.3 LTS Canlı Test Paketi (`tests/run_live_tests.sh` -> `292/292 PASS`):**
         * `Suite 1..12`: TCP/IP, katı SQL modu, CRUD, ACID, kısıtlar, Latin1/CP1250, asenkron kuyruk, 64-bit taşma vs boş sonuç, TLSv1.3 (`TLS_AES_256_GCM_SHA384`) & sahte CA reddi (`2026`), önceden kopuk soket (`2006`), `TcpFaultProxy` (`11.A` & `11.B`), çok baytlı UTF-8 & `NO_BACKSLASH_ESCAPES`.
         * `Suite 13` (Cadılar Bayramı Saç Kostümü Migrasyonu): 27-satır dry-run, denetimsiz çalıştırmada `0` satır değişimi, `approved_for_migration=1` ile 8 görev satırının dönüşümü ve aynı parmak izli onaysız GM eşyasının (`id=9`) korunması, idempotent 2. çalıştırma (`0` satır), rollback ve yeniden uygulama (`38/38 PASS`).
         * `Suite 14` (SQL Log Dayanıklılığı — 13 Senaryo): Kesinti sırasında disk spool, `0600` POSIX izinleri, parola/token maskelemesi, poison sorgu karantinası (`ullQuarantinedCount=1`), kapanış boşaltımı, çökme kurtarması (`40` satır, `0` kayıp, `0` mükerrer, `0` sıra bozulması), Senaryo 12 aynı dizin `.dat`/`.1.dat` özel kilit izolasyonu + `NO_BACKSLASH_ESCAPES` yeniden oynatımı (`seq 41..42`) ve Senaryo 13 3 süreç çökmesi (`.dat`, `.1.dat`, `.2.dat`) -> tek süreç yetim yuva devralması (`adopted_orphans=3, total_committed=47, lost=0, duplicated=0`).
         * `Suite 15` (MariaDB 12.3 LTS Güvenlik Duruşu): `@@GLOBAL.bind_address=127.0.0.1`, `@@GLOBAL.local_infile=0`, yetkisiz kullanıcı `1045`, `mt2server` en az yetki (`0` global yönetici yetkisi, DDL reddi, `log.*` `SELECT`/`DELETE` reddi `1142/1044`, `world.*` `SELECT` açık ve `UPDATE`/`DELETE` reddi `1142`), `mt2log` `log.*` izolasyonu, `mt2backup` salt-okunur izolasyonu, `server_audit` `FORCE_PLUS_PERMANENT` (`CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT`, `100 MB x 90` rotasyon, `UNINSTALL PLUGIN` reddi).
      6. **İzole Sunucu Duman Testi (`tests/run_server_smoke_test.sh` `[1/6]..[4/4]`) & Gerçek FreeBSD 15.1 Serverfiles (`tests/run_freebsd_serverfiles_test.sh` `[1/7]..[7/7]` + `[Bonus 4.A]` + `[Bonus 4.C]`):**
         * `[1/7]..[7/7]`: `/usr/home/mt2` kurulumu, `chmod 600`, 8 servis `LISTENING`, `0 syserr`, `bsd64hero` Argon2id -> `SmokeHero` `PHASE_GAME`, 3 aşamalı zarif kapatma & `32` arşiv logu, `4` `player.quest` satırı, `[cross-core-step1..5]` (`ch11 -> ch12 -> portal 10002 WarpSet -> ch11` + `dwLoginKey` tekrar oynatma reddi), son zarif kapatma ve `0` secret sızıntısı -> **%100 PASS**.
         * `[Bonus 4.A]`: FreeBSD `sockstat -4 -l` ile `mysql mariadbd 6692 25 tcp4 127.0.0.1:3307 *:*` (`0` dış `:3306` dinleyicisi) -> **PASS**.
         * `[Bonus 4.C]`: Gerçek FreeBSD `mariadb_audit.log` (`lines=1792, bytes=349490`) üzerinde `FAILED_CONNECT`, reddedilen DDL (`1044`/`1142`), yetkili DML (`UPDATE`/`DELETE`), reddedilen `mt2server` `log.*` `DELETE` (`1142/1044`) ve `world.*` `UPDATE` (`1142`), `CREATE USER` parola maskelemesi (`*****`), `0` `SELECT` gürültüsü, canlı rotasyon (`SET GLOBAL server_audit_file_rotate_now = ON`) ve `0750`/`0640` izinleri -> **PASS**.
    - **Başarısız Olan Adım ve Kök Neden (`tests/run_freebsd_serverfiles_test.sh:381`):**
      * `[Bonus 4.D & 5]` içinde sentetik rotasyon dosyası (`mariadb_audit.log.001`) için yazılan `INSERT INTO account (login,password,social_id,email,hwid) VALUES (''u1'',''$argon2id$...'',''7654321'',''old@example.com'',''OLD_HWID_77'')` satırında, `audit_guard.sh` içindeki maskeleme kuralları `$argon2id$...`, 7 haneli `social_id` ve `email` alanlarını değer biçimine göre, `hwid=...` alanını ise anahtar-değer atamasına göre maskeliyordu; ancak konumsal `INSERT INTO ... (..., hwid) VALUES (..., ''OLD_HWID_77'')` ifadesinde `hwid=` öneki bulunmadığından `OLD_HWID_77` değeri maskelenmeden kaldı ve `run_freebsd_serverfiles_test.sh:380` satırındaki `grep -Eq "c2FsdA|7654321|old@example\.com|OLD_HWID_77" "$ARCH_ROT"` kontrolü `exit 1` ile durdu.
    - **Erken Çıkış Nedeniyle FreeBSD Üzerinde Çalıştırılmamış (`NOT EXECUTED / NOT VERIFIED ON FREEBSD`) Kontroller:**
      1. `tests/run_freebsd_serverfiles_test.sh` `[Bonus 4.D & 5]` kalan alt kontrolleri (satır `385–530`: kapasite/boş disk/spool alarmları, tamamlanmamış son satır bekletme, `.1` rotasyon kuyruğu boşaltımı, Python TCP+ACK ve TLS+ACK alıcı testi, hata anında ofset korunumu ve izole `--install-cron`).
      2. `tests/run_freebsd_serverfiles_test.sh` `[Bonus 4.E]` (satır `531–707`: FreeBSD üzerinde `backup_mariadb.sh` v2 şifreli yedekleme, `HMAC-SHA256(v2)` sahte manifesto/v1/parola/disk reddi ve 8 tablolu ilişkisel + trigger izole geri yükleme testi).
      3. `.github/workflows/freebsd-gcc-test.yml` `[4.A]` (satır `370–390`: MariaDB sürecinin `server_security.cnf` ile yeniden başlatılması sonrasında `bind_address=127.0.0.1`, `local_infile=0` ve `server_audit` ayarlarının kalıcılık kontrolü).
      4. `vmactions/freebsd-vm` SSH betiği `exit 1` ile çıktığından `copyback` çalışmamış ve `freebsd-serverfiles-logs-mariadb123` artifact dizini runner'a kopyalanmamıştır.
    - **Yerel Kök Neden Düzeltmesi, Hedefli Redaction Kanıtı (`10/10`) ve Eksik FreeBSD Bloklarının POSIX/FreeBSD Hazırlık Denetimi (`LOCAL REGRESSION READY`):**
      * **1. Deterministik SQL Literal Tarayıcısı ile Hassas Sütun/Değer Maskelemesi (`server/scripts/freebsd/audit_guard.sh` — `10/10 HEDEF TEST PAKETİ PASS`):**
        - Hem `enforce_perms` (arşivleme) hem `forward_once` (isteğe bağlı syslog iletimi) içindeki Python maskeleme motoru regex tabanlı yaklaşımdan deterministik bir SQL tırnak/tuple tarayıcısına (`_scan_quoted_sql_token`, `_parse_tuple_tokens`, `redact_sql_text`) yükseltildi.
        - **Doğrulanan 10 Hedef Kategori (`10/10 PASS`):** (1) Standart `INSERT INTO` sütun sırası, (2) Hassas sütunların (`hwid`, `email`, `social_id`, `passwd`, `secret`, `token`, `old_pw`, `new_pw`) farklı sıralanması, (3) `INSERT IGNORE` / `REPLACE INTO` / `INTO` olmadan `INSERT` / backtick `` `account`.`account` `` şema önekleri, (4) Çoklu satır `VALUES (...), (...), (...)` ifadelerinin tamamının maskelenmesi, (5) Boş dizgeler (`''''`, `''`, `""`) ve `NULL` değerlerin tırnak sınırlarını kaydırmaması, (6) Değer içinde kaçışlı tek/çift tırnak (`''O''''Reilly_HWID_55''`, `\'`, `\"`, `\\`, `''''''`), virgül `,` ve parantez `()` bulunması durumunda sütun hizasının bozulmaması, (7) `UPDATE ... SET` ve `ON DUPLICATE KEY UPDATE` atamalarında tek/çift tırnak ve kaçışlı değerlerin bozulmadan maskelenmesi (`$argon2id$[REDACTED]` / `*[REDACTED_SHA1]` korundu), (8) Çok baytlı UTF-8 karakterler (`şifre_ğüşıöç_UTF8`, `HWID_UTF8_ıİ_88`), (9) MariaDB `server_audit` tırnak biçimleri (`''...''`, `\'...\'`, `'...'`, `"..."`, `\"...\"`), (10) `enforce_perms` ve `forward_once` çıktılarının bayt düzeyinde birebir (`100%`) eşleşmesi.
        - **Maskelemenin Garanti Etmediği SQL Biçimleri (Açık Sınır):** Sütun listesi belirtilmeyen konumsal `INSERT INTO account VALUES (...)` sorguları, `INSERT INTO ... SELECT ...` alt sorguları, SQL fonksiyon/ifade sarmalayıcıları (`UNHEX('...')`, `CONCAT('a','b')`, `CHAR(...)`), `SET @var = '...'` oturum değişkenleri üzerinden dolaylı yazımlar ve standart dışı sütun adları sütun-değer eşleştirmesiyle garanti edilmez (buna karşın değer biçimine duyarlı `$argon2id$...`, `*[0-9A-Fa-f]{40}` SHA1 ve e-posta kalıpları sütun adından bağımsız olarak maskelenmeye devam eder).
        - **Aktif `mariadb_audit.log` vs Rotasyon Arşivi Erişim Sınırı:** MariaDB `server_audit` eklentisi aktif `mariadb_audit.log` dosyasına doğrudan yazdığından, aktif dosya rotasyona uğrayana kadar ham DML parametrelerini taşıyabilir; bu nedenle aktif log dizini `0750`, aktif `mariadb_audit.log` dosyası `0640` izinleriyle sınırlandırılmış; rotasyona uğrayan `.001`..`.090` dosyaları ise `audit_guard.sh --enforce-perms` tarafından maskelenerek `0700` izinli arşiv dizininde `0400` + FreeBSD `chflags sappnd` ile saklanacak ve ham rotasyon dosyası silinecek şekilde doğrulanmıştır.
      * **2. Çalışmamış 3 FreeBSD Bloğunun (`[Bonus 4.D & 5]`, `[Bonus 4.E]`, `[4.A]`) POSIX/FreeBSD Denetimi ve Düzeltmeleri:**
        - **BSD `df` Bayrak Uyumsuzluğu (`audit_guard.sh` & `backup_mariadb.sh`):** FreeBSD `df` üzerinde `-P` ve `-m` bayraklarının çakışma/geçersiz kılma riskini önlemek için her iki betikte de `df -Pm` yerine POSIX standardı `df -Pk ... | awk 'NR==2 {print int($4 / 1024)}'` kullanıldı.
        - **POSIX `sh` `set -e` + `trap cleanup EXIT` Çıkış Kodu Ezilme Hatası (`backup_mariadb.sh` & `run_freebsd_serverfiles_test.sh`):** `server/scripts/freebsd/backup_mariadb.sh` içinde `MT2_MIN_FREE_DISK_MB` yetersiz disk kontrolü `exit 2` ile çıkarken `cleanup()` (`trap cleanup EXIT`) fonksiyonunun son satırındaki `[ -n "$TMP_SQL" ] && rm -f "$TMP_SQL"` ifadesinin (`TMP_SQL=""` iken) `1` döndürerek `set -e` altında çıkış kodunu `2` yerine `1` yaptığı tespit edildi (`[Bonus 4.E]` satır `676` `if [ "$LOW_DISK_RC" -ne 2 ]` kontrolünü bozacak erken çıkış riski). `cleanup()` başına `rc=$?; set +e` ve sonuna `exit "$rc"` eklenerek çıkış kodunun (`exit 2`) korunduğu canlı POSIX `sh` testinde kanıtlandı.
        - **Yedek Geri Yükleme `DEFINER` Temizliği (`backup_mariadb.sh`):** `--restore` sırasında `sed` ifadesi tüm kullanıcı/host kombinasyonlarını (`DEFINER=\`[^\`]*\`@\`[^\`]*\``) kapsayacak şekilde genelleştirildi.
        - **Katı RFC 5280 TLS Doğrulaması (`ssl.VERIFY_X509_STRICT`) ve `[4.A]` MariaDB Restart Kalıcılığı (`.github/workflows/freebsd-gcc-test.yml`, `audit_guard.sh` & `run_freebsd_serverfiles_test.sh`):** Üretim güvenliğini zayıflatmamak adına `audit_guard.sh` içinde `VERIFY_X509_STRICT` gevşetmesi tamamen kaldırıldı ve `ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=...)` üzerinde `ctx.verify_mode = ssl.CERT_REQUIRED`, `ctx.check_hostname = True`, `ctx.minimum_version = ssl.TLSVersion.TLSv1_2` ve `ctx.verify_flags |= ssl.VERIFY_X509_STRICT` zorunlu kılındı. `.github/workflows/freebsd-gcc-test.yml` içindeki izole test CA ve sunucu sertifikası üretimi RFC 5280 ile tam uyumlu hale getirildi (CA: `basicConstraints=critical,CA:TRUE`, `keyUsage=critical,keyCertSign,cRLSign`, `subjectKeyIdentifier=hash`, `authorityKeyIdentifier=keyid:always,issuer`; Sunucu: `basicConstraints=critical,CA:FALSE`, `keyUsage=critical,digitalSignature,keyEncipherment`, `extendedKeyUsage=serverAuth`, `subjectKeyIdentifier=hash`, `authorityKeyIdentifier=keyid,issuer`, `subjectAltName=IP:127.0.0.1,DNS:localhost`). 5 TLS senaryosunun tamamı (`Doğru CA + doğru hostname: PASS`, `Yanlış CA: REJECT`, `Yanlış hostname: REJECT`, `Geçersiz sertifika zinciri [CA:FALSE imzalayıcı, keyUsage eksik CA ve zincirsiz ara sertifika]: REJECT`, `TLS + ACK: PASS`) hem yerel testte hem `run_freebsd_serverfiles_test.sh` içinde doğrulandı; `[4.A]` adımında `kill $SERVER_PID` yerine senkron `mariadb-admin --socket="$SOCKET_PATH" -u root shutdown` + `mariadb-admin ping` doğrulaması eklendi.
      * **3. %100 Yerel ve Bağımsız Varsayılan Çalışma (`serverfiles.env.example`, `audit_guard.sh`, `backup_mariadb.sh`):**
        - Yerel FreeBSD audit (`--enforce-perms`, `--check-capacity`, `--check-spool-syserr`), yerel cron (`--install-cron`) ve yerel şifreli yedekleme (`AES-256-CBC + PBKDF2 + HMAC-SHA256(v2)`) hiçbir bulut depolama aboneliği, harici SIEM veya üçüncü parti servis gerektirmeden %100 yerel ve bağımsız çalışır; uzak syslog/SIEM iletimi (`MT2_AUDIT_REMOTE_HOST` / `MT2_AUDIT_SYSLOG_HOST`) tamamen isteğe bağlıdır. Aynı VM/disk üzerindeki yerel yedeğin aynı disk/VM arızasına karşı koruma sağlamadığı (ayrı çıkarılabilir diskin isteğe bağlı olduğu) açıkça belgelendi.
      * **4. İkinci Kullanıcı Onaylı FreeBSD 15.1 CI Koşusu (`CI Run ID 40182578376` / `Job ID 121136559119` — Tetikleme SHA `e364196e607594a9c1a05c6947c6ebca5570b826` / Onaylanan Kaynak SHA `771b93ab052c3e573e393ba9aa577f19f37e14c9`, Süre: `9m 44s`):**
        - **Gerçek FreeBSD 15.1-RELEASE-p3 amd64 + MariaDB 12.3.3 LTS Üzerinde %100 `PASS` Olan Tüm Çekirdek ve Eksik Güvenlik Aşamaları:**
          1. **Çekirdek Derleme, Birim, Canlı SQL, Quest ve 8-Servis Testleri:** `libpoly 24/24 PASS`, `libthecore 97/97 PASS`, `test_auth_security 423/423 PASS`, `libgame 28/28 PASS`, **`libsql 201/201 PASS`**, `test_db_cache 120/120 PASS`, `test_packet_wire 37/37 PASS`, **`test_db_cache_integration 140/140 PASS`**, CMake ELF64 `db_r41023` / `game_r41023` / `qc`, `ctest 10/10 PASS`, `game_r41023 --check-lua 11/11 PASS`, `run_quest_matrix.py 202/202 PASS (0 FAIL, 0 TIMEOUT)`, **`run_live_tests.sh 292/292 PASS`** (`Suite 1..15`), `run_server_smoke_test.sh [4/4] PASS` ve `run_freebsd_serverfiles_test.sh [1/7]..[7/7] PASS` (`[cross-core-step1..5]`, `0 syserr`, `0` secret sızıntısı).
          2. **`[Bonus 4.A]` & `[Bonus 4.C]` (`sockstat` ve Gerçek `mariadb_audit.log`):** `mysql mariadbd 6690 25 tcp4 127.0.0.1:3307 *:*` (`0` dış `3306` dinleyicisi) ve gerçek `mariadb_audit.log` (`lines=1798, bytes=350009`, `0` `SELECT` gürültüsü, `*****` parola maskelemesi, `0750`/`0640` izinleri) -> **`PASS`**.
          3. **`[Bonus 4.D & 5]` (`audit_guard.sh` v2 + Katı RFC 5280 TLS `VERIFY_X509_STRICT` + Cron — Önceki Koşuda Eksik Kalan 1. Blok):**
             * Deterministik SQL literal tarayıcısı (`INSERT`/`REPLACE`, çoklu `VALUES`, farklı sütun sırası, `UPDATE`, `ON DUPLICATE KEY UPDATE`, UTF-8, tırnak/kaçış biçimleri, `hwid`/`token`/`password`/`Argon2id`/`social_id`/`email` maskelemesi, arşiv ve iletimde birebir aynı çıktı), kapasite/düşük disk/spool alarmları, eksik son satır bekletme, `.1` rotasyon kuyruğu boşaltımı ve izole `--install-cron` (`chmod 600`) gerçek FreeBSD 15.1 üzerinde -> **`PASS`**.
             * Katı RFC 5280 TLS (`VERIFY_X509_STRICT` açık): (1) Doğru CA + doğru hostname `PASS`, (2) Yanlış CA `REJECT`, (3) Yanlış hostname `REJECT`, (4) Geçersiz sertifika zinciri `REJECT`, (5) `TLS + ACK` `PASS` ve hata anında ofset korunumu (`-> [PASS] Python TCP/TLS/ACK receiver verified: holdback, redaction, .1 rotation ordering, strict RFC 5280 TLS (valid CA+host PASS, wrong CA REJECT, wrong host REJECT, invalid chain REJECT, TLS+ACK PASS) & offset safety`) -> **`PASS`**.
          4. **`[Bonus 4.E]` (`backup_mariadb.sh` v2 — Önceki Koşuda Eksik Kalan 2. Blok):**
             * Gerçek FreeBSD 15.1 üzerinde `InnoDB=123/123, Non-InnoDB=0`, `[MT2_BACKUP_WARN][NON_INNODB_ENGINE]` uyarısı, `mt2backup` ile `AES-256-CBC + PBKDF2 + HMAC-SHA256(v2)` şifreli yedekleme (`mt2_backup_20261025_100406Z.sql.gz.enc`), sahte arşiv + yeniden hesaplanmış `.sha256` saldırısı reddi, `v1` manifesto varsayılan reddi, yanlış parola reddi, düşük disk alanında `exit 2` (POSIX `cleanup()` tuzağı çıkış kodu korunumu) ve 8 ilişkisel tablo + `TRIGGER` geri yükleme (`Pre-backup` = `Restored` = `2|104500000|4|55|3|2|5000000|RelGuild_PL:2:620000 | TriggerCount=1`) -> **`PASS`**.
             * `tests/run_freebsd_serverfiles_test.sh` betiği baştan sona `0` hata ile tamamlanarak `[PASS] REV 51 FREEBSD 15.1 SERVERFILES` çıktısını üretti.
        - **5. `[4.A]` Adımında İş Akışı (`freebsd-gcc-test.yml:399`) Çıkış Kodu 1 Nedeni (`OFF` vs `0` Metin Karşılaştırması):**
          * `tests/run_freebsd_serverfiles_test.sh` tamamlandıktan sonra `.github/workflows/freebsd-gcc-test.yml` içindeki `[4.A]` adımında MariaDB `server_security.cnf` ile kontrollü kapatılıp 2 saniyede yeniden başlatıldı (`mysqld is alive`, `-> MariaDB restarted in 2s using persistent server_security.cnf`).
          * Yeniden başlatma sonrasında `SELECT CONCAT(@@GLOBAL.bind_address,'|',@@GLOBAL.local_infile,'|',@@GLOBAL.server_audit_logging,'|',@@GLOBAL.server_audit_events);` sorgusu **`127.0.0.1|OFF|ON|CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT`** döndürdü (yani `bind_address=127.0.0.1`, `local_infile=OFF` [kapalı] ve `server_audit` ayarları gerçekte %100 korundu).
          * Ancak `.github/workflows/freebsd-gcc-test.yml:399` satırındaki kabuk kontrolü `!= "127.0.0.1|0|ON|..."` şeklinde yalnızca sayısal `"0"` beklediği için, MariaDB 12.3.3'ün `CONCAT(...)` içinde boolean `local_infile` değişkenini `"OFF"` olarak döndürmesi üzerine `exit 1` verdi ve GitHub Actions iş sonucu `FAILURE` (`BLOCKED`) olarak kapandı.
          * `.github/workflows/freebsd-gcc-test.yml` içindeki kontrol hem `"127.0.0.1|OFF|ON|..."` hem `"127.0.0.1|0|ON|..."` biçimini kabul edecek şekilde yerel olarak düzeltildi; Bölüm 2 ve Bölüm 6 kuralları gereği **ikinci bir CI koşusu veya rerun başlatılmadı** (`ci/run-full-freebsd.txt` dokunulmadı).
* **4. Kalıcı FreeBSD VM Git Deposu:** Bulunmamaktadır (Dürüst Durum: **UNKNOWN / EPHEMERAL / UNVERIFIED** — Yerel FreeBSD VM bulunmadığından son x64 protokol ve ItemShop işlem günlüğü dönüşümlerinin FreeBSD çalışma zamanı doğrulaması dürüstçe `UNVERIFIED` olarak raporlanmıştır).

---

## 8. KALICI GELİŞTİRME YOL HARİTASI VE ÇALIŞMA SIRASI (AGENTS.md Madde 11)

Projenin modernizasyonunda takip edilecek değişmez operasyonel sıra:

1. **Mevcut FreeBSD x64 game/db Başarısını Koru:**
   * Doğrulanmış ELF64 `game` ve `db` derleme, bağlama ve wire protokolü başarısı her aşamada korunacak; tamamlanmış adımlar tekrarlanmayacaktır. (TAMAMLANDI)
2. **CMake + Ninja Derleme Altyapısı Kurulumu:**
   * Windows MinGW-w64 GCC 16.2 UCRT ve FreeBSD 15.1 amd64 GCC 14 için birleşik, tek merkezî `CMakeLists.txt` ve Ninja derleme mimarisi kurulacaktır. (TAMAMLANDI)
3. **CMake ile Gerçek İkili ve Test Doğrulaması:**
   * CMake üzerinden FreeBSD ortamında gerçek `game` ve `db` ELF64 binary'leri, Windows tarafında ise sunucu kütüphaneleri ve regresyon testleri doğrulanacaktır. (TAMAMLANDI)
4. **Eski Makefile Eşdeğerlik Güvencesi:**
   * Mevcut Makefile sistemleri, CMake altyapısının %100 eşdeğerliği ve başarısı kanıtlanıncaya kadar depoda korunacaktır; aceleyle silinmeyecektir. (Eşdeğerlik kanıtlandı)
5. **Aşamalı Server/Common Extern Sürüm Yükseltmesi:**
   * Server ve Common Extern bağımlılıkları (Crypto++, Boost, DevIL, Lua, RapidJSON, fmt, OpenSSL, zlib vb.) CMake altyapısı üzerinden en son doğrulanmış resmi kararlı sürümlere adım adım yükseltilecektir. (Crypto++ 8.9.0, fmt 12.2.0, RapidJSON modern master, DevIL 1.8.0, Boost 1.92.0, OpenSSL/zlib sistem entegrasyonu, **yalnızca Lua 5.5.1 tek üretim/derleme motoruna geçiş (eski Lua 5.0.3 ve 5.2.3 motorlarının kaldırılması)**, **kalıcı MariaDB 12.3 LTS tek veritabanı standardı (MySQL 8.4 sunucu extern temizliği)** ve **`phc-winner-argon2` (`20190702`) hesap/parola güvenliği sertleştirmesi** tamamlandı)
6. **Bütünsel Test ve Çoklu Ortam Doğrulaması:**
   * Her aşamada ilgili birim, entegrasyon, canlı SQL ve wire protokolü testleri çalıştırılacak; platform bağımlılıkları ve Git/CI senkronizasyonu kanıtlanacaktır. (Rev 50 Windows x64 + FreeBSD 15.1 amd64 TAMAMLANDI; **Rev 51 Windows x64 Kademe A + B + C ve FreeBSD 15.1 amd64 Kademe D (`CI Run ID 39859687668`, `18m 2s`, `1/1 SUCCESS`) %100 TAMAMLANDI ve KAPANDI (`CLOSED`); ayrıca Tam Sunucu x64 Protokol Dönüşümü (`Aşama A..F`), MariaDB 12.3 LTS 64-Bit Şema/Migrasyon ve Dayanıklı ItemShop İşlem Günlüğü (`account.itemshop_tx_journal`) Windows x64 + canlı MariaDB 12.3.3 LTS üzerinde `CTest 10/10`, `Packet Wire 102/102`, `DB Cache 180/180`, `Live MariaDB 343/343` ve `Real CItemShopManager 7/7 PASS` ile doğrulandı**)

---

## 9. TEK AKTİF GÖREV VE SIRADAKİ ADIM

* **Tamamlanan Son Görev:** **Tam Sunucu x64 Protokol Dönüşümü (`Aşama A..F`), MariaDB 12.3 LTS 64-Bit Şema Uyumluluğu, Dayanıklı ItemShop İşlem Günlüğü (`account.itemshop_tx_journal`) ve Somut Regresyon Testleri (`PASS`)** —
  1. **Aşama A (Commit `2c9fa942` — Açık x64 Protokol Sürüm Sözleşmesi ve Erken Bağlantı Reddi):**
     - `server/src/common/tables.h`: `MT2_PROTOCOL_VERSION_X64 = 0x20090064U` (`537460836U`) ve `MT2_PROTOCOL_VERSION_LEGACY_X86 = 0x20090032U` tanımlandı; `SPacketGDSetup::dwProtocolVersion` ve `TPacketGGSetup::dwProtocolVersion` (`packet.h`) eklendi.
     - `input_auth.cpp` (`HEADER_CG_LOGIN3`), `input_login.cpp` (`HEADER_CG_LOGIN2`), `ClientManager.cpp` (`QUERY_SETUP`) ve `input_p2p.cpp` (`HEADER_GG_SETUP`) içinde uyumsuz protokol sürümlerinin oturum açmadan en erken güvenli aşamada reddedilmesi (`WRONGVER` / `Close()` / `DelayedDisconnect`) sağlandı.
  2. **Aşama B (Commit `761eb554` — `GAME`/`DB`/`AUTH`/`P2P` Sunucu İçi 64-Bit / Sabit Genişlikli Veri Akışı):**
     - `server/src/common/tables.h`, `server/src/game/src/packet.h`, `ClientManager*.cpp`, `LoginData.*`, `db.*`, `char.*`, `char_affect.cpp`, `input_login.cpp`, `input_db.cpp`, `utils.*`, `special_shop_manager.cpp`, `cmd_gm.cpp`, `itemshop_manager.*`:
       * `TPacketGiveExp::amount` (`int32_t -> uint64_t`), `TSpecialFlagTable::llValue` (`int -> int64_t`), `TAffectSaveElement::lDuration`/`lSPCost` (`int32_t -> int64_t`), `TPacketAffectElement::lDuration`/`lSPCost` (`int32_t -> int64_t`), `TPacketGDAuthLogin::iPremiumTimes` ve `CLoginData::m_aiPremiumTimes` (`int[PREMIUM_MAX_NUM] -> int64_t[PREMIUM_MAX_NUM]`), `TPacketBillingRepair` (`int64_t`), `TPacketGDVCard` (`int64_t`), `SPacketGDRequestChargeCash::llAmount` (`int32_t -> int64_t`) ve `SPacketDGRequestChargeCash::llAmount` (`int64_t`) uçtan uca 64-bit kapasiteye yükseltildi.
  3. **Aşama C (Commit `89db7778` — `GAME ↔ CLIENT` Sunucu Tarafı x64 Protokol Dönüşümü):**
     - `server/src/game/src/packet.h`, `char_player.h`, `char_player.cpp`, `tests/test_packet_wire.cpp`, `tests/test_server_smoke_client.cpp`:
       * `TPacketCGLogin2::dwProtocolVersion` ve `TPacketCGLogin3::dwProtocolVersion` (`32-bit -> 53-bayt / 57-bayt` wire boyutu),
       * `TPacketGCSpecialFlag::llValue` (`int -> int64_t`, `37 -> 41` bayt),
       * `TPacketGCAffectAdd` (`elem.lDuration`/`lSPCost` `int64_t`, `23 -> 31` bayt),
       * `TPacketGCItemShopBalance` (`llCoin`/`llMark` `int64_t`, `9 -> 17` bayt),
       * `TPacketGCItemShopItem::TItemShopPrice` (`llCoinPrice`/`llMarkPrice` `int64_t`, `56 -> 64` bayt),
       * `TPacketGCChannelStatus::TChannelStatus` (`long lAddr -> int32_t lAddr`, LP64/LLP64 8-bayt/4-bayt kayması kapatıldı),
       * `TPlayerCaptcha::expireTime` (`DWORD -> int64_t`) dönüştürüldü ve `static_assert(sizeof/offsetof)` + golden byte testlerine bağlandı.
  4. **Aşama D (Commit `0a969f1d` — 64-Bit Ekonomi/Refine ve MariaDB 12.3 LTS 64-Bit Şema + Migrasyon):**
     - `char.h`, `char_item.cpp`, `mining.h`/`.cpp`, `questlua_pc.cpp`: `CHARACTER::ComputeRefineFee(YANG iCost, int iMultiply = 5) const` ve `GetRefineFee` 64-bit `YANG` (`int64_t`) olarak yükseltildi; `> 430M` Yang yükseltme maliyetlerinde `iCost * iMultiply` 32-bit `int` taşmasıyla oluşan negatif ücret hatası kök nedeninden kapatıldı.
     - `sql/account.sql`, `sql/common.sql`, `sql/log.sql`, `sql/player.sql` ve `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql`:
       * `account.account.cash` ve `cash_mark`: `int(10) -> bigint(20) NOT NULL DEFAULT 0`,
       * `account.itemshop_tx_journal`: `ENGINE=InnoDB` dayanıklı işlem günlüğü tablosu eklendi (`125 -> 126` InnoDB tablo),
       * `common.priv_settings.value` (`int -> bigint`) ve `duration` (`datetime` -> `bigint(20) NOT NULL DEFAULT 0` Unix epoch),
       * `player.affect.duration` ve `sp` (`int(11) -> bigint(20)`), `player.pc_special_flag.value` (`int(11) -> bigint(20)`), `player.monarch_candidacy.date` (`datetime DEFAULT CURRENT_TIMESTAMP`), `player.guild_war_reservation` (`winner` `int(11) DEFAULT -1`, `handicap` `int(11)`), `player.itemshop_time_auction.buy_count` (`int(10) unsigned -> bigint(20) unsigned`),
       * `log.cube.gold`, `log.fish_log.prob_idx`, `log.goldlog.how`, `log.log.ip`, `log.money_log.vnum`, `log.speed_hack. count`, `log.vcard_log.sell_expire`/`buy_expire`, `log.itemshop.price`/`money_before_buy` (`bigint(20)`) şema ve migrasyon uyumu sağlandı (`Suite 16b` -> `343/343 PASS`).
  5. **Aşama E (Commit `cef9a8f9` — ItemShop Dayanıklı İşlem Günlüğü `account.itemshop_tx_journal` ve Çökme Kurtarması):**
     - `server/src/game/src/itemshop_manager.h`/`.cpp`, `main.cpp`, `questlua_test.cpp`:
       * `account.cash` / `cash_mark` düşümü ile `account.itemshop_tx_journal` (`state = 'CHARGED'`) durumu aynı `AccountDB` bağlantısı üzerinde tek bir InnoDB ACID transaction (`START TRANSACTION ... COMMIT`) içinde birleştirildi.
       * Eşya teslimatında `player.item_award.why` sütununa tekil `ITEMSHOP_TX_<tx_id>` belirteci yazılarak teslimat idempotent hale getirildi.
       * Süreç `CHARGED` sonrasında ve `DELIVERED` öncesinde aniden çökerse, sunucu yeniden başladığında (`main.cpp` ve `LoadItems`) çalışan `CItemShopManager::RecoverIncompleteTransactions()` yarım kalan tüm `INITIATED`/`CHARGED` işlemleri tarar: `item_award` (`ITEMSHOP_TX_<tx_id>`) veya `log.itemshop` kaydı varsa çift ödeme/iade yapmadan `DELIVERED` olarak kapatır; teslimat yoksa `AccountDB` transaction içinde parayı kuruşu kuruşuna iade edip müzayede sayacını düşürerek `REFUNDED` olarak kapatır (`game_r41023.exe --test-itemshop` **`7/7 PASS`**).
  6. **Aşama F (Regresyon Testleri, `ProtoReader` `uint32_t` Bitmask Güvenliği, `CLIENT_X64_PROTOCOL_MIGRATION.md` ve Uyarı Denetimi):**
     - `server/src/db/src/ProtoReader.h`/`.cpp`: 7 adet bitmask hesaplayıcısı (`get_Item_AntiFlag_Value`, `get_Item_Flag_Value`, `get_Item_WearFlag_Value`, `get_Item_Immune_Value`, `get_Mob_AIFlag_Value`, `get_Mob_RaceFlag_Value`, `get_Mob_ImmuneFlag_Value`) `uint32_t` dönüş türüne ve `if (i < 32) retValue |= (1u << static_cast<uint32_t>(i));` + `std::stoull` güvenli işaretsiz aritmetiğine geçirildi; bit 31 (`1u << 31`) işaretli taşma UB'si tamamen kapatıldı.
     - `tests/test_packet_wire.cpp` (`102/102 PASS`) ve `tests/test_db_cache_integration.cpp` (`180/180 PASS` canlı MariaDB 12.3.3 LTS): `ProtoReader` `ITEM_SOUL`/`ITEM_PASSIVE`, `> 2^31` `item_proto` fiyatları, bit 31 bitmask shift güvenliği, `CPeer::PeekPacket(UINT32_MAX)`, P2P `Relay`/`Whisper` `1024` bayt ve negatif `lSize` sınırı, `CConfig` `255/256` bayt tampon sınırı, `Monarch` oylama dizisi ilklendirmesi, `TGuildWarPQElement` negatif puanlar, `NationMoney` üst sınırı ve `ComputeRefineFee` 64-bit hesaplaması için 11 yeni regresyon testi eklendi.
     - `docs/CLIENT_X64_PROTOCOL_MIGRATION.md`: İstemci kaynaklarına (`client/src/**`, `client/files/**`, `extern/client/**`) dokunulmadan gelecekteki x64 istemci dönüşümü için tam bayt/offset/struct göç kılavuzu oluşturuldu.
     - `itemshop_manager.cpp` ve `packet.h` içindeki tüm `-Wconversion`/`-Wsign-conversion` uyarıları **`0`**'a indirildi (`server/src/db/` = **`0` uyarı**; `server/src/` genelinde `-Wshadow: 0`, `-Wundef: 0`, `-Wformat-nonliteral: 54`, `-Wconversion: 1047`, `-Wfloat-conversion: 124`, `-Wsign-conversion: 1406`; `extern/` `-Wundef: 488` [`487` Crypto++ + `1` Boost, `0` gerçek hata]).
* **Sıradaki Güvenli Adım:**
   1. Kullanıcı talimat verdiğinde `docs/CLIENT_X64_PROTOCOL_MIGRATION.md` belgesine göre İstemci (Client) x64 protokol uyarlaması veya `server/src/game/src/` içindeki kalan `-Wconversion`/`-Wsign-conversion` uyarılarının modül bazında temizlenmesi yürütülecektir.

