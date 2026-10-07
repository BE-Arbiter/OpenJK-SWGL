// Light edit mode of the RTX path tracer: cgame side.
// Tools, picking, drag, undo/redo, console commands and the 2D overlay.
// The renderer API is in rd-common/rtx_light_edit_api.h.

#include "cg_headers.h"
#include "cg_media.h"
#include "../rd-common/rtx_light_edit_api.h"
#include "../game/g_lightedit.h"
#include "cg_lightedit.h"

#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <stdio.h>

extern qboolean CG_WorldCoordToScreenCoordFloat( vec3_t worldCoord, float *x, float *y );

#define LEDIT_UNDO_DEPTH		256
#define LEDIT_PICK_MIN_RADIUS	8.0f
#define LEDIT_PICK_SCALE		0.015f
#define LEDIT_OCCLUDE_RANGE		3000.0f
#define LEDIT_MAX_TRACES		1024
#define LEDIT_FONT_SCALE		0.7f
#define LEDIT_MSG_TIME			4000

#define LEDIT_TOOL_SELECT		1
#define LEDIT_TOOL_CREATE		2
#define LEDIT_TOOL_MOVE			3
#define LEDIT_TOOL_DELETE		8

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

typedef enum {
	LEDU_SET = 0,
	LEDU_ADD,
	LEDU_REMOVE,
	LEDU_RESTORE
} ledUndoKind_t;

typedef struct {
	int				kind;
	int				id;
	rtxLightDesc_t	before;
	rtxLightDesc_t	after;
	char			label[48];
} ledUndo_t;

typedef struct {
	qboolean		active;
	int				id;
	float			dist;
	vec3_t			offset;			// light origin minus the hit point
	vec3_t			lastValid;
	vec3_t			target;
	qboolean		targetBad;
	rtxLightDesc_t	before;
} ledGrab_t;

static rtxLightEditAPI_t	*s_api = NULL;
static qboolean				s_active = qfalse;		// cgame side of the mode
static qboolean				s_leaving = qfalse;		// leave requested, game not yet inactive
static int					s_tool = LEDIT_TOOL_SELECT;
static int					s_sel = -1;
static int					s_pickId = -1;
static float				s_pickT = 0.0f;
static int					s_cycle = 0;
static int					s_cycleBase = -1;
static int					s_wheel = 0;			// wheel steps since the last frame
static int					s_prevButtons = 0;
static float				s_createOffset = 16.0f;
static vec3_t				s_ghost;
static qboolean				s_ghostSolid = qfalse;
static ledGrab_t			s_grab;
static rtxLightStats_t		s_stats;

static std::vector<ledRec_t>		s_recs;
static std::vector<ledHit_t>		s_hits;
static std::vector<unsigned char>	s_occ;			// last occlusion result per id

static ledUndo_t	s_undo[LEDIT_UNDO_DEPTH];
static int			s_numUndo = 0;
static ledUndo_t	s_redo[LEDIT_UNDO_DEPTH];
static int			s_numRedo = 0;

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
static void LE_Msg( const char *fmt, ... )
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

static qboolean LE_GetDesc( int id, rtxLightDesc_t *d )
{
	memset( d, 0, sizeof( *d ) );
	return s_api->Get( id, d );
}

// Compares the fields that Set copies.
static qboolean LE_DescEqual( const rtxLightDesc_t *a, const rtxLightDesc_t *b )
{
	return (qboolean)( VectorCompare( a->origin, b->origin ) && VectorCompare( a->color, b->color )
		&& a->intensity == b->intensity && a->radius == b->radius
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
Undo / redo
=================
*/
static void LE_UndoClear( void )
{
	s_numUndo = 0;
	s_numRedo = 0;
}

static void LE_UndoPush( int kind, int id, const rtxLightDesc_t *before, const rtxLightDesc_t *after, const char *label )
{
	ledUndo_t	*e;

	if ( s_numUndo == LEDIT_UNDO_DEPTH )
	{
		memmove( &s_undo[0], &s_undo[1], sizeof( s_undo[0] ) * ( LEDIT_UNDO_DEPTH - 1 ) );
		s_numUndo--;
	}
	e = &s_undo[s_numUndo++];
	memset( e, 0, sizeof( *e ) );
	e->kind = kind;
	e->id = id;
	if ( before )
	{
		e->before = *before;
	}
	if ( after )
	{
		e->after = *after;
	}
	Q_strncpyz( e->label, label, sizeof( e->label ) );
	s_numRedo = 0;
}

static qboolean LE_UndoApply( const ledUndo_t *e, qboolean undo )
{
	switch ( e->kind )
	{
	case LEDU_SET:
		return s_api->Set( e->id, undo ? &e->before : &e->after );
	case LEDU_ADD:
		return undo ? s_api->Remove( e->id ) : s_api->Restore( e->id );
	case LEDU_REMOVE:
		return undo ? s_api->Restore( e->id ) : s_api->Remove( e->id );
	case LEDU_RESTORE:
		return undo ? s_api->Remove( e->id ) : s_api->Restore( e->id );
	}
	return qfalse;
}

static void LE_Undo( void )
{
	ledUndo_t	e;

	if ( s_grab.active )
	{
		LE_Msg( "light edit: release the light first" );
		return;
	}
	if ( s_numUndo == 0 )
	{
		LE_Msg( "light edit: nothing to undo" );
		return;
	}
	e = s_undo[s_numUndo - 1];
	if ( !LE_UndoApply( &e, qtrue ) )
	{
		LE_Msg( "light edit: undo failed (%s)", s_api->LastError() );
		return;
	}
	s_numUndo--;
	s_redo[s_numRedo++] = e;
	s_sel = ( e.kind == LEDU_ADD || e.kind == LEDU_RESTORE ) ? -1 : e.id;
	LE_Msg( "light edit: undo %s (light %d)", e.label, e.id );
}

static void LE_Redo( void )
{
	ledUndo_t	e;

	if ( s_grab.active )
	{
		LE_Msg( "light edit: release the light first" );
		return;
	}
	if ( s_numRedo == 0 )
	{
		LE_Msg( "light edit: nothing to redo" );
		return;
	}
	e = s_redo[s_numRedo - 1];
	if ( !LE_UndoApply( &e, qfalse ) )
	{
		LE_Msg( "light edit: redo failed (%s)", s_api->LastError() );
		return;
	}
	s_numRedo--;
	s_undo[s_numUndo++] = e;
	s_sel = ( e.kind == LEDU_REMOVE ) ? -1 : e.id;
	LE_Msg( "light edit: redo %s (light %d)", e.label, e.id );
}

/*
=================
Actions shared by tools and commands
=================
*/
// Remove on a light, one undo entry.
static void LE_DoRemove( int id )
{
	rtxLightDesc_t	before, after;

	if ( !LE_GetDesc( id, &before ) )
	{
		return;
	}
	if ( before.flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED ) )
	{
		LE_Msg( "light edit: light %d is already removed", id );
		return;
	}
	if ( !s_api->Remove( id ) )
	{
		LE_Msg( "light edit: remove failed (%s)", s_api->LastError() );
		return;
	}
	LE_GetDesc( id, &after );
	LE_UndoPush( LEDU_REMOVE, id, &before, &after, "remove" );
	if ( s_sel == id && ( after.flags & RTX_LFLAG_DELETED ) )
	{
		s_sel = -1;
	}
	LE_Msg( "light edit: removed light %d", id );
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
static qboolean LE_DoSet( int id, const rtxLightDesc_t *want, const char *label )
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

static void LE_EndGrab( void )
{
	rtxLightDesc_t	after;

	if ( !s_grab.active )
	{
		return;
	}
	s_grab.active = qfalse;
	s_api->EndDrag( s_grab.id );
	if ( LE_GetDesc( s_grab.id, &after ) && !LE_DescEqual( &s_grab.before, &after ) )
	{
		LE_UndoPush( LEDU_SET, s_grab.id, &s_grab.before, &after, "move" );
	}
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
	s_grab.active = qfalse;
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
	s_sel = -1;
	s_pickId = -1;
	s_cycle = 0;
	s_cycleBase = -1;
	s_wheel = 0;
	memset( &s_grab, 0, sizeof( s_grab ) );
	s_occ.clear();
	LE_ReadButtons( &s_prevButtons );
}

static void LE_SetTool( int tool )
{
	if ( tool != LEDIT_TOOL_SELECT && tool != LEDIT_TOOL_CREATE && tool != LEDIT_TOOL_MOVE && tool != LEDIT_TOOL_DELETE )
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

	if ( s_sel >= n || ( s_sel >= 0 && !s_recs[s_sel].valid ) )
	{
		s_sel = -1;
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
		s_sel = s_pickId;
	}
	if ( secDown )
	{
		s_sel = -1;
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
		s_sel = id;
		LE_Msg( "light edit: added light %d", id );
	}
}

static void LE_BeginGrab( void )
{
	const ledRec_t	&r = s_recs[s_pickId];
	vec3_t			hitPoint;

	s_grab.active = qtrue;
	s_grab.id = s_pickId;
	s_grab.dist = s_pickT;
	VectorMA( cg.refdef.vieworg, s_pickT, cg.refdef.viewaxis[0], hitPoint );
	VectorSubtract( r.d.origin, hitPoint, s_grab.offset );
	VectorCopy( r.d.origin, s_grab.lastValid );
	VectorCopy( r.d.origin, s_grab.target );
	s_grab.targetBad = qfalse;
	s_grab.before = r.d;
	s_sel = s_pickId;
	s_api->BeginDrag( s_grab.id );
}

static void LE_ToolMove( qboolean prim, qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	if ( s_grab.active )
	{
		rtxLightDesc_t	cur;

		for ( int i = 0; i < abs( wheel ); i++ )
		{
			const float	f = fine ? 1.01f : 1.1f;

			s_grab.dist = ( wheel > 0 ) ? s_grab.dist * f : s_grab.dist / f;
			s_grab.dist = Com_Clamp( 16.0f, 8192.0f, s_grab.dist );
		}
		if ( !prim )
		{
			LE_EndGrab();
			return;
		}
		VectorMA( cg.refdef.vieworg, s_grab.dist, cg.refdef.viewaxis[0], s_grab.target );
		VectorAdd( s_grab.target, s_grab.offset, s_grab.target );
		if ( s_api->PointInSolid( s_grab.target ) )
		{
			s_grab.targetBad = qtrue;
			return;
		}
		s_grab.targetBad = qfalse;
		VectorCopy( s_grab.target, s_grab.lastValid );
		if ( LE_GetDesc( s_grab.id, &cur ) && !VectorCompare( cur.origin, s_grab.target ) )
		{
			VectorCopy( s_grab.target, cur.origin );
			s_api->Set( s_grab.id, &cur );
		}
		return;
	}

	if ( priDown && s_pickId >= 0 )
	{
		LE_BeginGrab();
	}
	else if ( secDown )
	{
		const int	id = ( s_pickId >= 0 ) ? s_pickId : s_sel;
		rtxLightDesc_t	orig, cur;

		if ( id < 0 )
		{
			return;
		}
		if ( !s_api->GetOriginal( id, &orig ) || !LE_GetDesc( id, &cur ) )
		{
			LE_Msg( "light edit: no original position for light %d", id );
			return;
		}
		if ( VectorCompare( cur.origin, orig.origin ) )
		{
			LE_Msg( "light edit: light %d is already at its original position", id );
			return;
		}
		VectorCopy( orig.origin, cur.origin );
		if ( LE_DoSet( id, &cur, "reset position" ) )
		{
			LE_Msg( "light edit: light %d back to its original position", id );
		}
	}
}

static void LE_ToolDelete( qboolean priDown, qboolean secDown )
{
	if ( priDown )
	{
		const int	id = ( s_pickId >= 0 ) ? s_pickId : s_sel;

		if ( id >= 0 )
		{
			LE_DoRemove( id );
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
	if ( s_grab.active )
	{
		s_pickId = s_grab.id;	// the grabbed light stays the picked one
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
	s_sel = -1;
	s_pickId = -1;
	s_cycle = 0;
	s_cycleBase = -1;
	s_wheel = 0;
	s_prevButtons = 0;
	s_createOffset = 16.0f;
	s_ghostSolid = qfalse;
	memset( &s_grab, 0, sizeof( s_grab ) );
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
			s_sel = -1;
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
		s_sel = -1;
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
	if ( !LE_NeedActive() )
	{
		return;
	}
	CG_Printf( "light edit: %d undo, %d redo\n", s_numUndo, s_numRedo );
	for ( int i = s_numUndo - 1; i >= 0; i-- )
	{
		CG_Printf( "  undo %3d: %s (light %d)\n", s_numUndo - i, s_undo[i].label, s_undo[i].id );
	}
	for ( int i = s_numRedo - 1; i >= 0; i-- )
	{
		CG_Printf( "  redo %3d: %s (light %d)\n", s_numRedo - i, s_redo[i].label, s_redo[i].id );
	}
}

static void LE_Cmd_Delete( void )
{
	if ( LE_NeedActive() && LE_NeedSelection() )
	{
		LE_DoRemove( s_sel );
	}
}

static void LE_Cmd_Deselect( void )
{
	if ( LE_NeedActive() )
	{
		s_sel = -1;
		LE_Msg( "light edit: selection cleared" );
	}
}

static void LE_Cmd_Revert( void )
{
	rtxLightDesc_t	before, after;

	if ( !LE_NeedActive() || !LE_NeedSelection() )
	{
		return;
	}
	if ( !LE_GetDesc( s_sel, &before ) )
	{
		return;
	}
	if ( !s_api->Revert( s_sel ) )
	{
		LE_Msg( "light edit: revert failed (%s)", s_api->LastError() );
		return;
	}
	LE_GetDesc( s_sel, &after );
	if ( !LE_DescEqual( &before, &after ) )
	{
		LE_UndoPush( LEDU_SET, s_sel, &before, &after, "revert" );
	}
	LE_Msg( "light edit: light %d reverted", s_sel );
}

static void LE_Cmd_Set( void )
{
	rtxLightDesc_t	d;
	char			what[32];
	const int		argc = cgi_Argc();

	Q_strncpyz( what, CG_Argv( 1 ), sizeof( what ) );
	char			label[48];

	if ( !LE_NeedActive() || !LE_NeedSelection() )
	{
		return;
	}
	if ( !LE_GetDesc( s_sel, &d ) )
	{
		return;
	}
	if ( !Q_stricmp( what, "origin" ) && argc >= 5 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			d.origin[i] = atof( CG_Argv( 2 + i ) );
		}
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
	if ( LE_DoSet( s_sel, &d, label ) )
	{
		LE_Msg( "light edit: light %d %s set", s_sel, what );
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
		"ledit_delete", "ledit_deselect", "ledit_revert", "ledit_set", "ledit_get"
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
static void LE_Line( float x1, float y1, float x2, float y2, const vec4_t col )
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

static void LE_Box( float x, float y, float half, const vec4_t col )
{
	CG_FillRect( x - half, y - half, half * 2, 1.0f, col );
	CG_FillRect( x - half, y + half - 1.0f, half * 2, 1.0f, col );
	CG_FillRect( x - half, y - half, 1.0f, half * 2, col );
	CG_FillRect( x + half - 1.0f, y - half, 1.0f, half * 2, col );
}

// Draws a circle of 3D points as projected segments.
static void LE_Circle3D( const vec3_t center, const vec3_t ax1, const vec3_t ax2, float radius, int points, const vec4_t col )
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
}

static void LE_DrawToolWorld( void )
{
	if ( s_tool == LEDIT_TOOL_CREATE )
	{
		LE_Circle3D( s_ghost, cg.refdef.viewaxis[1], cg.refdef.viewaxis[2], 8.0f, 24,
			s_ghostSolid ? colRed : colGreen );
	}
	else if ( s_tool == LEDIT_TOOL_MOVE && s_grab.active && s_grab.targetBad )
	{
		float	x1, y1, x2, y2;

		LE_Circle3D( s_grab.target, cg.refdef.viewaxis[1], cg.refdef.viewaxis[2], 8.0f, 24, colRed );
		if ( CG_WorldCoordToScreenCoordFloat( s_grab.lastValid, &x1, &y1 )
			&& CG_WorldCoordToScreenCoordFloat( s_grab.target, &x2, &y2 ) )
		{
			LE_Line( x1, y1, x2, y2, colRed );
		}
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
		name = "3 Move";
		fire = "hold to grab and drag a light";
		alt = "put a light back to its original position";
		wheel = "distance x1.1 while grabbing, walk x1.01";
		break;
	case LEDIT_TOOL_DELETE:
		name = "8 Delete / restore";
		fire = "delete the aimed or selected light";
		alt = "restore the aimed light";
		wheel = "none";
		break;
	default:
		name = "1 Select";
		fire = "select the aimed light";
		alt = "deselect";
		wheel = "cycle among the lights under the crosshair";
		break;
	}
	LE_Text( 6, y, va( "LIGHT EDIT  tool %s   (keys 1 2 3 8)", name ), colWhite );
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
	const qboolean			haveOrig = (qboolean)( ( d.flags & RTX_LFLAG_MODIFIED ) && s_api->GetOriginal( s_sel, &orig ) );
	char					main[LE_PANEL_LINES][128];
	char					was[LE_PANEL_LINES][128];
	const int				h = LE_TextH();
	int						n = 0, maxw = 0, y;

	memset( main, 0, sizeof( main ) );
	memset( was, 0, sizeof( was ) );

	Com_sprintf( main[n++], sizeof( main[0] ), "light %d", d.id );
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
		s_stats.numDisabled, s_stats.numLightPolys, s_stats.maxLightPolys, s_numUndo, s_numRedo,
		s_stats.unsavedChanges > 0 ? " *" : "" );
	LE_Text( 6, y, line, colWhite );
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
