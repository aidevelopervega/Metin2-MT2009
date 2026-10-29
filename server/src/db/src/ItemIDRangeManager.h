// vim:ts=4 sw=4
#ifndef __INC_METIN_II_ITEM_ID_RANGE_MANAGER_H__
#define __INC_METIN_II_ITEM_ID_RANGE_MANAGER_H__

class CItemIDRangeManager : public singleton<CItemIDRangeManager>
{
	private :
		const static uint64_t cs_dwMaxItemID = 9223372000000000000ULL;
		const static uint64_t cs_dwDefaultPrebuildMaxItemID = 4290000000ULL;
		const static uint64_t cs_dwMinimumRange = 10000000ULL;
		const static uint64_t cs_dwMinimumRemainCount = 10000ULL;

		std::list<TItemIDRangeTable> m_listData;

	public :
		CItemIDRangeManager();

		void Build();
		bool BuildRange(uint64_t dwMin, uint64_t dwMax, TItemIDRangeTable& range);
		void UpdateRange(uint64_t dwMin, uint64_t dwMax);

		TItemIDRangeTable GetRange();
};

#endif
