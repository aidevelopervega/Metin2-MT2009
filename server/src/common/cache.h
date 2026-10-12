#ifndef __INC_COMMON_CACHE_H__
#define __INC_COMMON_CACHE_H__

template <typename T> class cache
{
	public:
		cache()
			: m_bNeedQuery(false),
			  m_expireTime(600),
			  m_lastUpdateTime(0),
			  m_lastFlushTime(0),
			  m_dwVersion(0),
			  m_dwPendingSaveVersion(0),
			  m_bSavePending(false),
			  m_bPendingEviction(false),
			  m_bIsUnknown(false)
		{
			m_lastFlushTime = time(0);
			memset(&m_data, 0, sizeof(m_data));
		}

		virtual ~cache() = default;

		T * Get(bool bUpdateTime = true)
		{
			if (bUpdateTime)
				m_lastUpdateTime = time(0);

			return &m_data;
		}

		void Put(T * pNew, bool bSkipQuery = false)
		{
			thecore_memcpy(&m_data, pNew, sizeof(T));
			m_lastUpdateTime = time(0);
			++m_dwVersion;
			m_bPendingEviction = false;

			if (!bSkipQuery)
				m_bNeedQuery = true;
		}

		bool CheckFlushTimeout()
		{
			if (m_bNeedQuery && time(0) - m_lastFlushTime > m_expireTime)
				return true;

			return false;
		}

		bool CheckTimeout()
		{
			if (time(0) - m_lastUpdateTime > m_expireTime)
				return true;

			return false;
		}

		void Flush()
		{
			if (!m_bNeedQuery)
				return;

			m_dwPendingSaveVersion = m_dwVersion;
			m_bSavePending = true;
			m_bNeedQuery = false;
			m_lastFlushTime = time(0);
			OnFlush();
		}

		bool IsNeedQuery() const { return m_bNeedQuery; }
		void SetNeedQuery(bool bNeed = true) { m_bNeedQuery = bNeed; }

		// Version & generation tracking
		DWORD GetVersion() const { return m_dwVersion; }
		DWORD GetPendingSaveVersion() const { return m_dwPendingSaveVersion; }
		bool IsSavePending() const { return m_bSavePending; }

		// Deferred eviction states
		void MarkPendingEviction() { m_bPendingEviction = true; }
		void CancelPendingEviction() { m_bPendingEviction = false; }
		bool IsPendingEviction() const { return m_bPendingEviction; }
		bool CanEvict() const { return m_bPendingEviction && !m_bNeedQuery && !m_bSavePending; }

		// UNKNOWN state
		bool IsUnknown() const { return m_bIsUnknown; }
		void SetUnknown(bool bUnknown = true) { m_bIsUnknown = bUnknown; }

		// Save outcome callbacks
		bool OnSaveSuccess(DWORD dwSavedVersion = 0)
		{
			if (dwSavedVersion != 0 && dwSavedVersion < m_dwPendingSaveVersion)
				return false;

			m_bSavePending = false;
			m_bIsUnknown = false;

			if (m_dwVersion > m_dwPendingSaveVersion)
			{
				m_bNeedQuery = true;
			}

			return CanEvict();
		}

		void OnSaveFailure(DWORD dwFailedVersion = 0, bool bUnknown = false)
		{
			(void)dwFailedVersion;
			m_bSavePending = false;
			m_bNeedQuery = true;
			m_bIsUnknown = bUnknown;
		}

		virtual void OnFlush() = 0;

	protected:
		T       m_data;
		bool    m_bNeedQuery;
		time_t  m_expireTime;
		time_t	m_lastUpdateTime;
		time_t	m_lastFlushTime;

		DWORD   m_dwVersion;
		DWORD   m_dwPendingSaveVersion;
		bool    m_bSavePending;
		bool    m_bPendingEviction;
		bool    m_bIsUnknown;
};

#endif
