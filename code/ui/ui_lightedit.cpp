/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// In-game menu of the RTX light edit mode.
//
// The command ledit_menu (sent by the cgame on Ctrl+M) opens the menu "lighteditMenu". The
// list of lights comes from the light edit API of the renderer (read only). Every change goes
// through the cgame commands (ledit_select, ledit_set, ...) so that it stays undoable.

#include "../server/exe_headers.h"
#include "ui_local.h"
#include "menudef.h"
#include "ui_shared.h"
#include "../rd-common/rtx_light_edit_api.h"
#include "ui_lightedit.h"

#include <math.h>
#include <string>
#include <vector>

menuDef_t	*Menus_FindByName( const char *p );
void		Menus_CloseByName( const char *p );
void		UI_Cursor_Show( qboolean flag );

#define LE_MENU_NAME		"lighteditMenu"
#define LE_MENU_FILE		"ui/lightedit.menu"
#define LE_FILTER_LEN		32
#define LE_SEARCH_LEN		64

// Menu text used when ui/lightedit.menu does not exist. Panel in the right half of the screen.
static const char s_menuTextHead[] = R"MENU(
{
 menuDef
 {
  name "lighteditMenu"
  fullScreen 0
  rect 0 0 640 480
  visible 1
  focusColor 1 1 1 1
  onESC { play "sound/interface/esc.wav" ; uiScript "ledit_close" }

  itemDef { name panel style WINDOW_STYLE_FILLED rect 318 0 322 480 backcolor 0 0 0 .72 bordercolor 1 .682 0 .6 border 1 bordersize 1 visible 1 decoration }
  itemDef { name title text "Light edit" rect 326 4 200 14 font 2 textscale .8 textaligny 0 forecolor 1 .682 0 1 visible 1 decoration }
  itemDef { name stats ownerdraw 267 rect 326 22 308 24 font 2 textscale .45 forecolor .8 .8 .8 1 visible 1 decoration }

  itemDef { name f_filter type ITEM_TYPE_MULTI text "Filter:" cvar "ui_ledit_filter" rect 326 50 110 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1
   cvarStrList { "All" "all" "Added" "added" "Entity" "entity" "Lgt" "lgt" "Modified" "modified" "Disabled" "disabled" "Spots" "spots" }
   action { play "sound/interface/button1.wav" } }
  itemDef { name f_search type ITEM_TYPE_EDITFIELD text "Search:" cvar "ui_ledit_search" maxchars 24 rect 440 50 194 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }

  itemDef { name l_lights type ITEM_TYPE_LISTBOX style WINDOW_STYLE_FILLED rect 326 66 308 140 elementwidth 300 elementheight 11 elementtype LISTBOX_TEXT feeder 32
   font 2 textscale .42 border 1 bordersize 1 bordercolor 1 .682 0 .6 backcolor 0 0 0 .55 forecolor .85 .85 .85 1 outlinecolor 1 .682 0 .3 visible 1
   columns 6  0 22 22  24 44 44  70 34 34  106 34 34  142 24 24  168 120 120
   action { play "sound/interface/button1.wav" } }

  itemDef { name b_select type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 326 210 60 14 text "Select" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_select" } }
  itemDef { name b_addsel type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 388 210 60 14 text "Add to sel." font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_addsel" } }
  itemDef { name b_goto type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 450 210 60 14 text "Go to" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_goto" } }
  itemDef { name b_delete type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 512 210 60 14 text "Delete" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_delete" } }
  itemDef { name b_restore type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 574 210 60 14 text "Restore" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_restore" } }

  itemDef { name b_solo type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 326 226 60 14 text "Solo" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_solo" } }
  itemDef { name b_endsolo type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 388 226 60 14 text "End solo" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_endsolo" } }
  itemDef { name b_revert type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 450 226 60 14 text "Revert" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_revert" } }
  itemDef { name b_close type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 574 226 60 14 text "Close" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_close" } }
)MENU";

static const char s_menuTextTail[] = R"MENU(
  itemDef { name colour_title text "Colour" rect 326 248 100 12 font 2 textscale .6 textaligny 0 forecolor 1 1 1 1 visible 1 decoration }
  itemDef { name swatch ownerdraw 266 rect 326 262 44 44 visible 1 decoration }
  itemDef { name s_hue type ITEM_TYPE_SLIDER text "Hue" cvarfloat "ui_ledit_hue" 0 0 360 rect 376 262 200 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }
  itemDef { name s_sat type ITEM_TYPE_SLIDER text "Saturation" cvarfloat "ui_ledit_sat" 0 0 1 rect 376 276 200 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }
  itemDef { name s_val type ITEM_TYPE_SLIDER text "Value" cvarfloat "ui_ledit_val" 1 0 1 rect 376 290 200 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }
  itemDef { name s_temp type ITEM_TYPE_SLIDER text "Kelvin" cvarfloat "ui_ledit_temp" 6500 1000 12000 rect 376 304 200 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }

  itemDef { name b_load type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 326 322 74 14 text "Load from light" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 37 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_load" } }
  itemDef { name b_applycol type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 402 322 70 14 text "Apply colour" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 35 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_applycolor" } }
  itemDef { name b_usetemp type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 474 322 76 14 text "Use temperature" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 38 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_usetemp" } }
  itemDef { name b_applytemp type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 552 322 82 14 text "Apply temperature" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 41 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_applytemp" } }

  itemDef { name num_title text "Values" rect 326 346 100 12 font 2 textscale .6 textaligny 0 forecolor 1 1 1 1 visible 1 decoration }
  itemDef { name e_intensity type ITEM_TYPE_EDITFIELD text "Intensity:" cvar "ui_ledit_intensity" maxchars 12 rect 326 362 200 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }
  itemDef { name b_applyint type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 574 361 60 14 text "Apply" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_applyintensity" } }
  itemDef { name e_radius type ITEM_TYPE_EDITFIELD text "Radius:" cvar "ui_ledit_radius" maxchars 12 rect 326 380 200 12 font 2 textscale .5 textaligny 1 forecolor 1 .682 0 1 visible 1 }
  itemDef { name b_applyrad type ITEM_TYPE_BUTTON style WINDOW_STYLE_FILLED rect 574 379 60 14 text "Apply" font 2 textscale .5 textalign ITEM_ALIGN_CENTER textalignx 30 textaligny 2 forecolor 1 .682 0 1 backcolor .12 .06 0 .9 border 1 bordersize 1 bordercolor 1 .682 0 .6 visible 1
   action { play "sound/interface/button1.wav" ; uiScript "ledit_applyradius" } }

  itemDef { name hint text "Esc closes the menu. Every change can be undone with Ctrl+Z." rect 326 460 308 12 font 2 textscale .4 textaligny 0 forecolor .6 .6 .6 1 visible 1 decoration }
 }
}
)MENU";

static const char *const s_menuFilters[] = { "all", "added", "entity", "lgt", "modified", "disabled", "spots" };

static std::vector<int>	s_rows;						// ids of the lights that pass the filter
static qboolean			s_rowsValid;
static int				s_rowsGeneration;
static int				s_rowsCount;
static char				s_rowsFilter[LE_FILTER_LEN];
static char				s_rowsSearch[LE_SEARCH_LEN];
static qboolean			s_warned;					// "no API" message printed since the menu opened

/*
=================
LE_Api

Returns the light edit table of the renderer, or NULL when it is missing, of another version
or not available (no RTX path, no world).
=================
*/
static const rtxLightEditAPI_t *LE_Api( void )
{
	if ( !re.GetExtension )
	{
		return NULL;
	}

	const rtxLightEditAPI_t *api = (const rtxLightEditAPI_t *)re.GetExtension( RTX_LIGHTEDIT_API_NAME );
	if ( !api || api->version != RTX_LIGHTEDIT_API_VERSION )
	{
		return NULL;
	}
	if ( !api->IsAvailable || !api->Count || !api->Get || !api->GetStats || !api->Generation )
	{
		return NULL;
	}
	return api->IsAvailable() ? api : NULL;
}

// Same as LE_Api; prints a message the first time it fails after the menu opened.
static const rtxLightEditAPI_t *LE_ApiOrWarn( void )
{
	const rtxLightEditAPI_t *api = LE_Api();

	if ( !api && !s_warned )
	{
		s_warned = qtrue;
		Com_Printf( "light edit: the renderer gives no light list (RTX path off or no world)\n" );
	}
	return api;
}

static void LE_CvarString( const char *name, char *out, int size )
{
	ui.Cvar_VariableStringBuffer( name, out, size );
}

static void LE_Exec( const char *fmt, ... )
{
	va_list	args;
	char	text[256];

	va_start( args, fmt );
	Q_vsnprintf( text, sizeof( text ) - 1, fmt, args );
	va_end( args );
	Q_strcat( text, sizeof( text ), "\n" );
	ui.Cmd_ExecuteText( EXEC_APPEND, text );
}

static void LE_CvarFloat( const char *name, float value )
{
	ui.Cvar_Set( name, va( "%g", value ) );
}

static void LE_EnsureCvars( void )
{
	ui.Cvar_Create( "ui_ledit_filter", "all", 0 );
	ui.Cvar_Create( "ui_ledit_search", "", 0 );
	ui.Cvar_Create( "ui_ledit_row", "-1", 0 );
	ui.Cvar_Create( "ui_ledit_hue", "0", 0 );
	ui.Cvar_Create( "ui_ledit_sat", "0", 0 );
	ui.Cvar_Create( "ui_ledit_val", "1", 0 );
	ui.Cvar_Create( "ui_ledit_temp", "6500", 0 );
	ui.Cvar_Create( "ui_ledit_intensity", "0", 0 );
	ui.Cvar_Create( "ui_ledit_radius", "0", 0 );
}

/*
=================
LE_ParseBuffer

Parses the menu text like UI_ParseMenu does, from a buffer that the caller keeps.
The parse session ends on every path and the buffer is not freed here.
=================
*/
static void LE_ParseBuffer( const char *text, const char *name )
{
	COM_BeginParseSession();
	Q_strncpyz( parseData[parseDataCount].fileName, name, sizeof( parseData[0].fileName ) );
	parseData[parseDataCount].bufferStart = text;
	parseData[parseDataCount].bufferCurrent = text;

	while ( 1 )
	{
		const char *token = PC_ParseExt();

		if ( !*token || *token == '}' )
		{
			break;
		}
		if ( *token == '{' )
		{
			continue;
		}
		if ( Q_stricmp( token, "menudef" ) == 0 )
		{
			Menu_New( (char *)text );
			continue;
		}
		PC_ParseWarning( va( "Invalid keyword '%s'", token ) );
	}

	COM_EndParseSession();
}

/*
=================
LE_LoadMenu

Loads the menu from ui/lightedit.menu when the file exists, else from the embedded text.
=================
*/
static qboolean LE_LoadMenu( void )
{
	if ( Menus_FindByName( LE_MENU_NAME ) )
	{
		return qtrue;
	}
	if ( Menu_Count() >= MAX_MENUS )
	{
		Com_Printf( "light edit: too many menus, cannot load %s\n", LE_MENU_NAME );
		return qfalse;
	}

	char *fileBuffer = NULL;
	const long len = ui.FS_ReadFile( LE_MENU_FILE, (void **)&fileBuffer );

	if ( len > 0 && fileBuffer )
	{
		std::string text( fileBuffer, (size_t)len );

		ui.FS_FreeFile( fileBuffer );
		LE_ParseBuffer( text.c_str(), LE_MENU_FILE );
	}
	else
	{
		std::string text( s_menuTextHead );

		text += s_menuTextTail;
		LE_ParseBuffer( text.c_str(), "lightedit (built in)" );
	}

	if ( !Menus_FindByName( LE_MENU_NAME ) )
	{
		Com_Printf( "light edit: the menu %s did not load\n", LE_MENU_NAME );
		return qfalse;
	}
	return qtrue;
}

/*
=================
UI_LightEdit_ConsoleCommand

ledit_menu: opens the menu while the mode is active. UI_ConsoleCommand calls this before its
save check, because the game refuses saves during the mode.
=================
*/
qboolean UI_LightEdit_ConsoleCommand( void )
{
	char cmd[MAX_STRING_CHARS];
	char active[16];

	ui.Argv( 0, cmd, sizeof( cmd ) );
	if ( Q_stricmp( cmd, "ledit_menu" ) != 0 )
	{
		return qfalse;
	}

	LE_CvarString( "ledit_active", active, sizeof( active ) );
	if ( atoi( active ) != 1 )
	{
		Com_Printf( "light edit: not active\n" );
		return qtrue;
	}
	// The cvars come first: the menu parse creates its item cvars with an empty value.
	LE_EnsureCvars();
	if ( !LE_LoadMenu() )
	{
		return qtrue;
	}

	s_warned = qfalse;
	s_rowsValid = qfalse;
	LE_ApiOrWarn();

	Menus_CloseByName( "mainhud" );
	Menus_ActivateByName( LE_MENU_NAME );
	UI_Cursor_Show( qtrue );
	ui.Key_SetCatcher( ui.Key_GetCatcher() | KEYCATCH_UI );
	return qtrue;
}

/*
=================
LE_RowPasses

Tests one light against the filter and the lower-case search text.
=================
*/
static qboolean LE_RowPasses( const rtxLightDesc_t *d, const char *filter, const char *search )
{
	if ( d->flags & RTX_LFLAG_DELETED )
	{
		return qfalse;
	}
	if ( !Q_stricmp( filter, "added" ) && d->source != RTX_LSRC_ADDED )
	{
		return qfalse;
	}
	if ( !Q_stricmp( filter, "entity" ) && d->source != RTX_LSRC_ENTITY )
	{
		return qfalse;
	}
	if ( !Q_stricmp( filter, "lgt" ) && d->source != RTX_LSRC_LGT )
	{
		return qfalse;
	}
	if ( !Q_stricmp( filter, "modified" ) && !( d->flags & RTX_LFLAG_MODIFIED ) )
	{
		return qfalse;
	}
	if ( !Q_stricmp( filter, "disabled" ) && !( d->flags & RTX_LFLAG_DISABLED ) )
	{
		return qfalse;
	}
	if ( !Q_stricmp( filter, "spots" ) && d->type != RTX_LTYPE_SPOT )
	{
		return qfalse;
	}
	if ( search[0] )
	{
		char name[RTX_LIGHTEDIT_NAME_LEN];

		Q_strncpyz( name, d->name, sizeof( name ) );
		Q_strlwr( name );
		if ( !strstr( name, search ) )
		{
			return qfalse;
		}
	}
	return qtrue;
}

/*
=================
LE_Rows

Returns the ids of the visible lights. The list rebuilds when the light list, the filter or
the search text changes.
=================
*/
static const std::vector<int> &LE_Rows( const rtxLightEditAPI_t *api )
{
	char filter[LE_FILTER_LEN], search[LE_SEARCH_LEN];

	if ( !api )
	{
		s_rows.clear();
		s_rowsValid = qfalse;
		return s_rows;
	}

	LE_CvarString( "ui_ledit_filter", filter, sizeof( filter ) );
	LE_CvarString( "ui_ledit_search", search, sizeof( search ) );
	Q_strlwr( search );

	const int count = api->Count();
	const int generation = api->Generation();

	if ( s_rowsValid && count == s_rowsCount && generation == s_rowsGeneration
		&& !strcmp( filter, s_rowsFilter ) && !strcmp( search, s_rowsSearch ) )
	{
		return s_rows;
	}

	s_rows.clear();
	for ( int id = 0; id < count; id++ )
	{
		rtxLightDesc_t d;

		if ( api->Get( id, &d ) && LE_RowPasses( &d, filter, search ) )
		{
			s_rows.push_back( id );
		}
	}

	s_rowsValid = qtrue;
	s_rowsCount = count;
	s_rowsGeneration = generation;
	Q_strncpyz( s_rowsFilter, filter, sizeof( s_rowsFilter ) );
	Q_strncpyz( s_rowsSearch, search, sizeof( s_rowsSearch ) );
	return s_rows;
}

int UI_LightEdit_FeederCount( void )
{
	return (int)LE_Rows( LE_Api() ).size();
}

/*
=================
UI_LightEdit_FeederItemText

Columns: id, source, type, intensity, flags (M modified, D disabled, S in solid, m muted), name.
=================
*/
const char *UI_LightEdit_FeederItemText( int index, int column )
{
	static char		buffers[8][64];
	static int		next;
	rtxLightDesc_t	d;

	const rtxLightEditAPI_t *api = LE_Api();
	const std::vector<int> &rows = LE_Rows( api );

	if ( !api || index < 0 || index >= (int)rows.size() || !api->Get( rows[index], &d ) )
	{
		return "";
	}

	char *out = buffers[next++ & 7];

	switch ( column )
	{
	case 0:
		Com_sprintf( out, 64, "%d", d.id );
		break;
	case 1:
		if ( d.source == RTX_LSRC_ADDED )
		{
			Com_sprintf( out, 64, "added" );
		}
		else
		{
			Com_sprintf( out, 64, "%s %d", d.source == RTX_LSRC_ENTITY ? "ent" : "lgt", d.sourceKey );
		}
		break;
	case 2:
		Com_sprintf( out, 64, "%s", d.type == RTX_LTYPE_SPOT ? "spot" : "sphere" );
		break;
	case 3:
		Com_sprintf( out, 64, "%.5g", d.intensity );
		break;
	case 4:
		Com_sprintf( out, 64, "%s%s%s%s", ( d.flags & RTX_LFLAG_MODIFIED ) ? "M" : "", ( d.flags & RTX_LFLAG_DISABLED ) ? "D" : "",
			( d.flags & RTX_LFLAG_IN_SOLID ) ? "S" : "", ( d.flags & RTX_LFLAG_MUTED ) ? "m" : "" );
		break;
	case 5:
		Com_sprintf( out, 64, "%s", d.name );
		break;
	default:
		return "";
	}
	return out;
}

/*
=================
UI_LightEdit_FeederSelection

A click on a row stores its id in ui_ledit_row and selects the light in the cgame.
=================
*/
void UI_LightEdit_FeederSelection( int index )
{
	const rtxLightEditAPI_t *api = LE_Api();
	const std::vector<int> &rows = LE_Rows( api );

	if ( !api || index < 0 || index >= (int)rows.size() )
	{
		return;
	}
	ui.Cvar_Set( "ui_ledit_row", va( "%d", rows[index] ) );
	LE_Exec( "ledit_select %d", rows[index] );
}

/*
=================
LE_HsvToRgb / LE_RgbToHsv

Hue in degrees (0..360), saturation and value in 0..1.
=================
*/
static void LE_HsvToRgb( float h, float s, float v, float *rgb )
{
	h = h - 360.0f * floorf( h / 360.0f );
	s = Com_Clamp( 0.0f, 1.0f, s );
	v = Com_Clamp( 0.0f, 1.0f, v );

	const float c = v * s;
	const float hp = h / 60.0f;
	const float x = c * ( 1.0f - fabsf( fmodf( hp, 2.0f ) - 1.0f ) );
	float r = 0.0f, g = 0.0f, b = 0.0f;

	if ( hp < 1.0f )		{ r = c; g = x; }
	else if ( hp < 2.0f )	{ r = x; g = c; }
	else if ( hp < 3.0f )	{ g = c; b = x; }
	else if ( hp < 4.0f )	{ g = x; b = c; }
	else if ( hp < 5.0f )	{ r = x; b = c; }
	else					{ r = c; b = x; }

	const float m = v - c;
	rgb[0] = r + m;
	rgb[1] = g + m;
	rgb[2] = b + m;
}

static void LE_RgbToHsv( const float *rgb, float *h, float *s, float *v )
{
	const float mx = Q_max( rgb[0], Q_max( rgb[1], rgb[2] ) );
	const float mn = Q_min( rgb[0], Q_min( rgb[1], rgb[2] ) );
	const float d = mx - mn;

	*v = mx;
	*s = mx > 0.0f ? d / mx : 0.0f;
	if ( d <= 0.0f )
	{
		*h = 0.0f;
		return;
	}
	if ( mx == rgb[0] )			*h = 60.0f * fmodf( ( rgb[1] - rgb[2] ) / d, 6.0f );
	else if ( mx == rgb[1] )	*h = 60.0f * ( ( rgb[2] - rgb[0] ) / d + 2.0f );
	else						*h = 60.0f * ( ( rgb[0] - rgb[1] ) / d + 4.0f );
	if ( *h < 0.0f )
	{
		*h += 360.0f;
	}
}

/*
=================
LE_TemperatureToRgb

Black body colour of 1000..12000 K (Tanner Helland approximation), normalised to a maximum of 1.
=================
*/
static void LE_TemperatureToRgb( float kelvin, float *rgb )
{
	const float t = Com_Clamp( 1000.0f, 12000.0f, kelvin ) / 100.0f;
	float r, g, b;

	r = t <= 66.0f ? 255.0f : 329.698727446f * powf( t - 60.0f, -0.1332047592f );
	g = t <= 66.0f ? 99.4708025861f * logf( t ) - 161.1195681661f : 288.1221695283f * powf( t - 60.0f, -0.0755148492f );
	b = t >= 66.0f ? 255.0f : ( t <= 19.0f ? 0.0f : 138.5177312231f * logf( t - 10.0f ) - 305.0447927307f );

	rgb[0] = Com_Clamp( 0.0f, 255.0f, r ) / 255.0f;
	rgb[1] = Com_Clamp( 0.0f, 255.0f, g ) / 255.0f;
	rgb[2] = Com_Clamp( 0.0f, 255.0f, b ) / 255.0f;

	const float mx = Q_max( rgb[0], Q_max( rgb[1], rgb[2] ) );
	if ( mx > 0.0f )
	{
		rgb[0] /= mx;
		rgb[1] /= mx;
		rgb[2] /= mx;
	}
}

static void LE_PickerColor( float *rgb )
{
	LE_HsvToRgb( ui.Cvar_VariableValue( "ui_ledit_hue" ), ui.Cvar_VariableValue( "ui_ledit_sat" ),
		ui.Cvar_VariableValue( "ui_ledit_val" ), rgb );
}

void UI_LightEdit_DrawSwatch( float x, float y, float w, float h )
{
	vec4_t	color;
	float	rgb[3];

	LE_PickerColor( rgb );
	VectorCopy( rgb, color );
	color[3] = 1.0f;
	DC->fillRect( x, y, w, h, color );

	const vec4_t border = { 1.0f, 1.0f, 1.0f, 0.8f };
	DC->drawRect( x, y, w, h, 1.0f, border );
}

void UI_LightEdit_DrawStats( float x, float y, float scale, const vec4_t color, int fontIndex )
{
	rtxLightStats_t			st;
	const rtxLightEditAPI_t	*api = LE_Api();
	char					line[160];

	if ( !api )
	{
		DC->drawText( x, y, scale, (float *)color, "no light data (RTX path off or no world)", 0, ITEM_TEXTSTYLE_SHADOWED, fontIndex );
		return;
	}

	memset( &st, 0, sizeof( st ) );
	api->GetStats( &st );

	Com_sprintf( line, sizeof( line ), "Map %s    lights %d    emissive %d%s", st.mapName, st.numRecords, st.numEmissive,
		st.unsavedChanges ? "    * unsaved" : "" );
	DC->drawText( x, y, scale, (float *)color, line, 0, ITEM_TEXTSTYLE_SHADOWED, fontIndex );

	Com_sprintf( line, sizeof( line ), "entity %d  lgt %d  added %d  modified %d  disabled %d  in solid %d",
		st.numEntity, st.numLgt, st.numAdded, st.numModified, st.numDisabled, st.numInSolid );
	DC->drawText( x, y + 11.0f, scale, (float *)color, line, 0, ITEM_TEXTSTYLE_SHADOWED, fontIndex );
}

/*
=================
LE_CurrentLight

Fills d with the light whose id is in ui_ledit_row. Prints a message and returns qfalse when
there is no such light.
=================
*/
static qboolean LE_CurrentLight( const rtxLightEditAPI_t *api, rtxLightDesc_t *d )
{
	if ( !api )
	{
		return qfalse;
	}

	const int id = (int)ui.Cvar_VariableValue( "ui_ledit_row" );

	if ( id < 0 || id >= api->Count() || !api->Get( id, d ) || ( d->flags & RTX_LFLAG_DELETED ) )
	{
		Com_Printf( "light edit: choose a light in the list first\n" );
		return qfalse;
	}
	return qtrue;
}

static qboolean LE_ParseNumber( const char *cvarName, float *out )
{
	char	text[64];
	char	*end;

	LE_CvarString( cvarName, text, sizeof( text ) );
	*out = (float)strtod( text, &end );
	if ( end == text || *end )
	{
		Com_Printf( "light edit: %s is not a number\n", cvarName );
		return qfalse;
	}
	return qtrue;
}

/*
=================
UI_LightEdit_RunScript

The uiScript names of the menu. Only the names that start with "ledit_" are handled.
=================
*/
qboolean UI_LightEdit_RunScript( const char *name )
{
	rtxLightDesc_t	d;
	float			rgb[3], v;

	if ( Q_stricmpn( name, "ledit_", 6 ) != 0 )
	{
		return qfalse;
	}
	name += 6;

	if ( !Q_stricmp( name, "close" ) )
	{
		ui.Key_SetCatcher( ui.Key_GetCatcher() & ~KEYCATCH_UI );
		ui.Key_ClearStates();
		Menus_CloseAll();
		Menus_ActivateByName( "mainhud" );
		return qtrue;
	}

	const rtxLightEditAPI_t *api = LE_ApiOrWarn();

	if ( !api )
	{
		return qtrue;
	}

	if ( !Q_stricmp( name, "endsolo" ) )
	{
		LE_Exec( "pt_ledit_solo -1" );
	}
	else if ( !LE_CurrentLight( api, &d ) )
	{
		return qtrue;
	}
	else if ( !Q_stricmp( name, "select" ) )
	{
		LE_Exec( "ledit_select %d", d.id );
	}
	else if ( !Q_stricmp( name, "addsel" ) )
	{
		LE_Exec( "ledit_select %d add", d.id );
	}
	else if ( !Q_stricmp( name, "goto" ) )
	{
		LE_Exec( "ledit_select %d; ledit_goto", d.id );
	}
	else if ( !Q_stricmp( name, "delete" ) )
	{
		LE_Exec( "ledit_select %d; ledit_delete", d.id );
	}
	else if ( !Q_stricmp( name, "restore" ) )
	{
		LE_Exec( "pt_ledit_restore %d", d.id );
	}
	else if ( !Q_stricmp( name, "solo" ) )
	{
		LE_Exec( "pt_ledit_solo %d", d.id );
	}
	else if ( !Q_stricmp( name, "revert" ) )
	{
		LE_Exec( "ledit_select %d; ledit_revert", d.id );
	}
	else if ( !Q_stricmp( name, "load" ) )
	{
		float h, s, val;

		LE_RgbToHsv( d.color, &h, &s, &val );
		LE_CvarFloat( "ui_ledit_hue", h );
		LE_CvarFloat( "ui_ledit_sat", s );
		LE_CvarFloat( "ui_ledit_val", val );
		LE_CvarFloat( "ui_ledit_intensity", d.intensity );
		LE_CvarFloat( "ui_ledit_radius", d.radius );
	}
	else if ( !Q_stricmp( name, "applycolor" ) )
	{
		LE_PickerColor( rgb );
		LE_Exec( "ledit_select %d; ledit_set color %.4f %.4f %.4f", d.id, rgb[0], rgb[1], rgb[2] );
	}
	else if ( !Q_stricmp( name, "usetemp" ) )
	{
		float h, s, val;

		LE_TemperatureToRgb( ui.Cvar_VariableValue( "ui_ledit_temp" ), rgb );
		LE_RgbToHsv( rgb, &h, &s, &val );
		LE_CvarFloat( "ui_ledit_hue", h );
		LE_CvarFloat( "ui_ledit_sat", s );
		LE_CvarFloat( "ui_ledit_val", val );
	}
	else if ( !Q_stricmp( name, "applytemp" ) )
	{
		LE_TemperatureToRgb( ui.Cvar_VariableValue( "ui_ledit_temp" ), rgb );
		LE_Exec( "ledit_select %d; ledit_set color %.4f %.4f %.4f", d.id, rgb[0], rgb[1], rgb[2] );
	}
	else if ( !Q_stricmp( name, "applyintensity" ) )
	{
		if ( LE_ParseNumber( "ui_ledit_intensity", &v ) )
		{
			LE_Exec( "ledit_select %d; ledit_set intensity %g", d.id, v );
		}
	}
	else if ( !Q_stricmp( name, "applyradius" ) )
	{
		if ( LE_ParseNumber( "ui_ledit_radius", &v ) )
		{
			LE_Exec( "ledit_select %d; ledit_set radius %g", d.id, v );
		}
	}
	else
	{
		return qfalse;
	}
	return qtrue;
}
