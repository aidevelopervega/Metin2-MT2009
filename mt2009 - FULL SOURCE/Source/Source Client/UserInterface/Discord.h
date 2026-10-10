#include "StdAfx.h"
#include "PythonCharacterManager.h"
#include "PythonBackground.h"
#include "PythonPlayer.h"
#include "PythonGuild.h"
#include "fmt/fmt.h"

namespace Discord
{
	inline std::string key_chain = "UNKNOWN";
	inline std::string username = "UNKNOWN";

	constexpr auto DiscordClientID = "1180989036949680258";

	using DCDATA = std::pair<std::string, std::string>;

	inline void ReplaceStringInPlace(std::string& subject, const std::string& search,
		const std::string& replace) {
		size_t pos = 0;
		while ((pos = subject.find(search, pos)) != std::string::npos) {
			subject.replace(pos, search.length(), replace);
			pos += replace.length();
		}
	}
	inline void capitalizeWord(std::string& str)
	{
		bool canCapitalize = true;
		for (auto& c : str)
		{
			if (isalpha(c))
			{
				if (canCapitalize)
				{
					c = std::toupper(c);
					canCapitalize = false;
				}
			}
			else
				canCapitalize = true;
		}
	}

	/*NAME*/
	inline DCDATA GetNameData()
	{
		/*Map Name*/
		auto WarpName = std::string(CPythonBackground::Instance().GetWarpMapName());

		//atlasinfo.txt
		static const std::map<std::string, std::string> DCmapname{
			{ "metin2_map_a1", "Yongan (M1)" },
			{ "metin2_map_a3", "Jayang (M2)" },
			{ "metin2_map_guild_01", "Jungrang (M3)" },
			{ "metin2_map_b1", "Joan (M1)" },
			{ "metin2_map_b3", "Bokjung (M2)" },
			{ "metin2_map_guild_02", "Waryong (M3)" },
			{ "metin2_map_c1", "Pyungmoo (M1)" },
			{ "metin2_map_c3", "Bakra (M2)" },
			{ "metin2_map_guild_03", "Imha (M3)" },
			{ "map_a2", "Dolina Seungryong" },
			{ "map_n_snowm_01", "Góra Sohan" },
			{ "metin2_map_milgyo", "Œwi¹tynia Hwang" },
			{ "metin2_map_n_desert_01", "Pustynia Yongbi" },
			{ "metin2_map_n_flame_01", "Doyyumhwaji" },
			{ "metin2_map_trent", "Lungsam" },
			{ "metin2_map_trent02", "Czerwony Las" },
			{ "metin2_map_nusluck01", "Kraina Gigantów" },
			{ "metin2_map_deviltower1", "Wie¿a Demonów" },
			{ "metin2_map_devilscatacomb", "Diabelskie Katakumby" },
			{ "metin2_map_wl_pass", "G³ucha Prze³êcz" },
			{ "metin2_map_duel", "Arena PvP" },
			{ "metin2_map_oxevent", "Arena OX" },
			{ "metin2_map_wedding_01", "Obszar Œlubny" },
			{ "metin2_map_t3", "Wojna Gildii (Arena)" },
			{ "metin2_map_t4", "Wojna Gildii (Flagi)" },
			{ "metin2_map_spiderdungeon", "Kuahlo Dong (V1)" },
			{ "metin2_map_spiderdungeon_02", "Loch Paj¹ków V2" },
			{ "metin2_map_monkeydungeon", "Hasun Dong (£atwy Loch Ma³p)" },
			{ "metin2_map_monkeydungeon_02", "Jungsun Dong (Normalny Loch Ma³p)" },
			{ "metin2_map_monkeydungeon_03", "Sangsun Dong (Trudny Loch Ma³p)" },
			{ "metin2_map_empirewar01", "Zamek Milaard" },
			{ "metin2_map_empirewar02", "Zamek Listhmos" },
			{ "metin2_map_empirewar03", "Zamek Dendera" },
		};

		auto MapName = "Lokacja: " + (DCmapname.count(WarpName) ? DCmapname.at(WarpName) : "Nieznana");

		std::string level = "";
		auto pInstance = CPythonCharacterManager::Instance().GetMainInstancePtr();
		if (pInstance)
			level = std::to_string(pInstance->GetLevel());

		/*CH Name*/
		std::string CHName = "Nazwa:";
		if (level != "")
		{
			CHName += " Lv. " + std::string(level);
		}

		CHName += " " + std::string(CPythonPlayer::Instance().GetName());

		std::string GuildName;
		if (CPythonGuild::Instance().GetGuildName(CPythonPlayer::Instance().GetGuildID(), &GuildName))
			CHName += ", Gildia: " + GuildName;

		return { MapName, CHName };
	}

	/*RACE*/
	inline DCDATA GetRaceData()
	{
		auto pInstance = CPythonCharacterManager::Instance().GetMainInstancePtr();
		if (!pInstance)
			return { "","" };

		auto RACENUM = pInstance->GetRace();

		/*Image*/
		auto RaceImage = "race_" + std::to_string(RACENUM);

		/*Name*/
		auto RaceName = "Wojownik";
		switch (RACENUM)
		{
		case NRaceData::JOB_ASSASSIN:
		case NRaceData::JOB_ASSASSIN + 4:
			RaceName = "Ninja";
			break;
		case NRaceData::JOB_SURA:
		case NRaceData::JOB_SURA + 4:
			RaceName = "Sura";
			break;
		case NRaceData::JOB_SHAMAN:
		case NRaceData::JOB_SHAMAN + 4:
			RaceName = "Szaman";
			break;
#if defined(ENABLE_WOLFMAN_CHARACTER)
		case NRaceData::JOB_WOLFMAN:
			RaceName = "Lykan";
#endif
		}
		return { RaceImage , RaceName };
	}

	/*EMPIRE*/
	inline DCDATA GetEmpireData()
	{
		auto pInstance = CPythonCharacterManager::Instance().GetMainInstancePtr();
		if (!pInstance)
			return { "","" };

		auto EmpireID = pInstance->GetEmpireID();

		/*Image*/
		auto EmpireImage = "empire_" + std::to_string(EmpireID);

		/*Name*/
		auto EmpireName = "Shinsoo";
		switch (EmpireID)
		{
		case 2:
			EmpireName = "Chunjo";
			break;
		case 3:
			EmpireName = "Jinno";
		}
		return { EmpireImage, EmpireName };
	}
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
