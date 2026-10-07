// Light edit mode: tool 5 (properties). The wheel changes the active property of the targets,
// the primary fire copies it from the aimed light, the secondary fire resets it.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>
#include <math.h>

#define LEDIT_TEMP_MIN			1000.0f
#define LEDIT_TEMP_MAX			12000.0f
#define LEDIT_TEMP_SEARCH_STEP	50.0f
#define LEDIT_INTENSITY_LOG_MAX	5.0f		// gauge: 10^5 at the right end
#define LEDIT_RADIUS_GAUGE_MAX	64.0f

static int				s_prop = LEP_INTENSITY;

static const vec4_t		colGaugeBack	= { 0.00f, 0.00f, 0.00f, 0.70f };
static const vec4_t		colGaugeFill	= { 0.30f, 1.00f, 1.00f, 1.0f };
static const vec4_t		colGaugeMark	= { 1.00f, 1.00f, 1.00f, 1.0f };

static const char		*s_propNames[LEP_NUM] = {
	"intensity", "hue", "saturation", "temperature", "radius", "cone outer", "cone inner"
};

void LE_PropsInit( void )
{
	s_prop = LEP_INTENSITY;
}

int LE_PropActive( void )
{
	return s_prop;
}

const char *LE_PropName( int prop )
{
	return s_propNames[Com_Clampi( 0, LEP_NUM - 1, prop )];
}

void LE_PropCycle( int dir )
{
	s_prop = ( s_prop + ( dir > 0 ? 1 : LEP_NUM - 1 ) ) % LEP_NUM;
	LE_Msg( "light edit: property %s", LE_PropName( s_prop ) );
}

/*
=================
Colour maths
=================
*/
void LE_ColorToHSV( const vec3_t rgb, float *h, float *s, float *v )
{
	const float	mx = Q_max( rgb[0], Q_max( rgb[1], rgb[2] ) );
	const float	mn = Q_min( rgb[0], Q_min( rgb[1], rgb[2] ) );
	const float	delta = mx - mn;
	float		hue = 0.0f;

	if ( delta > 0.0f )
	{
		if ( mx == rgb[0] )
		{
			hue = ( rgb[1] - rgb[2] ) / delta;
		}
		else if ( mx == rgb[1] )
		{
			hue = 2.0f + ( rgb[2] - rgb[0] ) / delta;
		}
		else
		{
			hue = 4.0f + ( rgb[0] - rgb[1] ) / delta;
		}
		hue *= 60.0f;
		if ( hue < 0.0f )
		{
			hue += 360.0f;
		}
	}
	*h = hue;
	*s = ( mx > 0.0f ) ? delta / mx : 0.0f;
	*v = mx;
}

static void LE_HSVToColor( float h, float s, float v, vec3_t rgb )
{
	const float	hh = fmodf( h, 360.0f ) / 60.0f;
	const int	i = (int)floorf( hh );
	const float	f = hh - (float)i;
	const float	p = v * ( 1.0f - s );
	const float	q = v * ( 1.0f - s * f );
	const float	t = v * ( 1.0f - s * ( 1.0f - f ) );

	switch ( i % 6 )
	{
	case 0:		VectorSet( rgb, v, t, p );	break;
	case 1:		VectorSet( rgb, q, v, p );	break;
	case 2:		VectorSet( rgb, p, v, t );	break;
	case 3:		VectorSet( rgb, p, q, v );	break;
	case 4:		VectorSet( rgb, t, p, v );	break;
	default:	VectorSet( rgb, v, p, q );	break;
	}
	for ( int k = 0; k < 3; k++ )
	{
		rgb[k] = Com_Clamp( 0.0f, 1.0f, rgb[k] );
	}
}

// Blackbody colour for a temperature in K (approximation for 1000..12000 K), scaled so that the largest channel is 1.
static void LE_Blackbody( float kelvin, vec3_t out )
{
	const float	t = Com_Clamp( LEDIT_TEMP_MIN, LEDIT_TEMP_MAX, kelvin ) / 100.0f;
	float		r, g, b, mx;

	r = ( t <= 66.0f ) ? 255.0f : 329.698727446f * powf( t - 60.0f, -0.1332047592f );
	g = ( t <= 66.0f ) ? 99.4708025861f * logf( t ) - 161.1195681661f : 288.1221695283f * powf( t - 60.0f, -0.0755148492f );
	if ( t >= 66.0f )
	{
		b = 255.0f;
	}
	else
	{
		b = ( t <= 19.0f ) ? 0.0f : 138.5177312231f * logf( t - 10.0f ) - 305.0447927307f;
	}
	r = Com_Clamp( 0.0f, 255.0f, r );
	g = Com_Clamp( 0.0f, 255.0f, g );
	b = Com_Clamp( 0.0f, 255.0f, b );
	mx = Q_max( r, Q_max( g, b ) );
	VectorSet( out, r / mx, g / mx, b / mx );
}

// Temperature whose blackbody colour is the nearest to the colour (compared with the largest channel at 1).
float LE_ColorTemp( const vec3_t rgb )
{
	const float	mx = Q_max( rgb[0], Q_max( rgb[1], rgb[2] ) );
	vec3_t		n;
	float		best = 6500.0f, bestD = 1e30f;

	if ( mx <= 0.0f )
	{
		return best;
	}
	VectorScale( rgb, 1.0f / mx, n );
	for ( float k = LEDIT_TEMP_MIN; k <= LEDIT_TEMP_MAX; k += LEDIT_TEMP_SEARCH_STEP )
	{
		vec3_t	c;
		float	d;

		LE_Blackbody( k, c );
		d = ( c[0] - n[0] ) * ( c[0] - n[0] ) + ( c[1] - n[1] ) * ( c[1] - n[1] ) + ( c[2] - n[2] ) * ( c[2] - n[2] );
		if ( d < bestD )
		{
			bestD = d;
			best = k;
		}
	}
	return best;
}

/*
=================
Property operations
=================
*/
// Wheel step on one light. Returns qfalse when the property does not apply to it.
static qboolean LE_PropAdjust( rtxLightDesc_t *d, int prop, int wheel, qboolean fine )
{
	const float	n = (float)wheel;
	float		h, s, v;

	switch ( prop )
	{
	case LEP_INTENSITY:
		d->intensity = Q_max( 0.0f, d->intensity * powf( fine ? 1.01f : 1.1f, n ) );
		return qtrue;
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( d->color, &h, &s, &v );
		if ( v <= 0.0f )
		{
			return qfalse;
		}
		if ( prop == LEP_HUE )
		{
			h = fmodf( h + n * ( fine ? 1.0f : 10.0f ) + 3600.0f * 8.0f, 360.0f );
		}
		else
		{
			s = Com_Clamp( 0.0f, 1.0f, s + n * ( fine ? 0.01f : 0.05f ) );
		}
		LE_HSVToColor( h, s, v, d->color );
		return qtrue;
	case LEP_TEMP:
		LE_Blackbody( LE_ColorTemp( d->color ) + n * ( fine ? 50.0f : 250.0f ), d->color );
		return qtrue;
	case LEP_RADIUS:
		d->radius = Q_max( 0.5f, d->radius + n * ( fine ? 0.1f : 1.0f ) );
		return qtrue;
	case LEP_CONE_OUTER:
	case LEP_CONE_INNER:
		if ( d->type != RTX_LTYPE_SPOT )
		{
			return qfalse;
		}
		LE_ConeAdd( d, (qboolean)( prop == LEP_CONE_INNER ), n * LE_AngleStep( fine ) );
		return qtrue;
	}
	return qfalse;
}

// Gives dst the value of the property of src. The intensity keeps the emitted power.
static qboolean LE_PropCopy( rtxLightDesc_t *dst, const rtxLightDesc_t *src, int prop )
{
	float	h, s, v, sh, ss, sv;

	switch ( prop )
	{
	case LEP_INTENSITY:
	{
		const float	scaleSrc = s_api->IntensityScale( src->id );
		const float	scaleDst = s_api->IntensityScale( dst->id );

		dst->intensity = ( scaleSrc > 0.0f && scaleDst > 0.0f ) ? src->intensity * scaleSrc / scaleDst : src->intensity;
		return qtrue;
	}
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( dst->color, &h, &s, &v );
		LE_ColorToHSV( src->color, &sh, &ss, &sv );
		if ( v <= 0.0f )
		{
			return qfalse;
		}
		LE_HSVToColor( prop == LEP_HUE ? sh : h, prop == LEP_SAT ? ss : s, v, dst->color );
		return qtrue;
	case LEP_TEMP:
		LE_Blackbody( LE_ColorTemp( src->color ), dst->color );
		return qtrue;
	case LEP_RADIUS:
		dst->radius = src->radius;
		return qtrue;
	case LEP_CONE_OUTER:
	case LEP_CONE_INNER:
		if ( dst->type != RTX_LTYPE_SPOT || src->type != RTX_LTYPE_SPOT )
		{
			return qfalse;
		}
		if ( prop == LEP_CONE_OUTER )
		{
			dst->coneOuter = src->coneOuter;
			dst->coneInner = Q_min( dst->coneInner, dst->coneOuter );
		}
		else
		{
			dst->coneInner = Com_Clamp( 0.0f, dst->coneOuter, src->coneInner );
		}
		return qtrue;
	}
	return qfalse;
}

/*
=================
Tool
=================
*/
static void LE_PropsWheel( const std::vector<int> &ids, int wheel, qboolean fine )
{
	int	done = 0;

	if ( ids.empty() )
	{
		LE_Msg( "light edit: aim at a light or select some" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBeginMerge( LE_PropName( s_prop ), LE_HashTargets( 0x50 + s_prop, ids ) );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			if ( LE_PropAdjust( &d, s_prop, wheel, fine ) && LE_DoSet( ids[i], &d, LE_PropName( s_prop ) ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( !done )
	{
		LE_Msg( "light edit: %s does not apply to the targets", LE_PropName( s_prop ) );
	}
}

// Primary fire: the property of the aimed light goes to every other selected light.
static void LE_PropsCopyToSelection( void )
{
	rtxLightDesc_t	src;
	int				done = 0;

	if ( s_pickId < 0 || !LE_GetDesc( s_pickId, &src ) )
	{
		LE_Msg( "light edit: aim at the light to copy from" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( va( "copy %s", LE_PropName( s_prop ) ) );
		for ( size_t i = 0; i < s_selList.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( s_selList[i] == s_pickId || !LE_GetDesc( s_selList[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			if ( LE_PropCopy( &d, &src, s_prop ) && LE_DoSet( s_selList[i], &d, "copy property" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %s of light %d copied to %d light%s", LE_PropName( s_prop ), s_pickId, done, done > 1 ? "s" : "" );
	}
	else
	{
		LE_Msg( "light edit: select the lights to change, then aim at the one to copy from" );
	}
}

// Secondary fire: the property goes back to its original value on the targets.
static void LE_PropsReset( const std::vector<int> &ids )
{
	int	done = 0;

	if ( ids.empty() )
	{
		LE_Msg( "light edit: aim at a light or select some" );
		return;
	}
	{
		ledBatch	batch;

		LE_UndoBegin( va( "reset %s", LE_PropName( s_prop ) ) );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d, orig;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) || !s_api->GetOriginal( ids[i], &orig ) )
			{
				continue;
			}
			if ( s_prop == LEP_INTENSITY )
			{
				d.intensity = orig.intensity;
			}
			else if ( s_prop == LEP_TEMP )
			{
				VectorCopy( orig.color, d.color );
			}
			else if ( !LE_PropCopy( &d, &orig, s_prop ) )
			{
				continue;
			}
			if ( LE_DoSet( ids[i], &d, "reset property" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: %s reset on %d light%s", LE_PropName( s_prop ), done, done > 1 ? "s" : "" );
	}
}

void LE_ToolProps( qboolean priDown, qboolean secDown, int wheel, qboolean fine )
{
	std::vector<int>	ids;

	LE_ActionTargets( ids );
	if ( wheel )
	{
		LE_PropsWheel( ids, wheel, fine );
	}
	if ( priDown )
	{
		LE_PropsCopyToSelection();
	}
	if ( secDown )
	{
		LE_PropsReset( ids );
	}
}

void LE_PropsHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	static const char	*wheelText[LEP_NUM] = {
		"x1.1 per notch, walk x1.01",
		"+-10 deg, walk 1",
		"+-0.05, walk 0.01",
		"+-250 K, walk 50 K",
		"+-1, walk 0.1 (min 0.5)",
		"+-angle snap, walk 1 deg (1..89)",
		"+-angle snap, walk 1 deg (0..outer)"
	};

	static char	nameBuf[48], fireBuf[96], altBuf[96];

	Com_sprintf( nameBuf, sizeof( nameBuf ), "5 Properties [%s]", LE_PropName( s_prop ) );
	Com_sprintf( fireBuf, sizeof( fireBuf ), "copy the %s of the aimed light to the selection", LE_PropName( s_prop ) );
	Com_sprintf( altBuf, sizeof( altBuf ), "reset the %s to its original value", LE_PropName( s_prop ) );
	*name = nameBuf;
	*fire = fireBuf;
	*alt = altBuf;
	Com_sprintf( wheelBuf, wheelSize, "%s %s; [ ] picks the property", LE_PropName( s_prop ), wheelText[s_prop] );
}

// Gauge of the active property. It uses three dots of the budget.
void LE_PropsDrawGauge( float x, float y, float w, const rtxLightDesc_t *d )
{
	float	h, s, v, frac = 0.0f;

	switch ( s_prop )
	{
	case LEP_INTENSITY:
		frac = log10f( Q_max( d->intensity, 1.0f ) ) / LEDIT_INTENSITY_LOG_MAX;
		break;
	case LEP_HUE:
	case LEP_SAT:
		LE_ColorToHSV( d->color, &h, &s, &v );
		frac = ( s_prop == LEP_HUE ) ? h / 360.0f : s;
		break;
	case LEP_TEMP:
		frac = ( LE_ColorTemp( d->color ) - LEDIT_TEMP_MIN ) / ( LEDIT_TEMP_MAX - LEDIT_TEMP_MIN );
		break;
	case LEP_RADIUS:
		frac = d->radius / LEDIT_RADIUS_GAUGE_MAX;
		break;
	case LEP_CONE_OUTER:
		frac = d->coneOuter / 89.0f;
		break;
	case LEP_CONE_INNER:
		frac = d->coneInner / 89.0f;
		break;
	}
	frac = Com_Clamp( 0.0f, 1.0f, frac );
	LE_Rect( x - 1.0f, y - 1.0f, w + 2.0f, 7.0f, colGaugeBack );
	LE_Rect( x, y, w * frac, 5.0f, colGaugeFill );
	LE_Rect( x + w * frac - 1.0f, y - 2.0f, 2.0f, 9.0f, colGaugeMark );
}
