#pragma once

#include "../eterLib/NetStream.h"

class CServerStateChecker : public CSingleton<CServerStateChecker>
{
	public:
		CServerStateChecker();
		virtual ~CServerStateChecker();

		void Create(PyObject* poWnd);
		void AddChannel(UINT uServerIndex, const char* c_szAddr, UINT uPort);
		void Request();
		void Update();

		void Initialize();

	private:
		typedef struct SChannel
		{
			UINT uServerIndex;
			const char* c_szAddr;
			UINT uPort;
		} TChannel;

		PyObject* m_poWnd;

		std::list<TChannel> m_lstChannel;

		CNetworkStream m_kStream;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
