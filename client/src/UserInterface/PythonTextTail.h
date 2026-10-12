#pragma once

#include "../eterBase/Singleton.h"

class CPythonTextTail : public CSingleton<CPythonTextTail>
{
	public:
		typedef struct STextTail
		{
			CGraphicTextInstance*			pTextInstance;
			CGraphicTextInstance*			pOwnerTextInstance;

			CGraphicMarkInstance*			pMarkInstance;
			CGraphicTextInstance*			pGuildNameTextInstance;

			CGraphicTextInstance*			pTitleTextInstance;
			CGraphicTextInstance*			pLevelTextInstance;

			CGraphicObjectInstance *		pOwner;

			DWORD							dwVirtualID;

			float							x, y, z;
			float							fDistanceFromPlayer;
			D3DXCOLOR						Color;
			BOOL							bNameFlag;

			float							xStart, yStart;
			float							xEnd, yEnd;

			DWORD							LivingTime;

			float							fHeight;

			std::string groupFontName;
			int groupCount;

			STextTail() {}
			virtual ~STextTail() {}
		} TTextTail;

		typedef std::map<DWORD, TTextTail*>		TTextTailMap;
		typedef std::list<TTextTail*>			TTextTailList;
		typedef TTextTailMap					TChatTailMap;

		typedef struct SGroupedTextTail
		{
			bool isCharacterTextTail;
			std::vector<TTextTail*> textTailList;
		} GroupedTextTail;

	public:
		CPythonTextTail(void);
		virtual ~CPythonTextTail(void);

		void GetInfo(std::string* pstInfo);

		void Initialize();
		void Destroy();
		void Clear();

		void UpdateAllTextTail();
		void UpdateShowingTextTail();
		void Render();

		void AddToGroupedTextTailMap(std::unordered_map<std::string, GroupedTextTail>& groupedTextTailMap, TTextTailMap textTailMap, bool isCharacterTextTail);

		void ArrangeTextTail();
		void HideAllTextTail();
		void ShowAllTextTail();
		void ShowCharacterTextTail(DWORD VirtualID);
		void ShowItemTextTail(DWORD VirtualID);

		void RegisterCharacterTextTail(DWORD dwGuildID, DWORD dwVirtualID, const D3DXCOLOR & c_rColor, float fAddHeight=10.0f, bool bLarge=false);
		void RegisterItemTextTail(DWORD VirtualID, const char * c_szText, CGraphicObjectInstance * pOwner, ITEM_COUNT count=0, bool bIsAttr = false);
		void RegisterChatTail(DWORD VirtualID, const char * c_szChat);
		void RegisterInfoTail(DWORD VirtualID, const char * c_szChat);
		void SetCharacterTextTailColor(DWORD VirtualID, const D3DXCOLOR & c_rColor);
		void SetItemTextTailOwner(DWORD dwVID, const char * c_szName);
		void DeleteCharacterTextTail(DWORD VirtualID);
		void DeleteItemTextTail(DWORD VirtualID);

		int Pick(int ixMouse, int iyMouse);
		void SelectItemName(DWORD dwVirtualID);

		bool GetTextTailPosition(DWORD dwVID, float* px, float* py, float* pz);
		bool IsChatTextTail(DWORD dwVID);

		void EnablePKTitle(BOOL bFlag);
		void AttachTitle(DWORD dwVID, const char * c_szName, const D3DXCOLOR& c_rColor);
		void DetachTitle(DWORD dwVID);

		void AttachLevel(DWORD dwVID, const char* c_szText, const D3DXCOLOR& c_rColor);
		void DetachLevel(DWORD dwVID);

	protected:
		TTextTail * RegisterTextTail(DWORD dwVirtualID, const char * c_szText, CGraphicObjectInstance * pOwner, float fHeight, const D3DXCOLOR & c_rColor, bool bLarge=false);
		void DeleteTextTail(TTextTail * pTextTail);

		void UpdateTextTail(TTextTail * pTextTail);
		void RenderTextTailBox(TTextTail * pTextTail);
		void RenderTextTailName(TTextTail * pTextTail);
		void UpdateDistance(const TPixelPosition & c_rCenterPosition, TTextTail * pTextTail);

		bool isIn(TTextTail * pSource, TTextTail * pTarget);

	protected:
		TTextTailMap				m_CharacterTextTailMap;
		TTextTailMap				m_ItemTextTailMap;
		TChatTailMap				m_ChatTailMap;

		TTextTailList				m_CharacterTextTailList;
		TTextTailList				m_ItemTextTailList;

	private:
		CDynamicPool<STextTail>		m_TextTailPool;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
