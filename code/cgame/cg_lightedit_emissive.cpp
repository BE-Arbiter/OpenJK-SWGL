// Light edit mode: emissive multiplier per shader. Tool 5 picks the shader of an emissive dot
// and scales the light that its polygons cast. The commands ledit_emissive* do the same by name.

#include "cg_headers.h"
#include "cg_lightedit.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <algorithm>
#include <math.h>

#define LEDIT_EMI_PICK_RADIUS	14.0f
#define LEDIT_EMI_PICK_RANGE	2048.0f
#define LEDIT_EMI_HILITE_RANGE	4096.0f
#define LEDIT_EMI_HILITE_MAX	300
#define LEDIT_EMI_SCALE_MAX		100.0f
#define LEDIT_EMI_STEP			1.1f
#define LEDIT_EMI_STEP_FINE		1.01f
#define LEDIT_EMI_MIN_UP		0.01f		// a scale of 0 steps up from here
#define LEDIT_EMI_NAME_LEN		96

static int					s_emiSel = -1;			// selected shader, -1 for none
static std::vector<int>		s_shaderOf;				// shader of each cached emissive point
static int					s_shaderOfCount = -1;

static const vec4_t	colHilite	= { 1.00f, 0.90f, 0.20f, 0.90f };
static const vec4_t	colPanelBg	= { 0.00f, 0.00f, 0.00f, 0.45f };
static const vec4_t	colTxt		= { 1.00f, 1.00f, 1.00f, 1.0f };
static const vec4_t	colTxtYel	= { 1.00f, 0.90f, 0.20f, 1.0f };

void LE_EmissiveInit( void )
{
	s_emiSel = -1;
	s_shaderOf.clear();
	s_shaderOfCount = -1;
}

typedef struct {
	float	d;
	int		i;
} ledEmiNear_t;

static bool LE_EmiNearLess( const ledEmiNear_t &a, const ledEmiNear_t &b )
{
	return a.d < b.d;
}

// Fills the shader of each cached point. It runs again after a map load.
static void LE_EmiShaderCache( void )
{
	const int	n = LE_EmissivePointCount();

	if ( n == s_shaderOfCount && (int)s_shaderOf.size() == n )
	{
		return;
	}
	s_shaderOf.assign( n, -1 );
	for ( int i = 0; i < n; i++ )
	{
		int	idx;

		LE_EmissivePoint( i, &idx );
		s_shaderOf[i] = s_api->EmissiveShaderOf( idx );
	}
	s_shaderOfCount = n;
}

static qboolean LE_EmiReady( void )
{
	return (qboolean)( s_api && s_api->CountEmissiveShaders && s_api->SetEmissiveScale );
}

static qboolean LE_EmiTraceBlocked( const vec3_t eye, const vec3_t p )
{
	trace_t	tr;
	vec3_t	rel;

	CG_Trace( &tr, eye, vec3_origin, vec3_origin, p, cg.snap->ps.clientNum, CONTENTS_SOLID );
	if ( tr.startsolid || tr.allsolid )
	{
		return qfalse;
	}
	VectorSubtract( p, eye, rel );
	return (qboolean)( tr.fraction < 1.0f && VectorLength( rel ) * ( 1.0f - tr.fraction ) > 4.0f );
}

static qboolean LE_EmiInfo( int shader, char *name, int nameSize, float *scale, int *polys )
{
	char	tmp[LEDIT_EMI_NAME_LEN];
	float	sc = 1.0f;
	int		np = 0;

	if ( !name )
	{
		name = tmp;
		nameSize = sizeof( tmp );
	}
	name[0] = 0;
	if ( !LE_EmiReady() || !s_api->GetEmissiveShader( shader, name, nameSize, &sc, &np ) )
	{
		return qfalse;
	}
	if ( scale )
	{
		*scale = sc;
	}
	if ( polys )
	{
		*polys = np;
	}
	return qtrue;
}

// Sets the scale and records one undo entry. The caller opens the undo group.
static qboolean LE_EmiSetScale( int shader, float scale, const char *label )
{
	float	before;

	scale = Com_Clamp( 0.0f, LEDIT_EMI_SCALE_MAX, scale );
	if ( !LE_EmiInfo( shader, NULL, 0, &before, NULL ) )
	{
		return qfalse;
	}
	if ( !s_api->SetEmissiveScale( shader, scale ) )
	{
		LE_Msg( "light edit: emissive scale refused (%s)", s_api->LastError() );
		return qfalse;
	}
	LE_UndoPushScale( shader, before, scale, label );
	return qtrue;
}

/*
=================
Tool 5
=================
*/
// Primary fire in tool 5: the dot nearest to the crosshair gives the selected shader.
qboolean LE_EmissivePick( void )
{
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];
	std::vector<ledEmiNear_t>	cand;

	if ( !LE_EmiReady() || LE_ShowMode() < 2 )
	{
		return qfalse;
	}
	const int	n = LE_EmissivePointCount();

	LE_EmiShaderCache();
	for ( int i = 0; i < n; i++ )
	{
		int			idx;
		vec3_t		p, rel;
		float		sx, sy, dx, dy;
		ledEmiNear_t	e;

		VectorCopy( LE_EmissivePoint( i, &idx ), p );
		VectorSubtract( p, eye, rel );
		if ( s_shaderOf[i] < 0 || DotProduct( rel, fwd ) <= 0.0f || VectorLength( rel ) > LEDIT_EMI_PICK_RANGE
			|| !CG_WorldCoordToScreenCoordFloat( p, &sx, &sy ) )
		{
			continue;
		}
		dx = sx - 320.0f;
		dy = sy - 240.0f;
		e.d = dx * dx + dy * dy;
		e.i = i;
		if ( e.d <= LEDIT_EMI_PICK_RADIUS * LEDIT_EMI_PICK_RADIUS )
		{
			cand.push_back( e );
		}
	}
	std::sort( cand.begin(), cand.end(), LE_EmiNearLess );
	for ( size_t k = 0; k < cand.size(); k++ )
	{
		int		idx;
		vec3_t	p;

		VectorCopy( LE_EmissivePoint( cand[k].i, &idx ), p );
		if ( !LE_XrayOn() && LE_EmiTraceBlocked( eye, p ) )
		{
			continue;
		}
		char	name[LEDIT_EMI_NAME_LEN];
		float	scale;
		int		polys;

		s_emiSel = s_shaderOf[cand[k].i];
		if ( LE_EmiInfo( s_emiSel, name, sizeof( name ), &scale, &polys ) )
		{
			LE_Msg( "light edit: emissive shader %d %s, %d polygons, scale %g", s_emiSel, name, polys, scale );
		}
		return qtrue;
	}
	return qfalse;
}

static float LE_EmiStep( float scale, int wheel, qboolean fine )
{
	const float	step = powf( fine ? LEDIT_EMI_STEP_FINE : LEDIT_EMI_STEP, (float)wheel );
	float		out = ( scale < LEDIT_EMI_MIN_UP && wheel > 0 ? LEDIT_EMI_MIN_UP : scale ) * step;

	// The scale stops at 1 when a step crosses it.
	if ( ( scale - 1.0f ) * ( out - 1.0f ) < 0.0f )
	{
		out = 1.0f;
	}
	return Com_Clamp( 0.0f, LEDIT_EMI_SCALE_MAX, out );
}

// Wheel and secondary fire act on the selected shader. Returns qtrue when the selection took the action.
qboolean LE_EmissiveTool( qboolean secDown, int wheel, qboolean fine )
{
	float	scale;

	if ( s_emiSel < 0 || !LE_EmiInfo( s_emiSel, NULL, 0, &scale, NULL ) )
	{
		return qfalse;
	}
	if ( wheel )
	{
		const float	want = LE_EmiStep( scale, wheel, fine );

		if ( want != scale )
		{
			std::vector<int>	key( 1, s_emiSel );

			LE_UndoBeginMerge( "emissive scale", LE_HashTargets( 0x60, key ) );
			if ( LE_EmiSetScale( s_emiSel, want, "emissive scale" ) )
			{
				scale = want;
			}
			LE_UndoEnd();
		}
		return qtrue;
	}
	if ( secDown )
	{
		if ( scale != 1.0f )
		{
			LE_UndoBegin( "emissive reset" );
			LE_EmiSetScale( s_emiSel, 1.0f, "emissive reset" );
			LE_UndoEnd();
		}
		LE_Msg( "light edit: emissive shader %d scale reset to 1", s_emiSel );
		return qtrue;
	}
	return qfalse;
}

// Help texts of tool 5 while the emissive selection is in use.
qboolean LE_EmissiveHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	if ( s_emiSel < 0 || !s_selList.empty() || s_pickId >= 0 )
	{
		return qfalse;
	}
	*name = "5 Properties [emissive]";
	*fire = "select the shader of the emissive dot under the crosshair";
	*alt = "reset the scale of the selected shader to 1";
	Com_sprintf( wheelBuf, wheelSize, "scale of the selected shader x1.1 per notch, walk x1.01 (0..%g)", LEDIT_EMI_SCALE_MAX );
	return qtrue;
}

// Yellow boxes on the dots of the selected shader. The nearest ones only.
void LE_EmissiveDrawHighlight( void )
{
	if ( s_emiSel < 0 || s_tool != LEDIT_TOOL_PROPS || !LE_EmiReady() )
	{
		return;
	}
	const float	*eye = cg.refdef.vieworg;
	const float	*fwd = cg.refdef.viewaxis[0];
	const int	n = LE_EmissivePointCount();
	std::vector<ledEmiNear_t>	cand;

	LE_EmiShaderCache();
	for ( int i = 0; i < n; i++ )
	{
		vec3_t		rel;
		int			idx;
		ledEmiNear_t	e;

		if ( s_shaderOf[i] != s_emiSel )
		{
			continue;
		}
		VectorSubtract( LE_EmissivePoint( i, &idx ), eye, rel );
		e.d = DotProduct( rel, rel );
		if ( e.d <= LEDIT_EMI_HILITE_RANGE * LEDIT_EMI_HILITE_RANGE && DotProduct( rel, fwd ) > 0.0f )
		{
			e.i = i;
			cand.push_back( e );
		}
	}
	if ( (int)cand.size() > LEDIT_EMI_HILITE_MAX )
	{
		std::nth_element( cand.begin(), cand.begin() + LEDIT_EMI_HILITE_MAX, cand.end(), LE_EmiNearLess );
		cand.resize( LEDIT_EMI_HILITE_MAX );
	}
	for ( size_t k = 0; k < cand.size() && LE_DotsLeft() > 0; k++ )
	{
		int		idx;
		vec3_t	p;
		float	sx, sy;

		VectorCopy( LE_EmissivePoint( cand[k].i, &idx ), p );
		if ( CG_WorldCoordToScreenCoordFloat( p, &sx, &sy ) && sx > -4 && sx < 644 && sy > -4 && sy < 484 )
		{
			LE_Box( sx, sy, 5.0f, colHilite );
		}
	}
}

// Panel on the right when a shader is selected and no light is. y is the top of the panel.
qboolean LE_EmissiveDrawPanel( int y )
{
	char	name[LEDIT_EMI_NAME_LEN], shown[44], lines[3][64];
	float	scale;
	int		polys;

	if ( s_emiSel < 0 || s_tool != LEDIT_TOOL_PROPS || !LE_EmiInfo( s_emiSel, name, sizeof( name ), &scale, &polys ) )
	{
		return qfalse;
	}
	const int	len = (int)strlen( name );

	if ( len > 40 )
	{
		Com_sprintf( shown, sizeof( shown ), "~%s", name + len - 39 );
	}
	else
	{
		Q_strncpyz( shown, name, sizeof( shown ) );
	}
	Com_sprintf( lines[0], sizeof( lines[0] ), "emissive shader %d", s_emiSel );
	Com_sprintf( lines[1], sizeof( lines[1] ), "polygons: %d", polys );
	Com_sprintf( lines[2], sizeof( lines[2] ), "scale: %g%s", scale, scale != 1.0f ? " (was 1)" : "" );

	const int	h = LE_TextH();
	int			maxw = LE_TextW( shown );

	for ( int i = 0; i < 3; i++ )
	{
		maxw = Q_max( maxw, LE_TextW( lines[i] ) );
	}
	CG_FillRect( 640 - 8 - maxw - 4, y - 3, maxw + 10, 4 * h + 6, colPanelBg );
	LE_Text( 640 - 8 - LE_TextW( lines[0] ), y, lines[0], colTxt );
	LE_Text( 640 - 8 - LE_TextW( shown ), y + h, shown, colTxt );
	LE_Text( 640 - 8 - LE_TextW( lines[1] ), y + 2 * h, lines[1], colTxt );
	LE_Text( 640 - 8 - LE_TextW( lines[2] ), y + 3 * h, lines[2], scale != 1.0f ? colTxtYel : colTxt );
	return qtrue;
}

/*
=================
Commands
=================
*/
static qboolean LE_EmiCmdReady( void )
{
	if ( !CG_LightEdit_Active() || !s_api )
	{
		LE_Msg( "light edit: not active" );
		return qfalse;
	}
	if ( !LE_EmiReady() )
	{
		LE_Msg( "light edit: the renderer has no emissive shader list" );
		return qfalse;
	}
	return qtrue;
}

// True when text holds part, ignoring case.
static qboolean LE_EmiContains( const char *text, const char *part )
{
	const size_t	n = strlen( part );

	for ( ; *text; text++ )
	{
		if ( !Q_stricmpn( text, part, (int)n ) )
		{
			return qtrue;
		}
	}
	return qfalse;
}

// Index from a number, an exact name or a unique part of a name. Prints the reason when it fails.
static int LE_EmiFind( const char *arg )
{
	const int	count = s_api->CountEmissiveShaders();
	char		name[LEDIT_EMI_NAME_LEN];
	int			found = -1, matches = 0;

	if ( !arg[0] )
	{
		LE_Msg( "light edit: give a shader index or name (ledit_emissive_list)" );
		return -1;
	}
	if ( arg[strspn( arg, "0123456789" )] == 0 )
	{
		const int	idx = atoi( arg );

		if ( idx < 0 || idx >= count )
		{
			LE_Msg( "light edit: no emissive shader %d (0..%d)", idx, count - 1 );
			return -1;
		}
		return idx;
	}
	for ( int i = 0; i < count; i++ )
	{
		if ( LE_EmiInfo( i, name, sizeof( name ), NULL, NULL ) && !Q_stricmp( name, arg ) )
		{
			return i;
		}
	}
	for ( int i = 0; i < count; i++ )
	{
		if ( LE_EmiInfo( i, name, sizeof( name ), NULL, NULL ) && LE_EmiContains( name, arg ) )
		{
			found = i;
			matches++;
		}
	}
	if ( matches == 1 )
	{
		return found;
	}
	if ( matches )
	{
		LE_Msg( "light edit: %d shaders match \"%s\" (ledit_emissive_list %s)", matches, arg, arg );
	}
	else
	{
		LE_Msg( "light edit: no emissive shader matches \"%s\"", arg );
	}
	return -1;
}

// ledit_emissive_list [filter]: index, name, polygon count and scale; a scale other than 1 has a "*".
void LE_CmdEmissiveList( void )
{
	char	filter[64];
	int		shown = 0, changed = 0;

	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	Q_strncpyz( filter, CG_Argv( 1 ), sizeof( filter ) );
	const int	count = s_api->CountEmissiveShaders();

	for ( int i = 0; i < count; i++ )
	{
		char	name[LEDIT_EMI_NAME_LEN];
		float	scale;
		int		polys;

		if ( !LE_EmiInfo( i, name, sizeof( name ), &scale, &polys ) )
		{
			continue;
		}
		if ( scale != 1.0f )
		{
			changed++;
		}
		if ( filter[0] && !LE_EmiContains( name, filter ) )
		{
			continue;
		}
		CG_Printf( "%4d %-48s %6d polys  scale %g%s\n", i, name, polys, scale, scale != 1.0f ? " *" : "" );
		shown++;
	}
	LE_Msg( "light edit: %d of %d emissive shaders listed, %d with a scale other than 1", shown, count, changed );
}

// ledit_emissive <index|name> <scale>
void LE_CmdEmissive( void )
{
	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	if ( !CG_Argv( 2 )[0] )
	{
		LE_Msg( "usage: ledit_emissive <index | name> <scale 0..%g>", LEDIT_EMI_SCALE_MAX );
		return;
	}
	const int	shader = LE_EmiFind( CG_Argv( 1 ) );
	const float	scale = Com_Clamp( 0.0f, LEDIT_EMI_SCALE_MAX, (float)atof( CG_Argv( 2 ) ) );
	char		name[LEDIT_EMI_NAME_LEN];

	if ( shader < 0 )
	{
		return;
	}
	LE_UndoBegin( "emissive scale" );
	const qboolean	ok = LE_EmiSetScale( shader, scale, "emissive scale" );

	LE_UndoEnd();
	if ( ok && LE_EmiInfo( shader, name, sizeof( name ), NULL, NULL ) )
	{
		LE_Msg( "light edit: emissive shader %d %s scale %g", shader, name, scale );
	}
}

// ledit_emissive_reset <index|name|all>
void LE_CmdEmissiveReset( void )
{
	int	done = 0;

	if ( !LE_EmiCmdReady() )
	{
		return;
	}
	if ( !Q_stricmp( CG_Argv( 1 ), "all" ) )
	{
		const int	count = s_api->CountEmissiveShaders();

		LE_UndoBegin( "emissive reset" );
		for ( int i = 0; i < count; i++ )
		{
			float	scale;

			if ( LE_EmiInfo( i, NULL, 0, &scale, NULL ) && scale != 1.0f && LE_EmiSetScale( i, 1.0f, "emissive reset" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
		LE_Msg( "light edit: %d emissive shader%s reset to 1", done, done == 1 ? "" : "s" );
		return;
	}
	const int	shader = LE_EmiFind( CG_Argv( 1 ) );

	if ( shader < 0 )
	{
		return;
	}
	LE_UndoBegin( "emissive reset" );
	const qboolean	ok = LE_EmiSetScale( shader, 1.0f, "emissive reset" );

	LE_UndoEnd();
	if ( ok )
	{
		LE_Msg( "light edit: emissive shader %d scale reset to 1", shader );
	}
}
