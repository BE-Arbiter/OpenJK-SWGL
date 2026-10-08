// Light edit mode: game-side state.
// The cgame sends "ledit_mode <0|1>"; the cgame reads G_LightEdit_Active() and the ledit_active cvar.

#include "g_local.h"
#include "g_lightedit.h"

extern qboolean	CheatsOk( gentity_t *ent );

#define LEDIT_FLAGS	( FL_GODMODE | FL_NOTARGET | FL_NOFORCE )

static qboolean	s_active = qfalse;
static int		s_savedNoclip = 0;
static int		s_savedFlags = 0;
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
	s_savedNoclip = 0;
	s_savedFlags = 0;
	LightEdit_SetCvar( "0" );
}

static void LightEdit_Set( gentity_t *ent, qboolean on )
{
	if ( on )
	{
		if ( !CheatsOk( ent ) )
		{
			return;
		}
		s_savedNoclip = ent->client->noclip;
		s_savedFlags = ent->flags & LEDIT_FLAGS;
		ent->client->noclip = qtrue;
		ent->flags |= LEDIT_FLAGS;
		s_active = qtrue;
		LightEdit_SetCvar( "1" );
		gi.SendServerCommand( ent - g_entities, "print \"light edit ON\n\"" );
	}
	else
	{
		ent->client->noclip = s_savedNoclip;
		ent->flags = ( ent->flags & ~LEDIT_FLAGS ) | s_savedFlags;
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
	ucmd->buttons &= ~( BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_USE_FORCE | BUTTON_FORCEGRIP
		| BUTTON_FORCE_LIGHTNING | BUTTON_FORCE_DRAIN | BUTTON_FORCE_FOCUS | BUTTON_FORCEGRASP | BUTTON_USE );
}

// Moves the camera: the eye goes to `eye`, the view to `angles`. No effect, no telefrag, no velocity.
void G_LightEdit_Teleport( const vec3_t eye, const vec3_t angles )
{
	gentity_t	*ent = &g_entities[0];
	vec3_t		org, ang;

	if ( !s_active || !ent->inuse || !ent->client )
	{
		return;
	}
	VectorCopy( eye, org );
	org[2] -= ent->client->ps.viewheight;
	VectorSet( ang, angles[PITCH], angles[YAW], 0.0f );

	gi.unlinkentity( ent );
	VectorCopy( org, ent->client->ps.origin );
	VectorCopy( org, ent->currentOrigin );
	VectorClear( ent->client->ps.velocity );
	ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
	SetClientViewAngle( ent, ang );
	PlayerStateToEntityState( &ent->client->ps, &ent->s );
	gi.linkentity( ent );
}
