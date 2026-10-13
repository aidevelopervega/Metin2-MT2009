#include "../../common/stl.h"
#include "../../common/CommonDefines.h"
#include <cstdint>

class CMapLocation : public singleton<CMapLocation>
{
#ifdef ENABLE_MOVE_CHANNEL
	public:
		using PairLocation = std::pair<BYTE, WORD>;
		typedef struct SLocation
		{
			int		addr{};
			WORD	port{};
			BYTE	channel{};
		} TLocation;

		bool    Get(long x, long y, long &lMapIndex, long &lAddr, WORD &wPort, BYTE channel = 0);
		bool	Get(long iIndex, long &lAddr, WORD &wPort, BYTE channel);

		template <typename TIndex, typename TAddr>
		bool    Get(long x, long y, TIndex &lMapIndex, TAddr &lAddr, WORD &wPort, BYTE channel = 0)
		{
			long idx = 0;
			long addr = 0;
			bool bRet = Get(x, y, idx, addr, wPort, channel);
			lMapIndex = static_cast<TIndex>(idx);
			lAddr = static_cast<TAddr>(addr);
			return bRet;
		}

		template <typename TAddr>
		bool	Get(long iIndex, TAddr &lAddr, WORD &wPort, BYTE channel)
		{
			long addr = 0;
			bool bRet = Get(iIndex, addr, wPort, channel);
			lAddr = static_cast<TAddr>(addr);
			return bRet;
		}

		void    Insert(long lIndex, const char *c_pszHost, WORD wPort, BYTE channel);

	protected:
		std::map<PairLocation, TLocation> m_map_address;
#else
	public:
		typedef struct SLocation
		{
			long        addr;
			WORD        port;
		} TLocation;

		bool    Get(long x, long y, long &lMapIndex, long &lAddr, WORD &wPort);
		bool	Get(long iIndex, long &lAddr, WORD &wPort);

		template <typename TIndex, typename TAddr>
		bool    Get(long x, long y, TIndex &lMapIndex, TAddr &lAddr, WORD &wPort)
		{
			long idx = 0;
			long addr = 0;
			bool bRet = Get(x, y, idx, addr, wPort);
			lMapIndex = static_cast<TIndex>(idx);
			lAddr = static_cast<TAddr>(addr);
			return bRet;
		}

		template <typename TAddr>
		bool	Get(long iIndex, TAddr &lAddr, WORD &wPort)
		{
			long addr = 0;
			bool bRet = Get(iIndex, addr, wPort);
			lAddr = static_cast<TAddr>(addr);
			return bRet;
		}

		void    Insert(long lIndex, const char *c_pszHost, WORD wPort);

	protected:
		std::map<long, TLocation> m_map_address;
#endif

};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
