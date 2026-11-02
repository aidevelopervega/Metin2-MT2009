#include "stdafx.h"
#include "affect.h"

#ifndef DEBUG_ALLOC
#include <memory>
#include <vector>

namespace
{
	class CAffectObjectPool
	{
	public:
		CAffect* malloc()
		{
			if (!free_list_.empty())
			{
				CAffect* p = free_list_.back();
				free_list_.pop_back();
				*p = CAffect{};
				return p;
			}

			constexpr size_t kChunkSize = 256;
			auto chunk = std::make_unique<CAffect[]>(kChunkSize);
			CAffect* base = chunk.get();
			chunks_.push_back(std::move(chunk));
			free_list_.reserve(free_list_.size() + kChunkSize - 1);
			for (size_t i = kChunkSize - 1; i >= 1; --i)
				free_list_.push_back(&base[i]);
			base[0] = CAffect{};
			return &base[0];
		}

		void free(CAffect* p)
		{
			if (p != nullptr)
				free_list_.push_back(p);
		}

	private:
		std::vector<std::unique_ptr<CAffect[]>> chunks_;
		std::vector<CAffect*> free_list_;
	};

	CAffectObjectPool affect_pool;
}
#endif

CAffect* CAffect::Acquire()
{
#ifndef DEBUG_ALLOC
	return affect_pool.malloc();
#else
	return M2_NEW CAffect;
#endif
}

void CAffect::Release(CAffect* p)
{
#ifndef DEBUG_ALLOC
	affect_pool.free(p);
#else
	M2_DELETE(p);
#endif
}


