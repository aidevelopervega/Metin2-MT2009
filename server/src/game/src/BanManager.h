#pragma once
class BanManager : public singleton<BanManager>
{
	enum {
		BLOCK_INFINITE_DURATION = 5 * 365 * 3600 * 24,
	};

public:
	BanManager();
	virtual ~BanManager();

	struct BlockPlayerData
	{
		uint64_t aid;
		uint64_t pid;
		char name[24 + 1];
		bool foundOnline;
	};

	struct BlockHwidData
	{
		char hwid[255 + 1];
	};

	BlockPlayerData GetBlockPlayerData(uint64_t pid);
	BlockHwidData GetBlockHwidData(uint64_t aid);

	bool CloseOfflineShop(uint64_t pid);

	bool Block(uint64_t aid, uint64_t pid, DWORD duration, uint64_t who, std::string reason, bool instant_block);
	void BlockHWID(const char * hwid);
	void BlockAccountQuery(uint64_t aid, DWORD duration);
	int FlushBlock();
	void BlockHwidQuery(const char* hwid);
	void SendDisconnect(uint64_t pid);
	void SendDisconnectHwid(const char * hwid);
	void DisconnectHwid(const char * hwid);

};
