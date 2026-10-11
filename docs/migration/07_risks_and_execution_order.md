# MT2009 — KRİTİK GÜVENLİK RİSKLERİ, DÜZELTME PLANI VE GÖÇ SIRALAMASI

**Tarih:** 11 Ekim 2026  

---

## 1. TESPİT EDİLEN KRİTİK ÜRETİM KODU AÇIKLARI

### A. KRİTİK VERİ KAYBI: `IkarusShopCache.cpp:712-714` (`LoadSafebox`)
* **Dosya ve Satır:** `mt2009 - FULL SOURCE/Server Source/Server/db/src/IkarusShopCache.cpp:702-714`
* **Sorunlu Kod:**
  ```cpp
  auto safebox = m_safeboxMap[dwPID] = std::make_shared<OFFLINESHOP_SAFEBOX>();
  ...
  const auto itemquery = CreateSafeboxCacheLoadItemsQuery(dwPID);
  msg = CDBManager::instance().DirectQuery(itemquery.c_str());
  if (!msg || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
      return safebox; // <-- TEHLİKELİ SATIR!
  ```
* **Kök Neden:** Satır 702'de `safebox` boş eşya haritasıyla (`itemsmap`) önbelleğe yazılmaktadır. Eğer ikinci SQL sorgusu (eşyaları çeken sorgu) veritabanı meşguliyeti, ağ kopması veya deadlock nedeniyle başarısız olursa, fonksiyon `nullptr` dönmek yerine bu **boş eşyalı nesneyi** döndürür ve önbellekte tutmaya devam eder!
* **Veri Güvenliği Etkisi (FATAL / PERMANENT DATA LOSS):** Oyuncunun pazar deposundaki tüm eşyalar yok görünür. Daha sonra bu safebox belleğe kaydedilirken veya sunucu kapanışında/tahliyesinde boş eşya listesi SQL'e yazılarak oyuncunun pazar deposundaki **TÜM EŞYALAR KALICI OLARAK SİLİNİR**.
* **Güvenli Düzeltme Yöntemi:**
  İkinci sorgu başarısız olduğunda önbelleğe eklenen boş nesne silinmeli (`m_safeboxMap.erase(dwPID);`) ve açıkça `return nullptr;` dönülmelidir.
* **Zorunlu Doğrulama Testi:** `test_db_cache_integration.cpp` içerisine `LoadSafebox` ikinci sorgu hatası senaryosu eklenmeli; `nullptr` döndüğü ve önbellekte sahte boş safebox kalmadığı kanıtlanmalıdır.

---

### B. SQL INJECTION VE VERİ BOZULMASI RİSKİ: `DBManager.cpp:231-284` (`LocalEscapeString`)
* **Dosya ve Satır:** `mt2009 - FULL SOURCE/Server Source/Server/db/src/DBManager.cpp:231-284`
* **Sorunlu Kod:**
  ```cpp
  static unsigned long LocalEscapeString(char *to, const char *from, unsigned long length) { ... }
  unsigned long CDBManager::EscapeString(...) {
      if (!m_directSQL[iSlot] || !m_directSQL[iSlot]->IsConnected())
          return LocalEscapeString((char *) to, (const char *) from, length);
      return mysql_real_escape_string(...);
  }
  ```
* **Kök Neden:** Bağlantı yokken yerel ASCII kaçış fonksiyonu devreye girmektedir.
  1. Çok baytlı karakter setlerinde (Big5, GBK, SJIS) ikinci baytı `0x5C` (`\`) veya `0x27` (`'`) olan karakterlerde bayt sınırını bilmeden araya ters bölü atarak karakteri bozar (character corruption) veya klasik multibyte SQL injection'a yol açar.
  2. `SQL_MODE=NO_BACKSLASH_ESCAPES` altında MySQL/MariaDB `\` karakterini kaçış saymaz, tek tırnak sadece çift tek tırnak (`''`) ile kaçırılabilir. `LocalEscapeString` ise `\'` üretir. Bağlantı tekrar kurulup sorgu çalıştırıldığında SQL syntax error veya injection riski doğar.
  3. Bağlantı yokken SQL dizesi üretilmeye devam edilmesi bir anti-pattern'dir.
* **Güvenli Düzeltme Yöntemi:** Bağlantı yoksa `EscapeString` sessizce geçersiz string üretmemeli, açık hata döndürmeli veya çağıran yerler veritabanı aktif olana kadar sorguyu ham binary buffer seviyesinde tutmalıdır.

---

### C. FREEBSD DERLEME BETİĞİ PARÇALANMASI: `run_db_integration_tests.sh:67`
* **Dosya ve Satır:** `tests/run_db_integration_tests.sh:67`
* **Kök Neden:** `eval $CXX $FLAGS $RPATH $INCLUDES ... $DB_OBJS ...` satırında `"mt2009 - FULL SOURCE"` yolundaki boşluk ve tire karakterleri parçalanarak GCC'ye `-` argümanı gitmekte ve derleme `g++14: error: -E or -x required when input is from standard input` ile çökmektedir.
* **Güvenli Düzeltme:** `eval` kaldırılmalı, bağımsız argüman dizisi (array) veya tırnaklı POSIX kabuk komutu kullanılmalıdır.

---

## 2. GÖÇ VE İLERLEME SIRALAMASI (EXECUTION SEQUENCE)

```
[AŞAMA 1] ÖNCELİKLİ GÜVENLİK DÜZELTMELERİ
   ├── 1.1: IkarusShopCache.cpp:712-714 LoadSafebox veri kaybı açığını düzelt
   ├── 1.2: DBManager.cpp LocalEscapeString güvenlik açığını sınırlandır
   ├── 1.3: tests/run_db_integration_tests.sh kabuk tırnaklama hatasını gider
   └── 1.4: Rev 19 FreeBSD CI koşusunu (Run ID) 3 motor için de %100 SUCCESS doğrula

[AŞAMA 2] GÖÇ ÖN HAZIRLIKLARI
   ├── 2.1: new_ftp.tar.gz arşivini incele, CONFIG parolalarını şablonlaştır (.env)
   ├── 2.2: .gitignore kurallarını yeni dizin yapısına göre güncelle
   └── 2.3: CMake altyapısını test et

[AŞAMA 3] KAYIPSIZ KLASÖR GÖÇÜ (GIT MV)
   ├── 3.1: Boşluksuz dizin yapısına git mv ile taşıma (server/src, client/src, extern/...)
   ├── 3.2: Include yollarını ve Make/CMake dosyalarını güncelle
   └── 3.3: Windows ve FreeBSD derleme ve 492 test regresyonunu doğrula
```

---

## 3. NİHAİ KABUL VE GÖÇ KARARI

* **Genel Göç Kararı:** **`BLOCKED` (GÖÇ ENGELLENDİ)**
  * **Engelleme Sebepleri:**
    1. `IkarusShopCache.cpp` içerisindeki kalıcı eşya silinmesi (veri kaybı) açığı.
    2. `DBManager.cpp` içerisindeki `LocalEscapeString` SQL güvenlik ve çok baytlı karakter bozulma riski.
    3. Rev 19 FreeBSD CI koşusunun Step 5.1 tırnaklama hatası nedeniyle `FAIL` ile sonuçlanmış olması (82 entegrasyon assertion'ı FreeBSD üzerinde henüz çalışmamıştır).

* **Rev 19 FreeBSD CI Durumu:** **`FAIL`** (Run ID: `38167256710`, Commit: `d96b34371c1577e147fc57565707e028e6bd691c`).
