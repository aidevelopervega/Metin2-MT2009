# MT2009 — REV 19 BASELINE VE FREEBSD CI DENETİM RAPORU

**Tarih:** 11 Ekim 2026  
**Depo:** `x64bit-source/Metin2-MT2009`  
**Dal:** `dev/windows-gcc-baseline`  
**Kaynak Commit SHA:** `d96b34371c1577e147fc57565707e028e6bd691c`  
**İncelenen CI Run ID:** `38167256710`  

---

## 1. REV 19 GITHUB ACTIONS CI SONUCU

GitHub CLI (`gh run view 38167256710`) ile yapılan doğrudan denetim sonuçları:

* **Genel Durum (Conclusion):** `failure` (BAŞARISIZ)
* **İlişkili Commit (headSha):** `d96b34371c1577e147fc57565707e028e6bd691c` (Doğrulandı)
* **İş Akışı Süresi:** 4 dakika 17 saniye

### Matris İşleri ve Sonuçları:

| İş (Job) Adı | Job ID | Başlama / Bitiş | Gerçek Sonuç | Hata Adımı |
| :--- | :--- | :--- | :--- | :--- |
| **FreeBSD 15.1 - MySQL 8.4 LTS** | `114552279062` | 19:15:42Z - 19:19:05Z | **FAILURE** | Step 5.1 (Entegrasyon Testi) |
| **FreeBSD 15.1 - MariaDB 12.3 LTS** | `114552279226` | 19:15:42Z - 19:19:55Z | **FAILURE** | Step 5.1 (Entegrasyon Testi) |
| **FreeBSD 15.1 - MariaDB 11.4 LTS** | `114552279257` | 19:15:42Z - 19:19:59Z | **FAILURE** | Step 5.1 (Entegrasyon Testi) |

---

## 2. KÖK NEDEN ANALİZİ (ROOT CAUSE)

Her 3 FreeBSD işinde de başarısızlık derleme veya veritabanı motorundan kaynaklanmamıştır:

1. **Başarılı Aşamalar:**
   - 410/410 modül regresyon testi (libpoly: 24, libthecore: 97, libgame: 28, libsql: 141, db_cache: 120) **0 hata ile PASS**.
   - Gerçek 64-bit `db_r1` ikili dosyası `gmake` ile başarıyla derlendi ve bağlandı:
     `ELF 64-bit LSB executable, x86-64, dynamically linked (16M)`.
2. **Başarısızlık Nedeni (Step 5.1):**
   - Hata `tests/run_db_integration_tests.sh` betiğinde oluştu.
   - İlgili satır (Satır 67):
     ```sh
     eval $CXX $FLAGS $RPATH $INCLUDES \
         "$ROOT_DIR/tests/test_db_cache_integration.cpp" \
         $DB_OBJS \
         ...
     ```
   - `$DB_OBJS` değişkeni `.obj/*.o` nesne dosyalarını içermektedir. Dosya yolları `.../mt2009 - FULL SOURCE/...` şeklinde boşluk ve tire karakterleri barındırmaktadır.
   - `eval` komutu çalıştırıldığında tırnaksız genişleme nedeniyle `"mt2009 - FULL SOURCE"` parçalanarak `mt2009`, `-`, `FULL`, `SOURCE` sözcüklerine ayrılmış; aradaki tek başına duran `-` argümanı GCC tarafından **"kaynak kodu standart girdiden (stdin) oku"** bayrağı olarak yorumlanmıştır.
   - Hata çıktısı:
     ```text
     g++14: error: -E or -x required when input is from standard input
     ```
   - Betik `set -e` nedeniyle anında çıkış kodu 1 ile sonlanmış; **82 entegrasyon assertion'ı FreeBSD üzerinde HİÇ ÇALIŞTIRILMAMIŞTIR**.

---

## 3. DOĞRULANAN VE DOĞRULANMAYAN TESTLER

* **Windows amd64 GCC 16.2.0:**
  * 410/410 Birim Regresyon Testi: **PASS (%100)**
  * 82/82 DB Cache Entegrasyon Testi (`tests/test_db_cache_integration.cpp`): **PASS (%100)**
  * 219/219 Canlı Veritabanı Testi (`tests/test_live_db.cpp`): **PASS (%100)**
* **FreeBSD 15.1 amd64 GCC 14.2.0 (Rev 19):**
  * 410/410 Birim Regresyon Testi: **PASS (%100)**
  * Gerçek 64-bit `db_r1` Binary Derlemesi: **PASS (%100)**
  * 82/82 DB Cache Entegrasyon Testi: **NOT RUN / FAIL** (Kabuk tırnaklama hatası nedeniyle çalıştırılamadı)
  * Canlı Veritabanı Testleri: **NOT RUN** (Step 5.1 çöktüğü için sonraki adıma geçilemedi)

---

## 4. NİHAİ REV 19 KARARI

**Rev 19 FreeBSD CI Durumu: `FAIL`**

Önceki raporda ve `PROJECT_STATE.md` içerisinde yer alan "Rev 19 %100 BAŞARIYLA TAMAMLANDI" ifadesi gerçeği yansıtmamaktadır. FreeBSD üzerinde 82 assertion koşulmamıştır. Bu durum düzeltilmeden Aşama 11.4 kapatılamaz.
