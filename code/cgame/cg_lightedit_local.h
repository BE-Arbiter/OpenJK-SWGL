// Light edit mode: state and helpers that the cg_lightedit*.cpp files share.
// Not for use outside the light edit files; the public interface is cg_lightedit.h.
#ifndef CG_LIGHTEDIT_LOCAL_H
#define CG_LIGHTEDIT_LOCAL_H

#include "../rd-common/rtx_light_edit_api.h"
#include <vector>

#define LEDIT_TOOL_SELECT		1
#define LEDIT_TOOL_CREATE		2
#define LEDIT_TOOL_MOVE			3
#define LEDIT_TOOL_CLONE		7
#define LEDIT_TOOL_DELETE		8

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

#endif // CG_LIGHTEDIT_LOCAL_H
