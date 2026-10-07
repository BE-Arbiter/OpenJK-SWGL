/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light edit core of the RTX renderer.
//
// A record is one editable light. Its slot in world->light_polys is fixed for the session:
// the shaders use the slot as the ReSTIR identity and as the stride of the light stats
// buffer. A removed light becomes a tombstone (cluster -1, colour 0). Spare tombstones at the
// end of the array wait for added lights. The array grows by blocks, with a GPU wait.

#include "../tr_local.h"
#include "conversion.h"
#include "rtx_light_edit.h"

#include <vector>
#include <stdarg.h>
#include <math.h>

#define LEDIT_EPSILON			0.001f
#define LEDIT_MIN_SPARE			16
#define LEDIT_GROW_BLOCK		64
#define LEDIT_DYNAMIC_RESERVE	128		// slots kept for sabers, dlights and brush model lights
#define LEDIT_MAX_SLOTS			( MAX_LIGHT_POLYS - LEDIT_DYNAMIC_RESERVE )
#define LEDIT_DRAG_REBUILD_MS	100
#define LEDIT_ENTITY_RADIUS		16.0f	// as ENTITY_LIGHT_RADIUS in vk_rtx_bsp.cpp
#define LEDIT_LGT_RADIUS		4.0f	// as LIGHTGEN_EMITTER_RADIUS in vk_rtx_lightgen.cpp
#define LEDIT_ADD_RADIUS		8.0f

static std::vector<rtxLightRecord_t>	g_records;
static world_t		*g_world = NULL;
static qboolean		g_valid = qfalse;
static int			g_firstFree = 0;		// first slot no record uses
static int			g_numEmissive = 0;
static int			g_overfull = 0;
static int			g_rebuilds = 0;
static int			g_unsaved = 0;
static int			g_dragId = -1;
static qboolean		g_pendingRebuild = qfalse;
static int			g_lastRebuildMs = 0;
static char			g_lastError[128] = "";

void RTX_LightEdit_SetError( const char *fmt, ... )
{
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( g_lastError, sizeof(g_lastError), fmt, argptr );
	va_end( argptr );
}

void RTX_LightEdit_CountChange( int delta )
{
	if ( delta == 0 )
		g_unsaved = 0;
	else
		g_unsaved += delta;
}

qboolean RTX_LightEdit_IsReady( void )
{
	return (qboolean)( vk.rtxActive && tr.world && g_valid && g_world == tr.world && g_world->light_polys );
}

int RTX_LightEdit_NumRecords( void )
{
	return RTX_LightEdit_IsReady() ? (int)g_records.size() : 0;
}

rtxLightRecord_t *RTX_LightEdit_GetRecord( int id )
{
	if ( !RTX_LightEdit_IsReady() || id < 0 || id >= (int)g_records.size() )
	{
		RTX_LightEdit_SetError( "bad light id %i", id );
		return NULL;
	}

	return &g_records[id];
}

static int PointCluster( const vec3_t p )
{
	vec3_t v;

	VectorCopy( p, v );

	return BSP_PointLeaf( g_world->nodes, v )->cluster;
}

static float ClampF( float lo, float v, float hi )
{
	return v < lo ? lo : v > hi ? hi : v;
}

static qboolean IsInactive( const rtxLightRecord_t *rec )
{
	return (qboolean)( ( rec->flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED ) ) != 0 );
}

/*
=================
Registration at map load
=================
*/

void RTX_LightEdit_Reset( world_t &w )
{
	g_records.clear();
	g_world = &w;
	g_valid = qfalse;
	g_firstFree = 0;
	g_numEmissive = 0;
	g_overfull = 0;
	g_rebuilds = 0;
	g_unsaved = 0;
	g_dragId = -1;
	g_pendingRebuild = qfalse;
	g_lastRebuildMs = 0;
	g_lastError[0] = 0;
}

void RTX_LightEdit_Invalidate( void )
{
	g_valid = qfalse;
	g_world = NULL;
	g_dragId = -1;
	g_pendingRebuild = qfalse;
}

int RTX_LightEdit_NewRecord( int source, int sourceKey )
{
	rtxLightRecord_t rec;

	Com_Memset( &rec, 0, sizeof(rec) );

	rec.source = source;
	rec.sourceKey = source == RTX_LSRC_ADDED ? -1 : sourceKey;
	rec.type = RTX_LTYPE_SPHERE;
	rec.lightIndex = -1;
	VectorSet( rec.color, 1.0f, 1.0f, 1.0f );
	VectorSet( rec.origColor, 1.0f, 1.0f, 1.0f );
	rec.radius = rec.origRadius = source == RTX_LSRC_ENTITY ? LEDIT_ENTITY_RADIUS
		: source == RTX_LSRC_LGT ? LEDIT_LGT_RADIUS : LEDIT_ADD_RADIUS;
	rec.entClass = source == RTX_LSRC_ADDED ? LIGHT_ENT_EDIT : LIGHT_ENT_AMBIENT;

	g_records.push_back( rec );

	return (int)g_records.size() - 1;
}

int RTX_LightEdit_RegisterLoaded( int source, int sourceKey, const vec3_t origin, const vec3_t color,
	float intensity, int lightIndex, float rays, float error )
{
	const int id = RTX_LightEdit_NewRecord( source, sourceKey );
	rtxLightRecord_t *rec = &g_records[id];

	VectorCopy( origin, rec->origin );
	VectorCopy( origin, rec->origOrigin );
	VectorCopy( color, rec->color );
	VectorCopy( color, rec->origColor );
	rec->intensity = rec->origIntensity = intensity;
	rec->lightIndex = lightIndex;
	rec->rays = rays;
	rec->error = error;

	if ( lightIndex < 0 )
		rec->flags |= RTX_LFLAG_IN_SOLID;

	Com_sprintf( rec->name, sizeof(rec->name), "%s_%i", source == RTX_LSRC_ENTITY ? "ent" : "lgt", sourceKey );
	Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );

	return id;
}

// Reads what the load code made of each light: spot data, emitter radius, class.
void RTX_LightEdit_FinalizeLoad( world_t &w )
{
	int	slotted = 0;

	g_world = &w;

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		rtxLightRecord_t *rec = &g_records[i];

		if ( rec->lightIndex < 0 || rec->lightIndex >= w.num_light_polys )
			continue;

		const light_poly_t *light = w.light_polys + rec->lightIndex;

		slotted++;

		rec->type = light->type == LIGHT_SPOT ? RTX_LTYPE_SPOT : RTX_LTYPE_SPHERE;
		rec->radius = rec->origRadius = light->positions[3];
		rec->entClass = light->ent_class;

		if ( light->type == LIGHT_SPOT )
		{
			for ( int k = 0; k < 5; k++ )
				rec->spot[k] = light->positions[4 + k];
		}
	}

	g_numEmissive = w.num_light_polys - slotted;
	g_firstFree = w.num_light_polys;
	g_valid = qtrue;
}

/*
=================
Conversion between a record and its light_poly
=================
*/

int RTX_LightEdit_Convert( const rtxLightRecord_t *rec, light_poly_t *out )
{
	const float	r = MAX( rec->radius, 0.5f );

	Com_Memset( out, 0, sizeof(*out) );

	VectorCopy( rec->origin, out->positions + 0 );
	VectorCopy( rec->origin, out->off_center );
	out->positions[3] = rec->radius;

	if ( rec->type == RTX_LTYPE_SPOT )
	{
		for ( int k = 0; k < 5; k++ )
			out->positions[4 + k] = rec->spot[k];

		// A spot's irradiance does not depend on the emitter radius.
		VectorScale( rec->color, rec->intensity * 0.5f, out->color );
		out->type = LIGHT_SPOT;
	}
	else
	{
		// A sphere's irradiance is colour * r^2 / d^2, so the colour holds 1 / r^2.
		const float scale = rec->source == RTX_LSRC_LGT ? pt_lightgen_scale->value : 1.0f;

		VectorScale( rec->color, scale * rec->intensity / ( r * r ), out->color );
		out->type = LIGHT_SPHERE;
	}

	out->emissive_factor = 1.0f;
	out->material = NULL;
	out->style = 0;
	out->ent_class = rec->entClass;
	out->cluster = PointCluster( rec->origin );

	return out->cluster;
}

void RTX_LightEdit_Tombstone( int slot, const vec3_t origin )
{
	light_poly_t *light = g_world->light_polys + slot;

	Com_Memset( light, 0, sizeof(*light) );

	VectorCopy( origin, light->positions + 0 );
	VectorCopy( origin, light->off_center );
	light->positions[3] = 1.0f;
	light->cluster = -1;
	light->type = LIGHT_SPHERE;
	light->material = NULL;
	light->ent_class = LIGHT_ENT_AMBIENT;
}

/*
=================
Slots and growth
=================
*/

static void RecreateLightStats( world_t &w )
{
	uint32_t num_stats = w.numClusters * w.num_light_polys * 6 * 2;

	if ( num_stats == 0 )
		num_stats = 1;

	for ( int frame = 0; frame < NUM_LIGHT_STATS_BUFFERS; frame++ )
	{
		vk_rtx_buffer_destroy( vk.buf_light_stats + frame );

		if ( vk_rtx_buffer_create( vk.buf_light_stats + frame, sizeof(uint32_t) * num_stats,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) != VK_SUCCESS )
		{
			Com_Error( ERR_DROP, "light edit: cannot create the light stats buffer" );
		}
	}

	// The shaders accumulate into the buffers, so they start at zero.
	VkCommandBuffer cmd_buf = vkpt_begin_command_buffer( &vk.cmd_buffers_graphics );

	for ( int frame = 0; frame < NUM_LIGHT_STATS_BUFFERS; frame++ )
		qvkCmdFillBuffer( cmd_buf, vk.buf_light_stats[frame].buffer, 0, vk.buf_light_stats[frame].size, 0 );

	vkpt_submit_command_buffer( cmd_buf, vk.queue_graphics, (1 << vk.device_count) - 1, 0, NULL, NULL, NULL, 0, NULL, NULL, NULL );
	qvkDeviceWaitIdle( vk.device );

	// Patch the light stats binding of each set in place.
	for ( uint32_t i = 0; i < vk.swapchain_image_count; i++ )
	{
		vkdescriptor_t *desc = &vk.desc_set_vertex_buffer[i];

		if ( desc->set == VK_NULL_HANDLE )
			continue;

		for ( size_t k = 0; k < desc->size; k++ )
		{
			if ( desc->bindings[k].binding != BINDING_OFFSET_LIGHT_STATS_BUFFER )
				continue;

			for ( int j = 0; j < NUM_LIGHT_STATS_BUFFERS; j++ )
			{
				desc->data[k].buffer[j].buffer = vk.buf_light_stats[j].buffer;
				desc->data[k].buffer[j].offset = 0;
				desc->data[k].buffer[j].range = vk.buf_light_stats[j].size;
			}

			VkWriteDescriptorSet write;

			Com_Memset( &write, 0, sizeof(write) );
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = desc->set;
			write.dstBinding = BINDING_OFFSET_LIGHT_STATS_BUFFER;
			write.dstArrayElement = 0;
			write.descriptorCount = NUM_LIGHT_STATS_BUFFERS;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write.pBufferInfo = desc->data[k].buffer;

			qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
			break;
		}
	}
}

void RTX_LightEdit_RebuildClusters( void )
{
	g_overfull = vk_rtx_rebuild_cluster_lights( *g_world, qtrue );
	g_rebuilds++;
	g_pendingRebuild = qfalse;
	g_lastRebuildMs = ri.Milliseconds();
}

qboolean RTX_LightEdit_GrowSlots( int count )
{
	world_t	&w = *g_world;
	int		newNum = w.num_light_polys + ( ( count + LEDIT_GROW_BLOCK - 1 ) / LEDIT_GROW_BLOCK ) * LEDIT_GROW_BLOCK;

	if ( newNum > LEDIT_MAX_SLOTS )
		newNum = LEDIT_MAX_SLOTS;

	if ( newNum <= w.num_light_polys )
	{
		RTX_LightEdit_SetError( "no free light slot (limit %i)", LEDIT_MAX_SLOTS );
		return qfalse;
	}

	qvkDeviceWaitIdle( vk.device );

	light_poly_t *polys = (light_poly_t *)realloc( w.light_polys, newNum * sizeof(light_poly_t) );

	if ( !polys )
	{
		RTX_LightEdit_SetError( "out of memory" );
		return qfalse;
	}

	w.light_polys = polys;
	w.allocated_light_polys = newNum;

	const int oldNum = w.num_light_polys;
	const vec3_t zero = { 0.0f, 0.0f, 0.0f };

	w.num_light_polys = newNum;

	for ( int i = oldNum; i < newNum; i++ )
		RTX_LightEdit_Tombstone( i, zero );

	RecreateLightStats( w );
	RTX_LightEdit_RebuildClusters();

	return qtrue;
}

qboolean RTX_LightEdit_EnsureSlot( rtxLightRecord_t *rec )
{
	if ( rec->lightIndex >= 0 )
		return qtrue;

	if ( g_firstFree >= g_world->num_light_polys && !RTX_LightEdit_GrowSlots( 1 ) )
		return qfalse;

	rec->lightIndex = g_firstFree++;

	return qtrue;
}

qboolean RTX_LightEdit_ApplyRecord( rtxLightRecord_t *rec )
{
	light_poly_t	tmp;
	const int		cluster = RTX_LightEdit_Convert( rec, &tmp );

	if ( cluster < 0 )
		rec->flags |= RTX_LFLAG_IN_SOLID;
	else
		rec->flags &= ~RTX_LFLAG_IN_SOLID;

	const qboolean emits = (qboolean)( !IsInactive( rec ) && cluster >= 0 );

	if ( rec->lightIndex < 0 )
	{
		if ( !emits )
			return qfalse;

		if ( !RTX_LightEdit_EnsureSlot( rec ) )
			return qfalse;
	}

	light_poly_t	*light = g_world->light_polys + rec->lightIndex;
	const int		oldCluster = light->cluster;

	if ( emits )
		*light = tmp;
	else
		RTX_LightEdit_Tombstone( rec->lightIndex, rec->origin );

	return (qboolean)( light->cluster != oldCluster );
}

// True when a cluster of the PVS of `cluster` has no room for one more light.
static qboolean ClusterFull( int cluster )
{
	const world_t	&w = *g_world;

	if ( cluster < 0 || !w.cluster_light_offsets )
		return qfalse;

	const byte *pvs = BSP_GetPvs( g_world, cluster );

	for ( int c = 0; c < w.numClusters; c++ )
	{
		if ( !( pvs[c >> 3] & ( 1 << ( c & 7 ) ) ) )
			continue;

		if ( w.cluster_light_offsets[c + 1] - w.cluster_light_offsets[c] >= RTX_MAX_LIGHTS_PER_CLUSTER )
			return qtrue;
	}

	return qfalse;
}

// A cluster change rebuilds the lists. During a drag the rebuild runs at most each 100 ms.
static void ClusterChanged( int id )
{
	if ( id == g_dragId )
	{
		g_pendingRebuild = qtrue;

		if ( ri.Milliseconds() - g_lastRebuildMs < LEDIT_DRAG_REBUILD_MS )
			return;
	}

	RTX_LightEdit_RebuildClusters();
}

/*
=================
API
=================
*/

static qboolean IsModified( const rtxLightRecord_t *rec )
{
	if ( rec->source == RTX_LSRC_ADDED )
		return qfalse;

	if ( fabsf( rec->intensity - rec->origIntensity ) > LEDIT_EPSILON
		|| fabsf( rec->radius - rec->origRadius ) > LEDIT_EPSILON )
		return qtrue;

	for ( int k = 0; k < 3; k++ )
	{
		if ( fabsf( rec->origin[k] - rec->origOrigin[k] ) > LEDIT_EPSILON
			|| fabsf( rec->color[k] - rec->origColor[k] ) > LEDIT_EPSILON )
			return qtrue;
	}

	return qfalse;
}

static void FillDesc( int id, const rtxLightRecord_t *rec, qboolean original, rtxLightDesc_t *out )
{
	Com_Memset( out, 0, sizeof(*out) );

	out->id = id;
	out->source = rec->source;
	out->sourceKey = rec->sourceKey;
	out->type = rec->type;
	out->flags = rec->flags & ( RTX_LFLAG_DISABLED | RTX_LFLAG_DELETED | RTX_LFLAG_IN_SOLID );

	if ( IsModified( rec ) )
		out->flags |= RTX_LFLAG_MODIFIED;

	if ( id == g_dragId )
		out->flags |= RTX_LFLAG_DRAGGING;

	VectorCopy( original ? rec->origOrigin : rec->origin, out->origin );
	VectorCopy( original ? rec->origColor : rec->color, out->color );
	out->intensity = original ? rec->origIntensity : rec->intensity;
	out->radius = original ? rec->origRadius : rec->radius;
	Q_strncpyz( out->name, original ? rec->origName : rec->name, sizeof(out->name) );

	if ( rec->type == RTX_LTYPE_SPOT )
	{
		uint32_t packed;

		Com_Memcpy( &packed, &rec->spot[1], sizeof(packed) );
		VectorCopy( rec->spot + 2, out->dir );

		const float cosOuter = halfToFloat( (uint16_t)( packed & 0xFFFF ) );
		const float cosInner = halfToFloat( (uint16_t)( packed >> 16 ) );

		out->coneOuter = acosf( ClampF( -1.0f, cosOuter, 1.0f ) ) * 57.29578f;
		out->coneInner = acosf( ClampF( -1.0f, cosInner, 1.0f ) ) * 57.29578f;
	}
}

static qboolean LE_IsAvailable( void )
{
	return RTX_LightEdit_IsReady();
}

static qboolean LE_Begin( void )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return qfalse;
	}

	const int spare = g_world->num_light_polys - g_firstFree;

	if ( spare < LEDIT_MIN_SPARE )
		RTX_LightEdit_GrowSlots( LEDIT_MIN_SPARE - spare );

	return qtrue;
}

static void LE_EndDrag( int id );

static void LE_End( void )
{
	if ( g_dragId >= 0 )
		LE_EndDrag( g_dragId );
}

static int LE_Count( void )
{
	return RTX_LightEdit_NumRecords();
}

static qboolean LE_Get( int id, rtxLightDesc_t *out )
{
	const rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec || !out )
		return qfalse;

	FillDesc( id, rec, qfalse, out );

	return qtrue;
}

static qboolean LE_GetOriginal( int id, rtxLightDesc_t *out )
{
	const rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec || !out )
		return qfalse;

	FillDesc( id, rec, qtrue, out );

	return qtrue;
}

// Copies the editable values of a descriptor to a record, with limits.
static void CopyValues( rtxLightRecord_t *rec, const rtxLightDesc_t *desc )
{
	VectorCopy( desc->origin, rec->origin );

	for ( int k = 0; k < 3; k++ )
		rec->color[k] = ClampF( 0.0f, desc->color[k], 1.0f );

	rec->intensity = MAX( desc->intensity, 0.0f );
	rec->radius = desc->radius > 0.0f ? MAX( desc->radius, 0.5f ) : rec->radius;
	Q_strncpyz( rec->name, desc->name, sizeof(rec->name) );
}

static int LE_Add( const rtxLightDesc_t *desc )
{
	if ( !RTX_LightEdit_IsReady() )
	{
		RTX_LightEdit_SetError( "RTX light edit is not available" );
		return -1;
	}

	if ( !desc )
	{
		RTX_LightEdit_SetError( "no light description" );
		return -1;
	}

	const int cluster = PointCluster( desc->origin );

	if ( cluster < 0 )
	{
		RTX_LightEdit_SetError( "point in solid" );
		return -1;
	}

	if ( ClusterFull( cluster ) )
	{
		RTX_LightEdit_SetError( "cluster full" );
		return -1;
	}

	const int id = RTX_LightEdit_NewRecord( RTX_LSRC_ADDED, -1 );
	rtxLightRecord_t *rec = &g_records[id];

	CopyValues( rec, desc );

	VectorCopy( rec->origin, rec->origOrigin );
	VectorCopy( rec->color, rec->origColor );
	rec->origIntensity = rec->intensity;
	rec->origRadius = rec->radius;
	Q_strncpyz( rec->origName, rec->name, sizeof(rec->origName) );

	if ( !RTX_LightEdit_EnsureSlot( rec ) )
	{
		g_records.pop_back();
		return -1;
	}

	RTX_LightEdit_ApplyRecord( rec );
	RTX_LightEdit_RebuildClusters();
	RTX_LightEdit_CountChange( 1 );

	return id;
}

static qboolean LE_Set( int id, const rtxLightDesc_t *desc )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( !desc )
	{
		RTX_LightEdit_SetError( "no light description" );
		return qfalse;
	}

	if ( rec->flags & RTX_LFLAG_DELETED )
	{
		RTX_LightEdit_SetError( "light %i is deleted", id );
		return qfalse;
	}

	const int newCluster = PointCluster( desc->origin );

	if ( !IsInactive( rec ) && newCluster >= 0 )
	{
		const int oldCluster = rec->lightIndex >= 0 ? g_world->light_polys[rec->lightIndex].cluster : -1;

		if ( newCluster != oldCluster && ClusterFull( newCluster ) )
		{
			RTX_LightEdit_SetError( "cluster full" );
			return qfalse;
		}

		if ( !RTX_LightEdit_EnsureSlot( rec ) )
			return qfalse;
	}

	CopyValues( rec, desc );

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		ClusterChanged( id );

	RTX_LightEdit_CountChange( 1 );

	return qtrue;
}

static qboolean LE_Remove( int id )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	const int flag = rec->source == RTX_LSRC_ADDED ? RTX_LFLAG_DELETED : RTX_LFLAG_DISABLED;

	if ( rec->flags & flag )
	{
		RTX_LightEdit_SetError( "light %i is already removed", id );
		return qfalse;
	}

	rec->flags |= flag;

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RTX_LightEdit_RebuildClusters();

	RTX_LightEdit_CountChange( 1 );

	return qtrue;
}

static qboolean LE_Restore( int id )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( !IsInactive( rec ) )
	{
		RTX_LightEdit_SetError( "light %i is not removed", id );
		return qfalse;
	}

	const int cluster = PointCluster( rec->origin );

	if ( cluster >= 0 && ClusterFull( cluster ) )
	{
		RTX_LightEdit_SetError( "cluster full" );
		return qfalse;
	}

	rec->flags &= ~( RTX_LFLAG_DELETED | RTX_LFLAG_DISABLED );

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RTX_LightEdit_RebuildClusters();

	RTX_LightEdit_CountChange( 1 );

	return qtrue;
}

static qboolean LE_Revert( int id )
{
	rtxLightRecord_t *rec = RTX_LightEdit_GetRecord( id );

	if ( !rec )
		return qfalse;

	if ( rec->flags & RTX_LFLAG_DELETED )
	{
		RTX_LightEdit_SetError( "light %i is deleted", id );
		return qfalse;
	}

	const int cluster = PointCluster( rec->origOrigin );

	if ( !IsInactive( rec ) && cluster >= 0 )
	{
		const int oldCluster = rec->lightIndex >= 0 ? g_world->light_polys[rec->lightIndex].cluster : -1;

		if ( cluster != oldCluster && ClusterFull( cluster ) )
		{
			RTX_LightEdit_SetError( "cluster full" );
			return qfalse;
		}

		if ( !RTX_LightEdit_EnsureSlot( rec ) )
			return qfalse;
	}

	VectorCopy( rec->origOrigin, rec->origin );
	VectorCopy( rec->origColor, rec->color );
	rec->intensity = rec->origIntensity;
	rec->radius = rec->origRadius;
	Q_strncpyz( rec->name, rec->origName, sizeof(rec->name) );

	if ( RTX_LightEdit_ApplyRecord( rec ) )
		RTX_LightEdit_RebuildClusters();

	RTX_LightEdit_CountChange( 1 );

	return qtrue;
}

static void LE_BeginDrag( int id )
{
	if ( !RTX_LightEdit_GetRecord( id ) )
		return;

	g_dragId = id;
	g_pendingRebuild = qfalse;
}

static void LE_EndDrag( int id )
{
	if ( g_dragId != id )
		return;

	g_dragId = -1;

	if ( g_pendingRebuild && RTX_LightEdit_IsReady() )
		RTX_LightEdit_RebuildClusters();

	g_pendingRebuild = qfalse;
}

static qboolean LE_Save( void )
{
	RTX_LightEdit_SetError( "not implemented yet" );
	return qfalse;
}

static qboolean LE_Reload( void )
{
	RTX_LightEdit_SetError( "not implemented yet" );
	return qfalse;
}

static void LE_GetStats( rtxLightStats_t *out )
{
	if ( !out )
		return;

	Com_Memset( out, 0, sizeof(*out) );
	out->maxLightPolys = LEDIT_MAX_SLOTS;

	if ( !RTX_LightEdit_IsReady() )
		return;

	out->numRecords = (int)g_records.size();

	for ( size_t i = 0; i < g_records.size(); i++ )
	{
		const rtxLightRecord_t *rec = &g_records[i];

		if ( rec->source == RTX_LSRC_ENTITY )
			out->numEntity++;
		else if ( rec->source == RTX_LSRC_LGT )
			out->numLgt++;
		else if ( !( rec->flags & RTX_LFLAG_DELETED ) )
			out->numAdded++;

		if ( IsModified( rec ) )
			out->numModified++;

		if ( rec->flags & RTX_LFLAG_DISABLED )
			out->numDisabled++;

		if ( rec->flags & RTX_LFLAG_IN_SOLID )
			out->numInSolid++;
	}

	out->numEmissive = g_numEmissive;
	out->numLightPolys = g_world->num_light_polys;
	out->capacity = g_world->allocated_light_polys;
	out->overfullClusters = g_overfull;
	out->rebuilds = g_rebuilds;
	out->unsavedChanges = g_unsaved;
	Q_strncpyz( out->mapName, g_world->baseName, sizeof(out->mapName) );
}

static const char *LE_LastError( void )
{
	return g_lastError;
}

static qboolean LE_PointInSolid( const vec3_t point )
{
	if ( !RTX_LightEdit_IsReady() )
		return qtrue;

	return (qboolean)( PointCluster( point ) < 0 );
}

static rtxLightEditAPI_t g_api = {
	RTX_LIGHTEDIT_API_VERSION,
	LE_IsAvailable,
	LE_Begin,
	LE_End,
	LE_Count,
	LE_Get,
	LE_GetOriginal,
	LE_Add,
	LE_Set,
	LE_Remove,
	LE_Restore,
	LE_Revert,
	LE_BeginDrag,
	LE_EndDrag,
	LE_Save,
	LE_Reload,
	LE_GetStats,
	LE_LastError,
	LE_PointInSolid
};

void *RTX_LightEdit_GetExtension( const char *name )
{
	if ( name && !Q_stricmp( name, RTX_LIGHTEDIT_API_NAME ) )
		return &g_api;

	return NULL;
}

/*
=================
Console commands
=================
*/

static const char *SourceName( int source )
{
	return source == RTX_LSRC_ENTITY ? "entity" : source == RTX_LSRC_LGT ? "lgt" : "added";
}

void RTX_LightEdit_List_f( void )
{
	if ( !LE_IsAvailable() )
	{
		Com_Printf( "light edit is not available\n" );
		return;
	}

	for ( int i = 0; i < LE_Count(); i++ )
	{
		rtxLightDesc_t	d;

		LE_Get( i, &d );

		Com_Printf( "%4i %-6s key %-4i %-6s flags 0x%02x origin %.0f %.0f %.0f intensity %.2f radius %.1f slot %i\n",
			i, SourceName( d.source ), d.sourceKey, d.type == RTX_LTYPE_SPOT ? "spot" : "sphere", d.flags,
			d.origin[0], d.origin[1], d.origin[2], d.intensity, d.radius, g_records[i].lightIndex );
	}
}

void RTX_LightEdit_Add_f( void )
{
	if ( ri.Cmd_Argc() < 4 )
	{
		Com_Printf( "usage: pt_ledit_add x y z [intensity] [r g b]\n" );
		return;
	}

	rtxLightDesc_t	d;

	Com_Memset( &d, 0, sizeof(d) );

	for ( int k = 0; k < 3; k++ )
		d.origin[k] = atof( ri.Cmd_Argv( 1 + k ) );

	d.intensity = ri.Cmd_Argc() > 4 ? atof( ri.Cmd_Argv( 4 ) ) : 300.0f;
	VectorSet( d.color, 1.0f, 1.0f, 1.0f );

	if ( ri.Cmd_Argc() > 7 )
	{
		for ( int k = 0; k < 3; k++ )
			d.color[k] = atof( ri.Cmd_Argv( 5 + k ) );
	}

	d.radius = LEDIT_ADD_RADIUS;

	if ( !LE_Begin() )
	{
		Com_Printf( "light edit: %s\n", g_lastError );
		return;
	}

	const int id = LE_Add( &d );

	if ( id < 0 )
		Com_Printf( "light edit: %s\n", g_lastError );
	else
		Com_Printf( "light %i added\n", id );
}

void RTX_LightEdit_Set_f( void )
{
	if ( ri.Cmd_Argc() < 4 )
	{
		Com_Printf( "usage: pt_ledit_set <id> <origin|color|intensity|radius> <values...>\n" );
		return;
	}

	const int		id = atoi( ri.Cmd_Argv( 1 ) );
	const char		*what = ri.Cmd_Argv( 2 );
	rtxLightDesc_t	d;

	if ( !LE_Get( id, &d ) )
	{
		Com_Printf( "light edit: %s\n", g_lastError );
		return;
	}

	if ( !Q_stricmp( what, "origin" ) && ri.Cmd_Argc() >= 6 )
	{
		for ( int k = 0; k < 3; k++ )
			d.origin[k] = atof( ri.Cmd_Argv( 3 + k ) );
	}
	else if ( !Q_stricmp( what, "color" ) && ri.Cmd_Argc() >= 6 )
	{
		for ( int k = 0; k < 3; k++ )
			d.color[k] = atof( ri.Cmd_Argv( 3 + k ) );
	}
	else if ( !Q_stricmp( what, "intensity" ) )
		d.intensity = atof( ri.Cmd_Argv( 3 ) );
	else if ( !Q_stricmp( what, "radius" ) )
		d.radius = atof( ri.Cmd_Argv( 3 ) );
	else
	{
		Com_Printf( "usage: pt_ledit_set <id> <origin|color|intensity|radius> <values...>\n" );
		return;
	}

	if ( !LE_Set( id, &d ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Del_f( void )
{
	if ( ri.Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: pt_ledit_del <id>\n" );
		return;
	}

	if ( !LE_Remove( atoi( ri.Cmd_Argv( 1 ) ) ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Restore_f( void )
{
	if ( ri.Cmd_Argc() < 2 )
	{
		Com_Printf( "usage: pt_ledit_restore <id>\n" );
		return;
	}

	if ( !LE_Restore( atoi( ri.Cmd_Argv( 1 ) ) ) )
		Com_Printf( "light edit: %s\n", g_lastError );
}

void RTX_LightEdit_Stats_f( void )
{
	rtxLightStats_t s;

	LE_GetStats( &s );

	Com_Printf( "map %s: %i records (%i entity, %i lgt, %i added), %i modified, %i disabled, %i in solid, %i emissive\n",
		s.mapName, s.numRecords, s.numEntity, s.numLgt, s.numAdded, s.numModified, s.numDisabled, s.numInSolid, s.numEmissive );
	Com_Printf( "slots %i (capacity %i, limit %i), %i full clusters, %i rebuilds, %i unsaved changes\n",
		s.numLightPolys, s.capacity, s.maxLightPolys, s.overfullClusters, s.rebuilds, s.unsavedChanges );
}
