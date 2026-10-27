# METİN2 MT2009 — CLIENT x64 PROTOKOL UYARLAMA KILAVUZU (`CLIENT_X64_PROTOCOL_MIGRATION.md`)

> **ÖNEMLİ:** Bu belge, sunucu tarafında (`server/src/common/tables.h`, `server/src/game/src/packet.h`, `server/src/db/src/*`, `server/src/game/src/*`) tamamlanan **Tam x64 Protokol Dönüşümü** sonrasında, mevcut `client/src/**` dosyalarına dokunulmadan gelecekteki **Client x64 Uyarlama Aşaması** için hazırlanmış bayt ve ofset düzeyinde teknik sözleşmedir.

---

## 1. PROTOKOL SÜRÜM SÖZLEŞMESİ VE ERKEN BAĞLANTI REDDİ

| Sabit / Alan | Eski Değer (Legacy x86) | Yeni Değer (x64 Wire Protocol) | Tanım Konumu |
| :--- | :--- | :--- | :--- |
| `MT2_PROTOCOL_VERSION_LEGACY_X86` | `0x00000000U` veya rastgele `adwClientKey[3]` | `0x20090032U` | `server/src/common/tables.h` |
| `MT2_PROTOCOL_VERSION_X64` | Yok | `0x20090064U` | `server/src/common/tables.h` |
| `MT2_PROTOCOL_VERSION_CURRENT` | Yok | `MT2_PROTOCOL_VERSION_X64` (`0x20090064U`) | `server/src/common/tables.h` |
| `TPacketCGLogin3::adwClientKey[3]` | TEA anahtarının 4. `uint32_t` kelimesi | `MT2_PROTOCOL_VERSION_X64` (`0x20090064U`) | `input_auth.cpp:259` |
| `TPacketCGLogin2::adwClientKey[3]` | TEA anahtarının 4. `uint32_t` kelimesi | `MT2_PROTOCOL_VERSION_X64` (`0x20090064U`) | `input_login.cpp:175` |
| `HEADER_GD_SETUP::dwProtocolVersion` | Yok (`TPacketGDSetup = 67 B`) | `MT2_PROTOCOL_VERSION_X64` (`TPacketGDSetup = 71 B`, ofset `67`) | `tables.h`, `ClientManager.cpp:2505` |
| `HEADER_GG_SETUP::dwProtocolVersion` | Yok (`TPacketGGSetup = 7 B`) | `MT2_PROTOCOL_VERSION_X64` (`TPacketGGSetup = 11 B`, ofset `7`) | `packet.h`, `input_p2p.cpp:58` |

### Erken Protokol Reddi Davranışı
1. **AUTH (`HEADER_CG_LOGIN3` — `input_auth.cpp:259`):**
   - `pinfo->adwClientKey[3] != MT2_PROTOCOL_VERSION_X64` ise veritabanı sorgusu çalıştırılmadan `LoginFailure(d, "WRONGVER")` (`TPacketGCLoginFailure`, `szStatus = "WRONGVER"`) gönderilir ve bağlantı kapatılır.
2. **GAME LOGIN (`HEADER_CG_LOGIN2` — `input_login.cpp:175`):**
   - `pinfo->adwClientKey[3] != MT2_PROTOCOL_VERSION_X64` ise `HEADER_GD_LOGIN_BY_KEY` sorgusu gönderilmeden `LoginFailure(d, "WRONGVER")` gönderilir ve bağlantı kapatılır.
3. **Client Tarafında Yapılacak Sürüm Değişikliği:**
   - `client/src/UserInterface/Packet.h` içine `#define MT2_PROTOCOL_VERSION_X64 0x20090064U` eklenecek.
   - `CPythonNetworkStream::SendLoginPacketNew` (`HEADER_CG_LOGIN3`) ve `CPythonNetworkStream::SendDirectEnterPacket` / `SendLoginPacket` (`HEADER_CG_LOGIN2`) içinde `adwClientKey[3] = MT2_PROTOCOL_VERSION_X64;` atanacak.

---

## 2. TEMEL WIRE TÜR EŞLEMELERİ (`#pragma pack(1)` Küçük Sonlu / Little-Endian)

| Alan Anlamı | Eski Client Türü (`Packet.h`) | Yeni x64 Wire Türü | Boyut Değişimi |
| :--- | :--- | :--- | :--- |
| Platforma bağımlı işaretli 32-bit (`long`) | `long` | `int32_t` | `4 B -> 4 B` (LP64'te 8 B kaymasını önler) |
| Platforma bağımlı işaretsiz 32-bit (`DWORD`, `unsigned long`) | `DWORD` (`unsigned long`) | `uint32_t` (`DWORD`) | `4 B -> 4 B` |
| Oyun içi para / fiyat / maliyet / hazine (`YANG`, `gold`, `price`, `cost`, `lMoney`) | `int` / `long` / `DWORD` / `int64_t` (karışık) | `int64_t` (`YANG`) | `4 B -> 8 B` (önceden 32-bit kalan paketlerde) |
| Karakter puanları (`points[POINT_MAX_NUM]`, `amount`, `value`) | `long` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` |
| Sunucu zaman damgaları (`TPacketGCTime::time`, `TItemShop*Time`, `captchaExpireTime`) | `time_t` (`4 B` x86) / `DWORD` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` |
| ItemShop bakiyesi ve fiyatları (`dwDragonCoins`, `dwDragonMarks`, `dwPrice`) | `uint32_t` / `int32_t` (`4 B`) | `int64_t` (`8 B`) | `4 B -> 8 B` |
| Eşya soket ve limit değerleri (`alSockets[]`, `TItemLimit::lValue`) | `long` (`4 B`) | `int32_t` (`4 B`) | `4 B -> 4 B` |
| Harita indeksi ve koordinatlar (`lMapIndex`, `x`, `y`, `z`, `lX`, `lY`, `lZ`) | `long` (`4 B`) | `int32_t` (`4 B`) | `4 B -> 4 B` |

---

## 3. BOYUTU VE OFSETİ DEĞİŞEN `CLIENT ↔ GAME` PAKETLERİ (TAM TABLO)

Aşağıdaki paketlerin sunucu tarafındaki (`server/src/game/src/packet.h` ve `server/src/common/tables.h`) ikili (binary) boyutları ve alan ofsetleri 64-bit kapasiteye yükseltilmiştir:

### 3.1. `TPacketGCTime` (`HEADER_GC_TIME = 106`)
* **Yön:** `GAME -> CLIENT`
* **Eski Boyut:** `5 B` (32-bit `time_t`) | **Yeni Boyut:** `9 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `bHeader` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `time` | `time_t` (`4 B` x86) | `int64_t` (`8 B`) | `1` | `1` |

* **Sunucu Encoder:** `char.cpp` / `input_login.cpp`: `t.time = static_cast<int64_t>(get_global_time());`
* **Gelecek Client Değişikliği:** `client/src/UserInterface/Packet.h` içinde `TPacketGCTime::time` alanı `int64_t` yapılacak; `CPythonNetworkStream::RecvTimePacket` 9 bayt okuyacak.

---

### 3.2. `TPacketGCPoints` (`HEADER_GC_CHARACTER_POINTS = 16`)
* **Yön:** `GAME -> CLIENT`
* **Eski Boyut:** `1021 B` (`1 + 255 * 4`) | **Yeni Boyut:** `2041 B` (`1 + 255 * 8`)

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `header` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `points[POINT_MAX_NUM]` (`255` eleman) | `long[255]` (`1020 B`) | `int64_t[255]` (`2040 B`) | `1` | `1` |

* **Sunucu Encoder:** `char.cpp:1735` (`CHARACTER::SendPoints`): `points[i] = GetPoint(i)` ve `points[POINT_GOLD] = GetGold()` (`int64_t`) değerlerini doğrudan 64-bit yazar.
* **Gelecek Client Değişikliği:**
  - `client/src/UserInterface/Packet.h`: `TPacketGCPoints::points[POINT_MAX_NUM]` türü `int64_t` yapılacak (`sizeof == 2041`).
  - `CPythonPlayer::SetStatus(DWORD dwType, int64_t lValue)` ve `GetStatus(DWORD dwType) -> int64_t` 64-bit'e yükseltilecek; `POINT_GOLD`, `POINT_EXP`, `POINT_NEXT_EXP` değerleri kesilmeden saklanacak.

---

### 3.3. `TPacketGCPointChange` (`HEADER_GC_CHARACTER_POINT_CHANGE = 17`)
* **Yön:** `GAME -> CLIENT`
* **Eski Boyut:** `22 B` | **Yeni Boyut:** `25 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `header` | `int` (`4 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `dwVID` | `DWORD` (`4 B`) | `uint32_t` (`4 B`) | `4` | `1` |
| `type` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `8` | `5` |
| `amount` | `long` (`4 B`) | `int64_t` (`8 B`) | `9` | `6` |
| `value` | `long` (`4 B`) | `int64_t` (`8 B`) | `13` | `14` |
| `amountGold` / `valueGold` | Ayrı alan (varsa) | `amount` ve `value` içinde birleşik `int64_t` | — | — |
| *(toplam)* | — | — | `22 B` | `25 B` *(Not: `packet.h` içindeki tam tanıma göre `sizeof(TPacketGCPointChange) == 25`)* |

* **Sunucu Encoder:** `char.cpp:1765` (`CHARACTER::PointChange`): `amount` ve `value` 64-bit `int64_t` olarak doldurulur.
* **Gelecek Client Değişikliği:** `client/src/UserInterface/Packet.h` içinde `TPacketGCPointChange` yapısı sunucu `packet.h` ile birebir hizalanacak (`sizeof == 25`, `amount` ve `value` `int64_t`); `CPythonNetworkStream::RecvPointChange` 64-bit değerleri `CPythonPlayer::SetStatus` içine aktaracak.

---

### 3.4. `TPacketCGSafeboxMoney` (`HEADER_CG_SAFEBOX_MONEY = 71`) & `TPacketGCSafeboxMoneyChange` (`HEADER_GC_SAFEBOX_MONEY_CHANGE = 84`)
* **Yön:** `CLIENT <-> GAME`
* **Eski Boyut (`CG`):** `6 B` | **Yeni Boyut (`CG`):** `10 B`
* **Eski Boyut (`GC`):** `5 B` | **Yeni Boyut (`GC`):** `9 B`

| Paket | Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TPacketCGSafeboxMoney` | `bHeader` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `TPacketCGSafeboxMoney` | `bState` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `1` | `1` |
| `TPacketCGSafeboxMoney` | `lMoney` | `long` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `2` | `2` |
| `TPacketGCSafeboxMoneyChange` | `bHeader` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `0` | `0` |
| `TPacketGCSafeboxMoneyChange` | `lMoney` | `long` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `1` | `1` |

* **Sunucu Encoder/Decoder:** `input_main.cpp` (`SafeboxMoney`) ve `safebox.cpp` (`YANG` 64-bit bakiye).
* **Gelecek Client Değişikliği:** `Packet.h` ve `PythonSafeBox.cpp` içinde `lMoney` türü `int64_t` yapılacak (`sizeof(TPacketCGSafeboxMoney) == 10`, `sizeof(TPacketGCSafeboxMoneyChange) == 9`).

---

### 3.5. `TPacketGCRefineInformation` (`HEADER_GC_REFINE_INFORMATION = 95`) & `TPacketGCRefineInformaionNew` (`HEADER_GC_REFINE_INFORMATION_NEW = 119`)
* **Yön:** `GAME -> CLIENT`
* **Alt Yapı:** `TRefineTable` (`server/src/common/tables.h`)

| Yapı / Paket | Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset | Eski Boyut | Yeni Boyut |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TRefineTable` | `cost` | `int` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `5` | `5` | `53 B` | `57 B` |
| `TPacketGCRefineInformation` | `cost` | `int` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `6` | `6` | `52 B` | `56 B` |
| `TPacketGCRefineInformaionNew` | `cost` | `int` (`4 B`) | `YANG` (`int64_t`, `8 B`) | `7` | `7` | `53 B` | `57 B` |

* **Detaylı `TPacketGCRefineInformation` Alan Ofsetleri (`56 B`):**
  - `header` (`BYTE`, ofset `0`)
  - `pos` (`BYTE`, ofset `1`)
  - `src_vnum` (`uint32_t`, ofset `2`)
  - `result_vnum` (`uint32_t`, ofset `6` -> `packet.h` sırasına göre: `header[0]`, `pos[1]`, ` refine_table` / `cost`)
  - `cost` (`YANG` / `int64_t`: `4 B -> 8 B`, sonraki alanlar `+4 B` kayar)
* **Sunucu Encoder:** `char_item.cpp:960-964`, `CHARACTER::ComputeRefineFee(YANG iCost, int iMultiply = 5)` (`char.cpp:7272`).
* **Gelecek Client Değişikliği:** `Packet.h` içinde `TPacketGCRefineInformation::cost` ve `TPacketGCRefineInformaionNew::cost` alanları `int64_t` yapılacak; `CPythonNetworkStream::RecvRefineInformationPacket*` 64-bit maliyeti Python UI'a `PyLong_FromLongLong` ile iletecek.

---

### 3.6. ItemShop Paketleri (`HEADER_GC_ITEMSHOP = 160` Alt Paketleri)
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

* **Sunucu Encoder:** `itemshop_manager.cpp` (`SendBalance`, `SendItemShopData`, `GetPlayerAccountBalance`, `CheckBalance`, `AddCash`, `BuyItem`).
* **Gelecek Client Değişikliği:** `Packet.h` ve `PythonItemShop.cpp` içindeki `TPacketGCItemShopBalance`, `TPacketGCItemShopLatestTimeAuction`, `TItemShopItemPromotion`, `TItemShopTimeAuctionItem` ve `TItemShopItem` yapıları yukarıdaki 64-bit alanlarla güncellenecek.

---

### 3.7. `TSpecialShopItemClient` (`HEADER_GC_SPECIAL_SHOP`)
* **Yön:** `GAME -> CLIENT`
* **Eski Boyut:** `51 B` | **Yeni Boyut:** `55 B`

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

* **Sunucu Encoder:** `special_shop_manager.cpp:365-391` (`item.price` artık `UINT_MAX`'a kırpılmadan tam 64-bit `YANG` olarak gönderilir).
* **Gelecek Client Değişikliği:** `Packet.h` içindeki `TSpecialShopItemClient::price` alanı `int64_t` (`YANG`) yapılacak (`sizeof == 55`).

---

### 3.8. `TSubPacketGCCaptchaMeta` (`HEADER_GC_CAPTCHA` Alt Paketi)
* **Yön:** `GAME -> CLIENT`
* **Eski Boyut:** `5 B` | **Yeni Boyut:** `9 B`

| Alan | Eski Tür | Yeni Tür | Eski Ofset | Yeni Ofset |
| :--- | :--- | :--- | :--- | :--- |
| `captchaExpireTime` | `DWORD` (`4 B`) | `int64_t` (`8 B`) | `0` | `0` |
| `attemptCount` | `BYTE` (`1 B`) | `BYTE` (`1 B`) | `4` | `8` |

* **Sunucu Encoder:** `char_player.h` / `char_player.cpp` (`m_dwCaptchaExpireTime` -> `int64_t`).
* **Gelecek Client Değişikliği:** `Packet.h` içinde `TSubPacketGCCaptchaMeta::captchaExpireTime` alanı `int64_t` yapılacak (`sizeof == 9`).

---

## 4. BOYUTU DEĞİŞMEYEN ANCAK SABİT GENİŞLİKLİ (`int32_t` / `uint32_t` / `YANG`) HALE GETİRİLEN PAKETLER

Aşağıdaki ortak paketlerde eski `long` / `DWORD` alanları, 64-bit derleyicilerde (özellikle LP64 sistemlerde) `sizeof(long) == 8` nedeniyle oluşacak paket kaymalarını önlemek amacıyla sabit genişlikli `int32_t` / `uint32_t` / `int64_t` (`YANG`) türlerine sabitlenmiştir:

| Paket / Yapı | Opcode |Sabitlenen Alanlar | Paket Boyutu (`sizeof`) |
| :--- | :--- | :--- | :--- |
| `TPacketCGLogin2` | `HEADER_CG_LOGIN2 (109)` | `dwLoginKey` (`uint32_t`), `adwClientKey[4]` (`uint32_t[4]`, `[3] = MT2_PROTOCOL_VERSION_X64`) | `53 B` |
| `TPacketCGLogin3` | `HEADER_CG_LOGIN3 (111)` | `adwClientKey[4]` (`uint32_t[4]`, `[3] = MT2_PROTOCOL_VERSION_X64`) | `64 B` |
| `TPacketGCAuthSuccess` | `HEADER_GC_AUTH_SUCCESS (150)` | `dwLoginKey` (`uint32_t`), `bResult` (`BYTE`) | `6 B` |
| `TPacketGCCharacterAdditionalInfo` | `HEADER_GC_CHAR_ADDITIONAL_INFO (136)` | `dwVID`, `dwEmpire`, `dwGuildID`, `dwLevel`, `sAlignment`, `bPKMode`, `dwMountVnum` | `49 B` |
| `TPacketGCCharacterUpdate` | `HEADER_GC_CHARACTER_UPDATE (19)` | `dwVID`, `awPart[CHR_EQUIPPART_NUM]`, `bMovingSpeed`, `bAttackSpeed`, `bStateFlag`, `dwAffectFlag[2]`, `dwGuildID`, `sAlignment`, `bPKMode`, `dwMountVnum` | `46 B` |
| `TPacketGCItemSet` | `HEADER_GC_ITEM_SET (21)` | `Cell`, `vnum` (`uint32_t`), `count` (`uint16_t`), `flags` (`uint32_t`), `anti_flags` (`uint32_t`), `highlight` (`bool`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `54 B` |
| `TPacketGCItemUpdate` | `HEADER_GC_ITEM_UPDATE (25)` | `Cell`, `count` (`uint16_t`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `41 B` |
| `TShopItemData` | `HEADER_GC_SHOP (38)` | `vnum` (`uint32_t`), `price` (`YANG` / `int64_t`), `count` (`uint16_t`), `display_pos` (`BYTE`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `48 B` |
| `TShopItemTable` | `HEADER_CG_MYSHOP (55)` | `vnum` (`uint32_t`), `count` (`uint16_t`), `pos` (`TItemPos`), `price` (`YANG` / `int64_t`), `display_pos` (`BYTE`) | `18 B` |
| `TPacketCGExchange` | `HEADER_CG_EXCHANGE (27)` | `sub_header` (`BYTE`), `arg1` (`YANG` / `int64_t`), `arg2` (`BYTE`), `Pos` (`TItemPos`) | `13 B` |
| `TPacketGCExchange` | `HEADER_GC_EXCHANGE (42)` | `sub_header` (`BYTE`), `is_me` (`BYTE`), `arg1` (`YANG` / `int64_t`), `arg2` (`TItemPos`), `arg3` (`uint32_t`), `alSockets[3]` (`int32_t[3]`), `aAttr[7]` | `50 B` |
| `TPacketGCLandList` | `HEADER_GC_LAND_LIST (130)` | `TLandPacketElement`: `dwID` (`uint32_t`), `x`, `y`, `width`, `height` (`int32_t`), `dwGuildID` (`uint32_t`) | `3 B + N * 24 B` |
| `TPacketGCAffectAdd` | `HEADER_GC_AFFECT_ADD (126)` | `TPacketAffectElement`: `dwType` (`uint32_t`), `bApplyOn` (`BYTE`), `lApplyValue` (`int32_t`), `dwFlag` (`uint32_t`), `lDuration` (`int32_t`), `lSPCost` (`int32_t`) | `22 B` |
| `TPacketGCAffectRemove` | `HEADER_GC_AFFECT_REMOVE (127)` | `dwType` (`uint32_t`), `bApplyOn` (`BYTE`) | `6 B` |
| `TPacketGCQuestConfirm` | `HEADER_GC_QUEST_CONFIRM (46)` | `msg[64]`, `timeout` (`int32_t`), `requestPID` (`uint32_t`) | `73 B` |
| `TPacketGCGuildWar` | `HEADER_GC_GUILD` alt paketi | `dwGuildSelf` (`uint32_t`), `dwGuildOpp` (`uint32_t`), `bType` (`BYTE`), `bWarState` (`BYTE`) | `10 B` |

---

## 5. GELECEK CLIENT x64 UYARLAMA TEST LİSTESİ

Client modernizasyonu aşaması başlatıldığında aşağıdaki testlerin %100 geçmesi zorunludur:

1. **Derleme Zamanı `static_assert` Testleri (`client/src/UserInterface/Packet.h`):**
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
2. **Sürüm El Sıkışması Testi (`MT2_PROTOCOL_VERSION_X64`):**
   - `adwClientKey[3] == 0x20090064U` ile `AUTH` ve `GAME` bağlantısının kabul edilmesi.
   - `adwClientKey[3] == 0x20090032U` (eski istemci) ile `WRONGVER` yanıtının alınması.
3. **64-Bit Ekonomi ve Zaman Damgası Canlı Oturum Testi (`> 2^32`):**
   - `POINT_GOLD = 75,000,000,000` (`75B Yang`) değerinin `TPacketGCPoints` ve `TPacketGCPointChange` üzerinden Python arayüzünde kesilmeden görüntülenmesi.
   - `TPacketCGSafeboxMoney` / `TPacketGCSafeboxMoneyChange` ile `50,000,000,000` Yang depo transferi.
   - `TPacketGCRefineInformation` ile `> 2^31` Yang yükseltme maliyetinin negatifleşmeden gösterilmesi.
   - `TPacketGCItemShopBalance` ve `TItemShopItem` ile `> 2^32` Dragon Coin/Mark bakiyesi ve ürün fiyatlarının doğru okunması.
   - `TPacketGCTime` ile `> 2038` (`> 2,147,483,647`) zaman damgalarının taşmadan işlenmesi.
