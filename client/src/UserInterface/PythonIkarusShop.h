#ifndef __INCLUDE_HEADER_PYTHON_OFFLINESHO__
#define __INCLUDE_HEADER_PYTHON_OFFLINESHO__

#ifdef ENABLE_IKASHOP_RENEWAL
#include <ranges>
#include "PythonBackground.h"
#include "Packet.h"
#include <array>

namespace ikashop
{
	template <class T ,template <class> class S, typename S<T>::iterator >
	void ForEach(S<T>& container, std::function<void(T&)> func){
		std::for_each(container.begin(), container.end(), func);
	}

	template <class T , class K, template <class,class> class S, typename S<K,T>::iterator >
	void ForEach(S<K,T>& container, std::function<void(T&)> func){
		std::for_each(container.begin(), container.end(), [&](auto&& iter){ func(iter.second); });
	}

	template <class T>
	void DeletePointersContainer(T& obj) {
		ForEach(obj, [](auto&& iter){ delete(iter); });
	}

	enum eConstOfflineshop {
		OFFLINESHOP_DURATION_MAX_DAYS	= 8, // unused
		OFFLINESHOP_DURATION_MAX_HOURS	= 23, // unused
		OFFLINESHOP_DURATION_MAX_MINUTES = OFFLINESHOP_DURATION_MAX_DAYS * 24 * 60, // unused
		OFFLINESHOP_MAX_FILTER_HISTORY_SIZE = 25,
	};

	template<int WIDTH, int HEIGHT>
	class CShopGrid
	{
		static constexpr auto CELL_COUNT = WIDTH * HEIGHT;

	public:
		void Clear()
		{
			m_cells = {};
		}

		void RegisterItem(int cell, int size)
		{
			const auto [col, row] = _DecomposeCell(cell);
			for (int i = 0; i < size; i++)
				_SetCellState(_ComposeCell(col, row + i), true);
		}

		void UnregisterItem(int cell, int size)
		{
			const auto [col, row] = _DecomposeCell(cell);
			for (int i = 0; i < size; i++)
				_SetCellState(_ComposeCell(col, row + i), false);
		}

		bool CheckSpace(int cell, int size)
		{
			const auto [col, row] = _DecomposeCell(cell);
			return std::ranges::all_of(std::views::iota(row, row + size), [&](auto&& value) { return !_GetCellState(_ComposeCell(col, value)); });
		}

		std::optional<int> FindSpace(int size)
		{
			for (int cell = 0; cell < CELL_COUNT; cell++)
				if (CheckSpace(cell, size))
					return cell;
			return std::nullopt;
		}

		constexpr int GetCellCount() {
			return WIDTH * HEIGHT;
		}

	private:
		std::pair<int, int> _DecomposeCell(int cell) { return { cell % WIDTH, cell / WIDTH }; }
		int _ComposeCell(int col, int row) { return row * WIDTH + col; }
		void _SetCellState(int cell, bool state) { if (cell >= 0 && cell < CELL_COUNT) m_cells[cell] = state; }
		bool _GetCellState(int cell) { return cell >= 0 && cell < CELL_COUNT ? m_cells[cell] : true; }

	private:
		std::array<bool, WIDTH*HEIGHT> m_cells{};
	};


	DWORD GetItemHash(const TShopPlayerItem& item);

	namespace python 
	{
		struct PythonReference
		{
		public:
			static PythonReference steal(PyObject* obj){
				PythonReference ref;
				ref.m_obj = obj;
				return ref;
			}

		private:
			PythonReference() = default;
		
		public:
			PythonReference(PyObject* obj) : m_obj{ obj } { Py_XINCREF(obj); }
			PythonReference(const PythonReference& r) : m_obj{ r.m_obj } { Py_XINCREF(r.m_obj); }
			PythonReference(PythonReference&& r) : m_obj{ r.m_obj } { r.m_obj = nullptr; }
			~PythonReference() { Py_XDECREF(m_obj); }

			operator PyObject* () { return m_obj; }
			PyObject* weakRef() const { return m_obj; }

			operator bool() const { return m_obj != nullptr && m_obj != Py_None && PyObject_IsTrue(m_obj); }

		private:
			PyObject* m_obj = nullptr;
		};

		struct PythonGarbageCollector
		{
			// integer values
			PyObject* makeInteger(int val) { return _Register(PyInt_FromLong(static_cast<long>(val))); }
			PyObject* makeInteger(long val) { return _Register(PyInt_FromLong(val)); }
			PyObject* makeInteger(DWORD val) { return _Register(PyInt_FromSize_t(val)); }
			PyObject* makeInteger(int64_t val) { return _Register(PyLong_FromLongLong(val)); }
			PyObject* makeInteger(uint64_t val) { return _Register(PyLong_FromUnsignedLongLong(val)); }
			// floating values
			PyObject* makeFloat(float value) { return _Register(PyFloat_FromDouble(static_cast<double>(value))); }
			PyObject* makeFloat(double value) { return _Register(PyFloat_FromDouble(value)); }
			// boolean
			PyObject* makeBool(bool value) { return _Register(PyBool_FromLong(value ? 1 : 0)); }
			// string
			PyObject* makeString(const std::string& str) { return _Register(PyString_FromString(str.c_str())); }
			PyObject* makeString(const char* str) { return _Register(PyString_FromString(str)); }
			// objects
			PyObject* makeDict() { return _Register(PyDict_New()); }
			PyObject* makeList(size_t count = 0) { return _Register(PyList_New(count)); }
			PyObject* makeTuple(size_t count = 0) { return _Register(PyTuple_New(count)); }
			PyObject* makeValue(const std::string& str, const auto& ...arg) {
				return _Register(Py_BuildValue(str.c_str(), arg...));
			}

			PyObject* registerObject(PyObject* obj){ objects.emplace_back(obj); return obj; }

			~PythonGarbageCollector(){ std::ranges::for_each(objects, [](auto&& obj){ Py_XDECREF(obj); }); }

		private:
			PyObject* _Register(PyObject* ptr) { objects.emplace_back(ptr); return ptr; }

		private:
			std::vector<PyObject*> objects;
		};

		struct PythonSafeDict;
		struct PythonSafeList;

		template<class Type>
		concept PythonSafeType = std::is_same_v<Type, PythonSafeDict> || std::is_same_v<Type, PythonSafeList> || std::is_same_v<Type, PythonReference>;

		struct PythonSafeDict
		{
			PythonSafeDict() { dict = collector.makeDict(); }
			PythonSafeDict(const PythonSafeDict& r) { dict = collector.registerObject(r.incRef()); }

			// integers
			void set(const std::string& key, bool value) { PyDict_SetItemString(dict, key.c_str(), collector.makeBool(value)); }
			void set(const std::string& key, int value) { PyDict_SetItemString(dict, key.c_str(), collector.makeInteger(value)); }
			void set(const std::string& key, long value) { PyDict_SetItemString(dict, key.c_str(), collector.makeInteger(value)); }
			void set(const std::string& key, DWORD value) { PyDict_SetItemString(dict, key.c_str(), collector.makeInteger(value)); }
			void set(const std::string& key, int64_t value) { PyDict_SetItemString(dict, key.c_str(), collector.makeInteger(value)); }
			void set(const std::string& key, uint64_t value) { PyDict_SetItemString(dict, key.c_str(), collector.makeInteger(value)); }
			// floats
			void set(const std::string& key, float value) { PyDict_SetItemString(dict, key.c_str(), collector.makeFloat(value)); }
			void set(const std::string& key, double value) { PyDict_SetItemString(dict, key.c_str(), collector.makeFloat(value)); }
			// strings
			void set(const std::string& key, const std::string& value) { PyDict_SetItemString(dict, key.c_str(), collector.makeString(value)); }
			void set(const std::string& key, const char* value) { PyDict_SetItemString(dict, key.c_str(), collector.makeString(value)); }
			// objects
			void set(const std::string& key, PyObject* obj) { PyDict_SetItemString(dict, key.c_str(), obj); }

			void set(const std::string& key, const PythonSafeType auto& value) { PyDict_SetItemString(dict, key.c_str(), value.weakRef()); }

			// references
			PyObject* weakRef() const { return dict; }
			PythonReference ref() const { return { dict }; }
			PyObject* incRef() const { Py_XINCREF(dict); return dict; }

		private:
			PyObject* dict = nullptr;
			PythonGarbageCollector collector;
		};

		struct PythonSafeList
		{
			PythonSafeList() { list = collector.makeList(); }
			PythonSafeList(const PythonSafeList& r) { list = collector.registerObject(r.incRef()); }

			// integers
			void append(bool value) { PyList_Append(list, collector.makeBool(value)); }
			void append(int value) { PyList_Append(list, collector.makeInteger(value)); }
			void append(long value) { PyList_Append(list, collector.makeInteger(value)); }
			void append(DWORD value) { PyList_Append(list, collector.makeInteger(value)); }
			void append(int64_t value) { PyList_Append(list, collector.makeInteger(value)); }
			void append(uint64_t value) { PyList_Append(list, collector.makeInteger(value)); }
			// floats
			void append(float value) { PyList_Append(list, collector.makeFloat(value)); }
			void append(double value) { PyList_Append(list, collector.makeFloat(value)); }
			// strings
			void append(const std::string& value) { PyList_Append(list, collector.makeString(value)); }
			void append(const char* value) { PyList_Append(list, collector.makeString(value)); }
			// objects
			void append(PyObject* obj) { PyList_Append(list, obj); }

			void append(const PythonSafeType auto& value) { PyList_Append(list, value.weakRef()); }

			// references
			PyObject* weakRef() const { return list; }
			PythonReference ref() const { return { list }; }
			PyObject* incRef() const { Py_XINCREF(list); return list; }

		private:
			PyObject* list = nullptr;
			PythonGarbageCollector collector;
		};

		inline PythonSafeDict ExtractPythonDictPlayerItem(const auto& info) {
			// appending item attributes
			PythonGarbageCollector collector;
			PythonSafeList attrList;
			for (int j = 0; j < ITEM_ATTRIBUTE_SLOT_MAX_NUM; j++)
				attrList.append(collector.makeValue("(ii)", info.aAttr[j].bType, info.aAttr[j].sValue));

			// appending item sockets
			PythonSafeList socketList;
			for (int j = 0; j < ITEM_SOCKET_SLOT_MAX_NUM; j++)
				socketList.append(info.alSockets[j]);

			// making item dict
			PythonSafeDict itemDict;
			itemDict.set("id", info.id);
			itemDict.set("owner", info.owner);
			itemDict.set("vnum", info.vnum);
			itemDict.set("count", info.count);
#ifdef ENABLE_CHANGELOOK_SYSTEM
			itemDict.set("look", info.id);
#endif
			itemDict.set("attrs", attrList.weakRef());
			itemDict.set("sockets", socketList.weakRef());
			itemDict.set("window", info.window);
			itemDict.set("cell", info.pos);
#ifdef EXTEND_IKASHOP_PRO
			itemDict.set("hash", GetItemHash(info));
#endif
			return itemDict;
		}

		inline PythonSafeDict ExtractPythonDictShopItem(const auto& info) {
			auto itemDict = ExtractPythonDictPlayerItem(info);
			itemDict.set("price", info.price.yang);
#ifdef ENABLE_CHEQUE_SYSTEM
			itemDict.set("cheque", info.price.cheque);
#endif
			return itemDict;
		}


#ifdef EXTEND_IKASHOP_PRO
		inline PythonSafeDict ExtractPythonDictNotification(const ikashop::TNotificationInfo& info) {
			PythonSafeDict dict;
			dict.set("id", info.id);
			dict.set("type", (int)info.type);
			dict.set("who", info.who);
			dict.set("what", info.what);
			dict.set("format", info.format);
			dict.set("datetime", info.datetime);
			dict.set("seen", info.seenflag);
			return dict;
		};
#endif

		inline PythonSafeList ExtractPythonListFromContainer(const auto& container, auto func) {
			PythonSafeList list;
			for(auto& element : container)
				list.append(func(element));
			return list;
		}

		inline PythonSafeList ExtractPythonPlayerItemList(const auto& container) {
			static auto func = [](auto&& value) { return ExtractPythonDictPlayerItem(value); };
			return ExtractPythonListFromContainer(container, func);
		}

		inline PythonSafeList ExtractPythonShopItemList(const auto& container) {
			static auto func = [](auto&& value) { return ExtractPythonDictShopItem(value); };
			return ExtractPythonListFromContainer(container, func);
		}

	}
}

#ifdef ENABLE_IKASHOP_ENTITIES
namespace ikashop
{
	class ShopInstance {
		using SkinType = int;
		using ShopID = uint32_t;
		using SignString = std::string;

	public:
		ShopInstance(SkinType Skin);

		void SetVID(ShopID dwVID) {
			m_VirtualID = dwVID;
		}
		ShopID GetVID() const {
			return m_VirtualID;
		}
		void SetShopType(SkinType iType) {
			m_Skin = iType;
		}
		SkinType GetType() const {
			return m_Skin;
		}
		bool IsSkin() const {
			return m_Skin != 0;
		}
		CGraphicThingInstance* GetThingInstancePtr() {
			return &m_GraphicThing;
		}

		void SetOwnerName(const SignString& str);
		const SignString& GetName();
		void Show(float x, float y, float z);
		void Clear();
		void Render();
		void RenderText();
		void BlendRender();
		void Update();

		bool IsPickable();
		bool IsPicked();

		TPixelPosition GetPixelPos() { return m_PixelPos; }


		void FarFromView(bool state);
		bool IsFarFromView() const;

	private:
		CGraphicThingInstance m_GraphicThing{};
		CGraphicTextInstance m_Text;
		float m_TextHeight;
		ShopID m_VirtualID = 0;
		SkinType m_Skin = 0;
		bool m_shownEffects = true;
		bool m_isFarFromView = false;
		TPixelPosition m_PixelPos{};
	};
}
#endif


class CPythonIkarusShop : public CSingleton<CPythonIkarusShop>
{

public:
	struct TFilterHistoryElement : public ikashop::TFilterInfo
	{
		int64_t	datetime;
		DWORD count;

		friend auto operator <=> (const TFilterHistoryElement& left, const TFilterHistoryElement& right)
		{
			if(left.datetime == right.datetime)
				return std::strong_ordering::equal;
			if(left.datetime < right.datetime)
				return std::strong_ordering::less;
			return std::strong_ordering::greater;
		}
	};


	typedef std::vector<TFilterHistoryElement> FILTERHISTORY;
	typedef FILTERHISTORY::iterator FILTERHISTORY_ITER;

	static void NowDatetime(int64_t& datetime)
	{
		datetime = static_cast<int64_t>(std::time(nullptr));
	}

	static constexpr auto PRICE_HISTORY_EXPIRATION = 7 * 24 * 60 * 60;
	static constexpr auto NOTIFICATION_EXPIRATION = 10 * 24 * 60 * 60;

#ifdef EXTEND_IKASHOP_PRO
	struct PriceHistoryElement 
	{
		int64_t expiration;
		int64_t price;
		DWORD count = 1;
#ifdef ENABLE_CHEQUE_SYSTEM
		int cheque = 0;
#endif
	};
#endif

public:
	CPythonIkarusShop();
	~CPythonIkarusShop();

	//starts
	void SetSearchShopBoard(PyObject* poWindow);
	void SetBusinessBoard(PyObject* poWindow);
	PyObject* GetBusinessBoard();


	void OpenShop(const DWORD vid, const ikashop::TShopInfo& shop, const std::vector<ikashop::TShopItem>& vec, bool isOwnerOnline);
	void OpenShopOwner(const ikashop::TShopInfo& shop, const std::vector<ikashop::TShopItem>& vec, int tax);
	void OpenShopOwnerNoShop();
	void ShowMyShopPosition(DWORD vid, long x, long y);
	void ShowFoundShopPosition(DWORD vid, long x, long y);

	void ShopClose();
	void ShopExpiredGuesting(DWORD ownerid);
	void ShopRemoveItem(bool owner, DWORD itemid);

	void ShopGuestEditItem(DWORD itemid, const ikashop::TPriceInfo& price);
	void ShopOwnerEditItem(DWORD itemid, const ikashop::TPriceInfo& price);


	void ShopFilterResult(const std::vector<ikashop::TSearchResultInfo>& vec);
	void OfferListReceived(const std::vector<ikashop::TOfferListElement>& offers);
	void SafeboxRefresh(const ikashop::TValutesInfo& valute, const std::vector<ikashop::TShopPlayerItem>& item);
	void SafeboxRemoveItem(DWORD itemid);
	void SafeboxAddItem(const ikashop::TShopPlayerItem& item);

	void AuctionList(const std::vector<ikashop::TAuctionListElement>& auctions);
	void MyAuctionOpen(const ikashop::TAuctionInfo& auction, const std::vector<ikashop::TAuctionOfferInfo>& offers);
	void AuctionOpen(const ikashop::TAuctionInfo& auction, const std::vector<ikashop::TAuctionOfferInfo>& offers);
	void OpenMyAuctionNoAuction();

	//filter caching
	void LoadFilterHistory();
	void SaveFilterHistory();
	void AddNewFilterToHistory(TFilterHistoryElement& element);
	void PopOldestFilterHistoryElement();

#ifdef EXTEND_IKASHOP_PRO
	// price caching
	void LoadPriceHistory();
	void SavePriceHistory();
	void RegisterPriceHistory(DWORD vnum, DWORD count, DWORD hash, const ikashop::TPriceInfo& price);
	std::optional<ikashop::TPriceInfo> GetPriceHistory(DWORD vnum, DWORD hash, DWORD count);
#endif

	//interfaces methods
	ikashop::python::PythonSafeList GetFilterHistory();
	std::vector<std::string> GetItemNames();

	//create shop methods
	void ShopBuilding_AddItem(int win, int slot);
	void AuctionBuilding_AddItem(int win, int slot);

#ifdef EXTEND_IKASHOP_PRO
	void RefreshBoardCounters(int safebox, int auction, int offers, int notifications);
#else
	void RefreshBoardCounters(int safebox, int auction, int offers);
#endif

#ifdef EXTEND_IKASHOP_PRO
	void RegisterNotification(const ikashop::TNotificationInfo& info);
	void LoadNotifications();
	void SaveNotifications();
	const std::deque<ikashop::TNotificationInfo>& GetNotifications();
	void SetNotificationList(const std::vector<ikashop::TNotificationInfo>& notifications);
#endif


#ifdef ENABLE_IKASHOP_ENTITIES
	void InsertEntity(DWORD vid , int type, const char* name, const char* ownerName, long x, long y, long z);
	void RemoveEntity(DWORD vid);

	void RenderEntities();
	void UpdateEntities();

	ikashop::ShopInstance* GetShopInstancePtr(DWORD vid);

	bool GetShowNameFlag();
	void SetShowNameFlag(bool flag);

	void DeleteEntities();
	bool CheckPickedShopEntity(DWORD* ret);
#endif

	void PopupMessage(const std::string& message);
	void DeleteSearchResult(DWORD itemid);

	DWORD GetFoundMyShopVID() { return myShopVID; }

	void ClearFoundShopMap();
	DWORD GetFoundShopFromSearchTargetId(DWORD vid);
	bool IsFoundShopFromSearchViewed(DWORD vid);
	bool IsFoundShopFromSearchItem(DWORD vid);
	void SetFoundShopViewed(DWORD vid);

private:
	bool CallOnBusinessBoard(const std::string& func, PyObject* args = nullptr);
	bool CallOnSearchShopBoard(const std::string& func, PyObject* args = nullptr);

private:
	DWORD myShopVID = 0;

	PyObject* m_businessBoard = nullptr;
	PyObject* m_searchShopBoard = nullptr;

	FILTERHISTORY m_filterHistory;

	// price history
#ifdef EXTEND_IKASHOP_PRO
	std::map<DWORD, PriceHistoryElement> m_priceByVnum;
	std::map<DWORD, PriceHistoryElement> m_priceByHash;
#endif
	struct FoundShopInfo
	{
		DWORD targetId;
		bool isViewed;
	};
	std::map<DWORD, FoundShopInfo> m_foundShopMap;

	private:


#ifdef ENABLE_IKASHOP_ENTITIES
	std::vector<std::unique_ptr<ikashop::ShopInstance>> m_shopInstances;
	bool m_bIsShowName;
#endif

#ifdef EXTEND_IKASHOP_PRO
	std::deque<ikashop::TNotificationInfo> m_notifications;
#endif

};


extern void initikashop();



#endif

#endif //__INCLUDE_HEADER_PYTHON_OFFLINESHO__
// Files shared by GameCore.top
