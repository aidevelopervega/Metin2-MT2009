#pragma once

#include <windows.h>
#include <boost/unordered_map.hpp>
#include "../eterBase/Singleton.h"
#include "../eterBase/Stl.h"

#include "EterPack.h"

class CEterPackManager : public CSingleton<CEterPackManager>
{
	public:
		struct SCache
		{
			BYTE* m_abBufData;
			DWORD m_dwBufSize;
		};
	public:
		enum ESearchModes
		{
			SEARCH_FILE_FIRST,
			SEARCH_PACK_FIRST
		};

		enum ETextureType
		{
			DEFAULT_TEXTURES,
			OLD_EFFECTS,
			OLD_TEXTURES,
			OLD_TEXTURES_EFFECTS
		};

		typedef std::list<CEterPack*> TEterPackList;
		typedef boost::unordered_map<std::string, CEterPack*, stringhash> TEterPackMap;

	public:
		CEterPackManager();
		virtual ~CEterPackManager();

		void SetCacheMode();
		void SetRelativePathMode();

		void LoadStaticCache(const char* c_szFileName);

		void SetSearchMode(bool bPackFirst);
		int GetSearchMode();

		//THEMIDA
		bool Get(CMappedFile & rMappedFile, const char * c_szFileName, LPCVOID * pData);

		//THEMIDA
		bool GetFromPack(CMappedFile & rMappedFile, const char * c_szFileName, LPCVOID * pData);

		//THEMIDA
		bool GetFromFile(CMappedFile & rMappedFile, const char * c_szFileName, LPCVOID * pData);
		bool isExist(const char * c_szFileName);
		bool isExistInPack(const char * c_szFileName);

		bool RegisterPack(const char * c_szName, const char * c_szDirectory, const BYTE* c_pbIV = NULL);
		void RegisterRootPack(const char * c_szName);
		bool RegisterPackWhenPackMaking(const char * c_szName, const char * c_szDirectory, CEterPack* pPack);

		void SetTextureViewType(int type) { m_iTextureViewType = type; }

		bool DecryptPackIV(DWORD key);

		const char * GetRootPackFileName();

		//for hybridcrypt
		//THEMIDA
		void WriteHybridCryptPackInfo(const char* pFileName);

		//THEMIDA
		void RetrieveHybridCryptPackKeys( const BYTE* pStream );
		//THEMIDA
		void RetrieveHybridCryptPackSDB( const BYTE* pStream );

	public:
		void ArrangeMemoryMappedPack();

	protected:
		int ConvertFileName(const char * c_szFileName, std::string & rstrFileName);
#ifdef ENABLE_DEBUG_PACK_UNPACKED
		void DebugConvertFileName(const char* c_szFileName, std::string& rstrFileName);
#endif
		void ConvertToOldFileName(std::string& rstrFileName);

		bool CompareName(const char * c_szDirectoryName, DWORD iLength, const char * c_szFileName);

		CEterPack* FindPack(const char* c_szPathName);

		SCache* __FindCache(DWORD dwFileNameHash);
		void	__ClearCacheMap();

	protected:
		bool					m_bTryRelativePath;
		bool					m_isCacheMode;
		int						m_iSearchMode;

		CEterFileDict			m_FileDict;
		CEterPack				m_RootPack;
		TEterPackList			m_PackList;
		TEterPackMap			m_PackMap;
		TEterPackMap			m_DirPackMap;

		boost::unordered_map<DWORD, SCache> m_kMap_dwNameKey_kCache;

		CRITICAL_SECTION		m_csFinder;
		int						m_iTextureViewType;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
