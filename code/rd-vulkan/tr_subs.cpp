/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// tr_subs.cpp - common function replacements for modular renderer
#include "tr_local.h"

void QDECL Com_Printf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_OPrintf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);
	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_DPrintf( const char *format, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, format);
	Q_vsnprintf(text, sizeof(text), format, argptr);
	va_end(argptr);

	ri.Printf(PRINT_DEVELOPER, "%s", text);
}

void QDECL Com_Error( int level, const char *error, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	ri.Error(level, "%s", text);
}

// HUNK

// SP's refimport_t has no hunk allocator exposed to the renderer -- route
// through the zone allocator instead, matching code/rd-rend2's own path.
void *Hunk_Alloc( int size, ha_pref preference ) {
	// Hunk_Clear() (code/qcommon/common.cpp) runs on every map load and
	// bulk-frees everything tagged TAG_HUNKALLOC -- that's correct for
	// h_high (temporary, per-level data) but h_low is meant to survive
	// map transitions (e.g. tr_shader.cpp's persistent s_shaderText /
	// shaderTextHashTable, built once at renderer init), so it needs a
	// tag Hunk_Clear() never touches.
	memtag_t tag = ( preference == h_low ) ? TAG_GENERAL : TAG_HUNKALLOC;
	return ri.Z_Malloc( size, tag, qtrue, 4 );
}

void *Hunk_AllocateTempMemory( int size ) {
	return ri.Z_Malloc( size, TAG_TEMP_HUNKALLOC, qfalse, 4 );
}

void Hunk_FreeTempMemory( void *buf ) {
	ri.Z_Free( buf );
}

// ZONE
void *Z_Malloc( int iSize, memtag_t eTag, qboolean bZeroit, int iAlign ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit, iAlign );
}

// SP's qcommon.h declares a bare "int Z_Free(void*)" -- match its signature.
int Z_Free( void *ptr ) {
	return ri.Z_Free( ptr );
}

// SP's tr_image_jpg.cpp/tr_image_tga.cpp/tr_image_png.cpp (rd-common) call
// these directly -- rend2/vanilla both define them, matching tr_common.h.
void *R_Malloc( int iSize, memtag_t eTag, qboolean bZeroit ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit, 4 );
}

void R_Free( void *ptr ) {
	ri.Z_Free( ptr );
}

int Z_MemSize( memtag_t eTag ) {
	return ri.Z_MemSize( eTag );
}

void Z_MorphMallocTag( void *pvBuffer, memtag_t eDesiredTag ) {
	ri.Z_MorphMallocTag( pvBuffer, eDesiredTag );
}

// CACHED BSP DISK IMAGE
void *CM_GetCachedMapDiskImage( void ) {
	//TODO Check if this is correct
	return NULL; // SP never populates the disk-image cache for the renderer
}

void CM_SetCachedMapDiskImage( void *ptr ) {

}

void CM_SetUsingCache( qboolean usingCache ) {
	*(ri.gbUsingCachedMapDataRightNow()) = usingCache;
}

qboolean Com_TheHunkMarkHasBeenMade( void ) {
	//TODO Check if this is correct
	return qtrue; // unreachable on SP -- RE_RegisterServerSkin has no export entry

}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags, const char *var_desc ) {
	return ri.Cvar_Get( var_name, value, flags );
}

int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) {
	return ri.CIN_PlayCinematic( arg0, xpos, ypos, width, height, bits, NULL );
}
