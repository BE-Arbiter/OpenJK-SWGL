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
#ifndef RENDERER
	ri.OPrintf("%s", text);
#else
	ri.Printf(PRINT_ALL, "%s", text);
#endif
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
#ifdef RENDERER
// SP's refimport_t has no hunk allocator exposed to the renderer -- route
// through the zone allocator instead, matching code/rd-rend2's own path.
void *Hunk_Alloc( int size, ha_pref preference ) {
	return ri.Z_Malloc( size, TAG_HUNKALLOC, qtrue, 4 );
}

void *Hunk_AllocateTempMemory( int size ) {
	return ri.Z_Malloc( size, TAG_TEMP_HUNKALLOC, qfalse, 4 );
}

void Hunk_FreeTempMemory( void *buf ) {
	ri.Z_Free( buf );
}
#else
void *Hunk_AllocateTempMemory( int size ) {
	return ri.Hunk_AllocateTempMemory( size );
}

void Hunk_FreeTempMemory( void *buf ) {
	ri.Hunk_FreeTempMemory( buf );
}

void *Hunk_Alloc( int size, ha_pref preference ) {
	return ri.Hunk_Alloc( size, preference );
}

int Hunk_MemoryRemaining( void ) {
	return ri.Hunk_MemoryRemaining();
}
#endif

// ZONE
void *Z_Malloc( int iSize, memtag_t eTag, qboolean bZeroit, int iAlign ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit, iAlign );
}

#ifdef RENDERER
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
#else
void Z_Free( void *ptr ) {
	ri.Z_Free( ptr );
}
#endif

int Z_MemSize( memtag_t eTag ) {
	return ri.Z_MemSize( eTag );
}

void Z_MorphMallocTag( void *pvBuffer, memtag_t eDesiredTag ) {
	ri.Z_MorphMallocTag( pvBuffer, eDesiredTag );
}

// CACHED BSP DISK IMAGE
void *CM_GetCachedMapDiskImage( void ) {
#ifdef RENDERER
	return NULL; // SP never populates the disk-image cache for the renderer
#else
	return ri.CM_GetCachedMapDiskImage();
#endif
}

void CM_SetCachedMapDiskImage( void *ptr ) {
#ifndef RENDERER
	ri.CM_SetCachedMapDiskImage( ptr );
#endif
}

void CM_SetUsingCache( qboolean usingCache ) {
#ifdef RENDERER
	*(ri.gbUsingCachedMapDataRightNow()) = usingCache;
#else
	ri.CM_SetUsingCache( usingCache );
#endif
}

#ifndef RENDERER
// SP's refimport_t has no CM_BoxTrace at all -- every RENDERER caller of
// this wrapper has been ported to either ri.SV_Trace (Rag_Trace) or
// R_PointInLeaf-based PVS lookups (R_inPVS) instead, and the roof-culling
// automap feature (tr_world.cpp) that used it is MP-only.
void CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, int capsule ) {
	ri.CM_BoxTrace( results, start, end, mins, maxs, model, brushmask, capsule );
}
#endif

qboolean Com_TheHunkMarkHasBeenMade( void ) {
#ifdef RENDERER
	return qtrue; // unreachable on SP -- RE_RegisterServerSkin has no export entry
#else
	return ri.Com_TheHunkMarkHasBeenMade();
#endif
}

#ifndef RENDERER
int FS_FileIsInPAK( const char *filename, int *pChecksum ) {
	return ri.FS_FileIsInPAK( filename, pChecksum );
}
#endif

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags, const char *var_desc ) {
#ifdef RENDERER
	return ri.Cvar_Get( var_name, value, flags );
#else
	return ri.Cvar_Get( var_name, value, flags, var_desc );
#endif
}

int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) {
#ifdef RENDERER
	return ri.CIN_PlayCinematic( arg0, xpos, ypos, width, height, bits, NULL );
#else
	return ri.CIN_PlayCinematic( arg0, xpos, ypos, width, height, bits );
#endif
}
