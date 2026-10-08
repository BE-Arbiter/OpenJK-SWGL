// Light edit mode: state and helpers that the cg_lightedit*.cpp files share.
// Not for use outside the light edit files; the public interface is cg_lightedit.h.
#ifndef CG_LIGHTEDIT_LOCAL_H
#define CG_LIGHTEDIT_LOCAL_H

#include "../rd-common/rtx_light_edit_api.h"
#include <vector>

#define LEDIT_TOOL_SELECT		1
#define LEDIT_TOOL_CREATE		2
#define LEDIT_TOOL_MOVE			3
#define LEDIT_TOOL_ORIENT		4
#define LEDIT_TOOL_PROPS		5
#define LEDIT_TOOL_PIPETTE		6
#define LEDIT_TOOL_CLONE		7
#define LEDIT_TOOL_DELETE		8
#define LEDIT_TOOL_SOLO			9

// Shared state (cg_lightedit.cpp).
extern rtxLightEditAPI_t		*s_api;
extern int						s_sel;			// primary selected light, -1 for none
extern std::vector<int>			s_selList;		// selected lights in selection order; s_sel is a member
extern int						s_pickId;		// light under the crosshair, -1 for none
extern float					s_pickT;		// its depth along the view axis
extern int						s_tool;

// One cluster rebuild for all the Set / Add calls of a scope.
struct ledBatch
{
	ledBatch()	{ s_api->BeginBatch(); }
	~ledBatch()	{ s_api->EndBatch(); }
};

void		LE_Msg( const char *fmt, ... );
qboolean	LE_GetDesc( int id, rtxLightDesc_t *d );
qboolean	LE_DescEqual( const rtxLightDesc_t *a, const rtxLightDesc_t *b );
qboolean	LE_DoSet( int id, const rtxLightDesc_t *want, const char *label );	// Set plus one undo entry

extern qboolean CG_WorldCoordToScreenCoordFloat( vec3_t worldCoord, float *x, float *y );

// Selection.
qboolean	LE_SelHas( int id );
void		LE_SelClear( void );
void		LE_SelSet( int id );					// replaces the selection
void		LE_SelAdd( int id );					// appends, becomes primary
void		LE_SelToggle( int id );
void		LE_SelPrimary( int id );				// makes a member the primary light
void		LE_ActionTargets( std::vector<int> &out );	// aimed light's group, the aimed light, or the selection
void		LE_SetTool( int tool );

// 2D drawing within the dot budget (cg_lightedit.cpp).
void		LE_Line( float x1, float y1, float x2, float y2, const vec4_t col );
void		LE_Circle3D( const vec3_t center, const vec3_t ax1, const vec3_t ax2, float radius, int points, const vec4_t col );

// Undo groups (cg_lightedit_undo.cpp).
typedef enum {
	LEDU_SET = 0,
	LEDU_ADD,
	LEDU_REMOVE,
	LEDU_RESTORE
} ledUndoKind_t;

void		LE_UndoClear( void );
void		LE_UndoBegin( const char *label );		// opens a group; the pushes that follow join it
void		LE_UndoEnd( void );						// closes the group; an empty group is dropped
void		LE_UndoPush( int kind, int id, const rtxLightDesc_t *before, const rtxLightDesc_t *after, const char *label );
int			LE_UndoDepth( void );
int			LE_RedoDepth( void );
void		LE_UndoBeginMerge( const char *label, unsigned key );	// like Begin; joins the top group when it has this key and is under 500 ms old
void		LE_Undo( void );
void		LE_Redo( void );
void		LE_UndoHistory( void );

// Grid, snap and constraint maths (cg_lightedit_grid.cpp).
void		LE_GridInit( void );					// registers the cvars
void		LE_GridUpdate( void );					// once per frame
int			LE_GridSize( void );
qboolean	LE_SnapOn( void );
void		LE_SnapPoint( vec3_t p, int axisMask );	// rounds the axes whose bit is set; no-op when snap is off
void		LE_GridNext( void );					// cycles the grid size and prints it
void		LE_SnapToggle( void );
qboolean	LE_ClosestPointOnAxis( const vec3_t axisOrg, int axis, const vec3_t rayOrg, const vec3_t rayDir, vec3_t out );

// Tool 3 and the grab (cg_lightedit_move.cpp).
void		LE_GrabReset( void );
qboolean	LE_GrabActive( void );
int			LE_GrabPrimary( void );
void		LE_BeginGrab( int primary, const std::vector<int> &ids, float dist );
void		LE_EndGrab( void );
void		LE_ToolMove( qboolean prim, qboolean priDown, qboolean secDown, int wheel, qboolean fine );
void		LE_DrawGrabWorld( void );
void		LE_MoveHelp( const char **name, const char **fire, const char **alt, const char **wheel );

// Tool 7 (cg_lightedit_clone.cpp).
void		LE_CloneInit( void );
void		LE_ToolClone( qboolean priDown, qboolean secDown, int wheel );
void		LE_CloneHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );

// Hash of an action kind and its targets: merges the wheel changes of one gesture into one undo group.
static inline unsigned LE_HashTargets( int kind, const std::vector<int> &ids )
{
	unsigned	h = 2166136261u ^ (unsigned)kind;

	for ( size_t i = 0; i < ids.size(); i++ )
	{
		h = ( h ^ (unsigned)( ids[i] + 1 ) ) * 16777619u;
	}
	return h ? h : 1u;
}

// Overlay helpers that count against the dot budget (cg_lightedit.cpp).
void		LE_Dot( float x, float y, float size, const vec4_t col );
void		LE_Box( float x, float y, float half, const vec4_t col );
void		LE_Rect( float x, float y, float w, float h, const vec4_t col );	// one dot of the budget
int			LE_DotsLeft( void );
void		LE_DotFloor( int floorDots );						// the dots below this count stay free; 0 clears it
const rtxLightDesc_t	*LE_RecDesc( int id );						// this frame's descriptor, NULL when unknown
void		LE_CreateSetSpot( qboolean spot );					// creation type of tool 2

// Aim, cones and tool 4 (cg_lightedit_spot.cpp).
qboolean	LE_CrosshairHit( vec3_t pos, vec3_t normal );		// world hit under the crosshair; qfalse for sky or nothing
void		LE_SnapDir( vec3_t dir );							// rounds yaw and pitch to ledit_angle_snap
void		LE_ConeAdd( rtxLightDesc_t *d, qboolean inner, float delta );
float		LE_AngleStep( qboolean fine );
void		LE_DrawSpotWire( const vec3_t org, const vec3_t dir, float outer, float inner, const vec4_t col, qboolean full );
void		LE_DrawSpotWires( void );							// selected spots, and the aimed one for tools 4 and 5
void		LE_ToolOrient( qboolean priDown, qboolean secDown, int wheel, qboolean fine );
void		LE_OrientHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_OrientInit( void );

// Tool 5 (cg_lightedit_props.cpp).
typedef enum {
	LEP_INTENSITY = 0,
	LEP_HUE,
	LEP_SAT,
	LEP_TEMP,
	LEP_RADIUS,
	LEP_CONE_OUTER,
	LEP_CONE_INNER,
	LEP_NUM
} ledProp_t;

void		LE_PropsInit( void );
void		LE_PropCycle( int dir );							// invnext / invprev
int			LE_PropActive( void );
const char	*LE_PropName( int prop );
void		LE_ColorToHSV( const vec3_t rgb, float *h, float *s, float *v );
float		LE_ColorTemp( const vec3_t rgb );					// nearest blackbody temperature, in K
void		LE_ToolProps( qboolean priDown, qboolean secDown, int wheel, qboolean fine );
void		LE_PropsHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_PropsDrawGauge( float x, float y, float w, const rtxLightDesc_t *d );
void		LE_PropValueText( const rtxLightDesc_t *d, int prop, char *out, int size );	// current value, for the numeric entry
qboolean	LE_PropValueApply( rtxLightDesc_t *d, int prop, const float *val, int count );	// typed values

// Tool 6 (cg_lightedit_pipette.cpp).
void		LE_PipetteInit( void );
qboolean	LE_PipettePreset( rtxLightDesc_t *d );				// clipboard as a preset for tool 2; qfalse when empty
void		LE_ToolPipette( qboolean priDown, qboolean secDown, int wheel );
void		LE_PipetteHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );

// Tool 9, mute and solo, display filter, still accumulation (cg_lightedit_solo.cpp).
void		LE_SoloInit( void );								// registers the cvar, drops the state
void		LE_SoloUpdate( void );								// once per frame
void		LE_SoloRelease( void );								// ends the solo, unmutes the lights muted here
void		LE_SoloEndForAdd( void );							// ends the solo before an Add
int			LE_SoloId( void );									// -1 for none
qboolean	LE_FilterShows( const rtxLightDesc_t *d );
const char	*LE_FilterName( void );
void		LE_ToolSolo( qboolean priDown, qboolean secDown, int wheel );
void		LE_SoloHelp( const char **name, const char **fire, const char **alt, char *wheelBuf, int wheelSize );
void		LE_StillAccumFrame( int buttons );
void		LE_StillAccumRestore( void );

// Keys, numeric entry, go to the selection, select by id (cg_lightedit_keys.cpp).
void		LE_Cmd_Save( void );
void		LE_Cmd_Undo( void );
void		LE_Cmd_Redo( void );
void		LE_Cmd_Delete( void );
void		LE_Cmd_Deselect( void );
void		LE_KeysInit( void );								// closes the entry
void		LE_KeysUpdate( void );								// once per frame; closes the entry when it is stale
qboolean	LE_KeysEntryLine( char *out, int size );			// text of the open entry; qfalse when none
void		LE_GotoSelection( void );							// camera in front of the selection
void		LE_CmdGoto( void );									// ledit_goto
void		LE_CmdSelect( void );								// ledit_select <id | none> [add]

float		LE_AngleSnap( void );								// ledit_angle_snap, degrees (cg_lightedit_grid.cpp)

// Text within the glyph budget (cg_lightedit.cpp).
int			LE_TextW( const char *s );
int			LE_TextH( void );
void		LE_Text( int x, int y, const char *s, const vec4_t col );	// stops when the glyph budget is used
void		LE_TextFloor( int floorGlyphs );					// the glyphs below this count stay free; 0 clears it

// This frame's records and display state (cg_lightedit.cpp).
int			LE_RecCount( void );
qboolean	LE_RecScreen( int id, float *sx, float *sy, float *depth, qboolean *occluded );	// qfalse when invalid or off screen
int			LE_ShowMode( void );								// ledit_show
qboolean	LE_XrayOn( void );

// Labels, emissive and dynamic dots, bind file (cg_lightedit_label.cpp).
void		LE_LabelInit( void );								// registers ledit_label, drops the emissive cache
void		LE_LabelUpdate( void );								// once per frame
void		LE_DrawLabels( void );
void		LE_DrawExtraLights( void );
const char	*LE_ShowModeName( void );
void		LE_CmdWriteBinds( void );							// ledit_writebinds [force]

#endif // CG_LIGHTEDIT_LOCAL_H
