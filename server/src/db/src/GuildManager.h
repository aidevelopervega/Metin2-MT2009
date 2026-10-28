// vim:ts=8 sw=4
#ifndef __INC_GUILD_MANAGER_H
#define __INC_GUILD_MANAGER_H

#include "Peer.h"
#include <queue>
#include <utility>
#if __has_include("../../../../../server/src/libsql/libsql.h")
#include "../../../../../server/src/libsql/libsql.h"
#elif __has_include("../../../../server/src/libsql/libsql.h")
#include "../../../../server/src/libsql/libsql.h"
#elif __has_include("../../libsql/libsql.h")
#include "../../libsql/libsql.h"
#else
#include <libsql.h>
#endif
#if __has_include(<Poly.h>)
#include <Poly.h>
#elif __has_include("../../../../../server/src/libpoly/Poly.h")
#include "../../../../../server/src/libpoly/Poly.h"
#elif __has_include("../../../../server/src/libpoly/Poly.h")
#include "../../../../server/src/libpoly/Poly.h"
#elif __has_include("../../libpoly/Poly.h")
#include "../../libpoly/Poly.h"
#else
#include "Poly.h"
#endif

enum
{
    GUILD_WARP_WAR_CHANNEL = 99
};

class CGuildWarReserve;

struct TGuildDeclareInfo
{
	BYTE bType;
	uint64_t dwGuildID[2];

	TGuildDeclareInfo(BYTE _bType, uint64_t _dwGuildID1, uint64_t _dwGuildID2)
		: bType(_bType)
	{
		dwGuildID[0] = _dwGuildID1;
		dwGuildID[1] = _dwGuildID2;
	}

	bool operator < (const TGuildDeclareInfo& r) const
	{
		return ((dwGuildID[0] < r.dwGuildID[0]) || ((dwGuildID[0] == r.dwGuildID[0]) && (dwGuildID[1] < r.dwGuildID[1])));
	}

	TGuildDeclareInfo& operator = (const TGuildDeclareInfo& r)
	{
		bType = r.bType;
		dwGuildID[0] = r.dwGuildID[0];
		dwGuildID[1] = r.dwGuildID[1];
		return *this;
	}
};

struct TGuildWaitStartInfo
{
	BYTE			bType;
	uint64_t		GID[2];
	YANG			lWarPrice;
	long			lInitialScore;
	CGuildWarReserve *	pkReserve;

	TGuildWaitStartInfo(BYTE _bType,
			uint64_t _g1,
			uint64_t _g2,
			YANG _lWarPrice,
			long _lInitialScore,
			CGuildWarReserve * _pkReserve)
		: bType(_bType), lWarPrice(_lWarPrice), lInitialScore(_lInitialScore), pkReserve(_pkReserve)
	{
		GID[0] = _g1;
		GID[1] = _g2;
	}

	bool operator < (const TGuildWaitStartInfo& r) const
	{
		return ((GID[0] < r.GID[0]) || ((GID[0] == r.GID[0]) && (GID[1] < r.GID[1])));
	}
};

struct TGuildWarPQElement
{
	bool	bEnd;
	BYTE	bType;
	uint64_t	GID[2];
	int	iScore[2];
	int	iBetScore[2];

	TGuildWarPQElement(BYTE _bType, uint64_t GID1, uint64_t GID2) : bEnd(false), bType(_bType)
	{
		bType = _bType;
		GID[0] = GID1;
		GID[1] = GID2;
		iScore[0] = iScore[1] = 0;
		iBetScore[0] = iBetScore[1] = 0;
	}
};

struct TGuildSkillUsed
{
    uint64_t GID;
    DWORD dwSkillVnum;

    // GUILD_SKILL_COOLTIME_BUG_FIX
    TGuildSkillUsed(uint64_t _GID, DWORD _dwSkillVnum) : GID(_GID), dwSkillVnum(_dwSkillVnum)
    {
    }
    // END_OF_GUILD_SKILL_COOLTIME_BUG_FIX
};

inline bool operator < (const TGuildSkillUsed& a, const TGuildSkillUsed& b)
{
    return ((a.GID < b.GID) || ((a.GID == b.GID) && (a.dwSkillVnum < b.dwSkillVnum)));
}

typedef struct SGuild
{
	SGuild() : ladder_point(0), win(0), draw(0), loss(0), gold(0), level(0)
	{
		memset(szName, 0, sizeof(szName));
	}

	char	szName[GUILD_NAME_MAX_LEN+1];
	int		ladder_point;
	int		win;
	int		draw;
	int		loss;
	YANG	gold;
	int		level;
} TGuild;

typedef struct SGuildWarInfo
{
    time_t		tEndTime;
    TGuildWarPQElement * pElement;
    CGuildWarReserve * pkReserve;

    SGuildWarInfo() : pElement(NULL)
    {
    }
} TGuildWarInfo;

class CGuildWarReserve
{
    public:
	CGuildWarReserve(const TGuildWarReserve& rTable);

	void Initialize();

	TGuildWarReserve & GetDataRef()
	{
	    return m_data;
	}

	void	OnSetup(CPeer * peer);
	bool	Bet(const char * pszLogin, YANG dwGold, uint64_t dwGuild);
	void	Draw();
	void	End(int iScoreFrom, int iScoreTo);

	int	GetLastNoticeMin() { return m_iLastNoticeMin; }
	void	SetLastNoticeMin(int iMin) { m_iLastNoticeMin = iMin; }

    private:
	CGuildWarReserve();

	TGuildWarReserve				m_data;
	// <login, <guild, gold>>
	std::map<std::string, std::pair<uint64_t, YANG> > mapBet;
	int						m_iLastNoticeMin;
};

class CGuildManager : public singleton<CGuildManager>
{
    public:
	CGuildManager();
	virtual ~CGuildManager();

	void	Initialize();

	void	Load(uint64_t dwGuildID);

	TGuild & TouchGuild(uint64_t GID);

	void	Update();

	void	OnSetup(CPeer * peer);
	void	StartWar(BYTE bType, uint64_t GID1, uint64_t GID2, CGuildWarReserve * pkReserve = NULL);

	void	UpdateScore(uint64_t guild_gain_point, uint64_t guild_opponent, int iScore, int iBetScore);

	void	AddDeclare(BYTE bType, uint64_t guild_from, uint64_t guild_to);
	void	RemoveDeclare(uint64_t guild_from, uint64_t guild_to);

	bool	TakeBetPrice(uint64_t dwGuildTo, uint64_t dwGuildFrom, YANG lWarPrice);

	bool	WaitStart(TPacketGuildWar * p);

	void	RecvWarEnd(uint64_t GID1, uint64_t GID2);
	void	RecvWarOver(uint64_t dwGuildWinner, uint64_t dwGuildLoser, bool bDraw, YANG lWarPrice);

	void	ChangeLadderPoint(uint64_t GID, int change);

	void	UseSkill(uint64_t dwGuild, DWORD dwSkillVnum, DWORD dwCooltime);

	YANG	GetGuildGold(uint64_t dwGuild);
	void	DepositMoney(uint64_t dwGuild, YANG lGold);
	void	WithdrawMoney(CPeer* peer, uint64_t dwGuild, YANG lGold);
	void	WithdrawMoneyReply(uint64_t dwGuild, BYTE bGiveSuccess, YANG lGold);

	void	MoneyChange(uint64_t dwGuild, YANG dwGold);

	void	QueryRanking();
	void	ResultRanking(MYSQL_RES * pRes);
	int	GetRanking(uint64_t dwGID);

	// Reserve War

	void	BootReserveWar();
	bool	ReserveWar(TPacketGuildWar * p);
	void	ProcessReserveWar();
	bool	Bet(uint64_t dwID, const char * c_pszLogin, YANG dwGold, uint64_t dwGuild);

	void	CancelWar(uint64_t GID1, uint64_t GID2);

	bool	ChangeMaster(uint64_t dwGID, uint64_t dwFrom, uint64_t dwTo);

    private:
	void ParseResult(SQLResult * pRes);

	void RemoveWar(uint64_t GID1, uint64_t GID2);	// erase war from m_WarMap and set end on priority queue

	void WarEnd(uint64_t GID1, uint64_t GID2, bool bDraw = false);

	int GetLadderPoint(uint64_t GID);

	void GuildWarWin(uint64_t GID);
	void GuildWarDraw(uint64_t GID);
	void GuildWarLose(uint64_t GID);

	void ProcessDraw(uint64_t dwGuildID1, uint64_t dwGuildID2);
	void ProcessWinLose(uint64_t dwGuildWinner, uint64_t dwGuildLoser);

	bool IsHalfWinLadderPoint(uint64_t dwGuildWinner, uint64_t dwGuildLoser);

	std::map<uint64_t, TGuild>					m_map_kGuild;
	std::map<uint64_t, std::map<uint64_t, time_t> >		m_mapGuildWarEndTime;

	std::set<TGuildDeclareInfo>				m_DeclareMap;
	std::map<uint64_t, std::map<uint64_t, TGuildWarInfo> >	m_WarMap;

	typedef std::pair<time_t, TGuildWarPQElement *>	stPairGuildWar;
	typedef std::pair<time_t, TGuildSkillUsed>	stPairSkillUsed;
	typedef std::pair<time_t, TGuildWaitStartInfo>	stPairWaitStart;

	std::priority_queue<stPairGuildWar, std::vector<stPairGuildWar>, std::greater<stPairGuildWar> >
	    m_pqOnWar;
	std::priority_queue<stPairWaitStart, std::vector<stPairWaitStart>, std::greater<stPairWaitStart> >
	    m_pqWaitStart;
	std::priority_queue<stPairSkillUsed, std::vector<stPairSkillUsed>, std::greater<stPairSkillUsed> >
	    m_pqSkill;

	std::map<uint64_t, CGuildWarReserve *>			m_map_kWarReserve;
	CPoly							polyPower;
	CPoly							polyHandicap;

	// GID Ranking
	std::map<uint64_t, int>					map_kLadderPointRankingByGID;
};

#endif
