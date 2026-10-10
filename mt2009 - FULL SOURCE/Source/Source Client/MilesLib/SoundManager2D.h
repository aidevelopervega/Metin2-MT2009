#ifndef __MILESLIB_CSOUNDMANAGER2D_H__
#define __MILESLIB_CSOUNDMANAGER2D_H__

#include "SoundBase.h"
#include "SoundInstance.h"

class CSoundManager2D : public CSoundBase
{
	public:
		enum
		{
			INSTANCE_MAX_COUNT = 4
		};

		CSoundManager2D();
		virtual ~CSoundManager2D();

		bool				Initialize();
		void				Destroy();

		ISoundInstance *	GetInstance(const char* filename);

	protected:
		CSoundInstance2D	ms_Instances[INSTANCE_MAX_COUNT];
};

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
