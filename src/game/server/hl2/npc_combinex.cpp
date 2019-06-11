//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is the soldier version of the combine, analogous to the HL1 grunt.
//
//=============================================================================//

#include "cbase.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "npc_combinex.h"
#include "bitstring.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#include "hl2/hl2_player.h"
#include "game.h"
#include "ammodef.h"
#include "explode.h"
#include "ai_memory.h"
#include "Sprite.h"
#include "soundenvelope.h"
#include "weapon_physcannon.h"
//#include "hl2_gamerules.h"
#include "gameweaponmanager.h"
#include "vehicle_base.h"
#include "asw_gamerules.h"
#include "antlion_dust.h"
#include "npc_antlion.h"
#include "ai_moveprobe.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar	sk_combine_x_health( "sk_combine_x_health","0");
ConVar	sk_combine_x_kick( "sk_combine_x_kick","0");


LINK_ENTITY_TO_CLASS( npc_combine_x, CNPC_CombineX );


#define	COMBINE_X_LEAP_MIN			128.0f //from antlion


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------


void CNPC_CombineX::Spawn(void)
{
	SetHealth(sk_combine_x_health.GetFloat());
	SetMaxHealth(sk_combine_x_health.GetFloat());
	SetKickDamage(sk_combine_x_kick.GetFloat());

	CapabilitiesAdd(bits_CAP_MOVE_JUMP);

	BaseClass::Spawn();
}


bool CNPC_CombineX::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
	const float MAX_JUMP_RISE		= 256;
	const float MAX_JUMP_DROP		= 512;
	const float MAX_JUMP_DISTANCE	= 512;
	const float MIN_JUMP_DISTANCE   = 64;

	if ( CAntlionRepellant::IsPositionRepellantFree( endPos ) == false )
		 return false;
	
	//Adrian: Don't try to jump if my destination is right next to me.
	if ( ( endPos - GetAbsOrigin()).Length() < MIN_JUMP_DISTANCE ) 
		 return false;

	return BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE );
}


#if HL2_EPISODIC
//-----------------------------------------------------------------------------
// Purpose: Translate base class activities into combot activites
//-----------------------------------------------------------------------------
Activity CNPC_CombineX::NPC_TranslateActivity( Activity eNewActivity )
{
	// If the special ep2_outland_05 "use march" flag is set, use the more casual marching anim.
	if ( m_iUseMarch && eNewActivity == ACT_WALK )
	{
		eNewActivity = ACT_WALK_MARCH;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_CombineX )

	DEFINE_KEYFIELD( m_iUseMarch, FIELD_INTEGER, "usemarch" ),

END_DATADESC()
#endif