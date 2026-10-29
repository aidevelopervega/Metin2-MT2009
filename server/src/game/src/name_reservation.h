
class CNameReservationManager : public singleton<CNameReservationManager>
{
public:
	CNameReservationManager();

	bool Initialize(TNameReservationTable* p, WORD wSize);
	uint64_t FindOwner(const char* c_pszString);
	BYTE CheckReservation(uint64_t account_id, const char* c_pszString);

protected:
	std::map<std::string, uint64_t> m_map_NameReservations;
};


