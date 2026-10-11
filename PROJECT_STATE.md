# METİN2 MT2009 — PROJE DURUM TAKİBİ (PROJECT_STATE.md)

Bu belge, projenin anlık teknik durumunu, doğrulanmış bulgularını ve bir sonraki mikro görevini gösteren **tek doğruluk kaynağıdır (Single Source of Truth)**.

---

## 1. GİT VE VERSİYON DURUMU

* **Geçerli Dal (Branch):** `dev/windows-gcc-baseline`
* **Uzak Depo (Remote):** `https://github.com/x64bit-source/Metin2-MT2009.git` (Private)
* **Senkronizasyon:** Git geçmişi ve commit doğrulamaları doğrudan `git` komutları üzerinden izlenmektedir.
* **Yerel Dal Koruması:** `.githooks/` (`pre-commit`, `pre-push`) yapılandırması `core.hooksPath = .githooks` ile aktif. `main` dalına doğrudan commit atılması ve `main` hedefine push/force-push/delete yapılması yerel olarak engellidir.

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
  * **Toplu Regresyon Testi:**
    * `libpoly`: **24/24 test başarılı**
    * `libthecore`: **97/97 test başarılı**
    * `libgame`: **28/28 test başarılı** (Gerçek `libthecore.a` ile bağlı)
    * **Toplam:** **149/149 test başarılı (%100)**.
  * **Platform Doğrulama Notu:**
    * **Windows amd64:** MinGW-w64 GCC 16.2.0 (UCRT64) ile 149/149 test 0 hata, 0 uyarı ile tam doğrulandı.
    * **FreeBSD amd64:** GitHub Actions CI iş akışı (Rev 5: Run 38099052441) ile FreeBSD 15.1-RELEASE-p3 amd64 + GCC 14.2.0 ortamında 149/149 test 0 derleme hatası ve 0 derleyici uyarısı ile eksiksiz doğrulandı.
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
   * **Resmî Hedef Sürümler:** Güncel MySQL ve MariaDB hedef sürümleri, ilgili geliştirme aşamasına gelindiğinde resmî kaynaklardan belirlenmelidir.

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
* [ ] **Aşama 10 (Sıradaki Kütüphane Doğrulaması: libsql veya liblua):**
  * Sonraki aşama kullanıcı yönlendirmesine göre başlatılacaktır.

---

## 7. TEK AKTİF GÖREV

* **Aktif Görev:** Aşama 9 (`libthecore`) Windows amd64 ve FreeBSD 15.1 amd64 platformlarında 149/149 test ve 0 hata / 0 uyarı ile başarıyla tamamlandı. Kalıcı MySQL ve MariaDB çift motor uyumluluk hedefleri `AGENTS.md` ve `PROJECT_STATE.md` içine işlendi. Sonraki modüle (`libsql` veya `liblua`) geçiş için kullanıcı onayının ve direktifinin alınması.

---

## 8. SONRAKİ GÜVENLİ ADIM

* Kullanıcı onayıyla birlikte bir sonraki çekirdek kütüphanenin (`libsql` veya `liblua`) incelenmesi, bağımlılıklarının tespiti ve testlerinin hazırlanması. `libsql` aşamasına geçilirse Bölüm 5'teki kalıcı MySQL ve MariaDB çift motor kriterleri doğrultusunda hareket edilmesi. Hiçbir modüle kullanıcı onayı olmadan ve önceki modül eksiksiz doğrulanmadan geçilmemesi.
