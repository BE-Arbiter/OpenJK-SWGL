// Light edit mode: tool 6 (pipette). The primary fire copies the aimed light into a clipboard, which also
// becomes the preset of tool 2. The secondary fire pastes it, with a filter that the wheel cycles.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>

typedef enum {
	LEF_ALL = 0,
	LEF_COLOR,
	LEF_INTENSITY,
	LEF_SHAPE,
	LEF_NUM
} ledPasteFilter_t;

static const char	*s_filterNames[LEF_NUM] = { "all", "colour", "intensity", "shape" };

static qboolean			s_clipValid = qfalse;
static rtxLightDesc_t	s_clip;				// intensity in the units of an added light
static int				s_filter = LEF_ALL;

void LE_PipetteInit( void )
{
	s_clipValid = qfalse;
	memset( &s_clip, 0, sizeof( s_clip ) );
	s_filter = LEF_ALL;
}

// The clipboard as a preset for a new light: type, colour, intensity, radius, direction and cones.
qboolean LE_PipettePreset( rtxLightDesc_t *d )
{
	if ( !s_clipValid )
	{
		return qfalse;
	}
	d->type = s_clip.type;
	VectorCopy( s_clip.color, d->color );
	d->intensity = s_clip.intensity;
	d->radius = s_clip.radius;
	VectorCopy( s_clip.dir, d->dir );
	d->coneOuter = s_clip.coneOuter;
	d->coneInner = s_clip.coneInner;
	return qtrue;
}

static void LE_PipetteCopy( void )
{
	rtxLightDesc_t	src;
	float			scaleSrc, scaleNew;

	if ( s_pickId < 0 || !LE_GetDesc( s_pickId, &src ) || ( src.flags & RTX_LFLAG_DELETED ) )
	{
		LE_Msg( "light edit: aim at the light to copy" );
		return;
	}
	scaleSrc = s_api->IntensityScale( src.id );
	scaleNew = s_api->IntensityScale( -1 );
	s_clip = src;
	s_clip.id = -1;
	s_clip.flags = 0;
	VectorClear( s_clip.origin );
	s_clip.name[0] = 0;
	if ( scaleSrc > 0.0f && scaleNew > 0.0f )
	{
		s_clip.intensity = src.intensity * scaleSrc / scaleNew;
	}
	s_clipValid = qtrue;
	LE_CreateSetSpot( (qboolean)( src.type == RTX_LTYPE_SPOT ) );
	LE_Msg( "light edit: copied light %d (%s, intensity %.1f); tool 2 uses it as its preset", src.id,
		src.type == RTX_LTYPE_SPOT ? "spot" : "sphere", s_clip.intensity );
}

// Gives d the clipboard values that the filter selects. The intensity goes back to the units of the target.
static void LE_PipetteApply( rtxLightDesc_t *d )
{
	if ( s_filter == LEF_ALL || s_filter == LEF_COLOR )
	{
		VectorCopy( s_clip.color, d->color );
	}
	if ( s_filter == LEF_ALL || s_filter == LEF_INTENSITY )
	{
		const float	scaleNew = s_api->IntensityScale( -1 );
		const float	scaleDst = s_api->IntensityScale( d->id );

		d->intensity = ( scaleNew > 0.0f && scaleDst > 0.0f ) ? s_clip.intensity * scaleNew / scaleDst : s_clip.intensity;
	}
	if ( s_filter == LEF_ALL || s_filter == LEF_SHAPE )
	{
		if ( s_clip.type == RTX_LTYPE_SPOT )
		{
			if ( d->type != RTX_LTYPE_SPOT )
			{
				d->type = RTX_LTYPE_SPOT;
				VectorCopy( s_clip.dir, d->dir );
			}
			d->coneOuter = s_clip.coneOuter;
			d->coneInner = s_clip.coneInner;
		}
		else
		{
			d->type = RTX_LTYPE_SPHERE;
		}
		d->radius = s_clip.radius;
	}
}

static void LE_PipettePaste( void )
{
	std::vector<int>	ids;
	int					done = 0;

	if ( !s_clipValid )
	{
		LE_Msg( "light edit: the clipboard is empty (fire copies the aimed light)" );
		return;
	}
	if ( s_pickId < 0 )
	{
		LE_Msg( "light edit: aim at the light to paste on" );
		return;
	}
	LE_ActionTargets( ids );
	{
		ledBatch	batch;

		LE_UndoBegin( "paste" );
		for ( size_t i = 0; i < ids.size(); i++ )
		{
			rtxLightDesc_t	d;

			if ( !LE_GetDesc( ids[i], &d ) || ( d.flags & RTX_LFLAG_DELETED ) )
			{
				continue;
			}
			LE_PipetteApply( &d );
			if ( LE_DoSet( ids[i], &d, "paste" ) )
			{
				done++;
			}
		}
		LE_UndoEnd();
	}
	if ( done )
	{
		LE_Msg( "light edit: pasted %s on %d light%s", s_filterNames[s_filter], done, done > 1 ? "s" : "" );
	}
}

void LE_ToolPipette( qboolean priDown, qboolean secDown, int wheel )
{
	if ( wheel )
	{
		s_filter = ( ( s_filter + wheel ) % LEF_NUM + LEF_NUM ) % LEF_NUM;
	}
	if ( priDown )
	{
		LE_PipetteCopy();
	}
	if ( secDown )
	{
		LE_PipettePaste();
	}
}

void LE_PipetteHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize )
{
	static char	altBuf[96];

	*name = "6 Pipette";
	*fire = "copy the aimed light; it becomes the preset of tool 2";
	if ( s_clipValid )
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "paste %s on the aimed light (the selection if it is a member)", s_filterNames[s_filter] );
	}
	else
	{
		Com_sprintf( altBuf, sizeof( altBuf ), "paste on the aimed light (the clipboard is empty)" );
	}
	*alt = altBuf;
	Com_sprintf( wheelBuf, wheelSize, "paste filter: all / colour / intensity / shape (now %s)", s_filterNames[s_filter] );
}
