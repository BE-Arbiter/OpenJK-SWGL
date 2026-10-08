/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Emission factor of each emissive shader of the world. See rtx_light_emissive.h.
//
// The table lists the shaders of the world emissive lights. A shader name can have several
// shader indices (one per lightmap variant): all of them get the factor of the name.

#include "../tr_local.h"
#include "rtx_light_edit.h"
#include "rtx_light_emissive.h"

#include <stdlib.h>

#define EMISSIVE_MAX_SCALE	100.0f

struct emissiveShader_t {
	std::string	name;
	int			numPolys;
	float		scale;
};

float g_rtxEmissiveScale[MAX_SHADERS];

static float Com_Clamp( float lo, float hi, float v )
{
	return v < lo ? lo : v > hi ? hi : v;
}

static std::vector<emissiveShader_t>	g_shaders;
static std::vector<int>					g_polyShader;	// emissive index -> shader of the table, or -1
static rtxEmissiveScales_t				g_kept;			// file entries for shaders that are not in the map

static void FillOnes( void )
{
	for ( int i = 0; i < MAX_SHADERS; i++ )
		g_rtxEmissiveScale[i] = 1.0f;
}

static struct emissiveInit_t {
	emissiveInit_t() { FillOnes(); }
} g_emissiveInit;

static int FindByName( const char *name )
{
	for ( size_t i = 0; i < g_shaders.size(); i++ )
	{
		if ( !Q_stricmp( g_shaders[i].name.c_str(), name ) )
			return (int)i;
	}

	return -1;
}

// Gives the factor of an entry to every shader index of that name.
static void ApplyScale( const emissiveShader_t &e )
{
	for ( int s = 0; s < tr.numShaders && s < MAX_SHADERS; s++ )
	{
		if ( tr.shaders[s] && !Q_stricmp( tr.shaders[s]->name, e.name.c_str() ) )
			g_rtxEmissiveScale[s] = e.scale;
	}
}

void RTX_LightEmissive_Reset( void )
{
	FillOnes();
	g_shaders.clear();
	g_polyShader.clear();
	g_kept.clear();
}

void RTX_LightEmissive_Build( world_t &w, int numEmissive )
{
	g_shaders.clear();
	g_polyShader.assign( MAX( numEmissive, 0 ), -1 );

	for ( int i = 0; i < numEmissive && i < w.num_light_polys; i++ )
	{
		const rtx_material_t *mat = w.light_polys[i].material;

		if ( !mat || (int)mat->index >= tr.numShaders || !tr.shaders[mat->index] )
			continue;

		int idx = FindByName( tr.shaders[mat->index]->name );

		if ( idx < 0 )
		{
			emissiveShader_t e;

			e.name = tr.shaders[mat->index]->name;
			e.numPolys = 0;
			e.scale = 1.0f;
			g_shaders.push_back( e );
			idx = (int)g_shaders.size() - 1;
		}

		g_shaders[idx].numPolys++;
		g_polyShader[i] = idx;
	}
}

int RTX_LightEmissive_Count( void )
{
	return RTX_LightEdit_IsReady() ? (int)g_shaders.size() : 0;
}

qboolean RTX_LightEmissive_Get( int shader, char *name, int nameSize, float *scale, int *numPolys )
{
	if ( !RTX_LightEdit_IsReady() || shader < 0 || shader >= (int)g_shaders.size() )
		return qfalse;

	if ( name && nameSize > 0 )
		Q_strncpyz( name, g_shaders[shader].name.c_str(), nameSize );

	if ( scale )
		*scale = g_shaders[shader].scale;

	if ( numPolys )
		*numPolys = g_shaders[shader].numPolys;

	return qtrue;
}

int RTX_LightEmissive_ShaderOf( int emissiveIndex )
{
	if ( !RTX_LightEdit_IsReady() || emissiveIndex < 0 || emissiveIndex >= (int)g_polyShader.size() )
		return -1;

	return g_polyShader[emissiveIndex];
}

qboolean RTX_LightEmissive_Set( int shader, float scale )
{
	if ( !RTX_LightEdit_IsReady() || shader < 0 || shader >= (int)g_shaders.size() )
		return qfalse;

	scale = Com_Clamp( 0.0f, EMISSIVE_MAX_SCALE, scale );

	if ( g_shaders[shader].scale == scale )
		return qtrue;

	g_shaders[shader].scale = scale;
	ApplyScale( g_shaders[shader] );

	RTX_LightEdit_CountChange( 1 );
	RTX_LightEdit_BumpGeneration();

	return qtrue;
}

void RTX_LightEmissive_SetFromFile( const char *name, float scale )
{
	const int idx = FindByName( name );

	scale = Com_Clamp( 0.0f, EMISSIVE_MAX_SCALE, scale );

	if ( idx >= 0 )
	{
		g_shaders[idx].scale = scale;
		ApplyScale( g_shaders[idx] );
		return;
	}

	for ( size_t i = 0; i < g_kept.size(); i++ )
	{
		if ( !Q_stricmp( g_kept[i].first.c_str(), name ) )
		{
			g_kept[i].second = scale;
			return;
		}
	}

	Com_Printf( "light edit: emissive shader %s is not in this map, kept in the file\n", name );
	g_kept.push_back( std::make_pair( std::string( name ), scale ) );
}

void RTX_LightEmissive_ResetScales( void )
{
	FillOnes();

	for ( size_t i = 0; i < g_shaders.size(); i++ )
		g_shaders[i].scale = 1.0f;

	g_kept.clear();
}

void RTX_LightEmissive_GetForSave( rtxEmissiveScales_t &out )
{
	out.clear();

	for ( size_t i = 0; i < g_shaders.size(); i++ )
	{
		if ( g_shaders[i].scale != 1.0f )
			out.push_back( std::make_pair( g_shaders[i].name, g_shaders[i].scale ) );
	}

	for ( size_t i = 0; i < g_kept.size(); i++ )
		out.push_back( g_kept[i] );
}

int RTX_LightEmissive_NumChanged( void )
{
	int n = 0;

	for ( size_t i = 0; i < g_shaders.size(); i++ )
		n += g_shaders[i].scale != 1.0f ? 1 : 0;

	return n;
}

/*
=================
Console commands
=================
*/

// pt_ledit_emissive_shaders: lists the emissive shaders of the world.
void RTX_LightEdit_EmissiveShaders_f( void )
{
	const int count = RTX_LightEmissive_Count();

	for ( int i = 0; i < count; i++ )
	{
		char	name[MAX_QPATH];
		float	scale;
		int		polys;

		if ( RTX_LightEmissive_Get( i, name, sizeof(name), &scale, &polys ) )
			Com_Printf( "%4i: %-48s %5i polygons  scale %g\n", i, name, polys, scale );
	}

	Com_Printf( "%i emissive shaders\n", count );
}

// pt_ledit_emissive_scale <index|name> <scale>: sets the emission factor of a shader.
void RTX_LightEdit_EmissiveScale_f( void )
{
	if ( ri.Cmd_Argc() != 3 )
	{
		Com_Printf( "usage: pt_ledit_emissive_scale <index|name> <scale>\n" );
		return;
	}

	const char	*arg = ri.Cmd_Argv( 1 );
	int			shader = -1;

	if ( arg[0] >= '0' && arg[0] <= '9' )
		shader = atoi( arg );
	else if ( RTX_LightEdit_IsReady() )
		shader = FindByName( arg );

	if ( !RTX_LightEmissive_Set( shader, (float)atof( ri.Cmd_Argv( 2 ) ) ) )
		Com_Printf( "no emissive shader %s\n", arg );
}
