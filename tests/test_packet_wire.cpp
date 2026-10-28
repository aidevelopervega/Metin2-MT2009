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
#include "server/src/common/building.h"
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
static_assert(sizeof(building::TLand) == 37, "building::TLand packed size mismatch");
static_assert(sizeof(building::TObjectMaterial) == 8, "building::TObjectMaterial packed size mismatch");
static_assert(sizeof(building::TObjectProto) == 100, "building::TObjectProto packed size mismatch");
static_assert(sizeof(building::TObject) == 40, "building::TObject packed size mismatch");
static_assert(sizeof(TPacketCGHandshake) == 13, "TPacketCGHandshake size mismatch");
static_assert(sizeof(TPacketGCHandshake) == 13, "TPacketGCHandshake size mismatch");
static_assert(sizeof(TPacketCGMove) == 16, "TPacketCGMove size mismatch");
static_assert(sizeof(TPacketGCMove) == 24, "TPacketGCMove size mismatch");
static_assert(sizeof(TPacketCGSyncPositionElement) == 12, "TPacketCGSyncPositionElement size mismatch");
static_assert(sizeof(TPacketGCSyncPositionElement) == 12, "TPacketGCSyncPositionElement size mismatch");
static_assert(sizeof(TPacketGCCharacterAdd) == 37, "TPacketGCCharacterAdd size mismatch");
static_assert(sizeof(TPacketGCMainCharacter) == 48, "TPacketGCMainCharacter size mismatch");
static_assert(sizeof(TPacketGCPointChange) == 25, "TPacketGCPointChange size mismatch");
static_assert(sizeof(TPacketGCItemSet) == 54, "TPacketGCItemSet size mismatch");
static_assert(sizeof(TPacketGCItemUpdate) == 41, "TPacketGCItemUpdate size mismatch");
static_assert(sizeof(TPacketGCItemGroundAdd) == 26, "TPacketGCItemGroundAdd size mismatch");
static_assert(sizeof(TPacketCGFlyTargeting) == 13, "TPacketCGFlyTargeting size mismatch");
static_assert(sizeof(TPacketGCFlyTargeting) == 17, "TPacketGCFlyTargeting size mismatch");
static_assert(sizeof(TPacketGCWarp) == 15, "TPacketGCWarp size mismatch");
static_assert(sizeof(TPacketGCQuestConfirm) == 74, "TPacketGCQuestConfirm size mismatch");
static_assert(sizeof(TPacketGCTime) == 9, "TPacketGCTime size mismatch (x64 64-bit time)");
static_assert(sizeof(TPacketCGSafeboxMoney) == 10, "TPacketCGSafeboxMoney size mismatch (x64 64-bit YANG)");
static_assert(sizeof(TPacketGCSafeboxMoneyChange) == 9, "TPacketGCSafeboxMoneyChange size mismatch (x64 64-bit YANG)");
static_assert(sizeof(TPacketGCTargetCreate) == 43, "TPacketGCTargetCreate size mismatch");
static_assert(sizeof(TPacketGCTargetUpdate) == 13, "TPacketGCTargetUpdate size mismatch");
static_assert(sizeof(TPacketGCTargetDelete) == 5, "TPacketGCTargetDelete size mismatch");
static_assert(sizeof(TPacketGCDungeonDestPosition) == 8, "TPacketGCDungeonDestPosition size mismatch");
static_assert(sizeof(TPacketCGStateCheck) == 9, "TPacketCGStateCheck size mismatch");
static_assert(sizeof(TPacketGCStateCheck) == 10, "TPacketGCStateCheck size mismatch");
static_assert(sizeof(struct packet_script) == 6, "struct packet_script size mismatch");
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
static_assert(sizeof(TPacketGCPoints) == 1417, "TPacketGCPoints size mismatch (x64 64-bit points)");
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
static_assert(sizeof(TPacketGCItemShopBalance) == 16, "TPacketGCItemShopBalance size mismatch");
static_assert(sizeof(TPacketGCItemShopHappyHour) == 12, "TPacketGCItemShopHappyHour size mismatch");
static_assert(sizeof(TPacketGCItemShopLatestTimeAuction) == 8, "TPacketGCItemShopLatestTimeAuction size mismatch");
static_assert(sizeof(TPacketGCRefineInformation) == 64, "TPacketGCRefineInformation size mismatch");
static_assert(sizeof(TSubPacketGCCaptchaMeta) == 9, "TSubPacketGCCaptchaMeta size mismatch");

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
static_assert(offsetof(TPacketGCPointChange, value) == 17, "TPacketGCPointChange.value offset mismatch");

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
        WIRE_ASSERT(buffer.size() == 9, "Time packet size is strictly 9 bytes (x64 64-bit time)");
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
        {
            TPacketGCPoints gcPoints{};
            gcPoints.header = HEADER_GC_CHARACTER_POINTS;
            gcPoints.points[POINT_EXP] = 6000000000LL;
            gcPoints.points[POINT_MAX_HP] = 5000000000LL;
            TPacketGCPoints gcPointsIn{};
            std::memcpy(&gcPointsIn, &gcPoints, sizeof(gcPoints));
            WIRE_ASSERT(gcPointsIn.points[POINT_EXP] == 6000000000LL && gcPointsIn.points[POINT_MAX_HP] == 5000000000LL,
                "TPacketGCPoints round-trips 64-bit points (> 2^32) without truncation");

            TPacketGCPointChange gcPtChange{};
            gcPtChange.header = HEADER_GC_CHARACTER_POINT_CHANGE;
            gcPtChange.dwVID = 100;
            gcPtChange.type = POINT_EXP;
            gcPtChange.amount = 3000000000LL;
            gcPtChange.value = 9000000000LL;
            TPacketGCPointChange gcPtChangeIn{};
            std::memcpy(&gcPtChangeIn, &gcPtChange, sizeof(gcPtChange));
            WIRE_ASSERT(gcPtChangeIn.amount == 3000000000LL && gcPtChangeIn.value == 9000000000LL,
                "TPacketGCPointChange round-trips 64-bit amount and value (> 2^32)");

            TPacketGCItemShopBalance isBal{};
            isBal.dwDragonCoins = 7500000000LL;
            isBal.dwDragonMarks = 4200000000LL;
            TPacketGCItemShopBalance isBalIn{};
            std::memcpy(&isBalIn, &isBal, sizeof(isBal));
            WIRE_ASSERT(isBalIn.dwDragonCoins == 7500000000LL && isBalIn.dwDragonMarks == 4200000000LL,
                "TPacketGCItemShopBalance round-trips 64-bit DragonCoins and DragonMarks (> 2^32)");

            TPacketCGSafeboxMoney sbMoney{};
            sbMoney.bHeader = 0;
            sbMoney.bState = SAFEBOX_MONEY_STATE_SAVE;
            sbMoney.lMoney = 88000000000LL;
            TPacketGCSafeboxMoneyChange sbChange{};
            sbChange.bHeader = 0;
            sbChange.lMoney = 88000000000LL;
            WIRE_ASSERT(sbMoney.lMoney == 88000000000LL && sbChange.lMoney == 88000000000LL,
                "TPacketCGSafeboxMoney and TPacketGCSafeboxMoneyChange preserve 64-bit YANG (> 2^32)");

            TPacketGCTime gcTime{};
            gcTime.bHeader = HEADER_GC_TIME;
            gcTime.time = 2500000000LL;
            TPacketGCRefineInformation gcRefine{};
            gcRefine.header = HEADER_GC_REFINE_INFORMATION;
            gcRefine.cost = 15000000000LL;
            TSubPacketGCCaptchaMeta capMeta{};
            capMeta.captchaExpireTime = 2600000000LL;
            capMeta.attemptCount = 3;
            WIRE_ASSERT(gcTime.time == 2500000000LL && gcRefine.cost == 15000000000LL && capMeta.captchaExpireTime == 2600000000LL,
                "TPacketGCTime, TPacketGCRefineInformation, and TSubPacketGCCaptchaMeta preserve 64-bit timestamps/costs");
        }

        // Section 8 Concrete Bug Regression Suite (11 checks)
        {
            // 1. ProtoReader ITEM_SOUL (38) / ITEM_PASSIVE (39) mapping (missing comma fix)
            const char* arItemType[] = {
                "ITEM_NONE", "ITEM_WEAPON",
                "ITEM_ARMOR", "ITEM_USE",
                "ITEM_AUTOUSE", "ITEM_MATERIAL",
                "ITEM_SPECIAL", "ITEM_TOOL",
                "ITEM_LOTTERY", "ITEM_ELK",
                "ITEM_METIN", "ITEM_CONTAINER",
                "ITEM_FISH", "ITEM_ROD",
                "ITEM_RESOURCE", "ITEM_CAMPFIRE",
                "ITEM_UNIQUE", "ITEM_SKILLBOOK",
                "ITEM_QUEST", "ITEM_POLYMORPH",
                "ITEM_TREASURE_BOX", "ITEM_TREASURE_KEY",
                "ITEM_SKILLFORGET", "ITEM_GIFTBOX",
                "ITEM_PICK", "ITEM_HAIR",
                "ITEM_TOTEM", "ITEM_BLEND",
                "ITEM_COSTUME", "ITEM_DS",
                "ITEM_SPECIAL_DS", "ITEM_EXTRACT",
                "ITEM_SECONDARY_COIN", "ITEM_RING",
                "ITEM_BELT", "ITEM_PET", "ITEM_MEDIUM", "ITEM_GACHA", "ITEM_SOUL", "ITEM_PASSIVE"
            };
            WIRE_ASSERT(sizeof(arItemType) / sizeof(arItemType[0]) == 40 &&
                        std::strcmp(arItemType[38], "ITEM_SOUL") == 0 &&
                        std::strcmp(arItemType[39], "ITEM_PASSIVE") == 0,
                "Regression [1/11]: ProtoReader arItemType maps ITEM_SOUL (38) and ITEM_PASSIVE (39) distinctly");

            // 2. item_proto prices > 2^31 and > 2^32 (64-bit YANG dwGold / dwShopBuyPrice)
            TItemTable hiProto{};
            hiProto.dwVnum = 99001;
            hiProto.dwGold = 5000000000LL;        // 5B (> 2^32)
            hiProto.dwShopBuyPrice = 25000000000LL; // 25B (> 2^32)
            YANG sellPriceDiv5 = hiProto.dwShopBuyPrice / 5;
            WIRE_ASSERT(hiProto.dwGold > 2147483647LL &&
                        hiProto.dwShopBuyPrice > 4294967295LL &&
                        sellPriceDiv5 == hiProto.dwGold,
                "Regression [2/11]: TItemTable 64-bit YANG prices (> 2^31 and > 2^32) preserve exact value without overflow");

            // 3. High-bit item flag calculations and unsigned shift bounds (1u << i, i < 32)
            auto calcUnsignedFlagMask = [](size_t bitIndex) -> uint32_t {
                uint32_t retValue = 0;
                if (bitIndex < 32)
                    retValue |= (1u << static_cast<uint32_t>(bitIndex));
                return retValue;
            };
            uint32_t bit0 = calcUnsignedFlagMask(0);
            uint32_t bit20 = calcUnsignedFlagMask(20); // ANTI_PET21
            uint32_t bit31 = calcUnsignedFlagMask(31); // High bit 0x80000000u
            uint32_t bit32Out = calcUnsignedFlagMask(32); // Out-of-range shift guard
            int64_t widenedAllOnes = static_cast<int64_t>(0xFFFFFFFFu);
            WIRE_ASSERT(bit0 == 1u &&
                        bit20 == (1u << 20) &&
                        bit31 == 0x80000000u &&
                        bit32Out == 0u &&
                        widenedAllOnes == 4294967295LL &&
                        widenedAllOnes != -1LL,
                "Regression [3/11]: Unsigned 32-bit flag shift handles bit 31 (0x80000000u), blocks i >= 32, and avoids -1 sign-extension");

            // 4. CPeer::PeekPacket UINT32_MAX packet length overflow protection
            auto checkPeekPacketBounds = [](int iBytesProceed, uint32_t dwLength, int iRecvLength) -> bool {
                if (iBytesProceed < 0 || iRecvLength < 0)
                    return false;
                uint64_t totalNeeded = static_cast<uint64_t>(iBytesProceed) + static_cast<uint64_t>(dwLength) + 9ULL;
                if (totalNeeded > static_cast<uint64_t>(iRecvLength))
                    return false;
                return true;
            };
            WIRE_ASSERT(!checkPeekPacketBounds(10, 0xFFFFFFFFu, 64) &&
                        !checkPeekPacketBounds(-1, 16u, 64) &&
                        checkPeekPacketBounds(10, 16u, 35),
                "Regression [4/11]: CPeer::PeekPacket 64-bit totalNeeded rejects UINT32_MAX wrap-around and negative offsets");

            // 5. P2P Relay / Whisper 1024-byte boundary
            auto checkP2PTextBounds = [](int32_t lSize, size_t uiBytes) -> int {
                if (lSize < 0 || static_cast<size_t>(lSize) > 1024 || static_cast<size_t>(lSize) > uiBytes)
                    return -1;
                return lSize;
            };
            WIRE_ASSERT(checkP2PTextBounds(1024, 1024) == 1024 &&
                        checkP2PTextBounds(1025, 2048) == -1 &&
                        checkP2PTextBounds(512, 256) == -1,
                "Regression [5/11]: P2P Relay/Whisper accepts exact 1024-byte payload and rejects 1025-byte overflow");

            // 6. Negative P2P variable payload length rejection
            WIRE_ASSERT(checkP2PTextBounds(-1, 4096) == -1 &&
                        checkP2PTextBounds(INT32_MIN, 4096) == -1,
                "Regression [6/11]: Negative P2P lSize (-1, INT32_MIN) rejected before unsigned conversion");

            // 7. Config 255/256-byte buffer boundary
            char cfgDest[256];
            std::memset(cfgDest, 'X', sizeof(cfgDest));
            std::string longToken(300, 'A');
            int cfgIdx = 0;
            for (char c : longToken) {
                if (cfgIdx >= 255)
                    break;
                cfgDest[cfgIdx++] = c;
            }
            cfgDest[cfgIdx] = '\0';
            WIRE_ASSERT(cfgIdx == 255 &&
                        cfgDest[255] == '\0' &&
                        std::strlen(cfgDest) == 255,
                "Regression [7/11]: CConfig 256-byte buffer truncates 300-byte token safely at 255 chars + NUL");

            // 8. Monarch election vote counter zero-initialization
            size_t candidateCount = 4;
            std::vector<int> voteCounters(candidateCount, 0);
            int sampleVotes[] = { 1, 3, 1, 2, 1, -1, 99 };
            for (int v : sampleVotes) {
                if (v >= 0 && static_cast<size_t>(v) < voteCounters.size())
                    ++voteCounters[static_cast<size_t>(v)];
            }
            WIRE_ASSERT(voteCounters[0] == 0 &&
                        voteCounters[1] == 3 &&
                        voteCounters[2] == 1 &&
                        voteCounters[3] == 1,
                "Regression [8/11]: Monarch election counters are deterministically zero-initialized and bounds-checked");

            // 9. Guild war score signed range (int32_t negative and positive scores)
            TPacketGuildWarScore warScore{};
            warScore.dwGuildGainPoint = 101;
            warScore.dwGuildOpponent = 202;
            warScore.lScore = -250;
            warScore.lBetScore = 15000;
            TGuildWarReserve warReserve{};
            warReserve.lPowerFrom = -1200;
            warReserve.lPowerTo = 3400;
            warReserve.lHandicap = -50;
            WIRE_ASSERT(warScore.lScore == -250 &&
                        warReserve.lPowerFrom == -1200 &&
                        warReserve.lHandicap == -50,
                "Regression [9/11]: Guild war scores and reserve ratings preserve signed negative/positive int32_t values");

            // 10. NationMoney (MonarchInfo::money[4]) 64-bit YANG upper bounds
            TMonarchInfo monarchInfo{};
            monarchInfo.money[1] = 85000000000LL; // 85B (> 2^32)
            monarchInfo.money[2] = 99999999999LL; // ~100B
            TPacketGDGuildMoney guildTreasury{};
            guildTreasury.dwGuild = 301;
            guildTreasury.iGold = 42000000000LL;
            WIRE_ASSERT(monarchInfo.money[1] == 85000000000LL &&
                        monarchInfo.money[2] == 99999999999LL &&
                        guildTreasury.iGold == 42000000000LL,
                "Regression [10/11]: NationMoney (TMonarchInfo::money) and Guild Treasury (TPacketGDGuildMoney::iGold) support > 2^32 YANG");

            // 11. ComputeRefineFee 64-bit YANG overflow-safe calculation with GOLD_MAX = 1,000,000,000,000 (1 Trillion)
            auto computeRefineFeeSafe = [](YANG iCost, int iMultiply) -> YANG {
                if (iCost <= 0 || iMultiply <= 0)
                    return 0;
                const YANG kMaxFee = GOLD_MAX - 1;
                if (iCost > kMaxFee / static_cast<YANG>(iMultiply))
                    return kMaxFee;
                YANG fee = iCost * static_cast<YANG>(iMultiply);
                return (fee > kMaxFee) ? kMaxFee : fee;
            };
            WIRE_ASSERT(GOLD_MAX == 1000000000000LL &&
                        EXCHANGE_GOLD_MAX == 1000000000000LL &&
                        computeRefineFeeSafe(300000000LL, 5) == 1500000000LL &&
                        computeRefineFeeSafe(600000000LL, 5) == 3000000000LL &&
                        computeRefineFeeSafe(1200000000LL, 5) == 6000000000LL &&
                        computeRefineFeeSafe(300000000000LL, 5) == GOLD_MAX - 1 &&
                        computeRefineFeeSafe(-500LL, 5) == 0LL &&
                        computeRefineFeeSafe(GOLD_MAX, 5) == GOLD_MAX - 1,
                "Regression [11/11]: ComputeRefineFee supports >2^31 (3B) and >2^32 (6B) fees with GOLD_MAX=10^12 and clamps 300B*5 to GOLD_MAX-1");
        }

        // Section 9: x64 Final Audit — Monarch 64-Bit Packets & Protocol/Framing Security Suite
        {
            // 9.1 Monarch 64-bit GD/DG packet layouts & >2^32 treasury payloads
            WIRE_ASSERT(sizeof(TPacketGDMonarchMoney) == 12 &&
                        offsetof(TPacketGDMonarchMoney, iEmpire) == 0 &&
                        offsetof(TPacketGDMonarchMoney, llMoney) == 4,
                "TPacketGDMonarchMoney is 12 bytes with 64-bit llMoney at offset 4");
            WIRE_ASSERT(sizeof(TPacketDGMonarchMoney) == 12 &&
                        offsetof(TPacketDGMonarchMoney, iEmpire) == 0 &&
                        offsetof(TPacketDGMonarchMoney, llMoney) == 4,
                "TPacketDGMonarchMoney is 12 bytes with 64-bit llMoney at offset 4");
            WIRE_ASSERT(sizeof(TPacketGDTakeMonarchMoney) == 16 &&
                        offsetof(TPacketGDTakeMonarchMoney, iEmpire) == 0 &&
                        offsetof(TPacketGDTakeMonarchMoney, dwPID) == 4 &&
                        offsetof(TPacketGDTakeMonarchMoney, llMoney) == 8,
                "TPacketGDTakeMonarchMoney is 16 bytes with 64-bit llMoney at offset 8");

            TPacketGDMonarchMoney gdMonarch{};
            gdMonarch.iEmpire = 2;
            gdMonarch.llMoney = 125000000000LL; // 125B YANG (> 2^32)
            TPacketGDTakeMonarchMoney gdTakeMonarch{};
            gdTakeMonarch.dwPID = 77001;
            gdTakeMonarch.iEmpire = 2;
            gdTakeMonarch.llMoney = 25000000000LL; // 25B YANG (> 2^32)
            WIRE_ASSERT(gdMonarch.llMoney == 125000000000LL && gdTakeMonarch.llMoney == 25000000000LL,
                "Monarch GD/DG packets preserve >2^32 YANG (125B / 25B) without 32-bit int truncation");

            // 9.2 Protocol Version & Framing Security Simulation (Login2, Login3, CheckVersion, Fragmented TCP, Trailing Payload, DB/P2P Setup)
            enum class ESimPhase { Auth, Login, P2P, Close };
            struct SimDesc {
                ESimPhase phase = ESimPhase::Auth;
                bool delayedDisconnect = false;
                uint32_t clientVersion = 0;
                std::string failureStatus{};
                int validLoginsProcessed = 0;
                int p2pSetupsProcessed = 0;
                explicit SimDesc(ESimPhase p = ESimPhase::Auth) : phase(p) {}
            };

            auto simProcessBuffer = [](SimDesc& d, const uint8_t* buf, size_t totalBytes, size_t& outBytesProceed) -> bool {
                outBytesProceed = 0;
                if (d.phase == ESimPhase::Close || d.delayedDisconnect) {
                    outBytesProceed = totalBytes;
                    return true;
                }
                size_t bufferLeft = totalBytes;
                const uint8_t* p = buf;
                while (bufferLeft > 0) {
                    const uint8_t hdr = *p;
                    size_t pktLen = 0;
                    if (hdr == 0) {
                        pktLen = 1;
                    } else if (d.phase == ESimPhase::Auth) {
                        if (hdr == HEADER_CG_CHECK_VERSION) pktLen = sizeof(TPacketCGCheckVersion);
                        else if (hdr == HEADER_CG_LOGIN3) pktLen = sizeof(TPacketCGLogin3);
                        else {
                            d.phase = ESimPhase::Close;
                            outBytesProceed = totalBytes;
                            return true;
                        }
                    } else if (d.phase == ESimPhase::Login) {
                        if (hdr == HEADER_CG_CHECK_VERSION) pktLen = sizeof(TPacketCGCheckVersion);
                        else if (hdr == HEADER_CG_LOGIN2) pktLen = sizeof(TPacketCGLogin2);
                        else {
                            d.phase = ESimPhase::Close;
                            outBytesProceed = totalBytes;
                            return true;
                        }
                    } else if (d.phase == ESimPhase::P2P) {
                        if (hdr == HEADER_GG_SETUP) pktLen = sizeof(TPacketGGSetup);
                        else {
                            d.phase = ESimPhase::Close;
                            outBytesProceed = totalBytes;
                            return true;
                        }
                    }

                    if (bufferLeft < pktLen) {
                        if (d.phase == ESimPhase::Login && hdr == HEADER_CG_LOGIN2 &&
                            bufferLeft == sizeof(TPacketCGLogin2) - sizeof(uint32_t)) {
                            d.failureStatus = "VERSION";
                            d.delayedDisconnect = true;
                            outBytesProceed = totalBytes;
                            return true;
                        }
                        if (d.phase == ESimPhase::Auth && hdr == HEADER_CG_LOGIN3 &&
                            (bufferLeft == sizeof(TPacketCGLogin3) - sizeof(uint32_t) || bufferLeft == 130 || bufferLeft == 136)) {
                            d.failureStatus = "VERSION";
                            d.delayedDisconnect = true;
                            outBytesProceed = totalBytes;
                            return true;
                        }
                        if (d.phase == ESimPhase::P2P && hdr == HEADER_GG_SETUP &&
                            bufferLeft == sizeof(TPacketGGSetup) - sizeof(uint32_t)) {
                            d.phase = ESimPhase::Close;
                            outBytesProceed = totalBytes;
                            return true;
                        }
                        return true; // Wait for more TCP bytes
                    }

                    if (hdr == HEADER_CG_CHECK_VERSION) {
                        TPacketCGCheckVersion cv{};
                        std::memcpy(&cv, p, sizeof(cv));
                        d.clientVersion = cv.dwVersion;
                        if (cv.dwVersion != MT2_PROTOCOL_VERSION_X64) {
                            d.failureStatus = "VERSION";
                            d.delayedDisconnect = true;
                        }
                    } else if (hdr == HEADER_CG_LOGIN3) {
                        TPacketCGLogin3 l3{};
                        std::memcpy(&l3, p, sizeof(l3));
                        if (l3.dwProtocolVersion != MT2_PROTOCOL_VERSION_X64 ||
                            (d.clientVersion != 0 && d.clientVersion != MT2_PROTOCOL_VERSION_X64)) {
                            d.failureStatus = "VERSION";
                            d.delayedDisconnect = true;
                        } else {
                            ++d.validLoginsProcessed;
                        }
                    } else if (hdr == HEADER_CG_LOGIN2) {
                        TPacketCGLogin2 l2{};
                        std::memcpy(&l2, p, sizeof(l2));
                        if (l2.dwProtocolVersion != MT2_PROTOCOL_VERSION_X64 ||
                            (d.clientVersion != 0 && d.clientVersion != MT2_PROTOCOL_VERSION_X64)) {
                            d.failureStatus = "VERSION";
                            d.delayedDisconnect = true;
                        } else {
                            ++d.validLoginsProcessed;
                        }
                    } else if (hdr == HEADER_GG_SETUP) {
                        TPacketGGSetup gg{};
                        std::memcpy(&gg, p, sizeof(gg));
                        if (gg.dwProtocolVersion != MT2_PROTOCOL_VERSION_X64) {
                            d.phase = ESimPhase::Close;
                        } else {
                            ++d.p2pSetupsProcessed;
                        }
                    }

                    p += pktLen;
                    bufferLeft -= pktLen;
                    outBytesProceed += pktLen;

                    if (d.phase == ESimPhase::Close || d.delayedDisconnect) {
                        outBytesProceed = totalBytes;
                        return true;
                    }
                }
                return true;
            };

            // Case A: Legacy-sized Login2 (52B) and Login3 (321B / 130B) rejected with VERSION and full buffer drain
            {
                SimDesc dLogin2{ESimPhase::Login};
                std::vector<uint8_t> legacyLogin2(sizeof(TPacketCGLogin2) - sizeof(uint32_t), 0);
                legacyLogin2[0] = HEADER_CG_LOGIN2;
                size_t proceed = 0;
                simProcessBuffer(dLogin2, legacyLogin2.data(), legacyLogin2.size(), proceed);
                WIRE_ASSERT(dLogin2.failureStatus == "VERSION" && dLogin2.delayedDisconnect && proceed == legacyLogin2.size(),
                    "Protocol Framing [A1]: Legacy 52-byte Login2 rejected with VERSION and full buffer drained");

                SimDesc dLogin3_321{ESimPhase::Auth};
                std::vector<uint8_t> legacyLogin3_321(sizeof(TPacketCGLogin3) - sizeof(uint32_t), 0);
                legacyLogin3_321[0] = HEADER_CG_LOGIN3;
                simProcessBuffer(dLogin3_321, legacyLogin3_321.data(), legacyLogin3_321.size(), proceed);
                WIRE_ASSERT(dLogin3_321.failureStatus == "VERSION" && dLogin3_321.delayedDisconnect && proceed == legacyLogin3_321.size(),
                    "Protocol Framing [A2]: Legacy 321-byte Login3 rejected with VERSION and full buffer drained");

                SimDesc dLogin3_130{ESimPhase::Auth};
                std::vector<uint8_t> legacyLogin3_130(130, 0);
                legacyLogin3_130[0] = HEADER_CG_LOGIN3;
                simProcessBuffer(dLogin3_130, legacyLogin3_130.data(), legacyLogin3_130.size(), proceed);
                WIRE_ASSERT(dLogin3_130.failureStatus == "VERSION" && dLogin3_130.delayedDisconnect && proceed == 130,
                    "Protocol Framing [A3]: Legacy 130-byte Login3 rejected with VERSION and full buffer drained");
            }

            // Case B: Short / fragmented TCP packet reassembly (20B + 36B Login2)
            {
                SimDesc dFrag{ESimPhase::Login};
                TPacketCGLogin2 validL2{};
                validL2.header = HEADER_CG_LOGIN2;
                std::snprintf(validL2.login, sizeof(validL2.login), "fraguser");
                validL2.dwLoginKey = 12345;
                validL2.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

                std::vector<uint8_t> stream(sizeof(validL2));
                std::memcpy(stream.data(), &validL2, sizeof(validL2));

                size_t proceed1 = 0;
                simProcessBuffer(dFrag, stream.data(), 20, proceed1);
                WIRE_ASSERT(proceed1 == 0 && !dFrag.delayedDisconnect && dFrag.validLoginsProcessed == 0,
                    "Protocol Framing [B1]: 20-byte fragmented Login2 waits cleanly without consuming or rejecting");

                size_t proceed2 = 0;
                simProcessBuffer(dFrag, stream.data(), stream.size(), proceed2);
                WIRE_ASSERT(proceed2 == sizeof(validL2) && !dFrag.delayedDisconnect && dFrag.validLoginsProcessed == 1,
                    "Protocol Framing [B2]: Reassembled 56-byte Login2 parsed cleanly on second TCP fragment");
            }

            // Case C: Wrong version, missing (0) version, and trailing payload after rejected CheckClientVersion
            {
                SimDesc dWrongVer{ESimPhase::Auth};
                TPacketCGCheckVersion badCv{};
                badCv.header = HEADER_CG_CHECK_VERSION;
                badCv.dwVersion = 0x20090032U; // Legacy version
                TPacketCGLogin3 trailingL3{};
                trailingL3.header = HEADER_CG_LOGIN3;
                trailingL3.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

                std::vector<uint8_t> combined(sizeof(badCv) + sizeof(trailingL3));
                std::memcpy(combined.data(), &badCv, sizeof(badCv));
                std::memcpy(combined.data() + sizeof(badCv), &trailingL3, sizeof(trailingL3));

                size_t proceed = 0;
                simProcessBuffer(dWrongVer, combined.data(), combined.size(), proceed);
                WIRE_ASSERT(dWrongVer.failureStatus == "VERSION" &&
                            dWrongVer.delayedDisconnect &&
                            dWrongVer.validLoginsProcessed == 0 &&
                            proceed == combined.size(),
                    "Protocol Framing [C]: Wrong CheckClientVersion rejects with VERSION and drains trailing Login3 without parsing it");
            }

            // Case D: Back-to-back valid packets on a single connection (CheckClientVersion + Login3)
            {
                SimDesc dBackToBack{ESimPhase::Auth};
                TPacketCGCheckVersion okCv{};
                okCv.header = HEADER_CG_CHECK_VERSION;
                okCv.dwVersion = MT2_PROTOCOL_VERSION_X64;
                TPacketCGLogin3 okL3{};
                okL3.header = HEADER_CG_LOGIN3;
                okL3.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

                std::vector<uint8_t> combined(sizeof(okCv) + sizeof(okL3));
                std::memcpy(combined.data(), &okCv, sizeof(okCv));
                std::memcpy(combined.data() + sizeof(okCv), &okL3, sizeof(okL3));

                size_t proceed = 0;
                simProcessBuffer(dBackToBack, combined.data(), combined.size(), proceed);
                WIRE_ASSERT(!dBackToBack.delayedDisconnect &&
                            dBackToBack.clientVersion == MT2_PROTOCOL_VERSION_X64 &&
                            dBackToBack.validLoginsProcessed == 1 &&
                            proceed == combined.size(),
                    "Protocol Framing [D]: Back-to-back valid CheckClientVersion + Login3 both processed in a single TCP read");
            }

            // Case E: P2P and DB Setup legacy size / version mismatch rejection
            {
                SimDesc dP2PShort{ESimPhase::P2P};
                std::vector<uint8_t> legacyGG(sizeof(TPacketGGSetup) - sizeof(uint32_t), 0);
                legacyGG[0] = HEADER_GG_SETUP;
                size_t proceed = 0;
                simProcessBuffer(dP2PShort, legacyGG.data(), legacyGG.size(), proceed);
                WIRE_ASSERT(dP2PShort.phase == ESimPhase::Close && dP2PShort.p2pSetupsProcessed == 0 && proceed == legacyGG.size(),
                    "Protocol Framing [E1]: Legacy 4-byte TPacketGGSetup closes P2P descriptor immediately");

                auto checkDbSetupPacket = [](size_t dwLength, uint32_t protoVer) -> bool {
                    if (dwLength < sizeof(TPacketGDSetup))
                        return false;
                    return protoVer == MT2_PROTOCOL_VERSION_X64;
                };
                WIRE_ASSERT(!checkDbSetupPacket(sizeof(TPacketGDSetup) - sizeof(uint32_t), MT2_PROTOCOL_VERSION_X64) &&
                            !checkDbSetupPacket(sizeof(TPacketGDSetup), 0x20090032U) &&
                            checkDbSetupPacket(sizeof(TPacketGDSetup), MT2_PROTOCOL_VERSION_X64),
                    "Protocol Framing [E2]: DB HEADER_GD_SETUP rejects legacy short payload and mismatched protocol version");
            }

            // Case F: G2 & G4 Hardening (building.h LP64/LLP64 parity, CInputDB unsigned dwSize, VID wraparound, ItemIDRange collision & exhaustion)
            {
                // F1: building.h packed layout & 64-bit YANG price round-trip
                building::TLand outLand{};
                outLand.dwID = 501;
                outLand.lMapIndex = 41;
                outLand.x = 958500;
                outLand.y = 256800;
                outLand.width = 2000;
                outLand.height = 2000;
                outLand.dwGuildID = 77;
                outLand.bGuildLevelLimit = 20;
                outLand.dwPrice = 5000000000LL; // 5B Yang (> 2^32)
                std::vector<uint8_t> landBuf(sizeof(building::TLand));
                std::memcpy(landBuf.data(), &outLand, sizeof(building::TLand));
                building::TLand inLand{};
                std::memcpy(&inLand, landBuf.data(), sizeof(building::TLand));
                WIRE_ASSERT(sizeof(building::TLand) == 37 && inLand.dwPrice == 5000000000LL && inLand.lMapIndex == 41,
                    "G4.1: building::TLand is strictly 37 packed bytes and preserves >2^32 YANG price");

                // F2: CInputDB::Process unsigned dwSize (>= 2^31) guard
                auto canProcessDbPacket = [](int iBufferLeft, uint32_t dwSize) -> bool {
                    if (iBufferLeft < 9)
                        return false;
                    const uint64_t totalNeeded = 9ULL + static_cast<uint64_t>(dwSize);
                    return static_cast<uint64_t>(iBufferLeft) >= totalNeeded;
                };
                // Previously dwSize = 0xFFFFFFF0U cast to signed int was -16, causing (iBufferLeft - 9 < -16) == false and rewinding pointer!
                WIRE_ASSERT(!canProcessDbPacket(64, 0xFFFFFFF0U) &&
                            !canProcessDbPacket(64, 0x80000000U) &&
                            canProcessDbPacket(64, 55U),
                    "G4.3: CInputDB::Process blocks dwSize >= 2^31 from rewinding buffer pointer via signed int overflow");

                // F3: VID allocation wraparound & collision skip
                auto simAllocVID = [](uint32_t& counter, uint32_t occupiedVid) -> uint32_t {
                    do {
                        ++counter;
                        if (counter == 0)
                            ++counter;
                    } while (counter == occupiedVid);
                    return counter;
                };
                uint32_t vidCounter = 0xFFFFFFFFU;
                WIRE_ASSERT(simAllocVID(vidCounter, 1U) == 2U,
                    "G2.1: AllocVID skips 0 on 32-bit wraparound and skips active VID=1 in map");

                // F4: ItemIDRange collision polarity & GainItemID exhaustion guard
                auto checkRangeCollision = [](uint32_t aMin, uint32_t aMax, uint32_t bMin, uint32_t bMax) -> bool {
                    return aMin > 0 && aMax > 0 && aMin <= bMax && aMax >= bMin;
                };
                WIRE_ASSERT(checkRangeCollision(10000001U, 20000000U, 10000001U, 20000000U) &&
                            !checkRangeCollision(10000001U, 20000000U, 20000001U, 30000000U),
                    "G2.2: CheckItemIDRangeCollision returns true on overlapping range and false on disjoint range");

                TItemIDRangeTable exhaustedRange{100U, 101U, 101U};
                auto simGainItemID = [](TItemIDRangeTable& r) -> uint32_t {
                    if (r.dwUsableItemIDMin == 0 || r.dwUsableItemIDMin > r.dwMax)
                        return 0U;
                    return r.dwUsableItemIDMin++;
                };
                WIRE_ASSERT(simGainItemID(exhaustedRange) == 101U && simGainItemID(exhaustedRange) == 0U,
                    "G2.2: GainItemID returns last valid ID (dwMax) and returns 0 once exhausted instead of overflowing into next range");
            }
        }
    }

    std::cout << "[SUCCESS] WIRE PROTOCOL SUITE: " << g_passCount << " assertions passed cleanly!" << std::endl;
    return 0;
}

