# MT2009 — Antigravity Hesapları Arası Proje Devir Belgesi

> Bu belge hesaptan bağımsızdır. Yeni Google Antigravity hesabı, eski hesabın sohbet geçmişine erişemese bile
> projeyi yalnızca bu depo üzerinden sürdürebilsin diye hazırlanmıştır.
> **Kuralların tek kaynağı `AGENTS.md`, güncel teknik durumun tek kaynağı `PROJECT_STATE.md` dosyasıdır.**
> Bu belge ikisinin yerine geçmez; devir anındaki fotoğrafı ve hesaba bağlı ayrıntıları kaydeder.
> Hiçbir parola, token, cookie, OAuth bilgisi, SSH anahtarı veya veritabanı sırrı bu belgeye yazılmamıştır.

- **Devir denetim tarihi:** 2026-10-29
- **Devri hazırlayan:** Eski Antigravity hesabı (konuşma `aade639f-…`, aynı Windows kullanıcı profili)
- **Yeni hesabın ilk yapacağı iş:** [`ANTIGRAVITY_NEW_ACCOUNT_CHECKLIST.md`](ANTIGRAVITY_NEW_ACCOUNT_CHECKLIST.md) içindeki salt-okunur denetimler

---

## 1. Projenin Kimliği

| Alan | Değer |
| :--- | :--- |
| Proje | Metin2 MT2009 sunucu modernizasyonu (Windows amd64 + FreeBSD amd64, tek kaynak kod) |
| Yerel çalışma klasörü | `D:\mt22009` (aynı Windows bilgisayarı kullanılacak) |
| GitHub deposu | `aidevelopervega/Metin2-MT2009` — **PUBLIC** (2026-10-29 `gh repo view` ile doğrulandı; eski belgelerdeki "Private" ifadesi yanlıştı) |
| Zorunlu geliştirme dalı | `dev/windows-gcc-baseline` |
| Korunan dal | `main` (doğrudan commit/push yasak; yerel `pre-commit` / `pre-push` kancaları engeller) |
| GitHub CLI hesabı | `x64bit-source` (depoda `ADMIN` yetkisi var; `gh` oturumu Windows keyring içinde) |

> [!WARNING]
> Depo **herkese açık (PUBLIC)**. Commit edilen her şey herkes tarafından okunabilir. Sır sızıntısı riski bu yüzden normalden yüksektir.

---

## 2. Git Durumu (Devir Anında Gerçek Kanıt)

Komutlar 2026-10-29 11:49 (UTC+3) civarında çalıştırıldı:

| Kontrol | Sonuç |
| :--- | :--- |
| `git branch --show-current` | `dev/windows-gcc-baseline` |
| `git rev-parse HEAD` (devir belgelerinden önce) | `9a3956c2d752c5addb7d5daf698be641cb28fb47` |
| `git ls-remote --exit-code origin refs/heads/dev/windows-gcc-baseline` | `9a3956c2d752c5addb7d5daf698be641cb28fb47` (çıkış kodu `0`) |
| `git rev-list --left-right --count HEAD...origin/dev/windows-gcc-baseline` | `0 0` (önde/geride commit yok) |
| `git status --porcelain=v1 --untracked-files=all` | Boş (staged / unstaged / untracked değişiklik yok) |
| Uzak `main` | `ea770d4022b0213895e95a621d0f7cf6f28cde8a` (dokunulmadı) |
| `git remote -v` | `origin https://github.com/aidevelopervega/Metin2-MT2009.git` (URL içinde kimlik bilgisi yok) |
| Kimlik doğrulama yöntemi | HTTPS + Git Credential Manager (`credential.helper=manager`); SSH kullanılmıyor |
| `core.hooksPath` | `D:/mt22009/.git/hooks` (kaynak: `.githooks/`, kurulum: `.githooks/install.ps1`) |
| `core.autocrlf` | `true` |
| Git worktree | Tek worktree: `D:/mt22009` |
| Git submodule | Yok (`.gitmodules` bulunmuyor) |
| Git LFS | `git-lfs 3.7.1` kurulu, LFS ile takip edilen dosya yok |
| `.gitattributes` | Yok |

Bu belgeyi ekleyen commit, `9a3956c2`'nin doğrudan çocuğudur. Bir commit kendi SHA'sını içeremeyeceği için son SHA `git log -1` ve `git ls-remote` ile okunmalı; devir sırasında alınan değer kullanıcıya verilen son raporda yer alır.

### 2.1 Git'te Olmayan Ama Yerelde Bulunan Dosyalar (`.gitignore` ile dışlanmış)

| Yol | Ne? | Gerekli mi? | Git'e konmalı mı? |
| :--- | :--- | :--- | :--- |
| `build/` | CMake/Ninja derleme çıktıları ve test ikilileri | Yeniden üretilebilir | **Hayır** |
| `server/src/db/db_r41023.exe`, `server/src/game/game_r41023.exe` | Derlenmiş sunucu ikilileri | Yeniden üretilebilir | **Hayır** |
| `server/files/` | Çalışan Windows serverfiles kurulumu (`auth`, `chs`, `db`, `logs`, `mysql_data`, `share`, başlatma betikleri) | Kademe C canlı testleri için **gerekli** | **Hayır** — içinde canlı veritabanı (`mysql_data`), `serverfiles.env` ve `.sql_init_client.cnf` (sır içerebilir) var. `server/serverfiles_share.tar.xz` + `server/scripts/windows/install.ps1` ile yeniden kurulabilir. |
| `server/files/serverfiles.env` | Sunucu yapılandırması (değişken adları: `MT2_DB_HOST`, `MT2_DB_PORT`, `MT2_DB_USER`, `MT2_DB_PASS`, `MT2_DB_ADDR`, `MT2_AUTH_PORT`, `MT2_AUTH_P2P_PORT`, `MT2_SERVER_KEY`) | Gerekli | **Asla** (sır). Şablon: `server/config/serverfiles.env.example` (Git'te) |
| `object/` | Quest derleyicisi (`qc`) çıktıları | Yeniden üretilebilir | Hayır |
| `scratch/quest_matrix_results.json` | `run_quest_matrix.py` son çıktısı | Yeniden üretilebilir | Hayır |
| `scratch/handoff/` | **Bu devirde oluşturuldu** — aşağıdaki §6.3'e bakın | Kademe C için **gerekli** | **Hayır** (içinde test parolaları olan eski betikler ve 322 MB MariaDB ikilisi var) |
| `kurulum.py`, `push_to_github.bat`, `Packleri açmak için.txt`, `packleri açmak.png` | `.gitignore` tarafından "hassas" olarak işaretli yerel dosyalar | Projenin derlenmesi için gerekli değil | **Asla** (içerikleri okunmadı) |
| `locales-mt2009.rar`, `PackMakerLite-v3.0-20240214-2147.zip` | İstemci/paket arşivleri | İstemci ambargosu nedeniyle şu an kullanılmıyor | Hayır |
| `client/files/`, `extern/client/lib/*.obj`, `*.vcxproj.user` | İstemci ikilileri ve IDE dosyaları | İstemci ambargosu | Hayır |
| `server/src/libthecore/src/tea.s` | FreeBSD derleme artığı | Hayır | Hayır |

**Sonuç:** Git'e eklenmesi gereken ve güvenli olan eksik bir kaynak dosyası yoktur. Proje için gerekli olup Git dışında kalanlar (serverfiles kurulumu, yerel MariaDB ikilisi, sırlı yapılandırma) bilerek Git dışında tutulmaktadır ve aynı Windows profilinde diskte mevcuttur.

---

## 3. Mimari

- **Sunucu bileşenleri:** `db_r41023` (DB önbellek sunucusu), `game_r41023` (auth + kanal/çekirdek), `qc` (quest derleyicisi), kütüphaneler `libthecore`, `libsql`, `libgame`, `libpoly`, `liblua` (yalnızca Lua 5.5.1).
- **Ağ protokolü:** x64 paket sözleşmesi `MT2_PROTOCOL_VERSION_X64 = 0x20090064` (`server/src/common/tables.h:17`). Tüm paketler `#pragma pack(1)`.
- **Ekonomi:** `GOLD_MAX = 1.000.000.000.000` Yang ve `EXCHANGE_GOLD_MAX` aynı değer (`server/src/common/length.h:10-11`), `YANG = int64_t`.
- **Kalıcı kimlikler:** `AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID` → C++ `uint64_t`, SQL `bigint(20) unsigned`. Oturum içi `VID` bilerek `uint32_t` kaldı (çakışma korumalı).
- **Veritabanı:** Yalnızca MariaDB 12.3 LTS (istemci kütüphanesi MariaDB Connector/C 3.4.10, `libmariadb`).
- **İstemci:** 32-bit Windows; `client/**` ve `extern/client/**` **ambargolu** (salt-okunur referans).

### 3.1 Proje Dosya Haritası

| Yol | İçerik |
| :--- | :--- |
| `AGENTS.md` | Değişmez çalışma kuralları (tek kural kaynağı) |
| `PROJECT_STATE.md` | Doğrulanmış teknik durum (tek durum kaynağı) |
| `docs/SERVER_X64_GOALS.md` | `G0`–`G11` x64 hedef ve kusur envanteri |
| `docs/CLIENT_X64_PROTOCOL_MIGRATION.md` | İstemci tarafının ileride uyarlaması gereken paket sözleşmesi |
| `docs/migration/*` | Rev 19 dönemi göç envanteri (tarihsel) |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | Derleme sistemi (presetler: `windows-gcc-ucrt`, `freebsd-gcc14`) |
| `server/src/{common,db,game,libsql,libthecore,libgame,libpoly,liblua}` | Sunucu kaynak kodu |
| `server/scripts/{windows,freebsd}/` | Serverfiles kurulum/başlatma/durdurma/yedekleme betikleri |
| `server/config/*.example` | Sırsız yapılandırma şablonları |
| `server/serverfiles_share.tar.xz` | Oyun verisinin tek resmî arşivi (`share/data`, `share/locale/poland`, `share/package`) |
| `sql/*.sql`, `sql/migrations/*.sql` | Şemalar ve migration zinciri |
| `tests/` | Birim, entegrasyon, canlı DB ve duman test kaynakları ile betikleri |
| `.github/workflows/freebsd-gcc-test.yml` + `ci/run-full-freebsd.txt` | Bilinçli tetiklenen FreeBSD CI |
| `.githooks/` | `main` koruma kancaları |

---

## 4. Araç Zinciri (2026-10-29 itibarıyla doğrulandı)

| Araç | Sürüm / Konum |
| :--- | :--- |
| GCC | `gcc 16.2.0` (MinGW-W64 x86_64-ucrt-posix-seh, WinLibs, Brecht Sanders r1) — `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_…\mingw64\bin` (PATH'te) |
| CMake | `4.4.2` |
| Ninja | `1.13.2` |
| Python | `3.11.9` (`%LOCALAPPDATA%\Programs\Python\Python311`) |
| PowerShell | Windows PowerShell `5.1.26100` (`pwsh` kurulu değil) |
| Git | `2.55.0.windows.3`, Git LFS `3.7.1` |
| GitHub CLI | `gh 2.102.0` (`C:\Program Files\GitHub CLI`) |
| MariaDB (test) | `12.3.3-MariaDB for Win64` — **PATH'te değil, sistem servisi değil.** Taşınabilir ikili; bkz. §6.3 |
| Lua | 5.5.1 (kaynak olarak depoda, `liblua`; ayrı kurulum gerekmez) |
| actionlint | PATH'te yok (eski oturum `scratch/actionlint` içindeki kopyayı kullanıyordu) |
| Antigravity IDE | `1.107.0` |

Önemli ortam değişkenleri **kalıcı olarak tanımlı değil**; test betikleri çalışırken geçici olarak atar. Kullanılan değişken adları (değerler belgelenmez):
`MT2_TEST_HOST`, `MT2_TEST_PORT`, `MT2_TEST_USER`, `MT2_TEST_PASSWORD`, `MT2_TEST_DB`, `MT2_TEST_ENGINE`, `MT2_TEST_SSL_CA`, `MT2_TEST_INVALID_SSL_CA`, `MT2_RUNTIME_USER`, `MT2_RUNTIME_PASSWORD`, `MT2_LOG_DB_USER`, `MT2_LOG_DB_PASS`, `MT2_BACKUP_DB_USER`, `MT2_BACKUP_DB_PASS`, `MT2_DB_HOST`, `MT2_DB_PORT`, `MT2_DB_USER`, `MT2_DB_PASS`, `MT2_DB_BIN_DIR`, `MT2_MYSQL_DATADIR`.

---

## 5. Çalışma Kuralları Özeti (ayrıntı ve bağlayıcı metin: `AGENTS.md`)

- Yalnızca `dev/windows-gcc-baseline`; `main` değiştirilmez.
- `client/**`, `extern/client/**`, `ci/run-full-freebsd.txt` değiştirilmez.
- FreeBSD CI yalnızca kullanıcı izniyle ve `ci/run-full-freebsd.txt` değiştirilerek tetiklenir; izin yoksa çalıştırılmaz.
- FreeBSD 15.1 / GCC 14 üzerinde doğrulanmamış her şey **UNVERIFIED**'dır.
- MariaDB 12.3 LTS, Lua 5.5.1, 202 quest, `0x20090064`, `GOLD_MAX = 10^12` sabittir.
- `-Wno-*`, `-w`, `#pragma diagnostic ignored`, SQL mode gevşetme, `INSERT IGNORE` gibi maskeleme yasaktır.
- Kullanıcıya terminal komutu çalıştırma görevi verilmez; açıklamalar sade Türkçe olur.
- Üretim sunucusuna ve üretim veritabanına müdahale edilmez.
- Test kademeleri: `A` (statik) → `B` (Windows derleme + birim) → `C` (canlı MariaDB + protokol) → `D` (FreeBSD CI, yalnızca izinle).

### 5.1 CPU Kuralı (yalnızca derleme)

| Öğe | Değer |
| :--- | :--- |
| Zorunlu araç | `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_limited.ps1 -Build` (`-CleanFirst` isteğe bağlı) |
| CPU hard-cap | Job Object `%50` |
| Affinity | `0xFC0` (6 mantıksal işlemci: LP 6..11) |
| Paralellik | En fazla `-j6` |
| Öncelik | `BelowNormal` |
| Derleme dışı işler | `tests/run_limited.ps1 -CTest` / `-Command "..."` veya doğrudan çalıştırma: **sınırsız** (Normal öncelik, 12/12 LP) |

---

## 6. Test Ortamı

### 6.1 Konumlar

| Öğe | Konum |
| :--- | :--- |
| Windows build dizini | `D:\mt22009\build\windows-gcc-ucrt` (preset `windows-gcc-ucrt`) |
| Test ikilileri | `build\windows-gcc-ucrt\tests\*.exe` (`test_libpoly`, `test_libthecore`, `test_libgame`, `test_libsql`, `test_db_cache`, `test_db_cache_integration`, `test_packet_wire`, `test_lua55_quest`, `test_auth_security`, `test_live_db`, `test_server_smoke_client`) |
| Sunucu ikilileri | `server\src\db\db_r41023.exe`, `server\src\game\game_r41023.exe` → serverfiles için `server\files\share\bin\` içine kopyalanır |
| Quest matrisi | `python tests/run_quest_matrix.py` (çıktı: `scratch/quest_matrix_results.json`) |
| Lua öz-denetimi | `server\src\game\game_r41023.exe --check-lua` |
| ItemShop canlı testi | `game_r41023.exe --test-itemshop --host 127.0.0.1 --port 3307 --user … --db mt2_testdb` |
| Canlı DB testi | `build\windows-gcc-ucrt\tests\test_live_db.exe` (MT2_TEST_* değişkenleriyle) |
| Test MariaDB portu | `3307` (yalnızca `127.0.0.1`; üretim portu değil) |
| Serverfiles | `D:\mt22009\server\files` (`db`, `auth`, `chs\ch11` … ; başlatma `server\scripts\windows\start.ps1`) |
| Repo içi kalıcı betikler | `tests/run_limited.ps1`, `tests/run_tests.ps1`, `tests/run_live_tests.ps1`, `tests/run_db_integration_tests.ps1`, `tests/run_quest_matrix.py`; FreeBSD tarafı `tests/*.sh` |

### 6.2 Referans Test Sonuçları (eski hesap, 2026-10-29 02:23, SHA `9a3956c2`)

| Test | Sonuç | Yeni hesapta durum |
| :--- | :--- | :--- |
| `tests/run_limited.ps1 -Build` | 0 hata, 0 uyarı (`-Wall -Wextra`) | **Yeniden doğrulanmadı** |
| CTest | 10/10 | **Yeniden doğrulanmadı** |
| `test_packet_wire` | 123/123 | **Yeniden doğrulanmadı** |
| `test_db_cache` | 129/129 | **Yeniden doğrulanmadı** |
| `test_db_cache_integration` | 162/162 çevrimdışı, 187/187 canlı MariaDB | **Yeniden doğrulanmadı** |
| `test_live_db` | 354/354 | **Yeniden doğrulanmadı** |
| `--test-itemshop` | 15/15 | **Yeniden doğrulanmadı** |
| `--protocol-framing-audit` | 10/10 (4/4 kategori) | **Yeniden doğrulanmadı** |
| `--check-lua` | 11/11 | **Yeniden doğrulanmadı** |
| `run_quest_matrix.py` | 202/202 | **Yeniden doğrulanmadı** |

Bu değerler önceki raporlardır; yeni hesapta tekrar çalıştırılmadıkça "doğrulandı" sayılmaz.

### 6.3 Oturuma Bağlı Test Varlıkları (KRİTİK DEVİR BULGUSU)

Kademe C canlı testleri (MariaDB 12.3.3 ile `test_live_db`, `--test-itemshop`, idempotent migration, `test_db_cache_integration` canlı modu, serverfiles duman testi) eski hesapta **depo içinde değil, eski Antigravity oturumunun geçici `scratch` klasöründeki betiklerle** çalıştırılıyordu. MariaDB 12.3.3 taşınabilir ikilisi de yalnızca orada duruyordu.

Kayıp riskini kaldırmak için bu devirde (silme/taşıma yapmadan) **kopya** alındı:

| Kopya (Git dışı, `.gitignore: scratch/`) | İçerik |
| :--- | :--- |
| `D:\mt22009\scratch\handoff\isolated_db\mariadb-12.3.3-winx64\` | MariaDB `12.3.3-MariaDB for Win64` taşınabilir ikilisi (650 dosya, ~322 MB; `mariadbd.exe --version` ile doğrulandı) |
| `D:\mt22009\scratch\handoff\session_scripts_original\` | Eski oturum betiklerinin değiştirilmemiş kopyaları: `run_g2_g5_kademe_c.ps1` (Kademe C ana betiği), `run_part5_protocol_smoke.ps1` (serverfiles + framing + login duman testi), `run_final_x64_live_verification.ps1`, `run_post_rev51_live.ps1`, `run_audit_guard_test.ps1`, `run_red_green_3bugs.ps1`, `run_win_tier_c.ps1`, `verify_db_cache_176_and_warnings.ps1`, `rescan_modified_tus.py`, `run_extended_warnings.py`, `scan_current_warnings.py` |

> [!CAUTION]
> Bu betikler içinde **yalnızca yerel test** kullanıcıları için sabit test parolaları ve eski oturumun mutlak yolu (`%USERPROFILE%\.gemini\antigravity\brain\aade639f-…\scratch`) bulunur. Oldukları gibi Git'e eklenmemeli ve kalıcı prosedür olarak kullanılmamalıdır. Kalıcı çözüm açık hedef **H-1**'dir (§9).

Betikler çalışırken veri dizinini (`live_mariadb_1233_data`) **silip yeniden oluşturur**; yalnızca izole test veri dizinine işaret ettiklerinden emin olunmadan çalıştırılmamalıdır. `server\files\mysql_data` serverfiles veritabanıdır; silinmemelidir.

---

## 7. SQL Migration Zinciri

| Sıra | Dosya | Amaç |
| :--- | :--- | :--- |
| Temel şema | `sql/account.sql`, `sql/common.sql`, `sql/log.sql`, `sql/player.sql`, `sql/world.sql` | Temiz kurulum (64-bit kimlikler dahil) |
| 1 | `sql/migrations/rev47_account_security_upgrade.sql` | Hesap güvenliği (Argon2id vb.) |
| 2 | `sql/migrations/rev51_halloween_hair_attr_migration.sql` | Rev 51 saç/özellik verisi |
| 3 | `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql` | `log` şemasına IkarusShop/ItemShop tabloları |
| 4 | `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql` | x64 `BIGINT UNSIGNED` kimlik genişletmesi (account/player/common/log), `itemshop_tx_journal` (`req_id`, `'RECOVERING'`), tüm `log.*` tabloları için `ALTER` |

Kanıt (eski hesap, Windows): temiz 5 şema yükleme + 2. ve 4. migration'ın iki kez art arda (idempotent) uygulanması `PASS`; serverfiles veritabanında `20261027_…` iki kez uygulanıp `AID/PID = 5.000.000.001` ile `0 syserr`. Mevcut üretim verisinde uygulanmadan önce **doğrulanmış yedek + geri dönüş planı** zorunludur (`AGENTS.md` §4.7). FreeBSD'de bu migration **UNVERIFIED**.

---

## 8. Yapılmış Önemli Geliştirmeler (kısa tarihçe)

| Dönem | Sonuç |
| :--- | :--- |
| Rev 1–19 | Windows GCC (MinGW-w64 UCRT) temel derleme, `libpoly`/`libgame`/`libthecore`/`libsql` kök neden düzeltmeleri, kalıcı test altyapısı |
| Rev 20–50 | CMake + Ninja birleşik derleme, Extern yükseltmeleri (Boost, Crypto++, fmt, RapidJSON, DevIL, OpenSSL, zlib, Connector/C), MariaDB/MySQL çift motor testleri, FreeBSD 15.1 GCC 14 CI |
| Rev 51 (`da1f6775`) | Yalnızca Lua 5.5.1, 202 quest, MariaDB 12.3 LTS tek motor; FreeBSD CI `39859687668` **SUCCESS** (bu, FreeBSD'de son tam başarılı koşudur) |
| Rev 51 sonrası güvenlik (`771b93ab`) | FreeBSD CI `40182578376` → **FAILURE** (bkz. §10) |
| `374a3f41` | G1: 64-bit ekonomi, para çoğaltma/kaybı kusurları |
| `625e5284` | G2/G4: LP64/LLP64 sabit genişlik, paket sınır kontrolleri, VID/ItemID aralık sertleştirme |
| `6c6732fd` | G3/G5: ItemShop dayanıklı işlem günlüğü, SQL kaçışlama ve sızıntılar |
| `114b2d57` | G3/G4: kalıcı `req_id` idempotency, TCP framing (250 ms yarım çerçeve) |
| `9a3956c2` | G2/G5/G6/G7/G8: kalıcı kimliklerin 64-bit'e taşınması, Lua `lua_Integer`, `log.*` migration, 0 uyarı |

CPU sınırı kararı: önceki `0xF00` (4 LP) maskesi kaldırıldı, `0xFC0` (6 LP) + `%50` hard-cap + `-j6` + `BelowNormal` yalnızca derlemeye uygulanır.

---

## 9. Kalan Açık Hedefler ve Kanıt Seviyesi

`docs/SERVER_X64_GOALS.md` tüm `G0`–`G11` hedeflerini `VERIFIED` gösterir. Bu devir denetiminde bu işaretler sorgusuz kabul edilmemiş, aşağıdaki boşluklar bulunmuştur:

| ID | Konu | Durum | Açıklama |
| :--- | :--- | :--- | :--- |
| **H-1** | Kademe C betiklerinin depoya alınması | **OPEN** | Canlı MariaDB / ItemShop / migration / serverfiles duman testleri yalnızca oturum betikleriyle çalışıyor (§6.3). `AGENTS.md` §3.4 gereği sırsız, parametreli (`-MariaDbHome`, `-DataDir`, parolalar ortam değişkeninden veya çalışma anında rastgele) bir `tests/` betiğine dönüştürülüp Windows'ta gerçekten çalıştırılarak doğrulanmalı. |
| **H-2** | FreeBSD CI son durumu | **OPEN / FAILURE** | `40182578376` (Job `121136559119`, SHA `e364196e`) `[4.A]` MariaDB yeniden başlatma kalıcılık adımında hata verdi: değerler `127.0.0.1\|OFF\|ON\|CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT` okunduğu hâlde betik `did not persist` dedi (muhtemelen beklenen değer karşılaştırma hatası). `PROJECT_STATE.md` önceden bunu `PASS` olarak yazıyordu; düzeltildi. Kök neden yerelde bulunmadan CI yeniden çalıştırılamaz. |
| **H-3** | `G1`–`G11` FreeBSD doğrulaması | **UNVERIFIED** | `374a3f41`…`9a3956c2` arasındaki tüm x64 ekonomi, kimlik, framing, ItemShop ve migration değişiklikleri FreeBSD 15.1 / GCC 14 üzerinde hiç derlenip test edilmedi. |
| **H-4** | 64-bit kimlik zincirinde kalan 32-bit daraltmalar | **OPEN** | `server/src/game/src/char.cpp:7125` (`requestPID`), `char.cpp:7791` (`guildID`), `char.cpp:9652` (captcha `who.pid`), `char_item.cpp:3889, 3932` (eşya kimliğinin `DWORD` affect alanına yazılması), `priv_manager.cpp:121` (`GUILD_PRIV` log'unda `guild_id`). Bunlar 32-bit istemci paketleri veya log alanları yüzünden `static_cast<DWORD>` ile kesiliyor; `> 2^32` kimliklerde sessiz yanlış değer üretir. "Uçtan uca 64-bit" iddiası bu noktalarda tam değildir; ya istemci protokolüyle birlikte genişletilmeli ya da fail-closed davranış + test eklenmelidir. |
| **H-5** | Lua kesinlik sınırı | **BİLİNEN SINIR** | Lua 5.5.1 `lua_Integer` işaretli 64-bit'tir; `uint64_t` kimlikler `2^63` üzerinde negatif görünür. `> 2^53` test edildi; `>= 2^63` davranışı belgelenmemiş/test edilmemiş. |
| **H-6** | Genişletilmiş derleyici uyarıları | **OPEN / UNVERIFIED** | 0 uyarı kanıtı yalnızca `-Wall -Wextra` içindir. `-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Wformat=2` gibi genişletilmiş set eski oturumda yalnızca `scratch` betikleriyle tarandı; depoda tekrarlanabilir bir sıfır kanıtı yok. |
| **H-7** | Zayıf test kanıtlı alt hedefler | **WEAK EVIDENCE** | `G1.4` (Cube/DragonSoul ücretleri), `G2.3` (ItemAward tampon taşması), `G4.2` (`m_aiPremiumTimes` sıfırlama), `G4.4` (affect `int64_t`) için kanıt yalnızca genel "CTest 10/10"; bu davranışları doğrudan ölçen özel test yok. |
| **H-8** | İstemci x64 protokol uyarlaması | **BLOCKED (ambargo)** | Sunucu `0x20090064` x64 sözleşmesini kullanıyor; mevcut 32-bit istemci bu sunucuya bağlanamaz. İstemci ambargosu kullanıcı açıkça kaldırana kadar yapılamaz (`docs/CLIENT_X64_PROTOCOL_MIGRATION.md`). |
| **H-9** | Kademe A araçları | **OPEN** | `actionlint` PATH'te yok; YAML denetimi için eski oturum kopyası kullanılıyordu. |

---

## 10. FreeBSD Durumu

| Ortam | Durum |
| :--- | :--- |
| Son başarılı tam FreeBSD CI | `39859687668` (Rev 51, kaynak `da1f6775`, `1/1 SUCCESS`) |
| Son FreeBSD CI koşusu | `40182578376` → **FAILURE** (`[4.A]`, bkz. H-2) |
| `9a3956c2` ve x64 çalışmaları | **UNVERIFIED** (CI tetiklenmedi, `ci/run-full-freebsd.txt` değiştirilmedi) |
| Kalıcı FreeBSD VM Git deposu | Yok / erişilemiyor → **UNKNOWN** |
| CI tetikleme koşulu | Yalnızca `dev/windows-gcc-baseline` dalında `ci/run-full-freebsd.txt` değişirse (`paths` filtresi). Sıradan belge push'ları CI başlatmaz. |
| Bütçe | Depo PUBLIC olduğundan standart GitHub-hosted runner dakikaları genelde ücretsizdir; yine de `AGENTS.md` §6.4 bütçe kuralı (2.400 + 600 dk) korunur ve hesap geneli kullanım bilinmiyorsa `UNKNOWN` raporlanır. |

---

## 11. Hesaba Bağlı Ayarlar ve Antigravity Envanteri

### 11.1 Profil Klasörleri (salt-okunur tarandı, içerik kopyalanmadı)

| Konum | Var mı? | Ne içeriyor? | Projeye özgü kural? | Yeni hesap için gerekli mi? | Hesaba bağlı / hassas? | Aktarım |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `%APPDATA%\Antigravity` | Var (177 dosya, ~35 MB) | Antigravity uygulamasının Chromium profili: Cookie/Network, Local/Session Storage, `Preferences`, `app_storage.json` (son giriş kullanıcı adı, yerleşim tercihleri) | Hayır | Hayır | **Evet**, oturum/cookie içerir | **Aktarılmaz** |
| `%APPDATA%\Antigravity IDE` | Var (484 dosya, ~65 MB) | IDE (VS Code tabanlı) profili; `User\settings.json` yalnızca `dart.flutterSdkPath`, `python.languageServer` anahtarları; workspaceStorage, cache | Hayır | Hayır | Kısmen (oturum/cache) | Aktarılmaz; aynı Windows kullanıcısında zaten mevcut |
| `%USERPROFILE%\.gemini\antigravity` | Var (~44.388 dosya, ~1,5 GB) | `brain\` (7 klasör: bu konuşma + 5 alt ajan + medya), `conversations\`, `conversation_summaries.db`, `installation_id`, `builtin\` skills; `annotations`, `bin`, `crashes`, `lib`, `antigravity_state.pbtxt` sandbox tarafından **erişilemedi** | Eski konuşmada teknik karar kaydı var (bu belgeye aktarıldı) | Hayır (kararlar artık depoda) | **Evet** — konuşmalar hesaba bağlı; yeni hesapta okunabilirliği kanıtlanmadı | Aktarılmaz; yalnızca gerekli test varlıkları `scratch\handoff` içine kopyalandı (§6.3) |
| `%USERPROFILE%\.gemini\antigravity-ide` | Var | Sandbox tarafından **erişilemedi** | Bilinmiyor | Bilinmiyor | Muhtemelen | Erişilemedi olarak raporlandı |
| `%USERPROFILE%\.gemini\antigravity-cli` | Yok | — | — | — | — | — |
| `%USERPROFILE%\.gemini\config` | Var | `config.json` (sandbox **erişilemedi**), `mcp_config.json` (**0 bayt → MCP sunucusu yok**), `projects\e6bae02a-….json` (yalnızca `mt22009` → `D:\mt22009` eşlemesi, `settings: {}`), `plugins\` (boş), `sidecars\` (boş), `node_modules\` (erişilemedi), `.migrated` | Hayır | Hayır | `config.json` hesap/izin tercihleri içerebilir | Aktarılmaz |
| `%USERPROFILE%\.gemini\skills` | Yok | — | — | — | — | — |
| `%USERPROFILE%\.gemini\AGENTS.md`, `GEMINI.md` | Yok | Global kural yok | — | — | — | — |
| `%USERPROFILE%\.ssh` | Sandbox **erişilemedi** | İncelenmedi (özel anahtar okunmadı) | — | **Hayır** — Git HTTPS + Credential Manager kullanıyor, SSH gerekmiyor | Evet | Aktarılmaz |
| `%USERPROFILE%\.gemini\sandbox` | Var | `python\` (sandbox çalışma ortamı) | Hayır | Hayır | Hayır | Aktarılmaz |
| `D:\mt22009\AGENTS.md` | Var | **Tek etkin workspace kuralı** (Antigravity tarafından otomatik yükleniyor) | Evet | Evet | Hayır (sır yok) | Git'te; otomatik |
| `D:\mt22009\GEMINI.md`, `.agents\`, `.gemini\`, `.agent\` | Yok | — | — | — | — | — |
| Alt dizinlerde `AGENTS.md`/`GEMINI.md`/`SKILL.md`/`rules`/`workflows` | Yok | (`server\files\…\quest\systems\skills` bir oyun quest klasörüdür; `.github\workflows` GitHub Actions'tır, Antigravity workflow'u değildir) | — | — | — | — |

### 11.2 Kurallar, Skills, MCP, Hooks, Workflows

| Öğe | Bulgu |
| :--- | :--- |
| Global Rules (`~/.gemini/AGENTS.md`, `~/.gemini/GEMINI.md`, `~/.gemini/config/rules/`) | Yok |
| Workspace Rules | Yalnızca `D:\mt22009\AGENTS.md` |
| Workspace Skills (`.agents/skills/`) | Yok |
| Global / CLI Skills (`~/.gemini/config/skills/`, `~/.gemini/antigravity-cli/skills/`, `~/.gemini/skills`) | Yok (yalnızca Antigravity'nin yerleşik `builtin` skills'i) |
| Custom subagents | Kalıcı tanım yok; eski konuşmada yalnızca o konuşmaya özel `research` alt ajanları kullanıldı |
| MCP (`~/.gemini/config/mcp_config.json`, `.agents/mcp_config.json`) | Global dosya boş (0 bayt), workspace dosyası yok |
| Hooks (`~/.gemini/config/hooks.json`, `.agents/hooks.json`) | Yok |
| Plugins (`~/.gemini/config/plugins/`) | Boş |
| Legacy Workflows (`.agents/workflows/`, `~/.gemini/config/workflows/`) | Yok → Skills'e dönüştürülecek eski workflow bulunmuyor |
| Terminal Auto Execution / Non-workspace File Access / izinler / model tercihleri | Uygulama arayüzünde ve `~/.gemini/config/config.json` içinde tutuluyor; dosya sandbox tarafından korunduğu için okunamadı. Hesaba/kuruluma bağlıdır; yeni hesapta arayüzden yeniden ayarlanmalı. |

Taşınması gereken MT2009'a özel ve güvenli kuralların tamamı zaten `AGENTS.md` içindedir; bu devirde `AGENTS.md`'deki çelişkiler giderildi (bkz. `PROJECT_STATE.md` §9). Alakasız global ayar projeye kopyalanmadı.

### 11.3 Yeniden Yetkilendirilmesi Gereken Entegrasyonlar

| Entegrasyon | Durum | Yeni hesapta yapılacak |
| :--- | :--- | :--- |
| Google / Antigravity oturumu | Eski hesaba bağlı | Yeni Google hesabıyla giriş (kullanıcı arayüzden yapar) |
| Antigravity araç izinleri (terminal otomatik çalıştırma, workspace dışı dosya erişimi, sandbox bypass onayları) | Hesap/kurulum tercihi | Yeni hesapta arayüzden gözden geçirilmeli |
| Git (HTTPS) | Windows Credential Manager'da, Windows kullanıcısına bağlı; Antigravity hesabından bağımsız | Aynı Windows kullanıcısında genelde çalışmaya devam eder; `git ls-remote` ile doğrulanmalı |
| GitHub CLI | `x64bit-source` hesabıyla Windows keyring'de oturum açık (kapsamlar: `gist`, `read:org`, `repo`, `workflow`) | `gh auth status --hostname github.com` ile doğrulanmalı; token **asla** çıkarılmamalı (`AGENTS.md` §5.2) |
| Antigravity sandbox ağ erişimi | `github.com` sandbox içinden çözümlenemiyor | `git push`, `git ls-remote`, `gh` komutları kullanıcı onaylı sandbox dışı çalıştırma gerektirir |

---

## 12. Yeni Hesap İlk Açılış Kontrol Listesi (özet)

Ayrıntılı ve başarı ölçütlü sürüm: [`ANTIGRAVITY_NEW_ACCOUNT_CHECKLIST.md`](ANTIGRAVITY_NEW_ACCOUNT_CHECKLIST.md).

1. Workspace olarak `D:\mt22009` açıldı mı ve `AGENTS.md` otomatik kural olarak yüklendi mi?
2. `AGENTS.md`, `PROJECT_STATE.md`, bu belge ve `docs/SERVER_X64_GOALS.md` okundu mu?
3. Dal `dev/windows-gcc-baseline`, çalışma ağacı temiz, yerel HEAD = `git ls-remote` uzak HEAD mi?
4. `gh auth status` başarılı mı (token göstermeden)?
5. Araç sürümleri §4 ile aynı mı?
6. `scratch\handoff\isolated_db\…\mariadbd.exe --version` → `12.3.3` mü?
7. İlk geliştirme görevi H-1 (Kademe C betiğini depoya almak) olmalıdır; ardından H-2.
8. Kullanıcı izni olmadan FreeBSD CI tetiklenmez, `client/**` değiştirilmez.
