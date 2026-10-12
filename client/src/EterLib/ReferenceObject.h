#pragma once

class CReferenceObject
{
	public:
		CReferenceObject();
		virtual ~CReferenceObject();

		void AddReference();
		void AddReferenceOnly();
		void Release();

		int GetReferenceCount();

		bool canDestroy();

	protected:
		virtual void OnConstruct();
		virtual void OnSelfDestruct();

	private:
		int m_refCount;
		bool m_destructed;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
