#pragma once

//////////////////////////////////////////////////////////////////////////
// ### Default Ymir Macros ###
#define LOCALE_SERVICE_EUROPE
#define ENABLE_COSTUME_SYSTEM
//#define ENABLE_ENERGY_SYSTEM
//#define ENABLE_DRAGON_SOUL_SYSTEM
//#define ENABLE_NEW_EQUIPMENT_SYSTEM
// ### Default Ymir Macros ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New From LocaleInc ###
//#define ENABLE_SEQUENCE_SYSTEM
#define ENABLE_PACK_GET_CHECK
#define ENABLE_PROTOSTRUCT_AUTODETECT
//#define ENABLE_PLAYER_PER_ACCOUNT5
#define ENABLE_LEVEL_IN_TRADE
//#define ENABLE_DICE_SYSTEM
//#define ENABLE_EXTEND_INVEN_SYSTEM
#define ENABLE_LVL115_ARMOR_EFFECT
#define ENABLE_SLOT_WINDOW_EX
#define ENABLE_TEXT_LEVEL_REFRESH
#define ENABLE_USE_COSTUME_ATTR
#define ENABLE_DISCORD_RPC
//#define ENABLE_PET_SYSTEM_EX
#define ENABLE_LOCALE_EX
#define ENABLE_NO_DSS_QUALIFICATION
//#define ENABLE_NO_SELL_PRICE_DIVIDED_BY_5
//#define ENABLE_PENDANT_SYSTEM
//#define ENABLE_GLOVE_SYSTEM
#define ENABLE_MOVE_CHANNEL
//#define ENABLE_QUIVER_SYSTEM
#define ENABLE_RACE_HEIGHT
//#define ENABLE_ELEMENTAL_TARGET
#define ENABLE_INGAME_CONSOLE
#define ENABLE_4TH_AFF_SKILL_DESC
#define ENABLE_LOCALE_COMMON
#define ENABLE_GUILD_TOKEN_AUTH
#define ENABLE_DS_GRADE_MYTH
//#define ENABLE_CONQUEROR_UI

#define ENABLE_NEW_EVENT_STRUCT
#ifdef ENABLE_NEW_EVENT_STRUCT
#define USE_NEW_EVENT_TEXT_AUTO_Y
#endif

//#define WJ_SHOW_MOB_INFO
#ifdef WJ_SHOW_MOB_INFO
#define ENABLE_SHOW_MOBAIFLAG
#define ENABLE_SHOW_MOBLEVEL
#define WJ_SHOW_MOB_INFO_EX
#endif
// ### New From LocaleInc ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### From GameLib ###
//#define ENABLE_WOLFMAN_CHARACTER
#ifdef ENABLE_WOLFMAN_CHARACTER
// #define DISABLE_WOLFMAN_ON_CREATE
#endif
// #define ENABLE_MAGIC_REDUCTION_SYSTEM
//#define ENABLE_MOUNT_COSTUME_SYSTEM
//#define ENABLE_WEAPON_COSTUME_SYSTEM
// ### From GameLib ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New System Defines - Extended Version ###

// if is define ENABLE_ACCE_COSTUME_SYSTEM the players can use shoulder sash
//#define ENABLE_ACCE_COSTUME_SYSTEM
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
// #define USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS
#endif

// if you want use SetMouseWheelScrollEvent or you want use mouse wheel to move the scrollbar
#define ENABLE_MOUSEWHEEL_EVENT

//if you want to see highlighted a new item when dropped or when exchanged
#define ENABLE_HIGHLIGHT_NEW_ITEM

// it shows emojis in the textlines
#define ENABLE_EMOJI_SYSTEM

// effects while hidden won't show up
#define __ENABLE_STEALTH_FIX__

#define ENABLE_SET_ATLAS_SCALE
#define PARTY_POSITION

// circle dots in minimap instead of squares
//#define ENABLE_MINIMAP_WHITEMARK_CIRCLE
//#define ENABLE_MINIMAP_TELEPORT_CLICK // click on minimap as gm to warp directly

// enable the won system as a currency
//#define ENABLE_CHEQUE_SYSTEM
#ifdef ENABLE_CHEQUE_SYSTEM
#define DISABLE_CHEQUE_DROP
#define ENABLE_WON_EXCHANGE_WINDOW
#endif

// for debug: print received packets
// #define ENABLE_PRINT_RECV_PACKET_DEBUG

// ### New System Defines - Extended Version ###
//////////////////////////////////////////////////////////////////////////
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f


// MT2009
#ifndef _RELEASE
#define MT2009_CINEMATIC
#endif

#ifndef MT2009_CINEMATIC
#define ENABLE_CANSEEHIDDENTHING_FOR_GM
#endif

//#define DEBUG_CYTHON
//#if defined(_DEBUG) && !defined(DEBUG_CYTHON)
//#define ENABLE_DEBUG_PACK_UNPACKED
//#else
////#define __USE_CYTHON__
////#define __THEMIDA__
//#define ENABLE_DEBUG_PACK_UNPACKED
//#endif

#define ENABLE_DISCORD_RPC

//#ifdef __USE_CYTHON__
//#define __CYTHON_PYD__
//#define __EXTRA_CYTHON__
//#endif

#define ENABLE_IKASHOP_RENEWAL
#define ENABLE_IKASHOP_ENTITIES
#define EXTEND_IKASHOP_PRO
#define EXTEND_IKASHOP_ULTIMATE
#define ENABLE_LARGE_DYNAMIC_PACKETS

#define __BL_CLIP_MASK__

#define ENABLE_FIX_MOBS_LAG
#define CINEMATIC_CAMERA
//#define LEV_ANTICHEAT
// Files shared by GameCore.top
