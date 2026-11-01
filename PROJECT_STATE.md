# METİN2 MT2009 — PROJE DURUM HAFIZASI (`PROJECT_STATE.md`)

**Son Güncelleme:** 1 Kasım 2026 | **Aktif Dal:** `dev/windows-gcc-baseline` | **Uzak Depo:** `aidevelopervega/Metin2-MT2009` (`PUBLIC`)

> **Belge Disiplini:** Depoda yalnızca 3 Markdown belgesi tutulur:
> 1. [`AGENTS.md`](file:///D:/mt22009/AGENTS.md) — Kalıcı geliştirme, güvenlik ve çalışma kuralları.
> 2. [`PROJECT_STATE.md`](file:///D:/mt22009/PROJECT_STATE.md) — Güncel, doğrulanmış proje durumu ve önemli teknik kararlar.
> 3. [`docs/SERVER_X64_GOALS.md`](file:///D:/mt22009/docs/SERVER_X64_GOALS.md) — Sunucu + istemci native x64 teknik sözleşmesi, gerçek engeller ve kabul kriterleri.

---

## 1. Güncel Doğrulanmış Durum Özeti

| Alan / Bileşen | Durum | Son Doğrulanmış Kanıt ve Commit SHA |
| :--- | :---: | :--- |
| **Sunucu Kaynak Kodu (`game`, `db`, `qc`, `libthecore`, `libsql`, `libgame`, `libpoly`, `liblua`)** | **`VERIFIED`** | Windows amd64 (`PE32+`, MinGW-w64 GCC 16.2.0 UCRT) ve FreeBSD 15.1 amd64 (`ELF64`, GCC 14.2.0) üzerinde tek kaynak koddan `-std=c++20 -Wall -Wextra -pipe` (`0` `-Wno-*` bastırma bayrağı) ile **`0` hata ve `0` uyarı** (`d52603461d0789a96ce888661b9574b26db8aea1` — ShopEx, Exchange, SpecialShop ve Ikarus Offline Shop güvenlik/taşma sertleştirmeleri dahil). |
| **FreeBSD 15.1 amd64 GCC 14 + MariaDB 12.3.3 LTS Tam Sunucu Doğrulaması (`M-10` & 27-Dosya Sunucu/Protokol Sertleştirmeleri)** | **`VERIFIED`** | **GitHub Actions Run ID:** `41321849182` \| **Head SHA:** `d52603461d0789a96ce888661b9574b26db8aea1` \| **Sonuç:** **`SUCCESS`** (`3m 26s`, Job ID `124893559403`; önceki koşular: `41311852071`, `41183346026`). `CTest 10/10` (`test_packet_wire 25/25 [171/171]`), `--check-lua 11/11` (`Check 11b` ShopEx 3 sekme / `12111 B` ve `Check 11c` `CExchange::AddGold` `54 B` + `ikashop::CShopItem::CanBuy` dahil), `Quest 202/202`, `test_live_db 354/354`, `--test-itemshop 15/15`, `test_db_cache_integration 162/162 & 187/187`, `test_server_smoke_client 29/29` (`>2^32` AID/PID/envanter/quest/restart/replay + 8 servisli serverfiles + cross-core `ch11 <-> ch12` `WarpSet`), gerçek `pfctl -nf` (`exit 0`), audit/TLS, backup/restore ve `[4.A]` MariaDB restart kalıcılığı `PASS`. |
| **Windows x64 + MariaDB 12.3.3 LTS Birim ve Canlı Entegrasyon Testleri (`M-01`..`M-09`, `INT-01`, `F14`..`F16`)** | **`VERIFIED`** | `CTest 10/10` (`libsql 203/203`, `test_packet_wire 171/171`, `test_db_cache 139/139`, `test_db_cache_integration 162/162` çevrimdışı / `187/187` canlı MariaDB, `test_lua55_quest 11/11`, `test_auth_security 423/423`), `--check-lua 11/11` (`Check 11b` & `11c` dahil), `--test-itemshop 15/15`, `--protocol-framing-audit 10/10 (4/4)`, `test_live_db 354/354`, `run_quest_matrix.py 202/202`, `run_kademe_c_live.ps1 6/6 PASS`, `scan_compiler_warnings.py 285/285 TU` (`0` yüksek riskli uyarı). |
| **Native x64 İstemci (`client/src/**`, `extern/client/**`, `PE32+` & `0x20090064U` Protokolü) & Birleşik SDK Standardı** | **`UNVERIFIED`** (`sdk_inventory_complete = PASS`, `unused_sdks_removed = PASS`, `server_client_sdk_versions_unified = PASS`, `sdk_x64_headers_verified = PASS`, `sdk_x64_binaries_verified = PASS`, `sdk_runtime_tests = PASS`, `python3_sdk_verified = PASS`, `python3_client_embedded = PASS`, `python3_real_modules = PASS`, `miniaudio_dependency_acquired = PASS`, `miniaudio_real_api_compiled = PASS`, `miniaudio_runtime_tested = PASS`, `miniaudio_production_adapter = PASS`, `real_eterpack_audio = PASS`, `physical_audio_output = UNVERIFIED`, `d3d9ex_api_test = PASS`, `d3d9ex_renderer_integration = PASS`, `client_link = BLOCKED`, `gameplay = BLOCKED`) | **İzinli ve aktif geliştirme alanı.** Depodaki tüm üçüncü taraf bağımlılıkların makine-doğrulanabilir envanteri ([`tests/sdk_inventory_lock.json`](file:///D:/mt22009/tests/sdk_inventory_lock.json)) çıkarılmış, kullanılmadığı kanıtlanan `7` SDK grubu (`opencv`, `OptionParser`, `inih`, `minIni`, `lz4`, `snappy`, istemci `mysql`/`openssl`) ile gölgeleyen eski başlık/kütüphaneler (`extern/client/include/{boost,rapidjson,d3d9*.h,d3drm*.h}`, `extern/client/cryptopp` 8.2.0, `backup_dx8`, `old_python_not_static`, `121` gevşek `.obj`) temizlenmiş; sunucu ve istemci ortak SDK sürümleri (**Boost `1.92.0`**, **RapidJSON `1.1.0`**, **fmt `12.2.0`**, **msl `1.2.1`**, **Crypto++ `8.9.0`** `193/193` başlık + `cryptlib-x64.lib`, **DevIL `1.8.0`** `8/8` başlık + `DevIL_x64.dll` + `devil_x64.lib`, **LZO `2.10`** `14/14` başlık + `lzo2_x64.lib`) bayt-bayt eşitlenmiş ve [`tests/verify_sdk_inventory.py`](file:///D:/mt22009/tests/verify_sdk_inventory.py) (**`216/216 PASS`**) + [`tests/test_unified_sdks_x64.cpp`](file:///D:/mt22009/tests/test_unified_sdks_x64.cpp) (**`49/49 PASS`**) ile doğrulanmıştır. `MarkImage.h` içindeki `typedef unsigned long Pixel` LP64/LLP64 uyumsuzluğu her iki tarafta `uint32_t` olarak giderilmiştir. `tests/test_python3_real_modules_x64.cpp` (`32/32 PASS`), `tests/test_mileslib_miniaudio_production_x64.cpp` (`36/36 PASS`), `tests/test_miniaudio_x64_prototype.cpp` (`42/42 PASS`, `physical_audio_output = UNVERIFIED`) ve `tests/test_d3d9ex_x64.cpp` (`619/619 PASS` — kalıcı Direct3D 9Ex) gerçek `cryptlib-x64.lib` ve `lzo2_x64.lib` ile bağlanarak geçmiştir. Tam istemci `PE32+` bağlama (`link.exe /MACHINE:X64`), yalnızca kalan `3` harici 32-bit (`14C`) kütüphane (`Granny2`, `SpeedTreeRT`, `D3DX9`) nedeniyle **`BLOCKED`** durumdadır. |

---

## 2. Depo Yapısı, Araç Zinciri ve Yerel Ortam Envanteri

### 2.1 Dizin Yapısı
* `server/src/{common,db,game,libsql,libthecore,libgame,libpoly,liblua,qc}` — C++20 x64 sunucu ve çekirdek kütüphane kaynakları.
* `server/scripts/{windows,freebsd}/` — Serverfiles kurulum (`install.*`), başlatma (`start.*`), durdurma (`stop.*`), yedekleme (`backup_mariadb.*`), log rotasyonu (`rotate_logs.*`), `audit_guard.sh` ve `pf_guard.sh` betikleri.
* `server/config/*.example` — Sırsız yapılandırma şablonları (`serverfiles.env.example`, `mariadb-security.cnf.example`, `pf.conf.example`, `server_tls.cnf.example`).
* `server/serverfiles_share.tar.xz` — Oyun verilerinin (`share/data`, `share/locale/poland`, `share/package` — `14.341` dosya, `202` quest) tek resmî ve parolasız arşivi.
* `client/src/` — `Metin2Client.sln` ve 16 istemci modülü (`UserInterface`, `EterLib`, `GameLib`, `EterBase`, `EffectLib`, `EterGrnLib`, `MilesLib`, `SpeedTreeLib`, `EterPythonLib`, `EterPack`, `ScriptLib`, `EterLocale`, `SphereLib`, `EterImageLib`, `PRTerrainLib`, `CWebBrowser`).
* `extern/{common,server,client}/` — Üçüncü taraf başlıklar ve kütüphaneler.
* `sql/*.sql` & `sql/migrations/*.sql` — MariaDB 12.3 LTS temel şemaları (`account`, `common`, `log`, `player`, `world` — toplam `126` InnoDB tablo) ve migration betikleri.
* `tests/` — Birim, entegrasyon, canlı MariaDB ve ağ protokolü duman testi kaynakları ile PowerShell/POSIX/Python betikleri.
* `CMakeLists.txt`, `CMakePresets.json`, `cmake/` — Birleşik CMake + Ninja derleme altyapısı (`windows-gcc-ucrt`, `freebsd-gcc14`).
* `.github/workflows/freebsd-gcc-test.yml` & `ci/run-full-freebsd.txt` — Yalnızca `ci/run-full-freebsd.txt` değiştiğinde tetiklenen FreeBSD 15.1 amd64 GCC 14 CI iş akışı.
* `.githooks/` — `main` dalına doğrudan commit/push engelleyen Git kancaları (`core.hooksPath = D:/mt22009/.git/hooks`).

### 2.2 Doğrulanmış Araç Zinciri ve Sunucu Harici Bağımlılıkları (`Extern`)

| Bileşen | Windows Yerel Sürümü | FreeBSD 15.1 CI Sürümü | Not |
| :--- | :--- | :--- | :--- |
| **Sunucu Derleyicisi** | MinGW-w64 GCC `16.2.0` (`x86_64-ucrt-posix-seh`) | FreeBSD GCC `14.2.0` (`amd64`) | `-std=c++20 -Wall -Wextra -pipe` (`0` uyarı) |
| **İstemci Derleyicisi** | MSVC / Visual Studio 2022 (`v143`, `/std:c++20`) | — | Native x64 (`PE32+`) dönüşümü için izinli |
| **Derleme Sistemi** | CMake `4.4.2` + Ninja `1.13.2` | CMake + Ninja (`freebsd-gcc14`) | Windows derlemesi: `tests/run_limited.ps1 -Build` |
| **Veritabanı Motoru** | MariaDB `12.3.3-MariaDB` (`Win64`) | MariaDB `12.3.3-MariaDB` (`FreeBSD amd64`) | Tek resmi DB standardı; Connector/C `3.4.10` (`libmariadb`) |
| **Lua Motoru** | Lua `5.5.1` (`server/src/liblua`) | Lua `5.5.1` (`server/src/liblua`) | Tek üretim/derleme motoru (eski Lua 5.0/5.2 kaldırıldı) |
| **Sunucu & İstemci Ortak Extern Standardı** | Boost `1.92.0`, Crypto++ `8.9.0`, fmt `12.2.0`, RapidJSON `1.1.0`, msl `1.2.1`, DevIL `1.8.0`, LZO `2.10`, OpenSSL `3.5.6`, zlib `1.3.2` | Aynı kaynak/paket sürümleri | Tamamı 64-bit (`0x8664`) derlenip bayt-bayt başlık eşitliği (`tests/verify_sdk_inventory.py` `216/216 PASS`) ile kilitlenmiştir |

### 2.3 Yerel `.gitignore` İstisnaları ve İzole Test Ortamı (Git Dışında Korunanlar)

Aşağıdaki dosya ve dizinler güvenlik veya boyut sınırı nedeniyle `.gitignore` ile Git takibi dışında tutulur ve yerel diskte korunur:
1. **`D:\mt22009\scratch\handoff\isolated_db\mariadb-12.3.3-winx64\`:** Kademe C canlı testleri için kullanılan taşınabilir MariaDB `12.3.3` Win64 ikili dağıtımı (sistem servisi değildir; testlerde yalnızca `127.0.0.1:3307` üzerinde geçici veri diziniyle başlatılır).
2. **`D:\mt22009\server\files\` ve `D:\mt22009\server\files\serverfiles.env`:** Yerel Windows serverfiles çalışma dizini ve `chmod/icacls` korumalı yerel parola yapılandırması (asla Git'e eklenmez; şablonu `server/config/serverfiles.env.example` içindedir).
3. **`D:\mt22009\extern\client\lib\cryptlib-Debug.lib` (`110.819.592 B` / `~110,8 MB`):** GitHub'ın `100 MB` dosya boyutu sınırını aştığı için `.gitignore` içinde dışlanmıştır.
4. **`D:\mt22009\client\files\` ve `locales-mt2009.rar`:** İstemci çalışma zamanı varlıkları (`pack/`, `BGM/`, `miles/`, `lib/` ve mevcut 32-bit DLL/ikililer).
5. **`build/`, `object/`, `scratch/`:** Yeniden üretilebilir CMake/Ninja derleme çıktıları, `qc` çıktıları ve geçici test dosyaları.

### 2.4 Windows Derleme ve Test Komutları
* **Zorunlu Windows Sunucu Derleme Komutu (`%50` CPU Hard-Cap, `0xFC0` 6 LP, `-j6`, `BelowNormal`):**
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_limited.ps1 -Build
  ```
* **Derleme Dışı Testler (CPU kısıtlaması uygulanmaz, tam hızda çalışır):**
  - Birim testleri: `ctest --preset windows-gcc-ucrt --output-on-failure` (`10/10 PASS`)
  - Lua 5.5.1 çalışma zamanı öz-denetimi: `.\server\src\game\game_r41023.exe --check-lua` (`11/11 PASS`)
  - Quest derleme matrisi: `python -X utf8 tests/run_quest_matrix.py` (`202/202 PASS`)
  - Kademe C tam canlı doğrulama (izole MariaDB 12.3.3 port `3307`): `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_kademe_c_live.ps1 -IncludeSecurityAndBackup` (`6/6 PASS`)
  - Genişletilmiş derleyici uyarı taraması: `python tests/scan_compiler_warnings.py` (`285/285 TU`, `0` yüksek riskli uyarı)
  - SDK envanter kilidi ve başlık/mimari eşitlik doğrulaması: `python tests/verify_sdk_inventory.py` (`216/216 PASS`)

---

## 3. SQL Şema ve Migration Uygulama Sırası

1. **Sıfırdan Temiz Kurulum (`126` InnoDB Tablo, `utf8mb4`, Strict SQL Mode):**
   * `sql/account.sql` (`10` tablo)
   * `sql/common.sql` (`10` tablo)
   * `sql/log.sql` (`39` tablo)
   * `sql/player.sql` (`40` tablo)
   * `sql/world.sql` (`27` tablo)
2. **Mevcut Veritabanlarını Yükseltirken Zorunlu Migration Sırası (İdempotent):**
   * **Adım 1:** `sql/migrations/rev47_account_security_upgrade.sql` ve `sql/migrations/rev47_strict_sql_mode_alignment.sql` (Argon2id `varchar(128)` parola sütunu, `STRICT_TRANS_TABLES` / `NO_ZERO_DATE` sıfır tarih ve varsayılan değer hizalaması)
   * **Adım 2:** `sql/migrations/rev51_halloween_hair_attr_migration.sql` ve `sql/migrations/rev51_log_schema_64bit_alignment.sql` (7 Cadılar Bayramı saç seçeneği özelliği ve `log` şeması 64-bit hizalaması)
   * **Adım 3:** `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql` (`log.ikarusshop_log` ve `log.itemshop` tabloları, `bigint(20)` fiyat/bakiye uyumu)
   * **Adım 4:** `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql` (`account`, `common`, `log`, `player` şemalarında tüm kalıcı kimliklerin `BIGINT UNSIGNED` genişletmesi, `account.itemshop_tx_journal` `req_id` + `'RECOVERING'` durumu, `idx_itemshop_tx_aid_req` ve `idx_item_award_why` benzersiz indeksleri)

> **Not:** Mevcut üretim verisi üzerinde yapısal migration çalıştırmadan önce doğrulanmış yedek (`backup_mariadb.ps1` / `backup_mariadb.sh` `HMAC-SHA256(v2)`) alınması zorunludur (`AGENTS.md` §4.7).

---

## 4. Kalıcı Mimari, Protokol ve Veri Bütünlüğü Kararları

1. **Ağ Protokolü ve Çerçeveleme (`MT2_PROTOCOL_VERSION_X64 = 0x20090064U`):**
   * Tüm ağ paketleri `#pragma pack(1)` ile hizalanır; `long`/`DWORD` yerine sabit genişlikli `int32_t`, `uint32_t`, `int64_t` (`YANG`) ve `uint64_t` kullanılır.
   * `TPacketCGLogin3` (`325 B`, TEA `328 B`, ofset `52`) ve `TPacketCGLogin2` (`56 B`, ofset `51`) `dwProtocolVersion = 0x20090064U` taşır. Eski boyutlu çerçeveler `250 ms` boşta yarım çerçeve zamanlayıcısı (`partial_login_event`) sonunda `"VERSION"`, yanlış sürümlü tam çerçeveler ise SQL sorgusu çalıştırılmadan ve `dwLoginKey` tüketilmeden `"WRONGVER"` ile reddedilir.
2. **64-Bit Ekonomi (`GOLD_MAX = 1.000.000.000.000` Yang) ve Anti-Dupe Korumaları:**
   * `GOLD_MAX` ve `EXCHANGE_GOLD_MAX` `10^{12}` (`1 Trilyon` Yang) olarak sabitlenmiştir; NPC/PC dükkanları, Ikarus Offline Shop, Guild kasası/SP, Monarch hazinesi, Cube, DragonSoul, Safebox ve Quest/Lua para işlemlerinde 64-bit taşma ve negatif fiyat korumaları zorunludur.
   * `AutoStackItemProto`, `AutoStackItem`, `AutoGiveItem`, `MoveItem`, `SwapItem` ve `DragonSoul` rafine işlemlerinde aktif takas (`IsExchanging()`), kilit (`isLocked()`), pazar (`IsSellingItem()`) ve ekipman kontrolleri ile eşya çoğaltma (dupe) yolları kapatılmıştır.
3. **64-Bit Kalıcı Kimlikler ve Lua 5.5.1 Kesinlik Sınırı:**
   * `AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`, `TX_ID` ve `GUILD_TOKEN` uçtan uca `uint64_t` (`BIGINT UNSIGNED`) olarak işlenir; oturum içi `VID` ise çakışma korumalı `uint32_t` olarak korunur.
   * `CItemIDRangeManager::cs_dwMaxItemID = 9.223.372.000.000.000ULL` (`<= INT64_MAX`) sınırı ile yeni üretilen tüm kalıcı kimliklerin Lua 5.5.1 `lua_Integer` (`int64_t`) içinde pozitif kalması garanti altına alınmıştır; veritabanında önceden mevcut `>= 2^63` kimlikler de `m2_lua_pushuint64`/`m2_lua_touint64` ile kayıpsız taşınır.
   * `CQuestManager::QuestStateToIndex` içinde `"start"` durumu her zaman `0`, diğer durumlar `get_crc32` (`quest_CRC`) olarak kodlanır.
4. **ItemShop Dayanıklı İşlem Günlüğü (`account.itemshop_tx_journal`) ve `CAsyncSQL` Spool:**
   * Satın alma işlemleri `RESERVED -> CHARGED -> RECOVERING -> DELIVERED / REFUNDED` durum makinesi, kalıcı `(aid, req_id)` idempotency anahtarı, senkron `PersistInventoryItemSynchronously` yazımı ve `GET_LOCK('mt2_is_tx_<tx_id>', 0)` çoklu süreç yarış kilidi ile korunur.
   * `CAsyncSQL` bağlantı kopmalarında yazma sorgularını `0600` izinli dayanıklı spool dosyasına alır ve yeniden bağlanmada `m_bDurableQueryInFlight` / `WaitForDurableQueueIdle` korumasıyla sırayla yeniden oynatır.
5. **Kimlik Doğrulama, Oturum ve Altyapı Güvenliği:**
   * Parolalar Argon2id (`$argon2id$v=19$m=65536,t=3,p=1$...`) ile doğrulanır; eski MySQL41 hash'leri ilk başarılı girişte şeffaf biçimde Argon2id'ye yükseltilir.
   * `LoginByKey` oturum anahtarı (`dwLoginKey`) tek kullanımlıktır (`m_bLoginKeyConsumed`); tekrar oynatma (replay) girişimleri anında reddedilir.
   * FreeBSD üretim/test betiklerinde en az yetkili `mt2server`/`mt2leastpriv` ayrımı, `HMAC-SHA256(v2)` yedek doğrulaması, `audit_guard.sh` maskelemesi ve `pf_guard.sh` gerçek `pfctl -nf` (`kldload -n pf` + `exit code 0`) doğrulaması zorunludur.

---

## 5. FreeBSD 15.1 amd64 GCC 14 CI Doğrulama Kaydı (`M-10` & 27-Dosya Sunucu/Protokol Sertleştirmeleri `VERIFIED`)

* **Son Doğrulanmış CI Koşusu:** GitHub Actions Run ID **`41321849182`** (Önceki koşular: **`41311852071`**, **`41183346026`**)
* **Dal / Commit SHA:** `dev/windows-gcc-baseline` / `d52603461d0789a96ce888661b9574b26db8aea1` (ShopEx, Exchange, SpecialShop `requiredCountsByVnum` & `255` paket sınırı, Ikarus Offline Shop `CanBuy` / `self-buy` / `cross-shop` / `pTable` sertleştirmeleri ve `Check 11c` dahil)
* **Genel Sonuç ve Süre:** **`SUCCESS`** (`3m 26s`, Job ID `124893559403`, `FreeBSD 15.1 - MariaDB 12.3 LTS`)
* **Doğrulanan Aşamalar:**
  1. **Derleme ve ELF64 Bağlama:** `db_r41023`, `game_r41023`, `qc` ve tüm test ikilileri FreeBSD 15.1 GCC 14.2.0 altında `0` hata ve `0` uyarı ile derlendi; `readelf -d` üzerinde yalnızca `libmariadb.so.3` dinamik bağımlılığı doğrulandı.
  2. **Birim, Lua ve Quest Testleri:** `CTest 10/10 PASS` (`libpoly 24/24`, `libthecore 105/105`, `libgame 28/28`, `libsql 203/203`, `test_db_cache 139/139`, `test_db_cache_integration 162/162`, `test_packet_wire 25/25 [171/171]`, `test_lua55_quest 11/11`, `test_qc_compiler_lua55 1/1`, `test_auth_security 423/423`), `game_r41023 --check-lua 11/11 PASS` (`Check 11b` ShopEx 1/2/3 sekme / `12111 B` ve `Check 11c` `CExchange::AddGold` `54 B` + `ikashop::CShopItem::CanBuy` dahil), `run_quest_matrix.py 202/202 PASS`.
  3. **Canlı MariaDB 12.3.3 LTS Testleri:** `run_live_tests.sh` (`128/128 PASS`), `game_r41023 --test-itemshop` (`15/15 PASS`), `test_live_db` (`354/354 PASS`), `test_db_cache_integration` (`[Bonus 4.0]` canlı `187/187 PASS`).
  4. **64-Bit (`> 2^32`) Protokol ve Kalıcılık Duman Testi (`run_server_smoke_test.sh [1/4]..[4/4]` & `test_server_smoke_client 29/29 PASS`):** `test_server_smoke_client --protocol-framing-audit` (`10/10 PASS`), `aid = 5000000001`, `pid = 5000000001`, `item_id = 5000000001..5000000002` ile canlı Auth/Game girişi, envanter (`window=1, pos=3..4`), quest (`relogin_smoke.status=42`) yazımı, yeniden giriş doğrulaması, servis yeniden başlatma sonrası kalıcılık ve `dwLoginKey` replay reddi (`ALREADY`) **`PASS` (`0 syserr`)**.
  5. **8 Servisli Serverfiles ve Güvenlik Kontrolleri (`run_freebsd_serverfiles_test.sh [1/7]..[7/7]` + `[Bonus 4.A..4.F]` + `[4.A]`):** 8 servis (`db`, `auth`, `ch11`, `ch12`, `ch21`, `ch31`, `ch41`, `ch99`) `LISTENING` ve `0 syserr`, cross-core `ch11 -> ch12 -> portal 10002 WarpSet -> ch11` (`[cross-core-step1..5] PASS`), `backup_mariadb.sh` `HMAC-SHA256(v2)` yedekleme/geri yükleme `PASS`, `audit_guard.sh` v2 maskeleme `PASS`, `126/126` InnoDB tablo doğrulaması `PASS`, `pf_guard.sh` üzerinde **gerçek `pfctl -nf` sözdizimi doğrulaması (`exit code 0`) `PASS`** ve `[4.A]` MariaDB restart kalıcılığı (`127.0.0.1|OFF|ON|CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT`) **`PASS`**.

---

## 6. Aktif Geliştirme Alanı ve Kalan Harici Engeller (`BLOCKED` / `UNVERIFIED`)

* **Sunucu (`server/**`, `sql/**`, `tests/**`):** Windows x64 ve FreeBSD 15.1 amd64 (Run `41321849182` `SUCCESS`) üzerinde tüm hedefler, ShopEx sınır sertleştirmeleri, Exchange/Trade (`14B`/`54B`), SpecialShop çoklu yuva materyal toplama (`requiredCountsByVnum`) ve `255` paket sınırı, Ikarus Offline Shop `CanBuy`/`self-buy`/`cross-shop` kontrolleri ve `MarkImage.h` (`typedef uint32_t Pixel;`) LP64/LLP64 uyumu **`VERIFIED`** durumdadır (`test_packet_wire 171/171 PASS`, `ctest 10/10 PASS`, `--check-lua 11/11 PASS`, `Quest 202/202 PASS`).
* **Eksiksiz SDK Envanteri, Ortak Sunucu/İstemci Sürüm Standardı ve Kullanılmayan SDK Temizliği (`sdk_inventory_complete = PASS`, `unused_sdks_removed = PASS`, `server_client_sdk_versions_unified = PASS`, `sdk_x64_headers_verified = PASS`, `sdk_x64_binaries_verified = PASS`, `sdk_runtime_tests = PASS`):**
  - Depodaki tüm `server`, `common` ve `client` üçüncü taraf bağımlılıkları taranarak [`tests/sdk_inventory_lock.json`](file:///D:/mt22009/tests/sdk_inventory_lock.json) içinde SHA-256, sürüm, lisans, mimari ve kullanım noktalarıyla kilitlenmiş; [`tests/verify_sdk_inventory.py`](file:///D:/mt22009/tests/verify_sdk_inventory.py) (**`216/216 PASS`**) ile otomatik drift/mimari/başlık-eşitliği denetimi kurulmuştur.
  - **Ortak Sunucu/İstemci SDK Sürümleri ve Native x64 (`0x8664`) İkilileri:**
    * **Boost `1.92.0` (`109200`):** `extern/common/include/boost` tek kaynak olarak birleştirildi (`extern/client/include/boost` altındaki eski 2023 `.ipp` gölgelemeleri ve 32-bit `libboost_*-x32-1_67.lib` dosyaları kaldırıldı; `client/src/UserInterface/Hardware.cpp` header-only `std::filesystem` + `GetAdaptersInfo` kullanacak şekilde güncellendi).
    * **RapidJSON `1.1.0`**, **fmt `12.2.0`**, **msl `1.2.1`:** `extern/common/include/` üzerinden sunucu ve istemci arasında tek kaynak olarak paylaşılır (`extern/client/include/rapidjson` gölgelemesi kaldırıldı).
    * **Crypto++ `8.9.0` (`CRYPTOPP_VERSION = 890`):** `extern/client/include/cryptopp` (`193` başlık) `extern/server/include/cryptopp` ile bayt-bayt eşitlendi (`config_asm.h` içinde MSVC x64 `_MSC_VER && _M_X64` için `CRYPTOPP_DISABLE_ASM = 1` hizalaması dahil); `server/src/libcryptopp/src` kaynaklarından native `COFF AMD64` (`0x8664`) [`extern/client/lib/cryptlib-x64.lib`](file:///D:/mt22009/extern/client/lib/cryptlib-x64.lib) (`33.529.386 B`) derlendi; eski `extern/client/cryptopp` (`8.2.0`) ve 32-bit `cryptlib-Release.lib` kaldırıldı.
    * **DevIL `1.8.0` (`IL_VERSION = 180`):** `extern/client/include/IL` (`8` başlık) `extern/server/include/IL` ile bayt-bayt eşitlendi; native `PE32+` (`0x8664`) [`extern/client/dll/DevIL_x64.dll`](file:///D:/mt22009/extern/client/dll/DevIL_x64.dll) (`1.111.859 B`) ve `COFF AMD64` import kütüphanesi [`extern/client/lib/devil_x64.lib`](file:///D:/mt22009/extern/client/lib/devil_x64.lib) (`20.894 B`) eklendi; `MarkImage.h` içindeki `typedef unsigned long Pixel` LP64/LLP64 hatası hem sunucuda hem istemcide `typedef uint32_t Pixel` olarak düzeltildi; eski `DevIL 1.7.8` 32-bit `.lib` dosyaları kaldırıldı.
    * **LZO `2.10` (`0x20a0`):** `server/src/game/src/lzo` ve `extern/client/include/lzo` (`14` başlık) bayt-bayt eşitlendi; `server/src/game/src/minilzo.c` (`2.10`) kaynağından native `COFF AMD64` (`0x8664`) [`extern/client/lib/lzo2_x64.lib`](file:///D:/mt22009/extern/client/lib/lzo2_x64.lib) (`14.406 B`) üretildi; eski 32-bit `lzo2.lib` kaldırıldı.
  - **Temizlenen Kullanılmayan SDK'lar (`7` Grup, `0` Referans Doğrulamalı):** `opencv` (`extern/client/include/opencv2`, `opencv_*.lib`), `OptionParser` (`extern/server/include/optionparser`), `inih`/`minIni` (`extern/server/include/inih`, `extern/client/include/minIni`, `minIni-Release-x86-mt.lib`), `lz4` (`extern/client/include/lz4`, `liblz4_static-Release.lib`), `snappy` (`extern/client/include/snappy`, `snappy.lib`), istemci `mysql` (`extern/client/include/mysql`, `libmysql.lib`, `mysqlclient.lib`), istemci `openssl` `0.9.8k` (`extern/client/include/openssl`) ile `extern/client/backup_dx8`, `extern/client/include/old_python_not_static`, gölgeleyen `extern/client/include/d3d9*.h`/`d3drm*.h`, 32-bit `extern/client/lib/{d3d9,ddraw,dmoguids,dxguid,mss32,liblib,python27*}.lib` ve `121` adet gevşek 32-bit `.obj` (`extern/client/lib/*.obj`) kaldırılmıştır.
  - **Birleşik SDK Çalışma Zamanı Doğrulaması ([`tests/test_unified_sdks_x64.cpp`](file:///D:/mt22009/tests/test_unified_sdks_x64.cpp) — `49/49 PASS`):** `PE32+` (`0x8664`) olarak derlenen `test_unified_sdks_x64.exe` içinde `Boost 1.92.0`, `RapidJSON 1.1.0`, `fmt 12.2.0`, `msl 1.2.1`, `Crypto++ 8.9.0` (`HashUtils.cpp` AES-128, `Hardware.cpp` SMBIOS+CPUID+AES HWID, `cipher.cpp` `DH2KeyAgreement` + `CTR_Mode`, `EterPackPolicy_CSHybridCrypt.cpp` Camellia/Twofish/XTEA), `LZO 2.10` (`CLZO` XTEA+LZO sıkıştırma/açma) ve `DevIL 1.8.0` (`CGuildMarkImage` TGA oluşturma/yükleme/blok CRC/diff/LZO) **`49/49 PASS`** vermiştir.
* **İstemci x64 Ağ Protokolü, Python 3.11 Gerçek Modül Entegrasyonu, Gerçek `miniaudio` (`v0.11.26`) Üretim `MilesLib` Adaptörü ve Kalıcı Direct3D 9Ex Geçişi (`python3_sdk_verified = PASS`, `python3_client_embedded = PASS`, `python3_real_modules = PASS`, `miniaudio_dependency_acquired = PASS`, `miniaudio_real_api_compiled = PASS`, `miniaudio_runtime_tested = PASS`, `miniaudio_production_adapter = PASS`, `real_eterpack_audio = PASS`, `physical_audio_output = UNVERIFIED`, `d3d9ex_api_test = PASS`, `d3d9ex_renderer_integration = PASS`, `client_link = BLOCKED`, `gameplay = BLOCKED`):**
  - `Packet.h` içindeki tüm `CLIENT <-> GAME` paketleri sunucu x64 protokolü (`0x20090064U`) ile derleme zamanı `static_assert` kontrolleriyle hizalanmıştır.
  - Gerçek `cryptlib-x64.lib` (`Crypto++ 8.9.0`) ve `lzo2_x64.lib` (`LZO 2.10`) ile bağlanan [`tests/run_client_x64_suite.cmd`](file:///D:/mt22009/tests/run_client_x64_suite.cmd) altındaki tüm native `PE32+` (`0x8664`) test paketleri %100 geçmiştir:
    * `[0/6]` `tests/verify_sdk_inventory.py`: **`216/216 PASS`**
    * `[1/6]` `test_miniaudio_x64_prototype.exe`: **`42/42 PASS`**
    * `[2/6]`–`[3/6]` `test_mileslib_miniaudio_production_x64.exe` (gerçek `MilesLib` + `EterPack` `sound*`/`BGM` + `cryptlib-x64.lib` + `lzo2_x64.lib`): **`36/36 PASS`** (`physical_audio_output = UNVERIFIED`)
    * `[4/6]` `test_python3_real_modules_x64.exe` (`Python 3.11.9 AMD64` + `root`/`locale` `EterPack` + `liblib` 54/54 stdlib köprüsü + `cryptlib-x64.lib` + `lzo2_x64.lib`): **`32/32 PASS`** (`test_python3_x64_bridge.exe` **`34/34 PASS`**, `test_python3_x64_compat.exe` **`51/51 PASS`**)
    * `[5/6]` `test_d3d9ex_x64.exe` (kalıcı ve tek grafik API'si Direct3D 9Ex, Windows SDK `10.0.26100.0\um\x64\d3d9.lib`, 16 `COFF AMD64` `.obj`): **`619/619 PASS`**
    * `[6/6]` `test_unified_sdks_x64.exe` (`Crypto++ 8.9.0`, `LZO 2.10`, `DevIL 1.8.0`, `Boost 1.92.0`, `RapidJSON 1.1.0`, `fmt 12.2.0`, `msl 1.2.1`): **`49/49 PASS`**
  - **Kalan Gerçek Harici İkili Engelleri (`client_link = BLOCKED`, `gameplay = BLOCKED`):** İstemcinin tüm açık kaynak ve ortak kütüphaneleri (`Crypto++ 8.9.0`, `LZO 2.10`, `DevIL 1.8.0`, `Boost 1.92.0`, `RapidJSON 1.1.0`, `fmt 12.2.0`, `msl 1.2.1`, `CPython 3.11.9`, `miniaudio 0.11.26`, Windows SDK `um\x64\d3d9.lib`) gerçek native `x64` (`0x8664`) olarak tamamlanmıştır. Tam `metin2client.exe` `link.exe /MACHINE:X64` aşaması yalnızca kalan **3 harici 32-bit (`14C`) ikili bağımlılık** (`Granny2` `granny2.lib`/`granny2.dll`, `SpeedTreeRT` `SpeedTreeRT.lib`, DirectX June 2010 `d3dx9.lib`/`d3dx9_43.dll`) nedeniyle **`BLOCKED`** durumdadır (`docs/SERVER_X64_GOALS.md` §11–§14).




