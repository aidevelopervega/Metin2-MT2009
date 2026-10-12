#include "StdAfx.h"
#include "Thread.h"

CThread::CThread() : m_pArg(NULL), m_hThread(NULL), m_uThreadID(0)
{
}

int CThread::Create(void * arg)
{
	Arg(arg);
	m_hThread = (HANDLE) _beginthreadex(NULL, 0, EntryPoint, this, 0, &m_uThreadID);

	if (!m_hThread)
		return false;

	SetThreadPriority(m_hThread, THREAD_PRIORITY_NORMAL);
	return true;
}

UINT CThread::Run(void * arg)
{
	if (!Setup())
		return 0;

	return (Execute(arg));
}

/* Static */
UINT CALLBACK CThread::EntryPoint(void * pThis)
{
	CThread * pThread = (CThread *) pThis;
	return pThread->Run(pThread->Arg());
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
