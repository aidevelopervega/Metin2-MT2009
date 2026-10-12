#include "StdAfx.h"
#include "PythonNetworkStream.h"
#include "PythonItem.h"
#include "PythonShop.h"
#include "PythonExchange.h"
#include "PythonSafeBox.h"
#include "PythonCharacterManager.h"
#include "PythonCaptcha.h"

#include "AbstractPlayer.h"
#include <ThemidaSDK.h>

//////////////////////////////////////////////////////////////////////////
// SafeBox

bool CPythonNetworkStream::SendSafeBoxMoneyPacket(BYTE byState, DWORD dwMoney)
{
	assert(!"CPythonNetworkStream::SendSafeBoxMoneyPacket - Don't use this function");
	return false;
}

bool CPythonNetworkStream::SendSafeBoxCheckinPacket(TItemPos InventoryPos, BYTE bySafeBoxPos)
{
	__PlayInventoryItemDropSound(InventoryPos);

	TPacketCGSafeboxCheckin kSafeboxCheckin;
	kSafeboxCheckin.bHeader = HEADER_CG_SAFEBOX_CHECKIN;
	kSafeboxCheckin.ItemPos = InventoryPos;
	kSafeboxCheckin.bSafePos = bySafeBoxPos;
	if (!Send(kSafeboxCheckin))
		return false;

	return SendSequence();
}

bool CPythonNetworkStream::SendSafeBoxCheckoutPacket(BYTE bySafeBoxPos, TItemPos InventoryPos)
{
	__PlaySafeBoxItemDropSound(bySafeBoxPos);

	TPacketCGSafeboxCheckout kSafeboxCheckout;
	kSafeboxCheckout.bHeader = HEADER_CG_SAFEBOX_CHECKOUT;
	kSafeboxCheckout.bSafePos = bySafeBoxPos;
	kSafeboxCheckout.ItemPos = InventoryPos;
	if (!Send(kSafeboxCheckout))
		return false;

	return SendSequence();
}

bool CPythonNetworkStream::SendSafeBoxItemMovePacket(BYTE bySourcePos, BYTE byTargetPos)
{
	__PlaySafeBoxItemDropSound(bySourcePos);

	TPacketCGItemMove kItemMove;
	kItemMove.header = HEADER_CG_SAFEBOX_ITEM_MOVE;
	kItemMove.pos = TItemPos(INVENTORY, bySourcePos);
	kItemMove.change_pos = TItemPos(INVENTORY, byTargetPos);
	kItemMove.num = 0;
	if (!Send(kItemMove))
		return false;

	return SendSequence();
}

bool CPythonNetworkStream::RecvSafeBoxSetPacket()
{
	TPacketGCItemSet2 kItemSet;
	if (!AutoRecv(kItemSet))
		return false;

	TItemData kItemData;
	kItemData.vnum	= kItemSet.vnum;
	kItemData.count = kItemSet.count;
	kItemData.flags = kItemSet.flags;
	kItemData.anti_flags = kItemSet.anti_flags;
	for (int isocket=0; isocket<ITEM_SOCKET_SLOT_MAX_NUM; ++isocket)
		kItemData.alSockets[isocket] = kItemSet.alSockets[isocket];
	for (int iattr=0; iattr<ITEM_ATTRIBUTE_SLOT_MAX_NUM; ++iattr)
		kItemData.aAttr[iattr] = kItemSet.aAttr[iattr];

	CPythonSafeBox::Instance().SetItemData(kItemSet.Cell.cell, kItemData);

	__RefreshSafeboxWindow();

	return true;
}

bool CPythonNetworkStream::RecvSafeBoxDelPacket()
{
	TPacketGCItemDel kItemDel;
	if (!AutoRecv(kItemDel))
		return false;

	CPythonSafeBox::Instance().DelItemData(kItemDel.pos);

	__RefreshSafeboxWindow();

	return true;
}

bool CPythonNetworkStream::RecvSafeBoxWrongPasswordPacket()
{
	TPacketGCSafeboxWrongPassword kSafeboxWrongPassword;

	if (!AutoRecv(kSafeboxWrongPassword))
		return false;

	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "OnSafeBoxError", Py_BuildValue("()"));

	return true;
}

bool CPythonNetworkStream::RecvSafeBoxSizePacket()
{
	TPacketGCSafeboxSize kSafeBoxSize;
	if (!AutoRecv(kSafeBoxSize))
		return false;

	CPythonSafeBox::Instance().OpenSafeBox(kSafeBoxSize.bSize);
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "OpenSafeboxWindow", Py_BuildValue("(i)", kSafeBoxSize.bSize));

	return true;
}

bool CPythonNetworkStream::RecvSafeBoxMoneyChangePacket()
{
	TPacketGCSafeboxMoneyChange kMoneyChange;
	if (!AutoRecv(kMoneyChange))
		return false;

	CPythonSafeBox::Instance().SetMoney(kMoneyChange.dwMoney);
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "RefreshSafeboxMoney", Py_BuildValue("()"));

	return true;
}

// SafeBox
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Mall
bool CPythonNetworkStream::SendMallCheckoutPacket(BYTE byMallPos, TItemPos InventoryPos)
{
	__PlayMallItemDropSound(byMallPos);

	TPacketCGMallCheckout kMallCheckoutPacket;
	kMallCheckoutPacket.bHeader = HEADER_CG_MALL_CHECKOUT;
	kMallCheckoutPacket.bMallPos = byMallPos;
	kMallCheckoutPacket.ItemPos = InventoryPos;
	if (!Send(kMallCheckoutPacket))
		return false;

	return SendSequence();
}

bool CPythonNetworkStream::RecvMallOpenPacket()
{
	TPacketGCMallOpen kMallOpen;
	if (!AutoRecv(kMallOpen))
		return false;

	CPythonSafeBox::Instance().OpenMall(kMallOpen.bSize);
	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "OpenMallWindow", Py_BuildValue("(i)", kMallOpen.bSize));

	return true;
}
bool CPythonNetworkStream::RecvMallItemSetPacket()
{
	TPacketGCItemSet2 kItemSet;
	if (!AutoRecv(kItemSet))
		return false;

	TItemData kItemData;
	kItemData.vnum = kItemSet.vnum;
	kItemData.count = kItemSet.count;
	kItemData.flags = kItemSet.flags;
	kItemData.anti_flags = kItemSet.anti_flags;
	for (int isocket=0; isocket<ITEM_SOCKET_SLOT_MAX_NUM; ++isocket)
		kItemData.alSockets[isocket] = kItemSet.alSockets[isocket];
	for (int iattr=0; iattr<ITEM_ATTRIBUTE_SLOT_MAX_NUM; ++iattr)
		kItemData.aAttr[iattr] = kItemSet.aAttr[iattr];

	CPythonSafeBox::Instance().SetMallItemData(kItemSet.Cell.cell, kItemData);

	__RefreshMallWindow();

	return true;
}
bool CPythonNetworkStream::RecvMallItemDelPacket()
{
	TPacketGCItemDel kItemDel;
	if (!AutoRecv(kItemDel))
		return false;

	CPythonSafeBox::Instance().DelMallItemData(kItemDel.pos);

	__RefreshMallWindow();
	Tracef(" >> CPythonNetworkStream::RecvMallItemDelPacket\n");

	return true;
}
// Mall
//////////////////////////////////////////////////////////////////////////

// Item
// Recieve
bool CPythonNetworkStream::RecvItemSetPacket()
{
	TPacketGCItemSet packet_item_set;

	if (!AutoRecv(packet_item_set))
		return false;

	TItemData kItemData;
	kItemData.vnum	= packet_item_set.vnum;
	kItemData.count	= packet_item_set.count;
	kItemData.flags = 0;
	for (int i=0; i<ITEM_SOCKET_SLOT_MAX_NUM; ++i)
		kItemData.alSockets[i]=packet_item_set.alSockets[i];
	for (int j=0; j<ITEM_ATTRIBUTE_SLOT_MAX_NUM; ++j)
		kItemData.aAttr[j]=packet_item_set.aAttr[j];

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();

	rkPlayer.SetItemData(packet_item_set.Cell, kItemData);

	__RefreshInventoryWindow();
	return true;
}

bool CPythonNetworkStream::RecvItemSetPacket2()
{
	TPacketGCItemSet2 packet_item_set;

	if (!AutoRecv(packet_item_set))
		return false;

	TItemData kItemData;
	kItemData.vnum	= packet_item_set.vnum;
	kItemData.count	= packet_item_set.count;
	kItemData.flags = packet_item_set.flags;
	kItemData.anti_flags = packet_item_set.anti_flags;

	for (int i=0; i<ITEM_SOCKET_SLOT_MAX_NUM; ++i)
		kItemData.alSockets[i]=packet_item_set.alSockets[i];
	for (int j=0; j<ITEM_ATTRIBUTE_SLOT_MAX_NUM; ++j)
		kItemData.aAttr[j]=packet_item_set.aAttr[j];

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.SetItemData(packet_item_set.Cell, kItemData);

	if (packet_item_set.highlight)
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_Highlight_Item", Py_BuildValue("(ii)", packet_item_set.Cell.window_type, packet_item_set.Cell.cell));

	__RefreshInventoryWindow();
	return true;
}

bool CPythonNetworkStream::RecvItemUsePacket()
{
	TPacketGCItemUse packet_item_use;

	if (!AutoRecv(packet_item_use))
		return false;

	__RefreshInventoryWindow();
	return true;
}

bool CPythonNetworkStream::RecvItemUpdatePacket()
{
	TPacketGCItemUpdate packet_item_update;

	if (!AutoRecv(packet_item_update))
		return false;

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.SetItemCount(packet_item_update.Cell, packet_item_update.count);
	for (int i = 0; i < ITEM_SOCKET_SLOT_MAX_NUM; ++i)
		rkPlayer.SetItemMetinSocket(packet_item_update.Cell, i, packet_item_update.alSockets[i]);
	for (int j = 0; j < ITEM_ATTRIBUTE_SLOT_MAX_NUM; ++j)
		rkPlayer.SetItemAttribute(packet_item_update.Cell, j, packet_item_update.aAttr[j].bType, packet_item_update.aAttr[j].sValue);

	__RefreshInventoryWindow();
	return true;
}

bool CPythonNetworkStream::RecvItemGroundAddPacket()
{
	TPacketGCItemGroundAdd packet_item_ground_add;

	if (!AutoRecv(packet_item_ground_add))
		return false;

	__GlobalPositionToLocalPosition(packet_item_ground_add.lX, packet_item_ground_add.lY);

	CPythonItem::Instance().CreateItem(packet_item_ground_add.dwVID,
									   packet_item_ground_add.dwVnum,
									   packet_item_ground_add.lX,
									   packet_item_ground_add.lY,
									   packet_item_ground_add.lZ,
									   true,
									   packet_item_ground_add.count,
									   packet_item_ground_add.bIsAttr);
	return true;
}

bool CPythonNetworkStream::RecvItemOwnership()
{
	TPacketGCItemOwnership p;

	if (!AutoRecv(p))
		return false;

	CPythonItem::Instance().SetOwnership(p.dwVID, p.szName);
	return true;
}

bool CPythonNetworkStream::RecvItemGroundDelPacket()
{
	TPacketGCItemGroundDel	packet_item_ground_del;

	if (!AutoRecv(packet_item_ground_del))
		return false;

	CPythonItem::Instance().DeleteItem(packet_item_ground_del.vid);
	return true;
}

bool CPythonNetworkStream::RecvQuickSlotAddPacket()
{
	TPacketGCQuickSlotAdd packet_quick_slot_add;

	if (!AutoRecv(packet_quick_slot_add))
		return false;

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.AddQuickSlot(packet_quick_slot_add.pos, packet_quick_slot_add.slot.Type, packet_quick_slot_add.slot.Position);

	__RefreshInventoryWindow();

	return true;
}

bool CPythonNetworkStream::RecvQuickSlotDelPacket()
{
	TPacketGCQuickSlotDel packet_quick_slot_del;

	if (!AutoRecv(packet_quick_slot_del))
		return false;

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.DeleteQuickSlot(packet_quick_slot_del.pos);

	__RefreshInventoryWindow();

	return true;
}

bool CPythonNetworkStream::RecvQuickSlotMovePacket()
{
	TPacketGCQuickSlotSwap packet_quick_slot_swap;

	if (!AutoRecv(packet_quick_slot_swap))
		return false;

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.MoveQuickSlot(packet_quick_slot_swap.pos, packet_quick_slot_swap.change_pos);

	__RefreshInventoryWindow();

	return true;
}

bool CPythonNetworkStream::SendShopEndPacket()
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGShop packet_shop;
	packet_shop.header = HEADER_CG_SHOP;
	packet_shop.subheader = SHOP_SUBHEADER_CG_END;

	if (!Send(packet_shop))
	{
		Tracef("SendShopEndPacket Error\n");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendShopBuyPacket(BYTE bPos)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGShop PacketShop;
	PacketShop.header = HEADER_CG_SHOP;
	PacketShop.subheader = SHOP_SUBHEADER_CG_BUY;

	if (!Send(PacketShop))
	{
		Tracef("SendShopBuyPacket Error\n");
		return false;
	}

	BYTE bCount=1;
	if (!Send(bCount))
	{
		Tracef("SendShopBuyPacket Error\n");
		return false;
	}

	if (!Send(bPos))
	{
		Tracef("SendShopBuyPacket Error\n");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendShopSellPacket(BYTE bySlot)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGShop PacketShop;
	PacketShop.header = HEADER_CG_SHOP;
	PacketShop.subheader = SHOP_SUBHEADER_CG_SELL;

	if (!Send(PacketShop))
	{
		Tracef("SendShopSellPacket Error\n");
		return false;
	}
	if (!Send(bySlot))
	{
		Tracef("SendShopAddSellPacket Error\n");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendShopSellPacketNew(BYTE bySlot, ITEM_COUNT byCount)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGShop PacketShop;
	PacketShop.header = HEADER_CG_SHOP;
	PacketShop.subheader = SHOP_SUBHEADER_CG_SELL2;

	if (!Send(PacketShop))
	{
		Tracef("SendShopSellPacket Error\n");
		return false;
	}
	if (!Send(bySlot))
	{
		Tracef("SendShopAddSellPacket Error\n");
		return false;
	}
	if (!Send(byCount))
	{
		Tracef("SendShopAddSellPacket Error\n");
		return false;
	}

	Tracef(" SendShopSellPacketNew(bySlot=%d, byCount=%d)\n", bySlot, byCount);

	return SendSequence();
}

// Send
bool CPythonNetworkStream::SendItemUsePacket(TItemPos pos)
{
	if (!__CanActMainInstance())
		return true;

	if (__IsEquipItemInSlot(pos))
	{
		if (CPythonExchange::Instance().isTrading())
		{
			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_AppendNotifyMessage", Py_BuildValue("(s)", "CANNOT_EQUIP_EXCHANGE"));
			return true;
		}

		if (CPythonShop::Instance().IsOpen())
		{
			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_AppendNotifyMessage", Py_BuildValue("(s)", "CANNOT_EQUIP_SHOP"));
			return true;
		}

		if (__IsPlayerAttacking())
			return true;
	}

	__PlayInventoryItemUseSound(pos);

	TPacketCGItemUse itemUsePacket;
	itemUsePacket.header = HEADER_CG_ITEM_USE;
	itemUsePacket.pos = pos;

	if (!Send(itemUsePacket))
	{
		Tracen("SendItemUsePacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendItemUseToItemPacket(TItemPos source_pos, TItemPos target_pos)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGItemUseToItem itemUseToItemPacket;
	itemUseToItemPacket.header = HEADER_CG_ITEM_USE_TO_ITEM;
	itemUseToItemPacket.source_pos = source_pos;
	itemUseToItemPacket.target_pos = target_pos;

	if (!Send(itemUseToItemPacket))
	{
		Tracen("SendItemUseToItemPacket Error");
		return false;
	}

#ifdef _DEBUG
	Tracef(" << SendItemUseToItemPacket(src=%d, dst=%d)\n", source_pos, target_pos);
#endif

	return SendSequence();
}

bool CPythonNetworkStream::SendItemDropPacket(TItemPos pos, DWORD elk
	#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
	, DWORD cheque
	#endif
)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGItemDrop itemDropPacket;
	itemDropPacket.header = HEADER_CG_ITEM_DROP;
	itemDropPacket.pos = pos;
#ifndef DISABLE_GOLD_PLAYER_DROP
	itemDropPacket.elk = elk;
#endif
#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
	itemDropPacket.cheque = cheque;
#endif

	if (!Send(itemDropPacket))
	{
		Tracen("SendItemDropPacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendItemDropPacketNew(TItemPos pos, DWORD elk, DWORD count
#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
	, DWORD cheque
#endif
)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGItemDrop2 itemDropPacket;
	itemDropPacket.header = HEADER_CG_ITEM_DROP2;
	itemDropPacket.pos = pos;
#ifndef DISABLE_GOLD_PLAYER_DROP
	itemDropPacket.gold = elk;
#endif
	itemDropPacket.count = count;
#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
	itemDropPacket.cheque = cheque;
#endif

	if (!Send(itemDropPacket))
	{
		Tracen("SendItemDropPacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::__IsEquipItemInSlot(TItemPos uSlotPos)
{
	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	return rkPlayer.IsEquipItemInSlot(uSlotPos);
}

void CPythonNetworkStream::__PlayInventoryItemUseSound(TItemPos uSlotPos)
{
	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	DWORD dwItemID=rkPlayer.GetItemIndex(uSlotPos);

	CPythonItem& rkItem=CPythonItem::Instance();
	rkItem.PlayUseSound(dwItemID);
}

void CPythonNetworkStream::__PlayInventoryItemDropSound(TItemPos uSlotPos)
{
	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	DWORD dwItemID=rkPlayer.GetItemIndex(uSlotPos);

	CPythonItem& rkItem=CPythonItem::Instance();
	rkItem.PlayDropSound(dwItemID);
}

void CPythonNetworkStream::__PlaySafeBoxItemDropSound(UINT uSlotPos)
{
	DWORD dwItemID;
	CPythonSafeBox& rkSafeBox=CPythonSafeBox::Instance();
	if (!rkSafeBox.GetSlotItemID(uSlotPos, &dwItemID))
		return;

	CPythonItem& rkItem=CPythonItem::Instance();
	rkItem.PlayDropSound(dwItemID);
}

void CPythonNetworkStream::__PlayMallItemDropSound(UINT uSlotPos)
{
	DWORD dwItemID;
	CPythonSafeBox& rkSafeBox=CPythonSafeBox::Instance();
	if (!rkSafeBox.GetSlotMallItemID(uSlotPos, &dwItemID))
		return;

	CPythonItem& rkItem=CPythonItem::Instance();
	rkItem.PlayDropSound(dwItemID);
}

bool CPythonNetworkStream::SendItemMovePacket(TItemPos pos, TItemPos change_pos, ITEM_COUNT num)
{
	if (!__CanActMainInstance())
		return true;

	if (__IsEquipItemInSlot(pos))
	{
		if (CPythonExchange::Instance().isTrading())
		{
			if (pos.IsEquipPosition() || change_pos.IsEquipPosition())
			{
				PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_AppendNotifyMessage", Py_BuildValue("(s)", "CANNOT_EQUIP_EXCHANGE"));
				return true;
			}
		}

		if (CPythonShop::Instance().IsOpen())
		{
			if (pos.IsEquipPosition() || change_pos.IsEquipPosition())
			{
				PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_AppendNotifyMessage", Py_BuildValue("(s)", "CANNOT_EQUIP_SHOP"));
				return true;
			}
		}

		if (__IsPlayerAttacking())
			return true;
	}

	__PlayInventoryItemDropSound(pos);

	TPacketCGItemMove	itemMovePacket;
	itemMovePacket.header = HEADER_CG_ITEM_MOVE;
	itemMovePacket.pos = pos;
	itemMovePacket.change_pos = change_pos;
	itemMovePacket.num = num;

	if (!Send(itemMovePacket))
	{
		Tracen("SendItemMovePacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendItemPickUpPacket(DWORD vid)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGItemPickUp	itemPickUpPacket;
	itemPickUpPacket.header = HEADER_CG_ITEM_PICKUP;
	itemPickUpPacket.vid = vid;

	if (!Send(itemPickUpPacket))
	{
		Tracen("SendItemPickUpPacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendQuickSlotAddPacket(BYTE wpos, BYTE type, BYTE pos)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGQuickSlotAdd quickSlotAddPacket;

	quickSlotAddPacket.header		= HEADER_CG_QUICKSLOT_ADD;
	quickSlotAddPacket.pos			= wpos;
	quickSlotAddPacket.slot.Type	= type;
	quickSlotAddPacket.slot.Position = pos;

	if (!Send(quickSlotAddPacket))
	{
		Tracen("SendQuickSlotAddPacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendQuickSlotDelPacket(BYTE pos)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGQuickSlotDel quickSlotDelPacket;

	quickSlotDelPacket.header = HEADER_CG_QUICKSLOT_DEL;
	quickSlotDelPacket.pos = pos;

	if (!Send(quickSlotDelPacket))
	{
		Tracen("SendQuickSlotDelPacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::SendQuickSlotMovePacket(BYTE pos, BYTE change_pos)
{
	if (!__CanActMainInstance())
		return true;

	TPacketCGQuickSlotSwap quickSlotSwapPacket;

	quickSlotSwapPacket.header = HEADER_CG_QUICKSLOT_SWAP;
	quickSlotSwapPacket.pos = pos;
	quickSlotSwapPacket.change_pos = change_pos;

	if (!Send(quickSlotSwapPacket))
	{
		Tracen("SendQuickSlotSwapPacket Error");
		return false;
	}

	return SendSequence();
}

bool CPythonNetworkStream::RecvSpecialEffect()
{
	TPacketGCSpecialEffect kSpecialEffect;
	if (!AutoRecv(kSpecialEffect))
		return false;

	DWORD effect = -1;
	bool bPlayPotionSound = false;
	bool bAttachEffect = true;
	switch (kSpecialEffect.type)
	{
		case SE_HPUP_RED:
			effect = CInstanceBase::EFFECT_HPUP_RED;
			bPlayPotionSound = true;
			break;
		case SE_SPUP_BLUE:
			effect = CInstanceBase::EFFECT_SPUP_BLUE;
			bPlayPotionSound = true;
			break;
		case SE_SPEEDUP_GREEN:
			effect = CInstanceBase::EFFECT_SPEEDUP_GREEN;
			bPlayPotionSound = true;
			break;
		case SE_DXUP_PURPLE:
			effect = CInstanceBase::EFFECT_DXUP_PURPLE;
			bPlayPotionSound = true;
			break;
		case SE_CRITICAL:
			effect = CInstanceBase::EFFECT_CRITICAL;
			break;
		case SE_PENETRATE:
			effect = CInstanceBase::EFFECT_PENETRATE;
			break;
		case SE_BLOCK:
			effect = CInstanceBase::EFFECT_BLOCK;
			break;
		case SE_DODGE:
			effect = CInstanceBase::EFFECT_DODGE;
			break;
		case SE_CHINA_FIREWORK:
			effect = CInstanceBase::EFFECT_FIRECRACKER;
			bAttachEffect = false;
			break;
		case SE_SPIN_TOP:
			effect = CInstanceBase::EFFECT_SPIN_TOP;
			bAttachEffect = false;
			break;
		case SE_SUCCESS :
			effect = CInstanceBase::EFFECT_SUCCESS ;
			bAttachEffect = false ;
			break ;
		case SE_FAIL :
			effect = CInstanceBase::EFFECT_FAIL ;
			break ;
		case SE_FR_SUCCESS:
			effect = CInstanceBase::EFFECT_FR_SUCCESS;
			bAttachEffect = false ;
			break;
		case SE_LEVELUP_ON_14_FOR_GERMANY:
			effect = CInstanceBase::EFFECT_LEVELUP_ON_14_FOR_GERMANY;
			bAttachEffect = false ;
			break;
		case SE_LEVELUP_UNDER_15_FOR_GERMANY:
			effect = CInstanceBase::EFFECT_LEVELUP_UNDER_15_FOR_GERMANY;
			bAttachEffect = false ;
			break;
		case SE_PERCENT_DAMAGE1:
			effect = CInstanceBase::EFFECT_PERCENT_DAMAGE1;
			break;
		case SE_PERCENT_DAMAGE2:
			effect = CInstanceBase::EFFECT_PERCENT_DAMAGE2;
			break;
		case SE_PERCENT_DAMAGE3:
			effect = CInstanceBase::EFFECT_PERCENT_DAMAGE3;
			break;
		case SE_AUTO_HPUP:
			effect = CInstanceBase::EFFECT_AUTO_HPUP;
			break;
		case SE_AUTO_SPUP:
			effect = CInstanceBase::EFFECT_AUTO_SPUP;
			break;
		case SE_EQUIP_RAMADAN_RING:
			effect = CInstanceBase::EFFECT_RAMADAN_RING_EQUIP;
			break;
		case SE_EQUIP_HALLOWEEN_CANDY:
			effect = CInstanceBase::EFFECT_HALLOWEEN_CANDY_EQUIP;
			break;
		case SE_EQUIP_HAPPINESS_RING:
 			effect = CInstanceBase::EFFECT_HAPPINESS_RING_EQUIP;
			break;
		case SE_EQUIP_LOVE_PENDANT:
			effect = CInstanceBase::EFFECT_LOVE_PENDANT_EQUIP;
			break;
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
		case SE_EFFECT_ACCE_SUCESS_ABSORB:
			effect = CInstanceBase::EFFECT_ACCE_SUCESS_ABSORB;
			break;
		case SE_EFFECT_ACCE_EQUIP:
			effect = CInstanceBase::EFFECT_ACCE_EQUIP;
			break;
#endif
		case SE_FRESH_WATER:
			effect = CInstanceBase::EFFECT_FRESH_WATER;
			break;
		case SE_FLAME_BLOW:
			effect = CInstanceBase::EFFECT_FLAME_BLOW;
			break;
		case SE_DRINK_POTION:
			effect = CInstanceBase::EFFECT_DRINK_POTION;
			bPlayPotionSound = true;
			break;
		case SE_AOE_TREE_BLOW:
			effect = CInstanceBase::EFFECT_AOE_TREE_BLOW;
			break;
		case SE_AOE_GIANT_BLOW:
			effect = CInstanceBase::EFFECT_AOE_GIANT_BLOW;
			break;
		case SE_ENT_AURA:
			effect = CInstanceBase::EFFECT_ENT_AURA;
			break;

		case SE_BLOW_POISON:
			effect = CInstanceBase::EFFECT_BLOW_POISON;
			break;

		case SE_BRAVERY_CAPE:
			effect = CInstanceBase::EFFECT_BRAVERY_CAPE;
			break;

		case SE_STEALTH_0:
			effect = CInstanceBase::EFFECT_STEALTH_0;
			bAttachEffect = false;
			break;

		case SE_STEALTH_1:
			effect = CInstanceBase::EFFECT_STEALTH_1;
			bAttachEffect = false;
			break;

		case SE_STEALTH_2:
			effect = CInstanceBase::EFFECT_STEALTH_2;
			bAttachEffect = false;
			break;

		case SE_STEALTH_3:
			effect = CInstanceBase::EFFECT_STEALTH_3;
			bAttachEffect = false;
			break;

		default:
			TraceError("%d is not a special effect number. TPacketGCSpecialEffect",kSpecialEffect.type);
			break;
	}

	if (bPlayPotionSound)
	{
		IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
		if(rkPlayer.IsMainCharacterIndex(kSpecialEffect.vid))
		{
			CPythonItem& rkItem=CPythonItem::Instance();
			rkItem.PlayUsePotionSound();
		}
	}

	if (-1 != effect)
	{
		CInstanceBase * pInstance = CPythonCharacterManager::Instance().GetInstancePtr(kSpecialEffect.vid);
		if (pInstance)
		{
			if(bAttachEffect)
				pInstance->AttachSpecialEffect(effect);
			else
				pInstance->CreateSpecialEffect(effect);
		}
	}

	return true;
}

bool CPythonNetworkStream::RecvSpecificEffect()
{
	TPacketGCSpecificEffect kSpecificEffect;
	if (!AutoRecv(kSpecificEffect))
		return false;

	CInstanceBase * pInstance = CPythonCharacterManager::Instance().GetInstancePtr(kSpecificEffect.vid);
	//EFFECT_TEMP
	if (pInstance)
	{
		CInstanceBase::RegisterEffect(CInstanceBase::EFFECT_TEMP, "", kSpecificEffect.effect_file, false);
		pInstance->AttachSpecialEffect(CInstanceBase::EFFECT_TEMP);
	}

	return true;
}

bool CPythonNetworkStream::RecvDragonSoulRefine()
{
	TPacketGCDragonSoulRefine kDragonSoul;

	if (!AutoRecv(kDragonSoul))
		return false;

	switch (kDragonSoul.bSubType)
	{
	case DS_SUB_HEADER_OPEN:
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_DragonSoulRefineWindow_Open", Py_BuildValue("()"));
		break;
	case DS_SUB_HEADER_REFINE_FAIL:
	case DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE:
	case DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL:
	case DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY:
	case DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL:
	case DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL:
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_DragonSoulRefineWindow_RefineFail", Py_BuildValue("(iii)",
			kDragonSoul.bSubType, kDragonSoul.Pos.window_type, kDragonSoul.Pos.cell));
		break;
	case DS_SUB_HEADER_REFINE_SUCCEED:
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_DragonSoulRefineWindow_RefineSucceed",
				Py_BuildValue("(ii)", kDragonSoul.Pos.window_type, kDragonSoul.Pos.cell));
		break;
	}

	return true;
}
bool CPythonNetworkStream::RecvAddItemShop()
{
	TPacketGCItemShop packet_itemshop;

	if (!Recv(sizeof(packet_itemshop), &packet_itemshop))
		return false;

	switch (packet_itemshop.subheader)
	{
	case SUBHEADER_ITEMSHOP_OPEN_AUCTIONS:
	case SUBHEADER_ITEMSHOP_REFRESH:
	{
		TPacketGCItemShopRefresh subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;

		bool isRefreshItemShop = packet_itemshop.subheader == SUBHEADER_ITEMSHOP_REFRESH;
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_Reset_Data", Py_BuildValue("(i)", isRefreshItemShop));


		std::vector<TItemShopItem> itemShopItems;
		itemShopItems.resize(subpack.count);

		if (!Recv(sizeof(itemShopItems[0]) * itemShopItems.size(), itemShopItems.data()))
			return false;

		for (auto item : itemShopItems)
		{
			if (item.dwIndex == 0)
				break;

			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_Refresh_Item", Py_BuildValue("(iiiiiiiiiiiii)",
				item.dwIndex, item.dwVnum, item.dwCount, item.dwPrice, item.bCurrency, item.bMinLevel,
				item.promotion.dwPrice, item.promotion.dwStartTime, item.promotion.dwEndTime,
				item.timeAuction.dwStartTime, item.timeAuction.dwEndTime, item.timeAuction.dwAccountLimit, item.timeAuction.dwCurrentBuyAmount));

			if (!isRefreshItemShop)
				PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_Append_Item", Py_BuildValue("(i)", item.dwIndex));
		}

		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_Prepare", Py_BuildValue("(i)", isRefreshItemShop));
	}
	break;
	case SUBHEADER_ITEMSHOP_BALANCE:
	{
		TPacketGCItemShopBalance subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_Balance", Py_BuildValue("(ii)",
			subpack.dwDragonCoins, subpack.dwDragonMarks));
	}
	break;
	case SUBHEADER_ITEMSHOP_HAPPY_HOUR:
	{
		TPacketGCItemShopHappyHour subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_HappyHour", Py_BuildValue("(ii)",
			subpack.dwHappyHourValue, subpack.dwEndTime));
	}
	break;
	case SUBHEADER_ITEMSHOP_TIME_AUCTION:
	{
		TPacketGCItemShopLatestTimeAuction subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_TimeAuction", Py_BuildValue("(i)",
			subpack.dwEndTime));
	}
	break;
	case SUBHEADER_ITEMSHOP_UPDATE_AUCTION:
	{
		TItemShopUpdateAuctionItem subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;

		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_Auction_Update", Py_BuildValue("(iiiii)",
			subpack.dwIndex, subpack.dwProgress, subpack.dwStartTime, subpack.dwEndTime, subpack.dwAccountLimit));
	}
		break;

	case SUBHEADER_ITEMSHOP_USE_VOUCHER:
	{
		TPacketGCItemShopUseCodeVoucher subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;

		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_ItemShop_UseCodeVoucher", Py_BuildValue("(i)", subpack.bResponse));
	}
	break;

	default:
		return false;
	}

	return true;
}

bool CPythonNetworkStream::RecvSpecialShop()
{
	TPacketGCSpecialShop pack;

	if (!Recv(sizeof(pack), &pack))
		return false;

	switch (pack.subheader)
	{
		case SUBHEADER_GC_SPECIAL_SHOP_OPEN:
		{
			TSubPacketCGSpecialShopOpen subpack;
			if (!Recv(sizeof(subpack), &subpack))
				return false;

			std::vector<TSpecialShopItemClient> items;
			items.resize(subpack.count);

			if (!Recv(sizeof(items[0]) * items.size(), items.data()))
				return false;

			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_SpecialShop_Clear", Py_BuildValue("(i)", subpack.fraction));

			for (const TSpecialShopItemClient item : items)
			{
				std::string reqItemsStr;
				for (int i = 0; i < SPECIAL_SHOP_MAX_ITEM_COUNT; i++)
				{
					auto reqItem = item.items[i];
					reqItemsStr += std::to_string(reqItem.vnum) + "," + std::to_string(reqItem.count);
					if (i < SPECIAL_SHOP_MAX_ITEM_COUNT - 1)
					{
						reqItemsStr += ",";
					}
				}

				std::string limitsStr;
				for (int i = 0; i < SPECIAL_SHOP_MAX_LIMIT; i++)
				{
					auto limit = item.limits[i];
					limitsStr += std::to_string(limit.type) + "," + std::to_string(limit.value);
					if (i < SPECIAL_SHOP_MAX_LIMIT - 1)
					{
						limitsStr += ",";
					}
				}

				PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_SpecialShop_AddItem",
					Py_BuildValue("(iiiiiiss)", item.vnum, item.itemVnum, item.count, item.rarePct, item.priceType, item.price, reqItemsStr.c_str(), limitsStr.c_str()));
			}

			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_SpecialShop_Open", Py_BuildEmptyTuple());
		}
		break;

		case SUBHEADER_GC_SPECIAL_SHOP_BUY:
		{
			TSubPacketCGSpecialShopBuy subpack;
			if (!Recv(sizeof(subpack), &subpack))
				return false;

			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_SpecialShop_Buy", Py_BuildValue("(i)", subpack.vnum));
		}
		break;

		default:
			return false;
	}
	return true;
}

bool CPythonNetworkStream::RecvNpcInfoBoard()
{
	TPacketCGNpcInfoBoard pack;

	if (!Recv(sizeof(pack), &pack))
		return false;

	switch (pack.subheader)
	{
	case SUBHEADER_GC_NPC_INFO_REPUTATION:
	{
		TSubPacketCGNpcInfoReputation subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;

		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_NpcInfoBoard_Reputation", Py_BuildValue("(ii)", pack.vid, subpack.fraction));
	}
	break;

	default:
		return false;
	}
	return true;
}

bool CPythonNetworkStream::RecvBusyAction()
{
	TPacketGCBusyAction kBusyActionInfo;
	if (!Recv(sizeof(kBusyActionInfo), &kBusyActionInfo))
		return false;

	CInstanceBase* pkInst = CPythonCharacterManager::Instance().GetMainInstancePtr();
	if (pkInst)
	{
		//pkInst->SetBusyAction(kBusyActionInfo.dwDuration > 0);
	}

	PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_BusyAction", Py_BuildValue("(iii)",
		kBusyActionInfo.bActionType, kBusyActionInfo.dwDuration, kBusyActionInfo.bSuccess));
	return true;
}
bool CPythonNetworkStream::RecvCaptcha()
{

	TPacketGCCaptcha pack;
	if (!Recv(sizeof(pack), &pack))
		return false;

	switch (pack.subheader)
	{
	case SUBHEADER_GC_CAPTCHA_START:
	case SUBHEADER_GC_CAPTCHA_GM_START:

	case SUBHEADER_GC_CAPTCHA_CHANGE:
	case SUBHEADER_GC_CAPTCHA_GM_REQUEST_CHANGE:
	{
		DWORD imageSize = 0;

		bool isFreshCaptcha = pack.subheader == SUBHEADER_GC_CAPTCHA_START || pack.subheader == SUBHEADER_GC_CAPTCHA_GM_START;
		bool isGM = pack.subheader == SUBHEADER_GC_CAPTCHA_GM_START || pack.subheader == SUBHEADER_GC_CAPTCHA_GM_REQUEST_CHANGE;

		TSubPacketGCCaptchaImage subpack;
		if (!Recv(sizeof(subpack), &subpack))
			return false;

		imageSize = subpack.imageSize;

		DWORD whoPID = 0;
		std::string whoName = "";
		std::string code = "";
		if (isGM)
		{
			TSubPacketGCCaptchaGMWho who;
			if (!Recv(sizeof(who), &who))
				return false;

			whoPID = who.pid;
			whoName = who.name;

			TSubPacketGCCaptchaGMCode code_pack;
			if (!Recv(sizeof(code_pack), &code_pack))
				return false;

			code = code_pack.code;
		}

		TSubPacketGCCaptchaMeta metaPack;
		if (!Recv(sizeof(metaPack), &metaPack))
			return false;

		// unpack image
		std::vector<BYTE> buffer;
		buffer.resize(imageSize);

		if (!Recv(sizeof(BYTE) * imageSize, buffer.data()))
			return false;

		PythonCaptcha::instance().SetCaptchaImage(buffer);

		if (!isGM)
			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_OpenCaptcha", Py_BuildValue("(iii)", metaPack.captchaExpireTime, metaPack.attemptCount, isFreshCaptcha));
		else
			PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_OpenCaptcha_GameMaster", Py_BuildValue("(iiissi)", metaPack.captchaExpireTime, metaPack.attemptCount, whoPID, whoName.c_str(), code.c_str(), isFreshCaptcha));
	}
	break;

	case SUBHEADER_GC_CAPTCHA_STOP:
	{
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_CloseCaptcha", Py_BuildEmptyTuple());
	}
	break;

	case SUBHEADER_GC_CAPTCHA_ATTEMPT:
	{
		TSubPacketGCCaptchaMeta sub;
		if (!Recv(sizeof(sub), &sub))
			return false;

		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_CaptchaWrongAttempt", Py_BuildValue("(i)", sub.attemptCount));
	}
	break;

	case SUBHEADER_GC_CAPTCHA_GM_FEEDBACK:
	{
		TSubPacketGCCaptchaGMWho who;
		if (!Recv(sizeof(who), &who))
			return false;

		TSubPacketGCCaptchaGMFeedback sub;
		if (!Recv(sizeof(sub), &sub))
			return false;

		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_GAME], "BINARY_CaptchaFeedback_GameMaster", Py_BuildValue("(iisi)", who.pid, sub.type, sub.arg1, sub.arg2));
	}
	break;

	default:
		return false;
	}

	
	return true;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
