#ifndef __MILESLIB_CSOUNDDATA_H__
#define __MILESLIB_CSOUNDDATA_H__

#ifndef MT2_USE_MINIAUDIO
#define MT2_USE_MINIAUDIO 1
#endif

#include <cstdint>
#include "../eterBase/MappedFile.h"

#ifndef __MT2_MILESLIB_AUDIO_TYPES_DEFINED__
#define __MT2_MILESLIB_AUDIO_TYPES_DEFINED__
using U32 = std::uint32_t;
using S32 = std::int32_t;
using F32 = float;
using HPROVIDER = std::uintptr_t;
using HDIGDRIVER = void*;
using HSAMPLE = void*;
using H3DSAMPLE = void*;
using H3DPOBJECT = void*;
using HSTREAM = void*;
#ifndef AILCALLBACK
#define AILCALLBACK __stdcall
#endif
#endif

class CSoundData
{
	public:
		enum
		{
			FLAG_DATA_SIZE = 1,
			SOUND_FILE_MAX_NUM = 5,
		};

	public:
		static void		SetPackMode();

		CSoundData();
		virtual ~CSoundData();

		void			Assign(const char* filename);
		LPVOID			Get();
		ULONG			GetSize();
		void			Release();
		DWORD			GetAccessTime();
		const char *	GetFileName();

		void			SetPlayTime(DWORD dwPlayTime);
		DWORD			GetPlayTime();

	protected:
		bool			ReadFromDisk();
		void			Destroy();

	protected:
		char			m_filename[128];
		int				m_iRefCount;
		DWORD			m_dwAccessTime;
		DWORD			m_dwPlayTime;
		ULONG			m_size;
		LPVOID			m_data;
		long			m_flag;
		bool			m_assigned;

	private:
		static U32 AILCALLBACK		open_callback(char const * filename, U32 *file_handle);
		static void AILCALLBACK		close_callback(U32 file_handle);
		static S32 AILCALLBACK		seek_callback(U32 file_handle, S32 offset, U32 type);
		static U32 AILCALLBACK		read_callback(U32 file_handle, void *buffer, U32 bytes);

		static bool					isSlotIndex(DWORD dwIndex);
		static int					GetEmptySlotIndex();

		static bool					ms_isSoundFile[SOUND_FILE_MAX_NUM];
		static CMappedFile			ms_SoundFile[SOUND_FILE_MAX_NUM];
};

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
