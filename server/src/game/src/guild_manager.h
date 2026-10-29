#ifndef __GUILD_MANAGER_H
#define __GUILD_MANAGER_H

class CGuild;
struct TGuildCreateParameter;

#define GUILD_SYMBOL_FILENAME	"guild_symbol.tga"

class CGuildWarReserveForGame
{
	public:
		TGuildWarReserve				data;
		std::map<std::string, std::pair<uint64_t, YANG> > mapBet;
};

class CGuildManager : public singleton<CGuildManager>
{
	public:
		CGuildManager();
		virtual ~CGuildManager();

		uint64_t	CreateGuild(TGuildCreateParameter& gcp);
		CGuild *	FindGuild(uint64_t guild_id);
		CGuild *	FindGuildByName(const std::string guild_name);
		void		LoadGuild(uint64_t guild_id);
		CGuild *	TouchGuild(uint64_t guild_id);
		void		DisbandGuild(uint64_t guild_id);

		void		Initialize();

		void		Link(uint64_t pid, CGuild* guild);
		void		Unlink(uint64_t pid);
		CGuild *	GetLinkedGuild(uint64_t pid);

		void		LoginMember(LPCHARACTER ch);
		void		P2PLoginMember(uint64_t pid);
		void		P2PLogoutMember(uint64_t pid);

		void		SkillRecharge();

		void		ShowGuildWarList(LPCHARACTER ch);
		void		SendGuildWar(LPCHARACTER ch);

		void		RequestEndWar(uint64_t guild_id1, uint64_t guild_id2);
		void		RequestCancelWar(uint64_t guild_id1, uint64_t guild_id2);
		void		RequestWarOver(uint64_t dwGuild1, uint64_t dwGuild2, uint64_t dwGuildWinner, YANG lReward);

		void		DeclareWar(uint64_t guild_id1, uint64_t guild_id2, BYTE bType);
		void		RefuseWar(uint64_t guild_id1, uint64_t guild_id2);
		void		StartWar(uint64_t guild_id1, uint64_t guild_id2);
		void		WaitStartWar(uint64_t guild_id1, uint64_t guild_id2);
		void		WarOver(uint64_t guild_id1, uint64_t guild_id2, bool bDraw);
		void		CancelWar(uint64_t guild_id1, uint64_t guild_id2);
		bool		EndWar(uint64_t guild_id1, uint64_t guild_id2);
		void		ReserveWar(uint64_t dwGuild1, uint64_t dwGuild2, BYTE bType);

		void            ReserveWarAdd(TGuildWarReserve * p);
		void            ReserveWarDelete(uint64_t dwID);
		std::vector<CGuildWarReserveForGame *> & GetReserveWarRef();
		void		ReserveWarBet(TPacketGDGuildWarBet * p);
		bool		IsBet(uint64_t dwID, const char * c_pszLogin);

		void		StopAllGuildWar();

		void		Kill(LPCHARACTER killer, LPCHARACTER victim);

		int		GetRank(CGuild* g);
		//void		GetHighRankString(DWORD dwMyGuild, char * buffer);
		//void		GetAroundRankString(DWORD dwMyGuild, char * buffer);
		void		GetHighRankString(uint64_t dwMyGuild, char * buffer, size_t buflen);
		void		GetAroundRankString(uint64_t dwMyGuild, char * buffer, size_t buflen);

		template <typename Func> void for_each_war(Func & f);

		int		GetDisbandDelay();
		int		GetWithdrawDelay();

		void		ChangeMaster(uint64_t dwGID);

		#ifdef ENABLE_GUILD_TOKEN_AUTH
		uint64_t	GenerateTokenHashNumber();
		bool		IsCorrectGuildToken(uint64_t guildID, uint64_t hashID);
		void		SendGuildToken(LPCHARACTER ch, uint64_t token);
		void		GuildRelink(uint64_t guildID, LPCHARACTER ch);
		#endif

	private:
		typedef std::map<uint64_t, CGuild*> TGuildMap;
		TGuildMap m_mapGuild;

		typedef std::set<std::pair<uint64_t, uint64_t> > TGuildWarContainer;
		TGuildWarContainer m_GuildWar;

		typedef std::map<std::pair<uint64_t, uint64_t>, time_t> TGuildWarEndTimeContainer;
		TGuildWarEndTimeContainer m_GuildWarEndTime;

		TGuildMap				m_map_pkGuildByPID;

		std::map<uint64_t, CGuildWarReserveForGame *>	m_map_kReserveWar;
		std::vector<CGuildWarReserveForGame *>		m_vec_kReserveWar;

		friend class CGuild;
};

template <typename Func> void CGuildManager::for_each_war(Func & f)
{
	for (itertype(m_GuildWar) it = m_GuildWar.begin(); it != m_GuildWar.end(); ++it)
	{
		f(it->first, it->second);
	}
}

extern void SendGuildWarScore(uint64_t dwGuild, uint64_t dwGuildOpp, int iDelta, int iBetScoreDelta = 0);

#endif
