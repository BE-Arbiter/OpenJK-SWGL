// Light edit mode: game-side state.
// The cgame sends "ledit_mode <0|1>"; the cgame reads G_LightEdit_Active() and the ledit_active cvar.

#include "g_local.h"
#include "g_lightedit.h"

extern qboolean	CheatsOk( gentity_t *ent );

#define LEDIT_FLAGS	( FL_GODMODE | FL_NOTARGET | FL_NOFORCE )

static qboolean	s_active = qfalse;
static int		s_savedFlags = 0;
static vec3_t	s_frozenAngles;			// the body view angles while the mode runs
static short	s_rawAngles[3];			// the last command angles before the filter
static qboolean	s_haveRaw = qfalse;
static cvar_t	*s_activeCvar = NULL;

static void LightEdit_SetCvar( const char *value )
{
	if ( !s_activeCvar )
	{
		s_activeCvar = gi.cvar( "ledit_active", "0", CVAR_ROM );
	}
	gi.cvar_set( "ledit_active", value );	// cvar_set forces: it can write a ROM cvar
}

qboolean G_LightEdit_Active( void )
{
	return s_active;
}

void G_LightEdit_Init( void )
{
	s_active = qfalse;
	s_savedFlags = 0;
	s_haveRaw = qfalse;
	LightEdit_SetCvar( "0" );
}

// The next command angles plus the delta angles give the frozen angles again, whatever the mouse did.
static void LightEdit_RestoreView( gentity_t *ent )
{
	if ( !s_haveRaw )
	{
		SetClientViewAngle( ent, s_frozenAngles );
		return;
	}
	for ( int i = 0; i < 3; i++ )
	{
		ent->client->pers.cmd_angles[i] = s_rawAngles[i];
		ent->client->ps.delta_angles[i] = ( ANGLE2SHORT( s_frozenAngles[i] ) - s_rawAngles[i] ) & 0xffff;
	}
	VectorCopy( s_frozenAngles, ent->s.angles );
	VectorCopy( s_frozenAngles, ent->client->ps.viewangles );
}

static void LightEdit_Set( gentity_t *ent, qboolean on )
{
	if ( on )
	{
		if ( !CheatsOk( ent ) )
		{
			return;
		}
		s_savedFlags = ent->flags & LEDIT_FLAGS;
		VectorCopy( ent->client->ps.viewangles, s_frozenAngles );
		s_haveRaw = qfalse;
		ent->flags |= LEDIT_FLAGS;
		s_active = qtrue;
		LightEdit_SetCvar( "1" );
		gi.SendServerCommand( ent - g_entities, "print \"light edit ON\n\"" );
	}
	else
	{
		ent->flags = ( ent->flags & ~LEDIT_FLAGS ) | s_savedFlags;
		LightEdit_RestoreView( ent );
		s_active = qfalse;
		LightEdit_SetCvar( "0" );
		gi.SendServerCommand( ent - g_entities, "print \"light edit OFF\n\"" );
	}
}

// The cgame calls this directly. A console command would run only after the rest of the command buffer.
void G_LightEdit_SetMode( qboolean on )
{
	gentity_t *ent = &g_entities[0];

	if ( !ent->inuse || !ent->client || on == s_active )
	{
		return;
	}
	LightEdit_Set( ent, on );
}

void G_LightEdit_Cmd_f( gentity_t *ent )
{
	qboolean	on;

	if ( !ent || !ent->client || ent->s.number != 0 )
	{
		return;
	}
	if ( gi.argc() > 1 )
	{
		on = ( atoi( gi.argv( 1 ) ) != 0 ) ? qtrue : qfalse;
	}
	else
	{
		on = s_active ? qfalse : qtrue;
	}
	if ( on == s_active )
	{
		return;
	}
	LightEdit_Set( ent, on );
}

void G_LightEdit_FilterUcmd( gentity_t *ent, usercmd_t *ucmd )
{
	if ( !s_active || !ent || ent->s.number != 0 )
	{
		return;
	}
	// The body stays still and keeps its angles: the camera uses the move input and the raw angles.
	for ( int i = 0; i < 3; i++ )
	{
		s_rawAngles[i] = ucmd->angles[i];
	}
	s_haveRaw = qtrue;
	ucmd->forwardmove = 0;
	ucmd->rightmove = 0;
	ucmd->upmove = 0;
	ucmd->angles[PITCH] = ANGLE2SHORT( s_frozenAngles[PITCH] ) - ent->client->ps.delta_angles[PITCH];
	ucmd->angles[YAW] = ANGLE2SHORT( s_frozenAngles[YAW] ) - ent->client->ps.delta_angles[YAW];
	ucmd->buttons &= ~( BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_USE_FORCE | BUTTON_FORCEGRIP
		| BUTTON_FORCE_LIGHTNING | BUTTON_FORCE_DRAIN | BUTTON_FORCE_FOCUS | BUTTON_FORCEGRASP | BUTTON_USE );
}
