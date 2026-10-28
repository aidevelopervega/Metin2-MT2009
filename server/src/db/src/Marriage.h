// vim: ts=4 sw=4
#ifndef __MARRIAGE_H
#define __MARRIAGE_H

#include <set>
#include <queue>
#include <deque>

#include "Peer.h"

namespace marriage
{
	struct TWeddingInfo
	{
		DWORD dwTime;
		uint64_t dwPID1;
		uint64_t dwPID2;
		TWeddingInfo(DWORD time, uint64_t pid1, uint64_t pid2)
			: dwTime(time),
			dwPID1(pid1),
			dwPID2(pid2)
		{
		}
	};

	struct TWedding
	{
		DWORD dwTime;
		DWORD dwMapIndex;
		uint64_t dwPID1;
		uint64_t dwPID2;

		TWedding(DWORD time, DWORD _dwMapIndex, uint64_t pid1, uint64_t pid2)
			: dwTime(time),
			dwMapIndex(_dwMapIndex),
			dwPID1(pid1),
			dwPID2(pid2)
		{
		}
	};

	extern bool operator < (const TWedding& lhs, const TWedding& rhs);
	extern bool operator > (const TWedding& lhs, const TWedding& rhs);
	extern bool operator > (const TWeddingInfo& lhs, const TWeddingInfo& rhs);

	struct TMarriage
	{
		uint64_t pid1;
		uint64_t pid2;
		int   love_point;
		int64_t time;
		BYTE is_married;
		std::string name1;
		std::string name2;

		TMarriage(uint64_t _pid1, uint64_t _pid2, int _love_point, int64_t _time, BYTE _is_married, const char* _name1, const char* _name2)
			: pid1(_pid1), pid2(_pid2), love_point(_love_point), time(_time), is_married(_is_married), name1(_name1), name2(_name2)
		{
		}

		uint64_t GetOther(uint64_t PID)
		{
			if (pid1 == PID)
				return pid2;

			if (pid2 == PID)
				return pid1;

			return 0;
		}
	};

	class CManager : public singleton<CManager>
	{
		public:
			CManager();
			virtual ~CManager();

			bool Initialize();

			TMarriage* Get(uint64_t dwPlayerID);
			bool IsMarried(uint64_t dwPlayerID)
			{
				return Get(dwPlayerID) != NULL;
			}

			//void	Reserve(uint64_t dwPID1, uint64_t dwPID2);
			void	Add(uint64_t dwPID1, uint64_t dwPID2, const char* szName1, const char* szName2);
			void	Remove(uint64_t dwPID1, uint64_t dwPID2);
			void	Update(uint64_t dwPID1, uint64_t dwPID2, INT iLovePoint, BYTE byMarried);

			void	EngageToMarriage(uint64_t dwPID1, uint64_t dwPID2);

			void	ReadyWedding(DWORD dwMapIndex, uint64_t dwPID1, uint64_t dwPID2);
			void	EndWedding(uint64_t dwPID1, uint64_t dwPID2);

			void	OnSetup(CPeer* peer);

			void	Update();

		private:
			std::set<TMarriage *> m_Marriages;
			std::map<uint64_t, TMarriage *> m_MarriageByPID;

			std::priority_queue<TWedding, std::vector<TWedding>, std::greater<TWedding> > m_pqWeddingStart;

			std::priority_queue<TWeddingInfo, std::vector<TWeddingInfo>, std::greater<TWeddingInfo> > m_pqWeddingEnd;

			std::map<std::pair<uint64_t, uint64_t>, TWedding> m_mapRunningWedding;
	};
}

#endif
