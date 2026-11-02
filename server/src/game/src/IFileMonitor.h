#ifndef IFILEMONITOR_INCLUDED
#define IFILEMONITOR_INCLUDED

#include <unordered_map>

enum eFileUpdatedOptions
{
	e_FileUpdate_None = -1,
	e_FileUpdate_Error,
	e_FileUpdate_Deleted,
	e_FileUpdate_Modified,
	e_FileUpdate_AttrModified,
	e_FileUpdate_Linked,
	e_FileUpdate_Renamed,
	e_FileUpdate_Revoked,
};

typedef void (* PFN_FileChangeListener )(const std::string&, eFileUpdatedOptions);

struct IFileMonitor
{
	virtual void Update		(DWORD dwPulses) = 0;
	virtual void AddWatch	(const std::string& strFileName, PFN_FileChangeListener pListenerFunc) = 0;
};

#endif // IFILEMONITOR_INCLUDED

