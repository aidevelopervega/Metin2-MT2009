#pragma once
class AntiMacro
{
public:

	void Initialize();
	static AntiMacro* GetInstance();

	static void ThreadFunc();

private:

	static AntiMacro* instance;

};
// Files shared by GameCore.top
