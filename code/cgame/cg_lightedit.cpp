// Light edit mode of the RTX path tracer: cgame side.
// Tools, picking, drag, undo/redo, console commands and the 2D overlay.
// The renderer API is in rd-common/rtx_light_edit_api.h.

#include "cg_headers.h"
#include "cg_media.h"
#include "../game/g_lightedit.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <stdio.h>

#define LEDIT_PICK_MIN_RADIUS	8.0f
#define LEDIT_PICK_SCALE		0.015f
#define LEDIT_OCCLUDE_RANGE		3000.0f
#define LEDIT_MAX_TRACES		1024
#define LEDIT_FONT_SCALE		0.7f
#define LEDIT_MSG_TIME			4000

typedef struct {
	rtxLightDesc_t	d;
	qboolean		valid;		// Get succeeded and the light is not deleted
	qboolean		onScreen;
	qboolean		occluded;
	float			sx, sy;		// virtual 640x480 screen position
	float			depth;		// distance along the view axis
} ledRec_t;

typedef struct {
	int		id;
	float	t;
} ledHit_t;

rtxLightEditAPI_t			*s_api = NULL;
int							s_tool = LEDIT_TOOL_SELECT;
int							s_sel = -1;
std::vector<int>			s_selList;
int							s_pickId = -1;
float						s_pickT = 0.0f;
static qboolean				s_active = qfalse;		// cgame side of the mode
static qboolean				s_leaving = qfalse;		// leave requested, game not yet inactive
static int					s_cycle = 0;
static int					s_cycleBase = -1;
static int					s_wheel = 0;			// wheel steps since the last frame
static int					s_prevButtons = 0;
static float				s_createOffset = 16.0f;
static vec3_t				s_ghost;
static qboolean				s_ghostSolid = qfalse;
static rtxLightStats_t		s_stats;

static std::vector<ledRec_t>		s_recs;
static std::vector<ledHit_t>		s_hits;
static std::vector<unsigned char>	s_occ;			// last occlusion result per id

static char			s_msg[160];
static int			s_msgEnd = 0;

static vmCvar_t		ledit_xray;
static vmCvar_t		ledit_show;
static vmCvar_t		ledit_preset_intensity;
static vmCvar_t		ledit_preset_radius;
static vmCvar_t		ledit_preset_color;

static const vec4_t	colBlue		= { 0.30f, 0.55f, 1.00f, 1.0f };
static const vec4_t	colYellow	= { 1.00f, 0.90f, 0.20f, 1.0f };
static const vec4_t	colGreen	= { 0.20f, 1.00f, 0.30f, 1.0f };
static const vec4_t	colOrange	= { 1.00f, 0.55f, 0.10f, 1.0f };
static const vec4_t	colRed		= { 1.00f, 0.15f, 0.15f, 1.0f };
static const vec4_t	colRedDim	= { 1.00f, 0.15f, 0.15f, 0.45f };
static const vec4_t	colWhite	= { 1.00f, 1.00f, 1.00f, 1.0f };
static const vec4_t	colGrey		= { 0.60f, 0.60f, 0.60f, 1.0f };
static const vec4_t	colDark		= { 0.00f, 0.00f, 0.00f, 0.85f };
static const vec4_t	colPanel	= { 0.00f, 0.00f, 0.00f, 0.45f };
static const vec4_t	colCyan		= { 0.30f, 1.00f, 1.00f, 1.0f };

/*
=================
Messages and small helpers
=================
*/
void LE_Msg( const char *fmt, ... )
{
	va_list	args;

	va_start( args, fmt );
	vsnprintf( s_msg, sizeof( s_msg ), fmt, args );
	va_end( args );
	s_msg[sizeof( s_msg ) - 1] = 0;
	s_msgEnd = cg.time + LEDIT_MSG_TIME;
	CG_Printf( "%s\n", s_msg );
}

static rtxLightEditAPI_t *LE_GetAPI( void )
{
	rtxLightEditAPI_t	*api = (rtxLightEditAPI_t *)cgi_R_GetExtension( RTX_LIGHTEDIT_API_NAME );

	if ( api && api->version != RTX_LIGHTEDIT_API_VERSION )
	{
		api = NULL;
	}
	return api;
}

qboolean LE_GetDesc( int id, rtxLightDesc_t *d )
{
	memset( d, 0, sizeof( *d ) );
	return s_api->Get( id, d );
}

// Compares the fields that Set copies.
qboolean LE_DescEqual( const rtxLightDesc_t *a, const rtxLightDesc_t *b )
{
	return (qboolean)( VectorCompare( a->origin, b->origin ) && VectorCompare( a->color, b->color )
		&& a->intensity == b->intensity && a->radius == b->radius
		&& a->type == b->type && VectorCompare( a->dir, b->dir )
		&& a->coneOuter == b->coneOuter && a->coneInner == b->coneInner
		&& !strncmp( a->name, b->name, RTX_LIGHTEDIT_NAME_LEN ) );
}

static const char *LE_SourceStr( const rtxLightDesc_t *d )
{
	if ( d->source == RTX_LSRC_ENTITY )
	{
		return va( "entity %d", d->sourceKey );
	}
	if ( d->source == RTX_LSRC_LGT )
	{
		return va( "lgt %d", d->sourceKey );
	}
	return "added";
}

static const char *LE_FlagsStr( int flags )
{
	return va( "%s%s%s%s%s", ( flags & RTX_LFLAG_MODIFIED ) ? "MODIFIED " : "",
		( flags & RTX_LFLAG_DISABLED ) ? "DISABLED " : "", ( flags & RTX_LFLAG_DELETED ) ? "DELETED " : "",
		( flags & RTX_LFLAG_IN_SOLID ) ? "IN_SOLID " : "", ( flags & RTX_LFLAG_DRAGGING ) ? "DRAGGING" : "" );
}

static qboolean LE_NeedActive( void )
{
	if ( !s_active || !s_api )
	{
		LE_Msg( "light edit: not active" );
		return qfalse;
	}
	return qtrue;
}

static qboolean LE_NeedSelection( void )
{
	if ( s_sel < 0 )
	{
		LE_Msg( "light edit: nothing selected" );
		return qfalse;
	}
	return qtrue;
}

/*
=================
Selection
=================
*/
qboolean LE_SelHas( int id )
{
	return (qboolean)( std::find( s_selList.begin(), s_selList.end(), id ) != s_selList.end() );
}

void LE_SelClear( void )
{
	s_selList.clear();
	s_sel = -1;
}

// Appends the light and makes it the primary one.
void LE_SelAdd( int id )
{
	if ( id < 0 )
	{
		return;
	}
	if ( !LE_SelHas( id ) )
	{
		s_selList.push_back( id );
	}
	s_sel = id;
}

// A removed light leaves the selection; the last clicked one that remains becomes primary.
static void LE_SelRemove( int id )
{
	std::vector<int>::iterator	it = std::find( s_selList.begin(), s_selList.end(), id );

	if ( it != s_selList.end() )
	{
		s_selList.erase( it );
	}
	if ( s_sel == id || !LE_SelHas( s_sel ) )
	{
		s_sel = s_selList.empty() ? -1 : s_selList.back();
	}
}

void LE_SelSet( int id )
{
	LE_SelClear();
	LE_SelAdd( id );
}

void LE_SelToggle( int id )
{
	if ( LE_SelHas( id ) )
	{
		LE_SelRemove( id );
	}
	else
	{
		LE_SelAdd( id );
	}
}

void LE_SelPrimary( int id )
{
	if ( LE_SelHas( id ) )
	{
		s_sel = id;
	}
}

// Targets of an action: the selection when the aimed light is a member (or nothing is aimed), else the aimed light.
void LE_ActionTargets( std::vector<int> &out )
{
	out.clear();
	if ( s_pickId >= 0 && !LE_SelHas( s_pickId ) )
	{
		out.push_back( s_pickId );
		return;
	}
	out = s_selList;
}

/*
=================
Actions shared by tools and commands
=================
*/
// Remove on each light, one undo group.
static void LE_DoRemove( const std::vector<int> &ids )
{
	int	done = 0;

	{
		ledBatch	batch;

		LE_UndoBegin( "remove" );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	before, after;

			if ( !LE_GetDesc( ids[i], &before ) )
			{
				continue;
			}
			if ( before.flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED ) )
			{
				if ( ids.size() == 1 )
				{
					LE_Msg( "light edit: light %d is already removed", ids[i] );
				}
				continue;
			}
			if ( !s_api->Remove( ids[i] ) )
			{
				LE_Msg( "light edit: remove failed (%s)", s_api->LastError() );
				continue;
			}
			LE_GetDesc( ids[i], &after );
			LE_UndoPush( LEDU_REMOVE, ids[i], &before, &after, "remove" );
			if ( after.flags & RTX_LFLAG_DELETED )
			{
				LE_SelRemove( ids[i] );
			}
			done++;
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: removed %d light%s", done, done > 1 ? "s" : "" );
	}
}

// Restore on a disabled light, one undo entry.
static void LE_DoRestore( int id )
{
	rtxLightDesc_t	before, after;

	if ( !LE_GetDesc( id, &before ) )
	{
		return;
	}
	if ( !( before.flags & RTX_LFLAG_DISABLED ) )
	{
		LE_Msg( "light edit: light %d is not removed", id );
		return;
	}
	if ( !s_api->Restore( id ) )
	{
		LE_Msg( "light edit: restore failed (%s)", s_api->LastError() );
		return;
	}
	LE_GetDesc( id, &after );
	LE_UndoPush( LEDU_RESTORE, id, &before, &after, "restore" );
	LE_Msg( "light edit: restored light %d", id );
}

// Set on a light, one undo entry when something changes.
qboolean LE_DoSet( int id, const rtxLightDesc_t *want, const char *label )
{
	rtxLightDesc_t	before, after;

	if ( !LE_GetDesc( id, &before ) )
	{
		return qfalse;
	}
	if ( LE_DescEqual( &before, want ) )
	{
		return qtrue;
	}
	if ( !s_api->Set( id, want ) )
	{
		LE_Msg( "light edit: set failed (%s)", s_api->LastError() );
		return qfalse;
	}
	LE_GetDesc( id, &after );
	LE_UndoPush( LEDU_SET, id, &before, &after, label );
	return qtrue;
}

/*
=================
Mode enter / leave
=================
*/
static void LE_ReadButtons( int *buttons )
{
	usercmd_t	cmd;

	memset( &cmd, 0, sizeof( cmd ) );
	if ( !cgi_GetUserCmd( cgi_GetCurrentCmdNumber(), &cmd ) )
	{
		cmd.buttons = 0;
	}
	*buttons = cmd.buttons;
}

static void LE_LeaveSide( void )
{
	if ( s_active )
	{
		LE_EndGrab();
		if ( s_api )
		{
			s_api->End();
		}
	}
	s_active = qfalse;
	LE_GrabReset();
	s_pickId = -1;
	s_hits.clear();
}

static void LE_EnterSide( void )
{
	s_api = LE_GetAPI();
	if ( !s_api || !s_api->IsAvailable() || !s_api->Begin() )
	{
		LE_Msg( "light edit: %s", s_api ? s_api->LastError() : "needs the RTX renderer" );
		G_LightEdit_SetMode( qfalse );
		s_leaving = qtrue;
		return;
	}
	s_active = qtrue;
	s_tool = LEDIT_TOOL_SELECT;
	LE_SelClear();
	s_pickId = -1;
	s_cycle = 0;
	s_cycleBase = -1;
	s_wheel = 0;
	LE_GrabReset();
	s_occ.clear();
	LE_ReadButtons( &s_prevButtons );
}

void LE_SetTool( int tool )
{
	if ( tool != LEDIT_TOOL_SELECT && tool != LEDIT_TOOL_CREATE && tool != LEDIT_TOOL_MOVE
		&& tool != LEDIT_TOOL_CLONE && tool != LEDIT_TOOL_DELETE )
	{
		LE_Msg( "light edit: tool %d is not in this version", tool );
		return;
	}
	if ( tool != s_tool )
	{
		LE_EndGrab();
		s_tool = tool;
		s_cycle = 0;
	}
}

/*
=================
Per-frame snapshot and picking
=================
*/
static qboolean LE_Occluded( const vec3_t eye, const vec3_t org, float dist )
{
	trace_t	tr;

	CG_Trace( &tr, eye, vec3_origin, vec3_origin, org, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.startsolid || tr.allsolid )
	{
		return qfalse;
	}
	return (qboolean)( tr.fraction < 1.0f && dist * ( 1.0f - tr.fraction ) > 4.0f );
}

static bool LE_HitLess( const ledHit_t &a, const ledHit_t &b )
{
	return a.t < b.t;
}

// Caches every descriptor, projects the lights and builds the sorted list of lights under the crosshair.
static void LE_Snapshot( void )
{
	const int	n = s_api->Count();
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];
	int			traces = 0;

	s_recs.resize( n );
	if ( (int)s_occ.size() < n )
	{
		s_occ.resize( n, 0 );
	}
	s_hits.clear();

	for ( int i = 0; i < n; i++ )
	{
		ledRec_t	&r = s_recs[i];
		vec3_t		rel;
		float		t, pr, d2;
		qboolean	hit;

		r.valid = qfalse;
		r.onScreen = qfalse;
		r.occluded = qfalse;
		memset( &r.d, 0, sizeof( r.d ) );
		if ( !s_api->Get( i, &r.d ) || ( r.d.flags & RTX_LFLAG_DELETED ) )
		{
			continue;
		}
		r.valid = qtrue;

		VectorSubtract( r.d.origin, eye, rel );
		t = DotProduct( rel, fwd );
		r.depth = t;
		if ( t <= 0.0f )
		{
			continue;
		}
		if ( CG_WorldCoordToScreenCoordFloat( r.d.origin, &r.sx, &r.sy ) )
		{
			r.onScreen = (qboolean)( r.sx > -32 && r.sx < 672 && r.sy > -32 && r.sy < 512 );
		}

		pr = Q_max( LEDIT_PICK_MIN_RADIUS, LEDIT_PICK_SCALE * t );
		d2 = DotProduct( rel, rel ) - t * t;
		hit = (qboolean)( d2 <= pr * pr );

		if ( ( r.onScreen || hit ) && !ledit_xray.integer && !( r.d.flags & RTX_LFLAG_IN_SOLID )
			&& VectorLength( rel ) <= LEDIT_OCCLUDE_RANGE )
		{
			if ( traces < LEDIT_MAX_TRACES )
			{
				s_occ[i] = LE_Occluded( eye, r.d.origin, VectorLength( rel ) ) ? 1 : 0;
				traces++;
			}
			r.occluded = (qboolean)( s_occ[i] != 0 );
		}
		else if ( !ledit_xray.integer && !( r.d.flags & RTX_LFLAG_IN_SOLID ) && VectorLength( rel ) > LEDIT_OCCLUDE_RANGE )
		{
			// No trace this far: show it only in x-ray mode.
			r.occluded = qtrue;
		}

		if ( hit && !r.occluded )
		{
			ledHit_t	h;

			h.id = i;
			h.t = t;
			s_hits.push_back( h );
		}
	}
	std::sort( s_hits.begin(), s_hits.end(), LE_HitLess );

	// Deleted and unknown lights leave the selection.
	for ( size_t i = 0; i < s_selList.size(); )
	{
		const int	id = s_selList[i];

		if ( id >= n || !s_recs[id].valid )
		{
			LE_SelRemove( id );
		}
		else
		{
			i++;
		}
	}
}

// Picks among the hits; tool 1 cycles with the wheel.
static void LE_Pick( int wheel )
{
	const int	n = (int)s_hits.size();

	if ( n == 0 || s_hits[0].id != s_cycleBase )
	{
		s_cycle = 0;
		s_cycleBase = n ? s_hits[0].id : -1;
	}
	if ( s_tool == LEDIT_TOOL_SELECT )
	{
		s_cycle += wheel;
	}
	else
	{
		s_cycle = 0;
	}
	if ( n == 0 )
	{
		s_pickId = -1;
		s_pickT = 0.0f;
		return;
	}
	const int	idx = ( ( s_cycle % n ) + n ) % n;

	s_pickId = s_hits[idx].id;
	s_pickT = s_hits[idx].t;
}

/*
=================
Tools
=================
*/
static void LE_ToolSelect( qboolean priDown, qboolean secDown )
{
	if ( priDown )
	{
		if ( s_pickId >= 0 )
		{
			LE_SelSet( s_pickId );
		}
		else
		{
			LE_SelClear();
		}
	}
	if ( secDown && s_pickId >= 0 )
	{
		LE_SelToggle( s_pickId );
	}
}

static void LE_ParseColor( vec3_t out )
{
	float	c[3] = { 1.0f, 0.9f, 0.8f };

	sscanf( ledit_preset_color.string, "%f %f %f", &c[0], &c[1], &c[2] );
	for ( int i = 0; i < 3; i++ )
	{
		out[i] = Com_Clamp( 0.0f, 1.0f, c[i] );
	}
}

static void LE_UpdateGhost( void )
{
	trace_t		tr;
	vec3_t		end;
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];

	VectorMA( eye, 8192.0f, fwd, end );
	CG_Trace( &tr, eye, vec3_origin, vec3_origin, end, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.fraction < 1.0f && !tr.startsolid && !tr.allsolid && !( tr.surfaceFlags & SURF_SKY ) )
	{
		VectorMA( tr.endpos, s_createOffset, tr.plane.normal, s_ghost );
	}
	else
	{
		VectorMA( eye, 256.0f, fwd, s_ghost );
	}
	LE_SnapPoint( s_ghost, 7 );
	s_ghostSolid = s_api->PointInSolid( s_ghost );
}

static void LE_ToolCreate( qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	for ( int i = 0; i < abs( wheel ); i++ )
	{
		if ( fine )
		{
			s_createOffset += ( wheel > 0 ) ? 1.0f : -1.0f;
		}
		else
		{
			s_createOffset *= ( wheel > 0 ) ? 2.0f : 0.5f;
		}
		s_createOffset = Com_Clamp( 2.0f, 256.0f, s_createOffset );
	}
	LE_UpdateGhost();

	if ( secDown )
	{
		LE_Msg( "light edit: spots come in the next version" );
	}
	if ( priDown )
	{
		rtxLightDesc_t	d, after;
		int				id;

		if ( s_ghostSolid )
		{
			LE_Msg( "light edit: cannot add a light outside the world" );
			return;
		}
		memset( &d, 0, sizeof( d ) );
		d.type = RTX_LTYPE_SPHERE;
		VectorCopy( s_ghost, d.origin );
		LE_ParseColor( d.color );
		d.intensity = ledit_preset_intensity.value;
		d.radius = ledit_preset_radius.value;
		id = s_api->Add( &d );
		if ( id < 0 )
		{
			LE_Msg( "light edit: add failed (%s)", s_api->LastError() );
			return;
		}
		LE_GetDesc( id, &after );
		LE_UndoPush( LEDU_ADD, id, NULL, &after, "add" );
		LE_SelSet( id );
		LE_Msg( "light edit: added light %d", id );
	}
}

static void LE_ToolDelete( qboolean priDown, qboolean secDown )
{
	if ( priDown )
	{
		std::vector<int>	ids;

		LE_ActionTargets( ids );
		if ( !ids.empty() )
		{
			LE_DoRemove( ids );
		}
	}
	if ( secDown && s_pickId >= 0 )
	{
		LE_DoRestore( s_pickId );
	}
}

/*
=================
CG_LightEdit_Frame
=================
*/
void CG_LightEdit_Frame( void )
{
	int			buttons, wheel;
	qboolean	prim, sec, priDown, secDown, fine;
	const qboolean	gameActive = G_LightEdit_Active();

	cgi_Cvar_Update( &ledit_xray );
	cgi_Cvar_Update( &ledit_show );
	cgi_Cvar_Update( &ledit_preset_intensity );
	cgi_Cvar_Update( &ledit_preset_radius );
	cgi_Cvar_Update( &ledit_preset_color );
	LE_GridUpdate();

	if ( s_leaving && !gameActive )
	{
		s_leaving = qfalse;
	}
	if ( gameActive && !s_active && !s_leaving )
	{
		LE_EnterSide();
	}
	else if ( !gameActive && s_active )
	{
		LE_LeaveSide();
	}
	if ( !s_active )
	{
		s_wheel = 0;
		return;
	}
	if ( !cg.snap || !s_api->IsAvailable() )
	{
		LE_Msg( "light edit: the RTX renderer is no longer available" );
		LE_LeaveSide();
		G_LightEdit_SetMode( qfalse );
		s_leaving = qtrue;
		return;
	}

	LE_ReadButtons( &buttons );
	prim = (qboolean)( ( buttons & BUTTON_ATTACK ) != 0 );
	sec = (qboolean)( ( buttons & BUTTON_ALT_ATTACK ) != 0 );
	priDown = (qboolean)( prim && !( s_prevButtons & BUTTON_ATTACK ) );
	secDown = (qboolean)( sec && !( s_prevButtons & BUTTON_ALT_ATTACK ) );
	fine = (qboolean)( ( buttons & BUTTON_WALKING ) != 0 );
	s_prevButtons = buttons;

	wheel = Com_Clampi( -8, 8, s_wheel );
	s_wheel = 0;

	s_api->GetStats( &s_stats );
	LE_Snapshot();
	if ( LE_GrabActive() )
	{
		s_pickId = LE_GrabPrimary();	// the grabbed light stays the picked one
	}
	else
	{
		LE_Pick( wheel );
	}

	switch ( s_tool )
	{
	case LEDIT_TOOL_SELECT:
		LE_ToolSelect( priDown, secDown );
		break;
	case LEDIT_TOOL_CREATE:
		LE_ToolCreate( priDown, secDown, wheel, fine );
		break;
	case LEDIT_TOOL_MOVE:
		LE_ToolMove( prim, priDown, secDown, wheel, fine );
		break;
	case LEDIT_TOOL_CLONE:
		LE_ToolClone( priDown, secDown, wheel );
		break;
	case LEDIT_TOOL_DELETE:
		LE_ToolDelete( priDown, secDown );
		break;
	}
}

void CG_LightEdit_Init( void )
{
	// The map change drops the renderer state: nothing to release here.
	s_api = LE_GetAPI();
	s_active = qfalse;
	s_leaving = qfalse;
	s_tool = LEDIT_TOOL_SELECT;
	LE_SelClear();
	s_pickId = -1;
	s_cycle = 0;
	s_cycleBase = -1;
	s_wheel = 0;
	s_prevButtons = 0;
	s_createOffset = 16.0f;
	s_ghostSolid = qfalse;
	LE_GrabReset();
	LE_CloneInit();
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_recs.clear();
	s_hits.clear();
	s_occ.clear();
	LE_UndoClear();
	s_msg[0] = 0;
	s_msgEnd = 0;

	cgi_Cvar_Register( &ledit_xray, "ledit_xray", "0", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_show, "ledit_show", "1", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_intensity, "ledit_preset_intensity", "2000", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_radius, "ledit_preset_radius", "8", CVAR_ARCHIVE );
	cgi_Cvar_Register( &ledit_preset_color, "ledit_preset_color", "1 0.9 0.8", CVAR_ARCHIVE );
	LE_GridInit();
}

qboolean CG_LightEdit_Active( void )
{
	return s_active;
}

/*
=================
Console commands
=================
*/
static void LE_Cmd_LightEdit( void )
{
	char			a1[32], a2[32];

	// CG_Argv returns a static buffer: copy each argument.
	Q_strncpyz( a1, CG_Argv( 1 ), sizeof( a1 ) );
	Q_strncpyz( a2, CG_Argv( 2 ), sizeof( a2 ) );

	const qboolean	discard = (qboolean)( !Q_stricmp( a1, "discard" ) || !Q_stricmp( a2, "discard" ) );
	const qboolean	cur = (qboolean)( s_active || ( G_LightEdit_Active() && !s_leaving ) );
	qboolean		want;

	if ( a1[0] && Q_stricmp( a1, "discard" ) )
	{
		want = (qboolean)( atoi( a1 ) != 0 );
	}
	else
	{
		want = (qboolean)!cur;
	}

	if ( want )
	{
		rtxLightEditAPI_t	*api;

		if ( cur )
		{
			return;
		}
		api = LE_GetAPI();
		if ( !api || !api->IsAvailable() )
		{
			LE_Msg( "light edit: needs the RTX renderer (cl_renderer rdsp-vulkan, r_rtx 1)" );
			return;
		}
		s_api = api;
		s_leaving = qfalse;
		G_LightEdit_SetMode( qtrue );
		return;
	}

	if ( !cur )
	{
		LE_Msg( "light edit: not active" );
		return;
	}
	if ( s_active && s_api )
	{
		LE_EndGrab();
		if ( discard )
		{
			s_api->Reload();
			LE_UndoClear();
			LE_SelClear();
		}
		else
		{
			rtxLightStats_t	st;

			s_api->GetStats( &st );
			if ( st.unsavedChanges > 0 )
			{
				LE_Msg( "light edit: %d unsaved changes kept in memory until the next map load (ledit_save to write them, lightedit 0 discard to drop them)", st.unsavedChanges );
			}
		}
	}
	LE_LeaveSide();
	s_leaving = qtrue;
	G_LightEdit_SetMode( qfalse );
}

static void LE_Cmd_Save( void )
{
	if ( !LE_NeedActive() )
	{
		return;
	}
	if ( s_api->Save() )
	{
		LE_Msg( "light edit: saved" );
	}
	else
	{
		LE_Msg( "light edit: save failed (%s)", s_api->LastError() );
	}
}

static void LE_Cmd_Reload( void )
{
	if ( !LE_NeedActive() )
	{
		return;
	}
	LE_EndGrab();
	if ( s_api->Reload() )
	{
		LE_UndoClear();
		LE_SelClear();
		LE_Msg( "light edit: reloaded" );
	}
	else
	{
		LE_Msg( "light edit: reload failed (%s)", s_api->LastError() );
	}
}

static void LE_Cmd_Undo( void )
{
	if ( LE_NeedActive() )
	{
		LE_Undo();
	}
}

static void LE_Cmd_Redo( void )
{
	if ( LE_NeedActive() )
	{
		LE_Redo();
	}
}

static void LE_Cmd_History( void )
{
	if ( LE_NeedActive() )
	{
		LE_UndoHistory();
	}
}

static void LE_Cmd_Delete( void )
{
	if ( LE_NeedActive() && LE_NeedSelection() )
	{
		LE_DoRemove( s_selList );
	}
}

static void LE_Cmd_Deselect( void )
{
	if ( LE_NeedActive() )
	{
		LE_SelClear();
		LE_Msg( "light edit: selection cleared" );
	}
}

static void LE_Cmd_XrayToggle( void )
{
	if ( LE_NeedActive() )
	{
		const int	on = ledit_xray.integer ? 0 : 1;

		cgi_Cvar_Set( "ledit_xray", va( "%d", on ) );
		ledit_xray.integer = on;
		LE_Msg( "light edit: x-ray %s", on ? "on" : "off" );
	}
}

static void LE_Cmd_GridNext( void )
{
	if ( LE_NeedActive() )
	{
		LE_GridNext();
	}
}

static void LE_Cmd_SnapToggle( void )
{
	if ( LE_NeedActive() )
	{
		LE_SnapToggle();
	}
}

// Reverts every selected light, one undo group.
static void LE_Cmd_Revert( void )
{
	int	done = 0;

	if ( !LE_NeedActive() || !LE_NeedSelection() )
	{
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( "revert" );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	before, after;

			if ( !LE_GetDesc( s_selList[i], &before ) )
			{
				continue;
			}
			if ( !s_api->Revert( s_selList[i] ) )
			{
				LE_Msg( "light edit: revert failed (%s)", s_api->LastError() );
				continue;
			}
			LE_GetDesc( s_selList[i], &after );
			if ( !LE_DescEqual( &before, &after ) )
			{
				LE_UndoPush( LEDU_SET, s_selList[i], &before, &after, "revert" );
			}
			done++;
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %d light%s reverted", done, done > 1 ? "s" : "" );
	}
}

// Sets a property on every selected light. An origin moves the group: the primary light gets it.
static void LE_Cmd_Set( void )
{
	rtxLightDesc_t	d, prim;
	vec3_t			delta;
	char			what[32];
	const int		argc = cgi_Argc();
	char			label[48];
	int				done = 0;

	Q_strncpyz( what, CG_Argv( 1 ), sizeof( what ) );
	if ( !LE_NeedActive() || !LE_NeedSelection() )
	{
		return;
	}
	if ( !LE_GetDesc( s_sel, &prim ) )
	{
		return;
	}
	d = prim;
	VectorClear( delta );
	if ( !Q_stricmp( what, "origin" ) && argc >= 5 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			d.origin[i] = atof( CG_Argv( 2 + i ) );
		}
		VectorSubtract( d.origin, prim.origin, delta );
	}
	else if ( !Q_stricmp( what, "color" ) && argc >= 5 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			d.color[i] = Com_Clamp( 0.0f, 1.0f, atof( CG_Argv( 2 + i ) ) );
		}
	}
	else if ( !Q_stricmp( what, "intensity" ) && argc >= 3 )
	{
		d.intensity = atof( CG_Argv( 2 ) );
	}
	else if ( !Q_stricmp( what, "radius" ) && argc >= 3 )
	{
		d.radius = atof( CG_Argv( 2 ) );
	}
	else if ( !Q_stricmp( what, "name" ) && argc >= 3 )
	{
		Q_strncpyz( d.name, CG_Argv( 2 ), sizeof( d.name ) );
	}
	else
	{
		LE_Msg( "usage: ledit_set <origin x y z | color r g b | intensity v | radius v | name s>" );
		return;
	}
	Com_sprintf( label, sizeof( label ), "set %s", what );

	{
		ledBatch	batch;

		LE_UndoBegin( label );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	cur = d;

			if ( s_selList[i] != s_sel )
			{
				// Other lights keep their own values except the one that is set.
				if ( !LE_GetDesc( s_selList[i], &cur ) )
				{
					continue;
				}
				if ( !Q_stricmp( what, "origin" ) )
				{
					VectorAdd( cur.origin, delta, cur.origin );
				}
				else if ( !Q_stricmp( what, "color" ) )
				{
					VectorCopy( d.color, cur.color );
				}
				else if ( !Q_stricmp( what, "intensity" ) )
				{
					cur.intensity = d.intensity;
				}
				else if ( !Q_stricmp( what, "radius" ) )
				{
					cur.radius = d.radius;
				}
				else
				{
					Q_strncpyz( cur.name, d.name, sizeof( cur.name ) );
				}
			}
			if ( LE_DoSet( s_selList[i], &cur, label ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %d light%s: %s set", done, done > 1 ? "s" : "", what );
	}
}

static void LE_PrintDesc( const char *title, const rtxLightDesc_t *d )
{
	CG_Printf( "%s origin %.1f %.1f %.1f  color %.3f %.3f %.3f  intensity %.2f  radius %.2f  name '%s'\n", title,
		d->origin[0], d->origin[1], d->origin[2], d->color[0], d->color[1], d->color[2],
		d->intensity, d->radius, d->name );
}

static void LE_Cmd_Get( void )
{
	rtxLightDesc_t	d, orig;

	if ( !LE_NeedActive() || !LE_NeedSelection() || !LE_GetDesc( s_sel, &d ) )
	{
		return;
	}
	if ( s_selList.size() > 1 )
	{
		CG_Printf( "%d lights selected; the primary light follows\n", (int)s_selList.size() );
	}
	CG_Printf( "light %d (%s, %s) flags: %s\n", d.id, LE_SourceStr( &d ), d.type == RTX_LTYPE_SPOT ? "spot" : "sphere", LE_FlagsStr( d.flags ) );
	LE_PrintDesc( "  now:     ", &d );
	if ( ( d.flags & RTX_LFLAG_MODIFIED ) && s_api->GetOriginal( s_sel, &orig ) )
	{
		LE_PrintDesc( "  original:", &orig );
	}
}

qboolean CG_LightEdit_ConsoleCommand( const char *cmd )
{
	static const struct {
		const char	*name;
		void		(*func)( void );
	} commands[] = {
		{ "lightedit",		LE_Cmd_LightEdit },
		{ "ledit_save",		LE_Cmd_Save },
		{ "ledit_reload",	LE_Cmd_Reload },
		{ "ledit_undo",		LE_Cmd_Undo },
		{ "ledit_redo",		LE_Cmd_Redo },
		{ "ledit_history",	LE_Cmd_History },
		{ "ledit_delete",	LE_Cmd_Delete },
		{ "ledit_deselect",	LE_Cmd_Deselect },
		{ "ledit_revert",	LE_Cmd_Revert },
		{ "ledit_set",		LE_Cmd_Set },
		{ "ledit_get",		LE_Cmd_Get },
		{ "ledit_grid_next",	LE_Cmd_GridNext },
		{ "ledit_snap_toggle",	LE_Cmd_SnapToggle },
		{ "ledit_xray_toggle",	LE_Cmd_XrayToggle },
	};

	for ( size_t i = 0; i < ARRAY_LEN( commands ); i++ )
	{
		if ( !Q_stricmp( cmd, commands[i].name ) )
		{
			commands[i].func();
			return qtrue;
		}
	}

	if ( s_active )
	{
		if ( !Q_stricmp( cmd, "weapon" ) )
		{
			LE_SetTool( atoi( CG_Argv( 1 ) ) );
			return qtrue;
		}
		if ( !Q_stricmp( cmd, "weapnext" ) )
		{
			s_wheel++;
			return qtrue;
		}
		if ( !Q_stricmp( cmd, "weapprev" ) )
		{
			s_wheel--;
			return qtrue;
		}
	}
	return qfalse;
}

void CG_LightEdit_InitConsoleCommands( void )
{
	static const char *names[] = {
		"lightedit", "ledit_save", "ledit_reload", "ledit_undo", "ledit_redo", "ledit_history",
		"ledit_delete", "ledit_deselect", "ledit_revert", "ledit_set", "ledit_get",
		"ledit_grid_next", "ledit_snap_toggle", "ledit_xray_toggle"
	};

	for ( size_t i = 0; i < ARRAY_LEN( names ); i++ )
	{
		cgi_AddCommand( names[i] );
	}
}

/*
=================
Overlay
=================
*/
static int LE_TextW( const char *s )
{
	return cgi_R_Font_StrLenPixels( s, cgs.media.qhFontSmall, LEDIT_FONT_SCALE, cgs.widthRatioCoef );
}

static int LE_TextH( void )
{
	return cgi_R_Font_HeightPixels( cgs.media.qhFontSmall, LEDIT_FONT_SCALE );
}

static void LE_Text( int x, int y, const char *s, const vec4_t col )
{
	cgi_R_Font_DrawString( x, y, s, col, cgs.media.qhFontSmall, -1, LEDIT_FONT_SCALE, cgs.widthRatioCoef );
}

// Each dot is two render commands. The renderer command buffer is finite: when the overlay
// fills it, the frame drops commands. Keep the dots of one frame under this budget.
#define LEDIT_DOT_BUDGET		2000
#define LEDIT_LINE_MAX_DOTS		64

static int s_dotBudget = LEDIT_DOT_BUDGET;

static void LE_Dot( float x, float y, float size, const vec4_t col )
{
	if ( s_dotBudget <= 0 )
	{
		return;
	}
	s_dotBudget--;
	CG_FillRect( x - size * 0.5f, y - size * 0.5f, size, size, col );
}

// Clips a segment to the virtual screen (Liang-Barsky). Returns qfalse when nothing is left.
static qboolean LE_ClipToScreen( float *x1, float *y1, float *x2, float *y2 )
{
	const float	dx = *x2 - *x1, dy = *y2 - *y1;
	const float	p[4] = { -dx, dx, -dy, dy };
	const float	q[4] = { *x1, 640.0f - *x1, *y1, 480.0f - *y1 };
	float		t0 = 0.0f, t1 = 1.0f;

	for ( int i = 0; i < 4; i++ )
	{
		if ( p[i] == 0.0f )
		{
			if ( q[i] < 0.0f )
			{
				return qfalse;
			}
			continue;
		}
		const float t = q[i] / p[i];
		if ( p[i] < 0.0f )
		{
			t0 = Q_max( t0, t );
		}
		else
		{
			t1 = Q_min( t1, t );
		}
		if ( t0 > t1 )
		{
			return qfalse;
		}
	}
	*x2 = *x1 + t1 * dx;
	*y2 = *y1 + t1 * dy;
	*x1 = *x1 + t0 * dx;
	*y1 = *y1 + t0 * dy;
	return qtrue;
}

// Draws a segment as a row of small dots. Only the visible part is drawn.
void LE_Line( float x1, float y1, float x2, float y2, const vec4_t col )
{
	if ( !LE_ClipToScreen( &x1, &y1, &x2, &y2 ) )
	{
		return;
	}

	const float	dx = x2 - x1, dy = y2 - y1;
	const float	len = sqrtf( dx * dx + dy * dy );
	int			steps = (int)( len / 3.0f ) + 1;

	if ( steps > LEDIT_LINE_MAX_DOTS )
	{
		steps = LEDIT_LINE_MAX_DOTS;
	}
	for ( int i = 0; i <= steps; i++ )
	{
		const float	f = (float)i / (float)steps;

		LE_Dot( x1 + dx * f, y1 + dy * f, 1.5f, col );
	}
}

// A box is four rects: it uses four dots of the budget.
static void LE_Box( float x, float y, float half, const vec4_t col )
{
	if ( s_dotBudget < 4 )
	{
		return;
	}
	s_dotBudget -= 4;
	CG_FillRect( x - half, y - half, half * 2, 1.0f, col );
	CG_FillRect( x - half, y + half - 1.0f, half * 2, 1.0f, col );
	CG_FillRect( x - half, y - half, 1.0f, half * 2, col );
	CG_FillRect( x + half - 1.0f, y - half, 1.0f, half * 2, col );
}

// Draws a circle of 3D points as projected segments.
void LE_Circle3D( const vec3_t center, const vec3_t ax1, const vec3_t ax2, float radius, int points, const vec4_t col )
{
	float		px = 0, py = 0;
	qboolean	havePrev = qfalse;

	for ( int i = 0; i <= points; i++ )
	{
		const float	a = (float)i * ( 2.0f * M_PI / points );
		vec3_t		p;
		float		x, y;

		VectorMA( center, cosf( a ) * radius, ax1, p );
		VectorMA( p, sinf( a ) * radius, ax2, p );
		if ( CG_WorldCoordToScreenCoordFloat( p, &x, &y ) )
		{
			if ( havePrev )
			{
				LE_Line( px, py, x, y, col );
			}
			px = x;
			py = y;
			havePrev = qtrue;
		}
		else
		{
			havePrev = qfalse;
		}
	}
}

static const float *LE_IconColor( const rtxLightDesc_t *d )
{
	if ( d->flags & RTX_LFLAG_DISABLED )
	{
		return colRedDim;
	}
	if ( d->flags & RTX_LFLAG_IN_SOLID )
	{
		return colRed;
	}
	if ( d->flags & RTX_LFLAG_MODIFIED )
	{
		return colOrange;
	}
	if ( d->source == RTX_LSRC_ENTITY )
	{
		return colBlue;
	}
	if ( d->source == RTX_LSRC_LGT )
	{
		return colYellow;
	}
	return colGreen;
}

static void LE_DrawIcons( void )
{
	if ( !ledit_show.integer )
	{
		return;
	}
	for ( size_t i = 0; i < s_recs.size(); i++ )
	{
		const ledRec_t	&r = s_recs[i];

		if ( !r.valid || !r.onScreen || ( r.occluded && !ledit_xray.integer ) )
		{
			continue;
		}
		const float	size = Com_Clamp( 3.0f, 12.0f, 1200.0f / Q_max( r.depth, 1.0f ) );
		const float	*col = LE_IconColor( &r.d );

		LE_Dot( r.sx, r.sy, size + 2.0f, colDark );
		LE_Dot( r.sx, r.sy, size, col );
		if ( r.d.flags & RTX_LFLAG_DISABLED )
		{
			const float	h = size * 0.5f + 2.0f;

			LE_Line( r.sx - h, r.sy - h, r.sx + h, r.sy + h, colRed );
			LE_Line( r.sx - h, r.sy + h, r.sx + h, r.sy - h, colRed );
		}
	}
}

static void LE_DrawSelection( void )
{
	if ( s_pickId >= 0 && s_pickId < (int)s_recs.size() && s_recs[s_pickId].valid && s_recs[s_pickId].onScreen )
	{
		const ledRec_t	&r = s_recs[s_pickId];

		LE_Box( r.sx, r.sy, 9.0f, colWhite );
	}
	if ( s_sel >= 0 && s_sel < (int)s_recs.size() && s_recs[s_sel].valid )
	{
		const ledRec_t	&r = s_recs[s_sel];
		static const vec3_t	ax[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		const float		radius = Q_max( r.d.radius, 8.0f );

		if ( r.onScreen )
		{
			LE_Box( r.sx, r.sy, 13.0f, colWhite );
			LE_Box( r.sx, r.sy, 14.0f, colDark );
		}
		LE_Circle3D( r.d.origin, ax[0], ax[1], radius, 32, colCyan );
		LE_Circle3D( r.d.origin, ax[0], ax[2], radius, 32, colCyan );
		LE_Circle3D( r.d.origin, ax[1], ax[2], radius, 32, colCyan );
	}

	// The other selected lights get a frame only.
	for ( size_t i = 0; i < s_selList.size(); i++ )
	{
		const int	id = s_selList[i];

		if ( id == s_sel || id >= (int)s_recs.size() || !s_recs[id].valid || !s_recs[id].onScreen )
		{
			continue;
		}
		LE_Box( s_recs[id].sx, s_recs[id].sy, 12.0f, colCyan );
	}
}

static void LE_DrawToolWorld( void )
{
	if ( s_tool == LEDIT_TOOL_CREATE )
	{
		LE_Circle3D( s_ghost, cg.refdef.viewaxis[1], cg.refdef.viewaxis[2], 8.0f, 24,
			s_ghostSolid ? colRed : colGreen );
	}
	else if ( s_tool == LEDIT_TOOL_MOVE )
	{
		LE_DrawGrabWorld();
	}
}

static void LE_DrawHelp( void )
{
	const char	*name, *fire, *alt, *wheel;
	char		wheelBuf[96];
	int			y = 24;		// below the renderer name that rd-vulkan draws at the top left
	const int	h = LE_TextH();

	switch ( s_tool )
	{
	case LEDIT_TOOL_CREATE:
		name = "2 Create (sphere)";
		fire = "add a light at the ghost";
		alt = "spots come in the next version";
		Com_sprintf( wheelBuf, sizeof( wheelBuf ), "surface offset x2 or /2, walk +-1 (now %.0f)", s_createOffset );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_MOVE:
		LE_MoveHelp( &name, &fire, &alt, &wheel );
		break;
	case LEDIT_TOOL_CLONE:
		LE_CloneHelp( &name, &fire, &alt, wheelBuf, sizeof( wheelBuf ) );
		wheel = wheelBuf;
		break;
	case LEDIT_TOOL_DELETE:
		name = "8 Delete / restore";
		fire = "delete the aimed light, or the selection if it is a member";
		alt = "restore the aimed light";
		wheel = "none";
		break;
	default:
		name = "1 Select";
		fire = "select the aimed light (replaces the selection; none aimed: clear)";
		alt = "add the aimed light to the selection, or remove it";
		wheel = "cycle among the lights under the crosshair";
		break;
	}
	LE_Text( 6, y, va( "LIGHT EDIT  tool %s   (keys 1 2 3 7 8)", name ), colWhite );
	y += h + 2;
	LE_Text( 6, y, va( "Fire: %s", fire ), colGrey );
	y += h;
	LE_Text( 6, y, va( "Alt: %s", alt ), colGrey );
	y += h;
	LE_Text( 6, y, va( "Wheel: %s", wheel ), colGrey );
	y += h;
	if ( cg.time < s_msgEnd )
	{
		LE_Text( 6, y + 4, s_msg, colWhite );
	}
}

#define LE_PANEL_LINES	10

// Draws the selection panel on the right.
static void LE_DrawPanel( void )
{
	if ( s_sel < 0 || s_sel >= (int)s_recs.size() || !s_recs[s_sel].valid )
	{
		return;
	}
	const rtxLightDesc_t	&d = s_recs[s_sel].d;
	rtxLightDesc_t			orig;
	const qboolean			multi = (qboolean)( s_selList.size() > 1 );
	const qboolean			haveOrig = (qboolean)( !multi && ( d.flags & RTX_LFLAG_MODIFIED ) && s_api->GetOriginal( s_sel, &orig ) );
	char					main[LE_PANEL_LINES][128];
	char					was[LE_PANEL_LINES][128];
	const int				h = LE_TextH();
	int						n = 0, maxw = 0, y;

	memset( main, 0, sizeof( main ) );
	memset( was, 0, sizeof( was ) );

	if ( multi )
	{
		Com_sprintf( main[n++], sizeof( main[0] ), "%d lights (primary %d)", (int)s_selList.size(), d.id );
	}
	else
	{
		Com_sprintf( main[n++], sizeof( main[0] ), "light %d", d.id );
	}
	Com_sprintf( main[n++], sizeof( main[0] ), "source: %s", LE_SourceStr( &d ) );
	Com_sprintf( main[n++], sizeof( main[0] ), "type: %s", d.type == RTX_LTYPE_SPOT ? "spot" : "sphere" );
	Com_sprintf( main[n++], sizeof( main[0] ), "flags: %s", d.flags ? LE_FlagsStr( d.flags ) : "-" );

	Com_sprintf( main[n], sizeof( main[0] ), "origin: %.1f %.1f %.1f", d.origin[0], d.origin[1], d.origin[2] );
	if ( haveOrig && !VectorCompare( d.origin, orig.origin ) )
	{
		Com_sprintf( was[n], sizeof( was[0] ), "%.1f %.1f %.1f", orig.origin[0], orig.origin[1], orig.origin[2] );
	}
	n++;
	Com_sprintf( main[n], sizeof( main[0] ), "color: %.3f %.3f %.3f", d.color[0], d.color[1], d.color[2] );
	if ( haveOrig && !VectorCompare( d.color, orig.color ) )
	{
		Com_sprintf( was[n], sizeof( was[0] ), "%.3f %.3f %.3f", orig.color[0], orig.color[1], orig.color[2] );
	}
	n++;
	Com_sprintf( main[n], sizeof( main[0] ), "intensity: %.2f", d.intensity );
	if ( haveOrig && d.intensity != orig.intensity )
	{
		Com_sprintf( was[n], sizeof( was[0] ), "%.2f", orig.intensity );
	}
	n++;
	Com_sprintf( main[n], sizeof( main[0] ), "emitter radius: %.2f", d.radius );
	if ( haveOrig && d.radius != orig.radius )
	{
		Com_sprintf( was[n], sizeof( was[0] ), "%.2f", orig.radius );
	}
	n++;
	Com_sprintf( main[n], sizeof( main[0] ), "name: %s", d.name );
	if ( haveOrig && strncmp( d.name, orig.name, RTX_LIGHTEDIT_NAME_LEN ) )
	{
		Com_sprintf( was[n], sizeof( was[0] ), "%s", orig.name );
	}
	n++;

	if ( multi )
	{
		// A property that differs between the selected lights shows "-".
		qboolean	same[9];

		for ( int i = 0; i < 9; i++ )
		{
			same[i] = qtrue;
		}
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			const int	id = s_selList[i];

			if ( id == s_sel || id >= (int)s_recs.size() || !s_recs[id].valid )
			{
				continue;
			}

			const rtxLightDesc_t	&o = s_recs[id].d;

			same[1] = (qboolean)( same[1] && o.source == d.source );
			same[2] = (qboolean)( same[2] && o.type == d.type );
			same[3] = (qboolean)( same[3] && o.flags == d.flags );
			same[4] = (qboolean)( same[4] && VectorCompare( o.origin, d.origin ) );
			same[5] = (qboolean)( same[5] && VectorCompare( o.color, d.color ) );
			same[6] = (qboolean)( same[6] && o.intensity == d.intensity );
			same[7] = (qboolean)( same[7] && o.radius == d.radius );
			same[8] = (qboolean)( same[8] && !strncmp( o.name, d.name, RTX_LIGHTEDIT_NAME_LEN ) );
		}
		for ( int i = 1; i < 9; i++ )
		{
			if ( !same[i] )
			{
				char	*dash = strchr( main[i], ':' );

				if ( dash )
				{
					Q_strncpyz( dash + 1, " -", (int)( sizeof( main[0] ) - ( dash + 1 - main[i] ) ) );
				}
			}
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		int	w = LE_TextW( main[i] );

		if ( was[i][0] )
		{
			w += LE_TextW( was[i] ) + 10;
		}
		maxw = Q_max( maxw, w );
	}
	const int	x0 = 640 - 8 - maxw;

	y = 90;
	CG_FillRect( x0 - 4, y - 3, maxw + 10, n * h + 6, colPanel );
	for ( int i = 0; i < n; i++ )
	{
		int	xr = 640 - 8;

		if ( was[i][0] )
		{
			xr -= LE_TextW( was[i] );
			LE_Text( xr, y, was[i], colGrey );
			xr -= 10;
		}
		LE_Text( xr - LE_TextW( main[i] ), y, main[i], colWhite );
		y += h;
	}
}

static void LE_DrawBottom( void )
{
	const int	h = LE_TextH();
	int			y = 480 - h - 6;
	const char	*line;
	char		warn[96];
	char		full[256];

	if ( s_stats.overfullClusters > 0 || s_stats.numInSolid > 0 )
	{
		Com_sprintf( warn, sizeof( warn ), "%s", s_stats.overfullClusters > 0 ? "cluster full  " : "" );
		if ( s_stats.numInSolid > 0 )
		{
			Q_strcat( warn, sizeof( warn ), va( "%d lights in solid", s_stats.numInSolid ) );
		}
		LE_Text( 6, y, warn, colRed );
		y -= h;
	}
	line = va( "%s | entity %d lgt %d added %d modified %d disabled %d | slots %d/%d | undo %d redo %d%s",
		s_stats.mapName, s_stats.numEntity, s_stats.numLgt, s_stats.numAdded, s_stats.numModified,
		s_stats.numDisabled, s_stats.numLightPolys, s_stats.maxLightPolys, LE_UndoDepth(), LE_RedoDepth(),
		s_stats.unsavedChanges > 0 ? " *" : "" );
	Com_sprintf( full, sizeof( full ), "%s | grid %d snap %s", line, LE_GridSize(), LE_SnapOn() ? "on" : "off" );
	LE_Text( 6, y, full, colWhite );
}

qboolean CG_LightEdit_Draw2D( void )
{
	if ( !s_active || !s_api )
	{
		return qfalse;
	}

	// The selection and the tool shapes come first: the icons must not use up their budget.
	s_dotBudget = LEDIT_DOT_BUDGET;
	LE_DrawSelection();
	LE_DrawToolWorld();
	LE_DrawIcons();

	// crosshair
	CG_FillRect( 320 - 8, 240, 5, 1, colWhite );
	CG_FillRect( 320 + 4, 240, 5, 1, colWhite );
	CG_FillRect( 320, 240 - 8, 1, 5, colWhite );
	CG_FillRect( 320, 240 + 4, 1, 5, colWhite );

	LE_DrawHelp();
	LE_DrawPanel();
	LE_DrawBottom();
	CG_DrawCenterString();
	return qtrue;
}
