# MT2009 — Sunucu ve İstemci Native x64 Teknik Sözleşmesi, Doğrulama Matrisi ve Kabul Kriterleri (`SERVER_X64_GOALS.md`)

**Depo:** `aidevelopervega/Metin2-MT2009` | **Dal:** `dev/windows-gcc-baseline` | **Doğrulanmış Sunucu & FreeBSD CI SHA:** `1d6d5c6db59bf20b4fd9a01fd30f0ffdd31882bd` (Run `41183346026` — `SUCCESS`)

Bu belge, sunucu (Windows amd64 + FreeBSD 15.1 amd64) ve istemci (Windows native x64 `PE32+`) için ortak x64 teknik sözleşmesini, tamamlanan ve doğrulanmış sunucu dönüşüm kanıtlarını, `MT2_PROTOCOL_VERSION_X64 = 0x20090064U` ağ paket tablosunu, gerçek native x64 istemci bağımlılık engellerini ve kabul kriterlerini içerir.

---

## 1. Ana Hedef Özeti (`G0` – `G11` — Sunucu Tamamı `VERIFIED`)

| ID | Priority | Status | Kapsam |
| :--- | :--- | :--- | :--- |
| `G0` | `P0` | `VERIFIED` | 16 sunucu alanının tamamında (`game`, `db`, `auth`, `P2P`, ortak başlıklar, SQL, ekonomi, ItemShop, IkarusShop, Lua 5.5.1, LP64/LLP64, testler) uçtan uca kod denetimi |
| `G1` | `P0` | `VERIFIED` | Tam 64-bit ekonomi (`GOLD_MAX = 1.000.000.000.000` Yang), taşma/daraltma/negatif fiyat/para çoğaltma ve para kaybı kusurlarının giderilmesi |
| `G2` | `P0` | `VERIFIED` | Kalıcı kimliklerin (`AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`) uçtan uca gerçek 64-bit (`uint64_t` / `BIGINT UNSIGNED`) kapasiteye taşınması (`> 2^{32}` ve `> 2^{53}` Lua 5.5.1 `lua_Integer` kesinliği dahil); `VID` çalışma zamanı çakışma/ömür garantileri |
| `G3` | `P0` | `VERIFIED` | ItemShop işlem bütünlüğü (`A`–`L`): kalıcı `req_id` idempotency, çoklu `item_award.why` benzersizliği, mevcut `vnum` ayrımı, senkron `player.item` yazımı, `'RECOVERING'` ikinci çökme ve çoklu süreç yarış testleri (`15/15 PASS`) |
| `G4` | `P0` | `VERIFIED` | TCP framing (`Login3`/`Login2` parçalı/1-bayt/eski-uzunluk sınırlarında erken ret hatasının giderilmesi), 250ms boşta yarım çerçeve zaman aşımı, `0x20090064` x64 paket sözleşmesi ve canlı soket çerçeveleme testleri (`10/10 PASS`) |
| `G5` | `P0` | `VERIFIED` | MariaDB 12.3 LTS şema ve x64 kimlik (`BIGINT UNSIGNED`) migration'ları (`account`, `common`, `log`, `player`), PK/FK/JOIN/`AUTO_INCREMENT` bütünlüğü ve canlı DB doğrulaması (`354/354 PASS`) |
| `G6` | `P1` | `VERIFIED` | Tüm 194 derleme hedefinde (`db_r41023`, `game_r41023`, `qc`, statik kütüphaneler ve test ikilileri) `-Wall -Wextra` altında `0` hata ve `0` uyarı (`0` `-Wno-*` bastırma bayrağı) doğrulaması |
| `G7` | `P0` | `VERIFIED` | $2^{31}$, $2^{32}$, $10^{12}$ sınırları, $> 2^{32}$ ve $> 2^{53}$ kalıcı ID'ler, TCP framing (`10/10`) ve ItemShop (`A`–`L`, `15/15`) regresyon testlerinin tamamlanması |
| `G8` | `P0` | `VERIFIED` | `G2`–`G7` tamamlandıktan sonra gerçek ikinci uçtan uca denetim (canlı `AID/PID = 5.000.000.001 > 2^{32}` oturum/quest/log duman testi ve `0 syserr` doğrulaması dahil) |
| `G9` | `P0` | `VERIFIED` | `docs/SERVER_X64_GOALS.md` teknik sözleşme ve durum takibi |
| `G10` | `P0` | `VERIFIED` | Yalnızca derleme (`cmake`/`ninja`/`g++`) sırasında `tests/run_limited.ps1 -Build` (`%50` CPU sınırı, `-j6`, `0xFC0`, `BelowNormal`) kullanımı |
| `G11` | `P0` | `VERIFIED` | Çalışma sınırları (`dev/windows-gcc-baseline`, `ci/run-full-freebsd.txt` onay kuralı ve 3 ana belge disiplini) |

---

## 2. Detaylı Alt Hedef ve Kusur Envanteri (`G1` – `G7`)

### Grup A — `G1`: Tam 64-Bit Ekonomi, Mağazalar, Lonca, Cube, DragonSoul, Quest Lua ve Loglar

| ID | Priority | Status | Source Files | Root Cause | Fix Applied | Test Evidence | Commit SHA | Remaining Risk |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `G1.1` | `P0` | `VERIFIED` | `server/src/game/src/input_main.cpp:3010-3026`, `server/src/game/src/guild.cpp:1640-1672` | `GUILD_SUBHEADER_CG_CHARGE_GSP` içinde `const int gold = offer * 100;` ve `CGuild::ChargeSP` içinde `YANG gold = iSP * 300;` 32-bit işaretli `int` çarpımı yapıyor; `offer = 7.158.279` gönderildiğinde `iSP * 300` değeri `-2.147.483.596` olarak taşıyor ve `ch->ChangeGold(-gold)` oyuncuya bedava `+2,14 milyar` Yang veriyor. | `offer <= 0 \|\| static_cast<YANG>(offer) > (GOLD_MAX - 1) / 300` ve `static_cast<YANG>(iSP) * 300` 64-bit taşma koruması uygulandı. | `test_db_cache` (`129/129 PASS`, Senaryo J.1) | `374a3f41` | Yok |
| `G1.2` | `P0` | `VERIFIED` | `server/src/game/src/ikarus_shop_manager.cpp:77-85, 678-735, 1132-1158, 3261-3279`, `server/src/db/src/IkarusShopCache.cpp:93-104, 868-874, 953-966, 1260`, `server/src/db/src/ClientManagerIkarusShop.cpp:2163` | `IsGoodSalePrice` `price.yang == 0` değerine izin veriyor; `OpenOfflineShop` fiyat/toplam doğrulamasından önce ücreti kesip eşyaları siliyor; `RecvShopAddItemClientPacket`, `RecvShopEditItemClientPacket` ve `CShopCache::AddItem` toplam mağaza değerinin `GOLD_MAX` sınırını aşmasını denetlemiyor; `RemoveOfferOnShop` `offer->itemid` yerine `offer->offerid` geçmeli; `COfferCache::CancelOffer` `end()` iteratörünü dereference ediyor; `CAuctionCache::PutsAuction` `auction.owner` anahtarı kullanmalı; `CreateOfferCacheUpdateNotifiedQuery` `WHERE id = {}` yazmalı. | Tüm fiyat, toplam mağaza değeri (`GOLD_MAX`), `OpenOfflineShop` ön-doğrulama, `offerid`, `CancelOffer` iteratör, `auction.owner` ve `WHERE id = {}` düzeltmeleri uygulandı. | `test_db_cache` (`129/129 PASS`, Senaryo I & J.3) | `374a3f41` | Yok |
| `G1.3` | `P0` | `VERIFIED` | `server/src/game/src/guild.cpp:1729-1821`, `server/src/db/src/GuildManager.cpp:784-840`, `server/src/game/src/questlua_guild.cpp:620-622`, `server/src/game/src/input_main.cpp:2828` | `CGuild::RequestWithdrawMoney` / `RecvWithdrawMoneyGive` oyuncunun mevcut parası ile çekilen miktarın `GOLD_MAX` sınırını aşmasını kontrol etmiyor; `RequestDepositMoney` / `CGuildManager::DepositMoney` `GOLD_MAX` üst sınırını denetlemiyor; `questlua_guild.cpp` null/sınır kontrolü eksik; `input_main.cpp:2828` `sizeof(int)` yerine `sizeof(YANG)` kullanmalı. | Oyuncu ve lonca kasası için `GOLD_MAX` taşma korumaları (`RecvWithdrawMoneyGive` reddiyle DB iadesi dahil) ve `sizeof(YANG)` düzeltmesi uygulandı. | `test_db_cache` (`129/129 PASS`, Senaryo J.2) | `374a3f41` | Yok |
| `G1.4` | `P1` | `VERIFIED` | `server/src/game/src/cube.cpp:350, 376, 409, 552, 608`, `server/src/game/src/dragon_soul_table.h:32-34`, `server/src/game/src/dragon_soul_table.cpp:358-794`, `server/src/game/src/DragonSoul.cpp:528-897` | `Cube_load` `gold` değerini 32-bit `int value1` içine okuyor ve `GOLD_MAX` sınırını denetlemiyor; `DragonSoulTable` ve `DSManager` arıtma ücreti (`fee`) için 32-bit `int&` / `int` kullanıyor ve `DoRefineStrength:885` yanlış `pItem` tanımlayıcısına başvuruyor. | `Cube_load`, `FN_check_cube_data`, `Cube_make` ve `DragonSoul` `fee` 64-bit `YANG` türüne yükseltildi; `GOLD_MAX` kontrolleri eklendi. | `test_db_cache` (`135/135 PASS`, Senaryo K.1), `ctest 10/10 PASS` | `374a3f41` | Yok |
| `G1.5` | `P1` | `VERIFIED` | `server/src/game/src/questlua_pc.cpp:326, 1025-1050` | `pc_change_money` `lua_tonumber(L, -1)` ile çok argümanlı çağrılarda yanlış argümanı okuyor ve `gold + ch->GetGold() < 0` işaretli taşma UB içeriyor; `pc_give_gold` null `ch` ve `GOLD_MAX` kontrolü eksik. | `lua_tonumber(L, 1)`, `LLONG_MIN` / `GOLD_MAX` taşma-güvenli alt/üst sınır kontrolleri ve `pc_give_gold` korumaları uygulandı. | `test_db_cache` (`135/135 PASS`, Senaryo J.4), `game_r41023.exe --check-lua` (`11/11 PASS`) | `374a3f41` | Yok |
| `G1.6` | `P1` | `VERIFIED` | `server/src/game/src/shop.cpp:175, 276`, `server/src/game/src/shopEx.cpp:157-171`, `server/src/game/src/special_shop_manager.cpp:432-441`, `server/src/game/src/char.cpp:3284`, `server/src/game/src/char_battle.cpp:988-1120`, `server/src/game/src/char_item.cpp:1006, 3636-3645`, `server/src/game/src/refine.cpp:16`, `server/src/game/src/cmd_gm.cpp:3239, 3289, 4055`, `server/src/game/src/guild_manager.h:13` | `CShop::Buy` / `CShopEx::Buy` 3x imparatorluk fiyat çarpımında taşma ve `g_bEmpireShopPriceTripleDisable` uyumsuzluğu; `do_build` içinde `-t->dwPrice` (`DWORD`) işaretsiz negasyonunun `+4,29 milyar` Yang üretmesi; `ITEM_ELK_VNUM` `GOLD_MAX` taşması; `GetPoint(POINT_GOLD)` `0` dönmesi; `RewardGold` 32-bit çarpım taşması; `char_item.cpp:1006` çift `SendMoneyLog`; `mapBet` 32-bit `DWORD` türü. | Tüm 64-bit `YANG` genişletmeleri, `-static_cast<YANG>(t->dwPrice)` düzeltmesi, `ITEM_ELK_VNUM` `GOLD_MAX` koruması ve çift log temizliği uygulandı. | `test_db_cache` (`135/135 PASS`, Senaryo J.5), `ctest 10/10 PASS` | `374a3f41` | Yok |

### Grup B — `G2` & `G4`: Protokoller, LP64/LLP64 (`building.h`, `affect.h`), Paket Çerçeveleme ve Kimlik Zincirleri

| ID | Priority | Status | Source Files | Root Cause | Fix Applied | Test Evidence | Commit SHA | Remaining Risk |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `G4.1` | `P0` | `VERIFIED` | `server/src/common/building.h:11-60` | `TLand`, `TObjectMaterial`, `TObjectProto`, `TObject` yapılarında `#pragma pack(1)` eksik ve 16 adet ham `long` alanı bulunuyor (Windows LLP64 ile FreeBSD LP64 arasında ikili paket boyutu farkı). | `#pragma pack(1)` eklendi ve 16 `long` alanı sabit genişlikli `int32_t` olarak sabitlendi (`41 B`, `8 B`, `100 B`, `40 B`). | `test_packet_wire` (`131/131 PASS`, Case F.1) | `625e5284` | Yok |
| `G4.2` | `P0` | `VERIFIED` | `server/src/db/src/LoginData.cpp:5-23` | `CLoginData` kurucusu `m_aiPremiumTimes` dizisini sıfırlamıyor; başlatılmamış bellek `TPacketGDAuthLogin` üzerinden GAME sunucusuna gidebiliyor. | `memset(m_aiPremiumTimes, 0, sizeof(m_aiPremiumTimes));` eklendi. | `test_packet_wire` (`131/131 PASS`, Case G.3), `game_r41023.exe --check-lua` (`11/11 PASS`, Check 4) | `625e5284` | Yok |
| `G2.1` | `P0` | `VERIFIED` | `server/src/game/src/item_manager.cpp:259-262, 540-549`, `server/src/game/src/char_manager.h:125`, `server/src/game/src/char_manager.cpp:80-110` | `ITEM_MANAGER::DestroyItem` içinde `Find(GetPlayerID())` (`VID` araması) çağrıldığı için sahibin envanter slotu temizlenmiyor (sallanan işaretçi / UAF); `AllocVID()` (`int m_iVIDCount`) ve `ITEM_MANAGER::CreateItem` VID sayacı taşmasında `0` değerini ve yaşayan VID çakışmasını kontrol etmiyor. | `FindByPID(item->GetOwner()->GetPlayerID())` düzeltmesi, `DWORD m_iVIDCount` ve `0` + aktif `VID` çakışma kontrolü uygulandı. | `test_packet_wire` (`131/131 PASS`, Case F.3) | `625e5284` | Yok |
| `G2.2` | `P1` | `VERIFIED` | `server/src/db/src/ItemIDRangeManager.cpp:50-159`, `server/src/db/src/Peer.cpp:42-54, 184-201`, `server/src/db/src/ClientManager.cpp:3611-3614` | `CPeer::CheckItemIDRangeCollision` ters boolean döndürüyor ve `for_each_peer` functor kopyası yakalanmıyor; `CPeer::OnClose` `m_itemSpareRange` aralığını geri vermiyor; `BuildRange` önbellekteki henüz SQL'e yazılmamış `item_id` değerlerini kontrol etmiyor; `GainItemID` aralık tükenmesini denetlemiyor. | `CheckItemIDRangeCollision` polaritesi, `f = for_each_peer(f)`, `OnClose` yedek aralık iadesi, `GetMaxCachedItemIDInRange` ve `GainItemID` tükenme koruması uygulandı. | `test_packet_wire` (`131/131 PASS`, Case F.4) | `625e5284` | Yok |
| `G2.3` | `P1` | `VERIFIED` | `server/src/db/src/ItemAwardManager.cpp:110-124`, `server/src/db/src/ClientManagerPlayer.cpp:472-499` | `ItemAwardManager::Load` ve `CClientManager::ItemAward` / `GetCommand` `ITEM_AWARD_WHY_MAX_LEN` (128 bayt) olan `why` alanını 100 baytlık `cmdStr` ve 20 baytlık `command` yığın tamponuna sınırsız kopyalıyor (stack buffer overflow). | `cmdStr[ITEM_AWARD_WHY_MAX_LEN + 1]` ve `strlcpy` sınır korumaları uygulandı. | `test_db_cache` (`135/135 PASS`, Senaryo K.3), `ctest 10/10 PASS` | `625e5284` | Yok |
| `G2.4` | `P0` | `VERIFIED` | `server/src/common/{tables.h,building.h,PulseManager.h}`, `server/src/game/src/{packet.h,p2p.h,*.cpp}`, `server/src/db/src/*.cpp`, `server/src/libsql/AsyncSQL.h` | Kalıcı kimliklerin (`AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`) C++ yapı/paket/konteyner türlerinde 32-bit `DWORD`/`uint32_t` ve Lua API'da `lua_Number` (`double`, $>2^{53}$ duyarlılık kaybı) olarak kalması; `CGuild::CGuild` (`guild.cpp:103`) içinde `uiInsertID` kullanımı. | Tüm kalıcı kimlikler `uint64_t` yapıldı; `CItemIDRangeManager` / `TItemIDRangeTable` 64-bit aralıklara taşındı; Quest/Lua 5.5.1 kimlik fonksiyonları (`pc_get_player_id`, `pc_get_account_id`, `item_get_id`, `item_select`, `guild_*`, `marriage_*`, `monarch_*`) `lua_pushinteger`/`lua_tointeger` (`lua_Integer`) ile $>2^{53}$ ve $\ge 2^{63}$ tam duyarlılığa geçirildi; `TPacketGCGuildName` (`26 B`), `TSubPacketGCCaptchaGMWho` (`23 B`), autopotion `lSPCost`, `UseSilkBotary` (`HEADER_GD_MYSHOP_PRICELIST_REQ` `8 B`), `CPVP`, `OXEvent`, `threeway_war`, `name_reservation`, `special_shop_manager`, `mining`, `fishing` ve `m_dwQuestNPCVID` 64-bit'e yükseltildi. | `test_packet_wire` (`131/131 PASS`, Case F.5 & G.1–G.5), `test_db_cache` (`135/135 PASS`, Case K.2 & K.4), `test_db_cache_integration` (`187/187 PASS`, Senaryo 13), `test_lua55_quest` (`11/11 PASS`, Test 11) | `G2-G5-G6-G8` | Yok |
| `G4.3` | `P1` | `VERIFIED` | `server/src/game/src/input_db.cpp:3127-3155`, `server/src/game/src/input_p2p.cpp:562-595`, `server/src/game/src/input.cpp:147-155`, `server/src/db/src/ClientManager.cpp:2588, 2784` | `CInputDB::Process` işaretli `iSize` kullanıyor (`dwSize >= 2^31` olduğunda tampon işaretçisi geriye sarıyor); `CInputP2P::BlockMessenger` tampon sınırlarını kontrol etmiyor; `CInputProcessor::Process` `Analyze` dönüşünü kalan tamponla doğrulamıyor; `CClientManager` değişken uzunluklu paketlerde (`HEADER_GD_SETUP`, `HEADER_GD_MYSHOP_PRICELIST_UPDATE`) sınır kontrolü yapmıyor. | `DWORD dwSize` + 64-bit `totalNeeded`, `BlockMessenger` sınır/NUL kontrolleri, `iExtraPacketSize <= m_iBufferLeft - iPacketLen` ve `ClientManager` değişken boyut kontrolleri uygulandı. | `test_packet_wire` (`131/131 PASS`, Case F.2) | `625e5284` | Yok |
| `G4.4` | `P1` | `VERIFIED` | `server/src/game/src/affect.h:11-13`, `server/src/game/src/char.h:889-891`, `server/src/game/src/char_affect.cpp`, `server/src/game/src/questlua_affect.cpp`, `server/src/game/src/cmd_gm.cpp:4848-4867` | `CAffect::lDuration` ve `lSPCost` alanları `long` olarak kalmış (`TPacketAffectElement` `int64_t` iken LLP64'te 32-bit daraltma); `do_set_special_flag` / `do_get_special_flag` 64-bit `llValue` değerini 32-bit `int`'e daraltıyor. | `CAffect::lDuration`/`lSPCost`, `AddAffect`, `questlua_affect` ve `do_set_special_flag`/`do_get_special_flag` 64-bit `int64_t` yapıldı; istemci-sunucu paket sözleşmesi (§10) güncellendi. | `test_packet_wire` (`131/131 PASS`, Case G.2 & G.4), `game_r41023.exe --check-lua` (`11/11 PASS`, Check 4) | `625e5284` | Yok |
| `G4.5` | `P0` | `VERIFIED` | `server/src/game/src/desc.h`, `server/src/game/src/desc.cpp`, `server/src/game/src/input.cpp`, `server/src/game/src/input_auth.cpp`, `server/src/game/src/input_login.cpp`, `server/src/game/src/input_p2p.cpp`, `tests/test_server_smoke_client.cpp` | `CInputProcessor::Process` içinde `52`, `130`, `136`, `321` bayt uzunluklarında anında ret yapılması, TCP segmentasyonu nedeniyle ilk parçası bu uzunluklarda ulaşan meşru x64 `Login3` (`328 B`) veya `Login2` (`56 B`) istemcilerini yanlışlıkla reddediyor; ayrıca `ClearInputBuffers()` sonrasında `DESC::ProcessInput` içinde boş tamponda `buffer_read_proceed` çağrılması `syserr` üretiyor. | Uzunluğa dayalı erken ret kaldırıldı; `DESC` başına `250 ms` boşta yarım çerçeve zamanlayıcısı (`partial_login_event`), `DESC::ProcessInput` içinde paketler arası şifreleme/anahtar geçişi, `ClearInputBuffers()` sonrası `MIN(iBytesProceed, buffer_size(m_lpInputBuffer))` koruması ve `STargetDebuff::duration` `int64_t` sabit genişlik düzeltmesi uygulandı. | `test_server_smoke_client.exe --protocol-framing-audit` (`10/10 PASS`, `0 syserr`) | `114b2d57` / `G2-G5-G6-G8` | Yok |

### Grup C — `G3`, `G5` & `G6`: ItemShop İşlem Bütünlüğü, MariaDB 12.3 LTS 64-Bit Şema/Migration ve Derleyici Uyarı Temizliği

| ID | Priority | Status | Source Files | Root Cause | Fix Applied | Test Evidence | Commit SHA | Remaining Risk |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `G3.1` | `P0` | `VERIFIED` | `server/src/game/src/itemshop_manager.cpp:990-1395`, `sql/account.sql`, `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql` | `itemshop_tx_journal.state` enum'unda `'RECOVERING'` durumu eksik olduğu için kurtarma sırasında sunucu tekrar çökerse satır kurtarılamıyor veya kısmi eşya silme işlemi atomik sahiplenme öncesinde çalışıyor. | `state` enum'una `'RECOVERING'` eklendi (`information_schema.COLUMNS` kontrolüyle `mt2leastpriv` altında gereksiz `ALTER` engellendi); kısmi eşya silme işleminden önce atomik `UPDATE ... SET state='RECOVERING' WHERE state='CHARGED'` sahiplenmesi zorunlu kılındı. | `game_r41023.exe --test-itemshop` (`15/15 PASS`, Check 15), `test_live_db` (`354/354 PASS`) | `6c6732fd` | Yok |
| `G3.2` | `P0` | `VERIFIED` | `server/src/game/src/itemshop_manager.cpp:1161-1305, 1960-2005` | Envanter + `item_award` arasında bölünmüş teslimatta (`INV_PENDING` + `AWARD`) kurtarma mantığı iki tabloyu ayrı ayrı kontrol ettiği için tam teslimatı kısmi sanıp silip iade edebiliyor; VIP teslimatı (`AddVIP`) ile `DELIVERED` güncellemesi ayrı sorgularda çalışıyor; `FlushDelayedSave` sonrasında `HEADER_GD_ITEM_FLUSH` gönderilmiyor. | Bölünmüş teslimat için birleşik `invSumCount + awardSumCount >= tx.itemCount` (`RECOVERED_SPLIT_DELIVERY`) kontrolü, VIP + `DELIVERED` için tek `AccountDB` transaction (`START TRANSACTION`..`COMMIT`) ve `HEADER_GD_ITEM_FLUSH` eklendi. | `game_r41023.exe --test-itemshop` (`15/15 PASS`, Check 15) | `6c6732fd` | Yok |
| `G3.3` | `P1` | `VERIFIED` | `server/src/game/src/itemshop_manager.cpp:56-79, 838, 1985-1998`, `server/src/db/src/ClientManager.cpp:4760-4845` | `Initialize()` içinde `RecoverIncompleteTransactions()` açık artırma sayaçları yüklenmeden önce çalışmalı; `BuyItem` açık artırma ilerlemesini yüzde değişmese bile her satışta P2P/istemciye yaymalı; `UseCodeVoucher` çift kaçışlama ve yarış koşulu içeriyor. | `Initialize()` sırası düzeltildi; `if (current_progress != new_progress)` kapısı kaldırıldı; `UseCodeVoucher` tekli `EscapeString`, atomik `WHERE id=%u AND use_count < max_use_count` ve senkron `SQL_ACCOUNT` bakiye yüklemesi + hata anında kupon geri alma (rollback) ile sertleştirildi. | `game_r41023.exe --test-itemshop` (`15/15 PASS`) | `6c6732fd` | Yok |
| `G3.4` | `P0` | `VERIFIED` | `server/src/game/src/itemshop_manager.h`, `server/src/game/src/itemshop_manager.cpp`, `server/src/game/src/questlua_pc.cpp`, `server/src/game/src/main.cpp`, `server/src/game/src/questlua_test.cpp`, `sql/account.sql`, `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql` | `A–D`: 1sn bellek içi `PulseManager` filtresi kalıcı idempotency sağlamıyor ve bilinçli ardışık alımları engelliyor; `E`: yığınlanamayan çoklu eşyada `idx_item_award_why` çakışması; `F,G,L`: `GiveItem` (`AutoGiveItem`) yalnızca asenkron `SaveSingleItem` kuyruğuna ekliyor; `H,I`: `'RECOVERING'` durumunda ikinci süreç çökmesinde sahiplenme ve çoklu süreç yarış kilidi. | `itemshop_tx_journal.req_id` + `UNIQUE KEY idx_itemshop_tx_aid_req(aid, req_id)` ve `LookupTransactionByRequestId` eklendi (`PulseManager` satın alma filtresi kaldırıldı); çoklu `item_award` satırlarına `#1`, `#2` soneki verildi; `PersistInventoryItemSynchronously` ile `DELIVERED` öncesi senkron `player.item` yazımı sağlandı; `GET_LOCK('mt2_is_tx_<tx_id>', 0)` oturum kilidi ile `'RECOVERING'` ikinci çökme (`_Exit(97)`) ve 2 süreç yarışı (`--test-itemshop-recover-child`) güvenceye alındı. | `game_r41023.exe --test-itemshop` (`15/15 PASS`, Checks 9–13 & 15) | `114b2d57` | Yok |
| `G5.1` | `P0` | `VERIFIED` | `server/src/game/src/db.cpp:750-764` | `AccountDB::Process()` içinde `delete pMsg;` `while ((pMsg = PopResult()))` döngüsünün dışında kaldığı için aynı turda birden fazla sonuç döndüğünde yalnızca son `SQLMsg*` siliniyor ve öncekiler sızıyor. | `delete pMsg;` `while` döngüsünün içine alındı (`M2_DELETE(qi)` `AnalyzeReturnQuery` içinde korundu). | `ctest 10/10 PASS`, `--test-itemshop 15/15 PASS` | `6c6732fd` | Yok |
| `G5.2` | `P0` | `VERIFIED` | `server/src/game/src/log.cpp:470-494`, `server/src/game/src/Hardware.cpp:69-94`, `server/src/db/src/ClientManagerHorseName.cpp:5-38` | `LogManager::ChatLog`, `Hardware::Block`/`Unblock` (`Unblock` ayrıca yanlışlıkla `DBManager` yerine `AccountQuery` kullanmalı) ve `UpdateHorseName` içinde `EscapeString` eksikliği. | Tüm metin parametrelerine `EscapeString` uygulandı; `Hardware::Unblock` `AccountQuery` olarak düzeltildi; `AckHorseName` null sonuç koruması eklendi. | `ctest 10/10 PASS`, `test_db_cache_integration 187/187 PASS` | `6c6732fd` | Yok |
| `G5.3` | `P1` | `VERIFIED` | `server/src/db/src/ClientManager.cpp:1195`, `server/src/db/src/Monarch.cpp:270`, `server/src/db/src/GuildManager.cpp:333`, `server/src/db/src/ClientManagerPlayer.cpp:1194`, `server/src/db/src/Marriage.h:51`, `server/src/db/src/Marriage.cpp:58, 92`, `sql/migrations/rev52_log_ikarusshop_itemshop_tables.sql:48` | `RESULT_SAFEBOX_CHANGE_SIZE` `UPDATE` sorgusunda `uiNumRows > 0` beklediği için yanıt göndermiyor; `DelMonarch` `DELETE` sonrasında `mysql_fetch_row` çağırıyor; `GuildManager.cpp:333` çağrılmamış geçici `FSendGuildWarScore(peer)` oluşturuyor; `__RESULT_PLAYER_DELETE` `pi` sızdırıyor; `Marriage::TMarriage::time` 32-bit `DWORD` kalmış; `rev52` `itemshop.money_before_buy` `int(11)` kalmış. | `uiAffectedRows > 0` / `== 0` kontrolleri, `FSendGuildWarScore(peer)(peer)`, `delete pi;`, `Marriage` 64-bit `int64_t time` ve `rev52` `bigint(20)` düzeltmeleri uygulandı. | `test_live_db` (`354/354 PASS`), `test_db_cache_integration` (`187/187 PASS`) | `6c6732fd` | Yok |
| `G5.4` | `P0` | `VERIFIED` | `sql/{account,common,log,player}.sql`, `sql/migrations/{20261027_x64_protocol_and_itemshop_journal,rev51_halloween_hair_attr_migration,rev52_log_ikarusshop_itemshop_tables}.sql` | Kalıcı kimlik sütunlarının (`AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`) SQL şema ve migration dosyalarında 32-bit `int(10/11) unsigned` kalması; `log.*` tablolarında `> 2^{32}` `AID`/`PID` ile `errno 1264` (`Out of range value`) oluşması. | `account`, `common`, `log` ve `player` şemalarındaki tüm kalıcı kimlik sütunları ve `AUTO_INCREMENT` birincil anahtarları `bigint(20) unsigned` yapıldı; C++ sorgu biçimlendiricileri (`%llu`/`PRIu64`) ve `strtoull` ayrıştırıcıları 64-bit ile hizalandı. | `test_live_db` (`354/354 PASS`, Suite 16b), canlı `ch11` `AID=5.000.000.001` / `PID=5.000.000.001` (`0 syserr`) | `G2-G5-G6-G8` | Yok |
| `G6.1` | `P1` | `VERIFIED` | `server/src/db/src/*.cpp`, `server/src/game/src/*.cpp`, `tests/*.cpp` | 64-bit kimlik dönüşümü sonrasında 194 derleme hedefinde oluşabilecek `printf`/`snprintf` format (`-Wformat`), işaretli/işaretsiz karşılaştırma (`-Wsign-compare`) ve daraltma uyarıları. | Tüm `db`, `game` ve `tests` kaynaklarında format belirteçleri ve tür dönüşümleri kök nedeninden düzeltilerek 194/194 hedefte `-Wall -Wextra` altında `0` hata ve `0` uyarı sağlandı; `tests/scan_compiler_warnings.py` ile tüm 285 TU üzerinde yüksek riskli (`-Wformat=2`, `-Wshadow`, `-Wcast-align`, `-Wpointer-arith`, `-Wstrict-aliasing`, `-Wundef`) uyarıların `0` olduğu doğrulandı. | `tests/run_limited.ps1 -Build` (`0 errors, 0 warnings`), `scan_compiler_warnings.py` (`0 high-risk warnings`) | `G2-G5-G6-G8` | Yok |

---

## 3. `G8` — İkinci Uçtan Uca Denetim Özeti (12 Kontrol Noktası)

1. **İlk taramada bulunmayan yeni sorunlar:** Canlı `AID = 5.000.000.001` / `PID = 5.000.000.001` (`> 2^{32}`) serverfiles duman testinde yakalanan 3 somut kusur (`guild.cpp:103` `uiInsertID -> ullInsertID`, `desc.cpp` `ClearInputBuffers()` sonrası boş tamponda `buffer_read_proceed` çağrısı ve `20261027_x64_protocol_and_itemshop_journal.sql` içinde mevcut `log.*` tablolarının `ALTER TABLE` ifadeleri) ile `M-02`/`M-09` taramasında yakalanan `UseSilkBotary` (`HEADER_GD_MYSHOP_PRICELIST_REQ` 4-bayt vs 8-bayt `uint64_t` uyumsuzluğu), autopotion `dwFlag = item->GetID()` (`char_item.cpp:3889, 3932`), `TPacketGCGuildName` (`26 B`), `TSubPacketGCCaptchaGMWho` (`23 B`), `CPVP`, `OXEvent`, `threeway_war`, `name_reservation`, `special_shop_manager`, `mining`, `fishing`, `m_dwQuestNPCVID` (`npc.lock()`) ve `time_t` daralmaları kök nedeninden düzeltilip canlı ortamda `0 syserr` ile doğrulandı.
2. **Modüller arası yan etki kontrolü:** `libpoly`, `libthecore`, `libgame`, `libsql`, `db`, `game`, `qc` ve tüm test hedeflerinin tamamı (`194/194`) `0` hata ve `0` uyarıyla derlendi; canlı `db`/`auth`/`ch11` çalışma zamanı `syserr.txt` dosyalarında `0` hata doğrulandı.
3. **Fonksiyon çağrı noktaları:** Değiştirilen tüm 64-bit kimlik (`uint64_t`), ekonomi (`YANG`), `ItemShop`, `IkarusShop`, `Guild`, `Marriage`, `Monarch`, `ItemIDRangeManager` ve `Quest/Lua` fonksiyonlarının bildirim ve çağrı noktaları birebir uyumludur.
4. **Paket üretici/tüketici uyumu:** `tables.h`, `packet.h`, `p2p.h` ve `building.h` içindeki tüm 64-bit kimlik ve süre alanları (`TPacketGCLoginSuccess = 385 B`, `TPacketGCCharacterCreateSuccess = 73 B`, `TPacketGCCharacterUpdate = 48 B`, `TPacketGCCharacterAdditionalInfo = 66 B`, `TPacketGCQuestConfirm = 78 B`, `TPacketGCGuildName = 26 B`, `TSubPacketGCCaptchaGMWho = 23 B`, `TPacketGCTargetCreate = 43 B`, `TLand = 41 B`, `TItemIDRangeTable = 25 B`) hem `test_packet_wire` (`131/131 PASS`) hem canlı `db_r41023.exe <-> game_r41023.exe` ve `test_server_smoke_client.exe --protocol-framing-audit` (`10/10 PASS`) ile doğrulandı.
5. **C++ ve SQL veri tipi uyumu:** `account`, `common`, `log`, `player` ve `world` şemalarındaki tüm `bigint(20) unsigned` / `bigint(20)` sütunlar ve C++ format belirteçleri (`%llu`/`%lld`) birebir eşleşmektedir (`test_live_db 354/354 PASS`).
6. **Kimlik ve x64 alan envanteri:** Tüm kalıcı kimlikler (`AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`, `TX_ID`, `GUILD_TOKEN`) uçtan uca 64-bit `uint64_t` / `BIGINT UNSIGNED` kapasiteye taşınmış; oturum içi geçici `VID` (`uint32_t`) ise `0` ve yaşayan varlık çakışma korumasıyla güvenceye alınmıştır.
7. **Test beklentileri:** Tüm birim, entegrasyon ve canlı testler `GOLD_MAX = 10^{12}`, `> 2^{32}`, `> 2^{53}` ve $\ge 2^{63}$ kalıcı kimlikler, `0x20090064` x64 protokolü ve `'RECOVERING'` + `req_id` işlem günlüğü sözleşmesini doğrulamaktadır.
8. **Uyarı bastırma yasağı:** Hiçbir yerde `-Wno-*`, `-w` veya `#pragma diagnostic ignored` kullanılmamıştır (`0` uyarı).
9. **Regresyon kontrolü:** `CTest 10/10`, `Packet Wire 131/131`, `DB Cache 139/139`, `DB Cache Integration 162/162` (çevrimdışı) / `187/187` (canlı MariaDB), `Live MariaDB 354/354`, `ItemShop 15/15`, `Protocol Framing 10/10 (4/4 kategori)`, `Lua 11/11` ve `Quest Matrix 202/202` sıfır gerilemeyle geçti.
10. **Belge senkronizasyonu:** `AGENTS.md`, `PROJECT_STATE.md` ve `docs/SERVER_X64_GOALS.md` günceldir.
11. **Güvenlik ve veri bütünlüğü:** Tüm SQL kaçışlamaları (`EscapeString`), paket sınır kontrolleri ve `ItemShop` çökme/eşzamanlılık kurtarma garantileri `fail-closed` olarak doğrulanmıştır.
12. **Unutulmuş/ölü kod yolları:** `guild.cpp:103` (`ullInsertID`), `desc.cpp:501-522` (`buffer_read_proceed`), `GuildManager.cpp:333` (`FSendGuildWarScore`), `ClientManager.cpp:1195` (`RESULT_SAFEBOX_CHANGE_SIZE`) ve `UseSilkBotary` (`HEADER_GD_MYSHOP_PRICELIST_REQ`) onarıldı.

---

## 4. Kalıcı Hedef Eşleme Tablosu (Tarihsel Referans — Sunucu `M-01`..`M-10`, `M-12`, `INT-01` Tamamı `VERIFIED`)

| Ortak Madde | Kaynak Hedef Kodları | Teknik Kapsam | İlgili Dosya ve Fonksiyonlar | Eski Çalışma Kanıtı | Güncel Durum | Eksik / Tamamlanan Kabul Kriterleri | Bağımlılıklar | Risk Seviyesi | Öncelik | Doğrulama Yöntemi |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`M-01`** | `CONTEXT-H1`, `HANDOFF-H1`, `HANDOFF-H2` | Kademe C canlı test betiklerinin oturum/hesap bağımsız `tests/` altyapısına taşınması, `serverfiles.env` eski UUID yolunun onarılması ve `CAsyncSQL` dayanıklı spool (`test_live_db.cpp:1125` Senaryo 4) yarış koşulunun kök nedeninden çözülmesi | `server/src/libsql/AsyncSQL.{h,cpp}` (`ReplayDurableSpoolNow`, `WaitForDurableQueueIdle`, `FlushAndSpoolOnShutdown`, `m_bDurableQueryInFlight`), `tests/test_live_db.cpp`, `tests/run_kademe_c_live.ps1`, `tests/scan_compiler_warnings.py`, `tests/run_live_tests.ps1`, `tests/run_db_integration_tests.ps1` | Eski oturumda `scratch` altında geçici betikler vardı; devir sırasında `test_live_db.cpp:1125` üzerinde 1 kez geçici spool yarış hatası gözlenmişti | **`VERIFIED`** | `m_bDurableQueryInFlight` ve `WaitForDurableQueueIdle` ile yarış koşulu kapatıldı; ardışık `20/20` (`toplam 40/40`) `test_live_db.exe` (`354/354 PASS`) ve 6 aşamalı `tests/run_kademe_c_live.ps1 -IncludeSecurityAndBackup` (`Exit Code 0`) kanıtlandı (`7ee808b7`) | Yok | Yüksek | `P0` | `test_live_db.exe` `20/20` ardışık koşu (`354/354 PASS`) + `tests/run_kademe_c_live.ps1 -IncludeSecurityAndBackup` (`6/6 PASS`) |
| **`M-02`** | `CONTEXT-H4`, `GOALS-G2.4` | Kalan 6 adet `64 -> 32-bit` kimlik daraltma noktasının (`char.cpp:7125, 7791, 9652`, `char_item.cpp:3889, 3932`, `priv_manager.cpp:121`) ve ikinci dalga 64-bit kimlik/zaman zincirlerinin (`UseSilkBotary` `HEADER_GD_MYSHOP_PRICELIST_REQ`, `CPVP`, `OXEvent`, `SCharDeadEventInfo`, `char_resist`, `threeway_war`, `name_reservation`, `special_shop_manager`, `mining`, `fishing`, `m_dwQuestNPCVID`) kalıcı olarak düzeltilmesi | `server/src/game/src/{char.h,char.cpp,char_item.cpp,priv_manager.cpp,packet.h,pvp.h,pvp.cpp,guild_manager.h,guild_manager.cpp,OXEvent.h,OXEvent.cpp,char_battle.cpp,char_resist.cpp,cmd_emotion.cpp,threeway_war.h,threeway_war.cpp,name_reservation.h,name_reservation.cpp,special_shop_manager.h,special_shop_manager.cpp,item_attribute.cpp,mining.cpp,fishing.h}`, `server/src/db/src/ClientManager.cpp` | `G2.4` kapsamında ana yapı/SQL dönüşümü yapıldı; 6 çağrı noktası ve alt sistem konteynerlerinde `DWORD` daraltması kalmıştı | **`VERIFIED`** | `TPacketGCQuestConfirm::requestPID` (`uint64_t`), `TPacketGCGuildName::guildID` (`26 B`), `TSubPacketGCCaptchaGMWho::pid` (`23 B`), autopotion `dwFlag = AFF_NONE` vs `lSPCost = item->GetID()`, `GiveGuildPriv` `CharLog` 64-bit `guild_id`, `UseSilkBotary` `8-bayt` `uint64_t` ve tüm alt sistem 64-bit PID/AID/ItemID/time_t düzeltmeleri tamamlandı | `M-01` | Yüksek | `P1` | `tests/run_limited.ps1 -Build`, `test_packet_wire` (`131/131 PASS`), `test_db_cache` (`139/139 PASS`), `game_r41023.exe --check-lua` (`11/11 PASS`) |
| **`M-03`** | `GOALS-G3.1..G3.4` | ItemShop işlem günlüğü (`account.itemshop_tx_journal` şema/belge uyumu), `req_id` idempotency, `item_award`, bölünmüş teslimat ve çökme kurtarması denetimi | `server/src/game/src/itemshop_manager.{h,cpp}`, `sql/account.sql`, `sql/migrations/20261027_x64_protocol_and_itemshop_journal.sql` | `game_r41023.exe --test-itemshop` (`15/15 PASS`) | **`VERIFIED`** | `account.itemshop_tx_journal` ve `account.itemshop_purchase_idempotency` şema/kod/belge tutarlılığı ve `15/15` canlı kurtarma/idempotency kontrolü doğrulandı | `M-01` | Yüksek | `P1` | `game_r41023.exe --test-itemshop` (`15/15 PASS`), `test_live_db` (`354/354 PASS`) |
| **`M-04`** | `HANDOFF-H3`, `GOALS-G1.2` | IkarusShop, Safebox, Shop, Auction, Offer, `TShopItem.id` (`owner_id` vs `item_id`), `TSafeboxItemInfo` ve `shop_auto_increment` vs `> 2^53` `item.id` veri bütünlüğü | `server/src/game/src/ikarus_shop*.cpp`, `server/src/db/src/IkarusShopCache.{h,cpp}`, `server/src/db/src/ClientManagerIkarusShop.cpp`, `sql/player.sql`, `tests/test_db_cache.cpp` | `test_db_cache` (`129/129 PASS`), `test_db_cache_integration` (`187/187 PASS`) | **`VERIFIED`** | `TShopItem`, `TSafeboxItemInfo`, `TAuctionInfo`, `TOfferInfo` ve `> 2^53` `item.id` / `owner_id` ayrımları `test_db_cache` Senaryo K (`139/139 PASS`) ve `test_db_cache_integration` (`187/187 PASS`) ile doğrulandı | `M-01` | Yüksek | `P1` | `test_db_cache` (`139/139 PASS`, Case K), `test_db_cache_integration` (`187/187 PASS`) |
| **`M-05`** | `HANDOFF-H4`, `GOALS-G1.1, G1.3, G1.4, G1.6` | Guild (`RequestDepositMoney`/`WithdrawMoney`), `WarBet`, `Monarch`, Cube, DragonSoul, Exchange ve `GOLD_MAX = 1.000.000.000.000` ($10^{12}$) sınırları | `server/src/game/src/{guild,cube,DragonSoul,dragon_soul_table,exchange,shop,shopEx,char_item}.cpp`, `server/src/db/src/{GuildManager,Monarch}.cpp`, `tests/test_db_cache.cpp` | `test_db_cache` (`129/129 PASS`) | **`VERIFIED`** | `Cube` / `DragonSoul` 64-bit ücret sınırları (`G1.4`, Senaryo L) dahil tüm ekonomi alt sistemlerinin doğrudan davranışsal regresyon testleriyle kanıtlanması tamamlandı | `M-01` | Orta | `P1` | `test_db_cache` (`139/139 PASS`, Case J & L), `ctest 10/10` |
| **`M-06`** | `CONTEXT-H5`, `HANDOFF-H5`, `GOALS-G1.5, G2.4` | Lua 5.5.1 `lua_Integer` (`int64_t`) işaretsiz 64-bit (`>= 2^63` / `> INT64_MAX` vs `2^53 < id <= INT64_MAX`) sınır politikası ve Quest/Lua para & kimlik testleri | `server/src/db/src/ItemIDRangeManager.h`, `server/src/game/src/questlua_{pc,item,guild,marriage,monarch,affect}.cpp`, `server/src/game/src/questlua_test.cpp`, `tests/test_lua55_quest.cpp` | `--check-lua` (`11/11 PASS`), `test_lua55_quest` (`11/11 PASS`) | **`VERIFIED`** | `CItemIDRangeManager::cs_dwMaxItemID` değeri `9.223.372.000.000.000.000` (`<= INT64_MAX`) olarak sınırlandırılarak tüm kalıcı kimliklerin `lua_Integer` içinde pozitif kalması ve `%d`/`%u` SQL biçimlendirmesinde negatif işaret üretmemesi garanti altına alındı; ayrıca `>= 2^63` ikinin tümleyeni bit-kalıbı dönüşümü `test_lua55_quest` (`11/11 PASS`) ve `--check-lua` (`11/11 PASS`) içinde doğrulandı | `M-02` | Orta | `P1` | `test_lua55_quest` (`11/11 PASS`), `game_r41023.exe --check-lua` (`11/11 PASS`), `run_quest_matrix.py` (`202/202 PASS`) |
| **`M-07`** | `CONTEXT-H7`, `REG-G1.4`, `REG-G2.3`, `REG-G4.2`, `REG-G4.4` | `G1.4` (`Cube`/`DragonSoul` 64-bit ücret), `G2.3` (`ItemAward` 128-bayt `why` taşma koruması), `G4.2` (`CLoginData::m_aiPremiumTimes` sıfırlama) ve `G4.4` (`CAffect::lDuration`/`lSPCost` `int64_t` & `do_set_special_flag`) için özel davranışsal regresyon testleri | `tests/test_packet_wire.cpp`, `tests/test_db_cache.cpp`, `server/src/game/src/questlua_test.cpp` | Yalnızca genel `ctest 10/10 PASS` ile doğrulanmıştı (`WEAK EVIDENCE`) | **`VERIFIED`** | Her 4 alt hedef (`G1.4` -> `test_db_cache` L, `G2.3` -> `test_db_cache` K, `G4.2` -> `test_packet_wire` G.3 & `questlua_test` Check 4, `G4.4` -> `test_packet_wire` G.2/G.4 & `questlua_test` Check 4) için doğrudan davranışsal regresyon senaryoları eklendi ve doğrulandı | `M-02` | Orta | `P2` | `test_packet_wire` (`131/131 PASS`), `test_db_cache` (`139/139 PASS`), `--check-lua` (`11/11 PASS`) |
| **`M-08`** | `HANDOFF-H6`, `HANDOFF-H7` | `LogManager` (`MoneyLog`, `ItemLog`, `CharLog`) 64-bit imzaları ve 2038 sonrası (`> 2^31`) `time_t` / etki süresi / zamanlayıcı denetimi | `server/src/game/src/log.{h,cpp}`, `server/src/game/src/char_affect.cpp`, `server/src/db/src/Marriage.{h,cpp}`, `server/src/game/src/{maintenance.h,ikarus_shop.h,char_player.h,char_manager.cpp,horse_rider.cpp,char_skill.cpp}` | `test_live_db` (`354/354 PASS`), `test_packet_wire` (`123/123 PASS`) | **`VERIFIED`** | `LogManager` 64-bit `PID`/`ITEM_ID`/`GUILD_ID`/`YANG` (`test_db_cache` K.4, `quest::_char_log` `m2_lua_touint64`) ve `> 2^31` (`2038+`) `time_t` / `CAffect` süreleri (`test_packet_wire` G.2/G.4) doğrulandı | `M-02` | Orta | `P2` | `test_packet_wire` (`131/131 PASS`), `test_db_cache` (`139/139 PASS`), `test_live_db` (`354/354 PASS`) |
| **`M-09`** | `CONTEXT-H6`, `GOALS-G6.1` | Genişletilmiş GCC uyarı taraması (`-Wformat=2`, `-Wshadow`, `-Wconversion`, `-Wsign-conversion`, `-Wundef` vb.) için depoda kalıcı tarama aracı ve birinci parti (`server/src/`) yüksek riskli uyarıların temizlenmesi | `tests/scan_compiler_warnings.py`, `server/src/db/src/*`, `server/src/game/src/*`, `server/src/libsql/*` | Eski oturumda geçici Python betiğiyle taranmıştı; depoda kalıcı betik yoktu | **`VERIFIED`** | `tests/scan_compiler_warnings.py` ile tüm 285 TU tarandı: `server/src/` içinde `0` `-Wformat`, `0` `-Wshadow`, `0` `-Wcast-align`, `0` `-Wpointer-arith`, `0` `-Wstrict-aliasing`, `0` `-Wundef`; ayrıca `domain_64_to_32_narrowing` filtresiyle tespit edilen gerçek 64->32 kimlik/zaman daralmalarının tamamı onarıldı | `M-02`, `M-07` | Orta | `P2` | `python tests/scan_compiler_warnings.py` (`285 TU`, `0` yüksek riskli uyarı), `tests/run_limited.ps1 -Build` (`0` uyarı) |
| **`INT-01`** | `Cross-System Integrity & Anti-Dupe` | Eşya sahipliği ve devir zincirleri (`Acce` `SItemPosEx::id`, `AutoStackItemProto`/`AutoStackItem`/`AutoGiveItem`, `MoveItem`/`SwapItem`, `CSafebox::ChangeSize`/`__Destroy`/`Save`, `FlushDelayedSaveItem` `DRAGON_SOUL_INVENTORY`, `DragonSoul` rafine kilit kontrolleri), `WonExchange` `int32` taşma koruması, `move_channel_info`/`leave_guild_war_event_info` `DynamicCharacterPtr` yaşam döngüsü, `GetQuestNPC` `VID`/`PID` sınırı ve `cs_dwMaxItemID <= INT64_MAX` garantisi | `server/src/common/length.h`, `server/src/db/src/ItemIDRangeManager.h`, `server/src/game/src/{char.cpp,char_item.cpp,safebox.cpp,item_manager.cpp,DragonSoul.cpp,questlua_global.cpp,questlua_pc.cpp,questlua_npc.cpp,questpc.cpp}`, `tests/test_db_cache.cpp`, `tests/test_lua55_quest.cpp` | `M-01`..`M-09` tamamlandıktan sonra modüller arası entegrasyon ve anti-dupe denetimi yapıldı | **`VERIFIED`** | Tespit edilen 11 somut entegrasyon/anti-dupe/performans kusuru minimal ve izole yamalarla kapatıldı; `test_db_cache` Senaryo M (`139/139 PASS`), `test_lua55_quest` (`11/11 PASS`), `--check-lua` (`11/11 PASS`) ve Kademe C canlı testleriyle doğrulandı | `M-01`..`M-09` | Yüksek | `P1` | `tests/run_limited.ps1 -Build`, `test_db_cache` (`139/139 PASS`), `test_lua55_quest` (`11/11 PASS`), `run_kademe_c_live.ps1` (`6/6 PASS`) |
| **`M-10`** | `CONTEXT-H2`, `CONTEXT-H3` | FreeBSD 15.1 amd64 GCC 14 + MariaDB 12.3.3 LTS üzerinde tüm x64 sunucu hedeflerinin, `>2^32` kimlik/envanter/quest kalıcılığının, 8 servisli serverfiles ve gerçek `pfctl -nf` doğrulamasının tamamlanması | `server/src/common/tables.h:12`, `server/src/db/src/{ClientManager.cpp,Monarch.cpp}`, `server/src/game/src/{ikarus_shop.h,char.cpp}`, `server/scripts/freebsd/pf_guard.sh`, `tests/run_freebsd_serverfiles_test.sh`, `tests/run_server_smoke_test.sh` | `997784ff` (`QuestStateToIndex` `"start"` -> `0`) ve `d256b6da` (`pf_guard.sh` `kldload -n pf` + gerçek `pfctl -nf` exit 0 kontrolü) ile tüm FreeBSD engelleri giderildi | **`VERIFIED`** | GitHub Actions Run **`41183346026`** (`headSha = 1d6d5c6db59bf20b4fd9a01fd30f0ffdd31882bd`, `10m 02s`, `SUCCESS`): `[307/307]` ELF64 derleme, `CTest 10/10`, `--check-lua 11/11`, `Quest 202/202`, `test_live_db 354/354`, `--test-itemshop 15/15`, `test_db_cache_integration 187/187`, `>2^32` (`5000000001`) AID/PID/envanter/quest/restart/replay `PASS`, 8 servis + cross-core `PASS`, gerçek `pfctl -nf` (`exit 0`) ve `[4.A]` MariaDB restart kalıcılığı `PASS` | `M-01`..`M-09`, `INT-01` | Düşük | `P0` | FreeBSD 15.1 amd64 GCC 14 CI Run `41183346026` (`SUCCESS`) |
| **`M-11`** | `CONTEXT-H8`, `HANDOFF-H8` | Native x64 İstemci (`client/src/**`, `extern/client/**`, `PE32+`) dönüşümü ve `0x20090064U` x64 protokol uyarlaması (`Packet.h`, `PythonNetworkStream*.cpp`, `PythonPlayer.cpp`, `PythonSafeBox.cpp`, `PythonItemShop.cpp`) | `client/src/**`, `extern/client/**` (ayrıntılı teknik sözleşme: §8–§12) | İstemci x64 ağ protokolü sözleşmesi, 16 modül envanteri ve native x64 SDK bağımlılık analizi tamamlandı | **`UNVERIFIED` (İzinli Aktif Geliştirme Alanı)** | Kullanıcı native x64 istemci geliştirmesine açık izin verdi (`AGENTS.md` §8); hedef WoW64'e dayanan 32-bit istemci değil, gerçek **native x64 (`PE32+`)** istemci ikilisi ve `0x20090064U` protokol uyumudur (§8–§12 kabul kriterleri tamamlanıp test edilmeden `VERIFIED` yazılamaz) | Sunucu x64 (`VERIFIED`) | Yüksek | `P1` | §12'deki `PE32+` derleme, 15 `static_assert` ve canlı `>2^32` oturum testleri |
| **`M-12`** | `CONTEXT-H9`, `HANDOFF-H9` | Yerel ortamda `actionlint` aracının durumu ve `.github/workflows/freebsd-gcc-test.yml` statik doğrulaması | `.github/workflows/freebsd-gcc-test.yml` | `actionlint` sistemde kurulu değildi | **`VERIFIED` (Yerel Statik Denetim + CI `41183346026`)** | `.github/workflows/freebsd-gcc-test.yml` hem statik olarak hem gerçek GitHub Actions koşusu (`41183346026` `SUCCESS`) üzerinde doğrulandı | Yok | Düşük | `P3` | `.github/workflows/freebsd-gcc-test.yml` statik yapı, `sh -n` ve CI `41183346026` |

---

## 5. Eski Oturum Betikleri Envanteri (`11/11`) ve Kalıcı `tests/` Altyapısı (`M-01` / `H-1` -> `VERIFIED`)

Devir sırasında `D:\mt22009\scratch\handoff\session_scripts_original\` altında yedeklenen 11 oturum-bağımlı betiğin tamamı incelenmiş; içlerindeki sabit oturum yolları (`C:\Users\oyun\.gemini\antigravity\brain\aade639f-...`), düz metin şifreler ve `server/files` yan etkileri temizlenerek sürüm kontrolündeki kalıcı `tests/` betiklerine dönüştürülmüştür:

| # | Eski Oturum Betiği (`scratch/handoff/session_scripts_original/`) | Amacı | Tespit Edilen Kusurlar (Sabit Yol / Şifre / Yan Etki) | Kalıcı Sürüm Kontrolü Karşılığı (`tests/`) | Durum |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `1` | `run_final_x64_live_verification.ps1` | 6 aşamalı tam Kademe C (MariaDB 3307, 5 şema + 4 migration 2x, `test_live_db`, `--test-itemshop`, `test_db_cache_integration`, `>2^32` Auth/Game/Replay duman testi) | Eski oturum UUID yolu, sabit parolalar | `tests/run_kademe_c_live.ps1` | **`REPLACED & VERIFIED`** |
| `2` | `run_g2_g5_kademe_c.ps1` | `G2`/`G5` 64-bit kimlik ve şema migrasyonu canlı testi | Eski oturum UUID yolu, sabit parolalar | `tests/run_kademe_c_live.ps1` (Aşama `[1/6]`–`[4/6]`) | **`MERGED & VERIFIED`** |
| `3` | `run_part5_protocol_smoke.ps1` | `test_server_smoke_client.exe --protocol-framing-audit` (`10/10`) ve `>2^32` Auth/Game/Replay testi | Eski oturum UUID yolu, sabit parolalar | `tests/run_kademe_c_live.ps1` (Aşama `[6/6]`, SHA256 yedek/geri yükleme korumalı) | **`MERGED & VERIFIED`** |
| `4` | `run_post_rev51_live.ps1` | MariaDB `server_audit`, 5 şema InnoDB, `backup_mariadb.ps1` `HMAC-SHA256(v2)` ve `rev51` saç migrasyonu testi | Eski oturum UUID yolu, sabit parolalar | `tests/run_kademe_c_live.ps1 -IncludeSecurityAndBackup` + `tests/run_live_tests.ps1 -AutoProvision` | **`MERGED & VERIFIED`** |
| `5` | `run_audit_guard_test.ps1` | FreeBSD POSIX `sh -n` (`4` betik) ve `audit_guard.sh` v2 maskeleme/arşiv/alarm testi | Eski oturum UUID yolu | `tests/run_kademe_c_live.ps1 -IncludeSecurityAndBackup` (Aşama `[5/6]`) | **`MERGED & VERIFIED`** |
| `6` | `run_red_green_3bugs.ps1` | Rev 52 `ikarusshop_log`, `itemshop` ve `monarch` 3 SQL hatasının Red->Green doğrulaması | Eski oturum UUID yolu, sabit parolalar | `tests/test_live_db.cpp` (`Suite 16`) & `tests/test_db_cache_integration.cpp` (`Senaryo 11`) -> `tests/run_kademe_c_live.ps1` | **`SUPERSEDED BY C++ SUITES`** |
| `7` | `verify_db_cache_176_and_warnings.ps1` | `test_db_cache_integration` ve derleyici uyarı kontrolü | Eski oturum UUID yolu, sabit parolalar | `tests/run_db_integration_tests.ps1 -AutoProvision` & `tests/scan_compiler_warnings.py` | **`MERGED & VERIFIED`** |
| `8` | `run_win_tier_c.ps1` | 8 servisli Windows `server/files` duman testi (`ch11 -> ch12` P2P WarpSet) | Eski oturum UUID yolu, `server/files` üzerinde kalıcı durum bırakma riski | `tests/run_kademe_c_live.ps1` (izole `3307` + `finally` altında bayt-bayt SHA256 `CONFIG`/`conf.txt` geri yükleme) | **`MERGED & VERIFIED`** |
| `9` | `scan_current_warnings.py` | `compile_commands.json` üzerinden tüm derleme birimlerinde genişletilmiş GCC uyarı taraması | Geçici `scratch` betiğiydi | `tests/scan_compiler_warnings.py` | **`REPLACED & VERIFIED`** |
| `10` | `rescan_modified_tus.py` | Değiştirilen C++ dosyalarında hızlı genişletilmiş GCC uyarı taraması | Sabit dosya listesi içeriyordu | `tests/scan_compiler_warnings.py --modified-only` (`git status --porcelain` entegreli) | **`MERGED & VERIFIED`** |
| `11` | `run_extended_warnings.py` | `db` ve `game` hedefleri için genişletilmiş uyarı özeti | Geçici `scratch` betiğiydi | `tests/scan_compiler_warnings.py` (`server/src` vs `extern/` ayrımı ve JSON/metin raporu) | **`MERGED & VERIFIED`** |

---

## 6. `INT-01` — Cross-System Integrity, Anti-Dupe & Runtime Correctness (`VERIFIED`)

`M-01`–`M-09` hedeflerinin tamamlanmasının ardından, modüller arası veri akışı, eşya tekilliği (anti-dupe), ekonomi sınırları, zamanlayıcı/olay yaşam döngüsü ve çalışma zamanı maliyeti üzerinde yürütülen `INT-01` denetiminde tespit edilip kapatılan 11 somut bulgu:

1. **Acce (Kuşak) Sistemi `SItemPosEx::id` 64->32-Bit Eşya Kimliği Daralması ve Başarısız Kombinasyon Yuva Temizliği (`server/src/common/length.h:1371`, `server/src/game/src/char.cpp:8726`):**
   - `SItemPosEx::id` alanı `int` (`32-bit`) yerine `uint64_t` (`64-bit`) yapılarak `> 2^31` / `> 2^32` / `> 2^53` kimlikli eşyaların `CHARACTER::GetAcceMaterials()` içinde `ITEM_MANAGER::Find()` tarafından bulunamaması hatası kök nedeninden giderildi.
   - `CHARACTER::RefineAcceMaterials` içinde kombinasyon başarısız olduğunda yok edilen `pkItemMaterial[1]` için `SetAcceMaterial(1, nullptr)` çağrısı eklendi.
2. **`AutoStackItemProto` / `AutoStackItem` / `AutoGiveItem` Anti-Dupe, Aktif Takas/Pazar Koruması ve Gereksiz `HEADER_GD_ITEM_SAVE` Fırtınası Önlemi (`server/src/game/src/char_item.cpp:5609, 5999, 6575-6735`):**
   - `AutoStackItemProto` içindeki `ITEM_FLAG_MAKECOUNT` normalizasyonu (`bCount = p->alValues[1]`) envanter döngüsünün dışına (öncesine) çıkarılarak birden fazla kısmi yığın bulunduğunda her yığında `bCount` değerinin tekrar büyütülmesi (eşya çoğalma riski) kapatıldı; ayrıca `!IS_SET(p->dwAntiFlags, ITEM_ANTIFLAG_STACK)` kontrolü eklendi.
   - `AutoStackItemProto`, `AutoStackItem` ve `AutoGiveItem` (`ITEM_BLEND` döngüsü) içinde halihazırda dolu (`GetCount() >= GetMaxStack()`), aktif takasta (`IsExchanging()`), kilitli (`isLocked()`) veya kişisel pazarda satışta (`m_pkMyShop->IsSellingItem(id)`) olan yığınların atlanması sağlandı; böylece hem aktif takas/pazar yığınlarının dışarıdan değişmesi önlendi hem de her eşya toplamada dolu yığınlar için gereksiz `UpdatePacket()` + `Save()` (`HEADER_GD_ITEM_SAVE`) gönderilmesi engellendi.
   - `CHARACTER::MoveItem` ve `CHARACTER::SwapItem` içinde hedef/kaynak eşya için `IsExchanging()` ve `isLocked()` kontrolleri sıkılaştırıldı.
3. **`CSafebox::ChangeSize` Grid Sıfırlanması, `CSafebox::__Destroy` Bellek Sızıntısı ve Kopuk Bağlantıda `SAFEBOX`/`MALL` Kayıt Güvenliği (`server/src/game/src/safebox.cpp:56, 67, 111, 180, 204`, `server/src/game/src/item_manager.cpp:478`):**
   - `CSafebox::ChangeSize` içinde `std::move(m_pkGrid)` sonrasında boşalmış `m_pkGrid.get()` yerine `pkOldGrid.get()` kontrol edilerek mevcut depo/nesne market ızgara hücrelerinin sıfırlanması hatası düzeltildi.
   - `CSafebox::__Destroy` içindeki `m_pkGrid.release()` bellek sızıntısı `m_pkGrid.reset()` ile giderildi.
   - `CSafebox::Add`, `CSafebox::Remove`, `CSafebox::Save` ve `ITEM_MANAGER::SaveSingleItem` (`SAFEBOX`/`MALL`) içinde `GetDesc()` null kontrolü ve `GetAID()` geri dönüşü eklendi.
4. **Kanal Değiştirme / Işınlanma / Çıkış Sırasında `DRAGON_SOUL_INVENTORY` Gecikmeli Kayıt Tahliyesi (`server/src/game/src/char.cpp:1277`):**
   - `CHARACTER::FlushDelayedSaveItem()` fonksiyonuna `0 .. DRAGON_SOUL_INVENTORY_MAX_NUM - 1` döngüsü eklenerek Ejderha Taşı Simyası envanterindeki eşyaların da ışınlanma ve kanal geçişinde anında DB'ye tahliye edilmesi sağlandı.
5. **DragonSoul Rafine (`DoRefineGrade`, `DoRefineStep`, `DoRefineStrength`) Ekipman / Takas / Kilit Koruması (`server/src/game/src/DragonSoul.cpp:508, 651, 801`):**
   - Her üç simya arındırma/güçlendirme fonksiyonunda `IsEquipPosition()`, `IsEquipped()`, `IsExchanging()` ve `isLocked()` kontrolleri zorunlu hale getirildi.
6. **`CHARACTER::WonExchange` ve `PointChange(POINT_CHEQUE)` Taşma ve `CHEQUE_MAX` (`1000`) Sınır Tutarlılığı (`server/src/game/src/char.cpp:3635-3648, 8109-8150`):**
   - `PointChange(POINT_CHEQUE)` içinde `CHEQUE_MAX <= nTotalCheque` (`>= 1000` reddi) uygulanırken `WonExchange` ("buy") içinde `GetCheque() + value > CHEQUE_MAX` (`> 1000`) kontrol edildiği için `GetCheque() + value == 1000` sınırında Yang düşülüp (`PointChange(POINT_GOLD, -totalCost)`) Won eklemesinin reddedilmesi (Yang kaybı) kusuru tespit edildi.
   - `WonExchange` girişinde `value <= 0 || value >= CHEQUE_MAX` (`1..999`), `"sell"` dalında `GetGold() < 0 || totalYang >= GOLD_MAX - GetGold()`, `"buy"` dalında `const int64_t nTotalCheque = static_cast<int64_t>(GetCheque()) + static_cast<int64_t>(value); if (GetCheque() < 0 || nTotalCheque >= CHEQUE_MAX)` ve `PointChange(POINT_CHEQUE)` içinde `nTotalCheque < 0 || CHEQUE_MAX <= nTotalCheque` kontrolleriyle sınırlar birebir eşitlendi.
7. **`move_channel_info` ve `leave_guild_war_event_info` Zamanlayıcılarında `DynamicCharacterPtr` ve Önceki Zamanlayıcı İptali (`server/src/game/src/char.cpp:8872, 8922, 9054`):**
   - Her iki olay yapısında ham `LPCHARACTER` yerine `DynamicCharacterPtr` kullanıldı ve `CreateMoveChannelEvent` içinde yeni olay oluşturulmadan önce mevcut `m_pkTimedEvent` için `event_cancel(&m_pkTimedEvent)` çağrısı eklendi.
8. **`quest::_char_log`, `questlua_dnd` (`item_select`/`item_get_id`) ve `questlua_building` (`building_get_land_id`) 64-Bit Kimlik Genişletmesi (`server/src/game/src/questlua_global.cpp:353`, `server/src/game/src/questlua_dnd.cpp:65, 85`, `server/src/game/src/questlua_building.cpp:26`):**
   - `_char_log` birinci parametresi ve `questlua_dnd` / `questlua_building` içindeki eşya/arazi kimliği dönüşümleri `DWORD`/`lua_tonumber`/`lua_pushnumber` yerine `uint64_t` (`m2_lua_touint64` / `m2_lua_pushuint64`) yapılarak `> 2^32` ve `> 2^53` kimliklerin kırpılması önlendi.
9. **`quest::pc_give_or_drop_item*` Null İşaretçi Koruması (`server/src/game/src/questlua_pc.cpp:519, 568, 629`):**
   - `pc_give_or_drop_item`, `pc_give_or_drop_item_with_dice` ve `pc_give_or_drop_item_and_select` içinde `item == NULL` durumunda `item->GetID()` / `FPartyDropDiceRoll` çağrılması engellendi.
10. **`CHARACTER::GetQuestNPC` ve `npc_lock`/`npc_unlock` `PID` / `VID` Ayrımı (`server/src/game/src/char.cpp:6590`, `server/src/game/src/questlua_npc.cpp:226, 244`, `server/src/game/src/questpc.cpp:313`):**
    - Oyuncu (`IsNPC() == false`) üzerinde `SetQuestNPCID(npc->GetVID())` yalnızca hedef NPC'nin **32-bit `VID`** değerini saklarken, NPC (`IsNPC() == true`, yani `m_bCharType != CHAR_TYPE_PC`) üzerinde `npc.lock()` kilitleyen oyuncunun **64-bit `PID`** değerini saklar. `GetQuestNPC()` içine `IsNPC() || m_dwQuestNPCVID == 0 || m_dwQuestNPCVID > UINT32_MAX` kontrolü eklenerek: (a) `> 2^32` `PID` sahibi oyuncularda `ch->GetQuestNPC()` (`GetCurrentNPCCharacterPtr()`) çağrısının NPC'yi sorunsuz döndürmesi, (b) NPC üzerinde saklanan `PID` değerinin (`<= UINT32_MAX` olsa bile) asla `VID` olarak aranmaması garanti altına alındı.
11. **`CItemIDRangeManager::cs_dwMaxItemID` (`<= INT64_MAX`), Aralık Yedek Taraması ve Mevcut `uint64_t` (`>= 2^63`) Kayıt Uyumu (`server/src/db/src/ItemIDRangeManager.{h,cpp}`, `server/src/db/src/Peer.cpp:47, 58`):**
    - `CItemIDRangeManager::cs_dwMaxItemID` üst sınırı `9.223.372.000.000.000.000ULL` (`<= INT64_MAX`) olarak sabitlenerek yeni üretilen tüm kalıcı kimliklerin Lua 5.5.1 `lua_Integer` (`int64_t`) tipinde pozitif (`> 0`) kalması garanti altına alındı.
    - Veritabanında önceden mevcut `> cs_dwMaxItemID` (`>= 2^63` dahil) `uint64_t` eşya kayıtları `Build()` sırasında korunup loglanır; SQL yükleme/kaydetme (`CreateItemTableFromRes`, `CItemCache::OnFlush`), `ITEM_MANAGER::CreateItem`/`Find`, `CHARACTER::FindItemByID` ve Lua `item.select`/`item.get_id` (`m2_lua_pushuint64`/`m2_lua_touint64`) zincirinde kayıpsız işlenir ve yeni aralıklarla çakışmaz.
    - `Build()` içinde `ullGlobalMaxID >= cs_dwDefaultPrebuildMaxItemID || m_listData.size() < 16` koşulu ve `min64Chunk` yedek taraması (`baseChunk` `cs_dwMaxItemID` sınırında olduğunda `4.290.000.001` başlangıcından tarama) eklendi; `dbRange` çakışma kontrolü `Build()` ve `UpdateRange()` içinde tutularak `InitializeNowItemID()` yeniden çağrım güvenliği sağlandı.

### 6.1 `INT-01` Alt Davranışlarının Gerçek Runtime vs Statik Doğrulama Matrisi

| `INT-01` Alt Davranışı | Doğrulama Türü | Kanıt Kaynağı |
| :--- | :--- | :--- |
| `AutoStackItemProto` (`ITEM_FLAG_MAKECOUNT` döngü dışı normalizasyonu + `IsExchanging()` / `isLocked()` / `IsSellingItem()` / dolu yığın atlama) | **Gerçek Runtime (`game_r41023.exe`) + Birim Modeli** | `game_r41023.exe --check-lua` (`Check 4`: gerçek `CHARACTER` + `CItem` envanter örnekleri), `test_db_cache` (`Senaryo M [INT-01b]`) |
| `AutoStackItem` (`IsExchanging()` / `isLocked()` / `IsSellingItem()` atlama ve tam kalan miktar korunumu) | **Gerçek Runtime (`game_r41023.exe`) + Birim Modeli** | `game_r41023.exe --check-lua` (`Check 4`: gerçek `CHARACTER` + `CItem` örnekleri), `test_db_cache` (`Senaryo M [INT-01b2]`) |
| `CSafebox::ChangeSize` (`std::move(m_pkGrid)` sonrası mevcut çok-hücreli `CGrid` doluluğunun korunması) ve `__Destroy` (`reset()`) | **Gerçek Runtime (`game_r41023.exe`) + Birim Modeli** | `game_r41023.exe --check-lua` (`Check 4`: gerçek `CSafebox` + 2-slot `CItem` `ChangeSize(1 -> 2)`), `test_db_cache` (`Senaryo M [INT-01e]`) |
| `CItemIDRangeManager::cs_dwMaxItemID` (`<= INT64_MAX`), mevcut `>= 2^63` `uint64_t` kayıt korunumu, `ITEM_MANAGER::CreateItem`/`Find`, `CHARACTER::FindItemByID` ve Lua 5.5.1 `item.select` / `item.get_id` | **Gerçek Runtime (`db_r41023.exe` + `game_r41023.exe`) + Birim Testi** | `game_r41023.exe --check-lua` (`Check 3` & `Check 4`: `>2^53`, `cs_dwMaxItemID` ve `>=2^63` `CItem` örnekleri), `tests/run_kademe_c_live.ps1` (canlı `db_r41023.exe` `Build()` + `ch11`), `test_lua55_quest` (`11/11`), `test_db_cache` (`Senaryo M [INT-01d]`) |
| `SetQuestNPCID` / `GetQuestNPC` / `npc.lock` / `npc.unlock` (`> 2^32` `PID` oyuncularda `GetQuestNPC() == npc` ve NPC üzerinde `PID`/`VID` ayrımı) | **Gerçek Runtime (`game_r41023.exe`)** | `game_r41023.exe --check-lua` (`Check 4`: `CHARACTER_MANAGER::CreateCharacter` ile `PID = 5.000.000.001` ve `5.000.000.002` oyuncular + NPC üzerinde gerçek `npc.lock()` / `npc.unlock()` / `GetQuestNPC()` akışı) |
| `ENABLE_CHEQUE_SYSTEM` / `ENABLE_WON_EXCHANGE_WINDOW` (`WonExchange`) ve `ENABLE_ACCE_COSTUME_SYSTEM` (`Acce`) | **Proje Kapsamı Dışı (Kalıcı Kapalı Şablon Özelliği) — Statik/Birim Korumalı** | Hem `CommonDefines.h` hem `Locale_inc.h` içinde kapalıdır (`POINT_CHEQUE` ve `POINT_ACCEDRAIN_RATE` `EPointTypes` içinde tanımlı değildir); aktif derlemede yalnızca `SItemPosEx::id` (`uint64_t`) yapısı yer alır (`test_db_cache` `Senaryo M [INT-01a,c]`) |
| `FlushDelayedSaveItem` (`DRAGON_SOUL_INVENTORY`), `DragonSoul` rafine kilit kontrolleri, `move_channel_info`/`leave_guild_war_event_info` `DynamicCharacterPtr` ve `pc_give_or_drop_item*` null korumaları | **Statik Denetim + Derleme Doğrulaması (`0` uyarı)** | `tests/run_limited.ps1 -Build` (`0` hata, `0` uyarı), `scan_compiler_warnings.py` (`0` yüksek riskli uyarı) |

---

## 7. `M-10` — FreeBSD 15.1 amd64 (`LP64`) Hazırlık ve Canlı CI Doğrulaması (`VERIFIED` — Run `41183346026`)

FreeBSD 15.1 amd64 (`LP64`: `sizeof(long) == 8`, `int64_t = long`, `uint64_t = unsigned long`) ortamında derleme ve canlı çalışma zamanı testlerinin %100 geçmesini sağlayan kök neden düzeltmeleri ve doğrulama kanıtları:

1. **FreeBSD `LP64` Çakışan `typedef YANG` Derleme Engeli (`server/src/common/tables.h:12` vs `server/src/game/src/typedef.h:122`):**
   - `server/src/common/tables.h:12` içinde `typedef long long YANG;` ve `static_assert(sizeof(YANG) == 8, ...)` kullanılarak her iki platformda (`LLP64` ve `LP64`) 8 baytlık işaretli tam sayı tanımı ve `%lld` biçimlendirme uyumu birleştirildi.
2. **FreeBSD `LP64` `-Wformat` Uyarıları (`server/src/db/src/ClientManager.cpp:771`, `server/src/db/src/Monarch.cpp:145, 164`):**
   - `int64_t` (`LP64` üzerinde `long int`) değişkenlerin `snprintf(..., "%lld", ...)` çağrısına doğrudan geçildiği noktalar `static_cast<long long>(...)` ile giderildi.
3. **FreeBSD `run_freebsd_serverfiles_test.sh` `126/126` InnoDB Tablo Sayısı ve Canlı `test_db_cache_integration` (`187/187`):**
   - `sql/account.sql` şemasındaki `itemshop_tx_journal` tablosu ile 5 şemadaki toplam temel InnoDB tablo sayısı `[Bonus 4.E]` içinde `126/126` (`account=10, common=10, log=39, player=40, world=27`) olarak doğrulandı; `[Bonus 4.0]` içine canlı `test_db_cache_integration` (`187/187 PASS`) ve `tests/run_live_tests.sh` içine `game_r41023 --test-itemshop` (`15/15 PASS`) eklendi.
4. **FreeBSD `run_server_smoke_test.sh` `> 2^32` Kimlik, Quest Durum İndeksi (`997784ff`) ve `--protocol-framing-audit` (`10/10`):**
   - Satır içi `name_reservation.account_id`, `land.id` ve `land.guild_id` sütunları `BIGINT UNSIGNED`, `quest_reward_proto.gold` ve `shop_special_proto.price` sütunları `BIGINT` yapıldı; `PLAYER_ID_START = 5000000001`, `ITEM_ID_RANGE = 5000000001 5100000000`, `account.id = 5000000001`, `player AUTO_INCREMENT = 5000000001`, `item AUTO_INCREMENT = 5000000001` ve `--protocol-framing-audit` (`10/10 PASS`) eklendi.
   - `CQuestManager::QuestStateToIndex(stQuestName, "start")` (`questmanager.cpp:781`) `"start"` durumu için her zaman `0` döndürdüğünden ve diğer durumlarda `get_crc32` (`quest_CRC`) kullandığından, `run_server_smoke_test.sh` içindeki başlangıç `player.quest` (`lValue=0`) tohum kaydı düzeltilerek karakter çıkışında `QuestSave` (`INSERT ... ON DUPLICATE KEY UPDATE lValue`) ve yeniden girişte `VerifySmoke` kalıcılığı kök nedeninden doğrulandı (`997784ff`).
5. **FreeBSD `pf_guard.sh` Gerçek `pfctl -nf` Doğrulaması (`d256b6da`):**
   - `server/scripts/freebsd/pf_guard.sh` içindeki sahte başarı çıktısı kaldırıldı; FreeBSD'de `/dev/pf` yoksa `kldload -n pf` ile çekirdek modülü güvenli şekilde (PF'yi etkinleştirmeden ve gerçek güvenlik duvarı durumunu değiştirmeden) yüklenerek gerçek `pfctl -nf` sözdizimi kontrolünün `exit code 0` döndürmesi zorunlu kılındı.
6. **Canlı FreeBSD 15.1 amd64 GCC 14 + MariaDB 12.3.3 LTS CI Kanıtı (`VERIFIED`):**
   - **GitHub Actions Run ID:** `41183346026` | **Head SHA:** `1d6d5c6db59bf20b4fd9a01fd30f0ffdd31882bd` | **Süre:** `10m 02s` | **Sonuç:** **`SUCCESS`** (`ELF64` `[307/307]`, `CTest 10/10`, `--check-lua 11/11`, `Quest 202/202`, `test_live_db 354/354`, `--test-itemshop 15/15`, `test_db_cache_integration 187/187`, `run_server_smoke_test.sh [1/4]..[4/4]` `>2^32` AID/PID/envanter/quest/restart/replay `PASS`, `run_freebsd_serverfiles_test.sh [1/7]..[7/7]` 8 servis + cross-core `PASS`, `[Bonus 4.F]` gerçek `pfctl -nf` `exit 0` `PASS` ve `[4.A]` MariaDB restart kalıcılığı `PASS`).

---

## 8. İstemci–Sunucu x64 Ağ Protokolü Sözleşmesi (`MT2_PROTOCOL_VERSION_X64 = 0x20090064U`)

Sunucu tarafında (`server/src/common/tables.h`, `server/src/common/building.h`, `server/src/game/src/packet.h`, `server/src/db/src/*`, `server/src/game/src/*`) tamamlanan ve doğrulanmış olan **Tam x64 Protokol Dönüşümü** ile istemci (`client/src/UserInterface/Packet.h` ve ağ işleyicileri) arasındaki bağlayıcı ikili (wire) sözleşme aşağıdadır.

### 8.1 Protokol Sürüm Sabitleri ve El Sıkışma Alanları

| Sabit / Alan | Eski Değer (Legacy x86) | Yeni Değer (x64 Wire Protocol) | Tanım Konumu |
| :--- | :--- | :--- | :--- |
| `MT2_PROTOCOL_VERSION_LEGACY_X86` | Yok (sürüm alanı yoktu) | `0x20090032U` | `server/src/common/tables.h` |
| `MT2_PROTOCOL_VERSION_X64` | Yok | `0x20090064U` (`537460836U`) | `server/src/common/tables.h` |
| `MT2_PROTOCOL_VERSION_CURRENT` | Yok | `MT2_PROTOCOL_VERSION_X64` (`0x20090064U`) | `server/src/common/tables.h` |
| `TPacketCGLogin3::dwProtocolVersion` | Yok (`TPacketCGLogin3 = 321 B` / eski `130 B`) | `MT2_PROTOCOL_VERSION_X64` (`TPacketCGLogin3 = 325 B`, TEA çerçevesi `328 B`, ofset `52`) | `server/src/game/src/packet.h`, `input_auth.cpp:259` |
| `TPacketCGLogin2::dwProtocolVersion` | Yok (`TPacketCGLogin2 = 52 B`) | `MT2_PROTOCOL_VERSION_X64` (`TPacketCGLogin2 = 56 B`, ofset `51`) | `server/src/game/src/packet.h`, `input_login.cpp:175` |
| `HEADER_GD_SETUP::dwProtocolVersion` | Yok (`TPacketGDSetup = 67 B`) | `MT2_PROTOCOL_VERSION_X64` (`TPacketGDSetup = 71 B`, ofset `67`) | `server/src/common/tables.h`, `ClientManager.cpp:2505` |
| `HEADER_GG_SETUP::dwProtocolVersion` | Yok (`TPacketGGSetup = 7 B`) | `MT2_PROTOCOL_VERSION_X64` (`TPacketGGSetup = 11 B`, ofset `7`) | `server/src/game/src/packet.h`, `input_p2p.cpp:58` |

### 8.2 Erken Protokol Reddi, TCP Framing ve Güvenlik Kuralları (`"VERSION"` vs `"WRONGVER"`)

1. **Eski Boyutlu Paket Yakalama ve `250 ms` Yarım Çerçeve Zamanlayıcısı (`"VERSION"` — `input.cpp`, `desc.cpp`):**
   - Eski 32-bit istemciler `HEADER_CG_LOGIN3` paketini `130`, `136` veya `321` bayt (yeni `328` bayt yerine), `HEADER_CG_LOGIN2` paketini ise `52` bayt (yeni `56` bayt yerine) olarak gönderir.
   - `CInputProcessor::Process` (`input.cpp`), `HEADER_CG_LOGIN3` (`< 328 B`) veya `HEADER_CG_LOGIN2` (`< 56 B`) tam boyuta ulaşmadığında `DESC` başına `250 ms` boşta yarım çerçeve zamanlayıcısı (`partial_login_event`) başlatır. Süre dolduğunda tamponda bilinen eski paket boyutlarından (`130`, `136`, `321` veya `52` bayt) biri ve geçerli sonlandırılmış `login` dizgesi varsa girdi tamponunu tüketir, `LoginFailure(d, "VERSION")` (`TPacketGCLoginFailure`, `szStatus = "VERSION"`) gönderir ve bağlantıyı `PHASE_CLOSE` ile kapatır.
   - Gerçek TCP segmentasyonu (ör. `328` baytlık yeni x64 `Login3` paketinin `52 B + 276 B`, `130 B + 198 B` veya `1 B` parçalar halinde gelmesi) ise sonraki segment ulaştığında sıfır kayıpla birleştirilir (`test_server_smoke_client.exe --protocol-framing-audit` `10/10 PASS`).
2. **Tam Boyutlu (`328 B` / `56 B`) Pakette Yanlış Sürüm Reddi (`"WRONGVER"` — `input_auth.cpp:259` & `input_login.cpp:175`):**
   - `TPacketCGLogin3` (`328 B`) veya `TPacketCGLogin2` (`56 B`) tam boyutta ulaştığında `pinfo->dwProtocolVersion != MT2_PROTOCOL_VERSION_X64` (`0x20090064U`) ise hiçbir SQL sorgusu çalıştırılmadan ve tek kullanımlık `dwLoginKey` tüketilmeden `LoginFailure(d, "WRONGVER")` gönderilir ve bağlantı kapatılır.
3. **DB (`HEADER_GD_SETUP`) ve P2P (`HEADER_GG_SETUP`) Sürüm ve Kısa Paket Koruması (`Peer.cpp` & `ClientManager.cpp`):**
   - `CPeer::PeekPacket` (`Peer.cpp:150`) `HEADER_GD_SETUP` için `dwLength != sizeof(TPacketGDSetup)` (`71 B`) olan kısa çerçeveleri reddeder; `CClientManager::QUERY_SETUP` ve `CInputP2P::Setup` ise `dwProtocolVersion != MT2_PROTOCOL_VERSION_X64` olan düğümleri reddeder.
4. **İstemci Tarafında Uygulanacak Sürüm Değişikliği:**
   - `client/src/UserInterface/Packet.h` içine `#define MT2_PROTOCOL_VERSION_X64 0x20090064U` eklenecek; `TPacketCGLogin2` (`56 B`, ofset `51`) ve `TPacketCGLogin3` (`325 B` / TEA `328 B`, ofset `52`) yapılarına `uint32_t dwProtocolVersion;` alanı eklenip gönderim sırasında `MT2_PROTOCOL_VERSION_X64` atanacaktır.

### 8.3 Temel Wire Tür Eşlemeleri, Ekonomi Tavanı (`GOLD_MAX`) ve Kimlik (`ID`) Mimarisi

> **Kritik Mimari İlke (32-Bit Veri Alanı $\neq$ x86 Mimari Bağımlılığı):**
> Protokol ve veritabanı tasarımında sabit genişlikli `uint32_t` / `int32_t` / `uint16_t` olması gereken alanlar (`VID`, `vnum`, `opcode`, `flags`, `anti_flags`, `alSockets[3]`, `TItemLimit::lValue`, `lMapIndex`, `x/y/z`, `count` vb.) x86 bağımlılığı değildir; sırf x64 hedefi nedeniyle tüm `INT` sütunları, işaretçi olmayan sayılar veya bütün paket alanları 64-bit yapılmaz. Yalnızca gerçekten 64-bit kapasite gerektiren kalıcı kimlikler, ekonomi/fiyat/puan alanları, zaman damgaları ve etki süreleri 64-bit (`uint64_t` / `int64_t`) olarak genişletilmiştir; diğer alanlarda platforma bağımlı `long` / `DWORD` yerine sabit genişlikli `int32_t` / `uint32_t` kullanılır.

| Alan Anlamı | Eski İstemci Türü (`Packet.h`) | Yeni x64 Wire Türü | Boyut Değişimi |
| :--- | :--- | :--- | :--- |
| Platforma bağımlı işaretli 32-bit (`long`) | `long` | `int32_t` | `4 B -> 4 B` (`LP64`'te `8 B` kaymasını önler) |
| Platforma bağımlı işaretsiz 32-bit (`DWORD`, `unsigned long`) | `DWORD` (`unsigned long`) | `uint32_t` (`DWORD`) | `4 B -> 4 B` |
| Oyun içi para / fiyat / maliyet / hazine (`YANG`, `gold`, `price`, `cost`, `lMoney`) | `int` / `long` / `DWORD` / `int64_t` (karışık) | `int64_t` (`YANG`) | `4 B -> 8 B` (önceden 32-bit kalan paketlerde) |
| Karakter puanları (`points[POINT_MAX_NUM]`, `amount`, `value`) | `long` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` |
| Karakter etki süreleri ve maliyeti (`TPacketAffectElement::lDuration`, `lSPCost`) | `long` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` (`TPacketGCAffectAdd`: `26 B -> 34 B`) |
| Sunucu zaman damgaları (`TPacketGCTime::time`, `TItemShop*Time`, `captchaExpireTime`) | `time_t` (`4 B` x86) / `DWORD` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` |
| ItemShop bakiyesi ve fiyatları (`dwDragonCoins`, `dwDragonMarks`, `dwPrice`) | `uint32_t` / `int32_t` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` |
| Kalıcı kimlikler (`AID`, `PID`, `ITEM_ID`, `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`) | `DWORD` (`4 B`) | `uint64_t` (`8 B`) | `4 B -> 8 B` |
| Oturum içi geçici varlık kimliği (`VID`) | `DWORD` (`4 B`) | `uint32_t` (`4 B`) | `4 B -> 4 B` (değişmez) |
| Eşya soket ve limit değerleri (`alSockets[3]`, `TItemLimit::lValue`) | `long` (`4 B`) | `int32_t` (`4 B`) | `4 B -> 4 B` (değişmez) |
| Harita indeksi ve koordinatlar (`lMapIndex`, `x`, `y`, `z`, `lX`, `lY`, `lZ`) | `long` (`4 B`) | `int32_t` (`4 B`) | `4 B -> 4 B` (değişmez) |

* **`GOLD_MAX` ve `EXCHANGE_GOLD_MAX` (`server/src/common/length.h`):**
  - Eski `2.000.000.000LL` (`~2 Milyar` Yang) sınırı **`1.000.000.000.000LL` (`1 Trilyon` Yang = $10^{12}$)** seviyesine yükseltilmiştir.
  - Bu sınır hem `> 2^31` ve `> 2^32` Yang işlemlerini (`CHARACTER::PointChange(POINT_GOLD)`, `ChangeGold`, `ComputeRefineFee`, NPC/PC Dükkanları, Ikarus Offline Shop, Takas/Exchange, Depo/Safebox ve İmparatorluk Hazinesi `CMonarch`) tam 64-bit kapasitede destekler hem de Lua `double` ($2^{53} \approx 9 \times 10^{15}$) ve `INT64_MAX` ($\approx 9.22 \times 10^{18}$) taşma sınırının güvenli bölgesinde kalır.
* **Sunucu İçi İmparatorluk Hazinesi (`TMonarchInfo` & `TPacketGDTakeMonarchMoney` — `server/src/common/tables.h`):**
  - `TMonarchInfo::money[4]` alanı `int[4]` (`16 B`) yerine `YANG[4]` (`int64_t[4]`, `32 B`) yapılmıştır (`sizeof(TMonarchInfo) == 144 B`).
  - `TPacketGDTakeMonarchMoney::llMoney` alanı `int` (`4 B`) yerine `YANG` (`int64_t`, `8 B`) yapılmıştır (`sizeof(TPacketGDTakeMonarchMoney) == 16 B`); `player.monarch.money` sütunu `bigint(20)` olarak güncellenmiştir.
* **Kimlik (`ID`) Türleri ve Lua Sınır Sözleşmesi:**
  - **64-Bit Kalıcı Kimlikler (`uint64_t` / `bigint(20) unsigned`):** `ACCOUNT_ID` (`AID`), `PLAYER_ID` (`PID`), `ITEM_ID` (`TItemIDRangeTable` 64-bit aralık tablosu, `cs_dwMaxItemID = 9.223.372.000.000.000.000ULL <= INT64_MAX`), `GUILD_ID`, `SHOP_ID`, `AUCTION_ID`, `OFFER_ID`, `account.itemshop_tx_journal.tx_id` (`TRANSACTION_ID`) ve `player.guild.token` (`GUILD_TOKEN`).
  - **Oturum İçi Geçici Varlık Kimliği (`uint32_t` / `DWORD`):** `VID` (`CHARACTER_MANAGER::AllocVID()`, `0` ve yaşayan varlık çakışma korumalı 32-bit çalışma zamanı kimliği).

---

## 9. Boyutu ve Ofseti Değişen `CLIENT ↔ GAME` Paketleri (Tam Bayt ve Ofset Tablosu)

Tüm paketler `#pragma pack(1)` hizalamasına sahiptir. Sunucu (`server/src/game/src/packet.h`, `server/src/common/tables.h`) ve istemci (`client/src/UserInterface/Packet.h`) arasında aşağıdaki ikili düzen birebir korunmalıdır:

### 9.1 `TPacketGCTime` (`HEADER_GC_TIME = 106`)
* **Yön:** `GAME -> CLIENT` | **Eski Boyut:** `5 B` (32-bit `time_t`) | **Yeni Boyut:** `9 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `bHeader` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `time` | `time_t` (`4 B` x86) | `int64_t` (`8 B`) | `1` | `1` |

* **Sunucu Kaynağı:** `char.cpp` / `input_login.cpp` (`t.time = static_cast<int64_t>(get_global_time());`).
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h` (`TPacketGCTime::time` -> `int64_t`), `CPythonNetworkStream::RecvTimePacket` (`PythonNetworkStreamPhaseGame.cpp`).

### 9.2 `TPacketGCPoints` (`HEADER_GC_CHARACTER_POINTS = 16`)
* **Yön:** `GAME -> CLIENT` | **Eski Boyut:** `1021 B` (`1 + 255 * 4`) | **Yeni Boyut:** `2041 B` (`1 + 255 * 8`)

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `header` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `points[POINT_MAX_NUM]` (`255` eleman) | `long[255]` (`1020 B`) | `int64_t[255]` (`2040 B`) | `1` | `1` |

* **Sunucu Kaynağı:** `char.cpp:1735` (`CHARACTER::SendPoints`: `points[i] = GetPoint(i)` ve `points[POINT_GOLD] = GetGold()`).
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h` (`points[POINT_MAX_NUM]` -> `int64_t`), `CPythonPlayer::SetStatus(DWORD dwType, int64_t lValue)` ve `GetStatus(DWORD dwType) -> int64_t` (`PythonPlayer.{h,cpp}`, `PythonNetworkStreamPhaseGame.cpp`).

### 9.3 `TPacketGCPointChange` (`HEADER_GC_CHARACTER_POINT_CHANGE = 17`)
* **Yön:** `GAME -> CLIENT` | **Eski Boyut:** `22 B` | **Yeni Boyut:** `25 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `header` | `int` (`4 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `dwVID` | `DWORD` (`4 B`) | `uint32_t` (`4 B`) | `4` | `1` |
| `type` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `8` | `5` |
| `amount` | `long` (`4 B`) | `int64_t` (`8 B`) | `9` | `6` |
| `value` | `long` (`4 B`) | `int64_t` (`8 B`) | `13` | `14` |
| *(ek alanlar / toplam)* | `amountGold`/`valueGold` ayrıydı | `amount` ve `value` birleşik `int64_t` (`packet.h` tam boyutu `25 B`) | `22 B` | `25 B` |

* **Sunucu Kaynağı:** `char.cpp:1765` (`CHARACTER::PointChange`).
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h` (`sizeof(TPacketGCPointChange) == 25`), `CPythonNetworkStream::RecvPointChange` (`PythonNetworkStreamPhaseGame.cpp`).

### 9.4 `TPacketCGSafeboxMoney` (`HEADER_CG_SAFEBOX_MONEY = 71`) & `TPacketGCSafeboxMoneyChange` (`HEADER_GC_SAFEBOX_MONEY_CHANGE = 84`)
* **Yön:** `CLIENT <-> GAME` | **Eski/Yeni Boyut (`CG`):** `6 B -> 10 B` | **Eski/Yeni Boyut (`GC`):** `5 B -> 9 B`

| Paket | Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TPacketCGSafeboxMoney` | `bHeader` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `TPacketCGSafeboxMoney` | `bState` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `1` | `1` |
| `TPacketCGSafeboxMoney` | `lMoney` | `long` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `2` | `2` |
| `TPacketGCSafeboxMoneyChange` | `bHeader` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `TPacketGCSafeboxMoneyChange` | `lMoney` | `long` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `1` | `1` |

* **Sunucu Kaynağı:** `input_main.cpp` (`SafeboxMoney`) ve `safebox.cpp`.
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h`, `PythonSafeBox.{h,cpp}`, `PythonNetworkStreamPhaseGameItem.cpp`.

### 9.5 `TPacketGCRefineInformation` (`HEADER_GC_REFINE_INFORMATION = 95`) & `TPacketGCRefineInformaionNew` (`HEADER_GC_REFINE_INFORMATION_NEW = 119`)
* **Yön:** `GAME -> CLIENT` | **Alt Yapı:** `TRefineTable` (`server/src/common/tables.h`)

| Yapı / Paket | Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset | Eski Boyut | Yeni Boyut |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TRefineTable` | `cost` | `int` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `5` | `5` | `53 B` | `57 B` |
| `TPacketGCRefineInformation` | `cost` | `int` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `6` | `6` | `52 B` | `56 B` |
| `TPacketGCRefineInformaionNew` | `cost` | `int` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `7` | `7` | `53 B` | `57 B` |

* **Detaylı `TPacketGCRefineInformation` Alan Düzeni (`56 B`):** `header` (`BYTE`, ofset `0`), `pos` (`BYTE`, ofset `1`), `src_vnum` / `result_vnum` / `refine_table`, `cost` (`YANG` / `int64_t`: `4 B -> 8 B`, sonraki alanlar `+4 B` kayar).
* **Sunucu Kaynağı:** `char_item.cpp:960-964`, `CHARACTER::ComputeRefineFee(YANG iCost, int iMultiply = 5)` (`char.cpp:7272`).
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h`, `CPythonNetworkStream::RecvRefineInformationPacket*` (`PyLong_FromLongLong`).

### 9.6 ItemShop Paketleri (`HEADER_GC_ITEMSHOP = 160` Alt Paketleri)
* **Yön:** `GAME -> CLIENT`

| Yapı / Alt Paket | Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset | Eski Boyut | Yeni Boyut |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TPacketGCItemShopBalance` | `dwDragonCoins` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `1` | `1` | `9 B` | `17 B` |
| `TPacketGCItemShopBalance` | `dwDragonMarks` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `5` | `9` | `9 B` | `17 B` |
| `TPacketGCItemShopLatestTimeAuction` | `dwEndTime` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `0` | `0` | `4 B` | `8 B` |
| `TItemShopItemPromotion` | `dwPrice` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `0` | `0` | `12 B` | `24 B` |
| `TItemShopItemPromotion` | `dwStartTime` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `4` | `8` | `12 B` | `24 B` |
| `TItemShopItemPromotion` | `dwEndTime` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `8` | `16` | `12 B` | `24 B` |
| `TItemShopTimeAuctionItem` | `dwStartTime` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `0` | `0` | `16 B` | `24 B` |
| `TItemShopTimeAuctionItem` | `dwEndTime` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `4` | `8` | `16 B` | `24 B` |
| `TItemShopTimeAuctionItem` | `dwAccountLimit` | `uint32_t` (`4 B`) | `uint32_t` (`4 B`) | `8` | `16` | `16 B` | `24 B` |
| `TItemShopTimeAuctionItem` | `dwMaxBuyAmount` | `uint32_t` (`4 B`) | `uint32_t` (`4 B`) | `12` | `20` | `16 B` | `24 B` |
| `TItemShopItem` | `dwPrice` | `uint32_t` (`4 B`) | `int64_t` (`8 B`) | `13` | `13` | `78 B` | `102 B` |
| `TItemShopItem` | `promotion` | `TItemShopItemPromotion` (`12 B`) | `TItemShopItemPromotion` (`24 B`) | `50` | `54` | `78 B` | `102 B` |
| `TItemShopItem` | `timeAuction` | `TItemShopTimeAuctionItem` (`16 B`) | `TItemShopTimeAuctionItem` (`24 B`) | `62` | `78` | `78 B` | `102 B` |

* **Sunucu Kaynağı:** `itemshop_manager.cpp` (`SendBalance`, `SendItemShopData`, `GetPlayerAccountBalance`, `CheckBalance`, `AddCash`, `BuyItem`).
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h`, `PythonItemShop.{h,cpp}`.

### 9.7 `TSpecialShopItemClient` (`HEADER_GC_SPECIAL_SHOP`)
* **Yön:** `GAME -> CLIENT` | **Eski Boyut:** `51 B` | **Yeni Boyut:** `55 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `vnum` | `DWORD` (`4 B`) | `uint32_t` (`4 B`) | `0` | `0` |
| `count` | `WORD` (`2 B`) | `uint16_t` (`2 B`) | `4` | `4` |
| `priceType` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `6` | `6` |
| `price` | `DWORD` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `7` | `7` |
| `priceVnum` | `DWORD` (`4 B`) | `uint32_t` (`4 B`) | `11` | `15` |
| `alSockets[3]` | `long[3]` (`12 B`) | `int32_t[3]` (`12 B`) | `15` | `19` |
| `aAttr[7]` | `TPlayerItemAttribute[7]` (`21 B`) | `TPlayerItemAttribute[7]` (`21 B`) | `27` | `31` |
| `limitType` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `48` | `52` |
| `limitValue` | `WORD` (`2 B`) | `uint16_t` (`2 B`) | `49` | `53` |

* **Sunucu Kaynağı:** `special_shop_manager.cpp:365-391`.
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h` (`TSpecialShopItemClient::price` -> `int64_t` / `YANG`, `sizeof == 55`).

### 9.8 `TSubPacketGCCaptchaMeta` (`HEADER_GC_CAPTCHA` Alt Paketi)
* **Yön:** `GAME -> CLIENT` | **Eski Boyut:** `5 B` | **Yeni Boyut:** `9 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `captchaExpireTime` | `DWORD` (`4 B`) | `int64_t` (`8 B`) | `0` | `0` |
| `attemptCount` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `4` | `8` |

* **Sunucu Kaynağı:** `char_player.h` / `char_player.cpp` (`m_dwCaptchaExpireTime` -> `int64_t`).
* **İstemci Hedefi:** `client/src/UserInterface/Packet.h` (`sizeof(TSubPacketGCCaptchaMeta) == 9`).

---

## 10. Sabit Genişlikli (`int32_t` / `uint32_t` / `uint64_t` / `YANG`) Ortak Paketler ve Yapılar

Aşağıdaki ortak paket ve yapılarda eski `long` / `DWORD` alanları sabit genişlikli `int32_t`, `uint32_t`, `uint64_t` ve `int64_t` (`YANG`) türlerine sabitlenmiştir:

| Paket / Yapı | Opcode / Başlık | Sabitlenen Alanlar ve Türleri | Paket Boyutu (`sizeof`) |
| :--- | :--- | :--- | :--- |
| `TPacketCGLogin2` | `HEADER_CG_LOGIN2 (109)` | `dwLoginKey` (`uint32_t`), `adwClientKey[4]` (`uint32_t[4]`), `dwProtocolVersion` (`uint32_t`, `MT2_PROTOCOL_VERSION_X64`, ofset `51`) | `56 B` |
| `TPacketCGLogin3` | `HEADER_CG_LOGIN3 (111)` | `adwClientKey[4]` (`uint32_t[4]`), `hwid[256]`, `dwProtocolVersion` (`uint32_t`, `MT2_PROTOCOL_VERSION_X64`, ofset `52`) | `325 B` (TEA `328 B`) |
| `TPacketGCAuthSuccess` | `HEADER_GC_AUTH_SUCCESS (150)` | `dwLoginKey` (`uint32_t`), `bResult` (`BYTE`) | `6 B` |
| `TPacketGCLoginSuccess` | `HEADER_GC_LOGIN_SUCCESS (6 / 32)` | `players[4]` (`TSimplePlayer::dwID` `uint64_t`), `guild_id[4]` (`uint64_t[4]`), `handle` (`uint32_t`), `random_key` (`uint32_t`) | `385 B` |
| `TPacketGCCharacterCreateSuccess` | `HEADER_GC_CHARACTER_CREATE_SUCCESS (8)` | `bAccountCharacterSlot` (`BYTE`), `player` (`TSimplePlayer`, `dwID` `uint64_t`) | `73 B` |
| `TPacketGCCharacterAdditionalInfo` | `HEADER_GC_CHAR_ADDITIONAL_INFO (136)` | `dwVID` (`uint32_t`), `dwEmpire` (`BYTE`), `dwGuildID` (`uint64_t`), `dwLevel` (`uint32_t`), `sAlignment` (`int16_t`), `bPKMode` (`BYTE`), `dwMountVnum` (`uint32_t`) | `66 B` |
| `TPacketGCCharacterUpdate` | `HEADER_GC_CHARACTER_UPDATE (19)` | `dwVID`, `awPart[CHR_EQUIPPART_NUM]`, `bMovingSpeed`, `bAttackSpeed`, `bStateFlag`, `dwAffectFlag[2]`, `dwGuildID` (`uint64_t`), `sAlignment`, `bPKMode`, `dwMountVnum` | `48 B` |
| `TPacketGCItemSet` | `HEADER_GC_ITEM_SET (21)` | `Cell`, `vnum` (`uint32_t`), `count` (`uint16_t`), `flags` (`uint32_t`), `anti_flags` (`uint32_t`), `highlight` (`bool`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `54 B` |
| `TPacketGCItemUpdate` | `HEADER_GC_ITEM_UPDATE (25)` | `Cell`, `count` (`uint16_t`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `41 B` |
| `TShopItemData` | `HEADER_GC_SHOP (38)` | `vnum` (`uint32_t`), `price` (`YANG` / `int64_t`), `count` (`uint16_t`), `display_pos` (`BYTE`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `48 B` |
| `TShopItemTable` | `HEADER_CG_MYSHOP (55)` | `vnum` (`uint32_t`), `count` (`uint16_t`), `pos` (`TItemPos`), `price` (`YANG` / `int64_t`), `display_pos` (`BYTE`) | `18 B` |
| `TPacketCGExchange` | `HEADER_CG_EXCHANGE (27)` | `sub_header` (`BYTE`), `arg1` (`YANG` / `int64_t`), `arg2` (`BYTE`), `Pos` (`TItemPos`) | `13 B` |
| `TPacketGCExchange` | `HEADER_GC_EXCHANGE (42)` | `sub_header` (`BYTE`), `is_me` (`BYTE`), `arg1` (`YANG` / `int64_t`), `arg2` (`TItemPos`), `arg3` (`uint32_t`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `50 B` |
| `TPacketGCLandList` | `HEADER_GC_LAND_LIST (130)` | `TLandPacketElement`: `dwID` (`uint32_t`), `x`, `y`, `width`, `height` (`int32_t`), `dwGuildID` (`uint64_t`) | `3 B + N * 28 B` |
| `building::TLand` | `HEADER_DG_BOOT` / `building.h` | `#pragma pack(1)`: `dwID` (`uint32_t`), `lMapIndex`, `x`, `y`, `width`, `height` (`int32_t`), `dwGuildID` (`uint64_t`), `bGuildLevelLimit` (`BYTE`), `dwPrice` (`YANG` / `int64_t`) | `41 B` |
| `building::TObjectMaterial` | `HEADER_DG_BOOT` / `building.h` | `#pragma pack(1)`: `dwItemVnum` (`uint32_t`), `dwCount` (`ITEM_COUNT` / `uint32_t`) | `8 B` |
| `building::TObjectProto` | `HEADER_DG_BOOT` / `building.h` | `#pragma pack(1)`: `dwVnum` (`uint32_t`), `dwPrice` (`YANG`), `kMaterials[5]` (`40 B`), `dwUpgradeVnum`, `dwUpgradeLimitTime` (`uint32_t`), `lLife`, `lRegion[4]` (`int32_t`), `dwNPCVnum` (`uint32_t`), `lNPCX`, `lNPCY` (`int32_t`), `dwGroupVnum`, `dwDependOnGroupVnum` (`uint32_t`) | `100 B` |
| `building::TObject` | `HEADER_DG_BOOT` / `building.h` | `#pragma pack(1)`: `dwID`, `dwLandID`, `dwVnum` (`uint32_t`), `lMapIndex`, `x`, `y` (`int32_t`), `xRot`, `yRot`, `zRot` (`float`), `lLife` (`int32_t`) | `40 B` |
| `TPacketGCAffectAdd` & `CAffect` | `HEADER_GC_AFFECT_ADD (126)` | `TPacketAffectElement` & `CAffect`: `dwType` (`uint32_t`), `bApplyOn` (`BYTE`), `lApplyValue` (`int32_t`), `dwFlag` (`uint32_t`), `lDuration` (`int64_t`), `lSPCost` (`int64_t`), `dwItemVnum` (`uint32_t`) | `34 B` |
| `TPacketGCTargetCreate` | `HEADER_GC_TARGET_CREATE (125)` | `lID` (`int32_t`), `szName[33]`, `dwVID` (`uint32_t`), `bType` (`BYTE`), `bEmpire` (`BYTE`), `STargetDebuff::duration` (`int64_t`) | `43 B` |
| `TPacketGCAffectRemove` | `HEADER_GC_AFFECT_REMOVE (127)` | `dwType` (`uint32_t`), `bApplyOn` (`BYTE`) | `6 B` |
| `TPacketGCQuestConfirm` | `HEADER_GC_QUEST_CONFIRM (46)` | `msg[64]`, `timeout` (`int32_t`), `requestPID` (`uint64_t`) | `78 B` |
| `TPacketGCGuildWar` | `HEADER_GC_GUILD` alt paketi | `dwGuildSelf` (`uint64_t`), `dwGuildOpp` (`uint64_t`), `bType` (`BYTE`), `bWarState` (`BYTE`) | `18 B` |
| `TPacketGCGuildName` | `HEADER_GC_GUILD` (`GUILD_SUBHEADER_GC_GUILD_NAME`) | `header` (`BYTE`), `size` (`uint16_t`), `subheader` (`BYTE`), `guildID` (`uint64_t`), `guildName[GUILD_NAME_MAX_LEN + 2]` (`14 B`) | `26 B` |
| `TSubPacketGCCaptchaGMWho` | `HEADER_GC_CAPTCHA` (`CAPTCHA_SUBHEADER_GC_GM_WHO`) | `targetPid` (`uint64_t`), `requestId[CAPTCHA_REQUEST_ID_MAX_LEN + 1]` (`15 B`) | `23 B` |

---

## 11. Native x64 İstemci (`PE32+`) Mimari Envanteri ve Gerçek SDK/ABI Engelleri (`UNVERIFIED`)

> **Platform ve Mimari Ayrımı:**
> 32-bit Windows işletim sistemi 64-bit (`PE32+`) ikilileri çalıştıramaz; 64-bit Windows ise WoW64 alt sistemi üzerinden 32-bit (`PE32`) ikilileri çalıştırabilir. Bu projenin istemci hedefi WoW64'e dayanan 32-bit istemci **değildir**; yalnızca 64-bit Windows üzerinde çalışan, tüm statik kütüphaneleri (`.lib`), nesne dosyaları (`.obj`) ve dinamik kütüphaneleri (`.dll`) gerçek **x86_64 (`PE32+` / `COFF AMD64`)** olan **gerçek native x64 istemcidir**.

### 11.1 `Metin2Client.sln` İçindeki 16 İstemci Modülü

| Modül Adı | Konum | Dosya Sayısı | İşlevi ve x64 Dönüşüm Kapsamı |
| :--- | :--- | :---: | :--- |
| **UserInterface** | `client/src/UserInterface` | 199 | Ana giriş noktası (`UserInterface.cpp`), ağ akışı (`PythonNetworkStream*.cpp`), `Packet.h`, `PythonPlayer.cpp`, `PythonSafeBox.cpp`, `PythonItemShop.cpp`, `GuildMarkUploader.cpp` |
| **EterLib** | `client/src/EterLib` | 152 | Grafik motoru temel kütüphanesi, DirectX 8/9 sarmalayıcıları, kamera, yazı tipi, pencere/IME (`HWND`, `WPARAM`, `LPARAM`, `LONG_PTR`) |
| **GameLib** | `client/src/GameLib` | 100 | Oyun dünyası, aktörler, uçan nesneler, eşya/zemin verileri, harita yükleyici |
| **EterBase** | `client/src/EterBase` | 48 | Dosya I/O, bellek havuzları, LZO/TEA şifreleme yardımcıları, zamanlayıcı, CRC32 (`__asm` -> C++ / intrinsic dönüşümü) |
| **EffectLib** | `client/src/EffectLib` | 37 | Parçacık (particle) efektleri ve görsel efekt yöneticisi |
| **EterGrnLib** | `client/src/EterGrnLib` | 26 | Granny 3D modelleme, iskelet ve animasyon yöneticisi (`granny.h` x64 ABI uyumu) |
| **MilesLib** | `client/src/MilesLib` | 21 | Miles Sound System ses motoru yöneticisi (`MSS.H` x64 ABI / ses arka ucu uyumu) |
| **SpeedTreeLib** | `client/src/SpeedTreeLib` | 19 | SpeedTree RT orman, ağaç ve bitki örtüsü yöneticisi (`SpeedTreeRT.h` x64 uyumu) |
| **EterPythonLib** | `client/src/EterPythonLib` | 18 | Python UI bileşenleri, ızgara pencereleri, butonlar (`PyTuple` 64-bit işaretçi/tamsayı bağlamaları) |
| **EterPack** | `client/src/EterPack` | 14 | `.data` / `.index` ve `.epk` / `.eix` arşiv paket çözücü |
| **ScriptLib** | `client/src/ScriptLib` | 13 | Python script çalışma zamanı ve C API bağlayıcıları (`Py_ssize_t`) |
| **EterLocale** | `client/src/EterLocale` | 13 | String dönüşümleri, dil tabloları, ANSI/UTF-8 codepage eşleşmeleri |
| **SphereLib** | `client/src/SphereLib` | 11 | Küresel çarpışma (collision) tespit kütüphanesi |
| **EterImageLib** | `client/src/EterImageLib` | 9 | TGA, BMP, DXTC resim format işleyicisi (`DevIL`) |
| **PRTerrainLib** | `client/src/PRTerrainLib` | 8 | Yükseklik haritası ve arazi kaplama yöneticisi |
| **CWebBrowser** | `client/src/CWebBrowser` | 4 | Dahili web kontrolü (`OLE` / `ActiveX` 64-bit COM işaretçileri) |

### 11.2 Mevcut 32-Bit (`x86`) Bağımlılıkları ve Gerçek Native x64 Engelleri

`extern/client/lib/` ve `client/files/` dizinlerindeki mevcut ikililerin tamamı **32-bit (`PE32` / `COFF i386`)** olduğundan x64 bağlayıcısı (`link.exe /MACHINE:X64`) tarafından doğrudan bağlanamaz (`LNK4272: library machine type 'x86' conflicts with target machine type 'x64'`). Çözülmesi gereken gerçek bağımlılık engelleri:

1. **Granny 3D (`EterGrnLib` — `granny2.dll`, `granny2.4.0.10.lib` .. `granny2.11.8.0.lib`):**
   - `UserInterface.cpp:33-41` içinde `GRANNY_PRODUCT_MINOR_VERSION` değerine göre 32-bit `granny2.*.lib` bağlanmakta ve çalışma zamanında `client/files/granny2.dll` (`PE32`) yüklenmektedir.
   - **x64 Gereksinimi:** Gerçek 64-bit (`PE32+`) `granny2_x64.dll` (veya `granny2.dll` x64) + x64 import `.lib` kütüphanesi sağlanmalı ve `granny.h` içindeki yapı/işaretçi tanımları 64-bit ABI ile doğrulanmalıdır.
2. **SpeedTree RT (`SpeedTreeLib` — `SpeedTreeRT.dll`, `SpeedTreeRT.lib`):**
   - `extern/client/lib/SpeedTreeRT.lib` (`50.622 B`) ve `client/files/SpeedTreeRT.dll` 32-bit kapalı kaynak ikililerdir.
   - **x64 Gereksinimi:** Gerçek x64 `SpeedTreeRT` kütüphanesi (`.lib` / `.dll`) veya `SpeedTreeLib` arayüzünü koruyan native x64 derlemesi sağlanmalıdır.
3. **Miles Sound System (`MilesLib` — `MSS32.dll`, `mss32.lib`, `client/files/miles/*.m3d/*.asi`):**
   - `extern/client/lib/mss32.lib` (`263.786 B`), `client/files/MSS32.dll` ve `miles/` altındaki 10 ses sürücüsü (`mssa3d.m3d`, `mssds3d.m3d`, `mssmp3.asi` vb.) 32-bit (`x86`) ikililerdir.
   - **x64 Gereksinimi:** Gerçek 64-bit Miles (`mss64.dll` + x64 `.lib` + x64 filtreler) veya `MilesLib` (`CSoundManager`, `CSoundInstance`) sınıf sözleşmesini bozmayan native x64 ses arka ucu kullanılmalıdır.
4. **DirectX 9 ve Windows SDK Kütüphaneleri (`EterLib`, `UserInterface`):**
   - `extern/client/lib/` içindeki `d3d9.lib`, `d3dx9.lib`, `ddraw.lib`, `dxguid.lib`, `dmoguids.lib` 32-bit kütüphanelerdir; `client/files/d3dx9_43.dll` 32-bit (`PE32`) DLL'dir.
   - **x64 Gereksinimi:** Windows 10/11 SDK `um\x64` altındaki resmi 64-bit sistem kütüphaneleri (`d3d9.lib`, `dinput8.lib`, `dxguid.lib`, `dmoguids.lib`, `strmiids.lib`, `ddraw.lib`, `winmm.lib`, `imm32.lib`, `ws2_32.lib`, `version.lib`, `imagehlp.lib`, `uuid.lib`) ve DirectX End-User Runtime (June 2010) `x64` `d3dx9.lib` + 64-bit `d3dx9_43.dll` kullanılmalıdır.
5. **Python 2.7 ve Statik Modül Kütüphaneleri (`ScriptLib`, `EterPythonLib`, `UserInterface`):**
   - `extern/client/lib/python27-static.lib` (`30,4 MB`), `python27_d-static.lib` (`12,6 MB`), `liblib.lib` (`26,6 MB` — Cython ile derlenmiş gömülü UI/root modülleri) ve `extern/client/lib/*.obj` (`121` adet 32-bit COFF nesne dosyası) tamamen 32-bit (`x86`) derlenmiştir.
   - `extern/client/include/Python27/pyconfig.h` içinde 32-bit sabitler (`SIZEOF_VOID_P = 4`) bulunmaktadır.
   - **x64 Gereksinimi:** Python çalışma zamanı ve `liblib` statik modülleri `_WIN64` / `x64` (`SIZEOF_VOID_P = 8`, `Py_ssize_t = int64_t`) olarak derlenmeli; `ScriptLib` / `UserInterface` içindeki `PyTuple_GetInteger`, `PyTuple_GetLong`, `PyTuple_GetUnsignedLong` ve `Py_BuildValue` çağrılarında 64-bit tam sayı (`int64_t` / `uint64_t`) ve 64-bit pencere/nesne tanıtıcısı (`capsule` / `ull`) kesilmeleri giderilmelidir.
6. **Açık Kaynak / Yeniden Derlenebilir Statik ve Dinamik Kütüphaneler (`extern/client/lib/`):**
   - Mevcut 32-bit `.lib` dosyaları: `cryptlib-Release.lib` / `cryptlib-Debug.lib` (`110,8 MB`, Git-ignored), `libboost_filesystem-vc141-mt-s-x32-1_67.lib`, `libboost_system-vc141-mt-s-x32-1_67.lib`, `devil.lib` / `DevIL-1.7.8.lib` / `ILU.lib` / `ILUT.lib` (ve `client/files/devil.dll`), `lzo2.lib`, `liblz4_static-Release.lib`, `snappy.lib`, `libjpeg-MT.lib`, `libpngd.lib`, `zlibd.lib`, `minIni-Release-x86-mt.lib`, `discord_rpc_r.lib` / `discord_rpc_d.lib`, `WebView2Loader.dll`.
   - **x64 Gereksinimi:** İstemcinin fiilen bağladığı tüm yardımcı kütüphaneler gerçek native `x64` (`/MACHINE:X64`) olarak derlenmeli veya x64 sürümleriyle değiştirilmeli; kullanılmayan 32-bit kütüphaneler (`opencv_*`, `libmysql.lib`, `mysqlclient.lib` vb.) bağlayıcı girdilerinden çıkarılmalıdır.
7. **32-Bit Inline Assembly (`__asm`), `SecureEngineSDK32` ve İşaretçi Kesilmesi (`Pointer Truncation`) Engelleri:**
   - MSVC x64 (`cl.exe` amd64) satır içi `__asm { ... }` bloklarını desteklemez. `EterBase`, `EterPack`, `UserInterface` ve `SecureEngineCustomVMs_VC_inline.h` / `ThemidaSDK.h` (`SecureEngineSDK32.lib`) içindeki x86 `__asm` kullanımları standart C++ veya `<intrin.h>` intrinsic fonksiyonlarına dönüştürülmelidir.
   - Win32 API ve motor sınıflarında 8 baytlık işaretçileri (`void*`, `HWND`, `HINSTANCE`, `WNDPROC`, `LPARAM`, `WPARAM`, `DWORD_PTR`, `ULONG_PTR`) 32-bit `DWORD`, `int` veya `long` türlerine dönüştüren tüm noktalar (`SetWindowLong` -> `SetWindowLongPtr`, `GetWindowLong` -> `GetWindowLongPtr`, `GCL_*` -> `GCLP_*`) 64-bit güvenli hale getirilmelidir.
   - Ağ paketlerinde ve dosya formatlarında (`EterPack`, `GameLib`, `Packet.h`) bellek içi işaretçi boyutu (`sizeof(void*) == 8`) ile disk/ağ üzerindeki sabit genişlikli yapıların (`#pragma pack(1)`, `sizeof`, `offsetof`, `alignof`) birbirine karıştırılmaması zorunludur.

---

## 12. Native x64 İstemci ve Protokol Uyarlaması Kabul Kriterleri (`UNVERIFIED`)

İstemci dönüşümü tamamlandığında aşağıdaki somut doğrulamaların tamamı gerçek derleme ve canlı çalışma zamanında `PASS` olmadan `M-11` asla `VERIFIED` olarak işaretlenemez:

1. **Gerçek Native x64 (`PE32+`) İkili ve Bağımlılık Doğrulaması:**
   - `Metin2Client.sln` içindeki 16 modülün tamamının `x64` (`Release|x64` / `Debug|x64`) hedefinde `0` hata ile derlenmesi ve üretilen `metin2client.exe` dosyasının `PE32+ executable (GUI) x86-64` (`IMAGE_FILE_MACHINE_AMD64 = 0x8664`) olduğunun kanıtlanması.
   - Çalışma zamanında yüklenen tüm DLL dosyalarının `PE32+` (`x64`) olduğunun ve hiçbir 32-bit (`x86`) `.lib` / `.obj` / `.dll` bağımlılığının kalmadığının doğrulanması.
2. **Derleme Zamanı `static_assert` Kontrolleri (`client/src/UserInterface/Packet.h` — En Az 15 Zorunlu Kontrol):**
   - `static_assert(sizeof(TPacketGCTime) == 9);`
   - `static_assert(sizeof(TPacketGCPoints) == 2041);`
   - `static_assert(sizeof(TPacketGCPointChange) == 25);`
   - `static_assert(sizeof(TPacketCGSafeboxMoney) == 10);`
   - `static_assert(sizeof(TPacketGCSafeboxMoneyChange) == 9);`
   - `static_assert(sizeof(TPacketGCRefineInformation) == 56);`
   - `static_assert(sizeof(TPacketGCRefineInformaionNew) == 57);`
   - `static_assert(sizeof(TPacketGCItemShopBalance) == 17);`
   - `static_assert(sizeof(TItemShopItem) == 102);`
   - `static_assert(sizeof(TSpecialShopItemClient) == 55);`
   - `static_assert(sizeof(TSubPacketGCCaptchaMeta) == 9);`
   - `static_assert(sizeof(TSubPacketGCCaptchaGMWho) == 23);`
   - `static_assert(sizeof(TPacketGCAffectAdd) == 34);`
   - `static_assert(sizeof(TPacketGCQuestConfirm) == 78);`
   - `static_assert(sizeof(TPacketGCGuildName) == 26);`
   - *(Ek sabit boyut doğrulamaları: `TPacketCGLogin2 == 56`, `TPacketCGLogin3 == 325`, `TPacketGCLoginSuccess == 385`, `TPacketGCCharacterCreateSuccess == 73`, `TPacketGCCharacterAdditionalInfo == 66`, `TPacketGCCharacterUpdate == 48`, `TPacketGCTargetCreate == 43`.)*
3. **Protokol Sürüm El Sıkışması Doğrulaması (`MT2_PROTOCOL_VERSION_X64 = 0x20090064U`):**
   - `dwProtocolVersion == 0x20090064U` ile `AUTH` (`HEADER_CG_LOGIN3`) ve `GAME` (`HEADER_CG_LOGIN2`) bağlantısının kabul edilmesi.
   - `0x20090032U` veya eski boyutlu paketlerle bağlantının `"VERSION"` / `"WRONGVER"` ile reddedilmesi.
4. **Canlı 64-Bit Kimlik (`> 2^32`), Ekonomi (`> 2^32` Yang) ve Zaman Damgası (`> 2038`) Oturum Testi:**
   - `AID > 2^32`, `PID > 2^32`, `ITEM_ID > 2^32` ve `GUILD_ID > 2^32` kimlikleriyle oyuna giriş, karakter seçimi, envanter, lonca adı (`TPacketGCGuildName`), quest onayı (`TPacketGCQuestConfirm`) ve etki (`TPacketGCAffectAdd`) akışlarının kesintisiz çalışması.
   - `POINT_GOLD = 75.000.000.000` (`75 Milyar` Yang) değerinin `TPacketGCPoints` ve `TPacketGCPointChange` üzerinden Python arayüzünde kesilmeden görüntülenmesi.
   - `TPacketCGSafeboxMoney` / `TPacketGCSafeboxMoneyChange` ile `50.000.000.000` Yang depo transferi.
   - `TPacketGCRefineInformation` ile `> 2^31` Yang yükseltme maliyetinin negatifleşmeden gösterilmesi.
   - `TPacketGCItemShopBalance` ve `TItemShopItem` ile `> 2^32` Dragon Coin/Mark bakiyesi ve ürün fiyatlarının doğru okunması.
   - `TPacketGCTime` ile `> 2038` (`> 2.147.483.647`) zaman damgalarının taşmadan işlenmesi.
