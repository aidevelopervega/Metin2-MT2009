#ifndef __INC_METIN_II_GUILDLIB_MARK_MANAGER_H__
#define __INC_METIN_II_GUILDLIB_MARK_MANAGER_H__

#include "MarkImage.h"

class CGuildMarkManager : public singleton<CGuildMarkManager>
{
	public:
		enum
		{
			MAX_IMAGE_COUNT = 5,
			INVALID_MARK_ID = 0xffffffff,
		};

		// Symbol
		struct TGuildSymbol
		{
			DWORD crc;
			std::vector<BYTE> raw;
		};

		CGuildMarkManager();
		virtual ~CGuildMarkManager();

		const TGuildSymbol * GetGuildSymbol(uint64_t GID);
		bool LoadSymbol(const char* filename);
		void SaveSymbol(const char* filename);
		void UploadSymbol(uint64_t guildID, int iSize, const BYTE* pbyData);

		// Mark

		void SetMarkPathPrefix(const char * prefix);

		bool LoadMarkIndex();
		bool SaveMarkIndex();

		void LoadMarkImages();
		void SaveMarkImage(DWORD imgIdx);

		bool GetMarkImageFilename(DWORD imgIdx, std::string & path) const;
		bool AddMarkIDByGuildID(uint64_t guildID, DWORD markID);
		DWORD GetMarkImageCount() const;
		DWORD GetMarkCount() const;
		DWORD GetMarkID(uint64_t guildID);

		// SERVER
		void CopyMarkIdx(char * pcBuf) const;
		DWORD SaveMark(uint64_t guildID, BYTE * pbMarkImage);
		void DeleteMark(uint64_t guildID);
		void GetDiffBlocks(DWORD imgIdx, const DWORD * crcList, std::map<BYTE, const SGuildMarkBlock *> & mapDiffBlocks);

		// CLIENT
		bool SaveBlockFromCompressedData(DWORD imgIdx, DWORD idBlock, const BYTE * pbBlock, DWORD dwSize);
		bool GetBlockCRCList(DWORD imgIdx, DWORD * crcList);

	private:

		// Mark

		CGuildMarkImage * __NewImage();
		void __DeleteImage(CGuildMarkImage * pkImgDel);

		DWORD __AllocMarkID(uint64_t guildID);

		CGuildMarkImage * __GetImage(DWORD imgIdx);
		CGuildMarkImage * __GetImagePtr(DWORD idMark);

		std::map<DWORD, CGuildMarkImage *> m_mapIdx_Image; // index = image index
		std::map<uint64_t, DWORD> m_mapGID_MarkID; // index = guild id

		std::set<DWORD> m_setFreeMarkID;
		std::string		m_pathPrefix;

	private:

		// Symbol

		std::map<uint64_t, TGuildSymbol> m_mapSymbol;
};

#endif
