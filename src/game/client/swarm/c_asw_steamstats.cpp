#include "cbase.h"
#include "c_asw_steamstats.h"
#include "c_asw_debrief_stats.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "fmtstr.h"
#include "asw_equipment_list.h"
#include "string_t.h"
#include "asw_util_shared.h"
#include "asw_marine_profile.h"
#include <vgui/ILocalize.h>
#include "asw_shareddefs.h"
#include "c_asw_marine_resource.h"
#include "c_asw_campaign_save.h"
#include "asw_deathmatch_mode.h"
#include "rd_workshop.h"
#include "rd_lobby_utils.h"
#include "clientmode_asw.h"
#include "missioncompleteframe.h"
#include "missioncompletepanel.h"

CASW_Steamstats g_ASW_Steamstats;

ConVar asw_stats_leaderboard_debug( "asw_stats_leaderboard_debug", "0", FCVAR_NONE );

namespace 
{
#define FETCH_STEAM_STATS( apiname, membername ) \
	{ const char * szApiName = apiname; \
	if( !steamapicontext->SteamUserStats()->GetUserStat( playerSteamID, szApiName, &membername ) ) \
	{ \
		bOK = false; \
		Msg( "STEAMSTATS: Failed to retrieve stat %s.\n", szApiName ); \
} }

#define SEND_STEAM_STATS( apiname, membername ) \
	{ const char * szApiName = apiname; \
	steamapicontext->SteamUserStats()->SetStat( szApiName, membername ); }

	// Define some strings used by the stats API
	const char* szGamesTotal = ".games.total";
	const char* szGamesSuccess = ".games.success";
	const char* szGamesSuccessPercent = ".games.success.percent";
	const char* szKillsTotal = ".kills.total";
	const char* szDamageTotal = ".damage.total";
	const char* szFFTotal = ".ff.total";
	const char* szAccuracy = ".accuracy.avg";
	const char* szShotsTotal = ".shotsfired.total";
	const char* szShotsHit = ".shotshit.total";
	const char* szHealingTotal = ".healing.total";
	const char* szTimeTotal = ".time.total";
	const char* szTimeSuccess = ".time.success";
	const char* szKillsAvg = ".kills.avg";
	const char* szDamageAvg = ".damage.avg";
	const char* szFFAvg = ".ff.avg";
	const char* szTimeAvg = ".time.avg";
	const char* szBestDifficulty = ".difficulty.best";
	const char* szBestTime = ".time.best";
	const char* szBestSpeedrunDifficulty = ".time.best.difficulty";

	// difficulty names used when fetching steam stats
	const char* g_szDifficulties[] =
	{
		"Easy",
		"Normal",
		"Hard",
		"Insane",
		"imba"
	};

	const char *g_OfficialCampaigns[] =
	{
		"deathmatch_campaign",
		"jacob",
		"rd-operationcleansweep",
		"rd_research7",
		"rd-area9800",
		"rd-tarnorcampaign1",
		"rd_deadcity",
		"tilarus5",
		"rd_lanasescape_campaign",
		"rd_reduction_campaign",
		"rd_paranoia",
		"rd_bonus_missions",
	};
	const char *g_OfficialMaps[] =
	{
		"dm_desert",
		"dm_residential",
		"dm_testlab",
		"asi-jac1-landingbay_01",
		"asi-jac1-landingbay_02",
		"asi-jac2-deima",
		"asi-jac3-rydberg",
		"asi-jac4-residential",
		"asi-jac6-sewerjunction",
		"asi-jac7-timorstation",
		"rd-ocs1storagefacility",
		"rd-ocs2landingbay7",
		"rd-ocs3uscmedusa",
		"rd-res1forestentrance",
		"rd-res2research7",
		"rd-res3miningcamp",
		"rd-res4mines",
		"rd-area9800lz",
		"rd-area9800pp1",
		"rd-area9800pp2",
		"rd-area9800wl",
		"rd-tft1desertoutpost",
		"rd-tft2abandonedmaintenance",
		"rd-tft3spaceport",
		"rd-dc1_omega_city",
		"rd-dc2_breaking_an_entry",
		"rd-dc3_search_and_rescue",
		"rd-til1midnightport",
		"rd-til2roadtodawn",
		"rd-til3arcticinfiltration",
		"rd-til4area9800",
		"rd-til5coldcatwalks",
		"rd-til6yanaurusmine",
		"rd-til7factory",
		"rd-til8comcenter",
		"rd-til9syntekhospital",
		"rd-lan1_bridge",
		"rd-lan2_sewer",
		"rd-lan3_maintenance",
		"rd-lan4_vent",
		"rd-lan5_complex",
		"rd-reduction1",
		"rd-reduction2",
		"rd-reduction3",
		"rd-reduction4",
		"rd-reduction5",
		"rd-reduction6",
		"rd-par1unexpected_encounter",
		"rd-par2hostile_places",
		"rd-par3close_contact",
		"rd-par4high_tension",
		"rd-par5crucial_point",
		"rd-bonus_mission1",
		"rd-bonus_mission2",
		"rd-bonus_mission3",
		"rd-bonus_mission4",
		"rd-bonus_mission5",
	};
}

bool IsOfficialCampaign()
{
	if ( !ASWGameRules()->IsCampaignGame() )
		return false;

	CASW_Campaign_Save *pCampaign = ASWGameRules()->GetCampaignSave();

	const char *szMapName = engine->GetLevelNameShort();
	const char *szCampaignName = pCampaign->GetCampaignName();

	bool bCampaignMatches = false;
	for ( int i = 0; i < ARRAYSIZE( g_OfficialCampaigns ); i++ )
	{
		if ( FStrEq( szCampaignName, g_OfficialCampaigns[i] ) )
		{
			bCampaignMatches = true;
			break;
		}
	}

	if ( !bCampaignMatches )
	{
		return false;
	}

	for ( int i = 0; i < ARRAYSIZE( g_OfficialMaps ); i++ )
	{
		if ( FStrEq( szMapName, g_OfficialMaps[i] ) )
		{
				return true;
		}
	}

	return false;
}

bool IsWorkshopCampaign()
{
	if ( !ASWGameRules()->IsCampaignGame() )
		return false;

	CASW_Campaign_Save *pCampaign = ASWGameRules()->GetCampaignSave();

	const char *szMapName = engine->GetLevelNameShort();
	const char *szCampaignName = pCampaign->GetCampaignName();

	if ( g_ReactiveDropWorkshop.FindAddonProvidingFile( CFmtStr( "resource/campaigns/%s.txt", szCampaignName ) ) == k_PublishedFileIdInvalid )
	{
		return false;
	}

	return g_ReactiveDropWorkshop.FindAddonProvidingFile( CFmtStr( "resource/overviews/%s.txt", szMapName ) ) != k_PublishedFileIdInvalid;
}

bool IsDamagingWeapon( const char* szWeaponName, bool bIsExtraEquip )
{
	if( bIsExtraEquip )
	{
		// Check for the few damaging weapons
		if( !Q_strcmp( szWeaponName, "asw_weapon_laser_mines" ) || 
			!Q_strcmp( szWeaponName, "asw_weapon_hornet_barrage" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_mines" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_grenades" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_smart_bomb" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_tesla_trap" ) )
			return true;
		else
			return false;
	}
	else
	{
		// Check for the few non-damaging weapons
		if( !Q_strcmp( szWeaponName, "asw_weapon_heal_grenade" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_ammo_satchel" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_heal_gun" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_healamp_gun" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_fire_extinguisher" ) ||
			!Q_strcmp( szWeaponName, "asw_weapon_t75" ) )
			return false;
		else
			return true;
	}
}

Class_T GetDamagingWeaponClassFromName( const char *szClassName )
{
	if( FStrEq( szClassName, "asw_weapon_rifle") )
		return (Class_T)CLASS_ASW_RIFLE;
	else if( FStrEq( szClassName, "asw_weapon_prifle") )
		return (Class_T)CLASS_ASW_PRIFLE;
	else if( FStrEq( szClassName, "asw_weapon_autogun") )
		return (Class_T)CLASS_ASW_AUTOGUN;
	else if( FStrEq( szClassName, "asw_weapon_vindicator") )
		return (Class_T)CLASS_ASW_ASSAULT_SHOTGUN;
	else if( FStrEq( szClassName, "asw_weapon_pistol") )
		return (Class_T)CLASS_ASW_PISTOL;
	else if( FStrEq( szClassName, "asw_weapon_sentry") )
		return (Class_T)CLASS_ASW_SENTRY_GUN;
	else if( FStrEq( szClassName, "asw_weapon_shotgun") )
		return (Class_T)CLASS_ASW_SHOTGUN;
	else if( FStrEq( szClassName, "asw_weapon_tesla_gun") )
		return (Class_T)CLASS_ASW_TESLA_GUN;
	else if( FStrEq( szClassName, "asw_weapon_railgun") )
		return (Class_T)CLASS_ASW_RAILGUN;
	else if( FStrEq( szClassName, "asw_weapon_pdw") )
		return (Class_T)CLASS_ASW_PDW;
	else if( FStrEq( szClassName, "asw_weapon_flamer") )
		return (Class_T)CLASS_ASW_FLAMER;
	else if( FStrEq( szClassName, "asw_weapon_sentry_freeze") )
		return (Class_T)CLASS_ASW_SENTRY_FREEZE;
	else if( FStrEq( szClassName, "asw_weapon_minigun") )
		return (Class_T)CLASS_ASW_MINIGUN;
	else if( FStrEq( szClassName, "asw_weapon_sniper_rifle") )
		return (Class_T)CLASS_ASW_SNIPER_RIFLE;
	else if( FStrEq( szClassName, "asw_weapon_sentry_flamer") )
		return (Class_T)CLASS_ASW_SENTRY_FLAMER;
	else if( FStrEq( szClassName, "asw_weapon_chainsaw") )
		return (Class_T)CLASS_ASW_CHAINSAW;
	else if( FStrEq( szClassName, "asw_weapon_sentry_cannon") )
		return (Class_T)CLASS_ASW_SENTRY_CANNON;
	else if( FStrEq( szClassName, "asw_weapon_grenade_launcher") )
		return (Class_T)CLASS_ASW_GRENADE_LAUNCHER;
	else if( FStrEq( szClassName, "asw_weapon_mining_laser") )
		return (Class_T)CLASS_ASW_MINING_LASER;
	else if( FStrEq( szClassName, "asw_weapon_combat_rifle") )
		return (Class_T)CLASS_ASW_COMBAT_RIFLE;
	else if( FStrEq( szClassName, "asw_weapon_deagle") )
		return (Class_T)CLASS_ASW_DEAGLE;
	else if( FStrEq( szClassName, "asw_weapon_devastator") )
		return (Class_T)CLASS_ASW_DEVASTATOR;
	else if( FStrEq( szClassName, "asw_weapon_healamp_gun") )
		return (Class_T)CLASS_ASW_HEALAMP_GUN;
	else if( FStrEq( szClassName, "asw_weapon_50calmg") )
		return (Class_T)CLASS_ASW_50CALMG;

	else if( FStrEq( szClassName, "asw_weapon_laser_mines") )
		return (Class_T)CLASS_ASW_LASER_MINES;
	else if( FStrEq( szClassName, "asw_weapon_hornet_barrage") )
		return (Class_T)CLASS_ASW_HORNET_BARRAGE;
	else if( FStrEq( szClassName, "asw_weapon_mines") )
		return (Class_T)CLASS_ASW_MINES;
	else if( FStrEq( szClassName, "asw_weapon_grenades") )
		return (Class_T)CLASS_ASW_GRENADES;
	else if( FStrEq( szClassName, "asw_weapon_smart_bomb") )
		return (Class_T)CLASS_ASW_SMART_BOMB;
	else if( FStrEq( szClassName, "asw_weapon_t75") )
		return (Class_T)CLASS_ASW_T75;
	else if( FStrEq( szClassName, "asw_weapon_tesla_trap") )
		return (Class_T)CLASS_ASW_TESLA_TRAP;

	else if( FStrEq( szClassName, "asw_rifle_grenade") )
		return (Class_T)CLASS_ASW_RIFLE_GRENADE;
	else if( FStrEq( szClassName, "asw_prifle_grenade") )
		return (Class_T)CLASS_ASW_GRENADE_PRIFLE;
	else if( FStrEq( szClassName, "asw_vindicator_grenade") )
		return (Class_T)CLASS_ASW_GRENADE_VINDICATOR;
	else if( FStrEq( szClassName, "asw_combat_rifle_shotgun") )
		return (Class_T)CLASS_ASW_COMBAT_RIFLE_SHOTGUN;

	else
		return (Class_T)CLASS_ASW_UNKNOWN;
}
bool CASW_Steamstats::FetchStats( CSteamID playerSteamID, CASW_Player *pPlayer )
{
	bool bOK = true;

	m_PrimaryEquipCounts.Purge();
	m_SecondaryEquipCounts.Purge();
	m_ExtraEquipCounts.Purge();
	m_MarineSelectionCounts.Purge();
	m_DifficultyCounts.Purge();
	m_WeaponStats.Purge();

	// Returns true so we don't re-fetch stats
	if( !IsOfficialCampaign() && !IsWorkshopCampaign() )
		return true;

	// Fetch the player's overall stats
	FETCH_STEAM_STATS( "iTotalKills", m_iTotalKills );
	FETCH_STEAM_STATS( "fAccuracy", m_fAccuracy );
	FETCH_STEAM_STATS( "iFriendlyFire", m_iFriendlyFire );
	FETCH_STEAM_STATS( "iDamage", m_iDamage );
	FETCH_STEAM_STATS( "iShotsFired", m_iShotsFired );
	FETCH_STEAM_STATS( "iShotsHit", m_iShotsHit );
	FETCH_STEAM_STATS( "iAliensBurned", m_iAliensBurned );
	FETCH_STEAM_STATS( "iHealing", m_iHealing );
	FETCH_STEAM_STATS( "iFastHacks", m_iFastHacks );
	FETCH_STEAM_STATS( "iGamesTotal", m_iGamesTotal );
	FETCH_STEAM_STATS( "iGamesSuccess", m_iGamesSuccess );
	FETCH_STEAM_STATS( "fGamesSuccessPercent", m_fGamesSuccessPercent );
	FETCH_STEAM_STATS( "ammo_deployed", m_iAmmoDeployed );
	FETCH_STEAM_STATS( "sentryguns_deployed", m_iSentryGunsDeployed );
	FETCH_STEAM_STATS( "sentry_flamers_deployed", m_iSentryFlamerDeployed );
	FETCH_STEAM_STATS( "sentry_freeze_deployed", m_iSentryFreezeDeployed );
	FETCH_STEAM_STATS( "sentry_cannon_deployed", m_iSentryCannonDeployed );
	FETCH_STEAM_STATS( "medkits_used", m_iMedkitsUsed );
	FETCH_STEAM_STATS( "flares_used", m_iFlaresUsed );
	FETCH_STEAM_STATS( "adrenaline_used", m_iAdrenalineUsed );
	FETCH_STEAM_STATS( "tesla_traps_deployed", m_iTeslaTrapsDeployed );
	FETCH_STEAM_STATS( "freeze_grenades_thrown", m_iFreezeGrenadesThrown );
	FETCH_STEAM_STATS( "electric_armor_used", m_iElectricArmorUsed );
	FETCH_STEAM_STATS( "healgun_heals", m_iHealGunHeals );
	FETCH_STEAM_STATS( "healbeacon_heals", m_iHealBeaconHeals );
	FETCH_STEAM_STATS( "healgun_heals_self", m_iHealGunHeals_Self );
	FETCH_STEAM_STATS( "healbeacon_heals_self", m_iHealBeaconHeals_Self );
	FETCH_STEAM_STATS( "damage_amps_used", m_iDamageAmpsUsed );
	FETCH_STEAM_STATS( "healbeacons_deployed", m_iHealBeaconsDeployed );
	FETCH_STEAM_STATS( "medkit_heals_self", m_iMedkitHeals_Self );
	FETCH_STEAM_STATS( "grenade_extinguish_marines", m_iGrenadeExtinguishMarine );
	FETCH_STEAM_STATS( "grenade_freeze_aliens", m_iGrenadeFreezeAlien );
	FETCH_STEAM_STATS( "damage_amp_amps", m_iDamageAmpAmps );
	FETCH_STEAM_STATS( "normal_armor_reduction", m_iNormalArmorReduction );
	FETCH_STEAM_STATS( "electric_armor_reduction", m_iElectricArmorReduction );
	FETCH_STEAM_STATS( "healampgun_heals", m_iHealAmpGunHeals );
	FETCH_STEAM_STATS( "healampgun_amps", m_iHealAmpGunAmps );
	FETCH_STEAM_STATS( "playtime.total", m_iTotalPlayTime );

	// Fetch starting equip information
	int i=0;
	while( ASWEquipmentList()->GetRegular( i ) )
	{
		// Get weapon information
		if( IsDamagingWeapon( ASWEquipmentList()->GetRegular( i )->m_EquipClass, false ) )
		{
			int weaponIndex = m_WeaponStats.AddToTail();
			const char *szClassname = ASWEquipmentList()->GetRegular( i )->m_EquipClass;
			m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
			m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
			m_WeaponStats[weaponIndex].m_bIsExtra = false;
			m_WeaponStats[weaponIndex].m_szClassName = const_cast<char*>( szClassname );
		}
		
		// For primary equips
		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "equips.%s.primary", ASWEquipmentList()->GetRegular( i )->m_EquipClass ), iTempCount );
		m_PrimaryEquipCounts.AddToTail( iTempCount );

		// For secondary equips
		iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "equips.%s.secondary", ASWEquipmentList()->GetRegular( i++ )->m_EquipClass ), iTempCount );
		m_SecondaryEquipCounts.AddToTail( iTempCount );

	}

	i=0;
	while( ASWEquipmentList()->GetExtra( i ) )
	{
		// Get weapon information
		if( IsDamagingWeapon( ASWEquipmentList()->GetExtra( i )->m_EquipClass, true ) )
		{
			int weaponIndex = m_WeaponStats.AddToTail();
			const char *szClassname = ASWEquipmentList()->GetExtra( i )->m_EquipClass;
			m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
			m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
			m_WeaponStats[weaponIndex].m_bIsExtra = true;
			m_WeaponStats[weaponIndex].m_szClassName = const_cast<char*>( szClassname );
		}

		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "equips.%s.total", ASWEquipmentList()->GetExtra( i++ )->m_EquipClass ), iTempCount );
		m_ExtraEquipCounts.AddToTail( iTempCount );
	}

	// Get weapon stats for rifle grenade and vindicator grenade
	int weaponIndex = m_WeaponStats.AddToTail();
	char *szClassname = "asw_rifle_grenade";
	m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
	m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
	m_WeaponStats[weaponIndex].m_bIsExtra = false;
	m_WeaponStats[weaponIndex].m_szClassName = szClassname;

	weaponIndex = m_WeaponStats.AddToTail();
	szClassname = "asw_prifle_grenade";
	m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
	m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
	m_WeaponStats[weaponIndex].m_bIsExtra = false;
	m_WeaponStats[weaponIndex].m_szClassName = szClassname;

	weaponIndex = m_WeaponStats.AddToTail();
	szClassname = "asw_vindicator_grenade";
	m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
	m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
	m_WeaponStats[weaponIndex].m_bIsExtra = false;
	m_WeaponStats[weaponIndex].m_szClassName = szClassname;

	weaponIndex = m_WeaponStats.AddToTail();
	szClassname = "asw_combat_rifle_shotgun";
	m_WeaponStats[weaponIndex].FetchWeaponStats( steamapicontext, playerSteamID, szClassname );
	m_WeaponStats[weaponIndex].m_iWeaponIndex = GetDamagingWeaponClassFromName( szClassname );
	m_WeaponStats[weaponIndex].m_bIsExtra = false;
	m_WeaponStats[weaponIndex].m_szClassName = szClassname;


	// Fetch marine counts
	i=0;
	while( MarineProfileList()->GetProfile( i ) )
	{
		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "marines.%i.total", i++ ), iTempCount );
		m_MarineSelectionCounts.AddToTail( iTempCount );
	}

	// Get difficulty counts
	for( int i=0; i < 5; ++i )
	{
		int32 iTempCount;
		FETCH_STEAM_STATS( CFmtStr( "%s.games.total", g_szDifficulties[ i ] ), iTempCount );
		m_DifficultyCounts.AddToTail( iTempCount );

		// Fetch all stats for that difficulty
		bOK &= m_DifficultyStats[i].FetchDifficultyStats( steamapicontext, playerSteamID, i + 1 );
	}

	// Get stats for this mission/difficulty/marine
	bOK &= m_MissionStats.FetchMissionStats( steamapicontext, playerSteamID );
	
	return bOK;
}

void CASW_Steamstats::PrepStatsForSend( CASW_Player *pPlayer )
{
	if( !steamapicontext )
		return;

	// Update stats from the briefing screen
	if( !GetDebriefStats() 
		|| !ASWGameResource() 
#ifndef DEBUG 
		|| ASWGameRules()->m_bCheated 
#endif
		)
		return;

	if ( !IsOfficialCampaign() && !IsWorkshopCampaign() )
		return;

	PrepStatsForSend_Leaderboard( pPlayer, !IsOfficialCampaign() );

	if( m_MarineSelectionCounts.Count() == 0 || 
		m_DifficultyCounts.Count() == 0 || 
		m_PrimaryEquipCounts.Count() == 0 || 
		m_SecondaryEquipCounts.Count() == 0 || 
		m_ExtraEquipCounts.Count() == 0 )
		return;

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
		return;
	
	int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
	if ( iMarineIndex == -1 )
		return;

	int iMarineProfileIndex = pMR->m_MarineProfileIndex;

	m_iTotalKills += GetDebriefStats()->GetKills( iMarineIndex );
	m_iFriendlyFire += GetDebriefStats()->GetFriendlyFire( iMarineIndex );
	m_iDamage += GetDebriefStats()->GetDamageTaken( iMarineIndex );
	m_iShotsFired += GetDebriefStats()->GetShotsFired( iMarineIndex );
	m_iShotsHit += GetDebriefStats()->GetShotsHit( iMarineIndex );
	m_fAccuracy = ( m_iShotsFired > 0 ) ? ( m_iShotsHit / (float)m_iShotsFired * 100.0f ) : 0;
	m_iAliensBurned += GetDebriefStats()->GetAliensBurned( iMarineIndex );
	m_iHealing += GetDebriefStats()->GetHealthHealed( iMarineIndex );
	m_iFastHacks += GetDebriefStats()->GetFastHacks( iMarineIndex );
	m_iGamesTotal++;
	if( m_DifficultyCounts.IsValidIndex( ASWGameRules()->GetSkillLevel() - 1 ) )
		m_DifficultyCounts[ ASWGameRules()->GetSkillLevel() - 1 ] += 1;
	m_iGamesSuccess += ASWGameRules()->GetMissionSuccess() ? 1 : 0;
	m_fGamesSuccessPercent = m_iGamesSuccess / m_iGamesTotal * 100.0f;
	if( m_MarineSelectionCounts.IsValidIndex( iMarineProfileIndex ) )
		m_MarineSelectionCounts[iMarineProfileIndex] += 1;
	m_iAmmoDeployed += GetDebriefStats()->GetAmmoDeployed( iMarineIndex );
	m_iSentryGunsDeployed += GetDebriefStats()->GetSentrygunsDeployed( iMarineIndex );
	m_iSentryFlamerDeployed += GetDebriefStats()->GetSentryFlamersDeployed( iMarineIndex );
	m_iSentryFreezeDeployed += GetDebriefStats()->GetSentryFreezeDeployed( iMarineIndex );
	m_iSentryCannonDeployed += GetDebriefStats()->GetSentryCannonDeployed( iMarineIndex );
	m_iMedkitsUsed += GetDebriefStats()->GetMedkitsUsed( iMarineIndex );
	m_iFlaresUsed += GetDebriefStats()->GetFlaresUsed( iMarineIndex );
	m_iAdrenalineUsed += GetDebriefStats()->GetAdrenalineUsed( iMarineIndex );
	m_iTeslaTrapsDeployed += GetDebriefStats()->GetTeslaTrapsDeployed( iMarineIndex );
	m_iFreezeGrenadesThrown += GetDebriefStats()->GetFreezeGrenadesThrown( iMarineIndex );
	m_iElectricArmorUsed += GetDebriefStats()->GetElectricArmorUsed( iMarineIndex );
	m_iHealGunHeals += GetDebriefStats()->GetHealGunHeals( iMarineIndex );
	m_iHealBeaconHeals += GetDebriefStats()->GetHealBeaconHeals( iMarineIndex );
	m_iHealGunHeals_Self += GetDebriefStats()->GetHealGunHeals_Self( iMarineIndex );
	m_iHealBeaconHeals_Self += GetDebriefStats()->GetHealBeaconHeals_Self( iMarineIndex );
	m_iDamageAmpsUsed += GetDebriefStats()->GetDamageAmpsUsed( iMarineIndex );
	m_iHealBeaconsDeployed += GetDebriefStats()->GetHealBeaconsDeployed( iMarineIndex );
	m_iMedkitHeals_Self += GetDebriefStats()->GetMedkitHeals_Self( iMarineIndex );
	m_iGrenadeExtinguishMarine += GetDebriefStats()->GetGrenadeExtinguishMarine( iMarineIndex );
	m_iGrenadeFreezeAlien += GetDebriefStats()->GetGrenadeFreezeAlien( iMarineIndex );
	m_iDamageAmpAmps += GetDebriefStats()->GetDamageAmpAmps( iMarineIndex );
	m_iNormalArmorReduction += GetDebriefStats()->GetNormalArmorReduction( iMarineIndex );
	m_iElectricArmorReduction += GetDebriefStats()->GetElectricArmorReduction( iMarineIndex );
	m_iHealAmpGunHeals += GetDebriefStats()->GetHealampgunHeals( iMarineIndex );
	m_iHealAmpGunAmps += GetDebriefStats()->GetHealampgunAmps( iMarineIndex );
	m_iTotalPlayTime += (int)GetDebriefStats()->m_fTimeTaken;

	// Get starting equips
	int iPrimaryIndex = GetDebriefStats()->GetStartingPrimaryEquip( iMarineIndex );
	int iSecondaryIndex = GetDebriefStats()->GetStartingSecondaryEquip( iMarineIndex );
	int iExtraIndex = GetDebriefStats()->GetStartingExtraEquip( iMarineIndex );
	CASW_EquipItem *pPrimary = ASWEquipmentList()->GetRegular( iPrimaryIndex );
	CASW_EquipItem *pSecondary = ASWEquipmentList()->GetRegular( iSecondaryIndex );
	CASW_EquipItem *pExtra = ASWEquipmentList()->GetExtra( iExtraIndex );

	Assert( pPrimary && pSecondary && pExtra );

	m_PrimaryEquipCounts[iPrimaryIndex]++;
	m_SecondaryEquipCounts[iSecondaryIndex]++;
	m_ExtraEquipCounts[iExtraIndex]++;

	m_DifficultyStats[ASWGameRules()->GetSkillLevel() - 1].PrepStatsForSend( pPlayer );
	m_MissionStats.PrepStatsForSend( pPlayer );

	// Send player's overall stats
	SEND_STEAM_STATS( "iTotalKills", m_iTotalKills );
	SEND_STEAM_STATS( "fAccuracy", m_fAccuracy );
	SEND_STEAM_STATS( "iFriendlyFire", m_iFriendlyFire );
	SEND_STEAM_STATS( "iDamage", m_iDamage );
	SEND_STEAM_STATS( "iShotsFired", m_iShotsFired );
	SEND_STEAM_STATS( "iShotsHit", m_iShotsHit );
	SEND_STEAM_STATS( "iAliensBurned", m_iAliensBurned );
	SEND_STEAM_STATS( "iHealing", m_iHealing );
	SEND_STEAM_STATS( "iFastHacks", m_iFastHacks );
	SEND_STEAM_STATS( "iGamesTotal", m_iGamesTotal );
	SEND_STEAM_STATS( "iGamesSuccess", m_iGamesSuccess );
	SEND_STEAM_STATS( "fGamesSuccessPercent", m_fGamesSuccessPercent );

	SEND_STEAM_STATS( "ammo_deployed", m_iAmmoDeployed );
	SEND_STEAM_STATS( "sentryguns_deployed", m_iSentryGunsDeployed );
	SEND_STEAM_STATS( "sentry_flamers_deployed", m_iSentryFlamerDeployed );
	SEND_STEAM_STATS( "sentry_freeze_deployed", m_iSentryFreezeDeployed );
	SEND_STEAM_STATS( "sentry_cannon_deployed", m_iSentryCannonDeployed );
	SEND_STEAM_STATS( "medkits_used", m_iMedkitsUsed );
	SEND_STEAM_STATS( "flares_used", m_iFlaresUsed );
	SEND_STEAM_STATS( "adrenaline_used", m_iAdrenalineUsed );
	SEND_STEAM_STATS( "tesla_traps_deployed", m_iTeslaTrapsDeployed );
	SEND_STEAM_STATS( "freeze_grenades_thrown", m_iFreezeGrenadesThrown );
	SEND_STEAM_STATS( "electric_armor_used", m_iElectricArmorUsed );
	SEND_STEAM_STATS( "healgun_heals", m_iHealGunHeals );
	SEND_STEAM_STATS( "healbeacon_heals", m_iHealBeaconHeals );
	SEND_STEAM_STATS( "healgun_heals_self", m_iHealGunHeals_Self );
	SEND_STEAM_STATS( "healbeacon_heals_self", m_iHealBeaconHeals_Self );
	SEND_STEAM_STATS( "damage_amps_used", m_iDamageAmpsUsed );
	SEND_STEAM_STATS( "healbeacons_deployed", m_iHealBeaconsDeployed );
	SEND_STEAM_STATS( "medkit_heals_self", m_iMedkitHeals_Self );
	SEND_STEAM_STATS( "grenade_extinguish_marines", m_iGrenadeExtinguishMarine );
	SEND_STEAM_STATS( "grenade_freeze_aliens", m_iGrenadeFreezeAlien );
	SEND_STEAM_STATS( "damage_amp_amps", m_iDamageAmpAmps );
	SEND_STEAM_STATS( "normal_armor_reduction", m_iNormalArmorReduction );
	SEND_STEAM_STATS( "electric_armor_reduction", m_iElectricArmorReduction );
	SEND_STEAM_STATS( "healampgun_heals", m_iHealAmpGunHeals );
	SEND_STEAM_STATS( "healampgun_amps", m_iHealAmpGunAmps );
	SEND_STEAM_STATS( "playtime.total", m_iTotalPlayTime );

	SEND_STEAM_STATS( CFmtStr( "equips.%s.primary", pPrimary->m_EquipClass), m_PrimaryEquipCounts[iPrimaryIndex] );
	SEND_STEAM_STATS( CFmtStr( "equips.%s.secondary", pSecondary->m_EquipClass), m_SecondaryEquipCounts[iSecondaryIndex] );
	SEND_STEAM_STATS( CFmtStr( "equips.%s.total", pExtra->m_EquipClass), m_ExtraEquipCounts[iExtraIndex] );
	SEND_STEAM_STATS( CFmtStr( "marines.%i.total", iMarineProfileIndex ), m_MarineSelectionCounts[iMarineProfileIndex] );
	int iLevel = pPlayer->GetLevel();
	SEND_STEAM_STATS( "level", iLevel );
	int iPromotion = pPlayer->GetPromotion();
	float flXPRequired = ( iLevel == NELEMS( g_iLevelExperience ) ) ? 0 : g_iLevelExperience[ iLevel ];
	flXPRequired *= g_flPromotionXPScale[ iPromotion ];
	SEND_STEAM_STATS( "level.xprequired", (int) flXPRequired );

	// Send favorite equip info
	SEND_STEAM_STATS( "equips.primary.fav", GetFavoriteEquip(0) );
	SEND_STEAM_STATS( "equips.secondary.fav", GetFavoriteEquip(1) );
	SEND_STEAM_STATS( "equips.extra.fav", GetFavoriteEquip(2) );
	SEND_STEAM_STATS( "equips.primary.fav.pct", GetFavoriteEquipPercent(0) );
	SEND_STEAM_STATS( "equips.secondary.fav.pct", GetFavoriteEquipPercent(1) );
	SEND_STEAM_STATS( "equips.extra.fav.pct", GetFavoriteEquipPercent(2) );

	// Send favorite marine info
	SEND_STEAM_STATS( "marines.fav", GetFavoriteMarine() );
	SEND_STEAM_STATS( "marines.class.fav", GetFavoriteMarineClass() );
	SEND_STEAM_STATS( "marines.fav.pct", GetFavoriteMarinePercent() );
	SEND_STEAM_STATS( "marines.class.fav.pct", GetFavoriteMarineClassPercent() );

	// Send favorite difficulty info
	SEND_STEAM_STATS( "difficulty.fav", GetFavoriteDifficulty() );
	SEND_STEAM_STATS( "difficulty.fav.pct", GetFavoriteDifficultyPercent() );

	// Send weapon stats
	for( int i=0; i<m_WeaponStats.Count(); ++i )
	{
		m_WeaponStats[i].PrepStatsForSend( pPlayer );
	}

}

int CASW_Steamstats::GetFavoriteEquip( int iSlot )
{
	Assert( iSlot < ASW_MAX_EQUIP_SLOTS );

	StatList_Int_t *pList = NULL;
	if( iSlot == 0 )
		pList = &m_PrimaryEquipCounts;
	else if( iSlot == 1 )
		pList = &m_SecondaryEquipCounts;
	else
		pList = &m_ExtraEquipCounts;

	int iFav = 0;
	for( int i=0; i<pList->Count(); ++i )
	{
		if( pList->operator []( iFav ) < pList->operator [](i) )
			iFav = i;
	}

	return iFav;
}

int CASW_Steamstats::GetFavoriteMarine( void )
{
	int iFav = 0;
	for( int i=0; i<m_MarineSelectionCounts.Count(); ++i )
	{
		if( m_MarineSelectionCounts[ iFav ] < m_MarineSelectionCounts[i] )
			iFav = i;
	}

	return iFav;
}

int CASW_Steamstats::GetFavoriteMarineClass( void )
{
	// Find the marine's class
	CASW_Marine_Profile *pProfile = MarineProfileList()->GetProfile( GetFavoriteMarine() );
	Assert( pProfile );
	return pProfile ? pProfile->GetMarineClass() : 0;
}

int CASW_Steamstats::GetFavoriteDifficulty( void )
{
	int iFav = 0;
	for( int i=0; i<m_DifficultyCounts.Count(); ++i )
	{
		if( m_DifficultyCounts[ iFav ] < m_DifficultyCounts[i] )
			iFav = i;
	}

	return iFav + 1;
}

float CASW_Steamstats::GetFavoriteEquipPercent( int iSlot )
{
	Assert( iSlot < ASW_MAX_EQUIP_SLOTS );

	StatList_Int_t *pList = NULL;
	if( iSlot == 0 )
		pList = &m_PrimaryEquipCounts;
	else if( iSlot == 1 )
		pList = &m_SecondaryEquipCounts;
	else
		pList = &m_ExtraEquipCounts;

	int iFav = 0;
	float fTotal = 0;
	for( int i=0; i<pList->Count(); ++i )
	{
		fTotal += pList->operator [](i);
		if( iFav < pList->operator [](i) )
			iFav = pList->operator [](i);
	}
	return ( fTotal > 0.0f ) ? ( iFav / fTotal * 100.0f ) : 0.0f;
}

float CASW_Steamstats::GetFavoriteMarinePercent( void )
{
	int iFav = 0;
	float fTotal = 0;
	for( int i=0; i<m_MarineSelectionCounts.Count(); ++i )
	{
		fTotal += m_MarineSelectionCounts[i];
		if( m_MarineSelectionCounts[iFav] < m_MarineSelectionCounts[i] )
			iFav = i;
	}

	return ( fTotal > 0.0f ) ? ( m_MarineSelectionCounts[iFav] / fTotal * 100.0f ) : 0.0f;
}

float CASW_Steamstats::GetFavoriteMarineClassPercent( void )
{
	int iFav = 0;
	float fTotal = 0;
	int iClassCounts[NUM_MARINE_CLASSES] = {0};
	for( int i=0; i<m_MarineSelectionCounts.Count(); ++i )
	{
		// Find the marine's class
		CASW_Marine_Profile *pProfile = MarineProfileList()->GetProfile( i );
		Assert( pProfile );
		if( !pProfile )
			continue;

		int iProfileClass = pProfile->GetMarineClass();
		iClassCounts[iProfileClass] += m_MarineSelectionCounts[i];
		fTotal += m_MarineSelectionCounts[i];
		if( iClassCounts[iFav] < iClassCounts[iProfileClass] )
			iFav = iProfileClass;
	}

	return ( fTotal > 0.0f ) ? ( iClassCounts[iFav] / fTotal * 100.0f ) : 0.0f;
}

float CASW_Steamstats::GetFavoriteDifficultyPercent( void )
{
	int iFav = 0;
	float fTotal = 0;
	for( int i=0; i<m_DifficultyCounts.Count(); ++i )
	{
		fTotal += m_DifficultyCounts[i];
		if( iFav < m_DifficultyCounts[i] )
			iFav = m_DifficultyCounts[i];
	}

	return ( fTotal > 0.0f ) ? ( iFav / fTotal * 100.0f ) : 0.0f;
}

bool DifficultyStats_t::FetchDifficultyStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID, int iDifficulty )
{
	if( !ASWGameRules() )
		return false;

	bool bOK = true;
	char* szDifficulty = NULL;

	switch( iDifficulty )
	{
		case 1: szDifficulty = "easy"; 
			break;
		case 2: szDifficulty = "normal";
			break;
		case 3: szDifficulty = "hard";
			break;
		case 4: szDifficulty = "insane";
			break;
		case 5: szDifficulty = "imba";
			break;
	}
	if( szDifficulty )
	{
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesTotal ), m_iGamesTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccess ), m_iGamesSuccess );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccessPercent ), m_fGamesSuccessPercent );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szKillsTotal ), m_iKillsTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szDamageTotal ), m_iDamageTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szFFTotal ), m_iFFTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szAccuracy ), m_fAccuracyAvg );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsHit ), m_iShotsHitTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsTotal ), m_iShotsFiredTotal );
		FETCH_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szHealingTotal ), m_iHealingTotal );
	}

	return bOK;
}

void DifficultyStats_t::PrepStatsForSend( CASW_Player *pPlayer )
{
	if( !steamapicontext )
		return;

	// Update stats from the briefing screen
	if( !GetDebriefStats() || !ASWGameResource() )
		return;

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( pMR )
	{
		int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
		if ( iMarineIndex != -1 )
		{
			m_iKillsTotal += GetDebriefStats()->GetKills( iMarineIndex );
			m_iFFTotal += GetDebriefStats()->GetFriendlyFire( iMarineIndex );
			m_iDamageTotal += GetDebriefStats()->GetDamageTaken( iMarineIndex );
			m_iShotsFiredTotal += GetDebriefStats()->GetShotsFired( iMarineIndex );
			m_iHealingTotal += GetDebriefStats()->GetHealthHealed( iMarineIndex );
			m_iShotsHitTotal += GetDebriefStats()->GetShotsHit( iMarineIndex );
			m_fAccuracyAvg = ( m_iShotsFiredTotal > 0 ) ? ( m_iShotsHitTotal / (float)m_iShotsFiredTotal * 100.0f ) : 0;
			m_iGamesTotal++;
			m_iGamesSuccess += ASWGameRules()->GetMissionSuccess() ? 1 : 0;
			m_fGamesSuccessPercent = m_iGamesSuccess / (float)m_iGamesTotal * 100.0f;
		}
	}
	char* szDifficulty = NULL;
	int iDifficulty = ASWGameRules()->GetSkillLevel();

	switch( iDifficulty )
	{
	case 1: szDifficulty = "easy"; 
		break;
	case 2: szDifficulty = "normal";
		break;
	case 3: szDifficulty = "hard";
		break;
	case 4: szDifficulty = "insane";
		break;
	case 5: szDifficulty = "imba";
		break;
	}
	if( szDifficulty )
	{
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesTotal ), m_iGamesTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccess ), m_iGamesSuccess );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szGamesSuccessPercent ), m_fGamesSuccessPercent );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szKillsTotal ), m_iKillsTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szDamageTotal ), m_iDamageTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szFFTotal ), m_iFFTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szAccuracy ), m_fAccuracyAvg );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsHit ), m_iShotsHitTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szShotsTotal ), m_iShotsFiredTotal );
		SEND_STEAM_STATS( CFmtStr( "%s%s", szDifficulty, szHealingTotal ), m_iHealingTotal );
	}
}

bool MissionStats_t::FetchMissionStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID )
{
	if ( !IsOfficialCampaign() )
		return true;

	bool bOK = true;
	const char* szLevelName = NULL;
	if( !engine )
		return false;

	szLevelName = engine->GetLevelNameShort();
	if( !szLevelName )
		return false;

	// deathmatch maps don't have per-mission stats
	if ( ASWDeathmatchMode() )
		return true;

	// Fetch stats. Skip the averages, they're write only
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesTotal ), m_iGamesTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccess ), m_iGamesSuccess );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccessPercent ), m_fGamesSuccessPercent );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szKillsTotal ), m_iKillsTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szDamageTotal ), m_iDamageTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szFFTotal ), m_iFFTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeTotal ), m_iTimeTotal );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeSuccess ), m_iTimeSuccess );
	FETCH_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szBestDifficulty ), m_iHighestDifficulty );
	for( int i=0; i<5; ++i )
	{
		FETCH_STEAM_STATS( CFmtStr( "%s%s.%s", szLevelName, szBestTime, g_szDifficulties[i] ), m_iBestSpeedrunTimes[i] );
	}

	return bOK;
}

void MissionStats_t::PrepStatsForSend( CASW_Player *pPlayer )
{
	// Update stats from the briefing screen
	if( !GetDebriefStats() || !ASWGameResource() )
		return;

	// Custom campaigns don't have manually created stats.
	if ( !IsOfficialCampaign() )
		return;

	// deathmatch maps don't have per-mission stats
	if ( ASWDeathmatchMode() )
		return;

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	int iDifficulty = ASWGameRules()->GetSkillLevel();
	if ( pMR )
	{
		int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
		if ( iMarineIndex != -1 )
		{
			m_iKillsTotal += GetDebriefStats()->GetKills( iMarineIndex );
			m_iFFTotal += GetDebriefStats()->GetFriendlyFire( iMarineIndex );
			m_iDamageTotal += GetDebriefStats()->GetDamageTaken( iMarineIndex );
			m_iGamesTotal++;
			m_iGamesSuccess += ASWGameRules()->GetMissionSuccess() ? 1 : 0;
			m_fGamesSuccessPercent = m_iGamesSuccess / (float)m_iGamesTotal * 100.0f;
			m_iTimeTotal += GetDebriefStats()->m_fTimeTaken;
			if( ASWGameRules()->GetMissionSuccess() )
			{
				m_iTimeSuccess += GetDebriefStats()->m_fTimeTaken;

				if( iDifficulty > m_iHighestDifficulty )
					m_iHighestDifficulty = iDifficulty;

				if( (unsigned int)m_iBestSpeedrunTimes[ iDifficulty - 1 ] > GetDebriefStats()->m_fTimeTaken )
				{
					m_iBestSpeedrunTimes[ iDifficulty - 1 ] = GetDebriefStats()->m_fTimeTaken;
				}
			}
			
			
			// Safely compute averages
			m_fKillsAvg = m_iKillsTotal / (float)m_iGamesTotal;
			m_fFFAvg = m_iFFTotal / (float)m_iGamesTotal;
			m_fDamageAvg = m_iDamageTotal / (float)m_iGamesTotal;
			m_iTimeAvg = m_iTimeTotal / (float)m_iGamesTotal;
		}
	}

	const char* szLevelName = NULL;
	if( !engine )
		return;

	szLevelName = engine->GetLevelNameShort();
	if( !szLevelName )
		return;

	// Send stats
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesTotal ), m_iGamesTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccess ), m_iGamesSuccess );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szGamesSuccessPercent ), m_fGamesSuccessPercent );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szKillsTotal ), m_iKillsTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szDamageTotal ), m_iDamageTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szFFTotal ), m_iFFTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeTotal ), m_iTimeTotal );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeSuccess ), m_iTimeSuccess );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szKillsAvg ), m_fKillsAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szDamageAvg ), m_fDamageAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szFFAvg ), m_fFFAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szTimeAvg ), m_iTimeAvg );
	SEND_STEAM_STATS( CFmtStr( "%s%s", szLevelName, szBestDifficulty ), m_iHighestDifficulty );
	SEND_STEAM_STATS( CFmtStr( "%s%s.%s", szLevelName, szBestTime, g_szDifficulties[ iDifficulty - 1 ] ), m_iBestSpeedrunTimes[ iDifficulty - 1 ] );
}

bool WeaponStats_t::FetchWeaponStats( CSteamAPIContext * pSteamContext, CSteamID playerSteamID, const char *szClassName )
{
	bool bOK = true;

	const char *szDeathmatchPrefix = "";
	if ( ASWDeathmatchMode() )
	{
		szDeathmatchPrefix = "dm.";
	}

	// Fetch stats. Skip the averages, they're write only
	FETCH_STEAM_STATS( CFmtStr( "%sequips.%s.damage", szDeathmatchPrefix, szClassName ), m_iDamage );
	FETCH_STEAM_STATS( CFmtStr( "%sequips.%s.ff_damage", szDeathmatchPrefix, szClassName ), m_iFFDamage );
	FETCH_STEAM_STATS( CFmtStr( "%sequips.%s.shots_fired", szDeathmatchPrefix, szClassName ), m_iShotsFired );
	FETCH_STEAM_STATS( CFmtStr( "%sequips.%s.shots_hit", szDeathmatchPrefix, szClassName ), m_iShotsHit );
	FETCH_STEAM_STATS( CFmtStr( "%sequips.%s.kills", szDeathmatchPrefix, szClassName ), m_iKills );

	return bOK;
}

void WeaponStats_t::PrepStatsForSend( CASW_Player *pPlayer )
{
	if( !GetDebriefStats() || !ASWGameResource() || !ASWEquipmentList() )
		return;

	// Check to see if the weapon is in the debrief stats
	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
		return;
	
	int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
	if ( iMarineIndex == -1 )
		return;

	
	if( !m_szClassName )
		return;
	int iDamage, iFFDamage, iShotsFired, iShotsHit, iKills = 0;

	if( GetDebriefStats()->GetWeaponStats( iMarineIndex, m_iWeaponIndex, iDamage, iFFDamage, iShotsFired, iShotsHit, iKills ) )
	{
		const char *szDeathmatchPrefix = "";
		if ( ASWDeathmatchMode() )
		{
			szDeathmatchPrefix = "dm.";
		}

		SEND_STEAM_STATS( CFmtStr( "%sequips.%s.damage", szDeathmatchPrefix, m_szClassName ), m_iDamage + iDamage );
		SEND_STEAM_STATS( CFmtStr( "%sequips.%s.ff_damage", szDeathmatchPrefix, m_szClassName ), m_iFFDamage + iFFDamage );
		SEND_STEAM_STATS( CFmtStr( "%sequips.%s.shots_fired", szDeathmatchPrefix, m_szClassName ), m_iShotsFired + iShotsFired );
		SEND_STEAM_STATS( CFmtStr( "%sequips.%s.shots_hit", szDeathmatchPrefix, m_szClassName ), m_iShotsHit + iShotsHit );
		SEND_STEAM_STATS( CFmtStr( "%sequips.%s.kills", szDeathmatchPrefix, m_szClassName ), m_iKills + iKills );
	}
}

bool CASW_Steamstats::IsOfficialCampaign()
{
	return ::IsOfficialCampaign();
}

void CASW_Steamstats::PrepStatsForSend_Leaderboard( CASW_Player *pPlayer, bool bUnofficial )
{
	if ( !steamapicontext || !steamapicontext->SteamUserStats() || ASWDeathmatchMode() || !ASWGameRules() || !ASWGameResource() || !GetDebriefStats() || engine->IsPlayingDemo() )
	{
		return;
	}

	if ( !ASWGameRules()->GetMissionSuccess() )
	{
		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Not sending leaderboard entry: Mission failed!\n" );
		}
		return;
	}

	CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
	{
		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Not sending leaderboard entry: No marine!\n" );
		}
		engine->ServerCmd( "cl_leaderboard_ready\n" );
		return;
	}

	int iMR = ASWGameResource()->GetMarineResourceIndex( pMR );

	char szMissionFileName[MAX_PATH];
	Q_snprintf( szMissionFileName, sizeof( szMissionFileName ), "resource/overviews/%s.txt", IGameSystem::MapName() );
	PublishedFileId_t nWorkshopFileID = g_ReactiveDropWorkshop.FindAddonProvidingFile( szMissionFileName );
	if ( nWorkshopFileID == k_PublishedFileIdInvalid && bUnofficial )
	{
		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Not sending leaderboard entry: Unofficial map %s but no workshop ID!\n", IGameSystem::MapName() );
		}
		engine->ServerCmd( "cl_leaderboard_ready\n" );
		return;
	}

	extern ConVar rd_challenge;
	char szChallengeFileName[MAX_PATH];
	Q_snprintf( szChallengeFileName, sizeof( szChallengeFileName ), "resource/challenges/%s.txt", rd_challenge.GetString() );
	PublishedFileId_t nChallengeFileID = g_ReactiveDropWorkshop.FindAddonProvidingFile( szChallengeFileName );
	if ( UTIL_RD_GetCurrentLobbyID().IsValid() )
	{
		const char *pszChallengeFileID = UTIL_RD_GetCurrentLobbyData( "game:challengeinfo:workshop", "" );
		if ( *pszChallengeFileID )
		{
			nChallengeFileID = std::strtoull( pszChallengeFileID, NULL, 16 );
		}
	}

	char szLeaderboardName[k_cchLeaderboardNameMax];
	SpeedRunLeaderboardName( szLeaderboardName, sizeof( szLeaderboardName ), IGameSystem::MapName(), nWorkshopFileID, rd_challenge.GetString(), nChallengeFileID );
	char szDifficultyLeaderboardName[k_cchLeaderboardNameMax];
	DifficultySpeedRunLeaderboardName( szDifficultyLeaderboardName, sizeof( szDifficultyLeaderboardName ), ASWGameRules()->GetSkillLevel(), IGameSystem::MapName(), nWorkshopFileID, rd_challenge.GetString(), nChallengeFileID );

	if ( asw_stats_leaderboard_debug.GetBool() )
	{
		DevMsg( "Preparing leaderboard entry for leaderboards %s and %s\n", szLeaderboardName, szDifficultyLeaderboardName );
	}

	m_iLeaderboardScore = GetDebriefStats()->m_fTimeTaken * 1000;
	m_LeaderboardScoreDetails.m_iVersion = 2;
	m_LeaderboardScoreDetails.m_iMarine = pMR->GetProfileIndex();
	m_LeaderboardScoreDetails.m_iSquadSize = ASWGameResource()->GetNumMarineResources();
	m_LeaderboardScoreDetails.m_iPrimaryWeapon = GetDebriefStats()->m_iStartingEquip0.Get( iMR );
	m_LeaderboardScoreDetails.m_iSecondaryWeapon = GetDebriefStats()->m_iStartingEquip1.Get( iMR );
	m_LeaderboardScoreDetails.m_iExtraWeapon = GetDebriefStats()->m_iStartingEquip2.Get( iMR );
	m_LeaderboardScoreDetails.m_iSquadDead = pMR->GetHealthPercent() == 0 ? 1 : 0;
	int iSquadPosition = 0;
	for ( int i = 0; i < ASW_MAX_MARINE_RESOURCES; i++ )
	{
		C_ASW_Marine_Resource *pSquadMR = ASWGameResource()->GetMarineResource( i );
		if ( pSquadMR && pSquadMR != pMR )
		{
			C_ASW_Player *pCommander = pSquadMR->GetCommander();
			if ( pCommander && ASWGameResource()->GetFirstMarineResourceForPlayer( pCommander ) == pSquadMR )
			{
				m_LeaderboardScoreDetails.m_iSquadMarineSteam[iSquadPosition] = pCommander->GetSteamID().ConvertToUint64();
			}
			else
			{
				m_LeaderboardScoreDetails.m_iSquadMarineSteam[iSquadPosition] = 0;
			}
			m_LeaderboardScoreDetails.m_iSquadMarine[iSquadPosition] = pSquadMR->GetProfileIndex();
			m_LeaderboardScoreDetails.m_iSquadPrimaryWeapon[iSquadPosition] = GetDebriefStats()->m_iStartingEquip0.Get( i );
			m_LeaderboardScoreDetails.m_iSquadSecondaryWeapon[iSquadPosition] = GetDebriefStats()->m_iStartingEquip1.Get( i );
			m_LeaderboardScoreDetails.m_iSquadExtraWeapon[iSquadPosition] = GetDebriefStats()->m_iStartingEquip2.Get( i );

			iSquadPosition++;

			if ( pSquadMR->GetHealthPercent() == 0 )
			{
				m_LeaderboardScoreDetails.m_iSquadDead |= 1 << (unsigned) iSquadPosition;
			}
		}
	}
	for ( int i = iSquadPosition; i < 7; i++ )
	{
		m_LeaderboardScoreDetails.m_iSquadMarineSteam[i] = 0;
		m_LeaderboardScoreDetails.m_iSquadMarine[i] = 0;
		m_LeaderboardScoreDetails.m_iSquadPrimaryWeapon[i] = 0;
		m_LeaderboardScoreDetails.m_iSquadSecondaryWeapon[i] = 0;
		m_LeaderboardScoreDetails.m_iSquadExtraWeapon[i] = 0;
	}
	m_LeaderboardScoreDetails.m_iTimestamp = steamapicontext->SteamUtils()->GetServerRealTime();
	const char *pszCountry = steamapicontext->SteamUtils()->GetIPCountry();
	m_LeaderboardScoreDetails.m_CountryCode[0] = pszCountry[0];
	m_LeaderboardScoreDetails.m_CountryCode[1] = pszCountry[1];
	m_LeaderboardScoreDetails.m_iDifficulty = ASWGameRules()->GetSkillLevel();
	m_LeaderboardScoreDetails.m_iModeFlags = ( ASWGameRules()->IsOnslaught() ? 1 : 0 ) | ( ASWGameRules()->IsHardcoreFF() ? 2 : 0 );
	if ( asw_stats_leaderboard_debug.GetBool() )
	{
		DevMsg( "Leaderboard score: %d\n", m_iLeaderboardScore );
		DevMsg( 2, "Leaderboard payload: " );
		const byte *pLeaderboardDetails = reinterpret_cast<const byte *>( &m_LeaderboardScoreDetails );
		for ( int i = 0; i < sizeof( m_LeaderboardScoreDetails ) / sizeof( byte ); i++ )
		{
			DevMsg( 2, "%02x", pLeaderboardDetails[i] );
		}
		DevMsg( 2, "\n" );
	}

	SteamAPICall_t hAPICall = steamapicontext->SteamUserStats()->FindOrCreateLeaderboard( szLeaderboardName, k_ELeaderboardSortMethodAscending, k_ELeaderboardDisplayTypeTimeMilliSeconds );
	m_LeaderboardFindResultCallback.Set( hAPICall, this, &CASW_Steamstats::LeaderboardFindResultCallback );

	hAPICall = steamapicontext->SteamUserStats()->FindOrCreateLeaderboard( szDifficultyLeaderboardName, k_ELeaderboardSortMethodAscending, k_ELeaderboardDisplayTypeTimeMilliSeconds );
	m_LeaderboardDifficultyFindResultCallback.Set( hAPICall, this, &CASW_Steamstats::LeaderboardDifficultyFindResultCallback );
}

void CASW_Steamstats::LeaderboardFindResultCallback( LeaderboardFindResult_t *pResult, bool bIOFailure )
{
	if ( bIOFailure || !pResult->m_bLeaderboardFound )
	{
		engine->ServerCmd( "cl_leaderboard_ready\n" );

		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Not sending leaderboard entry: IO:%d Found:%d\n", bIOFailure, pResult->m_bLeaderboardFound );
		}
		return;
	}

	if ( asw_stats_leaderboard_debug.GetBool() )
	{
		DevMsg( "Sending leaderboard entry to leaderboard ID: %llu\n", pResult->m_hSteamLeaderboard );
	}

	MissionCompleteFrame *pMissionCompleteFrame = GetClientModeASW() ? assert_cast<MissionCompleteFrame *>( GetClientModeASW()->m_hMissionCompleteFrame.Get() ) : NULL;
	MissionCompletePanel *pMissionCompletePanel = pMissionCompleteFrame ? pMissionCompleteFrame->m_pMissionCompletePanel : NULL;
	if ( pMissionCompletePanel )
	{
		pMissionCompletePanel->OnLeaderboardFound( pResult->m_hSteamLeaderboard );
	}

	SteamAPICall_t hAPICall = steamapicontext->SteamUserStats()->UploadLeaderboardScore( pResult->m_hSteamLeaderboard, k_ELeaderboardUploadScoreMethodKeepBest,
		m_iLeaderboardScore, reinterpret_cast<const int32 *>( &m_LeaderboardScoreDetails ), sizeof( m_LeaderboardScoreDetails ) / sizeof( int32 ) );
	m_LeaderboardScoreUploadedCallback.Set( hAPICall, this, &CASW_Steamstats::LeaderboardScoreUploadedCallback );
}

void CASW_Steamstats::LeaderboardDifficultyFindResultCallback( LeaderboardFindResult_t *pResult, bool bIOFailure )
{
	if ( bIOFailure || !pResult->m_bLeaderboardFound )
	{
		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Not sending leaderboard entry (difficulty): IO:%d Found:%d\n", bIOFailure, pResult->m_bLeaderboardFound );
		}
		return;
	}

	if ( asw_stats_leaderboard_debug.GetBool() )
	{
		DevMsg( "Sending leaderboard entry to (difficulty) leaderboard ID: %llu\n", pResult->m_hSteamLeaderboard );
	}

	SteamAPICall_t hAPICall = steamapicontext->SteamUserStats()->UploadLeaderboardScore( pResult->m_hSteamLeaderboard, k_ELeaderboardUploadScoreMethodKeepBest,
		m_iLeaderboardScore, reinterpret_cast<const int32 *>( &m_LeaderboardScoreDetails ), sizeof( m_LeaderboardScoreDetails ) / sizeof( int32 ) );
	m_LeaderboardDifficultyScoreUploadedCallback.Set( hAPICall, this, &CASW_Steamstats::LeaderboardDifficultyScoreUploadedCallback );
}

void CASW_Steamstats::LeaderboardScoreUploadedCallback( LeaderboardScoreUploaded_t *pResult, bool bIOFailure )
{
	engine->ServerCmd( "cl_leaderboard_ready\n" );

	if ( bIOFailure || !pResult->m_bSuccess )
	{
		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Failed to send leaderboard entry: IO:%d Success:%d\n", bIOFailure, pResult->m_bSuccess );
		}
		return;
	}

	MissionCompleteFrame *pMissionCompleteFrame = GetClientModeASW() ? assert_cast<MissionCompleteFrame *>( GetClientModeASW()->m_hMissionCompleteFrame.Get() ) : NULL;
	MissionCompletePanel *pMissionCompletePanel = pMissionCompleteFrame ? pMissionCompleteFrame->m_pMissionCompletePanel : NULL;
	if ( pMissionCompletePanel && pResult->m_bScoreChanged )
	{
		RD_LeaderboardEntry_t entry;
		entry.entry.m_steamIDUser = steamapicontext->SteamUser()->GetSteamID();
		entry.entry.m_nGlobalRank = pResult->m_nGlobalRankNew;
		entry.entry.m_nScore = pResult->m_nScore;
		entry.entry.m_cDetails = sizeof( LeaderboardScoreDetails_v2_t ) / sizeof( int32 );
		entry.entry.m_hUGC = k_UGCHandleInvalid;
		entry.details.v2 = m_LeaderboardScoreDetails;
		pMissionCompletePanel->OnLeaderboardScoreUploaded( entry, pResult->m_bScoreChanged );
	}

	if ( asw_stats_leaderboard_debug.GetBool() )
	{
		DevMsg( "Leaderboard score uploaded: Score:%d PersonalBest:%d LeaderboardID:%llu GlobalRank(Previous:%d New:%d)\n", pResult->m_nScore, pResult->m_bScoreChanged, pResult->m_hSteamLeaderboard, pResult->m_nGlobalRankPrevious, pResult->m_nGlobalRankNew );
	}
}

void CASW_Steamstats::LeaderboardDifficultyScoreUploadedCallback( LeaderboardScoreUploaded_t *pResult, bool bIOFailure )
{
	if ( bIOFailure || !pResult->m_bSuccess )
	{
		if ( asw_stats_leaderboard_debug.GetBool() )
		{
			DevWarning( "Failed to send leaderboard entry (difficulty): IO:%d Success:%d\n", bIOFailure, pResult->m_bSuccess );
		}
		return;
	}

	if ( asw_stats_leaderboard_debug.GetBool() )
	{
		DevMsg( "Leaderboard score uploaded (difficulty): Score:%d PersonalBest:%d LeaderboardID:%llu GlobalRank(Previous:%d New:%d)\n", pResult->m_nScore, pResult->m_bScoreChanged, pResult->m_hSteamLeaderboard, pResult->m_nGlobalRankPrevious, pResult->m_nGlobalRankNew );
	}
}

void CASW_Steamstats::SpeedRunLeaderboardName( char *szBuf, size_t bufSize, const char *szMap, PublishedFileId_t nMapID, const char *szChallenge, PublishedFileId_t nChallengeID )
{
	char szChallengeLeaderboardName[k_cchLeaderboardNameMax];
	if ( !Q_strcmp( szChallenge, "0" ) )
	{
		Q_snprintf( szChallengeLeaderboardName, sizeof( szChallengeLeaderboardName ), "%llu", nChallengeID );
	}
	else
	{
		Q_snprintf( szChallengeLeaderboardName, sizeof( szChallengeLeaderboardName ), "%llu_%s", nChallengeID, szChallenge );
	}

	Q_snprintf( szBuf, bufSize, "RD_SpeedRun_%s/%llu_%s", szChallengeLeaderboardName, nMapID, szMap );
}

void CASW_Steamstats::DifficultySpeedRunLeaderboardName( char *szBuf, size_t bufSize, int iSkill, const char *szMap, PublishedFileId_t nMapID, const char *szChallenge, PublishedFileId_t nChallengeID )
{
	char szChallengeLeaderboardName[k_cchLeaderboardNameMax];
	if ( !Q_strcmp( szChallenge, "0" ) )
	{
		Q_snprintf( szChallengeLeaderboardName, sizeof( szChallengeLeaderboardName ), "%llu", nChallengeID );
	}
	else
	{
		Q_snprintf( szChallengeLeaderboardName, sizeof( szChallengeLeaderboardName ), "%llu_%s", nChallengeID, szChallenge );
	}

	Q_snprintf( szBuf, bufSize, "RD_%s_SpeedRun_%s/%llu_%s", g_szDifficulties[iSkill - 1], szChallengeLeaderboardName, nMapID, szMap );
}

void CASW_Steamstats::ReadDownloadedLeaderboard( CUtlVector<RD_LeaderboardEntry_t> & entries, SteamLeaderboardEntries_t hEntries, int nCount )
{
	entries.SetCount( nCount );

	for ( int i = 0; i < nCount; i++ )
	{
		steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( hEntries, i, &entries[i].entry, reinterpret_cast<int32 *>( &entries[i].details ), sizeof( entries[i].details ) / sizeof( int32 ) );
	}
}
