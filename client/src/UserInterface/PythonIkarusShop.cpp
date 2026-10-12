#include "StdAfx.h"
#ifdef ENABLE_IKASHOP_RENEWAL
#include <fstream>
#include <ranges>
#include <msl/msl.h>

#include "../GameLib/ItemManager.h"
#include "../EterPack/EterPackManager.h"

#include "Packet.h"
#include "PythonNetworkStream.h"
#include "PythonTextTail.h"
#include "PythonPlayer.h"
#include "PythonIkarusShop.h"
#include "PythonCharacterManager.h"
#include "PythonSystem.h"
#include "PythonMiniMap.h"

#ifdef min
#undef min
#undef max
#endif

#include <rapidjson/rapidjson.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <fmt\fmt.h>

#include "JsonHelper.h"

using namespace ikashop::python;


DWORD ikashop::GetItemHash(const TShopPlayerItem& item)
{
	// checking type - skipping non equip items
	CItemData* table = nullptr;
	if(!CItemManager::instance().GetItemDataPointer(item.vnum, &table))
		return 0;

	switch(table->GetType())
	{
	case ITEM_ARMOR:
	case ITEM_WEAPON:
	case ITEM_COSTUME:
	case ITEM_BELT:
		break;

	default:
		return 0;
	}

	// copying values
	auto copy {item};

	// resetting non hashable values
	copy.expiration = {};
	copy.id = {};
	copy.owner = {};
	copy.pos = {};
	copy.window = {};

	// returning memory hash
	return GetCRC32(reinterpret_cast<const char*>(&copy), sizeof(copy));
}

#ifdef ENABLE_IKASHOP_ENTITIES
namespace ikashop
{


	ShopInstance::ShopInstance(SkinType Skin) {
		m_VirtualID = 0;
		m_Skin = Skin;
	}
	void ShopInstance::SetOwnerName(const SignString& str) {
		static CGraphicText* font = nullptr;
		if (!font)
			font = static_cast<CGraphicText*>(DefaultFont_GetResource());
		m_Text.SetValue(str.c_str());
		m_Text.SetTextPointer(font);
		m_Text.SetOutline(true);
		m_Text.SetHorizonalAlign(CGraphicTextInstance::EHorizontalAlign::HORIZONTAL_ALIGN_CENTER);
		m_Text.SetColor(0xFF7AE75D);
		m_Text.Update();
	}

	const ShopInstance::SignString& ShopInstance::GetName() {
		return m_Text.GetValueStringReference();
	}

	void ShopInstance::Show(float x, float y, float z) {
		std::string model = "ikashop/lite/shop.gr2";

		m_PixelPos.x = x;
		m_PixelPos.y = -y;
		m_PixelPos.z = z;


		m_GraphicThing.Clear();
		m_GraphicThing.ReserveModelThing(1);
		m_GraphicThing.ReserveModelInstance(1);
		m_GraphicThing.RegisterModelThing(
			0, (CGraphicThing*)CResourceManager::Instance().GetResourcePointer(model.c_str()));
		m_GraphicThing.SetModelInstance(0, 0, 0);


		m_GraphicThing.SetPosition(x, -y, z);
		m_GraphicThing.Show();
		m_GraphicThing.Update();
		m_GraphicThing.Transform();
		m_GraphicThing.Deform();

		m_TextHeight = m_GraphicThing.GetHeight() + 10;
		if (!IsSkin())
			m_TextHeight -= 50;

	}

	void ShopInstance::Clear() {
		m_GraphicThing.Clear();
		m_VirtualID = 0;
		m_Skin = 0;
	}

	void ShopInstance::Render() {
		m_GraphicThing.Render();
	}

	void ShopInstance::RenderText() {
		if (m_GraphicThing.isShow())
			m_Text.Render();
	}

	void ShopInstance::BlendRender() {
		m_GraphicThing.BlendRender();
	}

	void ShopInstance::Update() {
		m_GraphicThing.Deform();
		m_GraphicThing.Update();


		// update and render text tail
		//CPythonGraphic& rpyGraphic = CPythonGraphic::Instance();
		//rpyGraphic.Identity();
		//float x = 0, y = 0, z = 0;
		//const D3DXVECTOR3& c_rv3Position = m_GraphicThing.GetPosition();
		//rpyGraphic.ProjectPosition(c_rv3Position.x, c_rv3Position.y, c_rv3Position.z + m_TextHeight, &x, &y, &z);
		//x = floorf(x);
		//y = floorf(y);
		//z *= CPythonGraphic::Instance().GetOrthoDepth() * -1.0f;
		//z += 15.0f;
		//m_Text.SetPosition(x, y, z);
	}

	bool ShopInstance::IsPickable() {
		return m_GraphicThing.isShow() && !IsFarFromView();
	}

	bool ShopInstance::IsPicked() {
		float u, v, t;
		return m_GraphicThing.Intersect(&u, &v, &t);
	}

	void ShopInstance::FarFromView(bool state)
	{
		m_isFarFromView = state;
		CPythonNetworkStream::instance().ToggleIkarusShopAdBoardVisibility(GetVID(), !state);
	}

	bool ShopInstance::IsFarFromView() const
	{
		return m_isFarFromView;
	}
}

#endif

CPythonIkarusShop::CPythonIkarusShop()
{
	m_businessBoard = nullptr;
#ifdef ENABLE_IKASHOP_ENTITIES
	m_bIsShowName = false;
#endif

	LoadFilterHistory();
#ifdef EXTEND_IKASHOP_PRO
	LoadPriceHistory();
#endif
}

void CPythonIkarusShop::SetSearchShopBoard(PyObject* poWindow)
{
	m_searchShopBoard = poWindow;
}

//starts
void CPythonIkarusShop::SetBusinessBoard(PyObject* poWindow)
{
	m_businessBoard = poWindow;
}


PyObject* CPythonIkarusShop::GetBusinessBoard()
{
	return m_businessBoard;
}

void CPythonIkarusShop::ClearFoundShopMap()
{
	for (auto it : m_foundShopMap)
	{
		CPythonBackground::Instance().DeleteTargetEffect(it.second.targetId);
	}

	m_foundShopMap.clear();
}

DWORD CPythonIkarusShop::GetFoundShopFromSearchTargetId(DWORD vid)
{
	auto it = m_foundShopMap.find(vid);
	if (it != m_foundShopMap.end())
	{
		return it->second.targetId;
	}
	return 0;
}

bool CPythonIkarusShop::IsFoundShopFromSearchViewed(DWORD vid)
{
	auto it = m_foundShopMap.find(vid);
	if (it != m_foundShopMap.end())
	{
		return it->second.isViewed;
	}
	return false;
}

bool CPythonIkarusShop::IsFoundShopFromSearchItem(DWORD vid)
{
	auto it = m_foundShopMap.find(vid);
	return it != m_foundShopMap.end();
}

void CPythonIkarusShop::SetFoundShopViewed(DWORD vid)
{
	auto it = m_foundShopMap.find(vid);
	if (it != m_foundShopMap.end())
	{
		if (it->second.isViewed)
			return;

		it->second.isViewed = true;

		CPythonBackground::Instance().DeleteTargetEffect(it->second.targetId);
		CPythonBackground::Instance().CreateShopTargetEffect(it->second.targetId, vid);
	}
}

bool CPythonIkarusShop::CallOnBusinessBoard(const std::string& func, PyObject* args)
{
	if(args == nullptr)
		args = PyTuple_New(0);
	
	bool ret = false;
	if(m_businessBoard != nullptr)
		PyCallClassMemberFunc(m_businessBoard, func.c_str(), args, &ret);
	return ret;
}

bool CPythonIkarusShop::CallOnSearchShopBoard(const std::string& func, PyObject* args)
{
	if (args == nullptr)
		args = PyTuple_New(0);

	bool ret = false;
	if (m_searchShopBoard != nullptr)
		PyCallClassMemberFunc(m_searchShopBoard, func.c_str(), args, &ret);
	return ret;
}

void CPythonIkarusShop::OpenShop(const DWORD vid, const ikashop::TShopInfo& shop, const std::vector<ikashop::TShopItem>& vec, bool isOwnerOnline)
{
	// making shop dict
	PythonSafeDict shopDict;
	shopDict.set("vid", vid);
	shopDict.set("id", shop.ownerid);
	shopDict.set("duration", shop.duration);
	shopDict.set("name", shop.name);
	shopDict.set("ownerName", shop.ownerName);
	shopDict.set("isOwnerOnline", isOwnerOnline);
	shopDict.set("unlockCount", shop.unlockCount);
	shopDict.set("isPremium", shop.isPremium);
	shopDict.set("items", ExtractPythonShopItemList(vec));

	CallOnSearchShopBoard("OpenShopGuest", Py_BuildValue("(O)", shopDict.weakRef()));
}

void CPythonIkarusShop::OpenShopOwner(const ikashop::TShopInfo& shop, const std::vector<ikashop::TShopItem>& vec, int tax)
{
	// making shop dict
	PythonSafeDict shopDict;
	shopDict.set("name", shop.name);
	shopDict.set("duration", shop.duration);
	shopDict.set("items", ExtractPythonShopItemList(vec));

	CallOnBusinessBoard("OpenShopOwner", Py_BuildValue("(Oi)", shopDict.weakRef(), tax));
}

void CPythonIkarusShop::ShowMyShopPosition(DWORD vid, long x, long y)
{
	myShopVID = vid;
	CPythonMiniMap::instance().CreateTarget(OFFLINESHOP_MYSHOP_TARGET_INDEX, "Sklep", x, y);
	CPythonBackground::Instance().CreateShopTargetEffect(OFFLINESHOP_MYSHOP_TARGET_INDEX, vid);
}

void CPythonIkarusShop::ShowFoundShopPosition(DWORD vid, long x, long y)
{
	auto index = OFFLINESHOP_START_TARGET_INDEX  + CPythonBackground::Instance().GetShopTargetCount();
	CPythonBackground::Instance().CreateShopTargetEffect(index, vid);
	m_foundShopMap[vid] = {index, false};
}

void CPythonIkarusShop::OpenShopOwnerNoShop()
{
	CallOnBusinessBoard("OpenShopOwnerEmpty" );
}

void CPythonIkarusShop::ShopClose()
{
	CallOnBusinessBoard("ShopClose" );
}

void CPythonIkarusShop::ShopExpiredGuesting(DWORD ownerid)
{
	CallOnSearchShopBoard("ShopExpiredGuesting", Py_BuildValue("(i)", ownerid));
}

void CPythonIkarusShop::ShopRemoveItem(bool owner, DWORD itemid)
{
	if(owner)
		CallOnBusinessBoard("ShopOwnerRemoveItem", Py_BuildValue("(i)", itemid));
	else
		CallOnSearchShopBoard("ShopGuestRemoveItem", Py_BuildValue("(i)", itemid));
}

void CPythonIkarusShop::ShopGuestEditItem(DWORD itemid, const ikashop::TPriceInfo& price)
{
	ikashop::python::PythonSafeDict dict;
	dict.set("price", price.yang);
#ifdef ENABLE_CHEQUE_SYSTEM
	dict.set("cheque", price.cheque);
#endif

	CallOnSearchShopBoard("ShopGuestEditItem", Py_BuildValue("(iO)", itemid, dict.weakRef()));
}

void CPythonIkarusShop::ShopOwnerEditItem(DWORD itemid, const ikashop::TPriceInfo& price)
{
	ikashop::python::PythonSafeDict dict;
	dict.set("price", price.yang);
#ifdef ENABLE_CHEQUE_SYSTEM
	dict.set("cheque", price.cheque);
#endif

	CallOnBusinessBoard("ShopOwnerEditItem", Py_BuildValue("(iO)", itemid, dict.weakRef()));
}

void CPythonIkarusShop::ShopFilterResult(const std::vector<ikashop::TSearchResultInfo>& vec)
{
	static auto extractOne = [](const ikashop::TSearchResultInfo& data){
		auto dict = ExtractPythonDictShopItem(data);
		dict.set("is_auction", data.auction);
		dict.set("seller_name", data.seller_name);
		dict.set("duration", data.duration);
		return dict;
	};

	auto itemList = ExtractPythonListFromContainer(vec, extractOne);
	CallOnSearchShopBoard("SetSearchResultItems", Py_BuildValue("(O)", itemList.weakRef()));
}

void CPythonIkarusShop::OfferListReceived(const std::vector<ikashop::TOfferListElement>& offers)
{
	static auto extractOffer = [](auto&& offer){
		auto item = ExtractPythonDictShopItem(offer.item);

		PythonSafeDict offerDict;
		offerDict.set("name", offer.shopname);
		offerDict.set("id", offer.offerid);
		offerDict.set("buyer_name", offer.buyername);
		offerDict.set("ownerid", offer.ownerid);
		offerDict.set("itemid", offer.itemid);
		offerDict.set("price", offer.price.yang);
		offerDict.set("notified", offer.notified);
		offerDict.set("accepted", offer.accepted);
		offerDict.set("item", item);
		offerDict.set("incoming", offer.incoming);
		offerDict.set("datetime", offer.datetime);
		return offerDict;
	};

	auto offerList = ExtractPythonListFromContainer(offers, extractOffer);
	CallOnBusinessBoard("SetOffers", Py_BuildValue("(O)", offerList.weakRef()));
}

void CPythonIkarusShop::SafeboxRefresh(const ikashop::TValutesInfo& valute, const std::vector<ikashop::TShopPlayerItem>& items)
{
	auto itemList = ExtractPythonPlayerItemList(items);
	CallOnBusinessBoard("SetupSafebox", Py_BuildValue("(KO)", valute.yang, itemList.weakRef()));
}

void CPythonIkarusShop::SafeboxRemoveItem(DWORD itemid)
{
	CallOnBusinessBoard("SafeboxRemoveItem", Py_BuildValue("(i)", itemid));
}

void CPythonIkarusShop::SafeboxAddItem(const ikashop::TShopPlayerItem& item)
{
	auto itemDict = ikashop::python::ExtractPythonDictPlayerItem(item);
	CallOnBusinessBoard("SafeboxAddItem", Py_BuildValue("(O)", itemDict.weakRef()));
}

void CPythonIkarusShop::AuctionList(const std::vector<ikashop::TAuctionListElement>& auctions)
{
	static auto extractAuctionListElement = [](auto&& element){
		auto auctionDict = ExtractPythonDictShopItem(element.auction);
		auctionDict.set("seller_name", element.auction.ownername);
		auctionDict.set("duration", element.auction.duration);
		auctionDict.set("price", element.actual_best.yang);
		auctionDict.set("is_auction", true);
		return auctionDict;
	};

	auto auctionList = ExtractPythonListFromContainer(auctions, extractAuctionListElement);
	CallOnSearchShopBoard("SetAuctionList", Py_BuildValue("(O)", auctionList.weakRef()));
}

void CPythonIkarusShop::MyAuctionOpen(const ikashop::TAuctionInfo& auction, const std::vector<ikashop::TAuctionOfferInfo>& offers)
{
	static auto extractAutionOffer = [](auto&& offer){
		PythonSafeDict offerDict;
		offerDict.set("buyerid", offer.buyerid);
		offerDict.set("buyername", offer.buyername);
		offerDict.set("offerid", offer.ownerid);
		offerDict.set("price", offer.price.yang);
		offerDict.set("datetime", offer.datetime);
		return offerDict;
	};

	auto auctionDict = ExtractPythonDictShopItem(auction);
	auctionDict.set("duration", auction.duration);
	auctionDict.set("offers", ExtractPythonListFromContainer(offers, extractAutionOffer));
	
	CallOnBusinessBoard("SetupAcutionOwner", Py_BuildValue("(O)", auctionDict.weakRef()));
}

void CPythonIkarusShop::AuctionOpen(const ikashop::TAuctionInfo& auction, const std::vector<ikashop::TAuctionOfferInfo>& offers)
{
	static auto extractAutionOffer = [](auto&& offer) {
		PythonSafeDict offerDict;
		offerDict.set("buyerid", offer.buyerid);
		offerDict.set("buyername", offer.buyername);
		offerDict.set("offerid", offer.ownerid);
		offerDict.set("price", offer.price.yang);
		offerDict.set("datetime", offer.datetime);
		return offerDict;
	};

	auto auctionDict = ExtractPythonDictShopItem(auction);
	auctionDict.set("ownername", auction.ownername);
	auctionDict.set("duration", auction.duration);
	auctionDict.set("offers", ExtractPythonListFromContainer(offers, extractAutionOffer));

	CallOnSearchShopBoard("OpenAuctionGuest", Py_BuildValue("(O)", auctionDict.weakRef()));
}

void CPythonIkarusShop::OpenMyAuctionNoAuction()
{
	CallOnBusinessBoard("OpenCreateAuctionQuestion");
}

//filter caching methods
void CPythonIkarusShop::LoadFilterHistory()
{
	// opening stream
	std::ifstream stream("cache/search_history.json", std::ios::in | std::ios::ate | std::ios::binary);
	if(stream.is_open() == false)
		return;

	// reading size
	auto fsize = stream.tellg();

	// resetting reading point
	stream.seekg(0);

	// making string receiving file content
	std::string jsonString;
	jsonString.resize(fsize);

	// reading file content inside string
	stream.read(jsonString.data(), fsize);
	stream.close();

	// making document
	rapidjson::Document jsonDoc;
	jsonDoc.Parse(jsonString.c_str());

	// checking for parser errors
	if(jsonDoc.HasParseError()){
		TraceError("PARSER ERROR PARSING SEARCH HISTORY FILE: %d", (int) jsonDoc.GetParseError());
		return;
	}

	// checking list object
	if(jsonDoc.IsArray() == false){
		TraceError("INVALID SEARCH HISTORY FILE: document.IsArray() == false");
		return;
	}

	// reserving dimension
	m_filterHistory.clear();
	m_filterHistory.reserve(jsonDoc.Size());

	// iterating over array
	for(auto& member : jsonDoc.GetArray())
	{
		auto& history = m_filterHistory.emplace_back();
		// decoding info
		history.count = jsonHelper::getValue<int>(member, "count");

		// decoding datetime
		history.datetime = jsonHelper::getValue<int64_t>(member, "datetime");

		// decoding filter name
		auto name = jsonHelper::getValue<std::string>(member, "item_name");
		snprintf(history.name, sizeof(history.name), "%s", name.c_str());

		// decoding filter
		history.type = jsonHelper::getValue<int>(member, "type");
		history.subtype = jsonHelper::getValue<int>(member, "subtype");
		history.levelstart = jsonHelper::getValue<int>(member, "level_start");
		history.levelend = jsonHelper::getValue<int>(member, "level_end");
		history.pricestart.yang = jsonHelper::getValue<int64_t>(member, "price_start");
		history.pricend.yang = jsonHelper::getValue<int64_t>(member, "price_end");
		history.sashGrade = jsonHelper::getValue<int>(member, "sash");
		history.alchemyGrade = jsonHelper::getValue<int>(member, "alchemy");

		for(int i=0; i < ITEM_ATTRIBUTE_SLOT_NORM_NUM; i++){
			auto& attr = history.attrs[i];
			attr.bType = jsonHelper::getValue<int>(member, fmt::format("attrtype{}", i));
			attr.sValue = jsonHelper::getValue<int>(member, fmt::format("attrvalue{}", i));
		}
	}
}

void CPythonIkarusShop::SaveFilterHistory()
{
	// creating document
	rapidjson::Document doc;
	doc.SetArray();

	// reserving array size
	doc.GetArray().Reserve(m_filterHistory.size(), doc.GetAllocator());

	// populating array
	for(auto& history : m_filterHistory)
	{
		rapidjson::Value member;
		member.SetObject();

		// encoding info
		jsonHelper::insertValue(member, "count", static_cast<int>(history.count), doc.GetAllocator());
		// encoding datetime
		jsonHelper::insertValue(member, "datetime", static_cast<int64_t>(history.datetime), doc.GetAllocator());
		// encoding filter
		jsonHelper::insertValue(member, "item_name", std::string(history.name), doc.GetAllocator());
		jsonHelper::insertValue(member, "type", static_cast<int>(history.type), doc.GetAllocator());
		jsonHelper::insertValue(member, "subtype", static_cast<int>(history.subtype), doc.GetAllocator());
		jsonHelper::insertValue(member, "level_start", static_cast<int>(history.levelstart), doc.GetAllocator());
		jsonHelper::insertValue(member, "level_end", static_cast<int>(history.levelend), doc.GetAllocator());
		jsonHelper::insertValue(member, "price_start", static_cast<int64_t>(history.pricestart.yang), doc.GetAllocator());
		jsonHelper::insertValue(member, "price_end", static_cast<int64_t>(history.pricend.yang), doc.GetAllocator());
		jsonHelper::insertValue(member, "alchemy", static_cast<int>(history.alchemyGrade), doc.GetAllocator());
		jsonHelper::insertValue(member, "sash", static_cast<int>(history.sashGrade), doc.GetAllocator());
		
		for (int i =0; i < ITEM_ATTRIBUTE_SLOT_NORM_NUM; i++){
			auto& attr = history.attrs[i];
			jsonHelper::insertValue(member, fmt::format("attrtype{}", i), static_cast<int>(attr.bType), doc.GetAllocator());
			jsonHelper::insertValue(member, fmt::format("attrvalue{}", i), static_cast<int>(attr.sValue), doc.GetAllocator());
		}

		// pushing element inside the array
		doc.PushBack(member, doc.GetAllocator());
	}

	// making json string
	rapidjson::StringBuffer buffer;
	buffer.Clear();

	rapidjson::Writer<rapidjson::StringBuffer> StringWriter(buffer);
	doc.Accept(StringWriter);


	// opening stream
	std::ofstream stream("cache/search_history.json", std::ios::out | std::ios::binary);
	if (!stream.is_open()){
		TraceError("CANNOT SAVE SEARCH HISTORY FILE!");
		return;
	}

	// completing stream data
	stream.write(buffer.GetString(), buffer.GetSize());
	stream.close();
}

void CPythonIkarusShop::AddNewFilterToHistory(TFilterHistoryElement& element)
{
	m_filterHistory.emplace_back(element);
	while (m_filterHistory.size() > ikashop::OFFLINESHOP_MAX_FILTER_HISTORY_SIZE)
		PopOldestFilterHistoryElement();
	SaveFilterHistory();
}

void CPythonIkarusShop::PopOldestFilterHistoryElement()
{
	if(!m_filterHistory.empty())
		m_filterHistory.erase(std::min_element(m_filterHistory.begin(), m_filterHistory.end()));
}

#ifdef EXTEND_IKASHOP_PRO
void CPythonIkarusShop::LoadPriceHistory()
{
	static auto loadFileIntoMap = [](const std::string& filename, auto& map){
		// opening stream
		std::ifstream stream(filename, std::ios::in | std::ios::ate | std::ios::binary);
		if (stream.is_open() == false)
			return;

		// reading size
		auto fsize = stream.tellg();

		// resetting reading point
		stream.seekg(0);

		// making string receiving file content
		std::string jsonString;
		jsonString.resize(fsize);

		// reading file content inside string
		stream.read(jsonString.data(), fsize);
		stream.close();

		// making document
		rapidjson::Document jsonDoc;
		jsonDoc.Parse(jsonString.c_str());

		// checking for parser errors
		if (jsonDoc.HasParseError()) {
			TraceError("PARSER ERROR PARSING PRICE HISTORY FILE: %d", (int)jsonDoc.GetParseError());
			return;
		}

		// checking list object
		if (jsonDoc.IsArray() == false) {
			TraceError("INVALID PRICE HISTORY FILE: document.IsArray() == false");
			return;
		}

		// reserving dimension
		map.clear();

		// getting current timestamp to check expiration
		const auto currentTime = std::time(nullptr);

		// iterating over array
		for (auto& member : jsonDoc.GetArray())
		{
			auto key = static_cast<DWORD>(jsonHelper::getValue<int>(member, "key"));
			auto expiration = jsonHelper::getValue<int64_t>(member, "expiration");
			auto price = jsonHelper::getValue<int64_t>(member, "price");
			auto count = jsonHelper::getValue<int>(member, "count");
#ifdef ENABLE_CHEQUE_SYSTEM
			auto cheque = jsonHelper::getValue<int>(member, "cheque");
#endif

			// checking expiration
			if(expiration < currentTime)
				continue;

			auto& registerPrice = map[key];
			registerPrice.expiration = expiration;
			registerPrice.price = price;
			registerPrice.count = static_cast<DWORD>(std::max(1, count));
#ifdef ENABLE_CHEQUE_SYSTEM
			registerPrice.cheque = cheque;
#endif
		}
	};

	loadFileIntoMap("cache/price_history_vnum.json", m_priceByVnum);
	loadFileIntoMap("cache/price_history_hash.json", m_priceByHash);
}

void CPythonIkarusShop::SavePriceHistory()
{
	static auto saveMapToFile = [](const std::string& file, const auto& map){
		// creating document
		rapidjson::Document doc;
		doc.SetArray();

		// reserving array size
		doc.GetArray().Reserve(map.size(), doc.GetAllocator());

		// populating array
		for (auto& [key, element] : map)
		{
			rapidjson::Value member;
			member.SetObject();

			// encoding info
			jsonHelper::insertValue(member, "key", static_cast<int>(key), doc.GetAllocator());
			jsonHelper::insertValue(member, "expiration", element.expiration, doc.GetAllocator());
			jsonHelper::insertValue(member, "price", element.price, doc.GetAllocator());
			jsonHelper::insertValue(member, "count", static_cast<int>(element.count), doc.GetAllocator());
#ifdef ENABLE_CHEQUE_SYSTEM
			jsonHelper::insertValue(member, "cheque", element.cheque, doc.GetAllocator());
#endif
			// pushing element inside the array
			doc.PushBack(member, doc.GetAllocator());
		}

		// making json string
		rapidjson::StringBuffer buffer;
		buffer.Clear();

		rapidjson::Writer<rapidjson::StringBuffer> StringWriter(buffer);
		doc.Accept(StringWriter);

		// opening stream
		std::ofstream stream(file, std::ios::out | std::ios::binary);
		if (!stream.is_open()) {
			TraceError("CANNOT SAVE PRICE HISTORY FILE!");
			return;
		}

		// completing stream data
		stream.write(buffer.GetString(), buffer.GetSize());
		stream.close();
	};

	saveMapToFile("cache/price_history_vnum.json", m_priceByVnum);
	saveMapToFile("cache/price_history_hash.json", m_priceByHash);
}

void CPythonIkarusShop::RegisterPriceHistory(DWORD vnum, DWORD count, DWORD hash, const ikashop::TPriceInfo& price)
{
	// calculating info
	const auto expiration = static_cast<int64_t>(std::time(nullptr)) + PRICE_HISTORY_EXPIRATION;
	const auto total = price.yang;
#ifdef ENABLE_CHEQUE_SYSTEM
	const auto cheque = price.cheque;
#endif
	
	// registering by vnum
	m_priceByVnum[vnum] = {expiration, total, count
#ifdef ENABLE_CHEQUE_SYSTEM
	, cheque
#endif
	};
	
	// registering by hash
	if(hash != 0)
		m_priceByHash[hash] = {expiration, total, count
#ifdef ENABLE_CHEQUE_SYSTEM
	, cheque
#endif
	};

	// moving data to file
	SavePriceHistory();
}

std::optional<ikashop::TPriceInfo> CPythonIkarusShop::GetPriceHistory(DWORD vnum, DWORD hash, DWORD count)
{
	// converting yang amount to cheque + yang
	static auto composePrice = [](auto price, auto registerCount, auto count
#ifdef ENABLE_CHEQUE_SYSTEM
		, auto cheque
#endif
		) -> ikashop::TPriceInfo {

		// checking if count matches
		if(registerCount == count)
			return { price
#ifdef ENABLE_CHEQUE_SYSTEM
					, cheque
#endif
		};

		// adjusting price using count
		auto total = price;
#ifdef ENABLE_CHEQUE_SYSTEM
		total += static_cast<long long>(cheque) * YANG_PER_CHEQUE;
#endif

		total *= count;
		total /= registerCount;

#ifdef ENABLE_CHEQUE_SYSTEM
		return { total % YANG_PER_CHEQUE, static_cast<int>(total / YANG_PER_CHEQUE)};
#endif
		return { total };
	};

	// searching by hash
	if(hash != 0)
	{
		auto iter = m_priceByHash.find(hash);
		if(iter != m_priceByHash.end())
			return composePrice(iter->second.price, iter->second.count, count
#ifdef ENABLE_CHEQUE_SYSTEM
				, iter->second.cheque
#endif
			);
	}

	// searching by vnum
	auto iter = m_priceByVnum.find(vnum);
	if(iter != m_priceByVnum.end())
		return composePrice(iter->second.price, iter->second.count, count
#ifdef ENABLE_CHEQUE_SYSTEM
			, iter->second.cheque
#endif
		);

	// not found
	return std::nullopt;
}
#endif

PythonSafeList CPythonIkarusShop::GetFilterHistory()
{
	static auto extractFilter = [](const TFilterHistoryElement& filter){
		PythonGarbageCollector collector;
		PythonSafeList attrlist;
		for (auto& attr : filter.attrs)
			attrlist.append(collector.makeValue("(ii)", attr.bType, attr.sValue));

		PythonSafeDict filterDict;		
		filterDict.set("datetime", filter.datetime);
		filterDict.set("count", filter.count);
		filterDict.set("type", filter.type);
		filterDict.set("subtype", filter.subtype);
		filterDict.set("name", filter.name);
		filterDict.set("price_min", filter.pricestart.yang);
		filterDict.set("price_max", filter.pricend.yang);
		filterDict.set("level_min", filter.levelstart);
		filterDict.set("level_max", filter.levelend);
		filterDict.set("alchemy", filter.alchemyGrade);
		filterDict.set("sash", filter.sashGrade);
		filterDict.set("level_max", filter.levelend);
		filterDict.set("attrs", attrlist);
		return filterDict;
	};

	return ExtractPythonListFromContainer(m_filterHistory, extractFilter);
}

std::vector<std::string> CPythonIkarusShop::GetItemNames()
{
	// getting item map
	std::map<DWORD, std::string> nameMap;
	CItemManager::instance().GetItemsNameMap(nameMap);
	// making vector
	std::vector<std::string> vec;
	vec.reserve(nameMap.size());
	for(auto& [vnum, name] : nameMap)
		vec.emplace_back(name);
	return vec;
}

void CPythonIkarusShop::ShopBuilding_AddItem(int win, int slot)
{
	CallOnBusinessBoard("InsertItemInSlot" , Py_BuildValue("(ii)", win, slot));
}

void CPythonIkarusShop::AuctionBuilding_AddItem(int win, int slot)
{
	CallOnBusinessBoard("AuctionInsertItemInSlot" , Py_BuildValue("(ii)", win, slot));
}

#ifdef EXTEND_IKASHOP_PRO
void CPythonIkarusShop::RefreshBoardCounters(int safebox, int auction, int offers, int notifications)
#else
void CPythonIkarusShop::RefreshBoardCounters(int safebox, int auction, int offers)
#endif
{
	CallOnBusinessBoard("SetSafeboxCounter", Py_BuildValue("(i)", safebox));
	CallOnBusinessBoard("SetAuctionCounter", Py_BuildValue("(i)", auction));
	CallOnBusinessBoard("SetOffersCounter", Py_BuildValue("(i)", offers));
#ifdef EXTEND_IKASHOP_PRO
	CallOnBusinessBoard("SetNotificationCounter", Py_BuildValue("(i)", notifications));
#endif
}

#ifdef EXTEND_IKASHOP_PRO
void CPythonIkarusShop::RegisterNotification(const ikashop::TNotificationInfo& info)
{
	for(auto it = m_notifications.begin(); it != m_notifications.end(); it++)
	{
		if(it->datetime < info.datetime)
		{
			m_notifications.emplace(it, info);
			return;
		}
	}

	m_notifications.emplace_back(info);
}

void CPythonIkarusShop::LoadNotifications()
{
	m_notifications.clear();

	// opening stream
	auto filename = fmt::format("cache/shop_notifications_{}.json", CPythonPlayer::instance().GetName());
	std::ifstream stream(filename, std::ios::in | std::ios::ate | std::ios::binary);
	if (stream.is_open() == false)
		return;

	// reading size
	auto fsize = stream.tellg();

	// resetting reading point
	stream.seekg(0);

	// making string receiving file content
	std::string jsonString;
	jsonString.resize(fsize);

	// reading file content inside string
	stream.read(jsonString.data(), fsize);
	stream.close();

	// making document
	rapidjson::Document jsonDoc;
	jsonDoc.Parse(jsonString.c_str());

	// checking for parser errors
	if (jsonDoc.HasParseError()) {
		TraceError("PARSER ERROR PARSING NOTIFICATION FILE: %d", (int)jsonDoc.GetParseError());
		return;
	}

	// checking list object
	if (jsonDoc.IsArray() == false) {
		TraceError("INVALID NOTIFICATION FILE: document.IsArray() == false");
		return;
	}

	// getting current timestamp to check expiration
	const auto currentTime = std::time(nullptr);

	// iterating over array
	for (auto& member : jsonDoc.GetArray())
	{
		ikashop::TNotificationInfo info{};
		info.seenflag = true;
		info.type = static_cast<ikashop::ENotificationType>(jsonHelper::getValue<int>(member, "type"));
		info.what = static_cast<DWORD>(jsonHelper::getValue<int>(member, "what"));
		info.datetime = jsonHelper::getValue<int64_t>(member, "datetime");

		auto who = jsonHelper::getValue<std::string>(member, "who");
		std::strncpy(info.who, who.c_str(), sizeof(info.who));

		auto format = jsonHelper::getValue<std::string>(member, "format");
		std::strncpy(info.format, format.c_str(), sizeof(info.format));


		// checking expiration
		if (currentTime > info.datetime && currentTime - info.datetime > NOTIFICATION_EXPIRATION)
			continue;

		m_notifications.emplace_back(info);
	}
}

void CPythonIkarusShop::SaveNotifications()
{
	// creating document
	rapidjson::Document doc;
	doc.SetArray();

	// reserving array size
	doc.GetArray().Reserve(m_notifications.size(), doc.GetAllocator());

	// populating array
	for (auto& info : m_notifications)
	{
		rapidjson::Value member;
		member.SetObject();

		// encoding info
		jsonHelper::insertValue(member, "type", static_cast<int>(info.type), doc.GetAllocator());
		jsonHelper::insertValue(member, "who", info.who, doc.GetAllocator());
		jsonHelper::insertValue(member, "what", static_cast<int>(info.what), doc.GetAllocator());
		jsonHelper::insertValue(member, "format", info.format, doc.GetAllocator());
		jsonHelper::insertValue(member, "datetime", static_cast<int64_t>(info.datetime), doc.GetAllocator());

		// pushing element inside the array
		doc.PushBack(member, doc.GetAllocator());
	}

	// making json string
	rapidjson::StringBuffer buffer;
	buffer.Clear();

	rapidjson::Writer<rapidjson::StringBuffer> StringWriter(buffer);
	doc.Accept(StringWriter);

	// opening stream
	auto filename = fmt::format("cache/shop_notifications_{}.json", CPythonPlayer::instance().GetName());
	std::ofstream stream(filename, std::ios::out | std::ios::binary);
	if (!stream.is_open()) {
		TraceError("CANNOT SAVE NOTIFICATION FILE!");
		return;
	}

	// completing stream data
	stream.write(buffer.GetString(), buffer.GetSize());
	stream.close();
}

const std::deque<ikashop::TNotificationInfo>& CPythonIkarusShop::GetNotifications()
{
	return m_notifications;
}

void CPythonIkarusShop::SetNotificationList(const std::vector<ikashop::TNotificationInfo>& notifications)
{
	auto list = ExtractPythonListFromContainer(notifications, ExtractPythonDictNotification);
	CallOnBusinessBoard("SetNotifications", Py_BuildValue("(O)", list.weakRef()));
}
#endif



#ifdef ENABLE_IKASHOP_ENTITIES
std::string GetEntityNameFormat(const char* name
)
{
	// getting localeinfo module
	auto pyModule = PythonReference::steal(PyImport_ImportModule("localeInfo"));
	if (!pyModule)
		return name;

	// getting attr name
	const std::string attr = "IKASHOP_SHOP_NAME_FORMAT";

	// checking module has attr
	if(!PyObject_HasAttrString(pyModule, attr.c_str()))
		return name;

	// getting module attribute
	auto pyAttr = PythonReference::steal(PyObject_GetAttrString(pyModule, attr.c_str()));
	if(!pyAttr)
		return name;

	// checking attribute is string
	if(!PyString_Check(pyAttr))
		return name;

	// getting string from attr
	const auto* format = PyString_AsString(pyAttr);
	return fmt::format(fmt::runtime(format), name);
}

void CPythonIkarusShop::InsertEntity(DWORD vid, int type, const char* name, const char* ownerName, long x, long y, long z)
{
	// checking for existing entities
	RemoveEntity(vid);

	auto& shop = m_shopInstances.emplace_back(std::make_unique<ikashop::ShopInstance>(type));
	shop->SetVID(vid);
	shop->SetShopType(type);
	//shop->SetOwnerName(GetEntityNameFormat(ownerName));

	CPythonBackground&rkBgMgr=CPythonBackground::Instance();
	rkBgMgr.GlobalPositionToLocalPosition(x, y);

	z = CPythonBackground::Instance().GetHeight(x, y) + 10.0f;
	shop->Show(x,y,z);
}

void CPythonIkarusShop::RemoveEntity(DWORD vid)
{
	for (auto it = m_shopInstances.begin(); it != m_shopInstances.end(); it++)
	{
		ikashop::ShopInstance& shop = *(*it);
		if (shop.GetVID() == vid)
		{
			shop.Clear();
			m_shopInstances.erase(it);
			return;
		}
	}
}

void CPythonIkarusShop::RenderEntities()
{
#ifdef EXTEND_IKASHOP_PRO
	auto mainInstance = CPythonCharacterManager::instance().GetMainInstancePtr();
#endif

	CPythonGraphic::Instance().SetDiffuseOperation();
	for (auto & iter : m_shopInstances)
	{
#ifdef EXTEND_IKASHOP_PRO
		if (!mainInstance || mainInstance->GetDistance(iter->GetThingInstancePtr()->GetPosition()) > CPythonSystem::instance().GetShopRangeView()) {
			iter->FarFromView(true);
			continue;
		}
#endif

		if (CPythonSystem::instance().IsShopViewOnlyFound()) {
			auto foundShopIt = m_foundShopMap.find(iter->GetVID());
			if (foundShopIt == m_foundShopMap.end())
			{
				iter->FarFromView(true);
				continue;
			}
		}

		iter->FarFromView(false);
		iter->Render();
		iter->BlendRender();
	}

	if (CPythonSystem::Instance().IsShowSalesText()) {
		auto& graphic = CPythonGraphic::Instance();
		graphic.PushState();
		graphic.SetInterfaceRenderState();
		for (auto& instance : m_shopInstances){
#ifdef EXTEND_IKASHOP_PRO
			if (instance->IsFarFromView())
				continue;
#endif
			instance->RenderText();
		}
		graphic.PopState();
		graphic.SetGameRenderState();
	}
}


void CPythonIkarusShop::UpdateEntities()
{
	for (auto & iter : m_shopInstances)
		iter->Update();
}

ikashop::ShopInstance* CPythonIkarusShop::GetShopInstancePtr(DWORD vid)
{
	for (const auto& shopInstance : m_shopInstances)
	{
		if (shopInstance->GetVID() == vid)
		{
			return shopInstance.get();
		}
	}
	return nullptr;
}

bool CPythonIkarusShop::GetShowNameFlag()
{
	return m_bIsShowName;
}

void CPythonIkarusShop::SetShowNameFlag(bool flag)
{
	m_bIsShowName = flag;
}

void CPythonIkarusShop::DeleteEntities()
{
	for (auto& iter : m_shopInstances)
		iter->Clear();

	m_shopInstances.clear();
}

bool CPythonIkarusShop::CheckPickedShopEntity(DWORD* ret)
{
	for (auto& instance : m_shopInstances) {
		if (instance->IsPickable()) {
			if (instance->IsPicked()) {
				if (ret)
					*ret = instance->GetVID();
				return true;
			}
		}
	}
	return false;
}
#endif


void CPythonIkarusShop::DeleteSearchResult(DWORD itemid)
{
	CallOnSearchShopBoard("DeleteSearchResultItem", Py_BuildValue("(i)", itemid));
}

void CPythonIkarusShop::PopupMessage(const std::string& message)
{
	CallOnBusinessBoard("ServerPopupMessage", Py_BuildValue("(s)", message.c_str()));
}

CPythonIkarusShop::~CPythonIkarusShop()
{
}

PyObject* ikashopSendShopReopen(PyObject* poSelf, PyObject* poArgs)
{
	char* szName;
	if (!PyTuple_GetString(poArgs, 0, &szName))
		return Py_BadArgument();

	BYTE timeIndex;
	if (!PyTuple_GetByte(poArgs, 1, &timeIndex))
		return Py_BadArgument();
	CPythonNetworkStream::instance().SendIkarusShopReopenShop(szName, timeIndex);
	return Py_BuildNone();
}

PyObject* ikashopSendForceCloseShop(PyObject* poSelf , PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopForceCloseShop();
	return Py_BuildNone();
}

PyObject* ikashopSendOpenShop(PyObject* poSelf , PyObject* poArgs)
{
	uint32_t id = 0;
	if(!PyTuple_GetUnsignedInteger(poArgs, 0, &id))
		return Py_BadArgument();

	CPythonNetworkStream::Instance().SendIkarusShopOpenShop(id);
	return Py_BuildNone();
}

PyObject* ikashopSendOpenShopOwner(PyObject* poSelf , PyObject* poArgs)
{
	CPythonNetworkStream::Instance().SendIkarusShopOpenShopOwner();
	return Py_BuildNone();
}

PyObject* ikashopSendBuyItem(PyObject* poSelf, PyObject* poArgs)
{
	uint32_t ownerid = 0;
	if(!PyTuple_GetUnsignedInteger(poArgs, 0, &ownerid))
		return Py_BadArgument();

	uint32_t itemid = 0;
	if (!PyTuple_GetUnsignedInteger(poArgs, 1, &itemid))
		return Py_BadArgument();

	long long seenprice = 0;
	if (!PyTuple_GetLongLong(poArgs, 2, &seenprice))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusShopBuyItem(ownerid, itemid, false, seenprice);
	return Py_BuildNone();
}

PyObject* ikashopSendAddItem(PyObject* poSelf , PyObject* poArgs)
{
	BYTE window = 0;
	if(!PyTuple_GetInteger(poArgs, 0, &window))
		return Py_BadArgument();

	WORD slot = 0;
	if(!PyTuple_GetInteger(poArgs, 1, &slot))
		return Py_BadArgument();

	int destpos = 0;
	if (!PyTuple_GetInteger(poArgs, 2, &destpos))
		return Py_BadArgument();

	long long yang=0;
	if(!PyTuple_GetLongLong(poArgs, 3, &yang))
		return Py_BadArgument();

#ifdef  ENABLE_CHEQUE_SYSTEM
	int cheque = 0;
	if(!PyTuple_GetInteger(poArgs, 4, &cheque))
		return Py_BadArgument();
#endif

	ikashop::TShopItemInfo info;
	info.pos = {window, slot};
	info.price.yang = yang;
	info.destpos = destpos;
#ifdef ENABLE_CHEQUE_SYSTEM
	info.price.cheque = cheque;
#endif

	CPythonNetworkStream::instance().SendIkarusShopAddItem(info);
	return Py_BuildNone();
}

PyObject* ikashopSendRemoveItem(PyObject* poSelf , PyObject* poArgs)
{
	uint32_t itemid = 0;
	if(!PyTuple_GetUnsignedInteger(poArgs, 0, &itemid))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusShopRemoveItem(itemid);
	return Py_BuildNone();
}

PyObject* ikashopSendRemoveAllItem(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopRemoveAllItem();
	return Py_BuildNone();
}

PyObject* ikashopSendEditItem(PyObject* poSelf , PyObject* poArgs)
{
	uint32_t itemid=0;
	if(!PyTuple_GetUnsignedInteger(poArgs, 0, &itemid))
		return Py_BadArgument();

	long long yang=0;
	if(!PyTuple_GetLongLong(poArgs, 1, &yang))
		return Py_BadArgument();

#ifdef  ENABLE_CHEQUE_SYSTEM
	int cheque = 0;
	if(!PyTuple_GetInteger(poArgs, 2, &cheque))
		return Py_BadArgument();
#endif

	ikashop::TPriceInfo price;
	price.yang = yang;
#ifdef ENABLE_CHEQUE_SYSTEM
	price.cheque = cheque;
#endif

	CPythonNetworkStream::instance().SendOfflineShopEditItem(itemid, price);
	return Py_BuildNone();
}

PyObject* ikashopSendSearchItem(PyObject* poSelf, PyObject* poArgs)
{
	DWORD itemVnum = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &itemVnum))
		return Py_BadArgument();

	int socket0 = 0;
	if (!PyTuple_GetInteger(poArgs, 1, &socket0))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusSearchItem(itemVnum, socket0);
	return Py_BuildNone();
}

PyObject* ikashopSendRequestEdit(PyObject* poSelf, PyObject* poArgs)
{
	bool state;
	if (!PyTuple_GetBoolean(poArgs, 0, &state))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusRequestEdit(state);
	return Py_BuildNone();
}

PyObject* ikashopSendFilterRequest(PyObject* poSelf , PyObject* poArgs)
{
	char* name = nullptr;
	ikashop::TFilterInfo info;
	if(!PyTuple_GetString(poArgs, 0, &name))
		return Py_BadArgument();

	if(!PyTuple_GetInteger(poArgs, 1, &info.type))
		return Py_BadArgument();

	if(!PyTuple_GetInteger(poArgs, 2, &info.subtype))
		return Py_BadArgument();

	PyObject* prices = nullptr;
	if(!PyTuple_GetObject(poArgs, 3, &prices))
		return Py_BadArgument();

	PyObject* levels = nullptr;
	if(!PyTuple_GetObject(poArgs, 4, &levels))
		return Py_BadArgument();

	PyObject* attrs=nullptr;
	if(!PyTuple_GetObject(poArgs, 5, &attrs))
		return Py_BadArgument();

	PyObject* specials=nullptr;
	if(!PyTuple_GetObject(poArgs, 6, &specials))
		return Py_BadArgument();

	// extracting prices
	if(!PyTuple_GetLongLong(prices, 0, &info.pricestart.yang))
		return Py_BadArgument();

	if(!PyTuple_GetLongLong(prices, 1, &info.pricend.yang))
		return Py_BadArgument();

	// extracting levels
	if(!PyTuple_GetInteger(levels, 0, &info.levelstart))
		return Py_BadArgument();

	if(!PyTuple_GetInteger(levels, 1, &info.levelend))
		return Py_BadArgument();

	// extracting attrs
	for (int i = 0; i < ITEM_ATTRIBUTE_SLOT_NORM_NUM; i++)
	{
		PyObject* attribute = nullptr;
		if(!PyTuple_GetObject(attrs, i, &attribute))
			return Py_BadArgument();

		if(!PyTuple_GetInteger(attribute, 0, &info.attrs[i].bType))
			return Py_BadArgument();

		int value = 0;
		if(!PyTuple_GetInteger(attribute, 1, &value))
			return Py_BadArgument();

		info.attrs[i].sValue = static_cast<short>(value);
	}

	// extracting specials
	if (!PyTuple_GetInteger(specials, 0, &info.sashGrade))
		return Py_BadArgument();

	if (!PyTuple_GetInteger(specials, 1, &info.alchemyGrade))
		return Py_BadArgument();

	// copying name
	strncpy(info.name, name, sizeof(info.name));

	CPythonNetworkStream::instance().SendIkarusShopFilterRequest(info);
	return Py_BuildNone();
}

PyObject* ikashopSendRandomSearchFillRequest(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopSearchFillRequest();
	return Py_BuildNone();
}

PyObject* ikashopSendOfferCreate(PyObject* poSelf , PyObject* poArgs)
{
	ikashop::TOfferInfo offer;
	
	if(!PyTuple_GetUnsignedLong(poArgs, 0, &offer.ownerid))
		return Py_BadArgument();

	if (!PyTuple_GetUnsignedLong(poArgs, 1, &offer.itemid))
		return Py_BadArgument();

	if (!PyTuple_GetLongLong(poArgs, 2, &offer.price.yang))
		return Py_BadArgument();

#ifdef ENABLE_CHEQUE_SYSTEM
	if(!PyTuple_GetInteger(poArgs, 3, &offer.price.cheque))
		return Py_BadArgument();
#endif

	CPythonNetworkStream::instance().SendIkarusShopOfferCreate(offer);
	return Py_BuildNone();
}

PyObject* ikashopSendOfferAccept(PyObject* poSelf , PyObject* poArgs)
{
	DWORD offerid=0;
	if(!PyTuple_GetUnsignedLong(poArgs, 0, &offerid))
		return Py_BadArgument();
	
	CPythonNetworkStream::instance().SendIkarusShopOfferAccept(offerid);
	return Py_BuildNone();
}

PyObject* ikashopSendOfferCancel(PyObject* poSelf , PyObject* poArgs)
{
	DWORD offerid = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &offerid))
		return Py_BadArgument();

	DWORD ownerid = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 1, &ownerid))
		return Py_BadArgument();
	
	CPythonNetworkStream::instance().SendIkarusShopOfferCancel(offerid, ownerid);
	return Py_BuildNone();
}

PyObject* ikashopSendOfferListRequest(PyObject* poSelf , PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopOfferListRequest();
	return Py_BuildNone();
}

PyObject* ikashopSendSafeboxOpen(PyObject* poSelf , PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopSafeboxOpen();
	return Py_BuildNone();
}

PyObject* ikashopSendSafeboxGetItem(PyObject* poSelf , PyObject* poArgs)
{
	DWORD itemid = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &itemid))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusShopSafeboxGetItem(itemid);
	return Py_BuildNone();
}

PyObject* ikashopSendSafeboxGetValutes(PyObject* poSelf , PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopSafeboxGetValutes();
	return Py_BuildNone();
}

PyObject* ikashopSendSafeboxClose(PyObject* poSelf , PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopSafeboxClose();
	return Py_BuildNone();
}

PyObject* ikashopSendOffersListClose(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusOffersListClose();
	return Py_BuildNone();
}

PyObject* ikashopSendCloseMyShopBoard(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusMyShopCloseBoard();
	return Py_BuildNone();
}

PyObject* ikashopSendCloseShopGuestBoard(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusCloseShopGuestBoard();
	return Py_BuildNone();
}

PyObject* ikashopSendChangeName(PyObject* poSelf, PyObject* poArgs)
{
	char* szName;
	if (!PyTuple_GetString(poArgs, 0, &szName))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusShopChangeName(szName);
	return Py_BuildNone();
}

PyObject* ikashopSendAuctionListRequest(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopAuctionListRequest();
	return Py_BuildNone();
}

PyObject* ikashopSendAuctionOpenAuction(PyObject* poSelf, PyObject* poArgs)
{
	DWORD ownerid = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &ownerid))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusShopAuctionOpen(ownerid);
	return Py_BuildNone();
}

PyObject* ikashopSendAuctionAddOffer(PyObject* poSelf, PyObject* poArgs)
{
	DWORD ownerid = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &ownerid))
		return Py_BadArgument();

	ikashop::TPriceInfo price;
	if(!PyTuple_GetLongLong(poArgs, 1, &price.yang))
		return Py_BadArgument();

#ifdef ENABLE_CHEQUE_SYSTEM
	if(!PyTuple_GetInteger(poArgs, 2, &price.cheque))
		return Py_BadArgument();
#endif

	CPythonNetworkStream::instance().SendIkarusShopAuctionAddOffer(ownerid, price);
	return Py_BuildNone();
}

PyObject* ikashopSendAuctionExitFrom(PyObject* poSelf, PyObject* poArgs)
{
	DWORD ownerid = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &ownerid))
		return Py_BadArgument();
	CPythonNetworkStream::instance().SendIkarusShopAuctionExitFrom(ownerid);
	return Py_BuildNone();
}

PyObject* ikashopSendAuctionCreate(PyObject* poSelf, PyObject* poArgs)
{
	TItemPos pos;
	if(!PyTuple_GetInteger(poArgs, 0, &pos.window_type))
		return Py_BadArgument();
	
	if(!PyTuple_GetInteger(poArgs, 1, &pos.cell))
		return Py_BadArgument();

	ikashop::TPriceInfo price;
	if(!PyTuple_GetLongLong(poArgs, 2, &price.yang))
		return Py_BadArgument();

	CPythonNetworkStream::instance().SendIkarusShopAuctionCreate(pos, price);
	return Py_BuildNone();
}

PyObject* ikashopSendMyAuctionOpen(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopMyAuctionOpen();
	return Py_BuildNone();
}

PyObject* ikashopSendMyAuctionClose(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopMyAuctionClose();
	return Py_BuildNone();
}

PyObject* ikashopSendMyAuctionCancel(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopMyAuctionCancel();
	return Py_BuildNone();
}

PyObject* SendCloseMyShopBoard(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusMyShopCloseBoard();
	return Py_BuildNone();
}

PyObject* ikashopGetFilterHistory(PyObject* poSelf , PyObject* poArgs)
{
	auto list = CPythonIkarusShop::instance().GetFilterHistory();
	return list.incRef();
}

PyObject* ikashopRegisterFilterHistory(PyObject* poSelf , PyObject* poArgs)
{
	CPythonIkarusShop::TFilterHistoryElement element;	
	CPythonIkarusShop::NowDatetime(element.datetime);

	if(!PyTuple_GetUnsignedLong(poArgs, 0, &element.count))
		return Py_BadArgument();

	char* name=nullptr;
	if(!PyTuple_GetString(poArgs, 1, &name))
		return Py_BadArgument();
	strncpy(element.name, name, sizeof(element.name));

	if(!PyTuple_GetInteger(poArgs, 2, &element.type))
		return Py_BadArgument();

	if(!PyTuple_GetInteger(poArgs, 3, &element.subtype))
		return Py_BadArgument();
	
	PyObject* prices = nullptr;
	if (!PyTuple_GetObject(poArgs, 4, &prices))
		return Py_BadArgument();
	
	PyObject* levels = nullptr;
	if (!PyTuple_GetObject(poArgs, 5, &levels))
		return Py_BadArgument();
	
	PyObject* attrs = nullptr;
	if (!PyTuple_GetObject(poArgs, 6, &attrs))
		return Py_BadArgument();

	PyObject* specials = nullptr;
	if (!PyTuple_GetObject(poArgs, 7, &specials))
		return Py_BadArgument();

	// extracting prices
	if(!PyTuple_GetLongLong(prices, 0, &element.pricestart.yang))
		return Py_BadArgument();
	if(!PyTuple_GetLongLong(prices, 1, &element.pricend.yang))
		return Py_BadArgument();

	// extracting levels
	if(!PyTuple_GetInteger(levels, 0, &element.levelstart))
		return Py_BadArgument();
	if(!PyTuple_GetInteger(levels, 1, &element.levelend))
		return Py_BadArgument();

	// extracting attributes
	for (int i = 0; i < ITEM_ATTRIBUTE_SLOT_NORM_NUM; i++)
	{
		PyObject* attribute = nullptr;
		if(!PyTuple_GetObject(attrs, i, &attribute))
			return Py_BadArgument();

		int value = 0;
		if(!PyTuple_GetInteger(attribute, 0, &element.attrs[i].bType))
			return Py_BadArgument();
		if(!PyTuple_GetInteger(attribute, 1, &value))
			return Py_BadArgument();

		element.attrs[i].sValue = static_cast<short>(value);
	}

	// extracting specials
	if (!PyTuple_GetInteger(specials, 0, &element.sashGrade))
		return Py_BadArgument();

	if (!PyTuple_GetInteger(specials, 1, &element.alchemyGrade))
		return Py_BadArgument();

	CPythonIkarusShop::instance().AddNewFilterToHistory(element);
	return Py_BuildNone();
}

PyObject* ikashopGetItemNames(PyObject* poSelf, PyObject* poArgs)
{
	auto vec = CPythonIkarusShop::instance().GetItemNames();
	// converting map to dict
	PythonSafeList list;
	for(auto& name : vec)
		list.append(name);
	return list.incRef();
}

PyObject* ikashopSetBusinessBoard(PyObject* poSelf , PyObject* poArgs)
{
	PyObject* board = nullptr;
	if(!PyTuple_GetObject(poArgs, 0, &board))
		return Py_BadArgument();
	if(board == Py_None)
		board = nullptr;
	CPythonIkarusShop::instance().SetBusinessBoard(board);
	return Py_BuildNone();
}

PyObject* ikashopSetSearchShopBoard(PyObject* poSelf , PyObject* poArgs)
{
	PyObject* board = nullptr;
	if (!PyTuple_GetObject(poArgs, 0, &board))
		return Py_BadArgument();
	
	if (board == Py_None)
		board = nullptr;

	CPythonIkarusShop::instance().SetSearchShopBoard(board);
	return Py_BuildNone();
}

PyObject* ikashopGetBusinessBoard(PyObject* poSelf , PyObject* poArgs)
{
	PyObject* poInterface = CPythonIkarusShop::instance().GetBusinessBoard();
	
	if(!poInterface)
		return Py_BuildNone();

	Py_IncRef(poInterface);
	return poInterface;
}


PyObject* ikashopShopBuilding_AddItem(PyObject* poSelf, PyObject* poArgs)
{
	int win = 0;
	if(!PyTuple_GetInteger(poArgs, 0, &win))
		return Py_BadArgument();

	int slot = 0;
	if(!PyTuple_GetInteger(poArgs, 1, &slot))
		return Py_BadArgument();

	CPythonIkarusShop::instance().ShopBuilding_AddItem(win, slot);
	return Py_BuildNone();
}

PyObject* ikashopAuctionBuilding_AddItem(PyObject* poSelf, PyObject* poArgs)
{
	int win = 0;
	if (!PyTuple_GetInteger(poArgs, 0, &win))
		return Py_BadArgument();

	int slot = 0;
	if (!PyTuple_GetInteger(poArgs, 1, &slot))
		return Py_BadArgument();

	CPythonIkarusShop::instance().AuctionBuilding_AddItem(win, slot);
	return Py_BuildNone();
}

#ifdef ENABLE_IKASHOP_ENTITIES
PyObject* ikashopHideShopNames(PyObject* poSelf, PyObject* poArgs)
{
	CPythonIkarusShop::instance().SetShowNameFlag(false);
	return Py_BuildNone();
}

PyObject* ikashopShowShopNames(PyObject* poSelf, PyObject* poArgs)
{
	CPythonIkarusShop::instance().SetShowNameFlag(true);
	return Py_BuildNone();
}

#ifdef EXTEND_IKASHOP_PRO
PyObject* ikashopSendMoveShopEntity(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusShopMoveShopEntity();
	return Py_BuildNone();
}
#endif
#endif

PyObject* ikashopGetFirstEmptyCell(PyObject* poSelf, PyObject* poArgs)
{
	int size = 0;
	if (!PyTuple_GetInteger(poArgs, 0, &size))
		return Py_BadArgument();

	PyObject* items = nullptr;
	if (!PyTuple_GetObject(poArgs, 1, &items) || !PyList_Check(items))
		return Py_BadArgument();

	constexpr auto width = SHOP_PLAYER_WIDTH;
	constexpr auto height = SHOP_PLAYER_HEIGHT;

	ikashop::CShopGrid<width, height> grid{};

	for(int i=0; i < PyList_Size(items); i++)
	{
		auto info = PyList_GetItem(items, i);
		if(!PyTuple_Check(info) || PyTuple_Size(info) != 2)
			return Py_BadArgument();

		DWORD vnum = 0;
		if(!PyTuple_GetUnsignedLong(info, 0, &vnum))
			return Py_BadArgument();
		int cell = 0;
		if(!PyTuple_GetInteger(info, 1, &cell))
			return Py_BadArgument();

		CItemData* data = nullptr;
		if(!CItemManager::instance().GetItemDataPointer(vnum, &data))
			return Py_BadArgument();

		grid.RegisterItem(cell, data->GetSize());
	}

	const auto cell = grid.FindSpace(size);
	if(!cell)
		return Py_BuildNone();

	return PyInt_FromLong(*cell);
}

PyObject* ikashopCheckShopSpace(PyObject* poSelf, PyObject* poArgs)
{
	PyObject* items = nullptr;
	if (!PyTuple_GetObject(poArgs, 0, &items) || !PyList_Check(items))
		return Py_BadArgument();

	DWORD vnum = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 1, &vnum))
		return Py_BadArgument();

	int cell = 0;
	if (!PyTuple_GetInteger(poArgs, 2, &cell))
		return Py_BadArgument();


	CItemData* itemData = nullptr;
	if(!CItemManager::instance().GetItemDataPointer(vnum, &itemData))
		return PyBool_FromLong(false);

	constexpr auto width = SHOP_PLAYER_WIDTH;
	constexpr auto height = SHOP_PLAYER_HEIGHT;

	ikashop::CShopGrid<width, height> grid{};

	// filling grids with items
	for(int i=0; i < PyList_Size(items); i++)
	{
		auto info = PyList_GetItem(items, i);
		if(!PyTuple_Check(info) || PyTuple_Size(info) != 2)
			return Py_BadArgument();

		DWORD vnum = 0;
		if(!PyTuple_GetUnsignedLong(info, 0, &vnum))
			return Py_BadArgument();
		int cell = 0;
		if(!PyTuple_GetInteger(info, 1, &cell))
			return Py_BadArgument();

		CItemData* data = nullptr;
		if(!CItemManager::instance().GetItemDataPointer(vnum, &data))
			return Py_BadArgument();

		grid.RegisterItem(cell, data->GetSize());
	}

	return PyBool_FromLong(grid.CheckSpace(cell, itemData->GetSize()));
}

PyObject* ikashopGetItemHash(PyObject* poSelf, PyObject* poArgs)
{
	// extracting data
	DWORD vnum = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &vnum))
		return Py_BadArgument();

	DWORD count = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 1, &count))
		return Py_BadArgument();

	PyObject* sockets = nullptr;
	if (!PyTuple_GetObject(poArgs, 2, &sockets))
		return Py_BadArgument();

	PyObject* attrs = nullptr;
	if (!PyTuple_GetObject(poArgs, 3, &attrs))
		return Py_BadArgument();

	PyObject* attribute = nullptr;
	int attrvalue = 0;

	ikashop::TShopPlayerItem item{};
	item.vnum = vnum;
	item.count = count;

	for (int i = 0; i < ITEM_SOCKET_SLOT_MAX_NUM; i++)
		if (!PyTuple_GetLong(sockets, i, &item.alSockets[i]))
			return Py_BadArgument();

	for (int i = 0; i < ITEM_ATTRIBUTE_SLOT_MAX_NUM; i++)
	{
		if (!PyTuple_GetObject(attrs, i, &attribute) || !PyTuple_GetInteger(attribute, 0, &item.aAttr[i].bType) || !PyTuple_GetInteger(attribute, 1, &attrvalue))
			return Py_BadArgument();
		item.aAttr[i].sValue = static_cast<short>(attrvalue);
	}

	return PyInt_FromSize_t(static_cast<size_t>(ikashop::GetItemHash(item)));
}

#ifdef EXTEND_IKASHOP_PRO
PyObject* ikashopGetItemPriceCache(PyObject* poSelf, PyObject* poArgs)
{
	// extracting data
	DWORD vnum = 0;
	if(!PyTuple_GetUnsignedLong(poArgs, 0, &vnum))
		return Py_BadArgument();

	DWORD hash = 0;
	if(!PyTuple_GetUnsignedLong(poArgs, 1, &hash))
		return Py_BadArgument();

	DWORD count = 0;
	if(!PyTuple_GetUnsignedLong(poArgs, 2, &count))
		return Py_BadArgument();

	// getting price
	auto price = CPythonIkarusShop::instance().GetPriceHistory(vnum, hash, count);
	if(!price)
		return Py_BuildNone();

	// making return value
	PythonSafeDict dict;
	dict.set("value", price->yang);
#ifdef ENABLE_CHEQUE_SYSTEM
	dict.set("cheque", price->cheque);
#endif
	return dict.incRef();
}

PyObject* ikashopRegisterItemPriceCache(PyObject* poSelf, PyObject* poArgs)
{
	// extracting data
	DWORD vnum = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &vnum))
		return Py_BadArgument();

	DWORD hash = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 1, &hash))
		return Py_BadArgument();

	DWORD count = 0;
	if (!PyTuple_GetUnsignedLong(poArgs, 2, &count))
		return Py_BadArgument();

	ikashop::TPriceInfo price;
	if (!PyTuple_GetLongLong(poArgs, 3, &price.yang))
		return Py_BadArgument();
#ifdef ENABLE_CHEQUE_SYSTEM
	if (!PyTuple_GetInteger(poArgs, 4, &price.cheque))
		return Py_BadArgument();
#endif

	CPythonIkarusShop::Instance().RegisterPriceHistory(vnum, count, hash, price);
	return Py_BuildNone();
}

PyObject* ikashopLoadNotifications(PyObject* poSelf, PyObject* poArgs)
{
	CPythonIkarusShop::instance().LoadNotifications();
	return Py_BuildNone();
}

PyObject* ikashopSendNotificationListRequest(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusNotificationListRequest();
	return Py_BuildNone();
}

PyObject* ikashopSendNotificationListClose(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusNotificationListClose();
	return Py_BuildNone();
}

PyObject* ikashopRegisterNotification(PyObject* poSelf, PyObject* poArgs)
{
	ikashop::TNotificationInfo info{};
	info.seenflag = true;

	int type = 0;
	if(!PyTuple_GetInteger(poArgs, 0, &type))
		return Py_BadArgument();
	info.type = static_cast<ikashop::ENotificationType>(type);

	char* who = nullptr;
	if (!PyTuple_GetString(poArgs, 1, &who))
		return Py_BadArgument();
	std::strncpy(info.who, who, sizeof(info.who));

	if (!PyTuple_GetUnsignedLong(poArgs, 2, &info.what))
		return Py_BadArgument();

	char* format = nullptr;
	if (!PyTuple_GetString(poArgs, 3, &format))
		return Py_BadArgument();
	std::strncpy(info.format, format, sizeof(info.format));

	if (!PyTuple_GetLongLong(poArgs, 4, &info.datetime))
		return Py_BadArgument();

	bool save = false;
	if(!PyTuple_GetBoolean(poArgs, 5, &save))
		return Py_BadArgument();

	CPythonIkarusShop::instance().RegisterNotification(info);
	if(save)
		CPythonIkarusShop::instance().SaveNotifications();
	return Py_BuildNone();
}

PyObject* ikashopGetNotifications(PyObject* poSelf, PyObject* poArgs)
{
	auto& notifications = CPythonIkarusShop::instance().GetNotifications();
	auto list = ExtractPythonListFromContainer(notifications, ExtractPythonDictNotification);
	return list.incRef();
}
#endif

PyObject* ikashopGetProjectPosition(PyObject* poSelf, PyObject* poArgs)
{
	int iVirtualID;
	if (!PyTuple_GetInteger(poArgs, 0, &iVirtualID))
		return Py_BuildException();
	int iHeight;
	if (!PyTuple_GetInteger(poArgs, 1, &iHeight))
		return Py_BuildException();

	auto shopInstance = CPythonIkarusShop::instance().GetShopInstancePtr(iVirtualID);

	if (!shopInstance)
		return Py_BuildValue("ii", -100, -100);

	auto position = shopInstance->GetPixelPos();

	CPythonGraphic& rpyGraphic = CPythonGraphic::Instance();

	float fx, fy, fz;
	rpyGraphic.ProjectPosition(position.x,
		position.y,
		position.z + float(iHeight),
		&fx, &fy, &fz);

	if (1 == int(fz))
		return Py_BuildValue("ii", -100, -100);

	float scale = CPythonSystem::instance().GetScaleInterface();
	return Py_BuildValue("ii", int(fx / scale), int(fy / scale));
}

PyObject* ikashopSendOnClickPacket(PyObject* poSelf, PyObject* poArgs)
{
	int iVirtualID;
	if (!PyTuple_GetInteger(poArgs, 0, &iVirtualID))
		return Py_BuildException();

	auto shopInstance = CPythonIkarusShop::instance().GetShopInstancePtr(iVirtualID);
	if (!shopInstance)
		return Py_BuildNone();

	CPythonNetworkStream::instance().SendIkarusShopOnClickShopEntity(iVirtualID);
	return Py_BuildNone();
}

PyObject* ikashopSendFindMyShop(PyObject* poSelf, PyObject* poArgs)
{
	CPythonNetworkStream::instance().SendIkarusFindMyShop();
	return Py_BuildNone();
}

PyObject* ikashopIsFoundShopFromSearchItem(PyObject* poSelf, PyObject* poArgs)
{
	DWORD dwVID;
	if (!PyTuple_GetUnsignedLong(poArgs, 0, &dwVID))
		return Py_BuildException();

	CPythonNetworkStream::instance().SendIkarusFindMyShop();
	return Py_BuildValue("(i)", CPythonIkarusShop::instance().IsFoundShopFromSearchItem(dwVID));
}

PyObject* ikashopClearFoundShopMap(PyObject* poSelf, PyObject* poArgs)
{
	CPythonIkarusShop::instance().ClearFoundShopMap();
	return Py_BuildNone();
}

void initikashop()
{
	static PyMethodDef s_methods[] =
	{
		//packets exchanging
		{ "SendShopReopen", ikashopSendShopReopen, METH_VARARGS },
		{ "SendForceCloseShop", ikashopSendForceCloseShop, METH_VARARGS },
		{ "SendOpenShop", ikashopSendOpenShop, METH_VARARGS },
		{ "SendOpenShopOwner", ikashopSendOpenShopOwner, METH_VARARGS },
		{ "SendBuyItem", ikashopSendBuyItem, METH_VARARGS },
		{ "SendAddItem", ikashopSendAddItem, METH_VARARGS },
		{ "SendRemoveItem", ikashopSendRemoveItem, METH_VARARGS },
		{ "SendRemoveAllItem", ikashopSendRemoveAllItem, METH_VARARGS },
		{ "SendEditItem", ikashopSendEditItem, METH_VARARGS },
		{ "SendFilterRequest", ikashopSendFilterRequest, METH_VARARGS },
		{ "SendRandomSearchFillRequest", ikashopSendRandomSearchFillRequest, METH_VARARGS },
		{ "SendOfferCreate", ikashopSendOfferCreate, METH_VARARGS },
		{ "SendOfferAccept", ikashopSendOfferAccept, METH_VARARGS },
		{ "SendOfferCancel", ikashopSendOfferCancel, METH_VARARGS },
		{ "SendOfferListRequest", ikashopSendOfferListRequest, METH_VARARGS },
		{ "SendSafeboxOpen", ikashopSendSafeboxOpen, METH_VARARGS },
		{ "SendSafeboxGetItem", ikashopSendSafeboxGetItem, METH_VARARGS },
		{ "SendSafeboxGetValutes", ikashopSendSafeboxGetValutes, METH_VARARGS },
		{ "SendSafeboxClose", ikashopSendSafeboxClose, METH_VARARGS },
		{ "SendOffersListClose", ikashopSendOffersListClose, METH_VARARGS },
		{ "SendCloseMyShopBoard", ikashopSendCloseMyShopBoard, METH_VARARGS },
		{ "SendCloseShopGuestBoard", ikashopSendCloseShopGuestBoard, METH_VARARGS },
		{ "SendChangeName", ikashopSendChangeName, METH_VARARGS },
		{ "SendFindMyShop", ikashopSendFindMyShop, METH_VARARGS },
		{ "SendSearchItem", ikashopSendSearchItem, METH_VARARGS },
		
		//AUCTION
		{ "SendAuctionListRequest", ikashopSendAuctionListRequest, METH_VARARGS },
		{ "SendAuctionOpenAuction", ikashopSendAuctionOpenAuction, METH_VARARGS },
		{ "SendAuctionAddOffer", ikashopSendAuctionAddOffer, METH_VARARGS },
		{ "SendAuctionExitFrom", ikashopSendAuctionExitFrom, METH_VARARGS },
		{ "SendAuctionCreate", ikashopSendAuctionCreate, METH_VARARGS },
		{ "SendMyAuctionOpen", ikashopSendMyAuctionOpen, METH_VARARGS },
		{ "SendMyAuctionClose", ikashopSendMyAuctionClose, METH_VARARGS },
		{ "SendMyAuctionCancel", ikashopSendMyAuctionCancel, METH_VARARGS },

		//interfaces methods
		{ "GetFilterHistory", ikashopGetFilterHistory, METH_VARARGS },
		{ "RegisterFilterHistory", ikashopRegisterFilterHistory, METH_VARARGS },

		{ "GetItemNames", ikashopGetItemNames, METH_VARARGS },
	
		{ "SetSearchShopBoard", ikashopSetSearchShopBoard, METH_VARARGS },
		{ "SetBusinessBoard", ikashopSetBusinessBoard, METH_VARARGS },
		{ "GetBusinessBoard", ikashopGetBusinessBoard, METH_VARARGS },
		
		//create shop methods
		{ "AddItemOnShop", ikashopShopBuilding_AddItem, METH_VARARGS },
		{ "AddItemOnAuction", ikashopAuctionBuilding_AddItem, METH_VARARGS },

#ifdef ENABLE_IKASHOP_ENTITIES
		{ "HideShopNames", ikashopHideShopNames, METH_VARARGS },
		{ "ShowShopNames", ikashopShowShopNames, METH_VARARGS },
#ifdef EXTEND_IKASHOP_PRO
		{ "SendMoveShopEntity", ikashopSendMoveShopEntity, METH_VARARGS },
#endif
#endif

		// helpers
		{ "GetFirstEmptyCell", ikashopGetFirstEmptyCell, METH_VARARGS },
		{ "CheckShopSpace", ikashopCheckShopSpace, METH_VARARGS },

		{ "GetItemHash", ikashopGetItemHash, METH_VARARGS },

		// mt2009
		{ "GetProjectPosition", ikashopGetProjectPosition, METH_VARARGS },
		{ "SendOnClickPacket", ikashopSendOnClickPacket, METH_VARARGS },
		// end of mt2009


#ifdef EXTEND_IKASHOP_PRO
		// price cache
		{ "GetItemPriceCache", ikashopGetItemPriceCache, METH_VARARGS },
		{ "RegisterItemPriceCache", ikashopRegisterItemPriceCache, METH_VARARGS },

		{ "LoadNotifications", ikashopLoadNotifications, METH_VARARGS },
		{ "SendNotificationListRequest", ikashopSendNotificationListRequest, METH_VARARGS },
		{ "SendNotificationListClose", ikashopSendNotificationListClose, METH_VARARGS },
		{ "RegisterNotification", ikashopRegisterNotification, METH_VARARGS },
		{ "GetNotifications", ikashopGetNotifications, METH_VARARGS },
#endif

		{ "IsFoundShopFromSearchItem", ikashopIsFoundShopFromSearchItem, METH_VARARGS },
		{ "ClearFoundShopMap", ikashopClearFoundShopMap, METH_VARARGS },
		{ "SendRequestEdit", ikashopSendRequestEdit, METH_VARARGS },

		{ NULL, NULL, NULL },
	};

	PyObject * poModule = Py_InitModule("ikashop", s_methods);
	PyModule_AddIntConstant(poModule, "FILTER_ATTRIBUTE_NUM", ITEM_ATTRIBUTE_SLOT_NORM_NUM);
	PyModule_AddIntConstant(poModule, "FILTER_NAME_MAX_LEN", ITEM_NAME_MAX_LEN);

	PyModule_AddIntConstant(poModule, "SHOP_GRID_WIDTH", SHOP_PLAYER_WIDTH);
	PyModule_AddIntConstant(poModule, "SHOP_GRID_HEIGHT", SHOP_PLAYER_HEIGHT);
	PyModule_AddIntConstant(poModule, "SAFEBOX_GRID_WIDTH", OFFLINESHOP_SAFEBOX_WIDTH);
	PyModule_AddIntConstant(poModule, "SAFEBOX_GRID_HEIGHT", OFFLINESHOP_SAFEBOX_HEIGHT);
	PyModule_AddIntConstant(poModule, "SAFEBOX_PAGE_COUNT", 1);
#ifdef ENABLE_CHEQUE_SYSTEM
	PyModule_AddIntConstant(poModule, "YANG_PER_CHEQUE", YANG_PER_CHEQUE);
#endif
#ifdef EXTEND_IKASHOP_PRO
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_SELLER_SOLD_ITEM", static_cast<int>(ikashop::ENotificationType::SELLER_SOLD_ITEM));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_NEW_OFFER_ON_ITEM", static_cast<int>(ikashop::ENotificationType::SELLER_NEW_OFFER_ON_ITEM));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_SHOP_EXPIRED", static_cast<int>(ikashop::ENotificationType::SELLER_SHOP_EXPIRED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_ITEM_EXPIRED", static_cast<int>(ikashop::ENotificationType::SELLER_ITEM_EXPIRED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_AUCTION_NEW_OFFER", static_cast<int>(ikashop::ENotificationType::SELLER_AUCTION_NEW_OFFER));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_AUCTION_EXPIRED", static_cast<int>(ikashop::ENotificationType::SELLER_AUCTION_EXPIRED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_AUCTION_SOLD", static_cast<int>(ikashop::ENotificationType::SELLER_AUCTION_SOLD));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_SELLER_OFFER_CANCELLED", static_cast<int>(ikashop::ENotificationType::SELLER_OFFER_CANCELLED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_BUYER_OFFER_DENIED", static_cast<int>(ikashop::ENotificationType::BUYER_OFFER_DENIED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_BUYER_OFFER_ACCEPTED", static_cast<int>(ikashop::ENotificationType::BUYER_OFFER_ACCEPTED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_BUYER_OFFER_EXPIRED", static_cast<int>(ikashop::ENotificationType::BUYER_OFFER_EXPIRED));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_BUYER_AUCTION_BUY", static_cast<int>(ikashop::ENotificationType::BUYER_AUCTION_BUY));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_BUYER_AUCTION_RAISE", static_cast<int>(ikashop::ENotificationType::BUYER_AUCTION_RAISE));
	PyModule_AddIntConstant(poModule, "NOTIFICATION_BUYER_AUCTION_LOST", static_cast<int>(ikashop::ENotificationType::BUYER_AUCTION_LOST));
#endif

	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_BOOKS", static_cast<int>(SHOP_SEARCH_CATEGORY_BOOKS));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_REFINE", static_cast<int>(SHOP_SEARCH_CATEGORY_REFINE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_SOULSTONE", static_cast<int>(SHOP_SEARCH_CATEGORY_SOULSTONE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_HERBALISM", static_cast<int>(SHOP_SEARCH_CATEGORY_HERBALISM));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_FISHING", static_cast<int>(SHOP_SEARCH_CATEGORY_FISHING));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_HORSE", static_cast<int>(SHOP_SEARCH_CATEGORY_HORSE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_SPECIAL", static_cast<int>(SHOP_SEARCH_CATEGORY_SPECIAL));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_MINING", static_cast<int>(SHOP_SEARCH_CATEGORY_MINING));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_POLYMORPH", static_cast<int>(SHOP_SEARCH_CATEGORY_POLYMORPH));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_ARMOR", static_cast<int>(SHOP_SEARCH_CATEGORY_ARMOR));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_ARMOR_ATTR", static_cast<int>(SHOP_SEARCH_CATEGORY_ARMOR_ATTR));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_WEAPON", static_cast<int>(SHOP_SEARCH_CATEGORY_WEAPON));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_WEAPON_ATTR", static_cast<int>(SHOP_SEARCH_CATEGORY_WEAPON_ATTR));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_JEWELRY", static_cast<int>(SHOP_SEARCH_CATEGORY_JEWELRY));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_CATEGORY_JEWELRY_ATTR", static_cast<int>(SHOP_SEARCH_CATEGORY_JEWELRY_ATTR));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WARRIOR_0", static_cast<int>(SHOP_SEARCH_SUB_WARRIOR_0));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WARRIOR_1", static_cast<int>(SHOP_SEARCH_SUB_WARRIOR_1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_ASSASSIN_0", static_cast<int>(SHOP_SEARCH_SUB_ASSASSIN_0));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_ASSASSIN_1", static_cast<int>(SHOP_SEARCH_SUB_ASSASSIN_1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SURA_0", static_cast<int>(SHOP_SEARCH_SUB_SURA_0));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SURA_1", static_cast<int>(SHOP_SEARCH_SUB_SURA_1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SHAMAN_0", static_cast<int>(SHOP_SEARCH_SUB_SHAMAN_0));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SHAMAN_1", static_cast<int>(SHOP_SEARCH_SUB_SHAMAN_1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_PASSIVE_SKILL", static_cast<int>(SHOP_SEARCH_SUB_PASSIVE_SKILL));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_M1", static_cast<int>(SHOP_SEARCH_SUB_REFINE_M1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_OATH", static_cast<int>(SHOP_SEARCH_SUB_REFINE_OATH));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_M2", static_cast<int>(SHOP_SEARCH_SUB_REFINE_M2));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_ORC", static_cast<int>(SHOP_SEARCH_SUB_REFINE_ORC));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_DESERT1", static_cast<int>(SHOP_SEARCH_SUB_REFINE_DESERT1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_DESERT2", static_cast<int>(SHOP_SEARCH_SUB_REFINE_DESERT2));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_SNOW", static_cast<int>(SHOP_SEARCH_SUB_REFINE_SNOW));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_HWANG", static_cast<int>(SHOP_SEARCH_SUB_REFINE_HWANG));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_END", static_cast<int>(SHOP_SEARCH_SUB_REFINE_END));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_SPECIAL", static_cast<int>(SHOP_SEARCH_SUB_REFINE_SPECIAL));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_REFINE_PEARL", static_cast<int>(SHOP_SEARCH_SUB_REFINE_PEARL));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SOULSTONE_0", static_cast<int>(SHOP_SEARCH_SUB_SOULSTONE_0));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SOULSTONE_1", static_cast<int>(SHOP_SEARCH_SUB_SOULSTONE_1));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SOULSTONE_2", static_cast<int>(SHOP_SEARCH_SUB_SOULSTONE_2));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SOULSTONE_3", static_cast<int>(SHOP_SEARCH_SUB_SOULSTONE_3));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SOULSTONE_4", static_cast<int>(SHOP_SEARCH_SUB_SOULSTONE_4));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_PRIMARY", static_cast<int>(SHOP_SEARCH_SUB_HERB_PRIMARY));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_SPECIAL", static_cast<int>(SHOP_SEARCH_SUB_HERB_SPECIAL));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_WATER_OFFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_WATER_OFFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_WATER_DEFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_WATER_DEFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_WATER_POWER", static_cast<int>(SHOP_SEARCH_SUB_HERB_WATER_POWER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_JUICE_OFFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_JUICE_OFFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_JUICE_DEFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_JUICE_DEFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_JUICE_POWER", static_cast<int>(SHOP_SEARCH_SUB_HERB_JUICE_POWER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_DEW_OFFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_DEW_OFFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_DEW_DEFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_DEW_DEFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_DEW_POWER", static_cast<int>(SHOP_SEARCH_SUB_HERB_DEW_POWER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_OTHER_POTION", static_cast<int>(SHOP_SEARCH_SUB_HERB_OTHER_POTION));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_AUTOPOTION", static_cast<int>(SHOP_SEARCH_SUB_HERB_AUTOPOTION));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_RECIPE_OFFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_RECIPE_OFFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_RECIPE_DEFENSIVE", static_cast<int>(SHOP_SEARCH_SUB_HERB_RECIPE_DEFENSIVE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_RECIPE_POWER", static_cast<int>(SHOP_SEARCH_SUB_HERB_RECIPE_POWER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HERB_RECIPE_OTHER", static_cast<int>(SHOP_SEARCH_SUB_HERB_RECIPE_OTHER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_FISHING_FISH", static_cast<int>(SHOP_SEARCH_SUB_FISHING_FISH));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_FISHING_FISH_COOK", static_cast<int>(SHOP_SEARCH_SUB_FISHING_FISH_COOK));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_FISHING_FISH_OTHER", static_cast<int>(SHOP_SEARCH_SUB_FISHING_FISH_OTHER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_REFINE", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_REFINE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_TOITEM", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_TOITEM));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_CHARACTER", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_CHARACTER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_OTHER", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_OTHER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_DRAGON_VOUCHER", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_DRAGON_VOUCHER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_QUEST", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_QUEST));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_SPECIAL_LOOTBOX", static_cast<int>(SHOP_SEARCH_SUB_SPECIAL_LOOTBOX));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_MINING_ORE", static_cast<int>(SHOP_SEARCH_SUB_MINING_ORE));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_MINING_MELT", static_cast<int>(SHOP_SEARCH_SUB_MINING_MELT));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_ARMOR_BODY", static_cast<int>(SHOP_SEARCH_SUB_ARMOR_BODY));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_ARMOR_SHIELD", static_cast<int>(SHOP_SEARCH_SUB_ARMOR_SHIELD));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_ARMOR_HEAD", static_cast<int>(SHOP_SEARCH_SUB_ARMOR_HEAD));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WEAPON_ONEHAND", static_cast<int>(SHOP_SEARCH_SUB_WEAPON_ONEHAND));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WEAPON_TWOHAND", static_cast<int>(SHOP_SEARCH_SUB_WEAPON_TWOHAND));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WEAPON_DAGGER", static_cast<int>(SHOP_SEARCH_SUB_WEAPON_DAGGER));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WEAPON_BOW", static_cast<int>(SHOP_SEARCH_SUB_WEAPON_BOW));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WEAPON_BELL", static_cast<int>(SHOP_SEARCH_SUB_WEAPON_BELL));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_WEAPON_FAN", static_cast<int>(SHOP_SEARCH_SUB_WEAPON_FAN));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_JEWELRY_EAR", static_cast<int>(SHOP_SEARCH_SUB_JEWELRY_EAR));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_JEWELRY_NECK", static_cast<int>(SHOP_SEARCH_SUB_JEWELRY_NECK));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_JEWELRY_WRIST", static_cast<int>(SHOP_SEARCH_SUB_JEWELRY_WRIST));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_JEWELRY_BOOTS", static_cast<int>(SHOP_SEARCH_SUB_JEWELRY_BOOTS));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HORSE_LEARN", static_cast<int>(SHOP_SEARCH_SUB_HORSE_LEARN));
	PyModule_AddIntConstant(poModule, "SHOP_SEARCH_SUB_HORSE_OTHER", static_cast<int>(SHOP_SEARCH_SUB_HORSE_OTHER));
	PyModule_AddIntConstant(poModule, "SHOP_CATEGORY_MAX_SUB", static_cast<int>(SHOP_CATEGORY_MAX_SUB));
}
#endif



// Files shared by GameCore.top
