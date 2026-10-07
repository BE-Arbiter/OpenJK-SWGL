/*
===========================================================================
Cloth and hair physics for Ghoul2 models.

A .phys file next to a .glm names groups of surfaces. Each group is
simulated as one piece of cloth. The animated (skinned) vertex positions
are the targets of the solver in rd-common/phys_cloth.cpp. The result goes
to RB_SurfaceGhoul, which draws the surface from the solved positions
instead of skinning it.
===========================================================================
*/

#include "../server/exe_headers.h"

#ifndef __Q_SHARED_H
	#include "../qcommon/q_shared.h"
#endif

#if !defined(TR_LOCAL_H)
	#include "tr_local.h"
#endif

#if !defined(G2_H_INC)
	#include "../ghoul2/G2.h"
#endif

#include "tr_WorldEffects.h"
#include "../rd-common/phys_cloth.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

static cvar_t	*ph_enable;
static cvar_t	*ph_debug;
static cvar_t	*ph_range;

#define PHYS_LOD				0		// the solver and the draw both use LOD 0
#define PHYS_STALE_FRAMES		2
#define PHYS_STALE_TIME			0.25f
#define PHYS_TELEPORT_DIST		192.0f
#define PHYS_MAX_WIND			600.0f
#define PHYS_DEFAULT_PROXY		4.0f	// cell size of the simulation mesh when the group does not set one

struct physResolvedCollider_t
{
	int		boneA, boneB;
	float	bindA[3];	// bind pose model space, offset included
	float	bindB[3];
	float	radius;
};

struct physGroupInst_t
{
	const physGroupDef_t				*def;
	const physDef_t						*owner;			// the file that holds the group
	std::vector<int>					matched;		// every surface the patterns match
	std::vector<int>					active;			// matched surfaces that are on
	std::vector<unsigned char>			activeMask;		// per surface of the model
	std::vector<unsigned char>			maskScratch;
	std::vector<int>					slotOfSurface;	// per surface of the model, index in active, or -1
	std::vector<std::vector<float> >	skinXYZ;		// per active surface, 3 per vertex, model space
	std::vector<std::vector<float> >	skinNormal;
	std::vector<std::vector<float> >	outXYZ;
	std::vector<std::vector<float> >	outNormal;
	std::vector<physSurfaceOut_t>		out;
	physTopology_t						topo;
	physProxy_t							proxy;			// the simulation mesh and its links to the full mesh
	physCloth_t							cloth;			// state of the simulation mesh
	physLag_t							lag;
	std::vector<float>					proxyTarget;	// per simulation particle, world space
	std::vector<float>					worldPos;		// per particle, world space
	bool								primed;			// the solver has a start state
	std::vector<physCollider_w>			worldColliders;	// of this frame
	std::vector<float>					freeFactor;		// per particle
	std::vector<float>					target;			// per particle, world space
	std::vector<float>					modelPos;		// per particle, model space
	std::vector<float>					particleNormal;
	std::vector<physResolvedCollider_t>	colliders;
	bool								built;
	bool								usable;
	int									frame;			// frame of the last update

	physGroupInst_t() : def( NULL ), owner( NULL ), primed( false ), built( false ), usable( false ), frame( -1 ) {}
};

struct physInstance_t
{
	const void					*header;
	int							epoch;
	const physDef_t				*def;
	int							numSurfaces;
	std::vector<physGroupInst_t> groups;
	std::vector<int>			surfaceGroup;	// per surface of the model, or -1
	int							lastFrame;
	int							lastTime;
	float						lastOrigin[3];

	physInstance_t() : header( NULL ), epoch( 0 ), def( NULL ), numSurfaces( 0 ), lastFrame( -1 ), lastTime( 0 )
	{
		lastOrigin[0] = lastOrigin[1] = lastOrigin[2] = 0.0f;
	}
};

// The renderers keep the mesh header in different places.
static inline const mdxaHeader_t *Phys_AnimHeader( const mdxmHeader_t *header )
{
	const model_t *anim = R_GetModelByHandle( header->animIndex );

	return anim ? anim->mdxa : NULL;
}

static inline const mdxmHeader_t *Phys_Header( const model_t *model )
{
	return model ? model->mdxm : NULL;
}

// Provided by tr_ghoul2.cpp, which owns the bone cache.
const mdxaBone_t		&G2Phys_BoneMatrix( CBoneCache *bc, int bone );
const mdxaHeader_t		*G2Phys_BoneHeader( CBoneCache *bc );
void					**G2Phys_BoneSlot( CBoneCache *bc );

static std::map<std::string, physDef_t *>	s_defs;
static int									s_epoch = 1;
static const char							*s_parseName = "";

//
// Definition cache
//

static void Phys_Print( const char *msg )
{
	ri.Printf( PRINT_WARNING, "%s: %s", s_parseName, msg );
}

static std::string Phys_LowerKey( const char *name )
{
	std::string key = name;

	for ( size_t i = 0; i < key.size(); i++ )
		key[i] = (char)tolower( (unsigned char)key[i] );

	return key;
}

static std::string Phys_FilePath( const std::string &key )
{
	std::string path = key;
	const size_t dot = path.rfind( '.' );

	if ( dot != std::string::npos )
		path.erase( dot );

	return path + ".phys";
}

static physDef_t *Phys_FindDef( const std::string &key )
{
	std::map<std::string, physDef_t *>::iterator it = s_defs.find( key );

	if ( it != s_defs.end() )
		return it->second;

	const std::string path = Phys_FilePath( key );
	physDef_t *def = NULL;
	char *buffer = NULL;
	const long len = ri.FS_ReadFile( path.c_str(), (void **)&buffer );

	if ( len > 0 && buffer )
	{
		s_parseName = path.c_str();
		def = Phys_ParseDef( buffer, path.c_str(), Phys_Print );

		if ( def && ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: loaded %s, %d group(s)\n", path.c_str(), (int)def->groups.size() );
	}

	if ( buffer )
		ri.FS_FreeFile( buffer );

	s_defs[key] = def;
	return def;
}

static const physDef_t *Phys_GetDef( const model_t *glm )
{
	return Phys_FindDef( Phys_LowerKey( glm->name ) );
}

static void Phys_Invalidate( void )
{
	s_epoch++;
}

static void Phys_ClearDefs( void )
{
	for ( std::map<std::string, physDef_t *>::iterator it = s_defs.begin(); it != s_defs.end(); ++it )
		delete it->second;

	s_defs.clear();
	s_epoch++;
}

void G2Phys_Reload_f( void )
{
	Phys_ClearDefs();
	ri.Printf( PRINT_ALL, "phys: definitions cleared, they load again when a model is drawn\n" );
}

void G2Phys_Init( void )
{
	ph_enable = ri.Cvar_Get( "ph_enable", "1", CVAR_ARCHIVE_ND );
	ph_debug = ri.Cvar_Get( "ph_debug", "0", 0 );
	ph_range = ri.Cvar_Get( "ph_range", "1", CVAR_ARCHIVE_ND );
}

void G2Phys_Shutdown( void )
{
	Phys_ClearDefs();
}

void G2Phys_FreeInstance( void *inst )
{
	delete (physInstance_t *)inst;
}

//
// Model data access
//

static const mdxmSurfHierarchy_t *Phys_SurfInfo( const mdxmHeader_t *header, int surfaceNum )
{
	const mdxmHierarchyOffsets_t *offsets = (const mdxmHierarchyOffsets_t *)( (const byte *)header + sizeof( mdxmHeader_t ) );

	return (const mdxmSurfHierarchy_t *)( (const byte *)offsets + offsets->offsets[surfaceNum] );
}

static const mdxaSkel_t *Phys_Skel( const mdxaHeader_t *header, int bone )
{
	const mdxaSkelOffsets_t *offsets = (const mdxaSkelOffsets_t *)( (const byte *)header + sizeof( mdxaHeader_t ) );

	return (const mdxaSkel_t *)( (const byte *)header + sizeof( mdxaHeader_t ) + offsets->offsets[bone] );
}

static int Phys_FindBone( const mdxaHeader_t *header, const std::string &name )
{
	for ( int i = 0; i < header->numBones; i++ )
	{
		if ( !Q_stricmp( Phys_Skel( header, i )->name, name.c_str() ) )
			return i;
	}

	return -1;
}

static void Phys_BindOrigin( const mdxaHeader_t *header, int bone, float out[3] )
{
	const mdxaSkel_t *skel = Phys_Skel( header, bone );

	out[0] = skel->BasePoseMat.matrix[0][3];
	out[1] = skel->BasePoseMat.matrix[1][3];
	out[2] = skel->BasePoseMat.matrix[2][3];
}

// Same blend of bone matrices as the CPU path of RB_SurfaceGhoul.
static void Phys_SkinVertex( CBoneCache *bc, const int *boneRefs, const mdxmVertex_t *v, float *xyz, float *normal )
{
	const int numWeights = G2_GetVertWeights( v );
	float total = 0.0f;

	xyz[0] = xyz[1] = xyz[2] = 0.0f;

	for ( int k = 0; k < numWeights; k++ )
	{
		const mdxaBone_t &bone = G2Phys_BoneMatrix( bc, boneRefs[G2_GetVertBoneIndex( v, k )] );
		const float w = G2_GetVertBoneWeight( v, k, total, numWeights );

		xyz[0] += w * ( DotProduct( bone.matrix[0], v->vertCoords ) + bone.matrix[0][3] );
		xyz[1] += w * ( DotProduct( bone.matrix[1], v->vertCoords ) + bone.matrix[1][3] );
		xyz[2] += w * ( DotProduct( bone.matrix[2], v->vertCoords ) + bone.matrix[2][3] );
	}

	const mdxaBone_t &first = G2Phys_BoneMatrix( bc, boneRefs[G2_GetVertBoneIndex( v, 0 )] );

	normal[0] = DotProduct( first.matrix[0], v->normal );
	normal[1] = DotProduct( first.matrix[1], v->normal );
	normal[2] = DotProduct( first.matrix[2], v->normal );
	VectorNormalize( normal );
}

//
// Instance creation
//

static physInstance_t *Phys_CreateInstance( const model_t *glm )
{
	physInstance_t *inst = new physInstance_t;
	const mdxmHeader_t *header = Phys_Header( glm );

	inst->header = header;
	inst->epoch = s_epoch;
	inst->def = Phys_GetDef( glm );
	inst->numSurfaces = header->numSurfaces;

	if ( !inst->def )
		return inst;

	inst->groups.resize( inst->def->groups.size() );
	inst->surfaceGroup.assign( header->numSurfaces, -1 );

	bool any = false;

	for ( size_t g = 0; g < inst->def->groups.size(); g++ )
	{
		inst->groups[g].def = &inst->def->groups[g];
		inst->groups[g].owner = inst->def;
	}

	for ( int s = 0; s < header->numSurfaces; s++ )
	{
		const char *name = Phys_SurfInfo( header, s )->name;

		for ( size_t g = 0; g < inst->groups.size() && inst->surfaceGroup[s] < 0; g++ )
		{
			const physGroupDef_t &def = inst->def->groups[g];

			if ( !def.enabled )
				continue;

			for ( size_t p = 0; p < def.surfaces.size(); p++ )
			{
				if ( Phys_MatchName( def.surfaces[p].c_str(), name ) )
				{
					inst->surfaceGroup[s] = (int)g;
					inst->groups[g].matched.push_back( s );
					any = true;
					break;
				}
			}
		}
	}

	if ( !any )
	{
		inst->groups.clear();
		inst->surfaceGroup.clear();

		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: %s has a .phys file but no surface matches\n", glm->name );
	}

	return inst;
}

//
// Group build
//

static float Phys_Clamp01( float f )
{
	return f < 0.0f ? 0.0f : ( f > 1.0f ? 1.0f : f );
}

// The pin factor of a particle: 0 is fixed to the animation, 1 is free.
static float Phys_PinFactor( const physGroupDef_t &def, const mdxaHeader_t *aHeader, const std::vector<int> &ruleBone,
							 const std::vector<std::vector<int> > &ruleBones, const float *bindPos,
							 const mdxmSurface_t *surf, int vertex )
{
	if ( def.pins.empty() )
		return 1.0f;

	const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
	const mdxmVertexTexCoord_t *texCoords = (const mdxmVertexTexCoord_t *)&verts[surf->numVerts];
	const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );
	float result = 1.0f;

	for ( size_t r = 0; r < def.pins.size(); r++ )
	{
		const physPinRule_t &rule = def.pins[r];
		float t = 1.0f;

		if ( rule.type == PHYS_PIN_GRADIENT )
		{
			if ( ruleBone[r] < 0 )
				continue;

			float origin[3];
			Phys_BindOrigin( aHeader, ruleBone[r], origin );

			const float c = ( bindPos[0] - origin[0] ) * rule.axis[0] + ( bindPos[1] - origin[1] ) * rule.axis[1] + ( bindPos[2] - origin[2] ) * rule.axis[2];

			t = Phys_Clamp01( ( c - rule.pinned ) / ( rule.free - rule.pinned ) );
		}
		else if ( rule.type == PHYS_PIN_RADIUS )
		{
			if ( ruleBone[r] < 0 )
				continue;

			float origin[3];
			Phys_BindOrigin( aHeader, ruleBone[r], origin );

			const float d[3] = { bindPos[0] - origin[0] - rule.axis[0], bindPos[1] - origin[1] - rule.axis[1], bindPos[2] - origin[2] - rule.axis[2] };
			const float dist = sqrtf( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );

			t = Phys_Clamp01( ( dist - rule.pinned ) / ( rule.free - rule.pinned ) );
		}
		else if ( rule.type == PHYS_PIN_BONES )
		{
			const mdxmVertex_t *v = &verts[vertex];
			const int numWeights = G2_GetVertWeights( v );
			float total = 0.0f;
			float weight = 0.0f;

			for ( int k = 0; k < numWeights; k++ )
			{
				const int bone = boneRefs[G2_GetVertBoneIndex( v, k )];
				const float w = G2_GetVertBoneWeight( v, k, total, numWeights );

				for ( size_t b = 0; b < ruleBones[r].size(); b++ )
				{
					if ( ruleBones[r][b] == bone )
					{
						weight += w;
						break;
					}
				}
			}

			t = Phys_Clamp01( 1.0f - weight );
		}
		else
		{
			t = Phys_Clamp01( ( texCoords[vertex].texCoords[1] - rule.pinned ) / ( rule.free - rule.pinned ) );
		}

		if ( t < result )
			result = t;
	}

	return result;
}

//
// Colliders fitted to the model: one capsule for each bone that carries body vertices.
//

#define PHYS_MIN_SURFACE_VERTS	20		// a surface with less vertices is a tag or a cap, not a real surface
#define PHYS_FIT_MIN_POINTS		6
#define PHYS_FIT_MAX_COLLIDERS	32

static bool Phys_RealSurface( const mdxmSurface_t *surf )
{
	return surf && surf->numVerts >= PHYS_MIN_SURFACE_VERTS;
}

static bool Phys_SurfaceInGroup( const physDef_t *def, const char *name )
{
	for ( size_t g = 0; g < def->groups.size(); g++ )
	{
		for ( size_t p = 0; p < def->groups[g].surfaces.size(); p++ )
		{
			if ( Phys_MatchName( def->groups[g].surfaces[p].c_str(), name ) )
				return true;
		}
	}

	return false;
}

struct physFitBone_t
{
	std::vector<float>	pts;	// bind pose, 3 per vertex
	int					bone;
};

static float Phys_Percentile( std::vector<float> &v, float f )
{
	if ( v.empty() )
		return 0.0f;

	std::sort( v.begin(), v.end() );

	return v[(size_t)( f * (float)( v.size() - 1 ) )];
}

static void Phys_FitColliders( const model_t *glm, const mdxaHeader_t *aHeader, const physDef_t *def, float scale,
							   std::vector<physCollider_t> &out )
{
	const mdxmHeader_t *header = Phys_Header( glm );
	const int numBones = aHeader->numBones;
	std::vector<std::vector<float> > pts( numBones );

	out.clear();

	// The vertices of the body, with the bone that has most of their weight.
	for ( int s = 0; s < header->numSurfaces; s++ )
	{
		const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, s, PHYS_LOD );

		if ( !Phys_RealSurface( surf ) || Phys_SurfaceInGroup( def, Phys_SurfInfo( header, s )->name ) )
			continue;

		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
		const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );

		for ( int i = 0; i < surf->numVerts; i++ )
		{
			const int numWeights = G2_GetVertWeights( &verts[i] );
			float total = 0.0f, best = 0.0f;
			int bestBone = -1;

			for ( int k = 0; k < numWeights; k++ )
			{
				const int bone = boneRefs[G2_GetVertBoneIndex( &verts[i], k )];
				const float w = G2_GetVertBoneWeight( &verts[i], k, total, numWeights );

				if ( w > best )
				{
					best = w;
					bestBone = bone;
				}
			}

			if ( bestBone >= 0 && bestBone < numBones && best >= 0.5f )
			{
				pts[bestBone].push_back( verts[i].vertCoords[0] );
				pts[bestBone].push_back( verts[i].vertCoords[1] );
				pts[bestBone].push_back( verts[i].vertCoords[2] );
			}
		}
	}

	std::vector<std::pair<int, physCollider_t> > found;

	for ( int b = 0; b < numBones; b++ )
	{
		const int count = (int)pts[b].size() / 3;

		if ( count < PHYS_FIT_MIN_POINTS )
			continue;

		const mdxaSkel_t *skel = Phys_Skel( aHeader, b );
		float origin[3];

		Phys_BindOrigin( aHeader, b, origin );

		// The child that carries most vertices gives the direction of the capsule.
		int child = -1, childCount = 0;

		for ( int c = 0; c < skel->numChildren; c++ )
		{
			const int n = (int)pts[skel->children[c]].size() / 3;

			if ( n > childCount )
			{
				childCount = n;
				child = skel->children[c];
			}
		}

		float end[3] = { origin[0], origin[1], origin[2] };
		bool capsule = false;

		if ( child >= 0 )
		{
			Phys_BindOrigin( aHeader, child, end );

			const float d[3] = { end[0] - origin[0], end[1] - origin[1], end[2] - origin[2] };

			capsule = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > 2.25f;
		}

		physCollider_t c;
		std::vector<float> dist;

		c.boneA = skel->name;
		c.offA[0] = c.offA[1] = c.offA[2] = 0.0f;
		c.offB[0] = c.offB[1] = c.offB[2] = 0.0f;

		if ( capsule )
		{
			c.capsule = true;
			c.boneB = Phys_Skel( aHeader, child )->name;

			float ab[3] = { end[0] - origin[0], end[1] - origin[1], end[2] - origin[2] };
			const float len2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];

			for ( int i = 0; i < count; i++ )
			{
				const float *p = &pts[b][i * 3];
				const float ap[3] = { p[0] - origin[0], p[1] - origin[1], p[2] - origin[2] };
				float t = ( ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2] ) / len2;

				t = t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );

				const float q[3] = { ap[0] - ab[0] * t, ap[1] - ab[1] * t, ap[2] - ab[2] * t };

				dist.push_back( sqrtf( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] ) );
			}
		}
		else
		{
			// A bone at the end of a chain: a sphere at the center of its vertices.
			float centroid[3] = { 0.0f, 0.0f, 0.0f };

			for ( int i = 0; i < count; i++ )
			{
				for ( int k = 0; k < 3; k++ )
					centroid[k] += pts[b][i * 3 + k] / (float)count;
			}

			c.capsule = false;
			c.boneB = c.boneA;

			for ( int k = 0; k < 3; k++ )
				c.offA[k] = c.offB[k] = centroid[k] - origin[k];

			for ( int i = 0; i < count; i++ )
			{
				const float *p = &pts[b][i * 3];
				const float q[3] = { p[0] - centroid[0], p[1] - centroid[1], p[2] - centroid[2] };

				dist.push_back( sqrtf( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] ) );
			}
		}

		float radius = Phys_Percentile( dist, 0.85f ) * scale;

		radius = radius < 1.0f ? 1.0f : ( radius > 14.0f ? 14.0f : radius );
		c.radius = radius;
		found.push_back( std::make_pair( count, c ) );
	}

	// Keep the colliders with most vertices.
	std::sort( found.begin(), found.end(), []( const std::pair<int, physCollider_t> &a, const std::pair<int, physCollider_t> &b ) { return a.first > b.first; } );

	for ( size_t i = 0; i < found.size() && i < PHYS_FIT_MAX_COLLIDERS; i++ )
		out.push_back( found[i].second );
}

static void Phys_BuildGroup( physGroupInst_t &gi, const model_t *glm, CBoneCache *bc, const char *modelName )
{
	const physGroupDef_t &def = *gi.def;
	const mdxaHeader_t *aHeader = G2Phys_BoneHeader( bc );
	const int numActive = (int)gi.active.size();

	gi.built = true;
	gi.usable = false;

	std::vector<const mdxmSurface_t *> surfs( numActive );
	std::vector<physSurfaceInput_t> inputs( numActive );
	std::vector<std::vector<float> > bind( numActive );

	for ( int s = 0; s < numActive; s++ )
	{
		surfs[s] = (const mdxmSurface_t *)G2_FindSurface( glm, gi.active[s], PHYS_LOD );

		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surfs[s] + surfs[s]->ofsVerts );

		bind[s].resize( surfs[s]->numVerts * 3 );

		for ( int i = 0; i < surfs[s]->numVerts; i++ )
			VectorCopy( verts[i].vertCoords, &bind[s][i * 3] );

		inputs[s].numVerts = surfs[s]->numVerts;
		inputs[s].xyz = bind[s].empty() ? NULL : &bind[s][0];
		inputs[s].numTris = surfs[s]->numTriangles;
		inputs[s].tris = (const int *)( (const byte *)surfs[s] + surfs[s]->ofsTriangles );
	}

	if ( numActive )
		Phys_BuildTopology( gi.topo, &inputs[0], numActive );
	else
		gi.topo = physTopology_t();

	if ( gi.topo.numParticles == 0 )
		return;

	// Resolve the bones of the pin rules.
	std::vector<int> ruleBone( def.pins.size(), -1 );
	std::vector<std::vector<int> > ruleBones( def.pins.size() );

	for ( size_t r = 0; r < def.pins.size(); r++ )
	{
		if ( def.pins[r].type == PHYS_PIN_GRADIENT || def.pins[r].type == PHYS_PIN_RADIUS )
		{
			ruleBone[r] = Phys_FindBone( aHeader, def.pins[r].bone );

			if ( ruleBone[r] < 0 )
				ri.Printf( PRINT_WARNING, "phys: %s: pin bone '%s' not found\n", modelName, def.pins[r].bone.c_str() );
		}
		else if ( def.pins[r].type == PHYS_PIN_BONES )
		{
			for ( size_t b = 0; b < def.pins[r].bones.size(); b++ )
			{
				const int bone = Phys_FindBone( aHeader, def.pins[r].bones[b] );

				if ( bone >= 0 )
					ruleBones[r].push_back( bone );
				else
					ri.Printf( PRINT_WARNING, "phys: %s: pin bone '%s' not found\n", modelName, def.pins[r].bones[b].c_str() );
			}
		}
	}

	const int n = gi.topo.numParticles;

	gi.freeFactor.assign( n, 1.0f );

	int numFree = 0;

	for ( int i = 0; i < n; i++ )
	{
		const int first = gi.topo.firstVertex[i];
		const int s = first >> 24;
		const int vertex = first & 0xFFFFFF;

		gi.freeFactor[i] = Phys_PinFactor( def, aHeader, ruleBone, ruleBones, &gi.topo.bindPos[i * 3], surfs[s], vertex );

		if ( gi.freeFactor[i] > 0.01f )
			numFree++;
	}

	gi.proxy = physProxy_t();
	gi.lag = physLag_t();
	gi.primed = false;

	if ( !def.lag )
	{
		// The soft transform of the lag mode has no per particle state. A cloth runs on a coarse mesh.
		Phys_BuildProxy( gi.proxy, gi.topo, &gi.freeFactor[0], def.proxy > 0.0f ? def.proxy : PHYS_DEFAULT_PROXY );
		Phys_ClothInit( gi.cloth, def, &gi.proxy.freeFactor[0], gi.proxy.numProxy );
		gi.proxyTarget.assign( gi.proxy.numProxy * 3, 0.0f );
	}

	gi.worldPos.assign( n * 3, 0.0f );

	gi.target.assign( n * 3, 0.0f );
	gi.modelPos.assign( n * 3, 0.0f );
	gi.particleNormal.assign( n * 3, 0.0f );

	gi.skinXYZ.assign( numActive, std::vector<float>() );
	gi.skinNormal.assign( numActive, std::vector<float>() );
	gi.outXYZ.assign( numActive, std::vector<float>() );
	gi.outNormal.assign( numActive, std::vector<float>() );
	gi.out.assign( numActive, physSurfaceOut_t() );
	gi.slotOfSurface.assign( Phys_Header( glm )->numSurfaces, -1 );

	for ( int s = 0; s < numActive; s++ )
	{
		const int count = surfs[s]->numVerts;

		gi.skinXYZ[s].assign( count * 3, 0.0f );
		gi.skinNormal[s].assign( count * 3, 0.0f );
		gi.outXYZ[s].assign( count * 3, 0.0f );
		gi.outNormal[s].assign( count * 3, 0.0f );
		gi.out[s].numVerts = count;
		gi.out[s].xyz = count ? &gi.outXYZ[s][0] : NULL;
		gi.out[s].normal = count ? &gi.outNormal[s][0] : NULL;
		gi.slotOfSurface[gi.active[s]] = s;
	}

	// Colliders.
	gi.colliders.clear();

	std::vector<physCollider_t> expanded;

	std::vector<physCollider_t> fitted;

	if ( def.modelFit && gi.owner )
		Phys_FitColliders( glm, aHeader, gi.owner, def.modelFitScale, fitted );

	Phys_ExpandColliders( def, expanded, &fitted );

	for ( size_t c = 0; c < expanded.size(); c++ )
	{
		const physCollider_t &src = expanded[c];
		physResolvedCollider_t rc;

		rc.boneA = Phys_FindBone( aHeader, src.boneA );
		rc.boneB = src.capsule ? Phys_FindBone( aHeader, src.boneB ) : rc.boneA;

		if ( rc.boneA < 0 || rc.boneB < 0 )
		{
			if ( ph_debug->integer )
				ri.Printf( PRINT_ALL, "phys: %s: collider bone '%s' or '%s' not found, skipped\n", modelName, src.boneA.c_str(), src.boneB.c_str() );

			continue;
		}

		float a[3], b[3];
		Phys_BindOrigin( aHeader, rc.boneA, a );
		Phys_BindOrigin( aHeader, rc.boneB, b );

		for ( int k = 0; k < 3; k++ )
		{
			rc.bindA[k] = a[k] + src.offA[k];
			rc.bindB[k] = b[k] + src.offB[k];
		}

		rc.radius = src.radius;
		gi.colliders.push_back( rc );
	}

	gi.usable = true;

	if ( ph_debug->integer )
	{
		ri.Printf( PRINT_ALL, "phys: %s group '%s' (%s): %d surface(s), %d particle(s), %d free, %d solved, %d edge(s), %d collider(s)\n",
			modelName, def.name.c_str(), def.lag ? "lag" : "cloth", numActive, n, numFree,
			def.lag ? 0 : gi.proxy.numProxy,
			(int)gi.proxy.topo.edgeRest.size(), (int)gi.colliders.size() );
	}
}

//
// Per frame update
//

static inline void Phys_ModelToWorld( const trRefEntity_t *ent, const float *p, float *out )
{
	for ( int k = 0; k < 3; k++ )
		out[k] = ent->e.origin[k] + p[0] * ent->e.axis[0][k] + p[1] * ent->e.axis[1][k] + p[2] * ent->e.axis[2][k];
}

static inline void Phys_WorldToModel( const trRefEntity_t *ent, const float *w, float *out )
{
	const float d[3] = { w[0] - ent->e.origin[0], w[1] - ent->e.origin[1], w[2] - ent->e.origin[2] };

	for ( int k = 0; k < 3; k++ )
	{
		const float len2 = DotProduct( ent->e.axis[k], ent->e.axis[k] );

		out[k] = len2 > 1e-6f ? DotProduct( d, ent->e.axis[k] ) / len2 : 0.0f;
	}
}

//
// Debug view of the colliders (ph_debug 2): dots along the axis and around the middle of each collider.
//

#define PHYS_MAX_COLLIDER_DOTS	16384

static polyVert_t	s_dotVerts[PHYS_MAX_COLLIDER_DOTS * 4];
static srfPoly_t	s_dotPolys[PHYS_MAX_COLLIDER_DOTS];
static int			s_dotFrame = -1;
static int			s_dotCount = 0;

static void Phys_AddDot( const trRefEntity_t *ent, const float p[3], float size, const byte col[4], const float left[3], const float up[3] )
{
	static const float corner[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
	shader_t *shader = tr.physMarkShader;

	if ( s_dotFrame != tr.frameCount )
	{
		s_dotFrame = tr.frameCount;
		s_dotCount = 0;
	}

	if ( s_dotCount >= PHYS_MAX_COLLIDER_DOTS )
		return;

	polyVert_t *v = &s_dotVerts[s_dotCount * 4];

	for ( int i = 0; i < 4; i++ )
	{
		for ( int k = 0; k < 3; k++ )
			v[i].xyz[k] = p[k] + ( left[k] * corner[i][0] + up[k] * corner[i][1] ) * size;

		v[i].st[0] = v[i].st[1] = 0.0f;
		Com_Memcpy( v[i].modulate, col, 4 );
	}

	s_dotPolys[s_dotCount].surfaceType = SF_POLY;
	s_dotPolys[s_dotCount].hShader = 0;
	s_dotPolys[s_dotCount].fogIndex = 0;
	s_dotPolys[s_dotCount].numVerts = 4;
	s_dotPolys[s_dotCount].verts = v;
	R_AddDrawSurf( (surfaceType_t *)&s_dotPolys[s_dotCount], shader, 0, qfalse );
	s_dotCount++;
}

// a and b are the ends of the collider in the model space of the entity.
static void Phys_DrawCollider( const trRefEntity_t *ent, const float a[3], const float b[3], float radius )
{
	const byte col[4] = { 255, 150, 0, 255 };
	float left[3], up[3];

	for ( int k = 0; k < 3; k++ )
	{
		const float len2 = DotProduct( ent->e.axis[k], ent->e.axis[k] );

		left[k] = len2 > 1e-6f ? DotProduct( tr.refdef.viewaxis[1], ent->e.axis[k] ) / len2 : 0.0f;
		up[k] = len2 > 1e-6f ? DotProduct( tr.refdef.viewaxis[2], ent->e.axis[k] ) / len2 : 0.0f;
	}

	float axis[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
	const float len = sqrtf( DotProduct( axis, axis ) );

	// The axis of the capsule.
	if ( len > 0.5f )
	{
		for ( int i = 0; i <= 8; i++ )
		{
			const float t = (float)i / 8.0f;
			const float p[3] = { a[0] + axis[0] * t, a[1] + axis[1] * t, a[2] + axis[2] * t };

			Phys_AddDot( ent, p, 0.35f, col, left, up );
		}

		axis[0] /= len;
		axis[1] /= len;
		axis[2] /= len;
	}
	else
	{
		axis[0] = 0.0f;
		axis[1] = 0.0f;
		axis[2] = 1.0f;
	}

	// Two directions that are perpendicular to the axis.
	float u[3], v[3];
	const float ref[3] = { fabsf( axis[2] ) < 0.9f ? 0.0f : 1.0f, fabsf( axis[2] ) < 0.9f ? 0.0f : 0.0f, fabsf( axis[2] ) < 0.9f ? 1.0f : 0.0f };

	u[0] = axis[1] * ref[2] - axis[2] * ref[1];
	u[1] = axis[2] * ref[0] - axis[0] * ref[2];
	u[2] = axis[0] * ref[1] - axis[1] * ref[0];
	VectorNormalize( u );
	v[0] = axis[1] * u[2] - axis[2] * u[1];
	v[1] = axis[2] * u[0] - axis[0] * u[2];
	v[2] = axis[0] * u[1] - axis[1] * u[0];

	// The radius, around the middle and around the ends.
	for ( int e = 0; e < 3; e++ )
	{
		const float t = e * 0.5f;
		const float c[3] = { a[0] + ( b[0] - a[0] ) * t, a[1] + ( b[1] - a[1] ) * t, a[2] + ( b[2] - a[2] ) * t };

		for ( int i = 0; i < 12; i++ )
		{
			const float ang = (float)i * ( 6.2831853f / 12.0f );
			const float p[3] = {
				c[0] + ( u[0] * cosf( ang ) + v[0] * sinf( ang ) ) * radius,
				c[1] + ( u[1] * cosf( ang ) + v[1] * sinf( ang ) ) * radius,
				c[2] + ( u[2] * cosf( ang ) + v[2] * sinf( ang ) ) * radius };

			Phys_AddDot( ent, p, 0.3f, col, left, up );
		}
	}
}

static void Phys_UpdateGroup( physInstance_t *inst, physGroupInst_t &gi, const trRefEntity_t *ent, const model_t *glm,
							  CBoneCache *bc, const surfaceInfo_v &slist, bool stale, float dt )
{
	const physGroupDef_t &def = *gi.def;
	const mdxmHeader_t *header = Phys_Header( glm );
	const int numSurfaces = header->numSurfaces;

	// Surfaces that are switched off leave the group.
	std::vector<unsigned char> &mask = gi.maskScratch;

	mask.assign( numSurfaces, 0 );

	for ( size_t i = 0; i < gi.matched.size(); i++ )
	{
		const int s = gi.matched[i];
		const surfaceInfo_t *override = G2_FindOverrideSurface( s, slist );
		const unsigned int offFlags = override ? override->offFlags : Phys_SurfInfo( header, s )->flags;

		if ( !offFlags )
			mask[s] = 1;
	}

	if ( !gi.built || mask != gi.activeMask )
	{
		gi.activeMask = mask;
		gi.active.clear();

		for ( size_t i = 0; i < gi.matched.size(); i++ )
		{
			if ( mask[gi.matched[i]] )
				gi.active.push_back( gi.matched[i] );
		}

		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: group '%s': %d surface(s) match, %d on\n", gi.def->name.c_str(), (int)gi.matched.size(), (int)gi.active.size() );

		Phys_BuildGroup( gi, glm, bc, glm->name );
		stale = true;
	}

	if ( !gi.usable )
		return;

	const int n = gi.topo.numParticles;
	const int numActive = (int)gi.active.size();

	// Animated positions of every vertex.
	for ( int s = 0; s < numActive; s++ )
	{
		const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, gi.active[s], PHYS_LOD );
		const mdxmVertex_t *verts = (const mdxmVertex_t *)( (const byte *)surf + surf->ofsVerts );
		const int *boneRefs = (const int *)( (const byte *)surf + surf->ofsBoneReferences );

		for ( int i = 0; i < surf->numVerts; i++ )
			Phys_SkinVertex( bc, boneRefs, &verts[i], &gi.skinXYZ[s][i * 3], &gi.skinNormal[s][i * 3] );
	}

	for ( int i = 0; i < n; i++ )
	{
		const int first = gi.topo.firstVertex[i];

		Phys_ModelToWorld( ent, &gi.skinXYZ[first >> 24][( first & 0xFFFFFF ) * 3], &gi.target[i * 3] );
	}

	// Colliders of this frame, in world space.
	std::vector<physCollider_w> &colliders = gi.worldColliders;

	colliders.resize( gi.colliders.size() );

	for ( size_t c = 0; c < gi.colliders.size(); c++ )
	{
		const physResolvedCollider_t &rc = gi.colliders[c];
		const mdxaBone_t &ma = G2Phys_BoneMatrix( bc, rc.boneA );
		const mdxaBone_t &mb = G2Phys_BoneMatrix( bc, rc.boneB );
		float a[3], b[3];

		for ( int k = 0; k < 3; k++ )
		{
			a[k] = DotProduct( ma.matrix[k], rc.bindA ) + ma.matrix[k][3];
			b[k] = DotProduct( mb.matrix[k], rc.bindB ) + mb.matrix[k][3];
		}

		Phys_ModelToWorld( ent, a, colliders[c].a );
		Phys_ModelToWorld( ent, b, colliders[c].b );
		colliders[c].radius = rc.radius;

		if ( ph_debug->integer >= 2 )
			Phys_DrawCollider( ent, a, b, rc.radius );

		if ( ph_debug->integer >= 3 && c == 0 )
			ri.Printf( PRINT_ALL, "phys dots: %s origin (%.0f %.0f %.0f) frame %d\n", glm->name, ent->e.origin[0], ent->e.origin[1], ent->e.origin[2], tr.frameCount );
	}

	const physCollider_w *colliderData = colliders.empty() ? NULL : &colliders[0];
	const bool hasGround = ( ent->e.renderfx & RF_SHADOW_PLANE ) != 0;

	if ( stale || !gi.primed )
	{
		if ( ph_debug->integer )
			ri.Printf( PRINT_ALL, "phys: reset %s at (%.0f %.0f %.0f) facing (%.2f %.2f)\n", glm->name, ent->e.origin[0], ent->e.origin[1], ent->e.origin[2], ent->e.axis[0][0], ent->e.axis[0][1] );

		if ( def.lag )
		{
			Phys_LagReset( gi.lag, &gi.target[0], &gi.freeFactor[0], n );
		}
		else
		{
			Phys_ProxyTargets( gi.proxy, &gi.target[0], &gi.proxyTarget[0] );
			Phys_ClothReset( gi.cloth, &gi.proxyTarget[0] );
		}

		gi.primed = true;
	}
	else if ( dt > 0.0f )
	{
		float windVel[3] = { 0.0f, 0.0f, 0.0f };

		if ( def.wind > 0.0f )
		{
			vec3_t point, dir;
			float speed = 0.0f;

			VectorCopy( ent->e.origin, point );

			if ( R_GetWindVector( dir, point ) && R_GetWindSpeed( speed, point ) )
			{
				speed *= def.wind;

				if ( speed > PHYS_MAX_WIND )
					speed = PHYS_MAX_WIND;

				VectorScale( dir, speed, windVel );
			}
		}

		if ( def.lag )
		{
			Phys_LagStep( gi.lag, def, dt, &gi.target[0], &gi.freeFactor[0], n, windVel );
		}
		else
		{
			Phys_ProxyTargets( gi.proxy, &gi.target[0], &gi.proxyTarget[0] );
			Phys_ClothStep( gi.cloth, gi.proxy.topo, def, dt, &gi.proxyTarget[0], windVel,
				colliderData, (int)colliders.size(), hasGround, ent->e.shadowPlane );
		}
	}

	// Solved positions of every particle, in world space.
	if ( def.lag )
	{
		Phys_LagApply( gi.lag, &gi.target[0], &gi.freeFactor[0], n, &gi.worldPos[0] );
		Phys_LagCollide( gi.lag, def, &gi.target[0], &gi.freeFactor[0], n, colliderData, (int)colliders.size(), hasGround, ent->e.shadowPlane, &gi.worldPos[0] );
	}
	else
	{
		Phys_ProxyApply( gi.proxy, &gi.cloth.x[0], &gi.proxyTarget[0], &gi.target[0], &gi.freeFactor[0], &gi.worldPos[0] );
		Phys_ProjectColliders( &gi.worldPos[0], &gi.target[0], &gi.freeFactor[0], n, def.margin, colliderData, (int)colliders.size(), hasGround, ent->e.shadowPlane );
	}

	// Solved positions, back in model space.
	for ( int i = 0; i < n; i++ )
		Phys_WorldToModel( ent, &gi.worldPos[i * 3], &gi.modelPos[i * 3] );

	Phys_ComputeNormals( gi.topo, &gi.modelPos[0], &gi.particleNormal[0] );

	for ( int s = 0; s < numActive; s++ )
	{
		const std::vector<int> &vp = gi.topo.vertParticle[s];

		for ( size_t i = 0; i < vp.size(); i++ )
		{
			const int p = vp[i];
			float *xyz = &gi.outXYZ[s][i * 3];
			float *nrm = &gi.outNormal[s][i * 3];
			const float *skin = &gi.skinNormal[s][i * 3];
			float sim[3] = { gi.particleNormal[p * 3], gi.particleNormal[p * 3 + 1], gi.particleNormal[p * 3 + 2] };

			VectorCopy( &gi.modelPos[p * 3], xyz );

			// A pinned vertex keeps the animated normal. The cloth is two sided, so the
			// solved normal takes the side of the animated one.
			if ( DotProduct( sim, skin ) < 0.0f )
				VectorScale( sim, -1.0f, sim );

			const float w = Phys_Clamp01( gi.freeFactor[p] * 2.0f );

			nrm[0] = skin[0] + ( sim[0] - skin[0] ) * w;
			nrm[1] = skin[1] + ( sim[1] - skin[1] ) * w;
			nrm[2] = skin[2] + ( sim[2] - skin[2] ) * w;

			if ( VectorNormalize( nrm ) == 0.0f )
				VectorCopy( skin, nrm );
		}
	}

	gi.frame = tr.frameCount;
}

physInstance_t *G2Phys_Update( const trRefEntity_t *ent, CBoneCache *bc, const model_t *glm, const surfaceInfo_v &slist )
{
	if ( !ph_enable->integer || !bc || !glm || !Phys_Header( glm ) )
		return NULL;

	physInstance_t **slot = (physInstance_t **)G2Phys_BoneSlot( bc );

	if ( *slot && ( (*slot)->header != Phys_Header( glm ) || (*slot)->epoch != s_epoch ) )
	{
		delete *slot;
		*slot = NULL;
	}

	if ( !*slot )
		*slot = Phys_CreateInstance( glm );

	physInstance_t *inst = *slot;

	if ( inst->groups.empty() )
		return NULL;

	// Another view of the same frame reuses the solved positions.
	if ( inst->lastFrame == tr.frameCount )
		return inst;

	const float distance = Distance( ent->e.origin, tr.viewParms.ori.origin );
	bool inRange = false;

	for ( size_t g = 0; g < inst->groups.size(); g++ )
	{
		if ( inst->groups[g].def->range <= 0.0f || inst->groups[g].def->range * ph_range->value >= distance )
			inRange = true;
	}

	if ( !inRange )
		return NULL;

	const float dt = ( tr.refdef.time - inst->lastTime ) * 0.001f;
	bool stale = inst->lastFrame < 0 || tr.frameCount - inst->lastFrame > PHYS_STALE_FRAMES || dt < 0.0f || dt > PHYS_STALE_TIME;

	if ( DistanceSquared( ent->e.origin, inst->lastOrigin ) > PHYS_TELEPORT_DIST * PHYS_TELEPORT_DIST )
		stale = true;

	for ( size_t g = 0; g < inst->groups.size(); g++ )
	{
		if ( !inst->groups[g].matched.empty() )
			Phys_UpdateGroup( inst, inst->groups[g], ent, glm, bc, slist, stale, dt );
	}

	inst->lastFrame = tr.frameCount;
	inst->lastTime = tr.refdef.time;
	VectorCopy( ent->e.origin, inst->lastOrigin );

	return inst;
}

const physSurfaceOut_t *G2Phys_GetSurface( const physInstance_t *inst, int surfaceNum )
{
	if ( !inst || inst->lastFrame != tr.frameCount || surfaceNum < 0 || surfaceNum >= (int)inst->surfaceGroup.size() )
		return NULL;

	const int g = inst->surfaceGroup[surfaceNum];

	if ( g < 0 )
		return NULL;

	const physGroupInst_t &gi = inst->groups[g];

	if ( !gi.usable || gi.frame != tr.frameCount || surfaceNum >= (int)gi.slotOfSurface.size() )
		return NULL;

	const int slot = gi.slotOfSurface[surfaceNum];

	return slot >= 0 ? &gi.out[slot] : NULL;
}


//
// Editor: the menu changes the definition of one model and sees the result on the preview.
//

static qhandle_t	s_editHandle = 0;
static std::string	s_editKey;
static std::string	s_highlight;
static std::string	s_editMessage;

static void Phys_EditPrint( const char *msg )
{
	s_editMessage += msg;
}

// Blinks the surface that the editor shows.
qboolean G2Phys_Highlighted( const char *surfaceName )
{
	if ( s_highlight.empty() || !surfaceName )
		return qfalse;

	if ( ( tr.refdef.time / 250 ) & 1 )
		return qfalse;

	// The text is a list of names or patterns separated by spaces.
	const char *p = s_highlight.c_str();

	while ( *p )
	{
		while ( *p == ' ' )
			p++;

		const char *start = p;

		while ( *p && *p != ' ' )
			p++;

		if ( p > start )
		{
			const std::string pattern( start, p );

			if ( Phys_MatchName( pattern.c_str(), surfaceName ) )
				return qtrue;
		}
	}

	return qfalse;
}

static physDef_t *Phys_EditDef( void )
{
	physDef_t *def = Phys_FindDef( s_editKey );

	if ( !def )
	{
		def = new physDef_t;
		def->name = Phys_FilePath( s_editKey );
		s_defs[s_editKey] = def;
	}

	return def;
}

static int Phys_EditGroupOfSurface( const physDef_t *def, const char *surfaceName )
{
	for ( size_t g = 0; g < def->groups.size(); g++ )
	{
		for ( size_t p = 0; p < def->groups[g].surfaces.size(); p++ )
		{
			if ( Phys_MatchName( def->groups[g].surfaces[p].c_str(), surfaceName ) )
				return (int)g;
		}
	}

	return -1;
}

static int Phys_EditFail( char *out, int outSize, const char *msg )
{
	Q_strncpyz( out, msg, outSize );
	return -1;
}

static void Phys_EditAppend( char *out, int outSize, const std::string &text )
{
	Q_strncpyz( out, text.c_str(), outSize );
}

//
// Bone markers: the editor shows the bones of the preview model.
//

#define PHYS_MAX_MARK_BONES	128

static bool			s_markAll = false;
static int			s_markSel = -1;
static polyVert_t	s_markVerts[PHYS_MAX_MARK_BONES * 24];
static srfPoly_t	s_markPolys[PHYS_MAX_MARK_BONES * 6];

// One dot at each bone, a square that faces the view, in the model space of the entity.
// Only the scene of the menu shows them.
void G2Phys_AddBoneMarkers( const trRefEntity_t *ent, CBoneCache *bc, const model_t *glm )
{
	if ( ( !s_markAll && s_markSel < 0 ) || !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) || !bc || !s_editHandle )
		return;

	if ( R_GetModelByHandle( s_editHandle ) != glm )
		return;

	const mdxaHeader_t *aHeader = G2Phys_BoneHeader( bc );
	int numBones = aHeader->numBones;

	if ( numBones > PHYS_MAX_MARK_BONES )
		numBones = PHYS_MAX_MARK_BONES;

	// The left and up directions of the view, in the model space of the entity.
	float left[3], up[3];

	for ( int k = 0; k < 3; k++ )
	{
		const float len2 = DotProduct( ent->e.axis[k], ent->e.axis[k] );

		left[k] = len2 > 1e-6f ? DotProduct( tr.refdef.viewaxis[1], ent->e.axis[k] ) / len2 : 0.0f;
		up[k] = len2 > 1e-6f ? DotProduct( tr.refdef.viewaxis[2], ent->e.axis[k] ) / len2 : 0.0f;
	}

	static const float corner[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
	shader_t *shader = tr.physMarkShader;
	int poly = 0, vert = 0;

	for ( int b = 0; b < numBones; b++ )
	{
		const bool selected = ( b == s_markSel );

		if ( !selected && !s_markAll )
			continue;

		const float r = selected ? 1.6f : 0.7f;
		const byte col[4] = { selected ? (byte)255 : (byte)0, selected ? (byte)0 : (byte)255, (byte)255, (byte)255 };
		float bind[3], p[3];
		const mdxaBone_t &m = G2Phys_BoneMatrix( bc, b );

		Phys_BindOrigin( aHeader, b, bind );

		for ( int k = 0; k < 3; k++ )
			p[k] = DotProduct( m.matrix[k], bind ) + m.matrix[k][3];

		// Both windings, so that the face is never culled.
		for ( int side = 0; side < 2; side++ )
		{
			polyVert_t *v = &s_markVerts[vert];

			for ( int i = 0; i < 4; i++ )
			{
				const int ci = side ? 3 - i : i;

				for ( int k = 0; k < 3; k++ )
					v[i].xyz[k] = p[k] + ( left[k] * corner[ci][0] + up[k] * corner[ci][1] ) * r;

				v[i].st[0] = v[i].st[1] = 0.0f;
				Com_Memcpy( v[i].modulate, col, 4 );
			}

			s_markPolys[poly].surfaceType = SF_POLY;
			s_markPolys[poly].hShader = 0;
			s_markPolys[poly].fogIndex = 0;
			s_markPolys[poly].numVerts = 4;
			s_markPolys[poly].verts = v;
			R_AddDrawSurf( (surfaceType_t *)&s_markPolys[poly], shader, 0, qfalse );

			poly++;
			vert += 4;
		}
	}
}

/*
==============
G2Phys_UICommand

Entry of the editor menu. Returns 0 on success, -1 on error. The result or the error message goes to out.
==============
*/
int G2Phys_UICommand( const char *cmd, const char *arg, char *out, int outSize )
{
	if ( !out || outSize <= 0 )
		return -1;

	out[0] = '\0';

	if ( !cmd )
		return -1;

	if ( !arg )
		arg = "";

	if ( !Q_stricmp( cmd, "bonemarks" ) )
	{
		s_markAll = atoi( arg ) != 0;
		return 0;
	}

	if ( !Q_stricmp( cmd, "bonesel" ) )
	{
		s_markSel = atoi( arg );
		return 0;
	}

	if ( !Q_stricmp( cmd, "highlight" ) )
	{
		s_highlight = arg;
		return 0;
	}

	if ( !Q_stricmp( cmd, "model" ) )
	{
		const qhandle_t handle = RE_RegisterModel( arg );
		const model_t *glm = handle ? R_GetModelByHandle( handle ) : NULL;

		if ( !glm || !Phys_Header( glm ) )
			return Phys_EditFail( out, outSize, "The model is not a Ghoul2 model" );

		s_editHandle = handle;
		s_editKey = Phys_LowerKey( glm->name );
		Phys_EditDef();
		return 0;
	}

	const model_t *glm = s_editHandle ? R_GetModelByHandle( s_editHandle ) : NULL;

	if ( !glm || !Phys_Header( glm ) )
		return Phys_EditFail( out, outSize, "No model selected" );

	const mdxmHeader_t *header = Phys_Header( glm );
	physDef_t *def = Phys_EditDef();
	const int index = atoi( arg );

	if ( !Q_stricmp( cmd, "surfaces" ) )
	{
		std::string text;
		char line[128];

		for ( int s = 0; s < header->numSurfaces; s++ )
		{
			const mdxmSurfHierarchy_t *info = Phys_SurfInfo( header, s );
			const mdxmSurface_t *surf = (const mdxmSurface_t *)G2_FindSurface( glm, s, 0 );

			// Only the real surfaces are listed. A tag, a cap or a helper surface has a few vertices.
			if ( !Phys_RealSurface( surf ) )
				continue;

			Com_sprintf( line, sizeof( line ), "%s\t%d\n", info->name, Phys_EditGroupOfSurface( def, info->name ) );
			text += line;
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "bones" ) )
	{
		const mdxaHeader_t *aHeader = Phys_AnimHeader( header );
		std::string text;
		char line[128];

		if ( !aHeader )
			return Phys_EditFail( out, outSize, "The skeleton is not loaded" );

		for ( int b = 0; b < aHeader->numBones; b++ )
		{
			int depth = 0;

			for ( int p = Phys_Skel( aHeader, b )->parent; p >= 0 && depth < 32; p = Phys_Skel( aHeader, p )->parent )
				depth++;

			Com_sprintf( line, sizeof( line ), "%s\t%d\n", Phys_Skel( aHeader, b )->name, depth );
			text += line;
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "groups" ) )
	{
		std::string text;
		char line[64];

		for ( size_t g = 0; g < def->groups.size(); g++ )
		{
			Com_sprintf( line, sizeof( line ), "%d\t", (int)g );
			text += line;
			text += def->groups[g].name + "\t";

			for ( size_t p = 0; p < def->groups[g].surfaces.size(); p++ )
				text += ( p ? " " : "" ) + def->groups[g].surfaces[p];

			text += "\n";
		}

		Phys_EditAppend( out, outSize, text );
		return 0;
	}

	if ( !Q_stricmp( cmd, "text" ) )
	{
		Phys_EditAppend( out, outSize, Phys_DefToText( *def ) );
		return 0;
	}

	if ( !Q_stricmp( cmd, "add" ) )
	{
		physGroupDef_t g;
		const std::string name = arg;
		const bool hair = Q_stristr( arg, "hair" ) || Q_stristr( arg, "tail" ) || Q_stristr( arg, "braid" );

		if ( !name.length() )
			return Phys_EditFail( out, outSize, "No surface" );

		Phys_InitGroup( g, hair );
		g.name = name;
		g.surfaces.push_back( name );

		if ( hair )
		{
			physPinRule_t r;

			g.lag = true;
			g.maxDist = 4.0f;
			r.type = PHYS_PIN_GRADIENT;
			r.bone = "cranium";
			r.axis[0] = r.axis[1] = 0.0f;
			r.axis[2] = -1.0f;
			r.pinned = -4.0f;
			r.free = 1.0f;
			g.pins.push_back( r );
		}
		else
		{
			g.humanoid = true;
			g.proxy = 4.0f;
		}

		def->groups.push_back( g );
		s_highlight = name;
		Phys_Invalidate();
		Com_sprintf( out, outSize, "%d", (int)def->groups.size() - 1 );
		return 0;
	}

	if ( !Q_stricmp( cmd, "remove" ) )
	{
		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		def->groups.erase( def->groups.begin() + index );
		Phys_Invalidate();
		return 0;
	}

	if ( !Q_stricmp( cmd, "get" ) )
	{
		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		Phys_EditAppend( out, outSize, Phys_GroupToText( def->groups[index] ) );
		return 0;
	}

	if ( !Q_stricmp( cmd, "set" ) )
	{
		// The group number is the first word of the argument, the text follows after a line break.
		const char *text = strchr( arg, '\n' );

		if ( !text )
			return Phys_EditFail( out, outSize, "No text" );

		if ( index < 0 || index >= (int)def->groups.size() )
			return Phys_EditFail( out, outSize, "No such group" );

		physGroupDef_t g = def->groups[index];

		s_editMessage.clear();

		if ( !Phys_ParseGroupText( text + 1, g, Phys_EditPrint ) )
		{
			Phys_EditAppend( out, outSize, s_editMessage.empty() ? std::string( "The group has no surface" ) : s_editMessage );
			return -1;
		}

		def->groups[index] = g;
		Phys_Invalidate();
		return 0;
	}

	if ( !Q_stricmp( cmd, "save" ) )
	{
		const std::string text = Phys_DefToText( *def );
		const std::string path = Phys_FilePath( s_editKey );
		const std::string body = text.empty() ? std::string( "// no group\n" ) : text;

		ri.FS_WriteFile( path.c_str(), body.c_str(), (int)body.size() );
		Q_strncpyz( out, path.c_str(), outSize );
		return 0;
	}

	if ( !Q_stricmp( cmd, "revert" ) )
	{
		std::map<std::string, physDef_t *>::iterator it = s_defs.find( s_editKey );

		if ( it != s_defs.end() )
		{
			delete it->second;
			s_defs.erase( it );
		}

		Phys_Invalidate();
		Phys_EditDef();
		return 0;
	}

	return Phys_EditFail( out, outSize, "Unknown command" );
}
