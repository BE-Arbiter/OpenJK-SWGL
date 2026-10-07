/*
===========================================================================
Menu to edit the cloth and hair physics (.phys) of the player model.

The renderer owns the definition. This file keeps the lists of the menu, the
text of the group that is edited and the text area. The menu is
ui/physedit.menu, it opens with the console command ph_menu.
===========================================================================
*/

#include "../server/exe_headers.h"

#include "ui_local.h"
#include "ui_shared.h"
#include "../qcommon/q_shared.h"

#include <string>
#include <vector>

#define FEEDER_PHYS_UNUSED	0x30
#define FEEDER_PHYS_USED	0x31
#define FEEDER_PHYS_BONES	0x32

#define UI_PHYS_TEXT		4000
#define UI_PHYS_HELP		4001
#define UI_PHYS_STATUS		4002

#define PHYS_TEXT_MAX		6000
#define PHYS_REPLY_MAX		16384

extern qboolean ItemParse_model_g2anim_go( itemDef_t *item, const char *animName );
extern qboolean ItemParse_asset_model_go( itemDef_t *item, const char *name );
extern qboolean ItemParse_model_g2skin_go( itemDef_t *item, const char *skinName );
menuDef_t	*Menus_FindByName( const char *p );

struct physUsedEntry_t
{
	int			index;		// group index in the definition
	std::string	name;
	std::string	surfaces;
};

static struct
{
	bool						menuLoaded;
	std::string					modelPath;
	std::vector<std::string>	unused;
	std::vector<physUsedEntry_t> used;
	std::vector<std::string>	bones;			// with the indent of the hierarchy
	std::vector<std::string>	boneNames;
	int							selBone;
	int							selUnused;
	int							selUsed;			// index in used, -1 = none
	char						text[PHYS_TEXT_MAX];
	int							textLen;
	int							caret;
	int							scroll;				// first line shown
	rectDef_t					textRect;
	float						textScale;
	int							textFont;
	bool						statusError;
	std::string					status;
	int							helpScroll;
} s_phys = { false };

// The help is the text of docs/phys-format.md, made by docs/phys/build_pk3.py.
static std::vector<std::string>	s_helpRaw;
static std::vector<std::string>	s_helpWrapped;
static float					s_helpWrapWidth = -1.0f;

static void Phys_LoadHelp( void )
{
	fileHandle_t f;
	const int len = ui.FS_FOpenFile( "ui/physhelp.txt", &f, FS_READ );

	s_helpRaw.clear();
	s_helpWrapWidth = -1.0f;

	if ( len <= 0 )
	{
		s_helpRaw.push_back( "ui/physhelp.txt is missing." );
		return;
	}

	std::string data( len, '\0' );

	ui.FS_Read( &data[0], len, f );
	ui.FS_FCloseFile( f );

	std::string line;

	for ( size_t i = 0; i < data.size(); i++ )
	{
		if ( data[i] == '\n' )
		{
			s_helpRaw.push_back( line );
			line.clear();
		}
		else if ( data[i] != '\r' )
		{
			line += data[i];
		}
	}

	if ( !line.empty() )
		s_helpRaw.push_back( line );
}

// Cuts the lines of the help to the width of the panel.
static void Phys_WrapHelp( float width, float scale, int font )
{
	if ( width == s_helpWrapWidth )
		return;

	s_helpWrapWidth = width;
	s_helpWrapped.clear();

	for ( size_t i = 0; i < s_helpRaw.size(); i++ )
	{
		const std::string &raw = s_helpRaw[i];
		size_t indentLen = 0;

		while ( indentLen < raw.size() && raw[indentLen] == ' ' )
			indentLen++;

		const std::string indent = raw.substr( 0, indentLen );
		std::string line = indent;
		size_t pos = indentLen;

		if ( raw.empty() )
		{
			s_helpWrapped.push_back( "" );
			continue;
		}

		while ( pos < raw.size() )
		{
			size_t end = raw.find( ' ', pos );

			if ( end == std::string::npos )
				end = raw.size();

			const std::string word = raw.substr( pos, end - pos );
			const std::string candidate = ( line.size() > indent.size() ) ? line + " " + word : line + word;

			if ( line.size() > indent.size() && DC->textWidth( candidate.c_str(), scale, font ) > width )
			{
				s_helpWrapped.push_back( line );
				line = indent + word;
			}
			else
			{
				line = candidate;
			}

			pos = end + 1;
		}

		s_helpWrapped.push_back( line );
	}
}

static int Phys_Cmd( const char *cmd, const char *arg, char *out, int outSize )
{
	if ( !ui.PhysCommand )
	{
		Q_strncpyz( out, "This renderer has no phys editor", outSize );
		return -1;
	}

	return ui.PhysCommand( cmd, arg, out, outSize );
}

static void Phys_Insert( char c );

static void Phys_SetStatus( const char *msg, bool error )
{
	s_phys.status = msg;
	s_phys.statusError = error;
}

static void Phys_SetText( const char *text )
{
	Q_strncpyz( s_phys.text, text, sizeof( s_phys.text ) );
	s_phys.textLen = (int)strlen( s_phys.text );
	s_phys.caret = 0;
	s_phys.scroll = 0;
}

// Splits "a\tb\n" lines into fields.
static void Phys_Lines( const char *reply, std::vector<std::vector<std::string> > &lines )
{
	lines.clear();

	const char *p = reply;

	while ( *p )
	{
		std::vector<std::string> fields;
		std::string field;

		while ( *p && *p != '\n' )
		{
			if ( *p == '\t' )
			{
				fields.push_back( field );
				field.clear();
			}
			else
			{
				field += *p;
			}

			p++;
		}

		fields.push_back( field );
		lines.push_back( fields );

		if ( *p == '\n' )
			p++;
	}
}

static void Phys_Highlight( const char *surfaces )
{
	char reply[256];

	Phys_Cmd( "highlight", surfaces, reply, sizeof( reply ) );
}

static void Phys_RefreshLists( void )
{
	static char reply[PHYS_REPLY_MAX];
	std::vector<std::vector<std::string> > lines;

	s_phys.unused.clear();
	s_phys.used.clear();

	if ( Phys_Cmd( "surfaces", "", reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 2 && atoi( lines[i][1].c_str() ) < 0 )
				s_phys.unused.push_back( lines[i][0] );
		}
	}

	if ( Phys_Cmd( "groups", "", reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 3 )
			{
				physUsedEntry_t e;

				e.index = atoi( lines[i][0].c_str() );
				e.name = lines[i][1];
				e.surfaces = lines[i][2];
				s_phys.used.push_back( e );
			}
		}
	}

	if ( s_phys.bones.empty() && Phys_Cmd( "bones", "", reply, sizeof( reply ) ) == 0 )
	{
		Phys_Lines( reply, lines );

		for ( size_t i = 0; i < lines.size(); i++ )
		{
			if ( lines[i].size() >= 2 )
			{
				s_phys.boneNames.push_back( lines[i][0] );
				s_phys.bones.push_back( std::string( atoi( lines[i][1].c_str() ) * 2, ' ' ) + lines[i][0] );
			}
		}
	}

	if ( s_phys.selUnused >= (int)s_phys.unused.size() )
		s_phys.selUnused = -1;

	if ( s_phys.selUsed >= (int)s_phys.used.size() )
		s_phys.selUsed = -1;
}

static void Phys_SelectUsed( int index )
{
	static char reply[PHYS_REPLY_MAX];

	s_phys.selUsed = index;

	if ( index < 0 || index >= (int)s_phys.used.size() )
	{
		Phys_SetText( "" );
		Phys_Highlight( "" );
		return;
	}

	char arg[16];

	Com_sprintf( arg, sizeof( arg ), "%d", s_phys.used[index].index );

	if ( Phys_Cmd( "get", arg, reply, sizeof( reply ) ) == 0 )
		Phys_SetText( reply );
	else
		Phys_SetStatus( reply, true );

	Phys_Highlight( s_phys.used[index].surfaces.c_str() );
}

static void Phys_UpdatePreview( void )
{
	menuDef_t *menu = Menus_FindByName( "physEdit" );
	itemDef_t *item = menu ? Menu_FindItemByName( menu, "character" ) : NULL;

	if ( !item )
		return;

	ItemParse_model_g2anim_go( item, "BOTH_STAND1" );
	ItemParse_asset_model_go( item, s_phys.modelPath.c_str() );

	// The skin of the player: it sets the surfaces on and off like in the game.
	ItemParse_model_g2skin_go( item, va( "models/players/%s/|%s|%s|%s", Cvar_VariableString( "g_char_model" ),
		Cvar_VariableString( "g_char_skin_head" ), Cvar_VariableString( "g_char_skin_torso" ), Cvar_VariableString( "g_char_skin_legs" ) ) );
}

// Parses the menu file once.
void UI_Phys_LoadMenu( void )
{
	extern void UI_ParseMenu( const char *menuFile );

	if ( !s_phys.menuLoaded )
	{
		UI_ParseMenu( "ui/physedit.menu" );
		s_phys.menuLoaded = true;
	}
}

static void Phys_Open( void )
{
	char reply[256];

	s_phys.selUnused = -1;
	s_phys.selUsed = -1;
	s_phys.selBone = -1;
	s_phys.bones.clear();
	s_phys.boneNames.clear();
	s_phys.helpScroll = 0;
	Phys_LoadHelp();
	s_phys.modelPath = va( "models/players/%s/model.glm", Cvar_VariableString( "g_char_model" ) );

	Phys_SetText( "" );
	Phys_UpdatePreview();

	if ( Phys_Cmd( "model", s_phys.modelPath.c_str(), reply, sizeof( reply ) ) != 0 )
	{
		Phys_SetStatus( reply, true );
		s_phys.unused.clear();
		s_phys.used.clear();
		return;
	}

	Phys_SetStatus( va( "Editing %s", s_phys.modelPath.c_str() ), false );
	Phys_RefreshLists();
}

// Console test: the text with | for the line breaks.
void UI_Phys_SetText( const char *text )
{
	std::string t = text;

	for ( size_t i = 0; i < t.size(); i++ )
	{
		if ( t[i] == '|' )
			t[i] = '\n';
	}

	Phys_SetText( t.c_str() );
}

qboolean UI_Phys_Script( const char *name, const char **args )
{
	static char reply[PHYS_REPLY_MAX];

	if ( !Q_stricmp( name, "physOpen" ) )
	{
		Phys_Open();
		return qtrue;
	}

	if ( !Q_stricmp( name, "physRefresh" ) )
	{
		if ( s_phys.selUsed < 0 )
		{
			Phys_SetStatus( "Select a group in Used", true );
			return qtrue;
		}

		// "<group>\n<text>"
		std::string arg = va( "%d\n", s_phys.used[s_phys.selUsed].index );

		arg += s_phys.text;

		if ( Phys_Cmd( "set", arg.c_str(), reply, sizeof( reply ) ) == 0 )
		{
			Phys_SetStatus( "Applied to the model", false );
			Phys_RefreshLists();
			Phys_SelectUsed( s_phys.selUsed );
		}
		else
		{
			Phys_SetStatus( reply, true );
		}

		return qtrue;
	}

	if ( !Q_stricmp( name, "physInsertBone" ) )
	{
		if ( s_phys.selBone < 0 || s_phys.selBone >= (int)s_phys.boneNames.size() )
		{
			Phys_SetStatus( "Select a bone in Bones", true );
			return qtrue;
		}

		const std::string &bone = s_phys.boneNames[s_phys.selBone];

		for ( size_t i = 0; i < bone.size(); i++ )
			Phys_Insert( bone[i] );

		Phys_SetStatus( va( "Inserted %s", bone.c_str() ), false );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physAdd" ) )
	{
		if ( s_phys.selUnused < 0 || s_phys.selUnused >= (int)s_phys.unused.size() )
		{
			Phys_SetStatus( "Select a surface in Unused", true );
			return qtrue;
		}

		if ( Phys_Cmd( "add", s_phys.unused[s_phys.selUnused].c_str(), reply, sizeof( reply ) ) == 0 )
		{
			const int group = atoi( reply );

			Phys_RefreshLists();
			s_phys.selUnused = -1;

			for ( size_t i = 0; i < s_phys.used.size(); i++ )
			{
				if ( s_phys.used[i].index == group )
					Phys_SelectUsed( (int)i );
			}

			Phys_SetStatus( "Group added. Edit it and press Refresh", false );
			Cvar_Set( "ui_ph_tab", "1" );
		}
		else
		{
			Phys_SetStatus( reply, true );
		}

		return qtrue;
	}

	if ( !Q_stricmp( name, "physRemove" ) )
	{
		if ( s_phys.selUsed < 0 )
		{
			Phys_SetStatus( "Select a group in Used", true );
			return qtrue;
		}

		char arg[16];

		Com_sprintf( arg, sizeof( arg ), "%d", s_phys.used[s_phys.selUsed].index );

		if ( Phys_Cmd( "remove", arg, reply, sizeof( reply ) ) == 0 )
		{
			Phys_RefreshLists();
			Phys_SelectUsed( -1 );
			Phys_SetStatus( "Group removed", false );
		}
		else
		{
			Phys_SetStatus( reply, true );
		}

		return qtrue;
	}

	if ( !Q_stricmp( name, "physSave" ) )
	{
		if ( Phys_Cmd( "save", "", reply, sizeof( reply ) ) == 0 )
			Com_Printf( "phys: written %s\n", reply );
		else
			Com_Printf( S_COLOR_RED "phys: %s\n", reply );

		Phys_Highlight( "" );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physRevert" ) )
	{
		Phys_Cmd( "revert", "", reply, sizeof( reply ) );
		Phys_Highlight( "" );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physClearHighlight" ) )
	{
		Phys_Highlight( "" );
		Phys_Cmd( "bonemarks", "0", reply, sizeof( reply ) );
		Phys_Cmd( "bonesel", "-1", reply, sizeof( reply ) );
		return qtrue;
	}

	if ( !Q_stricmp( name, "physBonesOn" ) )
	{
		Phys_Cmd( "bonemarks", "1", reply, sizeof( reply ) );
		return qtrue;
	}

	return qfalse;
}

//
// Feeders
//

int UI_Phys_FeederCount( float feederID )
{
	if ( feederID == FEEDER_PHYS_UNUSED )
		return (int)s_phys.unused.size();

	if ( feederID == FEEDER_PHYS_USED )
		return (int)s_phys.used.size();

	if ( feederID == FEEDER_PHYS_BONES )
		return (int)s_phys.bones.size();

	return 0;
}

const char *UI_Phys_FeederItemText( float feederID, int index, int column )
{
	static char text[256];

	if ( feederID == FEEDER_PHYS_UNUSED && index >= 0 && index < (int)s_phys.unused.size() )
		return s_phys.unused[index].c_str();

	if ( feederID == FEEDER_PHYS_BONES && index >= 0 && index < (int)s_phys.bones.size() )
		return s_phys.bones[index].c_str();

	if ( feederID == FEEDER_PHYS_USED && index >= 0 && index < (int)s_phys.used.size() )
	{
		const physUsedEntry_t &e = s_phys.used[index];

		Com_sprintf( text, sizeof( text ), "%s  [%s]", e.name.c_str(), e.surfaces.c_str() );
		return text;
	}

	return "";
}

qboolean UI_Phys_FeederSelection( float feederID, int index )
{
	if ( feederID == FEEDER_PHYS_UNUSED )
	{
		s_phys.selUnused = index;

		if ( index >= 0 && index < (int)s_phys.unused.size() )
			Phys_Highlight( s_phys.unused[index].c_str() );

		return qtrue;
	}

	if ( feederID == FEEDER_PHYS_USED )
	{
		Phys_SelectUsed( index );
		return qtrue;
	}

	if ( feederID == FEEDER_PHYS_BONES )
	{
		char reply[64];
		char arg[16];

		s_phys.selBone = index;
		Com_sprintf( arg, sizeof( arg ), "%d", index );
		Phys_Cmd( "bonesel", arg, reply, sizeof( reply ) );
		return qtrue;
	}

	return qfalse;
}

//
// Owner draws
//

static int Phys_LineHeight( void )
{
	return DC->textHeight( "Ag", s_phys.textScale, s_phys.textFont ) + 2;
}

// Start of the line, and its length.
static int Phys_LineStart( int line )
{
	int start = 0;

	for ( int l = 0; l < line && start < s_phys.textLen; l++ )
	{
		while ( start < s_phys.textLen && s_phys.text[start] != '\n' )
			start++;

		if ( start < s_phys.textLen )
			start++;
	}

	return start;
}

static int Phys_LineEnd( int start )
{
	while ( start < s_phys.textLen && s_phys.text[start] != '\n' )
		start++;

	return start;
}

static int Phys_LineOfCaret( void )
{
	int line = 0;

	for ( int i = 0; i < s_phys.caret && i < s_phys.textLen; i++ )
	{
		if ( s_phys.text[i] == '\n' )
			line++;
	}

	return line;
}

static int Phys_LineCount( void )
{
	int lines = 1;

	for ( int i = 0; i < s_phys.textLen; i++ )
	{
		if ( s_phys.text[i] == '\n' )
			lines++;
	}

	return lines;
}

static void Phys_PaintText( float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	static const vec4_t back = { 0.04f, 0.04f, 0.04f, 0.95f };
	static const vec4_t border = { 1.0f, 0.682f, 0.0f, 0.6f };
	char buf[512];

	s_phys.textRect.x = x;
	s_phys.textRect.y = y;
	s_phys.textRect.w = w;
	s_phys.textRect.h = h;
	s_phys.textScale = scale;
	s_phys.textFont = font;

	DC->fillRect( x, y, w, h, back );
	DC->drawRect( x, y, w, h, 1, border );

	const int lineH = Phys_LineHeight();
	const int visible = lineH > 0 ? (int)( ( h - 4 ) / lineH ) : 1;
	const int caretLine = Phys_LineOfCaret();

	if ( caretLine < s_phys.scroll )
		s_phys.scroll = caretLine;
	else if ( caretLine >= s_phys.scroll + visible )
		s_phys.scroll = caretLine - visible + 1;

	if ( s_phys.scroll < 0 )
		s_phys.scroll = 0;

	int start = Phys_LineStart( s_phys.scroll );

	for ( int row = 0; row < visible && start <= s_phys.textLen; row++ )
	{
		const int end = Phys_LineEnd( start );
		int len = end - start;

		if ( len > (int)sizeof( buf ) - 1 )
			len = (int)sizeof( buf ) - 1;

		memcpy( buf, s_phys.text + start, len );
		buf[len] = '\0';

		// A tab is shown as four spaces.
		std::string shown;

		for ( int i = 0; i < len; i++ )
		{
			if ( buf[i] == '\t' )
				shown += "    ";
			else
				shown += buf[i];
		}

		const float ty = y + 2 + row * lineH;

		DC->drawText( x + 3, ty, scale, color, shown.c_str(), (int)w - 6, style, font );

		if ( s_phys.scroll + row == caretLine && ( DC->realTime / 400 ) % 2 == 0 )
		{
			std::string before;

			for ( int i = start; i < s_phys.caret && i < end; i++ )
			{
				if ( s_phys.text[i] == '\t' )
					before += "    ";
				else
					before += s_phys.text[i];
			}

			// The width includes the spaces at the end of the text.
			const float cx = x + 3 + DC->textWidth( ( before + "|" ).c_str(), scale, font ) - DC->textWidth( "|", scale, font );

			DC->fillRect( cx, ty + 3, 1, (float)DC->textHeight( "Ag", scale, font ) - 1, color );
		}

		start = end + 1;
	}
}

static void Phys_PaintHelp( float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	static const vec4_t back = { 0.04f, 0.04f, 0.04f, 0.9f };
	const int lineH = DC->textHeight( "Ag", scale, font ) + 2;
	const int visible = lineH > 0 ? (int)( ( h - 4 ) / lineH ) : 1;

	Phys_WrapHelp( w - 8, scale, font );

	const int count = (int)s_helpWrapped.size();

	if ( s_phys.helpScroll > count - visible )
		s_phys.helpScroll = count - visible;

	if ( s_phys.helpScroll < 0 )
		s_phys.helpScroll = 0;

	DC->fillRect( x, y, w, h, back );

	for ( int row = 0; row < visible && s_phys.helpScroll + row < count; row++ )
		DC->drawText( x + 3, y + 2 + row * lineH, scale, color, s_helpWrapped[s_phys.helpScroll + row].c_str(), 0, style, font );
}

qboolean UI_Phys_OwnerDraw( int ownerDraw, float x, float y, float w, float h, float scale, vec4_t color, int style, int font )
{
	switch ( ownerDraw )
	{
	case UI_PHYS_TEXT:
		Phys_PaintText( x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_HELP:
		Phys_PaintHelp( x, y, w, h, scale, color, style, font );
		return qtrue;

	case UI_PHYS_STATUS:
	{
		vec4_t c = { 0.8f, 1.0f, 0.8f, 1.0f };

		if ( s_phys.statusError )
		{
			c[0] = 1.0f;
			c[1] = 0.4f;
			c[2] = 0.4f;
		}

		DC->drawText( x, y, scale, c, s_phys.status.c_str(), (int)w, style, font );
		return qtrue;
	}
	}

	return qfalse;
}

//
// Keys of the text area
//

static void Phys_Insert( char c )
{
	if ( s_phys.textLen >= PHYS_TEXT_MAX - 1 )
		return;

	memmove( s_phys.text + s_phys.caret + 1, s_phys.text + s_phys.caret, s_phys.textLen - s_phys.caret + 1 );
	s_phys.text[s_phys.caret] = c;
	s_phys.caret++;
	s_phys.textLen++;
}

static void Phys_Backspace( void )
{
	if ( s_phys.caret <= 0 )
		return;

	memmove( s_phys.text + s_phys.caret - 1, s_phys.text + s_phys.caret, s_phys.textLen - s_phys.caret + 1 );
	s_phys.caret--;
	s_phys.textLen--;
}

static void Phys_MoveVertical( int delta )
{
	const int line = Phys_LineOfCaret();
	const int target = line + delta;

	if ( target < 0 || target >= Phys_LineCount() )
		return;

	const int col = s_phys.caret - Phys_LineStart( line );
	const int start = Phys_LineStart( target );
	const int len = Phys_LineEnd( start ) - start;

	s_phys.caret = start + ( col < len ? col : len );
}

static void Phys_ClickCaret( void )
{
	const int lineH = Phys_LineHeight();

	if ( lineH <= 0 )
		return;

	int line = s_phys.scroll + (int)( ( DC->cursory - s_phys.textRect.y - 2 ) / lineH );

	if ( line < 0 )
		line = 0;

	if ( line >= Phys_LineCount() )
		line = Phys_LineCount() - 1;

	const int start = Phys_LineStart( line );
	const int end = Phys_LineEnd( start );
	const float localX = DC->cursorx - s_phys.textRect.x - 3;
	int col = 0;

	// The text of the line as it is drawn: a tab is four spaces.
	while ( start + col < end )
	{
		std::string shown;

		for ( int i = start; i <= start + col; i++ )
		{
			if ( s_phys.text[i] == '\t' )
				shown += "    ";
			else
				shown += s_phys.text[i];
		}

		const float width = DC->textWidth( ( shown + "|" ).c_str(), s_phys.textScale, s_phys.textFont ) - DC->textWidth( "|", s_phys.textScale, s_phys.textFont );

		// The caret goes to the nearest side of the character.
		std::string before = shown.substr( 0, shown.size() - ( s_phys.text[start + col] == '\t' ? 4 : 1 ) );
		const float widthBefore = DC->textWidth( ( before + "|" ).c_str(), s_phys.textScale, s_phys.textFont ) - DC->textWidth( "|", s_phys.textScale, s_phys.textFont );

		if ( localX < ( width + widthBefore ) * 0.5f )
			break;

		col++;
	}

	s_phys.caret = start + col;
}

qboolean UI_Phys_HandleKey( int ownerDraw, int key )
{
	if ( ownerDraw == UI_PHYS_HELP )
	{
		if ( key == A_MWHEELUP )
			s_phys.helpScroll -= 2;
		else if ( key == A_MWHEELDOWN )
			s_phys.helpScroll += 2;
		else
			return qfalse;

		return qtrue;
	}

	if ( ownerDraw != UI_PHYS_TEXT )
		return qfalse;

	if ( key & K_CHAR_FLAG )
	{
		const int c = key & ~K_CHAR_FLAG;

		if ( c == 'h' - 'a' + 1 )
		{
			Phys_Backspace();
			return qtrue;
		}

		if ( c >= 32 && c < 127 )
		{
			Phys_Insert( (char)c );
			return qtrue;
		}

		return qtrue;
	}

	switch ( key )
	{
	case A_BACKSPACE:	return qtrue;	// the char of the same key is handled above
	case A_ENTER:
	case A_KP_ENTER:	Phys_Insert( '\n' );				return qtrue;
	case A_TAB:			Phys_Insert( '\t' );				return qtrue;
	case A_DELETE:
		if ( s_phys.caret < s_phys.textLen )
		{
			s_phys.caret++;
			Phys_Backspace();
		}
		return qtrue;
	case A_CURSOR_LEFT:	if ( s_phys.caret > 0 ) s_phys.caret--;							return qtrue;
	case A_CURSOR_RIGHT: if ( s_phys.caret < s_phys.textLen ) s_phys.caret++;			return qtrue;
	case A_CURSOR_UP:	Phys_MoveVertical( -1 );			return qtrue;
	case A_CURSOR_DOWN:	Phys_MoveVertical( 1 );				return qtrue;
	case A_HOME:		s_phys.caret = Phys_LineStart( Phys_LineOfCaret() );				return qtrue;
	case A_END:			s_phys.caret = Phys_LineEnd( Phys_LineStart( Phys_LineOfCaret() ) );	return qtrue;
	case A_MWHEELUP:	if ( s_phys.scroll > 0 ) s_phys.scroll -= 2;	return qtrue;
	case A_MWHEELDOWN:	s_phys.scroll += 2;					return qtrue;
	case A_MOUSE1:		Phys_ClickCaret();					return qtrue;
	}

	return qfalse;
}
