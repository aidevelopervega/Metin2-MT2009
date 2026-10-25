#include "stdafx.h"

#include "config.h"
#include "char.h"
#include "char_ai.h"

#include "boss_elder_tree.h"
#include "boss_giant.h"
#include "common_ai.h"

void AIOnSpawn(LPCHARACTER ch)
{
	if (ch == NULL)
		return;

	if (ch->GetRaceNum() == static_cast<DWORD>(ELDER_TREE_BOSS_VNUM)) // red forest worldboss
		BossElderTree_Spawn(ch);
}

void AIOnDead(LPCHARACTER ch, [[maybe_unused]] LPCHARACTER killer)
{
	if (ch == NULL)
		return;

	// killer can be null

	if (ch->GetRaceNum() == static_cast<DWORD>(ELDER_TREE_BOSS_VNUM)) // red forest worldboss
		BossElderTree_Dead(ch);
}

void AIOnStartBattle(LPCHARACTER ch, LPCHARACTER victim)
{
	if (ch == NULL || victim == NULL)
		return;



	DWORD raceNum = ch->GetRaceNum();
	if (raceNum == 1491) // boos giant
		BossGiant_OnStartBattle(ch, victim);
}

int AIOnDamage([[maybe_unused]] LPCHARACTER attacker, LPCHARACTER victim, int damage, BYTE damageType)
{
	if (victim == NULL)
		return damage;

	DWORD raceNum = victim->GetRaceNum();

	if (raceNum == 2306) // red forest boss
		CommonAI_HealOnDamage(victim, damage, damageType);

	if (raceNum == 1491) // boos giant
		BossGiant_OnDamage(victim, damage, damageType);

	return damage;
}

