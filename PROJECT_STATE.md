# METİN2 MT2009 — PROJE DURUM TAKİBİ (PROJECT_STATE.md)

Bu belge, projenin anlık teknik durumunu, doğrulanmış bulgularını ve bir sonraki mikro görevini gösteren **tek doğruluk kaynağıdır (Single Source of Truth)**.

---

## 1. GİT VE VERSİYON DURUMU

* **Geçerli Dal (Branch):** `dev/windows-gcc-baseline`
* **Uzak Depo (Remote):** `https://github.com/x64bit-source/Metin2-MT2009.git` (Private)
* **Senkronizasyon:** Git geçmişi ve commit doğrulamaları doğrudan `git` komutları üzerinden izlenmektedir.

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
  * **Platform Doğrulama Notu:**
    * **Windows amd64:** MinGW-w64 GCC 16.2.0 (UCRT64) ile fiziksel olarak derlendi, 52/52 fonksiyonel ve negatif testlerle 0 hata ve 0 uyarı ile tam doğrulandı.
    * **FreeBSD amd64:** FreeBSD 15.1-RELEASE-p3 amd64 üzerinde GCC 14.2.0 (`g++14`) ile GitHub Actions CI ortamında (`vmactions/freebsd-vm`) fiziksel sanal makinede derlendi; `libpoly` (24/24) ve `libgame` (28/28) olmak üzere 52/52 regresyon testi 0 hata ve 0 uyarı ile tam başarıyla geçti (Run ID: 38096518176).
    * **Doğrulama Durumu:** Çift platform (Windows amd64 & FreeBSD amd64) hedefi bu iki modül için eksiksiz ve kalıcı olarak sağlanmıştır. `libpoly` ve `libgame` aşamasının kapatılması önünde hiçbir engel bulunmamaktadır.
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

## 5. TAMAMLANAN GELİŞTİRME AŞAMALARI

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

---

## 6. TEK AKTİF GÖREV

* **Aktif Görev:** `libpoly` ve `libgame` modüllerinin çift platform (Windows amd64 GCC 16.2 + FreeBSD 15.1 amd64 GCC 14) derleme ve 52/52 regresyon test doğrulaması başarıyla tamamlandı. Modül aşaması eksiksiz kapatıldı.

---

## 7. SONRAKİ GÜVENLİ ADIM

* Kullanıcı talimatı ve onayı doğrultusunda, bir sonraki çekirdek modül olan `libthecore` için Windows amd64 ve FreeBSD amd64 derleme, başlık analizi ve test hazırlığına geçilmesi.
