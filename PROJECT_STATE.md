# METİN2 MT2009 — PROJE DURUM HAFIZASI (`PROJECT_STATE.md`)

**Son Güncelleme:** 3 Kasım 2026 | **Aktif Dal:** `dev/windows-gcc-baseline` | **Uzak Depo:** `aidevelopervega/Metin2-MT2009` (`PUBLIC`)

> **Belge Disiplini:** Depoda yalnızca 3 Markdown belgesi tutulur:
> 1. [`AGENTS.md`](file:///D:/mt22009/AGENTS.md) — Kalıcı geliştirme, güvenlik ve çalışma kuralları.
> 2. [`PROJECT_STATE.md`](file:///D:/mt22009/PROJECT_STATE.md) — Güncel, doğrulanmış proje durumu ve önemli teknik kararlar.
> 3. [`docs/SERVER_X64_GOALS.md`](file:///D:/mt22009/docs/SERVER_X64_GOALS.md) — Sunucu + istemci native x64 teknik sözleşmesi, gerçek engeller ve kabul kriterleri.

---

## 1. Güncel Doğrulanmış Durum Özeti

| Alan / Bileşen | Durum | Son Doğrulanmış Kanıt ve Commit SHA |
| :--- | :---: | :--- |
| **Sunucu Kaynak Kodu (`game`, `db`, `qc`, `libthecore`, `libsql`, `libgame`, `libpoly`, `liblua`) & Standalone Asio `1.38.2` + OpenSSL `3.6.5` TLS 1.3 Üretim Entegrasyonu** | **`VERIFIED`** | Windows amd64 (`PE32+`, MinGW-w64 GCC 16.2.0 UCRT) ve FreeBSD 15.1 amd64 (`ELF64`, GCC 14.2.0) üzerinde tek kaynak koddan `-std=c++20 -Wall -Wextra -pipe` (`0` `-Wno-*` bastırma bayrağı) ile **`0` hata ve `0` uyarı** (`FAZ 0`–`FAZ 11`: **C++20 + Standalone Asio `1.38.2` (`ASIO_STANDALONE=1`, `0` Boost bağımlılığı) + OpenSSL `3.6.5` TLS 1.3 (`TLS1_3_VERSION`, AEAD `TLS_AES_256_GCM_SHA384`/`TLS_CHACHA20_POLY1305_SHA256`, `0-RTT=off`, `SSL_OP_NO_ANTI_REPLAY` temizlenerek anti-replay açık, SAN/hostname doğrulaması, P2P `mTLS`)** ortak taşıma katmanı [`extern/common/include/net/mt2_asio_tls13_transport.h`](file:///D:/mt22009/extern/common/include/net/mt2_asio_tls13_transport.h), `DESC` (`server/src/game/src/desc.{h,cpp}`), `DESC_MANAGER` (`StartTls13Listeners`, `ProcessTls13Io`, `AcceptTls13Desc`, `AcceptTls13P2PDesc`) ve `main.cpp` (`io_loop` + `config.cpp` `TLS13_*` anahtarları) üretim bağlantısı; **`Boost 1.92.0` (`16.336` dosya) tamamen kaldırılarak C++20 standart kütüphanesine geçildi**; `DevIL 1.8.0` kaldırılarak `stb_image v2.30` + `stb_image_write v1.16` + `mt2_image_stb.h` C++20 RAII geçişi, Birleşik SDK standardı, ShopEx, Exchange, SpecialShop ve Ikarus Offline Shop güvenlik/taşma sertleştirmeleri dahil). |
| **FreeBSD 15.1 amd64 GCC 14 + MariaDB 12.3.3 LTS + OpenSSL 3.5.6 Tam Sunucu Doğrulaması (`M-10`, `FAZ 0`–`FAZ 11` Standalone Asio `1.38.2` + OpenSSL TLS 1.3 Üretim Entegrasyonu, `Python 2.7` / `Miles` / `Boost` / `DevIL` Kaldırılması, `stb` Üretim Geçişi & Birleşik SDK Standardı)** | **`VERIFIED`** | **GitHub Actions Run ID:** `41482469059` \| **Head SHA:** `a3cb28a286f92053c5cbbc68f2947c703f8808fe` (kaynak SHA: `ac3fac6d2ee76305a9dd52cc1dea2637b68f0dd9`) \| **Sonuç:** **`SUCCESS`** (`7m 39s`, Job ID `125404085325`; önceki koşular: `41385255463`, `41378195177`, `41355864654`, `41321849182`, `41311852071`, `41183346026`). `CTest 12/12` (`libgame 93/93` [15 senaryoluk `CGuildMarkImage` + `stb` regresyon paketi dahil], `test_packet_wire 25/25 [171/171]`, **`test_asio_tls13_security 191/191 PASS`**, **`test_network_ab_benchmark 25/25 PASS`**), `readelf -d game_r41023` üzerinde `0` `libIL.so` ve `0` `boost` bağımlılığı, `--check-lua 11/11` (`Check 11b` ShopEx 3 sekme / `12111 B`, `Check 11c` `CExchange::AddGold` `54 B` + `ikashop::CShopItem::CanBuy` ve **`Check 11d` `DESC_MANAGER::StartTls13Listeners` + `ProcessTls13Io` + `AcceptTls13Desc` + `AcceptTls13P2PDesc` + çift TEA şifrelemesi olmaması** dahil), `Quest 202/202`, `test_live_db 354/354`, `--test-itemshop 15/15`, `test_db_cache_integration 162/162 & 187/187`, `test_server_smoke_client 29/29` (`>2^32` AID/PID/envanter/quest/restart/replay + 8 servisli serverfiles + cross-core `ch11 <-> ch12` `WarpSet`), gerçek `pfctl -nf` (`exit 0`), audit/TLS, backup/restore ve `[4.A]` MariaDB restart kalıcılığı `PASS`. |
| **Windows x64 + MariaDB 12.3.3 LTS Birim, TLS 1.3 Güvenlik, A/B Performans ve Canlı Entegrasyon Testleri (`M-01`..`M-09`, `INT-01`, `F14`..`F16`, `FAZ 0`–`FAZ 11`)** | **`VERIFIED`** | `CTest 12/12` (`libgame 93/93` [15 senaryoluk `CGuildMarkImage` + `stb` regresyon paketi dahil], `libsql 203/203`, `test_packet_wire 171/171`, `test_db_cache 139/139`, `test_db_cache_integration 162/162` çevrimdışı / `187/187` canlı MariaDB, `test_lua55_quest 11/11`, `test_auth_security 423/423`, **`test_asio_tls13_security 191/191`** [19 güvenlik/çerçeveleme/0-RTT negatif testi/`SSL_OP_NO_ANTI_REPLAY == 0`/Auth-Game-Economy-WarpSet/P2P mTLS/mobil testi], **`test_network_ab_benchmark 25/25`** [`100`/`500`/`1000` eşzamanlı bağlantı `fdwatch`+TEA vs Standalone Asio+TLS 1.3 A/B karşılaştırması]), `--check-lua 11/11` (`Check 11b`, `11c` & `11d` dahil), `--test-itemshop 15/15`, `--protocol-framing-audit 10/10 (4/4)`, `test_live_db 354/354`, `run_quest_matrix.py 202/202`, `run_kademe_c_live.ps1 6/6 PASS`, `scan_compiler_warnings.py` (`0` yüksek riskli uyarı). |
| **Native x64 İstemci (`client/src/**`, `extern/client/**`, `PE32+` & `0x20090064U` Protokolü), 21 Nihai Denetim Kapısı (`FAZ 0`–`FAZ 11`)** | **`UNVERIFIED`** (`python27_removed = REMOVED`, `python311_production_only = VERIFIED`, `cryptopp_usage_inventory = VERIFIED`, `cryptopp_removal_readiness = BLOCKED`, `d3d9ex_sole_api = VERIFIED`, `d3d9ex_game_runtime = UNVERIFIED`, `miniaudio_production_only = VERIFIED`, `miles_legacy_removed = REMOVED`, `stb_production = VERIFIED`, `devil_removed = REMOVED`, `boost_removed = REMOVED`, `asio_production_wiring = INTEGRATED`, `tls13_production_wiring = INTEGRATED`, `tls13_security = VERIFIED`, `sdk_inventory_complete = VERIFIED`, `sdk_binary_architecture = VERIFIED`, `release_x64_project_build = VERIFIED`, `client_objects = VERIFIED`, `client_link = BLOCKED`, `gameplay = BLOCKED`, `freebsd_ci = VERIFIED`) | **İzinli ve aktif geliştirme alanı.** `FAZ 0`–`FAZ 11` kapsamlı son denetiminde: (1) **`Python 2.7` (`extern/client/include/Python27` `92` dosya, `liblib.lib`, `python27.lib`, `#if MT2_USE_PYTHON3` karşıt Python 2 fallback dalları) tamamen kaldırılarak CPython `3.11.9` x64 tek gömülü yorumlayıcı yapıldı**; (2) **`Miles Sound System` (`extern/client/include/MSS.H`, `mss32.lib`, `AIL_*` fallback dalları) tamamen kaldırılarak `miniaudio 0.11.26` (`SHA-256: 5b325f7d...`) tek ses motoru yapıldı**; (3) **`Crypto++ 8.9.0` kullanım envanteri (`EterPack` `PanamaCipher<LittleEndian>`, `Tiger`, `Whirlpool`, `RIPEMD128`, `Camellia`, `Twofish`, `XTEA` + `Hardware.cpp`/`HashUtils.cpp`/`utils.cpp` AES-ECB & SHA1) gerçek `EterPack` Type-3 altın vektörleriyle (`test_unified_sdks_x64.exe` Gate 3e) kanıtlandı (`cryptopp_removal_readiness = BLOCKED` / `ACTIVE` zorunlu bağımlılık)**; (4) **`CNetworkStream::SetDefaultTls13Config` + `PythonNetworkStream.cpp` (`MT2_TLS13_CA_CERT` / `tls13_ca.pem`) ve `DESC_MANAGER::StartTls13Listeners` + `ProcessTls13Io` üretim giriş noktaları bağlanarak çift TEA şifrelemesi (`SetSecurityMode` no-op) engellendi**; (5) [`tests/verify_sdk_inventory.py`](file:///D:/mt22009/tests/verify_sdk_inventory.py) (**`260/260 PASS`**), [`tests/test_unified_sdks_x64.cpp`](file:///D:/mt22009/tests/test_unified_sdks_x64.cpp) (**`137/137 PASS`**), MSVC x64 `test_asio_tls13_security_msvc_x64.exe` (**`191/191 PASS`**), **15 statik kütüphane `.vcxproj` projesinin gerçek `MSBuild Release|x64` derlemesi (`15/15 .lib PASS`)** ve **16 modüldeki `344/344` çeviri biriminin tamamının (`COFF AMD64 0x8664`, `0` derleme hatası, `0` `/NODEFAULTLIB:python27*`/`liblib`/`mss32` bastırması) katı `link.exe /MACHINE:X64` ön-kontrolü (`LNK2005 = 0`, `other_non_commercial = 0`, `ddraw.lib` + `dmoguids.lib` ile `MovieMan.obj` dahil)** doğrulandı; tam istemci bağlama yalnızca kalan **2 tescilli 32-bit (`14C`) kütüphane** (`Granny2`: `57` sembol, `SpeedTreeRT`: `50` sembol) nedeniyle **`BLOCKED`** durumdadır. |

---

## 2. Depo Yapısı, Araç Zinciri ve Yerel Ortam Envanteri

### 2.1 Dizin Yapısı
* `server/src/{common,db,game,libsql,libthecore,libgame,libpoly,liblua,qc}` — C++20 x64 sunucu ve çekirdek kütüphane kaynakları (`0` Boost / `TR1_NS` bağımlılığı; `game/src/desc.{h,cpp}` içinde Standalone Asio + OpenSSL TLS 1.3 oturum entegrasyonu).
* `server/scripts/{windows,freebsd}/` — Serverfiles kurulum (`install.*`), başlatma (`start.*`), durdurma (`stop.*`), yedekleme (`backup_mariadb.*`), log rotasyonu (`rotate_logs.*`), `audit_guard.sh` ve `pf_guard.sh` betikleri.
* `server/config/*.example` — Sırsız yapılandırma şablonları (`serverfiles.env.example`, `mariadb-security.cnf.example`, `pf.conf.example`, `server_tls.cnf.example`).
* `server/serverfiles_share.tar.xz` — Oyun verilerinin (`share/data`, `share/locale/poland`, `share/package` — `14.341` dosya, `202` quest) tek resmî ve parolasız arşivi.
* `client/src/` — `Metin2Client.sln` ve 16 istemci modülü (`UserInterface`, `EterLib` [`NetStream.{h,cpp}` TLS 1.3 taşıma entegrasyonu dahil], `GameLib`, `EterBase`, `EffectLib`, `EterGrnLib`, `MilesLib`, `SpeedTreeLib`, `EterPythonLib`, `EterPack`, `ScriptLib`, `EterLocale`, `SphereLib`, `EterImageLib`, `PRTerrainLib`, `CWebBrowser`).
* `extern/{common,server,client}/` — Üçüncü taraf başlıklar (`extern/common/include/{asio,net,rapidjson,fmt,msl,stb}`, `extern/client/include/openssl`), kaynaklar (`extern/client/libjpeg`, `extern/client/discord-rpc`, `extern/server/cryptopp`) ve kütüphaneler (`Boost 1.92.0` ve `DevIL 1.8.0` kalıcı olarak kaldırıldı).
* `sql/*.sql` & `sql/migrations/*.sql` — MariaDB 12.3 LTS temel şemaları (`account`, `common`, `log`, `player`, `world` — toplam `126` InnoDB tablo) ve migration betikleri.
* `tests/` — Birim, entegrasyon, canlı MariaDB, ağ protokolü duman testi, Standalone Asio + OpenSSL TLS 1.3 güvenlik/protokol testi (`test_asio_tls13_security.cpp`), `fdwatch`+TEA vs Asio+TLS 1.3 A/B performans kıyaslaması (`test_network_ab_benchmark.cpp`), 15 senaryoluk `stb` lonca simgesi regresyon paketi (`guild_mark_stb_test_suite.h`), birleşik SDK üretici (`build_client_x64_sdks.py`) ve tam istemci `link.exe /MACHINE:X64` ön-kontrol (`run_client_x64_link_preflight.py`) betikleri.
* `CMakeLists.txt`, `CMakePresets.json`, `cmake/` — Birleşik CMake + Ninja derleme altyapısı (`windows-gcc-ucrt`, `freebsd-gcc14`).
* `.github/workflows/freebsd-gcc-test.yml` & `ci/run-full-freebsd.txt` — Yalnızca `ci/run-full-freebsd.txt` değiştiğinde tetiklenen FreeBSD 15.1 amd64 GCC 14 CI iş akışı.
* `.githooks/` — `main` dalına doğrudan commit/push engelleyen Git kancaları (`core.hooksPath = D:/mt22009/.git/hooks`).

### 2.2 Doğrulanmış Araç Zinciri ve Sunucu Harici Bağımlılıkları (`Extern`)

| Bileşen | Windows Yerel Sürümü | FreeBSD 15.1 CI Sürümü | Not |
| :--- | :--- | :--- | :--- |
| **Sunucu Derleyicisi** | MinGW-w64 GCC `16.2.0` (`x86_64-ucrt-posix-seh`) | FreeBSD GCC `14.2.0` (`amd64`) | `-std=c++20 -Wall -Wextra -pipe` (`0` uyarı) |
| **İstemci Derleyicisi** | MSVC / Visual Studio 2022 (`v143`, `/std:c++20`) | — | Native x64 (`PE32+`), `343/343` TU `COFF AMD64` (`0` hata) |
| **Derleme Sistemi** | CMake `4.4.2` + Ninja `1.13.2` | CMake + Ninja (`freebsd-gcc14`) | Windows derlemesi: `tests/run_limited.ps1 -Build` |
| **Veritabanı Motoru** | MariaDB `12.3.3-MariaDB` (`Win64`) | MariaDB `12.3.3-MariaDB` (`FreeBSD amd64`) | Tek resmi DB standardı; Connector/C `3.4.10` (`libmariadb`) |
| **Lua Motoru** | Lua `5.5.1` (`server/src/liblua`) | Lua `5.5.1` (`server/src/liblua`) | Tek üretim/derleme motoru (eski Lua 5.0/5.2 kaldırıldı) |
| **Sunucu & İstemci Ortak Extern Standardı** | Standalone Asio `1.38.2` (`ASIO_STANDALONE=1`), OpenSSL `3.6.5` (FreeBSD sistem OpenSSL `3.x`), Crypto++ `8.9.0`, fmt `12.2.0`, RapidJSON `1.1.0`, msl `1.2.1`, stb `v2.30/v1.16`, LZO `2.10`, D3DX9 `9.29.952.8`, libjpeg `9.3`, discord-rpc `3.4.0`, zlib `1.3.2` | Aynı kaynak/paket sürümleri | Tamamı 64-bit (`0x8664`) derlenip bayt-bayt başlık ve PE import denetimi (`tests/verify_sdk_inventory.py` `260/260 PASS`) ile kilitlenmiştir (`Boost 1.92.0`, `DevIL 1.8.0`, `Python 2.7` ve `Miles MSS.H` kalıcı olarak kaldırıldı) |

### 2.3 Yerel `.gitignore` İstisnaları ve İzole Test Ortamı (Git Dışında Korunanlar)

Aşağıdaki dosya ve dizinler güvenlik veya boyut sınırı nedeniyle `.gitignore` ile Git takibi dışında tutulur ve yerel diskte korunur:
1. **`D:\mt22009\scratch\handoff\isolated_db\mariadb-12.3.3-winx64\`:** Kademe C canlı testleri için kullanılan taşınabilir MariaDB `12.3.3` Win64 ikili dağıtımı (sistem servisi değildir; testlerde yalnızca `127.0.0.1:3307` üzerinde geçici veri diziniyle başlatılır).
2. **`D:\mt22009\server\files\` ve `D:\mt22009\server\files\serverfiles.env`:** Yerel Windows serverfiles çalışma dizini ve `chmod/icacls` korumalı yerel parola yapılandırması (asla Git'e eklenmez; şablonu `server/config/serverfiles.env.example` içindedir).
3. **`D:\mt22009\client\files\` ve `locales-mt2009.rar`:** İstemci çalışma zamanı varlıkları (`pack/`, `BGM/`, `miles/`, `lib/` ve mevcut 32-bit DLL/ikililer).
4. **`build/`, `object/`, `scratch/`:** Yeniden üretilebilir CMake/Ninja derleme çıktıları, `qc` çıktıları ve geçici test dosyaları.

### 2.4 Windows Derleme ve Test Komutları
* **Zorunlu Windows Sunucu Derleme Komutu (`%50` CPU Hard-Cap, `0xFC0` 6 LP, `-j6`, `BelowNormal`):**
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_limited.ps1 -Build
  ```
* **Derleme Dışı Testler (CPU kısıtlaması uygulanmaz, tam hızda çalışır):**
  - Birim, TLS 1.3 güvenlik ve A/B performans testleri: `ctest --preset windows-gcc-ucrt --output-on-failure` (`12/12 PASS`: `test_libgame 93/93`, `test_asio_tls13_security 191/191`, `test_network_ab_benchmark 25/25` dahil)
  - Lua 5.5.1 çalışma zamanı öz-denetimi: `.\server\src\game\game_r41023.exe --check-lua` (`11/11 PASS`, `Check 11d` `DESC_MANAGER` TLS 1.3 üretim bağlantısı dahil)
  - Quest derleme matrisi: `python -X utf8 tests/run_quest_matrix.py` (`202/202 PASS`)
  - Kademe C tam canlı doğrulama (izole MariaDB 12.3.3 port `3307`): `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_kademe_c_live.ps1 -IncludeSecurityAndBackup` (`6/6 PASS`)
  - Genişletilmiş derleyici uyarı taraması: `python tests/scan_compiler_warnings.py` (`285/285 TU`, `0` yüksek riskli uyarı)
  - SDK envanter kilidi, PE import tablosu ve başlık/mimari eşitlik doğrulaması: `python tests/verify_sdk_inventory.py` (`260/260 PASS`) ve `python tests/build_client_x64_sdks.py --verify-only`
  - Tam istemci x64 test paketi + MSVC x64 TLS 1.3 güvenlik paketi + 15 statik kütüphane `MSBuild Release|x64` derlemesi + `344/344` TU katı `link.exe /MACHINE:X64` ön-kontrolü: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_limited.ps1 -Build -Command "tests\run_client_x64_suite.cmd" -CpuCapPercent 50`

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
6. **C++20 + Standalone Asio `1.38.2` + OpenSSL `3.6.5` TLS 1.3 Ağ Mimarisi ve Üretim Bağlantısı (`FAZ 0`–`FAZ 11`):**
   * [`extern/common/include/net/mt2_asio_tls13_transport.h`](file:///D:/mt22009/extern/common/include/net/mt2_asio_tls13_transport.h) içinde `ASIO_STANDALONE=1` (`0` Boost bağımlılığı) ve `asio::ssl::stream<asio::ip::tcp::socket>` kullanılarak platform bağımsız C++20 taşıma katmanı uygulanmıştır.
   * **Zorunlu TLS 1.3 Güvenlik Kuralları:** Yalnızca `TLS1_3_VERSION` (`SSL_CTX_set_min_proto_version` / `SSL_CTX_set_max_proto_version`) kabul edilir; TLS 1.2 veya şifresiz TCP'ye otomatik düşüş (fallback) yoktur; `verify_peer | verify_fail_if_no_peer_cert` ve `SSL_set1_host` (`X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS`) ile sertifika zinciri ve SAN/hostname doğrulaması zorunludur; TLS 0-RTT early data kapalıdır (`SSL_CTX_clear_options(ctx, SSL_OP_NO_ANTI_REPLAY)` ile OpenSSL anti-replay koruması açık tutulur + `SSL_CTX_set_max_early_data(ctx, 0)` + `SSL_set_max_early_data(ssl, 0)`); yalnızca AEAD şifreleme paketleri (`TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256`) kullanılır; üretim özel anahtarları Git deposuna yazılmaz (testler çalışma zamanında bellekte geçici X.509 v3 PKI üretir).
   * **Üretim Giriş Noktaları, Tek İş Parçacığı Sahipliği ve Çift TEA Koruması:** Sunucuda `CONFIG` içindeki `TLS13_PORT` / `TLS13_P2P_PORT` / `TLS13_CERT_FILE` / `TLS13_KEY_FILE` / `TLS13_CA_FILE` (`config.cpp`) parametreleriyle `DESC_MANAGER::StartTls13Listeners` (`main.cpp`) başlatılır ve `io_loop` her turda `DESC_MANAGER::instance().ProcessTls13Io(fdw)` çağırarak el sıkışması tamamlanmış TLS 1.3 oturumlarını `AcceptTls13Desc` / `AcceptTls13P2PDesc` üzerinden ana `heart` iş parçacığında işler. İstemcide `CPythonNetworkStream` kurucusu (`PythonNetworkStream.cpp`) `MT2_TLS13_CA_CERT` veya `tls13_ca.pem` mevcutsa `CNetworkStream::SetDefaultTls13Config` çağırır ve `CNetworkStream::Connect` otomatik olarak TLS 1.3 oturumunu başlatır. Bir bağlantıda TLS 1.3 aktif olduğunda hem `DESC::SetSecurityKey` hem `CNetworkStream::SetSecurityMode` eski TEA paket şifrelemesini devreye sokmaz (`0` çift TEA şifrelemesi).

---

## 5. FreeBSD 15.1 amd64 GCC 14 CI Doğrulama Kaydı (`M-10`, `FAZ 0`–`FAZ 11` Standalone Asio + TLS 1.3, `Python 2.7` / `Miles` / `Boost` / `DevIL` Kaldırılması, `stb` Üretim Geçişleri & Birleşik SDK Standardı `VERIFIED`)

* **Son Doğrulanmış CI Koşusu:** GitHub Actions Run ID **`41482469059`** (Önceki koşular: **`41385255463`**, **`41378195177`**, **`41355864654`**, **`41321849182`**, **`41311852071`**, **`41183346026`**)
* **Dal / Commit SHA:** `dev/windows-gcc-baseline` / `a3cb28a286f92053c5cbbc68f2947c703f8808fe` (kaynak SHA `ac3fac6d2ee76305a9dd52cc1dea2637b68f0dd9`)
* **Genel Sonuç ve Süre:** **`SUCCESS`** (`7m 39s`, Job ID `125404085325`, `FreeBSD 15.1 - MariaDB 12.3 LTS`)
* **Doğrulanan Aşamalar:**
  1. **Derleme ve ELF64 Bağlama:** `db_r41023`, `game_r41023`, `qc` ve tüm test ikilileri FreeBSD 15.1 GCC 14.2.0 altında `0` hata ve `0` uyarı ile derlendi; `readelf -d` üzerinde `libIL.so` (`DevIL`) ve `boost` bağımlılığının tamamen kalktığı doğrulandı.
  2. **Birim, TLS 1.3, Lua ve Quest Testleri:** `CTest 12/12 PASS` (`libpoly 24/24`, `libthecore 105/105`, `libgame 93/93` [15 senaryoluk `CGuildMarkImage` + `stb` regresyon paketi dahil], `libsql 203/203`, `test_db_cache 139/139`, `test_db_cache_integration 162/162`, `test_packet_wire 25/25 [171/171]`, `test_lua55_quest 11/11`, `test_qc_compiler_lua55 1/1`, `test_auth_security 423/423`, `test_asio_tls13_security`, `test_network_ab_benchmark 25/25`), `game_r41023 --check-lua 11/11 PASS`, `run_quest_matrix.py 202/202 PASS`.
  3. **Canlı MariaDB 12.3.3 LTS Testleri:** `run_live_tests.sh` (`128/128 PASS`), `game_r41023 --test-itemshop` (`15/15 PASS`), `test_live_db` (`354/354 PASS`), `test_db_cache_integration` (`[Bonus 4.0]` canlı `187/187 PASS`).
  4. **64-Bit (`> 2^32`) Protokol ve Kalıcılık Duman Testi (`run_server_smoke_test.sh [1/4]..[4/4]` & `test_server_smoke_client 29/29 PASS`):** `test_server_smoke_client --protocol-framing-audit` (`10/10 PASS`), `aid = 5000000001`, `pid = 5000000001`, `item_id = 5000000001..5000000002` ile canlı Auth/Game girişi, envanter (`window=1, pos=3..4`), quest (`relogin_smoke.status=42`) yazımı, yeniden giriş doğrulaması, servis yeniden başlatma sonrası kalıcılık ve `dwLoginKey` replay reddi (`ALREADY`) **`PASS` (`0 syserr`)**.
  5. **8 Servisli Serverfiles ve Güvenlik Kontrolleri (`run_freebsd_serverfiles_test.sh [1/7]..[7/7]` + `[Bonus 4.A..4.F]` + `[4.A]`):** 8 servis (`db`, `auth`, `ch11`, `ch12`, `ch21`, `ch31`, `ch41`, `ch99`) `LISTENING` ve `0 syserr`, cross-core `ch11 -> ch12 -> portal 10002 WarpSet -> ch11` (`[cross-core-step1..5] PASS`), `backup_mariadb.sh` `HMAC-SHA256(v2)` yedekleme/geri yükleme `PASS`, `audit_guard.sh` v2 maskeleme `PASS`, `126/126` InnoDB tablo doğrulaması `PASS`, `pf_guard.sh` üzerinde **gerçek `pfctl -nf` sözdizimi doğrulaması (`exit code 0`) `PASS`** ve `[4.A]` MariaDB restart kalıcılığı (`127.0.0.1|OFF|ON|CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT`) **`PASS`**.

---

## 6. `FAZ 0` – `FAZ 11` Birleşik Nihai Durum Tablosu (21 Kabul Kapısı)

| Kapı (Gate) | Statü | Doğrulama Kanıtı ve Dosya Referansları |
| :--- | :---: | :--- |
| **`python27_removed`** | **`REMOVED`** | `extern/client/include/Python27/` (`92` dosya), `#include <Python27/Python.h>`, `liblib.lib`, `python27.lib`, `PY_VERSION_HEX == 0x020706f0` ve tüm `#if MT2_USE_PYTHON3 ... #else` Python 2.7 dalları (`ScriptLib`, `EterPythonLib`, `UserInterface`) kalıcı olarak silindi (`verify_sdk_inventory.py 260/260 PASS`). |
| **`python311_production_only`** | **`VERIFIED`** | CPython `3.11.9` x64 (`MSC v.1938 64 bit (AMD64)`) tek gömülü yorumlayıcıdır; `28` `PyImport_AppendInittab` yerleşik C modülü ve `54` dondurulmuş stdlib modülü `test_python3_real_modules_x64.exe` (`32/32` [`64/64`] `PASS`) ile gerçek `root` + `locale_pl` EterPack arşivleri üzerinde `/NODEFAULTLIB:python27*` veya `/NODEFAULTLIB:liblib.lib` olmaksızın doğrulandı. |
| **`cryptopp_usage_inventory`** | **`VERIFIED`** | `EterPack.cpp` (`PanamaCipher<LittleEndian>::Encryption/Decryption`, `Tiger`, `Whirlpool`, `RIPEMD128`, `SHA1` — Type 3 `MCOZ` ve Type 4/5 `CSHybridCrypt` gerçek üretim `.epk`/`.eix` arşivleri `root`, `ETC`, `sound_etc`, `locale_pl`), `EterPackPolicy_CSHybridCrypt.cpp` (`Camellia`, `Twofish`, `XTEA` CFB_Mode), `Hardware.cpp` (`AES` ECB_Mode + `HexEncoder`), `HashUtils.cpp` (`SHA1`) ve `server/src/game/src/utils.cpp` (`SHA1`) tam dosya:satır envanteriyle sınıflandırıldı ve `test_unified_sdks_x64.exe` Gate 3e (`137/137 PASS`) içinde gerçek Type-3 Panama altın vektörleriyle doğrulandı. |
| **`cryptopp_removal_readiness`** | **`BLOCKED`** | `ACTIVE` zorunlu bağımlılık: OpenSSL `3.6.5` `PanamaCipher<LittleEndian>`, `Tiger`, `Twofish` ve `XTEA` algoritmalarını içermez; mevcut `client/files/pack/` arşivlerini bozmadan Crypto++ `8.9.0` kaldırılamaz. |
| **`d3d9ex_sole_api`** | **`VERIFIED`** | `Direct3DCreate9Ex`, `CreateDeviceEx`, `PresentEx`, `ResetEx`, `CheckDeviceState`, `IDirect3D9Ex` ve `IDirect3DDevice9Ex` tek grafik API'sidir (`0` `Direct3DCreate9` fallback; Intel UHD Graphics `test_d3d9ex_x64.exe 619/619 PASS`, resmî Microsoft `D3DX9 9.29.952.8` x64 `test_unified_sdks_x64.exe` Gate 6 `PASS`). |
| **`d3d9ex_game_runtime`** | **`UNVERIFIED`** | D3D9Ex API ve `EterLib`/`GameLib` D3DPOOL_DEFAULT staging testleri `619/619 PASS`; ancak tam `metin2client.exe` oyun içi grafik testi `client_link = BLOCKED` (`Granny2` + `SpeedTreeRT`) nedeniyle henüz çalıştırılamamıştır. |
| **`miniaudio_production_only`** | **`VERIFIED`** | `miniaudio 0.11.26` (`extern/client/include/miniaudio.h`, `SHA-256: 5b325f7d1a6fd72f94294963a441e2442887bc43f63df796a12c4a75f38b22c6`) tek ses motorudur (`test_miniaudio_x64_prototype.exe 42/42 PASS`, `test_mileslib_miniaudio_production_x64.exe 36/36 PASS`; `physical_audio_output = UNVERIFIED`). |
| **`miles_legacy_removed`** | **`REMOVED`** | `extern/client/include/MSS.H`, `#include <mss.h>`, `mss32.lib`, `mss32.dll`, `AIL_*` çağrıları ve tüm `#if !MT2_USE_MINIAUDIO` eski Miles dalları `client/src/MilesLib/` içinden kalıcı olarak kaldırıldı (`verify_sdk_inventory.py 260/260 PASS`). |
| **`stb_production`** | **`VERIFIED`** | `stb_image.h v2.30` + `stb_image_write.h v1.16` + `mt2_image_stb.h` C++20 RAII katmanı sunucu (`test_libgame 93/93 PASS`) ve istemci (`test_unified_sdks_x64.exe 137/137 PASS`) üzerinde 15 senaryoluk `guild_mark_stb_test_suite.h` altın vektörleriyle doğrulandı (`libjpeg 9.3` ve `zlib 1.3.2` aktif üretim modüllerinde `ACTIVE`). |
| **`devil_removed`** | **`REMOVED`** | `DevIL 1.8.0` başlıkları, `.lib`/`.dll`/`.so` dosyaları ve CMake/kaynak referansları tamamen kaldırıldı (`0` `<IL/il.h>` / `devil` referansı). |
| **`boost_removed`** | **`REMOVED`** | `Boost 1.92.0` (`16.336` dosya) tamamen kaldırıldı; `server/src` ve `client/src` içinde `0` `boost::` / `<boost/...>` / `BOOST_*` / `TR1_NS` referansı vardır; `Standalone Asio 1.38.2` (`ASIO_STANDALONE=1`) ayrı ve aktif SDK olarak korunmuştur. |
| **`asio_production_wiring`** | **`INTEGRATED`** | Sunucu `DESC_MANAGER::StartTls13Listeners` + `ProcessTls13Io` (`main.cpp` `io_loop` + `config.cpp` `TLS13_*`) ve istemci `CNetworkStream::SetDefaultTls13Config` (`PythonNetworkStream.cpp` `MT2_TLS13_CA_CERT` / `tls13_ca.pem`) üretim giriş noktalarına bağlandı; paket ayrıştırıcıları tek iş parçacıklı `heart` / `Process` döngüsünde çalışır (`--check-lua` `Check 11d` & `test_unified_sdks_x64.exe` Gate 9b `PASS`). |
| **`tls13_production_wiring`** | **`INTEGRATED`** | El sıkışma tamamlanmadan oyun paketi işlenmez (`AcceptTls13Desc` / `AcceptTls13P2PDesc` yalnızca el sıkışma başarı geri çağrısında çalışır); TLS 1.3 aktifken `DESC::SetSecurityKey` ve `CNetworkStream::SetSecurityMode` eski TEA şifrelemesini devreye sokmaz (`0` çift TEA şifrelemesi). |
| **`tls13_security`** | **`VERIFIED`** | `mt2_asio_tls13_transport.h` içinde `SSL_CTX_clear_options(native, SSL_OP_NO_ANTI_REPLAY)` ile OpenSSL anti-replay koruması açık tutuldu, `SSL_CTX_set_max_early_data(native, 0)` + `SSL_set_max_early_data(ssl, 0)` uygulandı ve `test_asio_tls13_security` (`191/191 PASS` — MinGW GCC 16, MSVC 18 x64 ve FreeBSD GCC 14) içinde gerçek negatif 0-RTT (`SSL_write_early_data <= 0`) ve `(SSL_CTX_get_options & SSL_OP_NO_ANTI_REPLAY) == 0` kanıtlandı. |
| **`sdk_inventory_complete`** | **`VERIFIED`** | `tests/sdk_inventory_lock.json` ve `tests/verify_sdk_inventory.py` (`260/260 PASS`) ile tüm aktif, kaldırılan (`Python27`, `MSS.H`, `dxfile.h`, `Boost`, `DevIL`, `Lua 5.0`) ve harici (`Granny2`, `SpeedTreeRT`) SDK'lar SHA-256 ve mimari kanıtlarıyla kilitlendi (`server/src/liblua` `Lua 5.5.1` tek aktif Lua motorudur). |
| **`sdk_binary_architecture`** | **`VERIFIED`** | Tüm aktif x64 `.lib` ve `.dll` ikilileri (`0x8664` `COFF_AMD64` / `PE32+`) ve PE import tabloları (`0` MinGW runtime DLL sızıntısı) `verify_sdk_inventory.py` (`260/260 PASS`) ile doğrulandı. |
| **`release_x64_project_build`** | **`VERIFIED`** | 16 `.vcxproj` projesinin tamamında `OutDir`, `/utf-8`, `OldStyle` (`/Z7`) ve `$(DefaultPlatformToolset)` yapılandırıldı (`mileslib.vcxproj` içine `MiniaudioEngine.cpp` eklendi); 15 statik kütüphane `.vcxproj` projesinin tamamı gerçek `MSBuild.exe /p:Configuration=Release /p:Platform=x64` ile derlenerek `15/15` `.lib` arşivi üretildi ve doğrulandı. |
| **`client_objects`** | **`VERIFIED`** | 16 istemci modülündeki **`344/344` çeviri biriminin tamamı** `Python27` ve `MSS.H` silindikten sonra `0` derleme hatasıyla `COFF AMD64 (0x8664)` olarak derlendi (`tests/run_client_x64_link_preflight.py`). |
| **`client_link`** | **`BLOCKED`** | `/FORCE` ve `/NODEFAULTLIB:python27*`/`liblib`/`mss32` bayrakları olmaksızın çalıştırılan katı `link.exe /MACHINE:X64` ön-kontrolünde `0` çift tanımlı sembol (`LNK2005 = 0`) ve `0` ticari olmayan çözümlenmemiş sembol (`other_non_commercial = 0`, `ddraw.lib` + `dmoguids.lib` ile `MovieMan.obj` dahil) kanıtlandı; yalnızca kalan **2 tescilli 32-bit (`14C`) kütüphane** (`Granny2`: `57` sembol, `SpeedTreeRT`: `50` sembol) nedeniyle `BLOCKED` durumdadır. |
| **`gameplay`** | **`BLOCKED`** | `client_link` (`Granny2` + `SpeedTreeRT` x64 ikilileri) tamamlanana kadar tam `metin2client.exe` oyun içi oturum testi `BLOCKED` durumdadır. |
| **`freebsd_ci`** | **`VERIFIED`** | FreeBSD 15.1 amd64 GCC 14 + MariaDB 12.3.3 LTS + OpenSSL 3.5.6 tam CI koşusu (`CTest 12/12`, `test_asio_tls13_security 191/191`, `--check-lua 11/11`, `Quest 202/202`, `test_live_db 354/354`, `--test-itemshop 15/15`, `test_db_cache_integration 187/187`, `test_server_smoke_client 29/29`, 8 servisli serverfiles + `pfctl -nf`) `SUCCESS`. |






