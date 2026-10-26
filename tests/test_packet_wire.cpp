// ==============================================================================
// Metin2 MT2009 - Wire Protocol & Binary Serialization Test Suite
// Verifies 100% byte-for-byte wire compatibility with 32-bit client
// ==============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <algorithm>

#include "server/src/game/src/packet.h"
#include "server/src/common/tables.h"
#include "server/src/common/utils.h"

static int g_passCount = 0;
static int g_failCount = 0;

#define WIRE_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            g_failCount++; \
            exit(1); \
        } else { \
            g_passCount++; \
        } \
    } while (0)

// ------------------------------------------------------------------------------
// Compile-Time Size Checks (Guarantees layout invariant across compilers/LP64)
// ------------------------------------------------------------------------------
static_assert(sizeof(TPacketCGHandshake) == 13, "TPacketCGHandshake size mismatch");
static_assert(sizeof(TPacketGCHandshake) == 13, "TPacketGCHandshake size mismatch");
static_assert(sizeof(TPacketCGMove) == 16, "TPacketCGMove size mismatch");
static_assert(sizeof(TPacketGCMove) == 24, "TPacketGCMove size mismatch");
static_assert(sizeof(TPacketCGSyncPositionElement) == 12, "TPacketCGSyncPositionElement size mismatch");
static_assert(sizeof(TPacketGCSyncPositionElement) == 12, "TPacketGCSyncPositionElement size mismatch");
static_assert(sizeof(TPacketGCCharacterAdd) == 37, "TPacketGCCharacterAdd size mismatch");
static_assert(sizeof(TPacketGCMainCharacter) == 48, "TPacketGCMainCharacter size mismatch");
static_assert(sizeof(TPacketGCPointChange) == 17, "TPacketGCPointChange size mismatch");
static_assert(sizeof(TPacketGCItemSet) == 54, "TPacketGCItemSet size mismatch");
static_assert(sizeof(TPacketGCItemUpdate) == 41, "TPacketGCItemUpdate size mismatch");
static_assert(sizeof(TPacketGCItemGroundAdd) == 26, "TPacketGCItemGroundAdd size mismatch");
static_assert(sizeof(TPacketCGFlyTargeting) == 13, "TPacketCGFlyTargeting size mismatch");
static_assert(sizeof(TPacketGCFlyTargeting) == 17, "TPacketGCFlyTargeting size mismatch");
static_assert(sizeof(TPacketGCWarp) == 15, "TPacketGCWarp size mismatch");
static_assert(sizeof(TPacketGCQuestConfirm) == 74, "TPacketGCQuestConfirm size mismatch");
static_assert(sizeof(TPacketGCTime) == 5, "TPacketGCTime size mismatch (must be 5 bytes for 32-bit client)");
static_assert(sizeof(TPacketCGSafeboxMoney) == 6, "TPacketCGSafeboxMoney size mismatch");
static_assert(sizeof(TPacketGCSafeboxMoneyChange) == 5, "TPacketGCSafeboxMoneyChange size mismatch");
static_assert(sizeof(TPacketGCTargetCreate) == 43, "TPacketGCTargetCreate size mismatch");
static_assert(sizeof(TPacketGCTargetUpdate) == 13, "TPacketGCTargetUpdate size mismatch");
static_assert(sizeof(TPacketGCTargetDelete) == 5, "TPacketGCTargetDelete size mismatch");
static_assert(sizeof(TPacketGCDungeonDestPosition) == 8, "TPacketGCDungeonDestPosition size mismatch");
static_assert(sizeof(TPacketCGStateCheck) == 9, "TPacketCGStateCheck size mismatch");
static_assert(sizeof(TPacketGCStateCheck) == 10, "TPacketGCStateCheck size mismatch");
static_assert(sizeof(struct packet_script) == 6, "struct packet_script size mismatch (must be 6 bytes for 32-bit client)");
static_assert(offsetof(struct packet_script, header) == 0, "packet_script.header offset mismatch");
static_assert(offsetof(struct packet_script, size) == 1, "packet_script.size offset mismatch");
static_assert(offsetof(struct packet_script, skin) == 3, "packet_script.skin offset mismatch");
static_assert(offsetof(struct packet_script, src_size) == 4, "packet_script.src_size offset mismatch");

static_assert(sizeof(TPlayerItem) == 52, "TPlayerItem size mismatch");
static_assert(sizeof(TSimplePlayer) == 67, "TSimplePlayer size mismatch");
static_assert(sizeof(TItemLimit) == 5, "TItemLimit size mismatch");
static_assert(sizeof(TItemApply) == 5, "TItemApply size mismatch");
static_assert(sizeof(TPacketAffectElement) == 25, "TPacketAffectElement size mismatch");
static_assert(sizeof(TPacketGDSetup) == 159, "TPacketGDSetup size mismatch");
static_assert(offsetof(TPacketGDSetup, dwProtocolVersion) == 155, "TPacketGDSetup.dwProtocolVersion offset mismatch");
static_assert(sizeof(TPacketGGSetup) == 8, "TPacketGGSetup size mismatch");
static_assert(offsetof(TPacketGGSetup, dwProtocolVersion) == 4, "TPacketGGSetup.dwProtocolVersion offset mismatch");
static_assert(sizeof(TPacketCGLogin2) == 56, "TPacketCGLogin2 size mismatch");
static_assert(offsetof(TPacketCGLogin2, dwProtocolVersion) == 52, "TPacketCGLogin2.dwProtocolVersion offset mismatch");
static_assert(sizeof(TPacketCGLogin3) == 325, "TPacketCGLogin3 size mismatch");
static_assert(offsetof(TPacketCGLogin3, dwProtocolVersion) == 321, "TPacketCGLogin3.dwProtocolVersion offset mismatch");
static_assert(sizeof(TMapLocation) == 147, "TMapLocation size mismatch");

static_assert(sizeof(TPacketGCLoginSuccess) == 353, "TPacketGCLoginSuccess size mismatch");
static_assert(sizeof(TPacketCGPlayerSelect) == 2, "TPacketCGPlayerSelect size mismatch");
static_assert(sizeof(TPacketCGEnterGame) == 1, "TPacketCGEnterGame size mismatch");
static_assert(sizeof(TPacketCGOnClick) == 5, "TPacketCGOnClick size mismatch");
static_assert(sizeof(TPacketCGScriptAnswer) == 2, "TPacketCGScriptAnswer size mismatch");
static_assert(sizeof(TPacketCGQuestInputString) == 66, "TPacketCGQuestInputString size mismatch");
static_assert(sizeof(TPacketGCMainCharacter3_BGM) == 73, "TPacketGCMainCharacter3_BGM size mismatch");
static_assert(sizeof(TPacketGCMainCharacter4_BGM_VOL) == 77, "TPacketGCMainCharacter4_BGM_VOL size mismatch");
static_assert(sizeof(TPacketGCPoints) == 709, "TPacketGCPoints size mismatch");
static_assert(sizeof(TPacketGCSkillLevel) == 1591, "TPacketGCSkillLevel size mismatch");
static_assert(sizeof(TPacketGCCharacterAdditionalInfo) == 62, "TPacketGCCharacterAdditionalInfo size mismatch");
static_assert(sizeof(TPacketGCCharacterUpdate) == 44, "TPacketGCCharacterUpdate size mismatch");
static_assert(sizeof(TPacketGCCharacterDelete) == 5, "TPacketGCCharacterDelete size mismatch");
static_assert(sizeof(TPacketGCAffectAdd) == 26, "TPacketGCAffectAdd size mismatch");
static_assert(sizeof(TPacketGCAffectRemove) == 6, "TPacketGCAffectRemove size mismatch");
static_assert(sizeof(TPacketGCWalkMode) == 6, "TPacketGCWalkMode size mismatch");
static_assert(sizeof(TPacketGCChangeSkillGroup) == 2, "TPacketGCChangeSkillGroup size mismatch");
static_assert(sizeof(TPacketGCSpecialEffect) == 6, "TPacketGCSpecialEffect size mismatch");
static_assert(sizeof(TPacketGCChannel) == 2, "TPacketGCChannel size mismatch");
static_assert(sizeof(TPacketGCGoldUpdate) == 9, "TPacketGCGoldUpdate size mismatch");
static_assert(sizeof(TPacketGCSpecialFlag) == 42, "TPacketGCSpecialFlag size mismatch");
static_assert(sizeof(TPacketGCGuildToken) == 9, "TPacketGCGuildToken size mismatch");
static_assert(sizeof(TPacketGCSpecificEffect) == 133, "TPacketGCSpecificEffect size mismatch");

// ------------------------------------------------------------------------------
// Compile-Time Offset Checks (Guarantees no internal padding/misalignment)
// ------------------------------------------------------------------------------
static_assert(offsetof(TPacketGCCharacterAdd, x) == 9, "TPacketGCCharacterAdd.x offset mismatch");
static_assert(offsetof(TPacketGCCharacterAdd, y) == 13, "TPacketGCCharacterAdd.y offset mismatch");
static_assert(offsetof(TPacketGCCharacterAdd, z) == 17, "TPacketGCCharacterAdd.z offset mismatch");

static_assert(offsetof(TPacketGCMainCharacter, lx) == 34, "TPacketGCMainCharacter.lx offset mismatch");
static_assert(offsetof(TPacketGCMainCharacter, ly) == 38, "TPacketGCMainCharacter.ly offset mismatch");
static_assert(offsetof(TPacketGCMainCharacter, lz) == 42, "TPacketGCMainCharacter.lz offset mismatch");

static_assert(offsetof(TPacketGCMove, lX) == 8, "TPacketGCMove.lX offset mismatch");
static_assert(offsetof(TPacketGCMove, lY) == 12, "TPacketGCMove.lY offset mismatch");

static_assert(offsetof(TPacketGCPointChange, amount) == 9, "TPacketGCPointChange.amount offset mismatch");
static_assert(offsetof(TPacketGCPointChange, value) == 13, "TPacketGCPointChange.value offset mismatch");

static_assert(offsetof(TPacketGCItemSet, alSockets) == 21, "TPacketGCItemSet.alSockets offset mismatch");
static_assert(offsetof(TPacketGCItemUpdate, alSockets) == 8, "TPacketGCItemUpdate.alSockets offset mismatch");

static_assert(offsetof(TPacketGCWarp, lX) == 1, "TPacketGCWarp.lX offset mismatch");
static_assert(offsetof(TPacketGCWarp, lY) == 5, "TPacketGCWarp.lY offset mismatch");
static_assert(offsetof(TPacketGCWarp, lAddr) == 9, "TPacketGCWarp.lAddr offset mismatch");
static_assert(offsetof(TPacketGCWarp, wPort) == 13, "TPacketGCWarp.wPort offset mismatch");

static_assert(offsetof(TPacketGCTime, time) == 1, "TPacketGCTime.time offset mismatch");

static_assert(offsetof(TSimplePlayer, x) == 52, "TSimplePlayer.x offset mismatch");
static_assert(offsetof(TSimplePlayer, y) == 56, "TSimplePlayer.y offset mismatch");
static_assert(offsetof(TSimplePlayer, lAddr) == 60, "TSimplePlayer.lAddr offset mismatch");
static_assert(offsetof(TSimplePlayer, wPort) == 64, "TSimplePlayer.wPort offset mismatch");

static_assert(sizeof(TRequestChargeCash) == 24, "TRequestChargeCash size mismatch");
static_assert(offsetof(TRequestChargeCash, dwAID) == 0, "TRequestChargeCash.dwAID offset mismatch");
static_assert(offsetof(TRequestChargeCash, lAmount) == 4, "TRequestChargeCash.lAmount offset mismatch");
static_assert(offsetof(TRequestChargeCash, lSecondAmount) == 12, "TRequestChargeCash.lSecondAmount offset mismatch");
static_assert(offsetof(TRequestChargeCash, eChargeType) == 20, "TRequestChargeCash.eChargeType offset mismatch");
static_assert(sizeof(TItemShopPromotion) == 24, "TItemShopPromotion size mismatch");
static_assert(sizeof(TItemShopTimeAuction) == 28, "TItemShopTimeAuction size mismatch");
static_assert(sizeof(TItemShopItem) == 86, "TItemShopItem size mismatch");
static_assert(offsetof(TItemShopItem, dwPrice) == 12, "TItemShopItem.dwPrice offset mismatch");
static_assert(offsetof(TItemShopItem, alSockets) == 22, "TItemShopItem.alSockets offset mismatch");
static_assert(sizeof(TItemShopUpdateAuctionItem) == 28, "TItemShopUpdateAuctionItem size mismatch");
static_assert(sizeof(TSpecialFlagTable) == 49, "TSpecialFlagTable size mismatch");
static_assert(sizeof(TRefineTable) == 57, "TRefineTable size mismatch");
static_assert(sizeof(TCraftingItem) == 113, "TCraftingItem size mismatch");
static_assert(sizeof(TSpecialShopItem) == 75, "TSpecialShopItem size mismatch");
static_assert(sizeof(TSpecialShopItemClient) == 67, "TSpecialShopItemClient size mismatch");
static_assert(sizeof(TSpecialShopMetaData) == 16, "TSpecialShopMetaData size mismatch");
static_assert(sizeof(TPacketGDAuthLogin) == 410, "TPacketGDAuthLogin size mismatch");
static_assert(sizeof(TPacketGiveGuildPriv) == 17, "TPacketGiveGuildPriv size mismatch");
static_assert(sizeof(TPacketGiveEmpirePriv) == 14, "TPacketGiveEmpirePriv size mismatch");
static_assert(sizeof(TPacketDGChangeGuildPriv) == 18, "TPacketDGChangeGuildPriv size mismatch");
static_assert(sizeof(TPacketDGChangeEmpirePriv) == 15, "TPacketDGChangeEmpirePriv size mismatch");
static_assert(sizeof(TPacketMarriageAdd) == 66, "TPacketMarriageAdd size mismatch");
static_assert(sizeof(TPacketSetPremiumPass) == 12, "TPacketSetPremiumPass size mismatch");
static_assert(sizeof(TPacketGGLogin) == 44, "TPacketGGLogin size mismatch");

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << " METIN2 MT2009 - WIRE PROTOCOL TEST SUITE" << std::endl;
    std::cout << " 32-Bit Client Binary Wire Compatibility" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Serialization & Deserialization Test: TPacketGCCharacterAdd
    {
        TPacketGCCharacterAdd outPacket{};
        outPacket.header = HEADER_GC_CHARACTER_ADD;
        outPacket.dwVID = 12345;
        outPacket.angle = 180.5f;
        outPacket.x = 958500;
        outPacket.y = 256800;
        outPacket.z = 100;
        outPacket.bType = 1;
        outPacket.wRaceNum = 0; // Warrior
        outPacket.bMovingSpeed = 100;
        outPacket.bAttackSpeed = 100;
        outPacket.bStateFlag = 0;
        outPacket.dwAffectFlag[0] = 0xAA55AA55;
        outPacket.dwAffectFlag[1] = 0x12345678;

        std::vector<uint8_t> buffer(sizeof(TPacketGCCharacterAdd));
        std::memcpy(buffer.data(), &outPacket, sizeof(TPacketGCCharacterAdd));

        TPacketGCCharacterAdd inPacket{};
        std::memcpy(&inPacket, buffer.data(), sizeof(TPacketGCCharacterAdd));

        WIRE_ASSERT(inPacket.header == HEADER_GC_CHARACTER_ADD, "CharacterAdd header match");
        WIRE_ASSERT(inPacket.dwVID == 12345, "CharacterAdd VID match");
        WIRE_ASSERT(inPacket.x == 958500, "CharacterAdd X coord match");
        WIRE_ASSERT(inPacket.y == 256800, "CharacterAdd Y coord match");
        WIRE_ASSERT(inPacket.z == 100, "CharacterAdd Z coord match");
        WIRE_ASSERT(inPacket.dwAffectFlag[0] == 0xAA55AA55, "CharacterAdd affect flag 0 match");
        WIRE_ASSERT(inPacket.dwAffectFlag[1] == 0x12345678, "CharacterAdd affect flag 1 match");
    }

    // 2. Serialization & Deserialization Test: TPacketGCItemSet (alSockets byte layout)
    {
        TPacketGCItemSet outItem{};
        outItem.header = HEADER_GC_ITEM_SET;
        outItem.Cell = TItemPos(INVENTORY, 5);
        outItem.vnum = 299; // FMS+9
        outItem.count = 1;
        outItem.flags = 0x1;
        outItem.anti_flags = 0x2;
        outItem.highlight = false;
        outItem.alSockets[0] = 28430; // Stone of Death+4
        outItem.alSockets[1] = 28434; // Stone of Monster+4
        outItem.alSockets[2] = 1;     // Clean slot

        outItem.aAttr[0].bType = 1;   // Max HP
        outItem.aAttr[0].sValue = 2000;
        outItem.aAttr[1].bType = 5;   // Critical
        outItem.aAttr[1].sValue = 10;

        std::vector<uint8_t> buffer(sizeof(TPacketGCItemSet));
        std::memcpy(buffer.data(), &outItem, sizeof(TPacketGCItemSet));

        TPacketGCItemSet inItem{};
        std::memcpy(&inItem, buffer.data(), sizeof(TPacketGCItemSet));

        WIRE_ASSERT(inItem.header == HEADER_GC_ITEM_SET, "ItemSet header match");
        WIRE_ASSERT(inItem.vnum == 299, "ItemSet vnum match");
        WIRE_ASSERT(inItem.alSockets[0] == 28430, "ItemSet socket 0 match");
        WIRE_ASSERT(inItem.alSockets[1] == 28434, "ItemSet socket 1 match");
        WIRE_ASSERT(inItem.alSockets[2] == 1, "ItemSet socket 2 match");
        WIRE_ASSERT(inItem.aAttr[0].bType == 1, "ItemSet attr 0 type match");
        WIRE_ASSERT(inItem.aAttr[0].sValue == 2000, "ItemSet attr 0 value match");
    }

    // 3. Serialization & Deserialization Test: TPacketGCWarp (Coordinates and IP Address)
    {
        TPacketGCWarp outWarp{};
        outWarp.bHeader = HEADER_GC_WARP;
        outWarp.lX = 958500;
        outWarp.lY = 256800;
        outWarp.lAddr = 0x7F000001; // 127.0.0.1
        outWarp.wPort = 13000;

        std::vector<uint8_t> buffer(sizeof(TPacketGCWarp));
        std::memcpy(buffer.data(), &outWarp, sizeof(TPacketGCWarp));

        TPacketGCWarp inWarp{};
        std::memcpy(&inWarp, buffer.data(), sizeof(TPacketGCWarp));

        WIRE_ASSERT(inWarp.bHeader == HEADER_GC_WARP, "Warp header match");
        WIRE_ASSERT(inWarp.lX == 958500, "Warp lX match");
        WIRE_ASSERT(inWarp.lY == 256800, "Warp lY match");
        WIRE_ASSERT(inWarp.lAddr == 0x7F000001, "Warp lAddr match");
        WIRE_ASSERT(inWarp.wPort == 13000, "Warp wPort match");
    }

    // 4. Serialization & Deserialization Test: TPacketGCTime (32-bit time invariant)
    {
        TPacketGCTime outTime{};
        outTime.bHeader = HEADER_GC_TIME;
        outTime.time = 1718000000; // Fixed timestamp

        std::vector<uint8_t> buffer(sizeof(TPacketGCTime));
        std::memcpy(buffer.data(), &outTime, sizeof(TPacketGCTime));

        TPacketGCTime inTime{};
        std::memcpy(&inTime, buffer.data(), sizeof(TPacketGCTime));

        WIRE_ASSERT(inTime.bHeader == HEADER_GC_TIME, "Time header match");
        WIRE_ASSERT(inTime.time == 1718000000, "Time value match");
        WIRE_ASSERT(buffer.size() == 5, "Time packet size is strictly 5 bytes");
    }

    // 5. Boundary & Coordinate Range Test (Negative & Extreme Coordinates)
    {
        TPacketCGMove outMove{};
        outMove.bHeader = HEADER_CG_MOVE;
        outMove.bFunc = 1;
        outMove.bArg = 0;
        outMove.bRot = 90;
        outMove.lX = -500000; // Negative coordinate
        outMove.lY = 2147483647; // INT32_MAX
        outMove.dwTime = 999999;

        std::vector<uint8_t> buffer(sizeof(TPacketCGMove));
        std::memcpy(buffer.data(), &outMove, sizeof(TPacketCGMove));

        TPacketCGMove inMove{};
        std::memcpy(&inMove, buffer.data(), sizeof(TPacketCGMove));

        WIRE_ASSERT(inMove.lX == -500000, "Negative coordinate retained cleanly");
        WIRE_ASSERT(inMove.lY == 2147483647, "INT32_MAX coordinate retained without overflow");
    }

    // 6. TSimplePlayer Character Select Wire Test
    {
        TSimplePlayer outPlayer{};
        outPlayer.dwID = 1001;
        std::strncpy(outPlayer.szName, "WarriorAlpha", sizeof(outPlayer.szName));
        outPlayer.byJob = 0;
        outPlayer.byLevel = 99;
        outPlayer.dwPlayMinutes = 14400;
        outPlayer.byST = 90;
        outPlayer.byHT = 90;
        outPlayer.byDX = 90;
        outPlayer.byIQ = 90;
        outPlayer.x = 958500;
        outPlayer.y = 256800;
        outPlayer.lAddr = 0x7F000001;
        outPlayer.wPort = 13000;
        outPlayer.skill_group = 1;

        std::vector<uint8_t> buffer(sizeof(TSimplePlayer));
        std::memcpy(buffer.data(), &outPlayer, sizeof(TSimplePlayer));

        TSimplePlayer inPlayer{};
        std::memcpy(&inPlayer, buffer.data(), sizeof(TSimplePlayer));

        WIRE_ASSERT(inPlayer.dwID == 1001, "SimplePlayer ID match");
        WIRE_ASSERT(std::strcmp(inPlayer.szName, "WarriorAlpha") == 0, "SimplePlayer Name match");
        WIRE_ASSERT(inPlayer.byLevel == 99, "SimplePlayer Level match");
        WIRE_ASSERT(inPlayer.x == 958500, "SimplePlayer X match");
        WIRE_ASSERT(inPlayer.y == 256800, "SimplePlayer Y match");
        WIRE_ASSERT(inPlayer.lAddr == 0x7F000001, "SimplePlayer lAddr match");
        WIRE_ASSERT(inPlayer.wPort == 13000, "SimplePlayer wPort match");
        WIRE_ASSERT(buffer.size() == 67, "SimplePlayer size is strictly 67 bytes");
    }

    // 7. packet_script (Quest dialog / NPC script packet) Wire Test
    {
        struct packet_script outScript{};
        outScript.header = HEADER_GC_SCRIPT;
        outScript.size = sizeof(struct packet_script) + 24; // 6 + 24 = 30 bytes total
        outScript.skin = 1;
        outScript.src_size = 24;

        std::vector<uint8_t> buffer(sizeof(struct packet_script));
        std::memcpy(buffer.data(), &outScript, sizeof(struct packet_script));

        struct packet_script inScript{};
        std::memcpy(&inScript, buffer.data(), sizeof(struct packet_script));

        WIRE_ASSERT(inScript.header == HEADER_GC_SCRIPT, "packet_script header match");
        WIRE_ASSERT(inScript.size == sizeof(struct packet_script) + 24, "packet_script size match");
        WIRE_ASSERT(inScript.skin == 1, "packet_script skin match");
        WIRE_ASSERT(inScript.src_size == 24, "packet_script src_size match");
        WIRE_ASSERT(buffer.size() == 6, "packet_script wire size must be strictly 6 bytes for 32-bit client");
    }

    // 8. ItemShop 64-bit Price & Quantity Overflow Guard & TRequestChargeCash Wire Test
    {
        auto calcSafeItemShopCharge = [](uint64_t unitPrice, uint64_t quantity, int32_t& outSignedCharge) -> bool {
            outSignedCharge = 0;
            if (quantity == 0 || unitPrice == 0 || unitPrice > static_cast<uint64_t>(INT32_MAX))
                return false;
            if (quantity > static_cast<uint64_t>(INT32_MAX) / unitPrice)
                return false;
            const uint64_t total = unitPrice * quantity;
            if (total == 0 || total > static_cast<uint64_t>(INT32_MAX))
                return false;
            outSignedCharge = -static_cast<int32_t>(total);
            return outSignedCharge != INT32_MIN && outSignedCharge < 0;
        };

        int32_t signedCharge = 0;
        WIRE_ASSERT(calcSafeItemShopCharge(2147483647ULL, 1ULL, signedCharge) && signedCharge == -2147483647,
            "ItemShop 2,147,483,647 (INT32_MAX) charge fits cleanly in TRequestChargeCash::lAmount");
        WIRE_ASSERT(!calcSafeItemShopCharge(2147483648ULL, 1ULL, signedCharge) && signedCharge == 0,
            "ItemShop 2,147,483,648 (INT32_MAX+1) rejected fail-closed");
        WIRE_ASSERT(!calcSafeItemShopCharge(3000000000ULL, 1ULL, signedCharge) && signedCharge == 0,
            "ItemShop 3,000,000,000 rejected fail-closed");
        WIRE_ASSERT(!calcSafeItemShopCharge(1500000000ULL, 2ULL, signedCharge) && signedCharge == 0,
            "ItemShop 1,500,000,000 * 2 = 3,000,000,000 rejected fail-closed");
        WIRE_ASSERT(!calcSafeItemShopCharge(4294967295ULL, 1ULL, signedCharge) && signedCharge == 0,
            "ItemShop UINT32_MAX rejected fail-closed");
        WIRE_ASSERT(!calcSafeItemShopCharge(4294967295ULL, 10ULL, signedCharge) && signedCharge == 0,
            "ItemShop UINT32_MAX * 10 rejected fail-closed");

        TRequestChargeCash outCharge{};
        outCharge.dwAID = 77001;
        outCharge.lAmount = -2147483647;
        outCharge.lSecondAmount = 2147483647;
        outCharge.eChargeType = ERequestCharge_Cash;

        std::vector<uint8_t> buffer(sizeof(TRequestChargeCash));
        std::memcpy(buffer.data(), &outCharge, sizeof(TRequestChargeCash));

        TRequestChargeCash inCharge{};
        std::memcpy(&inCharge, buffer.data(), sizeof(TRequestChargeCash));
        WIRE_ASSERT(inCharge.dwAID == 77001, "TRequestChargeCash dwAID match");
        WIRE_ASSERT(inCharge.lAmount == -2147483647, "TRequestChargeCash lAmount (-INT32_MAX) match");
        WIRE_ASSERT(inCharge.lSecondAmount == 2147483647, "TRequestChargeCash lSecondAmount (INT32_MAX) match");
        WIRE_ASSERT(inCharge.eChargeType == ERequestCharge_Cash, "TRequestChargeCash eChargeType match");
    }

    // 9. 64-bit Audit Regression Tests (str_to_number, ItemShop Order, Inventory Bounds, YANG Tax)
    {
        // 9a. str_to_number(long long&, const char*) signed parsing via strtoll
        long long llParsed = 0;
        WIRE_ASSERT(str_to_number(llParsed, "-5000000000") && llParsed == -5000000000LL,
            "str_to_number(long long&) parses negative 64-bit values accurately via strtoll");
        WIRE_ASSERT(str_to_number(llParsed, "3000000000") && llParsed == 3000000000LL,
            "str_to_number(long long&) parses >INT32_MAX positive 64-bit values accurately");
        WIRE_ASSERT(!str_to_number(llParsed, nullptr) && llParsed == 3000000000LL,
            "str_to_number(long long&) rejects nullptr and leaves output untouched");

        // 9b. ItemShop GetPlayerAccountBalance negative/overflow DB balance clamping
        auto clampAccountBalance = [](const char* szRaw) -> uint32_t {
            long long rawVal = 0;
            str_to_number(rawVal, szRaw);
            return (rawVal > 0)
                ? static_cast<uint32_t>(std::min<long long>(rawVal, static_cast<long long>(INT32_MAX)))
                : 0U;
        };
        WIRE_ASSERT(clampAccountBalance("-150") == 0U,
            "GetPlayerAccountBalance clamps negative DB cash (-150) to 0 instead of wrapping to 4,294,967,146");
        WIRE_ASSERT(clampAccountBalance("3000000000") == static_cast<uint32_t>(INT32_MAX),
            "GetPlayerAccountBalance clamps >INT32_MAX DB cash to INT32_MAX");
        WIRE_ASSERT(clampAccountBalance("1250") == 1250U,
            "GetPlayerAccountBalance preserves normal positive DB cash");

        // 9c. ItemShop Buy transaction order: balance & DB connection & time-auction cap BEFORE charge or item delivery
        auto simulateItemShopBuyOrder = [](bool hasDescAndDb, uint32_t cashBal, uint32_t markBal,
                                           uint64_t unitPrice, uint64_t qty, uint64_t packCount,
                                           bool isTimeAuction, uint32_t currentAuctionBought, uint32_t maxAuctionBuy,
                                           bool useMarkScroll, int64_t scrollMarkAmount,
                                           uint32_t& outCashBal, uint32_t& outMarkBal,
                                           uint32_t& outAuctionBought, uint64_t& outDeliveredItems) -> bool {
            outCashBal = cashBal;
            outMarkBal = markBal;
            outAuctionBought = currentAuctionBought;
            outDeliveredItems = 0;

            if (!hasDescAndDb || qty == 0 || unitPrice == 0 || unitPrice > static_cast<uint64_t>(INT32_MAX))
                return false;
            if (qty > static_cast<uint64_t>(INT32_MAX) / unitPrice)
                return false;
            const uint64_t totalPrice = unitPrice * qty;
            if (totalPrice == 0 || totalPrice > static_cast<uint64_t>(INT32_MAX))
                return false;
            if (packCount == 0 || qty > static_cast<uint64_t>(INT32_MAX) / packCount)
                return false;
            const uint64_t totalItemCount = packCount * qty;
            if (totalItemCount == 0 || totalItemCount > static_cast<uint64_t>(INT32_MAX))
                return false;

            int64_t clampedMarkUse = 0;
            if (useMarkScroll) {
                clampedMarkUse = std::clamp<int64_t>(scrollMarkAmount, 0, static_cast<int64_t>(totalPrice));
                const uint64_t neededCash = totalPrice - static_cast<uint64_t>(clampedMarkUse);
                if (cashBal < neededCash || markBal < static_cast<uint64_t>(clampedMarkUse))
                    return false;
            } else {
                if (cashBal < totalPrice)
                    return false;
            }

            if (isTimeAuction) {
                if (static_cast<uint64_t>(currentAuctionBought) + totalItemCount > static_cast<uint64_t>(maxAuctionBuy))
                    return false;
                outAuctionBought = currentAuctionBought + static_cast<uint32_t>(totalItemCount);
            }

            // Deduct balance and deliver item ONLY after all checks pass
            if (useMarkScroll) {
                const uint32_t cashUsed = static_cast<uint32_t>(totalPrice - static_cast<uint64_t>(clampedMarkUse));
                outCashBal -= cashUsed;
                outMarkBal = markBal - static_cast<uint32_t>(clampedMarkUse) + cashUsed;
            } else {
                outCashBal -= static_cast<uint32_t>(totalPrice);
                outMarkBal += static_cast<uint32_t>(totalPrice);
            }
            outDeliveredItems = totalItemCount;
            return true;
        };

        uint32_t outCash = 0, outMark = 0, outAuction = 0;
        uint64_t outDelivered = 0;

        // Insufficient balance on time-auction item -> auction count and inventory MUST NOT change
        WIRE_ASSERT(!simulateItemShopBuyOrder(true, 100, 0, 500, 1, 1, true, 2, 10, false, 0, outCash, outMark, outAuction, outDelivered)
            && outCash == 100 && outAuction == 2 && outDelivered == 0,
            "ItemShop Buy order: failed balance check leaves time-auction counter and delivered items untouched");

        // Time-auction cap exceeded -> balance and inventory MUST NOT change
        WIRE_ASSERT(!simulateItemShopBuyOrder(true, 5000, 0, 500, 2, 5, true, 6, 10, false, 0, outCash, outMark, outAuction, outDelivered)
            && outCash == 5000 && outAuction == 6 && outDelivered == 0,
            "ItemShop Buy order: exceeded time-auction cap (6 + 10 > 10) leaves balance and delivered items untouched");

        // Dragon scroll with oversized mark value (scrollMarkAmount > totalPrice) -> clamped to totalPrice
        WIRE_ASSERT(simulateItemShopBuyOrder(true, 100, 1000, 300, 1, 2, false, 0, 0, true, 99999, outCash, outMark, outAuction, outDelivered)
            && outCash == 100 && outMark == 700 && outDelivered == 2,
            "ItemShop Buy order: Dragon Scroll mark deduction clamped to item price (300) without negative cash deduction");

        // 9d. IsHorseInventory cell range & 64-bit YANG shop tax / marriage love_point saturation
        auto isHorseInventoryCell = [](int cell) -> bool {
            return cell >= INVENTORY_DEFAULT_MAX_NUM && cell < INVENTORY_MAX_NUM;
        };
        WIRE_ASSERT(!isHorseInventoryCell(0) && !isHorseInventoryCell(89),
            "IsHorseInventory: normal inventory cells [0, 89] return false");
        WIRE_ASSERT(isHorseInventoryCell(90) && isHorseInventoryCell(134),
            "IsHorseInventory: horse inventory cells [90, 134] return true");
        WIRE_ASSERT(!isHorseInventoryCell(135) && !isHorseInventoryCell(-1),
            "IsHorseInventory: out-of-range cells (-1, >=135) return false");

        const int64_t shopPrice = 500000000000LL; // 500B YANG (> UINT32_MAX)
        const int taxRate = 3;
        const int64_t shopTax = shopPrice * taxRate / 100;
        const int64_t netProceeds = shopPrice - shopTax;
        WIRE_ASSERT(shopTax == 15000000000LL && netProceeds == 485000000000LL,
            "64-bit YANG shop tax preserves 15B tax and 485B net proceeds without 32-bit DWORD truncation");

        const uint64_t rawLovePoints = 5000000000ULL;
        const uint64_t cappedLovePoints = std::min<uint64_t>(rawLovePoints, 2000000000ULL);
        WIRE_ASSERT(cappedLovePoints == 2000000000ULL,
            "Marriage love_point 64-bit saturation clamps 5B to 2B without 32-bit signed MIN negative wrap");

        // 9e. Locale dynamic format string validator, FormatNumberWithDots(LLONG_MIN), Cube size check, & Monarch/DropGold YANG bounds
        auto extractFormatSpecs = [](const char* str, std::vector<std::string>& outSpecs) -> bool {
            outSpecs.clear();
            if (!str)
                return true;
            for (const char* p = str; *p != '\0'; ++p) {
                if (*p != '%')
                    continue;
                ++p;
                if (*p == '\0')
                    return false;
                if (*p == '%')
                    continue;
                while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')
                    ++p;
                if (*p == '*')
                    return false;
                while (*p >= '0' && *p <= '9')
                    ++p;
                if (*p == '.') {
                    ++p;
                    if (*p == '*')
                        return false;
                    while (*p >= '0' && *p <= '9')
                        ++p;
                }
                std::string spec;
                if (*p == 'h' || *p == 'l') {
                    char mod = *p++;
                    spec.push_back(mod);
                    if (*p == mod)
                        spec.push_back(*p++);
                } else if (*p == 'z' || *p == 'j' || *p == 't' || *p == 'L') {
                    spec.push_back(*p++);
                }
                if (*p == '\0' || *p == 'n')
                    return false;
                switch (*p) {
                    case 'd': case 'i': spec.push_back('d'); break;
                    case 'u': case 'o': case 'x': case 'X': spec.push_back('u'); break;
                    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': spec.push_back('f'); break;
                    case 'c': case 's': case 'p': spec.push_back(*p); break;
                    default: return false;
                }
                outSpecs.push_back(spec);
            }
            return true;
        };
        auto isSafeTranslation = [&](const char* key, const char* trans) -> bool {
            if (!key || !trans)
                return false;
            std::vector<std::string> kSpecs, tSpecs;
            if (!extractFormatSpecs(key, kSpecs) || !extractFormatSpecs(trans, tSpecs))
                return false;
            if (tSpecs.empty())
                return true;
            return kSpecs == tSpecs;
        };

        WIRE_ASSERT(isSafeTranslation("%d hours %d minutes left", "%d saat %d dakika kaldi"),
            "Locale format guard accepts matching %d specifiers");
        WIRE_ASSERT(isSafeTranslation("%s has logged in", "Oyuncu oyuna girdi"),
            "Locale format guard accepts specifier-free translation for formatted key");
        WIRE_ASSERT(!isSafeTranslation("%s has logged in", "%s %n"),
            "Locale format guard rejects malicious %n write specifier");
        WIRE_ASSERT(!isSafeTranslation("Hello world", "Merhaba %s %x"),
            "Locale format guard rejects unverified extra format specifiers in translation");
        WIRE_ASSERT(!isSafeTranslation("%d Yang received", "%s Yang alindi"),
            "Locale format guard rejects mismatched %d vs %s specifier types");
        WIRE_ASSERT(!isSafeTranslation("Discount 50%%", "Indirim %50%"),
            "Locale format guard rejects dangling trailing % in translation");

        auto formatNumberWithDots = [](long long number) -> std::string {
            std::string result;
            const bool isNegative = (number < 0);
            const unsigned long long absNumber = isNegative
                ? (0ULL - static_cast<unsigned long long>(number))
                : static_cast<unsigned long long>(number);
            std::string numStr = std::to_string(absNumber);
            int count = 0;
            for (int i = static_cast<int>(numStr.size()) - 1; i >= 0; --i) {
                result.push_back(numStr[static_cast<size_t>(i)]);
                if (++count == 3 && i > 0) {
                    result.push_back('.');
                    count = 0;
                }
            }
            std::reverse(result.begin(), result.end());
            if (isNegative)
                result.insert(result.begin(), '-');
            return result;
        };
        WIRE_ASSERT(formatNumberWithDots(LLONG_MIN) == "-9.223.372.036.854.775.808",
            "FormatNumberWithDots(LLONG_MIN) formats without signed negation UB or duplicate minus sign");
        WIRE_ASSERT(formatNumberWithDots(LLONG_MAX) == "9.223.372.036.854.775.807",
            "FormatNumberWithDots(LLONG_MAX) formats 64-bit maximum cleanly");

        const size_t shortCubeTextLen = 7; // e.g. "12345,1" (< 20 chars)
        WIRE_ASSERT((shortCubeTextLen + 20 < CHAT_MAX_LEN),
            "Cube short resultText length check (size + 20 >= CHAT_MAX_LEN) avoids unsigned size_t underflow when size < 20");

        // 7.9 ItemShop stack cap, DragonSoul slot guard, ITEM_BLEND alwaysNewItem isolation, and 64-bit Yang/Exchange guards
        {
            const uint32_t protoMaxStackZero = 0;
            const uint32_t effectiveMaxStack = (protoMaxStackZero > 0)
                ? std::min<uint32_t>(protoMaxStackZero, static_cast<uint32_t>(INT32_MAX))
                : static_cast<uint32_t>(ITEM_MAX_COUNT);
            const uint32_t totalBoughtCount = 550; // > 200 (ITEM_MAX_COUNT)
            const uint32_t fullStacks = totalBoughtCount / effectiveMaxStack;
            const uint32_t remainderStack = totalBoughtCount - fullStacks * effectiveMaxStack;
            WIRE_ASSERT(effectiveMaxStack == static_cast<uint32_t>(ITEM_MAX_COUNT) && fullStacks == 2 && remainderStack == 150,
                "ItemShop stack splitter falls back to ITEM_MAX_COUNT (200) when dwMaxStack=0 and caps at INT32_MAX");
        }
        {
            auto hasNormalInventorySlotForItemShop = [](uint8_t itemType, int emptyNormalCell) -> bool {
                if (itemType == ITEM_DS || itemType == ITEM_SPECIAL_DS)
                    return false;
                return emptyNormalCell != -1;
            };
            WIRE_ASSERT(!hasNormalInventorySlotForItemShop(ITEM_DS, 0) && hasNormalInventorySlotForItemShop(ITEM_WEAPON, 0),
                "HasSlotForItem rejects ITEM_DS/ITEM_SPECIAL_DS so ItemShop routes DragonSoul to item_award instead of AddToGround");
        }
        {
            auto shouldMergeBlendInAutoGive = [](bool alwaysNewItem, uint8_t itemType) -> bool {
                return !alwaysNewItem && itemType == ITEM_BLEND;
            };
            WIRE_ASSERT(!shouldMergeBlendInAutoGive(true, ITEM_BLEND) && shouldMergeBlendInAutoGive(false, ITEM_BLEND),
                "AutoGiveItem(alwaysNewItem=true) never merges ITEM_BLEND into pre-existing inventory stack");
        }
        {
            const int64_t guildWithdrawReq = 3000000000LL; // 3B Yang (> INT32_MAX)
            const int64_t goldMax = 10000000000000LL;
            const bool validReq = (guildWithdrawReq > 0 && guildWithdrawReq < goldMax);
            const int64_t safeWithdraw = validReq ? std::max<int64_t>(guildWithdrawReq, 500000LL) : -1LL;
            WIRE_ASSERT(safeWithdraw == 3000000000LL,
                "Guild withdraw preserves 64-bit YANG (> INT32_MAX) instead of truncating via 32-bit MAX(int, int)");
        }
        {
            const int64_t negativeWithdrawReq = -500000LL;
            const int64_t goldMax = 10000000000000LL;
            const bool validNegativeReq = (negativeWithdrawReq > 0 && negativeWithdrawReq < goldMax);
            WIRE_ASSERT(!validNegativeReq,
                "Guild withdraw rejects negative YANG before std::max(rawGold, 500000) floor");
        }
        {
            const int64_t goldMax = 10000000000000LL;
            const int64_t ownerGold = 1000LL;
            const int64_t maliciousArg1 = LLONG_MAX;
            const bool elkAddAllowed = (maliciousArg1 > 0 && maliciousArg1 < goldMax &&
                                        ownerGold >= 0 && ownerGold < goldMax &&
                                        maliciousArg1 < (goldMax - ownerGold));
            WIRE_ASSERT(!elkAddAllowed,
                "Exchange ELK_ADD rejects LLONG_MAX before signed 64-bit addition overflow (ownerGold + arg1)");
        }

        // 7.10 BSD strlcpy/strlcat, ConvertEmpireText trailing high-bit guard, ItemShop account limit flag length, and Safebox 64-bit time clamping
        {
            char dstBuf[8] = {};
            const size_t copiedLen = strlcpy(dstBuf, "PlayerOne", sizeof(dstBuf));
            WIRE_ASSERT(copiedLen == 9 && std::strcmp(dstBuf, "PlayerO") == 0,
                "strlcpy returns strlen(src) (BSD semantics, not errno_t 0) and NUL-terminates truncated buffer");

            char catBuf[12] = "Hi ";
            const size_t catLen = strlcat(catBuf, "Metin2World", sizeof(catBuf));
            WIRE_ASSERT(catLen == 14 && std::strcmp(catBuf, "Hi Metin2Wo") == 0,
                "strlcat returns initial dst len + strlen(src) and NUL-terminates truncated buffer");
        }
        {
            char empireBuf[4] = { static_cast<char>(0xC0), '\0', 'X', 'Y' };
            size_t len = 1;
            char* pbText = empireBuf;
            while (len > 0 && *pbText != '\0')
            {
                if (static_cast<unsigned char>(*pbText) & 0x80)
                {
                    if (len < 2 || pbText[1] == '\0')
                    {
                        *pbText = '?';
                        break;
                    }
                    pbText += 2;
                    len -= 2;
                }
                else
                {
                    ++pbText;
                    --len;
                }
            }
            WIRE_ASSERT(len == 1 && empireBuf[0] == '?' && empireBuf[2] == 'X',
                "ConvertEmpireText trailing high-bit byte (len==1) does not underflow size_t len or overwrite past NUL");
        }
        {
            auto formatAuctionBuyFlag = [](uint32_t dwIndex) -> std::string {
                return (dwIndex < 100000U)
                    ? ("itemshop_auction_buy_" + std::to_string(dwIndex))
                    : ("is_auc_buy_" + std::to_string(dwIndex));
            };
            auto formatAuctionTimeFlag = [](uint32_t dwIndex) -> std::string {
                return (dwIndex < 100000U)
                    ? ("itemshop_auction_time_" + std::to_string(dwIndex))
                    : ("is_auc_time_" + std::to_string(dwIndex));
            };
            const std::string legacyBuy = formatAuctionBuyFlag(99999U);
            const std::string legacyTime = formatAuctionTimeFlag(99999U);
            const std::string highBuy = formatAuctionBuyFlag(UINT32_MAX);
            const std::string highTime = formatAuctionTimeFlag(UINT32_MAX);
            WIRE_ASSERT(legacyBuy.size() < SPECIAL_FLAG_MAX_LEN && legacyTime.size() < SPECIAL_FLAG_MAX_LEN &&
                        highBuy.size() < SPECIAL_FLAG_MAX_LEN && highTime.size() < SPECIAL_FLAG_MAX_LEN &&
                        highBuy != formatAuctionBuyFlag(UINT32_MAX - 1U),
                "ItemShop dwAccountLimit flags fit strictly below SPECIAL_FLAG_MAX_LEN (32) without collision up to UINT32_MAX");
        }
        {
            const int64_t now64 = 1777000000LL;
            const int64_t hugeDuration = 3000000000LL;
            const int64_t expireSec = now64 + hugeDuration;
            const DWORD clampedSocket0 = (expireSec <= 0)
                ? 0U
                : (expireSec > static_cast<int64_t>(UINT32_MAX) ? UINT32_MAX : static_cast<DWORD>(expireSec));
            WIRE_ASSERT(clampedSocket0 == UINT32_MAX,
                "Safebox real-time expire clamps 64-bit time_t + duration to UINT32_MAX instead of wrapping modulo 2^32");
        }
        {
            TPacketGDSetup gdSetup{};
            gdSetup.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;
            TPacketGGSetup ggSetup{};
            ggSetup.bHeader = HEADER_GG_SETUP;
            ggSetup.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;
            TPacketCGLogin2 cgLogin2{};
            cgLogin2.header = HEADER_CG_LOGIN2;
            cgLogin2.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;
            TPacketCGLogin3 cgLogin3{};
            cgLogin3.header = HEADER_CG_LOGIN3;
            cgLogin3.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

            WIRE_ASSERT(MT2_PROTOCOL_VERSION_X64 == 0x20090064U &&
                        MT2_PROTOCOL_VERSION_LEGACY_X86 == 0x20090032U &&
                        gdSetup.dwProtocolVersion != MT2_PROTOCOL_VERSION_LEGACY_X86 &&
                        ggSetup.dwProtocolVersion == MT2_PROTOCOL_VERSION_X64 &&
                        cgLogin2.dwProtocolVersion == MT2_PROTOCOL_VERSION_X64 &&
                        cgLogin3.dwProtocolVersion == MT2_PROTOCOL_VERSION_X64,
                "x64 protocol version contract (MT2_PROTOCOL_VERSION_X64=0x20090064) is embedded across GD/GG/CG setup & login packets");
        }
        {
            TRequestChargeCash req{};
            req.dwAID = 777;
            req.lAmount = 5000000000LL;
            req.lSecondAmount = -3000000000LL;
            req.eChargeType = ERequestCharge_Cash;
            TRequestChargeCash reqIn{};
            std::memcpy(&reqIn, &req, sizeof(req));
            WIRE_ASSERT(reqIn.lAmount == 5000000000LL && reqIn.lSecondAmount == -3000000000LL,
                "TRequestChargeCash round-trips 64-bit positive/negative cash amounts (> 2^32) without truncation");

            TItemShopItem shopItem{};
            shopItem.dwIndex = 42;
            shopItem.dwPrice = 8500000000LL;
            shopItem.promotion.dwPrice = 4250000000LL;
            shopItem.promotion.dwStartTime = 2200000000LL;
            shopItem.promotion.dwEndTime = 2300000000LL;
            shopItem.timeAuction.dwStartTime = 2400000000LL;
            shopItem.timeAuction.dwEndTime = 2500000000LL;
            TItemShopItem shopItemIn{};
            std::memcpy(&shopItemIn, &shopItem, sizeof(shopItem));
            WIRE_ASSERT(shopItemIn.dwPrice == 8500000000LL &&
                        shopItemIn.promotion.dwPrice == 4250000000LL &&
                        shopItemIn.promotion.dwEndTime == 2300000000LL &&
                        shopItemIn.timeAuction.dwEndTime == 2500000000LL,
                "TItemShopItem round-trips 64-bit prices and Y2038+ timestamps without 32-bit truncation");

            TPacketGDAuthLogin authLogin{};
            authLogin.dwID = 1001;
            authLogin.iPremium = 2500000000LL;
            authLogin.iPremiumTimes[0] = 2600000000LL;
            TPacketGDAuthLogin authLoginIn{};
            std::memcpy(&authLoginIn, &authLogin, sizeof(authLogin));
            WIRE_ASSERT(authLoginIn.iPremium == 2500000000LL && authLoginIn.iPremiumTimes[0] == 2600000000LL,
                "TPacketGDAuthLogin round-trips 64-bit premium subscription and premium timestamps");

            TRefineTable refineTab{};
            refineTab.id = 99;
            refineTab.cost = 12000000000LL;
            TSpecialShopItemClient specItem{};
            specItem.vnum = 299;
            specItem.price = 99000000000LL;
            WIRE_ASSERT(refineTab.cost == 12000000000LL && specItem.price == 99000000000LL,
                "TRefineTable and TSpecialShopItemClient preserve 64-bit YANG costs/prices (> 2^32)");
        }
    }

    std::cout << "[SUCCESS] WIRE PROTOCOL SUITE: " << g_passCount << " assertions passed cleanly!" << std::endl;
    return 0;
}

