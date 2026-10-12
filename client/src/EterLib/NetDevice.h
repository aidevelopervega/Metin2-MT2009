#pragma once

class CNetworkDevice
{
	public:
		CNetworkDevice();
		virtual ~CNetworkDevice();

		void Destroy();
		bool Create();

	protected:
		void Initialize();

	protected:
		bool m_isWSA;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
