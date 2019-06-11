//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "tier0/vprof.h"
#include "fx_line.h"
#include "fx_quad.h"
#include "view.h"
#include "particles_localspace.h"
#include "dlight.h"
#include "iefx.h"
//#include "clienteffectprecachesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern Vector GetTracerOrigin( const CEffectData &data );
extern void FX_TracerSound( const Vector &start, const Vector &end, int iTracerType );


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//-----------------------------------------------------------------------------
void FX_PlayerAR2Tracer( const Vector &start, const Vector &end )
{
	VPROF_BUDGET( "FX_PlayerAR2Tracer", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	Vector	shotDir, dStart, dEnd;
	float	length;

	//Find the direction of the tracer
	VectorSubtract( end, start, shotDir );
	length = VectorNormalize( shotDir );

	//We don't want to draw them if they're too close to us
	if ( length < 128 )
		return;

	//Randomly place the tracer along this line, with a random length
	VectorMA( start, random->RandomFloat( 0.0f, 8.0f ), shotDir, dStart );
	VectorMA( dStart, MIN( length, random->RandomFloat( 256.0f, 1024.0f ) ), shotDir, dEnd );

	//Create the line
	CFXStaticLine *tracerLine = new CFXStaticLine( "Tracer", dStart, dEnd, random->RandomFloat( 6.0f, 12.0f ), 0.01f, "effects/gunshiptracer", 0 );
	assert( tracerLine );

	//Throw it into the list
	clienteffects->AddEffect( tracerLine );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//			velocity - 
//			makeWhiz - 
//-----------------------------------------------------------------------------
void FX_AR2Tracer( Vector& start, Vector& end, int velocity, bool makeWhiz )
{
	VPROF_BUDGET( "FX_AR2Tracer", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	//Don't make small tracers
	float dist;
	Vector dir;

	VectorSubtract( end, start, dir );
	dist = VectorNormalize( dir );

	// Don't make short tracers.
	if ( dist < 128 )
		return;

	float length = random->RandomFloat( 128.0f, 256.0f );
	float life = ( dist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well
	
	//Add it
	FX_AddDiscreetLine( start, dir, velocity, length, dist, random->RandomFloat( 0.5f, 1.5f ), life, "effects/gunshiptracer" );

	if( makeWhiz )
	{
		FX_TracerSound( start, end, TRACER_TYPE_GUNSHIP );	
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AR2TracerCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	if ( player == NULL )
		return;

	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	float flVelocity = data.m_flScale;
	bool bWhiz = (data.m_fFlags & TRACER_FLAG_WHIZ);
	int iEntIndex = data.entindex();

	if ( iEntIndex && iEntIndex == player->index )
	{
		Vector	foo = data.m_vStart;
		QAngle	vangles;
		Vector	vforward, vright, vup;

		engine->GetViewAngles( vangles );
		AngleVectors( vangles, &vforward, &vright, &vup );

		VectorMA( data.m_vStart, 4, vright, foo );
		foo[2] -= 0.5f;

		FX_PlayerAR2Tracer( foo, (Vector&)data.m_vOrigin );
		return;
	}
	
	// Use default velocity if none specified
	if ( !flVelocity )
	{
		flVelocity = 8000;
	}

	// Do tracer effect
	FX_AR2Tracer( (Vector&)vecStart, (Vector&)data.m_vOrigin, flVelocity, bWhiz );
}

DECLARE_CLIENT_EFFECT( AR2Tracer, AR2TracerCallback );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void AR2ExplosionCallback( const CEffectData &data )
{
	float lifetime = random->RandomFloat( 0.4f, 0.75f );

	// Ground splash
	FX_AddQuad( data.m_vOrigin, 
				data.m_vNormal, 
				data.m_flRadius, 
				data.m_flRadius * 4.0f,
				0.85f, 
				1.0f,
				0.0f,
				0.25f,
				random->RandomInt( 0, 360 ), 
				random->RandomFloat( -4, 4 ), 
				Vector( 1.0f, 1.0f, 1.0f ), 
				lifetime,
				"effects/combinemuzzle1",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	Vector	vRight, vUp;
	VectorVectors( data.m_vNormal, vRight, vUp );

	Vector	start, end;
	
	float radius = data.m_flRadius * 0.15f;

	// Base vertical shaft
	FXLineData_t lineData;

	start = data.m_vOrigin;
	end = start + ( data.m_vNormal * random->RandomFloat( radius*2.0f, radius*4.0f ) );

	lineData.m_flDieTime = lifetime;
	
	lineData.m_flStartAlpha= 1.0f;
	lineData.m_flEndAlpha = 0.0f;
	
	lineData.m_flStartScale = radius*4;
	lineData.m_flEndScale = radius*5; 

	lineData.m_pMaterial = materials->FindMaterial( "effects/ar2ground2", 0, 0 );

	lineData.m_vecStart = start;
	lineData.m_vecStartVelocity = vec3_origin;

	lineData.m_vecEnd = end;
	lineData.m_vecEndVelocity = data.m_vNormal * random->RandomFloat( 200, 350 );

	FX_AddLine( lineData );

	// Inner filler shaft
	start = data.m_vOrigin;
	end = start + ( data.m_vNormal * random->RandomFloat( 16, radius*0.25f ) );

	lineData.m_flDieTime = lifetime - 0.1f;
	
	lineData.m_flStartAlpha= 1.0f;
	lineData.m_flEndAlpha = 0.0f;
	
	lineData.m_flStartScale = radius*2;
	lineData.m_flEndScale = radius*4; 

	lineData.m_pMaterial = materials->FindMaterial( "effects/ar2ground2", 0, 0 );

	lineData.m_vecStart = start;
	lineData.m_vecStartVelocity = vec3_origin;

	lineData.m_vecEnd = end;
	lineData.m_vecEndVelocity = data.m_vNormal * random->RandomFloat( 64, 128 );

	FX_AddLine( lineData );
}

DECLARE_CLIENT_EFFECT( AR2Explosion, AR2ExplosionCallback );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void AR2ImpactCallback( const CEffectData &data )
{
	FX_AddQuad( data.m_vOrigin, 
				data.m_vNormal, 
				random->RandomFloat( 24, 32 ),
				0,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.25f, 
				"effects/combinemuzzle2_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );
}

DECLARE_CLIENT_EFFECT( AR2Impact, AR2ImpactCallback );
