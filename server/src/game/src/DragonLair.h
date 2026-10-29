#include <boost/unordered_map.hpp>

#include "../../common/stl.h"

class CDragonLair
{
	public:
		CDragonLair (uint64_t dwGuildID, long BaseMapID, long PrivateMapID);
		virtual ~CDragonLair ();

		DWORD GetEstimatedTime () const;

		void OnDragonDead (LPCHARACTER pDragon);

	private:
		time_t StartTime_;
		uint64_t GuildID_;
		[[maybe_unused]] long BaseMapIndex_;
		[[maybe_unused]] long PrivateMapIndex_;
};

class CDragonLairManager : public singleton<CDragonLairManager>
{
	public:
		CDragonLairManager ();
		virtual ~CDragonLairManager ();

		bool Start (long MapIndexFrom, long BaseMapIndex, uint64_t GuildID);
		void OnDragonDead (LPCHARACTER pDragon, uint64_t KillerGuildID);

		size_t GetLairCount () const { return LairMap_.size(); }

	private:
		boost::unordered_map<uint64_t, CDragonLair*> LairMap_;
};

