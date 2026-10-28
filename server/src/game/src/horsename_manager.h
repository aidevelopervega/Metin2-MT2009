class CHorseNameManager : public singleton<CHorseNameManager>
{
	private :
		std::map<uint64_t, std::string> m_mapHorseNames;

		void BroadcastHorseName(uint64_t dwPlayerID, const char* szHorseName);

	public :
		CHorseNameManager();

		const char* GetHorseName(uint64_t dwPlayerID);

		void UpdateHorseName(uint64_t dwPlayerID, const char* szHorseName, bool broadcast=false);
};
