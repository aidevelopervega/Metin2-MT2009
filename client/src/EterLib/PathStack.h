#pragma once

#include <deque>
#include <string>

class CPathStack
{
	public:
		CPathStack();
		virtual ~CPathStack();

		void SetBase();

		void MoveBase();

		void Push();

		bool Pop();

		void Move(const char* c_szPathName);
		void GetCurrentPathName(std::string* pstCurPathName);

	protected:
		std::string				m_stBasePathName;
		std::deque<std::string>	m_stPathNameDeque;
};
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
