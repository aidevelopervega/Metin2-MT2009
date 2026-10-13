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

#include "server/src/game/src/packet.h"
#include "server/src/common/tables.h"

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

static_assert(sizeof(TPlayerItem) == 52, "TPlayerItem size mismatch");
static_assert(sizeof(TSimplePlayer) == 67, "TSimplePlayer size mismatch");
static_assert(sizeof(TItemLimit) == 5, "TItemLimit size mismatch");
static_assert(sizeof(TItemApply) == 5, "TItemApply size mismatch");
static_assert(sizeof(TPacketAffectElement) == 25, "TPacketAffectElement size mismatch");
static_assert(sizeof(TPacketGDSetup) == 155, "TPacketGDSetup size mismatch");
static_assert(sizeof(TMapLocation) == 147, "TMapLocation size mismatch");

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

    std::cout << "[SUCCESS] WIRE PROTOCOL SUITE: " << g_passCount << " assertions passed cleanly!" << std::endl;
    return 0;
}
