#pragma once

//        Icon, Model (droped on ground), Game Data

#include "../eterLib/GrpSubImage.h"
#include "../eterGrnLib/Thing.h"
#include "GameLibDefines.h"

#include "../../Server/common/length.h"
#include "../../Server/common/item_length.h"

typedef	DWORD		ITEM_COUNT;
typedef long long	YANG;

class CItemData
{
	public:
		enum
		{
			ITEM_SMALL_DESCR_MAX_LEN = 256,
#ifdef __UNIMPLEMENTED__
			ITEM_PICK_MAX_LEVEL = 9,
			ITEM_ROD_MAX_LEVEL = 20,
#endif
		};

#ifdef ENABLE_SOULBIND_SYSTEM
		enum ESealBind
		{
			SEAL_BIND_FLAG_DROP,
			SEAL_BIND_FLAG_UPGRADE,
			SEAL_BIND_FLAG_SELL,
			SEAL_BIND_FLAG_ENCHANT,
			SEAL_BIND_FLAG_TRADE,
			SEAL_BIND_FLAG_UNSEAL,
			SEAL_BIND_FLAG_MAX,
		};

		enum ESealDate
		{
			SEAL_DATE_DEFAULT_TIMESTAMP = 0, // 2015/11/12 it's -1
			SEAL_DATE_UNLIMITED_TIMESTAMP = -1, // 2015/11/12 it doesn't exist
		};

		enum ESealItem
		{
			SEAL_ITEM_BINDING_VNUM = 50263,
			SEAL_ITEM_UNBINDING_VNUM = 50264,
		};
#endif

#ifdef __UNIMPLEMENTED__
		enum EPetData
		{
			EGG_USE_SUCCESS = 0,
			EGG_USE_FAILED_TIMEOVER = 2,
			EGG_USE_FAILED_BECAUSE_NAME = 1,
			GROWTH_PET_ITEM_USE_COOL_TIME = 1,
			PET_EGG_USE_TRUE = 0,
			PET_EGG_USE_FAILED_BECAUSE_TRADING = 1,
			PET_EGG_USE_FAILED_BECAUSE_SHOP_OPEN = 2,
			PET_EGG_USE_FAILED_BECAUSE_MALL_OPEN = 3,
			PET_EGG_USE_FAILED_BECAUSE_SAFEBOX_OPEN = 4,
			PET_HATCHING_MONEY = 100000,
			PET_NAME_MAX_SIZE = 20,
			PET_NAME_MIN_SIZE = 4,
		};
#endif

#pragma pack(push)
#pragma pack(1)
		typedef struct SItemLimit
		{
			BYTE        bType;
			long        lValue;
		} TItemLimit;

		typedef struct SItemApply
		{
			BYTE        bType;
			long        lValue;
		} TItemApply;

		typedef struct SItemTable_r152
		{
			DWORD       dwVnum;
			char        szName[ITEM_NAME_MAX_LEN + 1];
			char        szLocaleName[ITEM_NAME_MAX_LEN + 1];
			BYTE        bType;
			BYTE        bSubType;

			BYTE        bWeight;
			BYTE        bSize;
			DWORD       dwMaxStack;

			DWORD       dwAntiFlags;
			DWORD       dwFlags;
			DWORD       dwWearFlags;
			DWORD       dwImmuneFlag;

			YANG       dwIBuyItemPrice;
			YANG		dwISellItemPrice;

			TItemLimit  aLimits[ITEM_LIMIT_MAX_NUM];
			TItemApply  aApplies[ITEM_APPLY_MAX_NUM];
			long        alValues[ITEM_VALUES_MAX_NUM];
			long        alSockets[ITEM_SOCKET_MAX_NUM];
			DWORD       dwRefinedVnum;
			WORD		wRefineSet;
			BYTE        bAlterToMagicItemPct;
			BYTE		bSpecular;
			BYTE        bGainSocketPct;
		} TItemTable_r152;

		typedef struct SItemTable_r156
		{
			DWORD       dwVnum;
			DWORD       dwVnumRange;
			char        szName[ITEM_NAME_MAX_LEN + 1];
			char        szLocaleName[ITEM_NAME_MAX_LEN + 1];
			BYTE        bType;
			BYTE        bSubType;

			BYTE        bWeight;
			BYTE        bSize;
			DWORD       dwMaxStack;

			DWORD       dwAntiFlags;
			DWORD       dwFlags;
			DWORD       dwWearFlags;
			DWORD       dwImmuneFlag;

			YANG	    dwIBuyItemPrice;
			YANG		dwISellItemPrice;

			TItemLimit  aLimits[ITEM_LIMIT_MAX_NUM];
			TItemApply  aApplies[ITEM_APPLY_MAX_NUM];
			long        alValues[ITEM_VALUES_MAX_NUM];
			long        alSockets[ITEM_SOCKET_MAX_NUM];
			DWORD       dwRefinedVnum;
			WORD		wRefineSet;
			BYTE        bAlterToMagicItemPct;
			BYTE		bSpecular;
			BYTE        bGainSocketPct;
		} TItemTable_r156;

		typedef struct SItemTable_r158
		{
			DWORD       dwVnum;
			DWORD       dwVnumRange;
			char        szName[ITEM_NAME_MAX_LEN + 1];
			char        szLocaleName[ITEM_NAME_MAX_LEN + 1];
			BYTE        bType;
			BYTE        bSubType;

			BYTE        bWeight;
			BYTE        bSize;
			DWORD       dwMaxStack;

			DWORD       dwAntiFlags;
			DWORD       dwFlags;
			DWORD       dwWearFlags;
			DWORD       dwImmuneFlag;

			YANG       dwIBuyItemPrice;
			YANG		dwISellItemPrice;

			TItemLimit  aLimits[ITEM_LIMIT_MAX_NUM];
			TItemApply  aApplies[ITEM_APPLY_MAX_NUM];
			long        alValues[ITEM_VALUES_MAX_NUM];
			long        alSockets[ITEM_SOCKET_MAX_NUM];
			DWORD       dwRefinedVnum;
			WORD		wRefineSet;
			BYTE        bAlterToMagicItemPct;
			BYTE		bSpecular;
			BYTE        bGainSocketPct;
			WORD		wWearableFlag;
		} TItemTable_r158;

		typedef SItemTable_r156 SItemTable, TItemTable;

#ifdef ENABLE_PROTOSTRUCT_AUTODETECT
		typedef struct SItemTableAll
		{
			static bool IsValidStruct(DWORD structSize)
			{
				switch (structSize)
				{
					case sizeof(TItemTable_r152):
					case sizeof(TItemTable_r156):
					case sizeof(TItemTable_r158):
						return true;
						break;
				}
				return false;
			}

			static void Process(void* obj, DWORD structSize, DWORD i, CItemData::TItemTable& t)
			{
				#define ITABLE_COPY_STR(x) strncpy_s(t.##x##, sizeof(t.##x##), r.##x##, _TRUNCATE)
				#define ITABLE_COPY_INT(x) t.##x## = r.##x
				#define ITABLE_ZERO_STR(x) memset(t.##x##, 0, sizeof(t.##x##));
				#define ITABLE_ZERO_INT(x) t.##x## = 0
				#define ITABLE_COUNT(x) _countof(t.##x##)
				#define ITABLE_PROCESS(len)\
					CItemData::TItemTable_r##len## & r = *((CItemData::TItemTable_r##len## *) obj + i);\
					ITABLE_COPY_INT(dwVnum);\
					ITABLE_COPY_STR(szName);\
					ITABLE_COPY_STR(szLocaleName);\
					ITABLE_COPY_INT(bType);\
					ITABLE_COPY_INT(bSubType);\
					ITABLE_COPY_INT(bWeight);\
					ITABLE_COPY_INT(bSize);\
					ITABLE_COPY_INT(dwMaxStack);\
					ITABLE_COPY_INT(dwAntiFlags);\
					ITABLE_COPY_INT(dwFlags);\
					ITABLE_COPY_INT(dwWearFlags);\
					ITABLE_COPY_INT(dwImmuneFlag);\
					ITABLE_COPY_INT(dwIBuyItemPrice);\
					ITABLE_COPY_INT(dwISellItemPrice);\
					for (size_t i=0; i<ITABLE_COUNT(aLimits); ++i)\
					{\
						ITABLE_COPY_INT(aLimits[i].bType);\
						ITABLE_COPY_INT(aLimits[i].lValue);\
					}\
					for (size_t i=0; i<ITEM_APPLY_MAX_NUM; ++i)\
					{\
						ITABLE_COPY_INT(aApplies[i].bType);\
						ITABLE_COPY_INT(aApplies[i].lValue);\
					}\
					for (size_t i=0; i<ITEM_VALUES_MAX_NUM; ++i)\
					{\
						ITABLE_COPY_INT(alValues[i]);\
					}\
					for (size_t i=0; i<ITEM_SOCKET_MAX_NUM; ++i)\
					{\
						ITABLE_COPY_INT(alSockets[i]);\
					}\
					ITABLE_COPY_INT(dwRefinedVnum);\
					ITABLE_COPY_INT(wRefineSet);\
					ITABLE_COPY_INT(bAlterToMagicItemPct);\
					ITABLE_COPY_INT(bSpecular);\
					ITABLE_COPY_INT(bGainSocketPct);

				switch (structSize)
				{
					case sizeof(TItemTable_r152):
						{
							ITABLE_PROCESS(152);
							ITABLE_ZERO_INT(dwVnumRange);
						}
						break;
					case sizeof(TItemTable_r156):
						{
							ITABLE_PROCESS(156);
							ITABLE_COPY_INT(dwVnumRange);
						}
						break;
					case sizeof(TItemTable_r158):
						{
							ITABLE_PROCESS(158);
							ITABLE_COPY_INT(dwVnumRange);
						}
						break;
				}
			}
		} TItemTableAll;
#endif //ENABLE_PROTOSTRUCT_AUTODETECT

#pragma pack(pop)

	public:
		CItemData();
		virtual ~CItemData();

		void Clear();
		void SetSummary(const std::string& c_rstSumm);
		void SetDescription(const std::string& c_rstDesc);

		void SetLocaleName(const std::string& c_rstName);

		CGraphicThing * GetModelThing();
		CGraphicThing * GetSubModelThing();
		CGraphicThing * GetDropModelThing();
		CGraphicSubImage * GetIconImage();

		DWORD GetLODModelThingCount();
		BOOL GetLODModelThingPointer(DWORD dwIndex, CGraphicThing ** ppModelThing);

		DWORD GetAttachingDataCount();
		BOOL GetCollisionDataPointer(DWORD dwIndex, const NRaceData::TAttachingData ** c_ppAttachingData);
		BOOL GetAttachingDataPointer(DWORD dwIndex, const NRaceData::TAttachingData ** c_ppAttachingData);

		/////
		const TItemTable*	GetTable() const;
		DWORD GetIndex() const;
		const char * GetOriginalName() const;
		const char * GetName() const;
		const char * GetDescription() const;
		const char * GetSummary() const;
		BYTE GetType() const;
		BYTE GetSubType() const;
		DWORD GetMaxStack() const;
		UINT GetRefine() const;
		const char* GetUseTypeString() const;
		DWORD GetWeaponType() const;
		BYTE GetSize() const;
		BOOL IsAntiFlag(DWORD dwFlag) const;
		BOOL IsFlag(DWORD dwFlag) const;
		BOOL IsWearableFlag(DWORD dwFlag) const;
		BOOL HasNextGrade() const;
		DWORD GetWearFlags() const;
		YANG GetIBuyItemPrice() const;
		YANG GetISellItemPrice() const;
		BOOL GetLimit(BYTE byIndex, TItemLimit * pItemLimit) const;
		BOOL GetApply(BYTE byIndex, TItemApply * pItemApply) const;
		long GetValue(BYTE byIndex) const;
		long GetSocket(BYTE byIndex) const;
		long SetSocket(BYTE byIndex,DWORD value);
		int GetSocketCount() const;
		DWORD GetIconNumber() const;

		UINT	GetSpecularPoweru() const;
		float	GetSpecularPowerf() const;

		/////

		BOOL IsEquipment() const;

		/////

		//BOOL LoadItemData(const char * c_szFileName);
		void SetDefaultItemData(const char * c_szIconFileName, const char * c_szModelFileName  = NULL);
		void SetItemTableData(TItemTable * pItemTable);

	protected:
		void __LoadFiles();
		void __SetIconImage(const char * c_szFileName);

	protected:
		std::string m_strLocaleName;
		std::string m_strModelFileName;
		std::string m_strSubModelFileName;
		std::string m_strDropModelFileName;
		std::string m_strIconFileName;
		std::string m_strDescription;
		std::string m_strSummary;
		std::vector<std::string> m_strLODModelFileNameVector;

		CGraphicThing * m_pModelThing;
		CGraphicThing * m_pSubModelThing;
		CGraphicThing * m_pDropModelThing;
		CGraphicSubImage * m_pIconImage;
		std::vector<CGraphicThing *> m_pLODModelThingVector;

		NRaceData::TAttachingDataVector m_AttachingDataVector;
		DWORD		m_dwVnum;
		TItemTable m_ItemTable;

	public:
		static void DestroySystem();

		static CItemData* New();
		static void Delete(CItemData* pkItemData);

		static CDynamicPool<CItemData>		ms_kPool;

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	protected:
		typedef struct SItemScaleTable
		{
			D3DXVECTOR3 v3Scale[NRaceData::SEX_MAX_NUM][NRaceData::JOB_MAX_NUM]{};
			float fScaleParticle[NRaceData::SEX_MAX_NUM][NRaceData::JOB_MAX_NUM]{};
		} TItemScaleTable;
		TItemScaleTable m_ItemScaleTable;

	public:
		float GetItemParticleScale(BYTE bJob, BYTE bSex);
		void SetItemTableScaleData(BYTE bJob, BYTE bSex, float fScaleX, float fScaleY, float fScaleZ, float fScaleParticle);
		D3DXVECTOR3& GetItemScaleVector(BYTE bJob, BYTE bSex);
#endif
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
