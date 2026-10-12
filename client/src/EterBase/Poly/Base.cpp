#include "StdAfx.h"
#include "Base.h"

CBase::CBase()
{
    id = 0;
}

CBase::~CBase()
{
}

bool CBase::isNumber()
{
	return (id & MID_NUMBER) != 0 ? true : false;
}

bool CBase::isVar()
{
    return (id & MID_VARIABLE) != 0 ? true : false;
}

bool CBase::isSymbol()
{
    return (id & MID_SYMBOL) != 0 ? true : false;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
