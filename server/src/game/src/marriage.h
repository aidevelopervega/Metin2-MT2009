#ifndef __MARRIAGE_H
#define __MARRIAGE_H

namespace marriage
{
	struct TWeddingInfo
	{
		DWORD dwMapIndex;
	};

	extern const int MARRIAGE_POINT_PER_DAY;
	struct TMarriage
	{
		uint64_t m_pid1;
		uint64_t m_pid2;
		int   love_point;
		time_t marry_time;
		LPCHARACTER ch1;
		LPCHARACTER ch2;
		bool bSave;
		bool is_married;
		std::string name1;
		std::string name2;

		TWeddingInfo * pWeddingInfo;

		TMarriage(uint64_t pid1, uint64_t pid2, int _love_point, time_t _marry_time, const char* _name1, const char* _name2) :
			m_pid1(pid1),
			m_pid2(pid2),
			love_point(_love_point),
			marry_time(_marry_time),
			is_married(false),
			name1(_name1),
			name2(_name2),
			pWeddingInfo(NULL),
			eventNearCheck(NULL)
		{
			ch1 = ch2 = NULL;
			bSave = false;
			isLastNear = false;
			byLastLovePoint = 0;
		}

		~TMarriage();

		void Login(LPCHARACTER ch);
		void Logout(uint64_t pid);

		bool IsOnline()
		{
			return ch1 && ch2;
		}

		bool IsNear();

		uint64_t GetOther(uint64_t PID) const
		{
			if (m_pid1 == PID)
				return m_pid2;

			if (m_pid2 == PID)
				return m_pid1;

			return 0;
		}

		int GetMarriagePoint();
		int GetMarriageGrade();

		int GetBonus(DWORD dwItemVnum, bool bShare = true, LPCHARACTER me = NULL);

		void WarpToWeddingMap(uint64_t dwPID);
		void Save();
		void SetMarried();

		void Update(DWORD point);
		bool RequestEndWedding();

		void StartNearCheckEvent();
		void StopNearCheckEvent();
		void NearCheck();

		bool isLastNear;
		BYTE byLastLovePoint;
		LPEVENT eventNearCheck;
	};

	class CManager : public singleton<CManager>
	{
		public:
			CManager();
			virtual ~CManager();

			bool	Initialize();
			void	Destroy();

			TMarriage*	Get(uint64_t dwPlayerID);

			bool	IsMarriageUniqueItem(DWORD dwItemVnum);

			bool	IsMarried(uint64_t dwPlayerID);
			bool	IsEngaged(uint64_t dwPlayerID);
			bool	IsEngagedOrMarried(uint64_t dwPlayerID);

			void	RequestAdd(uint64_t dwPID1, uint64_t dwPID2, const char* szName1, const char* szName2);
			void	Add(uint64_t dwPID1, uint64_t dwPID2, time_t tMarryTime, const char* szName1, const char* szName2);

			void	RequestUpdate(uint64_t dwPID1, uint64_t dwPID2, int iUpdatePoint, BYTE byMarried);
			void	Update(uint64_t dwPID1, uint64_t dwPID2, long lTotalPoint, BYTE byMarried);

			void	RequestRemove(uint64_t dwPID1, uint64_t dwPID2);
			void	Remove(uint64_t dwPID1, uint64_t dwPID2);

			//void	P2PLogin(uint64_t dwPID);
			//void	P2PLogout(uint64_t dwPID);

			void	Login(LPCHARACTER ch);

			void	Logout(uint64_t pid);
			void	Logout(LPCHARACTER ch);

			void	WeddingReady(uint64_t dwPID1, uint64_t dwPID2, DWORD dwMapIndex);
			void	WeddingStart(uint64_t dwPID1, uint64_t dwPID2);
			void	WeddingEnd(uint64_t dwPID1, uint64_t dwPID2);

			bool	RequestEndWedding(uint64_t dwPID1, uint64_t dwPID2);

			template <typename Func>
				Func	for_each_wedding(Func f);

		private:
			TR1_NS::unordered_set<TMarriage*> m_Marriages;
			std::map<uint64_t, TMarriage *> m_MarriageByPID;
			std::set<std::pair<uint64_t, uint64_t> > m_setWedding;
	};

	template <typename Func>
		Func CManager::for_each_wedding(Func f)
		{
			for (itertype(m_setWedding) it = m_setWedding.begin(); it!=m_setWedding.end(); ++it)
			{
				TMarriage* pMarriage = Get(it->first);
				if (pMarriage)
					f(pMarriage);
			}
			return f;
		}
}

#endif
