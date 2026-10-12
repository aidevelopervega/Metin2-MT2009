#pragma once

class CPythonSystem : public CSingleton<CPythonSystem>
{
	public:
		enum EWindow
		{
			WINDOW_STATUS,
			WINDOW_INVENTORY,
			WINDOW_ABILITY,
			WINDOW_SOCIETY,
			WINDOW_JOURNAL,
			WINDOW_COMMAND,

			WINDOW_QUICK,
			WINDOW_GAUGE,
			WINDOW_MINIMAP,
			WINDOW_CHAT,

			WINDOW_MAX_NUM,
		};

		enum EWindowMode
		{
			WINDOW_MODE_FULLSCREEN = 0,
			WINDOW_MODE_WINDOWED = 1,
			WINDOW_MODE_FULLSCREEN_WINDOWED = 2,
		};

		enum EShowDamageFlag
		{
			ShowDamageNone,
			ShowDamageTarget,
			ShowDamageAll,
		};

		enum
		{
			FREQUENCY_MAX_NUM  = 30,
			RESOLUTION_MAX_NUM = 100
		};

		typedef struct SResolution
		{
			DWORD	width;
			DWORD	height;
			DWORD	bpp;		// bits per pixel (high-color = 16bpp, true-color = 32bpp)

			DWORD	frequency[20];
			BYTE	frequency_count;
		} TResolution;

		typedef struct SWindowStatus
		{
			int		isVisible;
			int		isMinimized;

			int		ixPosition;
			int		iyPosition;
			int		iHeight;
		} TWindowStatus;

		typedef struct SConfig
		{
			DWORD			width;
			DWORD			height;
			int				windowHeightDifference;

			DWORD			custom_width;
			DWORD			custom_height;

			DWORD			bpp;
			DWORD			frequency;

			DWORD			scaled_width;
			DWORD			scaled_height;
			int				windowScaleHeightDifference;

			FLOAT			scale_interface;
			bool			bScaleInterface;

			bool			is_software_cursor;
			bool			is_object_culling;
			int				iDistance;
			int				iShadowLevel;
			int				iAliasingLevel;

			FLOAT			music_volume;
			BYTE			voice_volume;

			int				gamma;

			int				isSaveID;
			char			SaveID[20];

			BYTE			bWindowMode;
			bool			bDecompressDDS;
			bool			bNoSoundCard;
			bool			bUseDefaultIME;
			BYTE			bSoftwareTiling;
			bool			bViewChat;
			BYTE			bAlwaysShowName;
			BYTE			bShowDamage;
			bool			bShowSalesText;
			bool			bIsShrinkMap;
			BYTE			bShowFloatingText;
			int				iQuestTextDelay;
			BYTE			bNightMode;
			FLOAT			shop_render_distance;
			int				iTextureViewType;
			bool			bShowEmpireFlag;
			std::string		fontName;
			int				fontSize;
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
			bool			bShowMobAIFlag;
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
			bool			bShowMobLevel;
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
			float			shopRangeView = 5000.f;
			bool			shopViewOnlyFound = false;
#endif
#endif

			std::string		language;
		} TConfig;

	public:
		CPythonSystem();
		virtual ~CPythonSystem();

		void Clear();
		void SetInterfaceHandler(PyObject * poHandler);
		void DestroyInterfaceHandler();

		// Config
		void							SetDefaultConfig();
		bool							LoadConfig();
		void							FixScreenResolution();
		bool							SaveConfig();
		void							ApplyConfig();
		void							SetConfig(TConfig * set_config);
		TConfig *						GetConfig();
		void							ChangeSystem();

		// Interface
		bool							LoadInterfaceStatus();
		void							SaveInterfaceStatus();
		bool							isInterfaceConfig();
		const TWindowStatus &			GetWindowStatusReference(int iIndex);

		DWORD							GetWidth();
		DWORD							GetHeight();

		DWORD							GetCustomWidth();
		DWORD							GetCustomHeight();

		DWORD							GetBPP();
		DWORD							GetFrequency();

		bool							IsWindowed();
		bool							IsFullscreen();
		bool							IsFullscreenWindowed();
		BYTE							GetWindowMode();


		bool							IsSoftwareCursor();
		bool							IsViewChat();
		bool							IsAlwaysShowName();
		bool							IsGroupShowName();
		BYTE							GetShowName();
		BYTE							GetShowDamage();
		bool							IsUseDefaultIME();
		bool							IsNoSoundCard();
		bool							IsAutoTiling();
		bool							IsSoftwareTiling();

		bool							IsShowEmpireFlag();
		bool							IsShrinkMap();
		BYTE							GetNightMode();
		BYTE							GetShowFloatingText();
		int								GetQuestTextDelay();

		void							SetSoftwareTiling(bool isEnable);
		void							SetViewChatFlag(int iFlag);
		void							SetShowNameFlag(int iFlag);
		void							SetShowDamageFlag(int iFlag);

		void							SetShowSalesTextFlag(int iFlag);
		bool							IsShowSalesText();


		void							SetShowFloatingText(int iFlag);
		void							SetShrinkMap(bool state);
		void							SetQuestTextDelay(int delay);
		void							SetNightMode(BYTE bMode);

		void							SetWindowMode(int iFlag);

		void							SetResolution(DWORD width, DWORD height, DWORD bpp);
		void							SetScaledResolution(DWORD width, DWORD height);
		void							SetCustomResolution(DWORD width, DWORD height);
		void							SetScaleInterface(float scale);
		void							SetScaleInterface(bool isScale);

		int								GetDistance();
		int								GetShadowLevel();
		void							SetShadowLevel(unsigned int level);

		int								GetTextureViewType();
		void							SetTextureViewType(unsigned int type);

		int								GetScaledWidth();
		int								GetScaledHeight();
		float							GetScaleInterface();
		bool							IsScaleMode();

		int								GetAntiAliasingLevel();
		void							SetAntiAliasingLevel(unsigned int level);

		const char* GetFontName();
		int								GetFontSize();
		std::string						GetFont();
		void							SetFont(const char* fontName, int size);

#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
		bool							IsShowMobAIFlag();
		void							SetShowMobAIFlagFlag(int iFlag);
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
		bool							IsShowMobLevel();
		void							SetShowMobLevelFlag(int iFlag);
#endif
#ifdef ENABLE_IKASHOP_RENEWAL
#ifdef EXTEND_IKASHOP_PRO
		void							SetShopRangeView(float distance) { m_Config.shopRangeView = MINMAX(600.0f, distance, 5000.f); }
		float							GetShopRangeView() const {return m_Config.shopRangeView;}

		bool	IsShopViewOnlyFound() const { return m_Config.shopViewOnlyFound; }
		void	SetShopViewOnlyFound(bool state) { m_Config.shopViewOnlyFound = state; }
#endif
#endif

		std::string						GetLanguage();
		void							SetLanguage(const char* language);

		// Window
		void							SaveWindowStatus(int iIndex, int iVisible, int iMinimized, int ix, int iy, int iHeight);

		// SaveID
		int								IsSaveID();
		const char *					GetSaveID();
		void							SetSaveID(int iValue, const char * c_szSaveID);

		/// Display
		void							GetDisplaySettings();

		int								GetResolutionCount();
		int								GetFrequencyCount(int index);
		bool							GetResolution(int index, OUT DWORD *width, OUT DWORD *height, OUT DWORD *bpp);
		bool							GetFrequency(int index, int freq_index, OUT DWORD *frequncy);
		int								GetResolutionIndex(DWORD width, DWORD height, DWORD bpp);
		int								GetFrequencyIndex(int res_index, DWORD frequency);
		bool							isViewCulling();

		// Sound
		float							GetMusicVolume();
		int								GetSoundVolume();
		void							SetMusicVolume(float fVolume);
		void							SetSoundVolumef(float fVolume);

	protected:
		TResolution						m_ResolutionList[RESOLUTION_MAX_NUM];
		int								m_ResolutionCount;

		TConfig							m_Config;
		TConfig							m_OldConfig;

		bool							m_isInterfaceConfig;
		PyObject *						m_poInterfaceHandler;
		TWindowStatus					m_WindowStatus[WINDOW_MAX_NUM];
};

void SetGameConfigFileName(const char* c_szFileName);

//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
