#include "StdAfx.h"
#include "PythonItem.h"

#include "../gamelib/ItemManager.h"
#include "../gamelib/GameLibDefines.h"
#include "InstanceBase.h"
#include "AbstractApplication.h"

extern int TWOHANDED_WEWAPON_ATT_SPEED_DECREASE_VALUE;

PyObject * itemSetUseSoundFileName(PyObject * poSelf, PyObject * poArgs)
{
	int iUseSound;
	if (!PyTuple_GetInteger(poArgs, 0, &iUseSound))
		return Py_BadArgument();

	char* szFileName;
	if (!PyTuple_GetString(poArgs, 1, &szFileName))
		return Py_BadArgument();

	CPythonItem& rkItem=CPythonItem::Instance();
	rkItem.SetUseSoundFileName(iUseSound, szFileName);
	return Py_BuildNone();
}

PyObject * itemSetDropSoundFileName(PyObject * poSelf, PyObject * poArgs)
{
	int iDropSound;
	if (!PyTuple_GetInteger(poArgs, 0, &iDropSound))
		return Py_BadArgument();

	char* szFileName;
	if (!PyTuple_GetString(poArgs, 1, &szFileName))
		return Py_BadArgument();

	CPythonItem& rkItem=CPythonItem::Instance();
	rkItem.SetDropSoundFileName(iDropSound, szFileName);
	return Py_BuildNone();
}

PyObject * itemSelectItem(PyObject * poSelf, PyObject * poArgs)
{
	int iIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iIndex))
		return Py_BadArgument();

	if (!CItemManager::Instance().SelectItemData(iIndex))
	{
		TraceError("Cannot find item by %d", iIndex);
		CItemManager::Instance().SelectItemData(60001);
	}

	return Py_BuildNone();
}

PyObject * itemGetItemName(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("s", pItemData->GetName());
}

PyObject * itemGetItemDescription(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("s", pItemData->GetDescription());
}

PyObject * itemGetItemSummary(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("s", pItemData->GetSummary());
}

PyObject * itemGetIconImage(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

//	if (CItemData::ITEM_TYPE_SKILLBOOK == pItemData->GetType())
//	{
//		char szItemName[64+1];
//		_snprintf(szItemName, "d:/ymir work/ui/items/etc/book_%02d.sub", );
//		CGraphicImage * pImage = (CGraphicImage *)CResourceManager::Instance().GetResourcePointer(szItemName);
//	}

	return Py_BuildValue("i", pItemData->GetIconImage());
}

PyObject * itemGetIconImageFileName(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	CGraphicSubImage * pImage = pItemData->GetIconImage();
	if (!pImage)
		return Py_BuildValue("s", "Noname");

	return Py_BuildValue("s", pImage->GetFileName());
}

PyObject * itemGetItemSize(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("(ii)", 1, pItemData->GetSize());
}

PyObject * itemGetItemType(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->GetType());
}

PyObject * itemGetItemSubType(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->GetSubType());
}

PyObject * itemGetIBuyItemPrice(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();

	if (!pItemData)
		return Py_BuildException("no selected item data");

	return PyLong_FromLongLong(pItemData->GetIBuyItemPrice());
}

PyObject * itemGetISellItemPrice(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();

	if (!pItemData)
		return Py_BuildException("no selected item data");

	return PyLong_FromLongLong(pItemData->GetISellItemPrice());
}

PyObject * itemIsAntiFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->IsAntiFlag(iFlag));
}

PyObject * itemIsFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->IsFlag(iFlag));
}

PyObject * itemIsWearableFlag(PyObject * poSelf, PyObject * poArgs)
{
	int iFlag;
	if (!PyTuple_GetInteger(poArgs, 0, &iFlag))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->IsWearableFlag(iFlag));
}

PyObject * itemIs1GoldItem(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->IsFlag(ITEM_FLAG_COUNT_PER_1GOLD));
}

PyObject * itemGetLimit(PyObject * poSelf, PyObject * poArgs)
{
	int iValueIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iValueIndex))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Not yet select item data");

	CItemData::TItemLimit ItemLimit;
	if (!pItemData->GetLimit(iValueIndex, &ItemLimit))
		return Py_BuildException();

	return Py_BuildValue("ii", ItemLimit.bType, ItemLimit.lValue);
}

PyObject * itemGetAffect(PyObject * poSelf, PyObject * poArgs)
{
	int iValueIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iValueIndex))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Not yet select item data");

	CItemData::TItemApply ItemApply;
	if (!pItemData->GetApply(iValueIndex, &ItemApply))
		return Py_BuildException();

	if ((POINT_ATT_SPEED == ItemApply.bType) && (ITEM_WEAPON == pItemData->GetType()) && (WEAPON_TWO_HANDED == pItemData->GetSubType()))
	{
		ItemApply.lValue -= TWOHANDED_WEWAPON_ATT_SPEED_DECREASE_VALUE;
	}

	return Py_BuildValue("ii", ItemApply.bType, ItemApply.lValue);
}

PyObject * itemGetValue(PyObject * poSelf, PyObject * poArgs)
{
	int iValueIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iValueIndex))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Not yet select item data");

	return Py_BuildValue("i", pItemData->GetValue(iValueIndex));
}

PyObject * itemGetSocket(PyObject * poSelf, PyObject * poArgs)
{
	int iValueIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iValueIndex))
		return Py_BadArgument();

	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Not yet select item data");

	return Py_BuildValue("i", pItemData->GetSocket(iValueIndex));
}

PyObject * itemGetIconInstance(PyObject * poSelf, PyObject * poArgs)
{
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Not yet select item data");

	CGraphicSubImage * pImage = pItemData->GetIconImage();
	if (!pImage)
		return Py_BuildException("Cannot get icon image by %d", pItemData->GetIndex());

	CGraphicImageInstance * pImageInstance = CGraphicImageInstance::New();
	pImageInstance->SetImagePointer(pImage);

	return Py_BuildValue("i", pImageInstance);
}

PyObject * itemDeleteIconInstance(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BadArgument();

	CGraphicImageInstance::Delete((CGraphicImageInstance *) iHandle);

	return Py_BuildNone();
}

PyObject * itemIsEquipmentVID(PyObject * poSelf, PyObject * poArgs)
{
	int iItemVID;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemVID))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemVID);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Not yet select item data");

	return Py_BuildValue("i", pItemData->IsEquipment());
}

PyObject* itemGetUseType(PyObject * poSelf, PyObject * poArgs)
{
	int iItemVID;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemVID))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemVID);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Can't find select item data");

	return Py_BuildValue("s", pItemData->GetUseTypeString());
}

PyObject * itemIsRefineScroll(PyObject * poSelf, PyObject * poArgs)
{
	int iItemIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemIndex))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemIndex);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Can't find select item data");

	if (pItemData->GetType() != ITEM_USE)
		return Py_BuildValue("i", FALSE);

	switch (pItemData->GetSubType())
	{
		case USE_TUNING:
			return Py_BuildValue("i", TRUE);
			break;
	}

	return Py_BuildValue("i", FALSE);
}

PyObject* itemGetItemMaxStack(PyObject* poSelf, PyObject* poArgs)
{
	CItemData* pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("no selected item data");

	return Py_BuildValue("i", pItemData->GetMaxStack());
}

PyObject * itemIsDetachScroll(PyObject * poSelf, PyObject * poArgs)
{
	int iItemIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemIndex))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemIndex);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Can't find select item data");

	int iType = pItemData->GetType();
	int iSubType = pItemData->GetSubType();
	if (iType == ITEM_USE)
	if (iSubType == USE_DETACHMENT)
	{
		return Py_BuildValue("i", TRUE);
	}

	return Py_BuildValue("i", FALSE);
}

PyObject * itemCanAddToQuickSlotItem(PyObject * poSelf, PyObject * poArgs)
{
	int iItemIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemIndex))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemIndex);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Can't find select item data");

	if (ITEM_USE == pItemData->GetType() ||
		ITEM_QUEST == pItemData->GetType() ||
		ITEM_POTION == pItemData->GetType())
	{
		return Py_BuildValue("i", TRUE);
	}

	return Py_BuildValue("i", FALSE);
}

PyObject * itemIsKey(PyObject * poSelf, PyObject * poArgs)
{
	int iItemIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemIndex))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemIndex);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Can't find select item data");

	if (ITEM_TREASURE_KEY == pItemData->GetType())
	{
		return Py_BuildValue("i", TRUE);
	}

	return Py_BuildValue("i", FALSE);
}

PyObject * itemIsMetin(PyObject * poSelf, PyObject * poArgs)
{
	int iItemIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iItemIndex))
		return Py_BadArgument();

	CItemManager::Instance().SelectItemData(iItemIndex);
	CItemData * pItemData = CItemManager::Instance().GetSelectedItemDataPointer();
	if (!pItemData)
		return Py_BuildException("Can't find select item data");

	if (ITEM_METIN == pItemData->GetType())
	{
		return Py_BuildValue("i", TRUE);
	}

	return Py_BuildValue("i", FALSE);
}

PyObject * itemRender(PyObject * poSelf, PyObject * poArgs)
{
	CPythonItem::Instance().Render();
	return Py_BuildNone();
}

PyObject * itemUpdate(PyObject * poSelf, PyObject * poArgs)
{
	IAbstractApplication& rkApp=IAbstractApplication::GetSingleton();

	POINT ptMouse;
	rkApp.GetMousePosition(&ptMouse);

	CPythonItem::Instance().Update(ptMouse);
	return Py_BuildNone();
}

PyObject * itemCreateItem(PyObject * poSelf, PyObject * poArgs)
{
	int iVirtualID;
	if (!PyTuple_GetInteger(poArgs, 0, &iVirtualID))
		return Py_BadArgument();
	int iVirtualNumber;
	if (!PyTuple_GetInteger(poArgs, 1, &iVirtualNumber))
		return Py_BadArgument();

	float x;
	if (!PyTuple_GetFloat(poArgs, 2, &x))
		return Py_BadArgument();
	float y;
	if (!PyTuple_GetFloat(poArgs, 3, &y))
		return Py_BadArgument();
	float z;
	if (!PyTuple_GetFloat(poArgs, 4, &z))
		return Py_BadArgument();

	bool bDrop = true;
	PyTuple_GetBoolean(poArgs, 5, &bDrop);

	CPythonItem::Instance().CreateItem(iVirtualID, iVirtualNumber, x, y, z, bDrop);

	return Py_BuildNone();
}

PyObject * itemDeleteItem(PyObject * poSelf, PyObject * poArgs)
{
	int iVirtualID;
	if (!PyTuple_GetInteger(poArgs, 0, &iVirtualID))
		return Py_BadArgument();

	CPythonItem::Instance().DeleteItem(iVirtualID);
	return Py_BuildNone();
}

PyObject * itemPick(PyObject * poSelf, PyObject * poArgs)
{
	DWORD dwItemID;
	if (CPythonItem::Instance().GetPickedItemID(&dwItemID))
		return Py_BuildValue("i", dwItemID);
	else
		return Py_BuildValue("i", -1);
}

PyObject* itemLoadItemTable(PyObject* poSelf, PyObject* poArgs)
{
	char * szFileName;
	if (!PyTuple_GetString(poArgs, 0, &szFileName))
		return Py_BadArgument();

	CItemManager::Instance().LoadItemTable(szFileName);
	return Py_BuildNone();
}

#include "../UserInterface/Packet.h"

typedef struct SApplyInfo
{
	BYTE	bPointType;                          // APPLY -> POINT
} TApplyInfo;


void initItem()
{
	static PyMethodDef s_methods[] =
	{
		{ "SetUseSoundFileName",			itemSetUseSoundFileName,				METH_VARARGS },
		{ "SetDropSoundFileName",			itemSetDropSoundFileName,				METH_VARARGS },
		{ "SelectItem",						itemSelectItem,							METH_VARARGS },

		{ "GetItemName",					itemGetItemName,						METH_VARARGS },
		{ "GetItemDescription",				itemGetItemDescription,					METH_VARARGS },
		{ "GetItemSummary",					itemGetItemSummary,						METH_VARARGS },
		{ "GetIconImage",					itemGetIconImage,						METH_VARARGS },
		{ "GetIconImageFileName",			itemGetIconImageFileName,				METH_VARARGS },
		{ "GetItemSize",					itemGetItemSize,						METH_VARARGS },
		{ "GetItemType",					itemGetItemType,						METH_VARARGS },
		{ "GetItemSubType",					itemGetItemSubType,						METH_VARARGS },
		{ "GetIBuyItemPrice",				itemGetIBuyItemPrice,					METH_VARARGS },
		{ "GetISellItemPrice",				itemGetISellItemPrice,					METH_VARARGS },
		{ "IsAntiFlag",						itemIsAntiFlag,							METH_VARARGS },
		{ "IsFlag",							itemIsFlag,								METH_VARARGS },
		{ "IsWearableFlag",					itemIsWearableFlag,						METH_VARARGS },
		{ "Is1GoldItem",					itemIs1GoldItem,						METH_VARARGS },
		{ "GetLimit",						itemGetLimit,							METH_VARARGS },
		{ "GetAffect",						itemGetAffect,							METH_VARARGS },
		{ "GetValue",						itemGetValue,							METH_VARARGS },
		{ "GetSocket",						itemGetSocket,							METH_VARARGS },
		{ "GetIconInstance",				itemGetIconInstance,					METH_VARARGS },
		{ "GetUseType",						itemGetUseType,							METH_VARARGS },
		{ "DeleteIconInstance",				itemDeleteIconInstance,					METH_VARARGS },
		{ "IsEquipmentVID",					itemIsEquipmentVID,						METH_VARARGS },
		{ "IsRefineScroll",					itemIsRefineScroll,						METH_VARARGS },
		{ "IsDetachScroll",					itemIsDetachScroll,						METH_VARARGS },
		{ "IsKey",							itemIsKey,								METH_VARARGS },
		{ "IsMetin",						itemIsMetin,							METH_VARARGS },
		{ "CanAddToQuickSlotItem",			itemCanAddToQuickSlotItem,				METH_VARARGS },
		{ "GetItemMaxStack",				itemGetItemMaxStack,					METH_VARARGS },

		{ "Update",							itemUpdate,								METH_VARARGS },
		{ "Render",							itemRender,								METH_VARARGS },
		{ "CreateItem",						itemCreateItem,							METH_VARARGS },
		{ "DeleteItem",						itemDeleteItem,							METH_VARARGS },
		{ "Pick",							itemPick,								METH_VARARGS },

		{ "LoadItemTable",					itemLoadItemTable,						METH_VARARGS },

#ifdef ENABLE_SEALBIND_SYSTEM
		{ "IsSealScroll",					itemIsSealScroll,						METH_VARARGS },
		{ "GetDefaultSealDate",				itemGetDefaultSealDate,					METH_VARARGS },
		{ "GetUnlimitedSealDate",			itemGetUnlimitedSealDate,				METH_VARARGS },
#endif

		{ NULL,								NULL,									NULL		 },
	};

	PyObject * poModule = Py_InitModule("item", s_methods);

	PyModule_AddIntConstant(poModule, "USESOUND_ACCESSORY",			CPythonItem::USESOUND_ACCESSORY);
	PyModule_AddIntConstant(poModule, "USESOUND_ARMOR",				CPythonItem::USESOUND_ARMOR);
	PyModule_AddIntConstant(poModule, "USESOUND_BOW",				CPythonItem::USESOUND_BOW);
	PyModule_AddIntConstant(poModule, "USESOUND_DEFAULT",			CPythonItem::USESOUND_DEFAULT);
	PyModule_AddIntConstant(poModule, "USESOUND_WEAPON",			CPythonItem::USESOUND_WEAPON);
	PyModule_AddIntConstant(poModule, "USESOUND_POTION",			CPythonItem::USESOUND_POTION);
	PyModule_AddIntConstant(poModule, "USESOUND_PORTAL",			CPythonItem::USESOUND_PORTAL);

	PyModule_AddIntConstant(poModule, "DROPSOUND_ACCESSORY",		CPythonItem::DROPSOUND_ACCESSORY);
	PyModule_AddIntConstant(poModule, "DROPSOUND_ARMOR",			CPythonItem::DROPSOUND_ARMOR);
	PyModule_AddIntConstant(poModule, "DROPSOUND_BOW",				CPythonItem::DROPSOUND_BOW);
	PyModule_AddIntConstant(poModule, "DROPSOUND_DEFAULT",			CPythonItem::DROPSOUND_DEFAULT);
	PyModule_AddIntConstant(poModule, "DROPSOUND_WEAPON",			CPythonItem::DROPSOUND_WEAPON);

	PyModule_AddIntConstant(poModule, "EQUIPMENT_COUNT",			c_Equipment_Count);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_HEAD",				c_Equipment_Head);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_BODY",				c_Equipment_Body);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_WEAPON",			c_Equipment_Weapon);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_WRIST",			c_Equipment_Wrist);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_SHOES",			c_Equipment_Shoes);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_NECK",				c_Equipment_Neck);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_EAR",				c_Equipment_Ear);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_UNIQUE1",			c_Equipment_Unique1);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_UNIQUE2",			c_Equipment_Unique2);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_ARROW",			c_Equipment_Arrow);
	PyModule_AddIntConstant(poModule, "EQUIPMENT_SHIELD",			c_Equipment_Shield);

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
	PyModule_AddIntConstant(poModule, "EQUIPMENT_BELT",				c_Equipment_Belt);
#endif
#ifdef ENABLE_PENDANT_SYSTEM
	PyModule_AddIntConstant(poModule, "EQUIPMENT_PENDANT",			c_Equipment_Pendant);
#endif
#ifdef ENABLE_GLOVE_SYSTEM
	PyModule_AddIntConstant(poModule, "EQUIPMENT_GLOVE",			c_Equipment_Glove);
#endif

	PyModule_AddIntConstant(poModule, "ITEM_TYPE_NONE",				ITEM_NONE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_WEAPON",			ITEM_WEAPON);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_ARMOR",			ITEM_ARMOR);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_USE",				ITEM_USE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_AUTOUSE",			ITEM_AUTOUSE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_MATERIAL",			ITEM_MATERIAL);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_SPECIAL",			ITEM_SPECIAL);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_TOOL",				ITEM_TOOL);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_LOTTERY",			ITEM_LOTTERY);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_ELK",				ITEM_ELK);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_METIN",			ITEM_METIN);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_CONTAINER",		ITEM_CONTAINER);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_FISH",				ITEM_FISH);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_ROD",				ITEM_ROD);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_RESOURCE",			ITEM_RESOURCE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_CAMPFIRE",			ITEM_CAMPFIRE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_UNIQUE",			ITEM_UNIQUE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_SKILLBOOK",		ITEM_SKILLBOOK);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_QUEST",			ITEM_QUEST);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_POLYMORPH",		ITEM_POLYMORPH);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_TREASURE_BOX",		ITEM_TREASURE_BOX);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_TREASURE_KEY",		ITEM_TREASURE_KEY);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_SKILLFORGET",		ITEM_SKILLFORGET);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_GIFTBOX",			ITEM_GIFTBOX);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_PICK",				ITEM_PICK);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_HAIR",				ITEM_HAIR);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_TOTEM",			ITEM_TOTEM);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_BLEND",			ITEM_BLEND);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_COSTUME",			ITEM_COSTUME);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_DS",				ITEM_DS);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_SPECIAL_DS",		ITEM_SPECIAL_DS);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_EXTRACT",			ITEM_EXTRACT);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_SECONDARY_COIN",	ITEM_SECONDARY_COIN);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_RING",				ITEM_RING);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_BELT",				ITEM_BELT);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_PET",				ITEM_PET);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_HERB_KNIFE",		ITEM_HERB_KNIFE);
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_POTION",			ITEM_POTION);

#ifdef ENABLE_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "ITEM_TYPE_COSTUME",			ITEM_COSTUME);

	// Item Sub Type
	PyModule_AddIntConstant(poModule, "COSTUME_TYPE_BODY",			COSTUME_BODY);
	PyModule_AddIntConstant(poModule, "COSTUME_TYPE_HAIR",			COSTUME_HAIR);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "COSTUME_TYPE_MOUNT",			COSTUME_MOUNT);
#endif
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "COSTUME_TYPE_ACCE",			COSTUME_ACCE);
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "COSTUME_TYPE_WEAPON",		COSTUME_WEAPON);
#endif

	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_START",			c_Costume_Slot_Start);
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_COUNT",			c_Costume_Slot_Count);
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_BODY",			c_Costume_Slot_Body);
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_HAIR",			c_Costume_Slot_Hair);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_MOUNT",			c_Costume_Slot_Mount);
#endif
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_ACCE",			c_Costume_Slot_Acce);
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_WEAPON",		c_Costume_Slot_Weapon);
#endif
	PyModule_AddIntConstant(poModule, "COSTUME_SLOT_END",			c_Costume_Slot_End);
#endif

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
	PyModule_AddIntConstant(poModule, "BELT_INVENTORY_SLOT_START",			c_Belt_Inventory_Slot_Start);
	PyModule_AddIntConstant(poModule, "BELT_INVENTORY_SLOT_COUNT",			c_Belt_Inventory_Slot_Count);
	PyModule_AddIntConstant(poModule, "BELT_INVENTORY_SLOT_END",			c_Belt_Inventory_Slot_End);

#endif

	PyModule_AddIntConstant(poModule, "WEAPON_SWORD",				WEAPON_SWORD);
	PyModule_AddIntConstant(poModule, "WEAPON_DAGGER",				WEAPON_DAGGER);
	PyModule_AddIntConstant(poModule, "WEAPON_BOW",					WEAPON_BOW);
	PyModule_AddIntConstant(poModule, "WEAPON_TWO_HANDED",			WEAPON_TWO_HANDED);
	PyModule_AddIntConstant(poModule, "WEAPON_BELL",				WEAPON_BELL);
	PyModule_AddIntConstant(poModule, "WEAPON_FAN",					WEAPON_FAN);
	PyModule_AddIntConstant(poModule, "WEAPON_ARROW",				WEAPON_ARROW);
	PyModule_AddIntConstant(poModule, "WEAPON_MOUNT_SPEAR",			WEAPON_MOUNT_SPEAR);
#ifdef ENABLE_WOLFMAN_CHARACTER
	PyModule_AddIntConstant(poModule, "WEAPON_CLAW",				WEAPON_CLAW);
#endif
#ifdef ENABLE_QUIVER_SYSTEM
	PyModule_AddIntConstant(poModule, "WEAPON_QUIVER",				WEAPON_QUIVER);
#endif
#ifdef __UNIMPLEMENTED__
	PyModule_AddIntConstant(poModule, "WEAPON_BOUQUET",				WEAPON_BOUQUET);
#endif
	PyModule_AddIntConstant(poModule, "WEAPON_NUM_TYPES",			WEAPON_NUM_TYPES);

	PyModule_AddIntConstant(poModule, "USE_POTION",					USE_POTION);
	PyModule_AddIntConstant(poModule, "USE_TALISMAN",				USE_TALISMAN);
	PyModule_AddIntConstant(poModule, "USE_TUNING",					USE_TUNING);
	PyModule_AddIntConstant(poModule, "USE_MOVE",					USE_MOVE);
	PyModule_AddIntConstant(poModule, "USE_TREASURE_BOX",			USE_TREASURE_BOX);
	PyModule_AddIntConstant(poModule, "USE_MONEYBAG",				USE_MONEYBAG);
	PyModule_AddIntConstant(poModule, "USE_BAIT",					USE_BAIT);
	PyModule_AddIntConstant(poModule, "USE_ABILITY_UP",				USE_ABILITY_UP);
	PyModule_AddIntConstant(poModule, "USE_AFFECT",					USE_AFFECT);
	PyModule_AddIntConstant(poModule, "USE_CREATE_STONE",			USE_CREATE_STONE);
	PyModule_AddIntConstant(poModule, "USE_SPECIAL",				USE_SPECIAL);
	PyModule_AddIntConstant(poModule, "USE_POTION_NODELAY",			USE_POTION_NODELAY);
	PyModule_AddIntConstant(poModule, "USE_CLEAR",					USE_CLEAR);
	PyModule_AddIntConstant(poModule, "USE_INVISIBILITY",			USE_INVISIBILITY);
	PyModule_AddIntConstant(poModule, "USE_DETACHMENT",				USE_DETACHMENT);
	PyModule_AddIntConstant(poModule, "USE_TIME_CHARGE_PER",		USE_TIME_CHARGE_PER);
	PyModule_AddIntConstant(poModule, "USE_TIME_CHARGE_FIX",		USE_TIME_CHARGE_FIX);
	PyModule_AddIntConstant(poModule, "USE_VIP",					USE_VIP);
	PyModule_AddIntConstant(poModule, "USE_REFINE_BONUS",			USE_REFINE_BONUS);

	PyModule_AddIntConstant(poModule, "MATERIAL_DS_REFINE_NORMAL",	MATERIAL_DS_REFINE_NORMAL);
	PyModule_AddIntConstant(poModule, "MATERIAL_DS_REFINE_BLESSED",	MATERIAL_DS_REFINE_BLESSED);
	PyModule_AddIntConstant(poModule, "MATERIAL_DS_REFINE_HOLLY",	MATERIAL_DS_REFINE_HOLLY);

	PyModule_AddIntConstant(poModule, "METIN_NORMAL",				METIN_NORMAL);
	PyModule_AddIntConstant(poModule, "METIN_GOLD",					METIN_GOLD);

	PyModule_AddIntConstant(poModule, "LIMIT_NONE",					LIMIT_NONE);
	PyModule_AddIntConstant(poModule, "LIMIT_LEVEL",				LIMIT_LEVEL);
	PyModule_AddIntConstant(poModule, "LIMIT_STR",					LIMIT_STR);
	PyModule_AddIntConstant(poModule, "LIMIT_DEX",					LIMIT_DEX);
	PyModule_AddIntConstant(poModule, "LIMIT_INT",					LIMIT_INT);
	PyModule_AddIntConstant(poModule, "LIMIT_CON",					LIMIT_CON);
	PyModule_AddIntConstant(poModule, "LIMIT_PCBANG",				LIMIT_PCBANG);
	PyModule_AddIntConstant(poModule, "LIMIT_REAL_TIME",			LIMIT_REAL_TIME);
	PyModule_AddIntConstant(poModule, "LIMIT_REAL_TIME_START_FIRST_USE",	LIMIT_REAL_TIME_START_FIRST_USE);
	PyModule_AddIntConstant(poModule, "LIMIT_TIMER_BASED_ON_WEAR",	LIMIT_TIMER_BASED_ON_WEAR);
	PyModule_AddIntConstant(poModule, "LIMIT_TYPE_MAX_NUM",			LIMIT_MAX_NUM);
	PyModule_AddIntConstant(poModule, "LIMIT_MAX_NUM",				ITEM_LIMIT_MAX_NUM);

	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_FEMALE",		ITEM_ANTIFLAG_FEMALE);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_MALE",			ITEM_ANTIFLAG_MALE);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_WARRIOR",		ITEM_ANTIFLAG_WARRIOR);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_ASSASSIN",		ITEM_ANTIFLAG_ASSASSIN);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_SURA",			ITEM_ANTIFLAG_SURA);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_SHAMAN",		ITEM_ANTIFLAG_SHAMAN);
#ifdef ENABLE_WOLFMAN_CHARACTER
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_WOLFMAN",		ITEM_ANTIFLAG_WOLFMAN);
#endif
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_GET",			ITEM_ANTIFLAG_GET);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_DROP",			ITEM_ANTIFLAG_DROP);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_SELL",			ITEM_ANTIFLAG_SELL);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_EMPIRE_A",		ITEM_ANTIFLAG_EMPIRE_A);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_EMPIRE_B",		ITEM_ANTIFLAG_EMPIRE_B);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_EMPIRE_C",		ITEM_ANTIFLAG_EMPIRE_C);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_SAVE",			ITEM_ANTIFLAG_SAVE);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_GIVE",			ITEM_ANTIFLAG_GIVE);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_PKDROP",		ITEM_ANTIFLAG_PKDROP);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_STACK",		ITEM_ANTIFLAG_STACK);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_MYSHOP",		ITEM_ANTIFLAG_MYSHOP);
	PyModule_AddIntConstant(poModule, "ITEM_ANTIFLAG_SAFEBOX",		ITEM_ANTIFLAG_SAFEBOX);

	PyModule_AddIntConstant(poModule, "ITEM_FLAG_REFINEABLE",			ITEM_FLAG_REFINEABLE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_SAVE",					ITEM_FLAG_SAVE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_STACKABLE",			ITEM_FLAG_STACKABLE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_COUNT_PER_1GOLD",		ITEM_FLAG_COUNT_PER_1GOLD);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_SLOW_QUERY",			ITEM_FLAG_SLOW_QUERY);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_RARE",					ITEM_FLAG_UNUSED01);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_UNIQUE",				ITEM_FLAG_UNIQUE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_MAKECOUNT",			ITEM_FLAG_MAKECOUNT);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_IRREMOVABLE",			ITEM_FLAG_IRREMOVABLE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_CONFIRM_WHEN_USE",		ITEM_FLAG_CONFIRM_WHEN_USE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_QUEST_USE",			ITEM_FLAG_QUEST_USE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_QUEST_USE_MULTIPLE",	ITEM_FLAG_QUEST_USE_MULTIPLE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_QUEST_GIVE",			ITEM_FLAG_QUEST_GIVE);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_LOG",					ITEM_FLAG_LOG);
	PyModule_AddIntConstant(poModule, "ITEM_FLAG_APPLICABLE",			ITEM_FLAG_APPLICABLE);

	PyModule_AddIntConstant(poModule, "ANTIFLAG_FEMALE",			ITEM_ANTIFLAG_FEMALE);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_MALE",				ITEM_ANTIFLAG_MALE);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_WARRIOR",			ITEM_ANTIFLAG_WARRIOR);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_ASSASSIN",			ITEM_ANTIFLAG_ASSASSIN);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_SURA",				ITEM_ANTIFLAG_SURA);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_SHAMAN",			ITEM_ANTIFLAG_SHAMAN);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_GET",				ITEM_ANTIFLAG_GET);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_DROP",				ITEM_ANTIFLAG_DROP);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_SELL",				ITEM_ANTIFLAG_SELL);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_EMPIRE_A",			ITEM_ANTIFLAG_EMPIRE_A);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_EMPIRE_B",			ITEM_ANTIFLAG_EMPIRE_B);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_EMPIRE_C",			ITEM_ANTIFLAG_EMPIRE_C);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_SAVE",				ITEM_ANTIFLAG_SAVE);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_GIVE",				ITEM_ANTIFLAG_GIVE);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_PKDROP",			ITEM_ANTIFLAG_PKDROP);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_STACK",				ITEM_ANTIFLAG_STACK);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_MYSHOP",			ITEM_ANTIFLAG_MYSHOP);
	PyModule_AddIntConstant(poModule, "ANTIFLAG_SAFEBOX",			ITEM_ANTIFLAG_SAFEBOX);
#ifdef ENABLE_WOLFMAN_CHARACTER
	PyModule_AddIntConstant(poModule, "ANTIFLAG_WOLFMAN",			ITEM_ANTIFLAG_WOLFMAN);
#endif

	PyModule_AddIntConstant(poModule, "WEARABLE_BODY",				WEARABLE_BODY);
	PyModule_AddIntConstant(poModule, "WEARABLE_HEAD",				WEARABLE_HEAD);
	PyModule_AddIntConstant(poModule, "WEARABLE_FOOTS",				WEARABLE_FOOTS);
	PyModule_AddIntConstant(poModule, "WEARABLE_WRIST",				WEARABLE_WRIST);
	PyModule_AddIntConstant(poModule, "WEARABLE_WEAPON",			WEARABLE_WEAPON);
	PyModule_AddIntConstant(poModule, "WEARABLE_NECK",				WEARABLE_NECK);
	PyModule_AddIntConstant(poModule, "WEARABLE_EAR",				WEARABLE_EAR);
	PyModule_AddIntConstant(poModule, "WEARABLE_UNIQUE",			WEARABLE_UNIQUE);
	PyModule_AddIntConstant(poModule, "WEARABLE_SHIELD",			WEARABLE_SHIELD);
	PyModule_AddIntConstant(poModule, "WEARABLE_ARROW",				WEARABLE_ARROW);
	PyModule_AddIntConstant(poModule, "WEARABLE_HAIR",				WEARABLE_HAIR);
	PyModule_AddIntConstant(poModule, "WEARABLE_ABILITY",			WEARABLE_ABILITY);
#ifdef ENABLE_PENDANT_SYSTEM
	PyModule_AddIntConstant(poModule, "WEARABLE_PENDANT",			WEARABLE_PENDANT);
#endif
#ifdef ENABLE_GLOVE_SYSTEM
	PyModule_AddIntConstant(poModule, "WEARABLE_GLOVE",				WEARABLE_GLOVE);
#endif

	PyModule_AddIntConstant(poModule, "ARMOR_BODY",					ARMOR_BODY);
	PyModule_AddIntConstant(poModule, "ARMOR_HEAD",					ARMOR_HEAD);
	PyModule_AddIntConstant(poModule, "ARMOR_SHIELD",				ARMOR_SHIELD);
	PyModule_AddIntConstant(poModule, "ARMOR_WRIST",				ARMOR_WRIST);
	PyModule_AddIntConstant(poModule, "ARMOR_FOOTS",				ARMOR_FOOTS);
	PyModule_AddIntConstant(poModule, "ARMOR_NECK",					ARMOR_NECK);
	PyModule_AddIntConstant(poModule, "ARMOR_EAR",					ARMOR_EAR);
#ifdef ENABLE_PENDANT_SYSTEM
	PyModule_AddIntConstant(poModule, "ARMOR_PENDANT",				ARMOR_PENDANT);
#endif
#ifdef ENABLE_GLOVE_SYSTEM
	PyModule_AddIntConstant(poModule, "ARMOR_GLOVE",				ARMOR_GLOVE);
#endif

	PyModule_AddIntConstant(poModule, "ITEM_APPLY_MAX_NUM",			ITEM_APPLY_MAX_NUM);
	PyModule_AddIntConstant(poModule, "ITEM_SOCKET_MAX_NUM",		ITEM_SOCKET_MAX_NUM);

#ifdef ENABLE_SEALBIND_SYSTEM
	PyModule_AddIntConstant(poModule, "E_SEAL_DATE_DEFAULT_TIMESTAMP",		CItemData::SEAL_DATE_DEFAULT_TIMESTAMP);
	PyModule_AddIntConstant(poModule, "E_SEAL_DATE_UNLIMITED_TIMESTAMP",	CItemData::SEAL_DATE_UNLIMITED_TIMESTAMP);
#endif

#ifdef ENABLE_PET_SYSTEM_EX
	PyModule_AddIntConstant(poModule, "PET_EGG",				PET_EGG);
	PyModule_AddIntConstant(poModule, "PET_UPBRINGING",			PET_UPBRINGING);
	PyModule_AddIntConstant(poModule, "PET_BAG",				PET_BAG);
	PyModule_AddIntConstant(poModule, "PET_FEEDSTUFF",			PET_FEEDSTUFF);
	PyModule_AddIntConstant(poModule, "PET_SKILL",				PET_SKILL);
	PyModule_AddIntConstant(poModule, "PET_SKILL_DEL_BOOK",		PET_SKILL_DEL_BOOK);
	PyModule_AddIntConstant(poModule, "PET_NAME_CHANGE",		PET_NAME_CHANGE);
	PyModule_AddIntConstant(poModule, "PET_EXPFOOD",			PET_EXPFOOD);
	PyModule_AddIntConstant(poModule, "PET_SKILL_ALL_DEL_BOOK", PET_SKILL_ALL_DEL_BOOK);
	PyModule_AddIntConstant(poModule, "PET_EXPFOOD_PER",		PET_EXPFOOD_PER);
	PyModule_AddIntConstant(poModule, "PET_ATTR_DETERMINE",		PET_ATTR_DETERMINE);
	PyModule_AddIntConstant(poModule, "PET_ATTR_CHANGE",		PET_ATTR_CHANGE);
	PyModule_AddIntConstant(poModule, "PET_PAY",				PET_PAY);
	PyModule_AddIntConstant(poModule, "PET_PRIMIUM_FEEDSTUFF",	PET_PRIMIUM_FEEDSTUFF);
#endif
	PyModule_AddIntConstant(poModule, "TUNING_FLAG_100_CHANCE", TUNING_FLAG_100_CHANCE);
	PyModule_AddIntConstant(poModule, "TUNING_FLAG_NO_GOLD", TUNING_FLAG_NO_GOLD);
	PyModule_AddIntConstant(poModule, "TUNING_FLAG_NO_ITEM", TUNING_FLAG_NO_ITEM);
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
