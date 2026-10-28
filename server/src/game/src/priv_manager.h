#ifndef __PRIV_MANAGER_H
#define __PRIV_MANAGER_H

class CPrivManager : public singleton<CPrivManager>
{
	public:
		CPrivManager();

		void RequestGiveGuildPriv(uint64_t guild_id, BYTE type, int value, time_t dur_time_sec);
		void RequestGiveEmpirePriv(BYTE empire, BYTE type, int value, time_t dur_time_sec);
		void RequestGiveCharacterPriv(uint64_t pid, BYTE type, int value);

		void GiveGuildPriv(uint64_t guild_id, BYTE type, int value, BYTE bLog, time_t end_time_sec);
		void GiveEmpirePriv(BYTE empire, BYTE type, int value, BYTE bLog, time_t end_time_sec);
		void GiveCharacterPriv(uint64_t pid, BYTE type, int value, BYTE bLog);

		void RemoveGuildPriv(uint64_t guild_id, BYTE type);
		void RemoveEmpirePriv(BYTE empire, BYTE type);
		void RemoveCharacterPriv(uint64_t pid, BYTE type);

		int GetPriv(LPCHARACTER ch, BYTE type);
		int GetPrivByEmpire(BYTE bEmpire, BYTE type);
		int GetPrivByGuild(uint64_t guild_id, BYTE type);
		int GetPrivByCharacter(uint64_t pid, BYTE type);

	public:
		struct SPrivEmpireData
		{
			int m_value;
			time_t m_end_time_sec;
		};

		SPrivEmpireData* GetPrivByEmpireEx(BYTE bEmpire, BYTE type);

		struct SPrivGuildData
		{
			int		value;
			time_t	end_time_sec;
		};

		const SPrivGuildData*	GetPrivByGuildEx( uint64_t dwGuildID, BYTE byType ) const;

	private:
		SPrivEmpireData m_aakPrivEmpireData[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
		std::map<uint64_t, SPrivGuildData> m_aPrivGuild[MAX_PRIV_NUM];
		std::map<uint64_t, int> m_aPrivChar[MAX_PRIV_NUM];
};
#endif
