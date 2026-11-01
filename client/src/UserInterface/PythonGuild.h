#pragma once

#include "Packet.h"

class CPythonGuild : public CSingleton<CPythonGuild>
{
	public:
		enum
		{
			GUILD_SKILL_MAX_NUM = 12,
			ENEMY_GUILD_SLOT_MAX_COUNT = 6,
		};

		typedef struct SGulidInfo
		{
			uint64_t dwGuildID;
			char szGuildName[GUILD_NAME_MAX_LEN+1];
			uint64_t dwMasterPID;
			DWORD dwGuildLevel;
			DWORD dwCurrentExperience;
			DWORD dwCurrentMemberCount;
			DWORD dwMaxMemberCount;
			YANG dwGuildMoney;
			BOOL bHasLand;
		} TGuildInfo;

		typedef struct SGuildGradeData
		{
			SGuildGradeData(){}
			SGuildGradeData(BYTE byAuthorityFlag_, const char * c_szName_) : byAuthorityFlag(byAuthorityFlag_), strName(c_szName_) {}
			BYTE byAuthorityFlag;
			std::string strName;
		} TGuildGradeData;
		typedef std::map<BYTE, TGuildGradeData> TGradeDataMap;

		typedef struct SGuildMemberData
		{
			uint64_t dwPID;

			std::string strName;
			BYTE byGrade;
			BYTE byJob;
			BYTE byLevel;
			BYTE byGeneralFlag;
			DWORD dwOffer;
		} TGuildMemberData;
		typedef std::vector<TGuildMemberData> TGuildMemberDataVector;

		typedef struct SGuildBoardCommentData
		{
			DWORD dwCommentID;
			std::string strName;
			std::string strComment;
		} TGuildBoardCommentData;
		typedef std::vector<TGuildBoardCommentData> TGuildBoardCommentDataVector;

		typedef struct SGuildSkillData
		{
			BYTE bySkillPoint;
			BYTE bySkillLevel[GUILD_SKILL_MAX_NUM];
			WORD wGuildPoint;
			WORD wMaxGuildPoint;
		} TGuildSkillData;

		typedef std::map<uint64_t, std::string> TGuildNameMap;

	public:
		CPythonGuild();
		virtual ~CPythonGuild();

		void Destroy();

		void EnableGuild();
		void SetGuildMoney(YANG dwMoney);
		void SetGuildEXP(BYTE byLevel, DWORD dwEXP);
		void SetGradeData(BYTE byGradeNumber, const TGuildGradeData & rGuildGradeData);
		void SetGradeName(BYTE byGradeNumber, const char * c_szName);
		void SetGradeAuthority(BYTE byGradeNumber, BYTE byAuthority);
		void ClearComment();
		void RegisterComment(DWORD dwCommentID, const char * c_szName, const char * c_szComment);
		void RegisterMember(TGuildMemberData & rGuildMemberData);
		void ChangeGuildMemberGrade(uint64_t dwPID, BYTE byGrade);
		void ChangeGuildMemberGeneralFlag(uint64_t dwPID, BYTE byFlag);
		void RemoveMember(uint64_t dwPID);
		void RegisterGuildName(uint64_t dwID, const char * c_szName);

		BOOL IsMainPlayer(uint64_t dwPID);
		BOOL IsGuildEnable();
		TGuildInfo & GetGuildInfoRef();
		BOOL GetGradeDataPtr(DWORD dwGradeNumber, TGuildGradeData ** ppData);
		const TGuildBoardCommentDataVector & GetGuildBoardCommentVector();
		DWORD GetMemberCount();
		BOOL GetMemberDataPtr(DWORD dwIndex, TGuildMemberData ** ppData);
		BOOL GetMemberDataPtrByPID(uint64_t dwPID, TGuildMemberData ** ppData);
		BOOL GetMemberDataPtrByName(const char * c_szName, TGuildMemberData ** ppData);
		DWORD GetGuildMemberLevelSummary();
		DWORD GetGuildMemberLevelAverage();
		DWORD GetGuildExperienceSummary();
		TGuildSkillData & GetGuildSkillDataRef();
		bool GetGuildName(uint64_t dwID, std::string * pstrGuildName);
		uint64_t GetGuildID();
		BOOL HasGuildLand();

		void StartGuildWar(uint64_t dwEnemyGuildID);
		void EndGuildWar(uint64_t dwEnemyGuildID);
		uint64_t GetEnemyGuildID(DWORD dwIndex);
		BOOL IsDoingGuildWar();

	protected:
		void __CalculateLevelAverage();
		void __SortMember();
		BOOL __IsGradeData(BYTE byGradeNumber);

		void __Initialize();

	protected:
		TGuildInfo m_GuildInfo;
		TGradeDataMap m_GradeDataMap;
		TGuildMemberDataVector m_GuildMemberDataVector;
		TGuildBoardCommentDataVector m_GuildBoardCommentVector;
		TGuildSkillData m_GuildSkillData;
		TGuildNameMap m_GuildNameMap;
		uint64_t m_adwEnemyGuildID[ENEMY_GUILD_SLOT_MAX_COUNT];

		DWORD m_dwMemberLevelSummary;
		DWORD m_dwMemberLevelAverage;
		DWORD m_dwMemberExperienceSummary;

		BOOL m_bGuildEnable;
};

static_assert(sizeof(CPythonGuild::TGuildInfo::dwGuildID) == 8, "TGuildInfo::dwGuildID must be 64-bit");
static_assert(sizeof(CPythonGuild::TGuildInfo::dwMasterPID) == 8, "TGuildInfo::dwMasterPID must be 64-bit");
static_assert(sizeof(CPythonGuild::TGuildMemberData::dwPID) == 8, "TGuildMemberData::dwPID must be 64-bit");
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
