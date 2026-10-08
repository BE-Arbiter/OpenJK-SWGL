/*
===========================================================================
Copyright (C) 2026 OpenJK-SWGL contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.
===========================================================================
*/

// Light edit extension of the RTX renderer.
//
// The client gets this table from refexport_t::GetExtension( RTX_LIGHTEDIT_API_NAME ).
// A renderer without the RTX path returns NULL. All calls run on the main thread, between
// two frames. An id is stable until the next map load: a removed light keeps its id.

#pragma once

#define RTX_LIGHTEDIT_API_NAME		"rtxLightEdit_v1"
#define RTX_LIGHTEDIT_API_VERSION	3

#define RTX_LIGHTEDIT_NAME_LEN		32

// Where a light comes from.
typedef enum {
	RTX_LSRC_ADDED = 0,		// made in the editor or read from an "added" block of the .lgt
	RTX_LSRC_ENTITY,		// a `light` entity of the BSP
	RTX_LSRC_LGT			// a light that pt_lightgen reconstructed from the lightmaps
} rtxLightSource_t;

typedef enum {
	RTX_LTYPE_SPHERE = 0,
	RTX_LTYPE_SPOT
} rtxLightType_t;

// Flags of rtxLightDesc_t. Get sets them; Add and Set ignore them.
#define RTX_LFLAG_MODIFIED		0x0001	// a source light that differs from its original values
#define RTX_LFLAG_DISABLED		0x0002	// a source light that Remove switched off
#define RTX_LFLAG_DELETED		0x0004	// an added light that Remove deleted
#define RTX_LFLAG_IN_SOLID		0x0008	// the origin is outside the world; the light emits nothing
#define RTX_LFLAG_DRAGGING		0x0010	// between BeginDrag and EndDrag
#define RTX_LFLAG_MUTED			0x0020	// switched off by Mute or Solo; not saved, not an edit

typedef struct {
	int		id;				// Get only
	int		source;			// rtxLightSource_t, Get only
	int		sourceKey;		// rank among the `light` entities, or .lgt block index; -1 for added
	int		type;			// rtxLightType_t
	int		flags;			// RTX_LFLAG_*, Get only

	vec3_t	origin;
	vec3_t	color;			// 0..1
	float	intensity;		// units of the source: q3map2 `light` for added and entity lights, lightmap units for lgt
	float	radius;			// emitter radius, sets the shadow softness only

	vec3_t	dir;			// spot only, unit vector
	float	coneOuter;		// spot only, half angle in degrees, 1..89
	float	coneInner;		// spot only, half angle in degrees, 0..coneOuter

	char	name[RTX_LIGHTEDIT_NAME_LEN];
} rtxLightDesc_t;

typedef struct {
	int		numRecords;			// ids are 0 .. numRecords - 1
	int		numEntity;			// source lights of each kind, deleted added lights excluded
	int		numLgt;
	int		numAdded;
	int		numModified;
	int		numDisabled;
	int		numInSolid;
	int		numEmissive;		// polygon lights, not editable
	int		numLightPolys;		// static entries the tracer sees, spare slots included
	int		capacity;			// static entries before the next growth
	int		maxLightPolys;		// hard limit on static entries
	int		overfullClusters;	// clusters that hit MAX_LIGHTS_PER_CLUSTER at the last rebuild
	int		rebuilds;			// cluster list rebuilds since the map load
	int		unsavedChanges;		// edits since the last load, save or reload
	char	mapName[64];
} rtxLightStats_t;

typedef struct rtxLightEditAPI_s {
	int			version;	// RTX_LIGHTEDIT_API_VERSION

	// True when the RTX path is active and a world is loaded.
	qboolean	(*IsAvailable)( void );

	// Begin reserves spare light slots. End releases nothing; it ends the drag, if any.
	qboolean	(*Begin)( void );
	void		(*End)( void );

	int			(*Count)( void );
	qboolean	(*Get)( int id, rtxLightDesc_t *out );

	// Original values of a source light, as read at map load. An added light gives the
	// values of its Add call (or of its .lgt block).
	qboolean	(*GetOriginal)( int id, rtxLightDesc_t *out );

	// Add returns the new id, or -1. LastError then gives the reason.
	int			(*Add)( const rtxLightDesc_t *desc );

	// Set copies origin, color, intensity, radius, name, type, dir and cones. A source light then gets
	// RTX_LFLAG_MODIFIED when it differs from its original values.
	qboolean	(*Set)( int id, const rtxLightDesc_t *desc );

	// Remove deletes an added light and disables a source light. Restore undoes Remove.
	qboolean	(*Remove)( int id );
	qboolean	(*Restore)( int id );

	// Revert gives a source light its original values back. It does not touch DISABLED.
	qboolean	(*Revert)( int id );

	// Between BeginDrag and EndDrag, Set moves the light without a full rebuild at each call.
	void		(*BeginDrag)( int id );
	void		(*EndDrag)( int id );

	// Save writes maps/<map>.lgt (format v2) to the homepath, after a copy to .lgt.bak.
	// Reload drops the edits in memory and applies the file again.
	qboolean	(*Save)( void );
	qboolean	(*Reload)( void );

	void		(*GetStats)( rtxLightStats_t *out );
	const char	*(*LastError)( void );

	// True when the point is outside the world (no cluster).
	qboolean	(*PointInSolid)( const vec3_t point );

	// Version 2.

	// Between BeginBatch and EndBatch, the cluster lists are rebuilt once, at EndBatch.
	void		(*BeginBatch)( void );
	void		(*EndBatch)( void );

	// Mute switches a light off for the session only: no edit, no save, no undo.
	// Solo mutes every editable light except id; Solo( -1 ) ends it. Mute and Solo are
	// independent: a light emits when no Mute and no Solo hides it.
	qboolean	(*Mute)( int id, qboolean muted );
	void		(*Solo)( int id );
	int			(*GetSolo)( void );

	// Counts each change of the light list. The tracer restarts its accumulation when it
	// changes.
	int			(*Generation)( void );

	// Factor from the intensity of a light to what the tracer emits, for its source and
	// class (pt_light_scale_*, pt_lightgen_scale). -1 gives the factor of a new added light.
	// A copy between two lights keeps its brightness with I2 = I1 * scale1 / scale2.
	float		(*IntensityScale)( int id );

	// Version 3.

	// Emissive polygon lights of the world (read only). center is the polygon centre, color
	// the emitted colour before the class scale, normalised to max 1.
	int			(*CountEmissive)( void );
	qboolean	(*GetEmissive)( int index, vec3_t center, vec3_t color );

	// Dynamic lights of the last traced frame (dlights, sabers, beams), read only. Fills at
	// most maxCount entries and returns how many it filled.
	int			(*GetDynamic)( int maxCount, vec3_t *origins, vec3_t *colors );
} rtxLightEditAPI_t;
