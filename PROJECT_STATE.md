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
| **Native x64 İstemci (`client/src/**`, `extern/client/**`, `PE32+` & `0x20090064U` Protokolü)** | **`UNVERIFIED`** (`python3_sdk_verified = PASS`, `python3_client_embedded = PASS`, `miniaudio_dependency_acquired = PASS`, `miniaudio_real_api_compiled = PASS`, `miniaudio_runtime_tested = PASS`, `miles_adapter_integrated = UNVERIFIED`, `client_link = BLOCKED`, `gameplay = BLOCKED`) | **İzinli ve aktif geliştirme alanı.** 16 istemci modülünün tamamını temsil eden **67 çeviri birimi (`67/67` `.obj`)** varsayılan başlıklarla ve **35 Python C++ modülünün tamamı (`35/35` `.obj` — `ScriptLib`, `EterPythonLib`, `UserInterface`)** resmî PSF CPython `3.11.9 AMD64` (`-DMT2_USE_PYTHON3`) SDK başlıklarıyla MSVC x64 (`cl.exe /c /std:c++20 /utf-8 /W3`) altında `COFF AMD64` (`8664 machine (x64)`) olarak `0` hatayla derlenmiştir. Gerçek CPython `3.11.9 AMD64` (`python311.lib`/`python311.dll`, PSF v2) çalışma zamanına bağlanan `tests/test_python3_x64_compat.cpp` (`PE32+`, `8664`) **`51/51 PASS`** (modül kaydı, CRLF UI script yükleme, PEP 552 `.pyc`, `PyFrame_GetCode`/`GetLineNumber` trace ve `5.000` çağrılık `Py_REFCNT` sıfır sızıntı denetimi dahil) ile doğrulanmıştır. Resmî `mackron/miniaudio` deposundan sabitlenen gerçek [`miniaudio.h`](file:///D:/mt22009/extern/client/include/miniaudio/miniaudio.h) (`v0.11.26`, commit `877ea4a7c2601c7b0f2a44e83dfac42e6470fea6`, SHA-256 `6b376c7fbb1352ba1e0e432ab66069b335ea9405e45cb6c30b97ef0f3c9c3e0e`, Unlicense / MIT-0) ile derlenen `tests/test_miniaudio_x64_prototype.cpp` (`PE32+`, `8664`) **`42/42 PASS`** (`ma_engine`, `ma_sound`, `ma_decoder`, özel `EterPack` `ma_vfs`, 2D/3D konumsal ses, BGM çapraz geçiş ve `ma_backend_null` cihaz geri çağrısı; fiziksel hoparlör çıkışı `UNVERIFIED`) vermiştir. Mevcut `MilesLib` korunmakta olup tam istemci `PE32+` bağlama (`link.exe /MACHINE:X64`), `57/57` adet 32-bit (`14C`) harici kütüphane (`Granny2`, `SpeedTreeRT`, `Miles32`, `liblib` vb.) nedeniyle **`BLOCKED`** durumdadır. |

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
| **Sunucu Extern** | Boost `1.90.0`, Crypto++ `8.9.0`, fmt `12.1.0`, RapidJSON `1.1.0+`, DevIL `1.8.0`, OpenSSL `3.5.6`, zlib `1.3.2` | Aynı kaynak/paket sürümleri | Tamamı 64-bit derlenip fiziksel olarak bağlanmıştır |

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

* **Sunucu (`server/**`, `sql/**`, `tests/**`):** Windows x64 ve FreeBSD 15.1 amd64 (Run `41321849182` `SUCCESS`) üzerinde tüm hedefler, ShopEx sınır sertleştirmeleri, Exchange/Trade (`14B`/`54B`), SpecialShop çoklu yuva materyal toplama (`requiredCountsByVnum`) ve `255` paket sınırı, Ikarus Offline Shop `CanBuy`/`self-buy`/`cross-shop` kontrolleri **`VERIFIED`** durumdadır (`test_packet_wire 171/171 PASS`, `ctest 10/10 PASS`, `--check-lua 11/11 PASS`, `Quest 202/202 PASS`).
* **İstemci x64 Ağ Protokolü, `COFF AMD64` Nesne Derlemesi, Python 3.11 Gerçek Modül Entegrasyonu, Gerçek `miniaudio` (`v0.11.26`) Üretim `MilesLib` Adaptörü ve Opsiyonel Direct3D 9Ex (`EterLib`) Geçişi (`python3_sdk_verified = PASS`, `python3_client_embedded = PASS`, `python3_real_modules = PASS`, `miniaudio_dependency_acquired = PASS`, `miniaudio_real_api_compiled = PASS`, `miniaudio_runtime_tested = PASS`, `miniaudio_production_adapter = PASS`, `real_eterpack_audio = PASS`, `physical_audio_output = UNVERIFIED`, `d3d9ex_api_test = PASS`, `d3d9ex_renderer_integration = PASS`, `client_link = BLOCKED`, `gameplay = BLOCKED`):**
  - `Packet.h` içindeki tüm `CLIENT <-> GAME` paketleri sunucu x64 protokolü (`0x20090064U`) ile derleme zamanı `static_assert` kontrolleriyle hizalanmıştır.
  - **CPython 3.11.9 AMD64 SDK Envanteri ve Gerçek İstemci Modülleri (`python3_sdk_verified = PASS`, `python3_client_embedded = PASS`, `python3_real_modules = PASS`):** `C:\Users\oyun\AppData\Local\Programs\Python\Python311` altındaki resmî PSF Windows AMD64 kurulumu (`3.11.9`, tag `v3.11.9:de54cf5`, `MSC v.1938 64 bit (AMD64)`, PSF v2 lisansı `LICENSE.txt` SHA-256 `e502c6b8...`, `3.544` `.py` standart kütüphane modülü; CPython kaynak ağacı indirilmemiştir) doğrulanmıştır (`python.exe` `PE32+` `0x8664` SHA-256 `5f7b89a6...`, `python311.dll` `PE32+` `0x8664` SHA-256 `0817a2a6...`, `libs/python311.lib` `COFF 0x8664` SHA-256 `e46f77c9...`, `include/Python.h` SHA-256 `95c2b86d...`).
  - 16 istemci modülünün tamamını temsil eden **67 çeviri birimi (`67/67` `.obj`)** varsayılan başlıklarla ve **35 Python C++ modülünün tamamı (`35/35` `.obj` — `ScriptLib`, `EterPythonLib`, `UserInterface`)** CPython `3.11.9 AMD64` (`-DMT2_USE_PYTHON3`) başlıklarıyla MSVC x64 (`cl.exe /c /std:c++20 /utf-8 /W3`) altında gerçek `COFF AMD64` (`8664 machine (x64)`) nesne dosyalarına derlenmiştir.
  - Gerçek `ScriptLib` (`PythonLauncher.cpp`, `PythonUtils.cpp`, `PythonMarshal.cpp`, `PythonDebugModule.cpp`) + `UserInterface` (`PythonliblibManager.cpp` — 32-bit `liblib.lib` olmadan `54/54` standart kütüphane ve Python 2->3 `copy_reg`/`UserDict`/`_abcoll`/`ordered_dict`/`simplejson` köprüsü; `PythonPackModule.cpp` — PEP 383 `surrogateescape` destekli `pack.Get`) + gerçek `CEterPackManager` (`client/files/pack/root` ve `client/files/pack/locale`) modülleri `python311.lib` ile `PE32+` (`0x8664`) olarak bağlanmış ve `tests/test_python3_real_modules_x64.cpp` **`32/32 PASS`**, `tests/test_python3_x64_bridge.cpp` **`34/34 PASS`** ve `tests/test_python3_x64_compat.cpp` **`51/51 PASS`** ile doğrulanmıştır.
  - **Gerçek `miniaudio` (`v0.11.26`) Üretim `MilesLib` Adaptörü ve Gerçek `EterPack` Ses Doğrulaması (`42/42 PASS` Prototip + `36/36 PASS` Üretim `MilesLib` + `EterPack`):** Resmî `https://github.com/mackron/miniaudio` deposundan `v0.11.26` (`2026-10-31`, commit `877ea4a7c2601c7b0f2a44e83dfac42e6470fea6`) sürümü [`extern/client/include/miniaudio/miniaudio.h`](file:///D:/mt22009/extern/client/include/miniaudio/miniaudio.h) (`4.161.918 B`, SHA-256 `6b376c7fbb1352ba1e0e432ab66069b335ea9405e45cb6c30b97ef0f3c9c3e0e`) olarak sabitlenmiştir. `client/src/MilesLib/MiniaudioEngine.{h,cpp}` ve `MT2_USE_MINIAUDIO` derleme seçeneğiyle tüm `MilesLib` sınıfları (`CSoundBase`, `CSoundData`, `CSoundInstance2D/3D/Stream`, `CSoundManager`, `CSoundManager2D/3D/Stream`) dış arayüzü değiştirmeden ve `MSS32.dll` geri dönüş yolunu koruyarak gerçek `miniaudio` motoruna bağlanmıştır. Gerçek `client/files/pack/sound`, `sound_npc`, `sound_pc` MCOZ/TEA+LZO arşivlerini (`sound_etc=111`, `sound_npc=2500`, `sound_pc=111`) ve `client/files/BGM/*.mp3` dosyalarını açan `tests/test_mileslib_miniaudio_production_x64.cpp` (`PE32+`, `0x8664`) **`36/36 PASS`** (`packDataReads=8, vfsPackOpens=1, vfsDiskOpens=2, totalPackBytes=330.520`, WASAPI donanım yoklaması `devInit=1, devStart=1, mixedFrames=3360`; insan kulağıyla tam oyun içi ses dinlemesi henüz yapılmadığından `physical_audio_output = UNVERIFIED`) vermiştir.
  - **Direct3D 9 -> Direct3D 9Ex Opsiyonel Geçişi (`d3d9ex_api_test = PASS`, `d3d9ex_renderer_integration = PASS`):** `client/src/EterLib/GrpBase.{h,cpp}`, `GrpDevice.cpp`, `GrpScreen.cpp`, `GrpIndexBuffer.cpp`, `GrpVertexBufferStatic.cpp`, `BlockTexture.{h,cpp}` ve `GrpImageTexture.{h,cpp}` içinde mevcut D3D9 yolunu varsayılan olarak koruyan (`ms_bUseD3D9Ex = false`), etkinleştirildiğinde (`CGraphicBase::SetUseD3D9Ex(true)`) `Direct3DCreate9Ex` + `CreateDeviceEx` + `PresentEx` + `ResetEx` + `CheckDeviceState` kullanan ve `D3DPOOL_MANAGED` kaynakları `D3DPOOL_DEFAULT` + `D3DPOOL_SYSTEMMEM` staging (`AddDirtyRect` + `UpdateTexture`) / `D3DUSAGE_DYNAMIC` ile yöneten opsiyonel D3D9Ex mimarisi tamamlanmış; 9 `EterLib` çeviri birimi `COFF AMD64` (`0x8664`) olarak derlenmiş ve `tests/test_d3d9ex_x64.cpp` (`PE32+`, `0x8664`, Windows SDK `10.0.26100.0\um\x64\d3d9.lib` ile bağlı) gerçek `Intel(R) UHD Graphics` (`igdumdim64.dll`) üzerinde **`599/599 PASS`** (`ResetEx` gecikmesi `1,301 ms` vs D3D9 `Reset` `2,747 ms`) vermiştir.
  - **Gerçek Harici İkili Engeli (`client_link = BLOCKED`, `gameplay = BLOCKED`):** `liblib.lib` (`PythonliblibManager.cpp` x64 stdlib köprüsüyle), `mss32.lib` (`MilesLib` `MT2_USE_MINIAUDIO` adaptörüyle), `python27.lib` (`python311.lib` ile) ve Windows SDK `10.0.26100.0\um\x64` (`d3d9.lib`, `dinput8.lib`, `dxguid.lib`, `dmoguids.lib`, `ddraw.lib`, `winmm.lib`, `imm32.lib`, `ws2_32.lib`, `ole32.lib`) + `minilzo` x64 karşılıkları doğrulanmıştır; ancak kapalı kaynak `granny2.lib`/`granny2.dll`, `SpeedTreeRT.lib` ve harici 32-bit görüntü/yardımcı `.lib` dosyalarının gerçek 64-bit (`COFF AMD64` / `PE32+`) karşılıkları sağlanmadan nihai `metin2client.exe` `link.exe /MACHINE:X64` aşaması tamamlanamaz (`docs/SERVER_X64_GOALS.md` §11–§13).




