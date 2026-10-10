#pragma once

class CPythonSafeBox : public CSingleton<CPythonSafeBox>
{
	public:
		typedef std::vector<TItemData> TItemInstanceVector;

	public:
		CPythonSafeBox();
		virtual ~CPythonSafeBox();

		void OpenSafeBox(int iSize);
		void SetItemData(DWORD dwSlotIndex, const TItemData & rItemData);
		void DelItemData(DWORD dwSlotIndex);

		void SetMoney(DWORD dwMoney);
		DWORD GetMoney();

		BOOL GetSlotItemID(DWORD dwSlotIndex, DWORD* pdwItemID);

		int GetCurrentSafeBoxSize();
		BOOL GetItemDataPtr(DWORD dwSlotIndex, TItemData ** ppInstance);

		// MALL
		void OpenMall(int iSize);
		void SetMallItemData(DWORD dwSlotIndex, const TItemData & rItemData);
		void DelMallItemData(DWORD dwSlotIndex);
		BOOL GetMallItemDataPtr(DWORD dwSlotIndex, TItemData ** ppInstance);
		BOOL GetSlotMallItemID(DWORD dwSlotIndex, DWORD * pdwItemID);
		DWORD GetMallSize();

	protected:
		TItemInstanceVector m_ItemInstanceVector;
		TItemInstanceVector m_MallItemInstanceVector;
		DWORD m_dwMoney;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
