/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Emission factor of each emissive shader. Include after tr_local.h.
//
// The factor scales the light that the polygons of a shader cast. It does not change how bright
// the surface looks: the tracer bakes that value in the primitive buffer at the map load.

#pragma once

#include <string>
#include <vector>
#include <utility>

typedef std::vector< std::pair<std::string, float> >	rtxEmissiveScales_t;

extern float	g_rtxEmissiveScale[MAX_SHADERS];	// by shader index, 1 when unchanged

// Factor of the light that a material casts. O(1), called by copy_light for each light.
static inline float RTX_LightEmissive_Scale( const rtx_material_t *material )
{
	return material->index < MAX_SHADERS ? g_rtxEmissiveScale[material->index] : 1.0f;
}

// Map load. Reset clears the table and the scales. Build reads the first numEmissive entries of
// world->light_polys.
void		RTX_LightEmissive_Reset( void );
void		RTX_LightEmissive_Build( world_t &w, int numEmissive );

// Implementation of the API functions.
int			RTX_LightEmissive_Count( void );
qboolean	RTX_LightEmissive_Get( int shader, char *name, int nameSize, float *scale, int *numPolys );
int			RTX_LightEmissive_ShaderOf( int emissiveIndex );
qboolean	RTX_LightEmissive_Set( int shader, float scale );

// File layer. SetFromFile sets the scale of a shader by name. A name that is not in the map is
// kept for the next Save. ResetScales gives every shader the factor 1 and drops the kept names.
void		RTX_LightEmissive_SetFromFile( const char *name, float scale );
void		RTX_LightEmissive_ResetScales( void );

// Names and scales to write: the shaders of the map with a factor other than 1, then the kept names.
void		RTX_LightEmissive_GetForSave( rtxEmissiveScales_t &out );

// Number of shaders of the map that have a factor other than 1.
int			RTX_LightEmissive_NumChanged( void );

// Console commands.
void		RTX_LightEdit_EmissiveShaders_f( void );
void		RTX_LightEdit_EmissiveScale_f( void );
