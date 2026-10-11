# MT2009 — SAHTE PASS (FALSE-GREEN) VE HATA MASKESİ DENETİMİ

**Tarih:** 11 Ekim 2026  

---

## 1. SAHTE PASS RİSKLERİ ANALİZİ

CI iş akışlarında, kabuk betiklerinde ve PowerShell çalıştırıcılarında tespit edilen kalıplar ve değerlendirmeleri:

### A. KRİTİK: `.github/workflows/freebsd-gcc-test.yml` — Satır 150 (`exit 0`)
* **Kod:**
  ```sh
  if ! pkg install -y "$SERVER_PKG"; then
    echo " Durum: $ENGINE_NAME -> BLOCKED / NOT TESTED (BINARY PKG NOT PUBLISHED IN 15.1 REPOSITORY)"
    exit 0
  fi
  ```
* **Etki Değerlendirmesi:** **YÜKSEK RİSK (FALSE GREEN POTANSİYELİ)**.
  Eğer FreeBSD 15.1 paket deposunda `$SERVER_PKG` (`mariadb114-server`, `mariadb123-server` veya `mysql84-server`) geçici bir ağ kesintisi veya paket kaldırma nedeniyle kurulamazsa, betik `exit 0` döndürür. GitHub Actions bu işi **BAŞARILI (YEŞİL TİK)** olarak işaretler. Oysa canlı veritabanı testleri HİÇ KOŞULMAMIŞTIR!
* **Düzeltme Önerisi:** Paket kurulamadığında `exit 0` yerine `exit 1` verilmeli; test koşulmadıysa durum yeşil olarak maskelenmemelidir.

### B. `|| true` Kullanımları
* **Satır 124 (`pkg -vv || true`):** Tanı amaçlıdır, test sonucunu etkilemez.
* **Satır 147 (`pkg search ... || true`):** Tanı amaçlıdır, test sonucunu etkilemez.
* **Satır 154 (`pkg info ... || true`):** Bilgilendirme amaçlıdır.
* **Satır 160 (`file ... || true`):** İkili dosya formatını konsola basar. Öncesindeki `gmake` başarısız olursa `set -e` veya kabuk hata yakalaması devreye girer.
* **Satır 186 ve 220 (`mariadb-install-db ... || true` / `mysqld --initialize ... || true`):** Daemon başlatma öncesi hata maskeleme potansiyeli taşır. Ancak takip eden `ping` döngüleri (`mariadb-admin ping`, `mysqladmin ping`) sunucunun gerçekten ayakta olduğunu doğrular; ayakta değilse `exit 1` ile durur.
* **Satır 264-265 (`kill $SERVER_PID || true`):** Temizlik adımıdır, zararsızdır.

### C. `continue-on-error` Kontrolü
* İş akışında hiçbir adımda `continue-on-error: true` KULLANILMAMIŞTIR. Adımlardan biri başarısız olduğunda süreç anında fail olur (örnek: Rev 19 Step 5.1'deki fail durumunda Step 6 ve 7'ye geçip işi tamamlayarak çıkışı failure vermiştir).

### D. PowerShell `Out-Null` ve `$LASTEXITCODE` (`tests/run_tests.ps1`)
* `$ErrorActionPreference = "Stop"` aktiftir.
* `Out-Null` yalnızca `New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null` (dizin oluşturma) için kullanılmıştır; derleme veya test çıktısını yutmaz.
* Her derleme ve test yürütmesi sonrasında `if ($LASTEXITCODE -ne 0) { throw ... }` denetimi mevcuttur. Çıkış kodları eksiksiz yakalanır.

---

## 2. TEST TÜRLERİ VE DÜRÜST SINIFLANDIRMA

Mevcut test paketlerinin mimari seviyeleri:

1. **Birim / Simülasyon Testleri (`tests/test_db_cache.cpp` — 120 Assertion):**
   - Bellekteki `cache<T>` şablon sınıfının durum geçişlerini ve versiyon korumasını mock yapılarla test eder.
   - Gerçek `CClientManager`, `CDBManager` veya ağ/SQL kuyruğu çalıştırmaz.
2. **Bileşen Entegrasyon Testleri (`tests/test_db_cache_integration.cpp` — 82 Assertion):**
   - Gerçek `CClientManager`, `CPlayerTableCache`, `CItemCache`, `CDBManager`, `CAsyncSQL` sınıflarını çalıştırır.
   - SQL bağlantı ve callback yanıtlarını kontrollü hata enjeksiyonu (fault injection / synthetic callback) ile simüle ederek kriz anlarını (deadlock, kopma, UNKNOWN) test eder.
   - Bu test bir "Canlı Veritabanı E2E Testi" DEĞİLDİR; Component Integration Test'tir.
3. **Canlı E2E Ağ ve Veritabanı Testleri (`tests/test_live_db.cpp` — 219 Assertion):**
   - Gerçek çalışan MySQL 8.4 LTS, MariaDB 12.3 LTS ve MariaDB 11.4 LTS daemon'larına fiziksel TCP soketiyle bağlanır.
   - Canlı TLS el sıkışması, TCP Fault Proxy ile canlı hat kesme ve UNKNOWN işlem durumunu test eder.
