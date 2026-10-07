/*
===========================================================================
Cloth and hair physics: .phys parser and position based solver.

This module does not depend on any renderer. The renderer builds a
topology from the mesh surfaces named by a .phys group, then calls the
solver once per frame with the skinned (animated) vertex positions as
targets.
===========================================================================
*/

#ifndef PHYS_CLOTH_H
#define PHYS_CLOTH_H

#include <string>
#include <utility>
#include <vector>

//
// .phys definition
//

enum physPinType_t
{
	PHYS_PIN_GRADIENT,	// distance from a bone along an axis, in bind pose model space
	PHYS_PIN_BONES,		// weight of the vertex on the listed bones
	PHYS_PIN_UV,		// texture coordinate v
	PHYS_PIN_RADIUS		// distance from a bone origin, with an offset
};

struct physPinRule_t
{
	physPinType_t				type;
	std::string					bone;		// PHYS_PIN_GRADIENT: reference bone
	float						axis[3];	// PHYS_PIN_GRADIENT: unit axis. PHYS_PIN_RADIUS: offset from the bone
	float						pinned;		// distance or v value where the vertex is fixed
	float						free;		// distance or v value where the vertex is free
	std::vector<std::string>	bones;		// PHYS_PIN_BONES
};

struct physCollider_t
{
	bool		capsule;	// false: sphere on boneA
	std::string	boneA;
	std::string	boneB;
	float		offA[3];	// offsets in bind pose model space
	float		offB[3];
	float		radius;
};

struct physGroupDef_t
{
	std::string					name;
	bool						hair;
	bool						enabled;
	std::vector<std::string>	surfaces;	// names or wildcard patterns
	std::vector<physPinRule_t>	pins;
	std::vector<physCollider_t>	colliders;		// explicit colliders
	bool						humanoid;		// adds the colliders of the stock humanoid skeleton
	float						humanoidScale;	// multiplier of the radius of those colliders
	bool						modelFit;		// adds capsules fitted to the body surfaces of the model
	float						modelFitScale;	// multiplier of the fitted radius
	std::vector<std::pair<std::string, float> >	radiusOverride;	// bone name, radius
	std::vector<std::string>	removed;		// bone names: the collider that starts on the bone is removed

	bool	lag;		// mode lag: one soft transform for the whole group, no per vertex solver
	float	proxy;		// cell size of the simulation mesh, in units
	float	maxDist;	// backstop radius of a free vertex, in the surface plane
	float	maxDistN;	// backstop radius along the surface normal, 0 = same as maxDist
	float	maxSpeed;	// largest speed relative to the animation, units per second
	float	gravity;	// multiplier of 800 units per second squared
	float	damping;	// loss of the speed relative to the animation, 1 per second
	float	drag;		// pull of the speed toward the wind, 1 per second
	float	aero;		// push of the relative air flow on a triangle, 1 per second
	float	wind;		// multiplier of the map wind
	float	stretch;	// edge stiffness, 0 to 1
	float	bend;		// bend stiffness, 0 to 1
	float	follow;		// natural frequency of the soft link to the animated pose, radians per second, 0 = none
	float	inertia;	// part of the motion of the pinned particles that the free particles do not follow, 0 to 1
	float	lagFreq;	// mode lag: natural frequency, radians per second
	float	lagDamp;	// mode lag: damping ratio, 1 = critical
	float	margin;		// extra distance kept from the colliders
	float	range;		// maximum distance from the view that runs the solver
	int		iterations;
};

struct physDef_t
{
	std::string					name;
	std::vector<physGroupDef_t>	groups;
};

// Sets the defaults of a group.
void		Phys_InitGroup( physGroupDef_t &g, bool hair );

// Parses the lines of one group, with or without the header and the braces. Returns false on a syntax error.
bool		Phys_ParseGroupText( const char *text, physGroupDef_t &g, void ( *print )( const char *msg ) );

// The text of a group or of a file. Values equal to the default are not written.
std::string	Phys_GroupToText( const physGroupDef_t &g );
std::string	Phys_DefToText( const physDef_t &def );

// The colliders of a group: the humanoid template, with the radius overrides, and the explicit colliders.
void		Phys_ExpandColliders( const physGroupDef_t &g, std::vector<physCollider_t> &out, const std::vector<physCollider_t> *fitted = 0 );

// Returns NULL when the text has no valid group. Messages go to the print callback.
physDef_t	*Phys_ParseDef( const char *text, const char *name, void ( *print )( const char *msg ) );

bool		Phys_MatchName( const char *pattern, const char *name );

//
// Topology
//

struct physSurfaceInput_t
{
	int			numVerts;
	const float	*xyz;		// 3 floats per vertex, bind pose model space
	int			numTris;
	const int	*tris;		// 3 indexes per triangle
};

struct physTopology_t
{
	int							numParticles;
	std::vector<float>			bindPos;		// 3 per particle
	std::vector<int>			firstVertex;	// per particle: (surface << 24) | vertex of the first user
	std::vector<std::vector<int> >	vertParticle;	// [surface][vertex]
	std::vector<int>			edges;			// 2 per edge
	std::vector<float>			edgeRest;
	std::vector<int>			bends;			// 2 per bend constraint
	std::vector<float>			bendRest;
	std::vector<int>			tris;			// 3 per triangle, particle indexes
	std::vector<float>			valence;		// per particle: number of triangles
};

void	Phys_BuildTopology( physTopology_t &topo, const physSurfaceInput_t *surfaces, int numSurfaces );

// Builds edges, bend constraints and valence from bindPos and tris.
void	Phys_BuildEdges( physTopology_t &topo );

//
// Simulation mesh (the "proxy"): a coarse set of particles that carries the full mesh. The solver runs on it.
//

struct physProxy_t
{
	int					numProxy;
	physTopology_t		topo;			// proxy particles
	std::vector<int>	clusterOf;		// per full particle
	std::vector<int>	link;			// 4 per full particle: proxy index, or -1
	std::vector<float>	weight;			// 4 per full particle
	std::vector<float>	freeFactor;		// per proxy particle
	std::vector<int>	memberCount;	// per proxy particle

	physProxy_t() : numProxy( 0 ) {}
};

void	Phys_BuildProxy( physProxy_t &proxy, const physTopology_t &full, const float *freeFactor, float cell );

// Mean of the member targets of each proxy particle.
void	Phys_ProxyTargets( const physProxy_t &proxy, const float *fullTarget, float *proxyTarget );

// Full positions: the target plus the weighted displacement of the proxy particles.
void	Phys_ProxyApply( const physProxy_t &proxy, const float *proxyX, const float *proxyTarget,
						 const float *fullTarget, const float *freeFactor, float *outPos );

//
// Solver
//

struct physCollider_w
{
	float	a[3];	// world space segment
	float	b[3];
	float	radius;
};

struct physCloth_t
{
	int					numParticles;
	bool				valid;				// false until Phys_ClothReset
	std::vector<float>	x;					// 3 per particle, world space
	std::vector<float>	v;
	std::vector<float>	prevTarget;
	std::vector<float>	maxDist;			// per particle backstop, 0 = pinned
	std::vector<unsigned char> pinned;

	// Work buffers of Phys_ClothStep, kept between frames so a step does not allocate.
	std::vector<float>	p, tgt, tgtVel, acc, targetNormal, lambda, lambdaFollow;
	std::vector<unsigned char> hit;

	physCloth_t() : numParticles( 0 ), valid( false ) {}
};

// freeFactor: 0 = pinned, 1 = free, per particle.
void	Phys_ClothInit( physCloth_t &cloth, const physGroupDef_t &def, const float *freeFactor, int numParticles );

// Snap every particle to the target. Clears the velocity.
void	Phys_ClothReset( physCloth_t &cloth, const float *target );

void	Phys_ClothStep( physCloth_t &cloth, const physTopology_t &topo, const physGroupDef_t &def,
						float dt, const float *target, const float windVel[3],
						const physCollider_w *colliders, int numColliders, bool hasGround, float groundZ );

//
// Lag mode: the free particles move as one mass on a spring. It rotates around the pivot.
//

struct physLag_t
{
	bool	valid;
	float	xc[3];		// mass center, world space
	float	vc[3];
	float	ct[3];		// animated mass center
	float	vct[3];		// speed of the animated mass center
	float	pivot[3];	// mean of the fixed particles
	float	reach;		// distance of the farthest free particle from the pivot
	float	maxAngle;	// largest turn around the pivot, radians: keeps the farthest particle within maxdist

	physLag_t() : valid( false ), reach( 0.0f ), maxAngle( 3.0f ) {}
};

void	Phys_LagReset( physLag_t &lag, const float *target, const float *freeFactor, int n );
void	Phys_LagStep( physLag_t &lag, const physGroupDef_t &def, float dt, const float *target,
					  const float *freeFactor, int n, const float windVel[3] );
void	Phys_LagApply( const physLag_t &lag, const float *target, const float *freeFactor, int n, float *outPos );

// Pushes the free particles out of the colliders and above the floor. A particle never gets closer to a
// collider than its animated position (target) is, so the animation itself is not changed.
void	Phys_ProjectColliders( float *pos, const float *target, const float *freeFactor, int n, float margin,
							   const physCollider_w *colliders, int numColliders, bool hasGround, float groundZ );

// Lag mode, after Phys_LagApply: a contact moves the mass center and removes the speed into the collider,
// then the positions are rebuilt. Only what is left is pushed out of the colliders one particle at a time.
void	Phys_LagCollide( physLag_t &lag, const physGroupDef_t &def, const float *target, const float *freeFactor, int n,
						 const physCollider_w *colliders, int numColliders, bool hasGround, float groundZ, float *outPos );

// Area weighted particle normals from the given positions (3 per particle).
void	Phys_ComputeNormals( const physTopology_t &topo, const float *pos, float *normals );

#endif
