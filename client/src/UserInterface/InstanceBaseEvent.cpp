#include "StdAfx.h"
#include "InstanceBase.h"

CActorInstance::IEventHandler& CInstanceBase::GetEventHandlerRef()
{
	return m_GraphicThingInstance.__GetEventHandlerRef();
}

CActorInstance::IEventHandler* CInstanceBase::GetEventHandlerPtr()
{
	return m_GraphicThingInstance.__GetEventHandlerPtr();
}

void CInstanceBase::SetEventHandler(CActorInstance::IEventHandler* pkEventHandler)
{
	m_GraphicThingInstance.SetEventHandler(pkEventHandler);
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
