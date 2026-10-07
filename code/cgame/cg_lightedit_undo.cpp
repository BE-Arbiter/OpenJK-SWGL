// Light edit mode: undo and redo stacks of grouped entries.
// A group holds the sub-entries of one user action. Undo applies them in reverse order,
// redo in order.

#include "cg_headers.h"
#include "cg_lightedit_local.h"

#include <vector>

#define LEDIT_UNDO_DEPTH		256

typedef struct {
	int				kind;
	int				id;
	rtxLightDesc_t	before;
	rtxLightDesc_t	after;
} ledUndoSub_t;

typedef struct {
	char						label[48];
	std::vector<ledUndoSub_t>	subs;
} ledUndoGroup_t;

static std::vector<ledUndoGroup_t>	s_undo;
static std::vector<ledUndoGroup_t>	s_redo;
static ledUndoGroup_t				s_open;
static qboolean						s_groupOpen = qfalse;

void LE_UndoClear( void )
{
	s_undo.clear();
	s_redo.clear();
	s_open.subs.clear();
	s_groupOpen = qfalse;
}

int LE_UndoDepth( void )
{
	return (int)s_undo.size();
}

int LE_RedoDepth( void )
{
	return (int)s_redo.size();
}

static void LE_UndoPushGroup( const ledUndoGroup_t &g )
{
	if ( (int)s_undo.size() == LEDIT_UNDO_DEPTH )
	{
		s_undo.erase( s_undo.begin() );
	}
	s_undo.push_back( g );
	s_redo.clear();
}

void LE_UndoBegin( const char *label )
{
	s_open.subs.clear();
	Q_strncpyz( s_open.label, label, sizeof( s_open.label ) );
	s_groupOpen = qtrue;
}

void LE_UndoEnd( void )
{
	if ( s_groupOpen && !s_open.subs.empty() )
	{
		LE_UndoPushGroup( s_open );
	}
	s_open.subs.clear();
	s_groupOpen = qfalse;
}

// Without an open group, the entry makes a group of its own.
void LE_UndoPush( int kind, int id, const rtxLightDesc_t *before, const rtxLightDesc_t *after, const char *label )
{
	ledUndoSub_t	sub;

	memset( &sub, 0, sizeof( sub ) );
	sub.kind = kind;
	sub.id = id;
	if ( before )
	{
		sub.before = *before;
	}
	if ( after )
	{
		sub.after = *after;
	}
	if ( s_groupOpen )
	{
		s_open.subs.push_back( sub );
		return;
	}

	ledUndoGroup_t	g;

	Q_strncpyz( g.label, label, sizeof( g.label ) );
	g.subs.push_back( sub );
	LE_UndoPushGroup( g );
}

static qboolean LE_UndoApply( const ledUndoSub_t &e, qboolean undo )
{
	switch ( e.kind )
	{
	case LEDU_SET:
		return s_api->Set( e.id, undo ? &e.before : &e.after );
	case LEDU_ADD:
		return undo ? s_api->Remove( e.id ) : s_api->Restore( e.id );
	case LEDU_REMOVE:
		return undo ? s_api->Restore( e.id ) : s_api->Remove( e.id );
	case LEDU_RESTORE:
		return undo ? s_api->Remove( e.id ) : s_api->Restore( e.id );
	}
	return qfalse;
}

// Applies a group, then selects the lights that are still there.
static int LE_ApplyGroup( const ledUndoGroup_t &g, qboolean undo )
{
	const int	n = (int)g.subs.size();
	int			failed = 0;

	{
		ledBatch	batch;

		for ( int i = 0; i < n; i++ )
		{
			const ledUndoSub_t	&e = g.subs[undo ? n - 1 - i : i];

			if ( !LE_UndoApply( e, undo ) )
			{
				failed++;
			}
		}
	}

	LE_SelClear();
	for ( int i = 0; i < n; i++ )
	{
		const ledUndoSub_t	&e = g.subs[i];
		const bool			gone = undo ? ( e.kind == LEDU_ADD || e.kind == LEDU_RESTORE ) : ( e.kind == LEDU_REMOVE );

		if ( !gone && !LE_SelHas( e.id ) )
		{
			LE_SelAdd( e.id );
		}
	}
	return failed;
}

static const char *LE_GroupDesc( const ledUndoGroup_t &g )
{
	if ( g.subs.size() == 1 )
	{
		return va( "%s (light %d)", g.label, g.subs[0].id );
	}
	return va( "%s (%d lights)", g.label, (int)g.subs.size() );
}

void LE_Undo( void )
{
	if ( LE_GrabActive() )
	{
		LE_Msg( "light edit: release the light first" );
		return;
	}
	if ( s_undo.empty() )
	{
		LE_Msg( "light edit: nothing to undo" );
		return;
	}

	const ledUndoGroup_t	g = s_undo.back();
	const int				failed = LE_ApplyGroup( g, qtrue );

	s_undo.pop_back();
	s_redo.push_back( g );
	if ( failed )
	{
		LE_Msg( "light edit: undo %s: %d of %d failed (%s)", LE_GroupDesc( g ), failed, (int)g.subs.size(), s_api->LastError() );
		return;
	}
	LE_Msg( "light edit: undo %s", LE_GroupDesc( g ) );
}

void LE_Redo( void )
{
	if ( LE_GrabActive() )
	{
		LE_Msg( "light edit: release the light first" );
		return;
	}
	if ( s_redo.empty() )
	{
		LE_Msg( "light edit: nothing to redo" );
		return;
	}

	const ledUndoGroup_t	g = s_redo.back();
	const int				failed = LE_ApplyGroup( g, qfalse );

	s_redo.pop_back();
	s_undo.push_back( g );
	if ( failed )
	{
		LE_Msg( "light edit: redo %s: %d of %d failed (%s)", LE_GroupDesc( g ), failed, (int)g.subs.size(), s_api->LastError() );
		return;
	}
	LE_Msg( "light edit: redo %s", LE_GroupDesc( g ) );
}

// One line per group, most recent first.
void LE_UndoHistory( void )
{
	CG_Printf( "light edit: %d undo, %d redo\n", (int)s_undo.size(), (int)s_redo.size() );
	for ( int i = (int)s_undo.size() - 1; i >= 0; i-- )
	{
		CG_Printf( "  undo %3d: %s, %d sub-entries\n", (int)s_undo.size() - i, LE_GroupDesc( s_undo[i] ), (int)s_undo[i].subs.size() );
	}
	for ( int i = (int)s_redo.size() - 1; i >= 0; i-- )
	{
		CG_Printf( "  redo %3d: %s, %d sub-entries\n", (int)s_redo.size() - i, LE_GroupDesc( s_redo[i] ), (int)s_redo[i].subs.size() );
	}
}
