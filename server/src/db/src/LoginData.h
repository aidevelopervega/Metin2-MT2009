// vim:ts=8 sw=4
#ifndef __INC_METIN_II_DB_LOGINDATA_H__
#define __INC_METIN_II_DB_LOGINDATA_H__

class CLoginData
{
    public:
	CLoginData();

	TAccountTable & GetAccountRef();
	void            SetClientKey(const DWORD * c_pdwClientKey);

	const DWORD *   GetClientKey();
	void            SetKey(DWORD dwKey);
	DWORD           GetKey();

	void            SetConnectedPeerHandle(DWORD dwHandle);
	DWORD		GetConnectedPeerHandle();

	void            SetLogonTime();
	DWORD		GetLogonTime();

	void		SetIP(const char * c_pszIP);
	const char *	GetIP();

	void		SetHWID(const char* c_pszHwid) { strlcpy(m_szHwid, c_pszHwid, sizeof(m_szHwid)); }
	const char* GetHWID() { return m_szHwid; }

	void		SetPlay(bool bOn);
	bool		IsPlay();

	void		SetDeleted(bool bSet);
	bool		IsDeleted();

	time_t		GetLastPlayTime() { return m_lastPlayTime; }

	void            SetPremium(int * paiPremiumTimes);
	int             GetPremium(BYTE type);
	int *           GetPremiumPtr();

	int				GetPremiumSubscription() { return m_iPremium; }
	void            SetPremiumSubscription(int premium);

	DWORD		GetLastPlayerID() const { return m_dwLastPlayerID; }
	void		SetLastPlayerID(DWORD id) { m_dwLastPlayerID = id; }
	void		SetWarpHandoffPending(bool bPending) { m_bWarpHandoffPending = bPending; }
	bool		IsWarpHandoffPending() const { return m_bWarpHandoffPending; }

    private:
	DWORD           m_dwKey;
	DWORD           m_adwClientKey[4];
	DWORD           m_dwConnectedPeerHandle;
	DWORD           m_dwLogonTime;
	char		m_szIP[MAX_HOST_LENGTH+1];
	bool		m_bPlay;
	bool		m_bDeleted;
	bool		m_bWarpHandoffPending;
	int			m_iPremium;

	char		m_szHwid[HWID_MAX_LEN + 1];
	DWORD		m_dwClientVersion;

	time_t		m_lastPlayTime;
	int		m_aiPremiumTimes[PREMIUM_MAX_NUM];

	DWORD		m_dwLastPlayerID;

	TAccountTable   m_data;
};

#endif

