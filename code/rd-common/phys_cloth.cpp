/*
===========================================================================
Cloth and hair physics: .phys parser and position based solver.
===========================================================================
*/

#include "phys_cloth.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <utility>

#define PHYS_GRAVITY		800.0f
#define PHYS_MAX_STEP		( 1.0f / 120.0f )
#define PHYS_MAX_SUBSTEPS	4
#define PHYS_MIN_FREE		0.01f

//
// Text helpers
//

static int Phys_Stricmp( const char *a, const char *b )
{
	for ( ; *a && *b; a++, b++ )
	{
		const int ca = tolower( (unsigned char)*a );
		const int cb = tolower( (unsigned char)*b );

		if ( ca != cb )
			return ca - cb;
	}

	return tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
}

bool Phys_MatchName( const char *pattern, const char *name )
{
	while ( *pattern )
	{
		if ( *pattern == '*' )
		{
			while ( *pattern == '*' )
				pattern++;

			if ( !*pattern )
				return true;

			for ( ; *name; name++ )
			{
				if ( Phys_MatchName( pattern, name ) )
					return true;
			}

			return false;
		}

		if ( !*name )
			return false;

		if ( *pattern != '?' && tolower( (unsigned char)*pattern ) != tolower( (unsigned char)*name ) )
			return false;

		pattern++;
		name++;
	}

	return *name == '\0';
}

// Reads the next non-empty line as a list of tokens. The braces are tokens of their own.
static bool Phys_ReadLine( const char *&p, std::vector<std::string> &tok )
{
	tok.clear();

	while ( *p && tok.empty() )
	{
		while ( *p && *p != '\n' )
		{
			if ( *p == ' ' || *p == '\t' || *p == '\r' )
			{
				p++;
			}
			else if ( p[0] == '/' && p[1] == '/' )
			{
				while ( *p && *p != '\n' )
					p++;
			}
			else if ( p[0] == '/' && p[1] == '*' )
			{
				p += 2;

				while ( *p && !( p[0] == '*' && p[1] == '/' ) )
					p++;

				if ( *p )
					p += 2;
			}
			else if ( *p == '{' || *p == '}' )
			{
				tok.push_back( std::string( 1, *p ) );
				p++;
			}
			else if ( *p == '"' )
			{
				const char *start = ++p;

				while ( *p && *p != '"' && *p != '\n' )
					p++;

				tok.push_back( std::string( start, p ) );

				if ( *p == '"' )
					p++;
			}
			else
			{
				const char *start = p;

				while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '{' && *p != '}' && *p != '"' )
					p++;

				tok.push_back( std::string( start, p ) );
			}
		}

		if ( *p == '\n' )
			p++;
	}

	return !tok.empty();
}

//
// Parser
//

void Phys_InitGroup( physGroupDef_t &g, bool hair )
{
	g.hair = hair;
	g.humanoid = false;
	g.modelFit = false;
	g.modelFitScale = 1.0f;
	g.humanoidScale = 1.0f;
	g.colliders.clear();
	g.radiusOverride.clear();
	g.removed.clear();
	g.pins.clear();
	g.surfaces.clear();
	g.enabled = true;
	g.lag = false;
	g.proxy = 4.0f;

	g.maxDist = hair ? 8.0f : 24.0f;
	g.maxDistN = 0.0f;
	g.maxSpeed = hair ? 400.0f : 700.0f;
	g.gravity = hair ? 0.3f : 0.5f;
	g.damping = hair ? 3.0f : 2.0f;
	g.drag = hair ? 0.5f : 0.25f;
	g.aero = hair ? 0.0f : 1.0f;
	g.wind = hair ? 0.3f : 0.5f;
	g.stretch = hair ? 1.0f : 0.9f;
	g.bend = hair ? 0.3f : 0.15f;
	g.follow = hair ? 14.0f : 6.0f;
	g.inertia = hair ? 0.5f : 0.7f;
	g.lagFreq = 10.0f;
	g.lagDamp = 0.5f;
	g.margin = 0.5f;
	g.range = 0.0f;
	g.iterations = 4;
}

static void Phys_AddSphere( physGroupDef_t &g, const char *bone, float radius, float ox, float oy, float oz )
{
	physCollider_t c;

	c.capsule = false;
	c.boneA = bone;
	c.boneB = bone;
	c.offA[0] = c.offB[0] = ox;
	c.offA[1] = c.offB[1] = oy;
	c.offA[2] = c.offB[2] = oz;
	c.radius = radius;
	g.colliders.push_back( c );
}

static void Phys_AddCapsule( physGroupDef_t &g, const char *boneA, const char *boneB, float radius )
{
	physCollider_t c;

	c.capsule = true;
	c.boneA = boneA;
	c.boneB = boneB;
	c.offA[0] = c.offA[1] = c.offA[2] = 0.0f;
	c.offB[0] = c.offB[1] = c.offB[2] = 0.0f;
	c.radius = radius;
	g.colliders.push_back( c );
}

// Colliders for the bones of the stock _humanoid skeleton.
static void Phys_AddHumanoidColliders( std::vector<physCollider_t> &out, float scale )
{
	physGroupDef_t tmp;

	Phys_AddCapsule( tmp, "pelvis", "lower_lumbar", 7.0f );
	Phys_AddCapsule( tmp, "lower_lumbar", "cervical", 6.5f );
	Phys_AddSphere( tmp, "cranium", 7.5f, 0.0f, 0.0f, 3.5f );

	Phys_AddCapsule( tmp, "lhumerus", "lradius", 3.2f );
	Phys_AddCapsule( tmp, "lradius", "lhand", 2.6f );
	Phys_AddCapsule( tmp, "rhumerus", "rradius", 3.2f );
	Phys_AddCapsule( tmp, "rradius", "rhand", 2.6f );

	Phys_AddCapsule( tmp, "lfemurYZ", "ltibia", 5.2f );
	Phys_AddCapsule( tmp, "ltibia", "ltalus", 3.6f );
	Phys_AddCapsule( tmp, "rfemurYZ", "rtibia", 5.2f );
	Phys_AddCapsule( tmp, "rtibia", "rtalus", 3.6f );

	for ( size_t i = 0; i < tmp.colliders.size(); i++ )
	{
		tmp.colliders[i].radius *= scale;
		out.push_back( tmp.colliders[i] );
	}
}

void Phys_ExpandColliders( const physGroupDef_t &g, std::vector<physCollider_t> &out, const std::vector<physCollider_t> *fitted )
{
	out.clear();

	if ( g.modelFit && fitted )
	{
		for ( size_t i = 0; i < fitted->size(); i++ )
			out.push_back( ( *fitted )[i] );
	}

	if ( g.humanoid )
		Phys_AddHumanoidColliders( out, g.humanoidScale );

	for ( size_t i = 0; i < g.colliders.size(); i++ )
		out.push_back( g.colliders[i] );

	for ( size_t i = 0; i < g.radiusOverride.size(); i++ )
	{
		for ( size_t c = 0; c < out.size(); c++ )
		{
			if ( !Phys_Stricmp( out[c].boneA.c_str(), g.radiusOverride[i].first.c_str() ) )
				out[c].radius = g.radiusOverride[i].second;
		}
	}

	for ( size_t i = 0; i < g.removed.size(); i++ )
	{
		for ( size_t c = 0; c < out.size(); )
		{
			if ( !Phys_Stricmp( out[c].boneA.c_str(), g.removed[i].c_str() ) )
				out.erase( out.begin() + c );
			else
				c++;
		}
	}
}

static bool Phys_ParseAxis( const char *s, float axis[3] )
{
	float sign = 1.0f;

	axis[0] = axis[1] = axis[2] = 0.0f;

	if ( *s == '-' )
	{
		sign = -1.0f;
		s++;
	}
	else if ( *s == '+' )
	{
		s++;
	}

	if ( s[1] )
		return false;

	switch ( tolower( (unsigned char)*s ) )
	{
	case 'x':	axis[0] = sign;	return true;
	case 'y':	axis[1] = sign;	return true;
	case 'z':	axis[2] = sign;	return true;
	}

	return false;
}

static float Phys_Arg( const std::vector<std::string> &tok, size_t i, float fallback )
{
	return i < tok.size() ? (float)atof( tok[i].c_str() ) : fallback;
}

static void Phys_ParseLine( physGroupDef_t &g, const std::vector<std::string> &tok, void ( *print )( const char *msg ) )
{
	const char *key = tok[0].c_str();
	char msg[256];

	if ( !Phys_Stricmp( key, "surfaces" ) )
	{
		for ( size_t i = 1; i < tok.size(); i++ )
			g.surfaces.push_back( tok[i] );
	}
	else if ( !Phys_Stricmp( key, "pin" ) && tok.size() >= 2 )
	{
		physPinRule_t r;
		bool ok = false;

		r.axis[0] = r.axis[1] = r.axis[2] = 0.0f;
		r.pinned = r.free = 0.0f;

		if ( !Phys_Stricmp( tok[1].c_str(), "gradient" ) && tok.size() >= 6 )
		{
			r.type = PHYS_PIN_GRADIENT;
			r.bone = tok[2];
			r.pinned = Phys_Arg( tok, 4, 0.0f );
			r.free = Phys_Arg( tok, 5, 1.0f );
			ok = Phys_ParseAxis( tok[3].c_str(), r.axis ) && r.pinned != r.free;
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "bones" ) && tok.size() >= 3 )
		{
			r.type = PHYS_PIN_BONES;

			for ( size_t i = 2; i < tok.size(); i++ )
				r.bones.push_back( tok[i] );

			ok = true;
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "radius" ) && tok.size() >= 5 )
		{
			r.type = PHYS_PIN_RADIUS;
			r.bone = tok[2];
			r.pinned = Phys_Arg( tok, 3, 0.0f );
			r.free = Phys_Arg( tok, 4, 1.0f );
			r.axis[0] = Phys_Arg( tok, 5, 0.0f );
			r.axis[1] = Phys_Arg( tok, 6, 0.0f );
			r.axis[2] = Phys_Arg( tok, 7, 0.0f );
			ok = r.pinned != r.free;
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "uv" ) && tok.size() >= 4 )
		{
			r.type = PHYS_PIN_UV;
			r.pinned = Phys_Arg( tok, 2, 0.0f );
			r.free = Phys_Arg( tok, 3, 1.0f );
			ok = r.pinned != r.free;
		}

		if ( ok )
			g.pins.push_back( r );
		else if ( print )
			print( "bad pin line\n" );
	}
	else if ( !Phys_Stricmp( key, "collide" ) && tok.size() >= 2 )
	{
		if ( !Phys_Stricmp( tok[1].c_str(), "humanoid" ) )
		{
			g.humanoid = true;
			g.humanoidScale = Phys_Arg( tok, 2, 1.0f );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "model" ) )
		{
			g.modelFit = true;
			g.modelFitScale = Phys_Arg( tok, 2, 1.0f );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "radius" ) && tok.size() >= 4 )
		{
			g.radiusOverride.push_back( std::make_pair( tok[2], Phys_Arg( tok, 3, 1.0f ) ) );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "remove" ) && tok.size() >= 3 )
		{
			g.removed.push_back( tok[2] );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "capsule" ) && tok.size() >= 5 )
		{
			Phys_AddCapsule( g, tok[2].c_str(), tok[3].c_str(), Phys_Arg( tok, 4, 1.0f ) );
		}
		else if ( !Phys_Stricmp( tok[1].c_str(), "sphere" ) && tok.size() >= 4 )
		{
			Phys_AddSphere( g, tok[2].c_str(), Phys_Arg( tok, 3, 1.0f ), Phys_Arg( tok, 4, 0.0f ), Phys_Arg( tok, 5, 0.0f ), Phys_Arg( tok, 6, 0.0f ) );
		}
		else if ( tok.size() == 4 && ( isdigit( (unsigned char)tok[3][0] ) || tok[3][0] == '.' || tok[3][0] == '-' ) )
		{
			// Short form of a capsule: collide <boneA> <boneB> <radius>
			Phys_AddCapsule( g, tok[1].c_str(), tok[2].c_str(), Phys_Arg( tok, 3, 1.0f ) );
		}
		else if ( print )
		{
			print( "bad collide line\n" );
		}
	}
	else if ( !Phys_Stricmp( key, "maxdist" ) )
	{
		g.maxDist = Phys_Arg( tok, 1, g.maxDist );
		g.maxDistN = Phys_Arg( tok, 2, 0.0f );
	}
	else if ( !Phys_Stricmp( key, "mode" ) && tok.size() >= 2 )
	{
		if ( !Phys_Stricmp( tok[1].c_str(), "lag" ) )
			g.lag = true;
		else if ( !Phys_Stricmp( tok[1].c_str(), "cloth" ) )
			g.lag = false;
		else if ( print )
			print( "mode is 'cloth' or 'lag'\n" );
	}
	else if ( !Phys_Stricmp( key, "maxspeed" ) )	g.maxSpeed = Phys_Arg( tok, 1, g.maxSpeed );
	else if ( !Phys_Stricmp( key, "aero" ) )		g.aero = Phys_Arg( tok, 1, g.aero );
	else if ( !Phys_Stricmp( key, "proxy" ) )		g.proxy = Phys_Arg( tok, 1, g.proxy );
	else if ( !Phys_Stricmp( key, "lagfreq" ) )		g.lagFreq = Phys_Arg( tok, 1, g.lagFreq );
	else if ( !Phys_Stricmp( key, "lagdamp" ) )		g.lagDamp = Phys_Arg( tok, 1, g.lagDamp );
	else if ( !Phys_Stricmp( key, "gravity" ) )		g.gravity = Phys_Arg( tok, 1, g.gravity );
	else if ( !Phys_Stricmp( key, "damping" ) )		g.damping = Phys_Arg( tok, 1, g.damping );
	else if ( !Phys_Stricmp( key, "drag" ) )		g.drag = Phys_Arg( tok, 1, g.drag );
	else if ( !Phys_Stricmp( key, "wind" ) )		g.wind = Phys_Arg( tok, 1, g.wind );
	else if ( !Phys_Stricmp( key, "stretch" ) )		g.stretch = Phys_Arg( tok, 1, g.stretch );
	else if ( !Phys_Stricmp( key, "bend" ) )		g.bend = Phys_Arg( tok, 1, g.bend );
	else if ( !Phys_Stricmp( key, "follow" ) )		g.follow = Phys_Arg( tok, 1, g.follow );
	else if ( !Phys_Stricmp( key, "inertia" ) )		g.inertia = Phys_Arg( tok, 1, g.inertia );
	else if ( !Phys_Stricmp( key, "margin" ) )		g.margin = Phys_Arg( tok, 1, g.margin );
	else if ( !Phys_Stricmp( key, "range" ) )		g.range = Phys_Arg( tok, 1, g.range );
	else if ( !Phys_Stricmp( key, "iterations" ) )	g.iterations = (int)Phys_Arg( tok, 1, (float)g.iterations );
	else if ( !Phys_Stricmp( key, "enabled" ) )		g.enabled = Phys_Arg( tok, 1, 1.0f ) != 0.0f;
	else if ( print )
	{
		snprintf( msg, sizeof( msg ), "unknown keyword '%s'\n", key );
		print( msg );
	}

	if ( g.iterations < 1 )
		g.iterations = 1;
	else if ( g.iterations > 16 )
		g.iterations = 16;

	if ( g.inertia < 0.0f )
		g.inertia = 0.0f;
	else if ( g.inertia > 1.0f )
		g.inertia = 1.0f;
}

physDef_t *Phys_ParseDef( const char *text, const char *name, void ( *print )( const char *msg ) )
{
	physDef_t *def = new physDef_t;
	std::vector<std::string> tok;
	const char *p = text;

	def->name = name;

	while ( Phys_ReadLine( p, tok ) )
	{
		bool hair;

		if ( !Phys_Stricmp( tok[0].c_str(), "cloth" ) )
		{
			hair = false;
		}
		else if ( !Phys_Stricmp( tok[0].c_str(), "hair" ) )
		{
			hair = true;
		}
		else
		{
			if ( print )
				print( "expected 'cloth' or 'hair'\n" );

			continue;
		}

		physGroupDef_t g;
		Phys_InitGroup( g, hair );

		if ( tok.size() > 1 && tok[1] != "{" )
			g.name = tok[1];

		if ( tok.back() != "{" )
		{
			if ( !Phys_ReadLine( p, tok ) || tok[0] != "{" )
			{
				if ( print )
					print( "expected '{'\n" );

				break;
			}
		}

		bool closed = false;

		while ( Phys_ReadLine( p, tok ) )
		{
			if ( tok[0] == "}" )
			{
				closed = true;
				break;
			}

			Phys_ParseLine( g, tok, print );
		}

		if ( !closed && print )
			print( "missing '}'\n" );

		if ( g.surfaces.empty() )
		{
			if ( print )
				print( "group has no surfaces\n" );

			continue;
		}

		def->groups.push_back( g );
	}

	if ( def->groups.empty() )
	{
		delete def;
		return NULL;
	}

	return def;
}

//
// Vector helpers
//

static inline void Vec3Sub( const float *a, const float *b, float *out )
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static inline float Vec3Dot( const float *a, const float *b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline float Vec3Dist( const float *a, const float *b )
{
	float d[3];

	Vec3Sub( a, b, d );
	return sqrtf( Vec3Dot( d, d ) );
}

//
// Topology
//

struct physQuantKey_t
{
	int x, y, z;

	bool operator<( const physQuantKey_t &o ) const
	{
		if ( x != o.x )	return x < o.x;
		if ( y != o.y )	return y < o.y;
		return z < o.z;
	}
};

static inline physQuantKey_t Phys_Quantize( const float *p, float scale )
{
	physQuantKey_t key;

	key.x = (int)floorf( p[0] * scale + 0.5f );
	key.y = (int)floorf( p[1] * scale + 0.5f );
	key.z = (int)floorf( p[2] * scale + 0.5f );
	return key;
}

void Phys_BuildEdges( physTopology_t &topo )
{
	std::map<std::pair<int, int>, int> edgeIndex;
	std::map<std::pair<int, int>, int> edgeOpposite;	// first opposite vertex seen on an edge
	const int numTris = (int)topo.tris.size() / 3;

	topo.edges.clear();
	topo.edgeRest.clear();
	topo.bends.clear();
	topo.bendRest.clear();
	topo.valence.assign( topo.numParticles, 0.0f );

	for ( int t = 0; t < numTris; t++ )
	{
		const int tri[3] = { topo.tris[t * 3], topo.tris[t * 3 + 1], topo.tris[t * 3 + 2] };

		for ( int k = 0; k < 3; k++ )
		{
			const int a = tri[k];
			const int b = tri[( k + 1 ) % 3];
			const int opposite = tri[( k + 2 ) % 3];
			const std::pair<int, int> key = a < b ? std::make_pair( a, b ) : std::make_pair( b, a );

			topo.valence[a] += 1.0f;

			if ( edgeIndex.find( key ) == edgeIndex.end() )
			{
				edgeIndex[key] = (int)topo.edges.size() / 2;
				topo.edges.push_back( key.first );
				topo.edges.push_back( key.second );
				topo.edgeRest.push_back( Vec3Dist( &topo.bindPos[key.first * 3], &topo.bindPos[key.second * 3] ) );
			}

			std::map<std::pair<int, int>, int>::iterator it = edgeOpposite.find( key );

			if ( it == edgeOpposite.end() )
			{
				edgeOpposite[key] = opposite;
			}
			else if ( it->second != opposite )
			{
				const int c = it->second;
				const int d = opposite;

				topo.bends.push_back( c );
				topo.bends.push_back( d );
				topo.bendRest.push_back( Vec3Dist( &topo.bindPos[c * 3], &topo.bindPos[d * 3] ) );
			}
		}
	}
}

void Phys_BuildTopology( physTopology_t &topo, const physSurfaceInput_t *surfaces, int numSurfaces )
{
	std::map<physQuantKey_t, int> weld;

	topo = physTopology_t();
	topo.vertParticle.resize( numSurfaces );

	// Vertices at the same bind position are one particle, also across surfaces.
	for ( int s = 0; s < numSurfaces; s++ )
	{
		const physSurfaceInput_t &in = surfaces[s];

		topo.vertParticle[s].resize( in.numVerts );

		for ( int i = 0; i < in.numVerts; i++ )
		{
			const float *p = in.xyz + i * 3;
			const physQuantKey_t key = Phys_Quantize( p, 100.0f );
			std::map<physQuantKey_t, int>::iterator it = weld.find( key );

			if ( it == weld.end() )
			{
				const int index = topo.numParticles++;

				weld[key] = index;
				topo.bindPos.push_back( p[0] );
				topo.bindPos.push_back( p[1] );
				topo.bindPos.push_back( p[2] );
				topo.firstVertex.push_back( ( s << 24 ) | i );
				topo.vertParticle[s][i] = index;
			}
			else
			{
				topo.vertParticle[s][i] = it->second;
			}
		}
	}

	for ( int s = 0; s < numSurfaces; s++ )
	{
		const physSurfaceInput_t &in = surfaces[s];

		for ( int t = 0; t < in.numTris; t++ )
		{
			int tri[3];

			for ( int k = 0; k < 3; k++ )
			{
				const int vert = in.tris[t * 3 + k];

				tri[k] = ( vert >= 0 && vert < in.numVerts ) ? topo.vertParticle[s][vert] : -1;
			}

			if ( tri[0] < 0 || tri[1] < 0 || tri[2] < 0 || tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2] )
				continue;

			topo.tris.push_back( tri[0] );
			topo.tris.push_back( tri[1] );
			topo.tris.push_back( tri[2] );
		}
	}

	Phys_BuildEdges( topo );
}

//
// Proxy mesh
//

#define PHYS_PROXY_LINKS	4

void Phys_BuildProxy( physProxy_t &px, const physTopology_t &full, const float *freeFactor, float cell )
{
	const int n = full.numParticles;
	std::map<physQuantKey_t, int> cells;
	std::vector<physQuantKey_t> keys;
	std::vector<float> sum;
	std::vector<float> freeSum;

	px = physProxy_t();

	if ( cell < 0.5f )
		cell = 0.5f;

	px.clusterOf.assign( n, 0 );

	for ( int i = 0; i < n; i++ )
	{
		const physQuantKey_t key = Phys_Quantize( &full.bindPos[i * 3], 1.0f / cell );
		std::map<physQuantKey_t, int>::iterator it = cells.find( key );
		int c;

		if ( it == cells.end() )
		{
			c = (int)keys.size();
			cells[key] = c;
			keys.push_back( key );
			sum.resize( sum.size() + 3, 0.0f );
			freeSum.push_back( 0.0f );
			px.memberCount.push_back( 0 );
		}
		else
		{
			c = it->second;
		}

		px.clusterOf[i] = c;
		px.memberCount[c]++;
		freeSum[c] += freeFactor[i];

		for ( int k = 0; k < 3; k++ )
			sum[c * 3 + k] += full.bindPos[i * 3 + k];
	}

	px.numProxy = (int)keys.size();
	px.topo.numParticles = px.numProxy;
	px.topo.bindPos.resize( px.numProxy * 3 );
	px.freeFactor.resize( px.numProxy );

	for ( int c = 0; c < px.numProxy; c++ )
	{
		const float inv = 1.0f / (float)px.memberCount[c];

		for ( int k = 0; k < 3; k++ )
			px.topo.bindPos[c * 3 + k] = sum[c * 3 + k] * inv;

		px.freeFactor[c] = freeSum[c] * inv;
	}

	// Each full particle follows its nearest proxy particles.
	px.link.assign( n * PHYS_PROXY_LINKS, -1 );
	px.weight.assign( n * PHYS_PROXY_LINKS, 0.0f );

	for ( int i = 0; i < n; i++ )
	{
		const physQuantKey_t home = keys[px.clusterOf[i]];
		int best[PHYS_PROXY_LINKS];
		float bestDist[PHYS_PROXY_LINKS];

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
		{
			best[k] = -1;
			bestDist[k] = 1e30f;
		}

		for ( int dx = -1; dx <= 1; dx++ )
		{
			for ( int dy = -1; dy <= 1; dy++ )
			{
				for ( int dz = -1; dz <= 1; dz++ )
				{
					physQuantKey_t key = home;

					key.x += dx;
					key.y += dy;
					key.z += dz;

					std::map<physQuantKey_t, int>::iterator it = cells.find( key );

					if ( it == cells.end() )
						continue;

					float dist = Vec3Dist( &full.bindPos[i * 3], &px.topo.bindPos[it->second * 3] );

					int c = it->second;

					for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
					{
						if ( dist < bestDist[k] )
						{
							std::swap( dist, bestDist[k] );
							std::swap( c, best[k] );
						}
					}
				}
			}
		}

		float total = 0.0f;

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
		{
			if ( best[k] < 0 )
				continue;

			const float w = 1.0f / ( bestDist[k] * bestDist[k] + 1.0f );

			px.link[i * PHYS_PROXY_LINKS + k] = best[k];
			px.weight[i * PHYS_PROXY_LINKS + k] = w;
			total += w;
		}

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
			px.weight[i * PHYS_PROXY_LINKS + k] /= total;
	}

	// Triangles of the proxy mesh: the triangles of the full mesh with three different clusters.
	std::map<physQuantKey_t, int> seen;
	const int numTris = (int)full.tris.size() / 3;

	for ( int t = 0; t < numTris; t++ )
	{
		int c[3] = { px.clusterOf[full.tris[t * 3]], px.clusterOf[full.tris[t * 3 + 1]], px.clusterOf[full.tris[t * 3 + 2]] };

		if ( c[0] == c[1] || c[1] == c[2] || c[0] == c[2] )
			continue;

		int sorted[3] = { c[0], c[1], c[2] };

		if ( sorted[0] > sorted[1] )	std::swap( sorted[0], sorted[1] );
		if ( sorted[1] > sorted[2] )	std::swap( sorted[1], sorted[2] );
		if ( sorted[0] > sorted[1] )	std::swap( sorted[0], sorted[1] );

		physQuantKey_t key;

		key.x = sorted[0];
		key.y = sorted[1];
		key.z = sorted[2];

		if ( seen.find( key ) != seen.end() )
			continue;

		seen[key] = 1;
		px.topo.tris.push_back( c[0] );
		px.topo.tris.push_back( c[1] );
		px.topo.tris.push_back( c[2] );
	}

	Phys_BuildEdges( px.topo );
}

void Phys_ProxyTargets( const physProxy_t &px, const float *fullTarget, float *proxyTarget )
{
	const int n = (int)px.clusterOf.size();

	memset( proxyTarget, 0, sizeof( float ) * 3 * px.numProxy );

	for ( int i = 0; i < n; i++ )
	{
		const int c = px.clusterOf[i];

		proxyTarget[c * 3 + 0] += fullTarget[i * 3 + 0];
		proxyTarget[c * 3 + 1] += fullTarget[i * 3 + 1];
		proxyTarget[c * 3 + 2] += fullTarget[i * 3 + 2];
	}

	for ( int c = 0; c < px.numProxy; c++ )
	{
		const float inv = 1.0f / (float)px.memberCount[c];

		proxyTarget[c * 3 + 0] *= inv;
		proxyTarget[c * 3 + 1] *= inv;
		proxyTarget[c * 3 + 2] *= inv;
	}
}

void Phys_ProxyApply( const physProxy_t &px, const float *proxyX, const float *proxyTarget,
					  const float *fullTarget, const float *freeFactor, float *outPos )
{
	const int n = (int)px.clusterOf.size();

	for ( int i = 0; i < n; i++ )
	{
		float d[3] = { 0.0f, 0.0f, 0.0f };

		for ( int k = 0; k < PHYS_PROXY_LINKS; k++ )
		{
			const int c = px.link[i * PHYS_PROXY_LINKS + k];

			if ( c < 0 )
				continue;

			const float w = px.weight[i * PHYS_PROXY_LINKS + k];

			d[0] += w * ( proxyX[c * 3 + 0] - proxyTarget[c * 3 + 0] );
			d[1] += w * ( proxyX[c * 3 + 1] - proxyTarget[c * 3 + 1] );
			d[2] += w * ( proxyX[c * 3 + 2] - proxyTarget[c * 3 + 2] );
		}

		// A fixed vertex stays on the animation.
		float f = freeFactor[i] * 3.0f;

		if ( f > 1.0f )
			f = 1.0f;

		outPos[i * 3 + 0] = fullTarget[i * 3 + 0] + d[0] * f;
		outPos[i * 3 + 1] = fullTarget[i * 3 + 1] + d[1] * f;
		outPos[i * 3 + 2] = fullTarget[i * 3 + 2] + d[2] * f;
	}
}

//
// Solver
//
// Extended position based dynamics on the simulation mesh. A constraint has a compliance, so the
// stiffness does not depend on the number of iterations or on the frame rate.
//

// Compliance of a constraint with stiffness 1 / 1000 (soft). A stiffness of 1 is rigid.
#define PHYS_STRETCH_COMPLIANCE	2e-5f
#define PHYS_BEND_COMPLIANCE	2e-4f
#define PHYS_HIT_DAMPING		0.8f	// speed kept by a particle that touched a collider

static inline float Phys_Compliance( float stiffness, float scale )
{
	if ( stiffness >= 1.0f )
		return 0.0f;

	if ( stiffness < 0.001f )
		stiffness = 0.001f;

	return ( 1.0f / stiffness - 1.0f ) * scale;
}

void Phys_ClothInit( physCloth_t &cloth, const physGroupDef_t &def, const float *freeFactor, int numParticles )
{
	cloth.numParticles = numParticles;
	cloth.valid = false;
	cloth.x.assign( numParticles * 3, 0.0f );
	cloth.v.assign( numParticles * 3, 0.0f );
	cloth.prevTarget.assign( numParticles * 3, 0.0f );
	cloth.maxDist.assign( numParticles, 0.0f );
	cloth.pinned.assign( numParticles, 1 );

	for ( int i = 0; i < numParticles; i++ )
	{
		const float f = freeFactor[i];

		if ( f > PHYS_MIN_FREE )
		{
			cloth.pinned[i] = 0;
			cloth.maxDist[i] = def.maxDist * f;
		}
	}

	cloth.p.assign( numParticles * 3, 0.0f );
	cloth.tgt.assign( numParticles * 3, 0.0f );
	cloth.tgtVel.assign( numParticles * 3, 0.0f );
	cloth.acc.clear();
	cloth.targetNormal.clear();
	cloth.lambda.clear();
	cloth.lambdaFollow.assign( numParticles, 0.0f );
	cloth.hit.assign( numParticles, 0 );
}

void Phys_ClothReset( physCloth_t &cloth, const float *target )
{
	if ( cloth.numParticles == 0 )
		return;

	memcpy( &cloth.x[0], target, sizeof( float ) * 3 * cloth.numParticles );
	memcpy( &cloth.prevTarget[0], target, sizeof( float ) * 3 * cloth.numParticles );
	memset( &cloth.v[0], 0, sizeof( float ) * 3 * cloth.numParticles );
	cloth.valid = true;
}

// XPBD distance constraint between particles a and b. A pinned particle does not move.
static inline void Phys_SolveDistance( float *p, const unsigned char *pinned, int a, int b, float rest, float alpha, float &lambda )
{
	const float wa = pinned[a] ? 0.0f : 1.0f;
	const float wb = pinned[b] ? 0.0f : 1.0f;

	if ( wa + wb == 0.0f )
		return;

	float d[3];
	Vec3Sub( p + a * 3, p + b * 3, d );

	const float len = sqrtf( Vec3Dot( d, d ) );

	if ( len < 1e-5f )
		return;

	const float dl = ( -( len - rest ) - alpha * lambda ) / ( wa + wb + alpha );
	const float k = dl / len;

	lambda += dl;

	for ( int c = 0; c < 3; c++ )
	{
		p[a * 3 + c] += d[c] * k * wa;
		p[b * 3 + c] -= d[c] * k * wb;
	}
}

// The point of the segment ab that is nearest to pos. Returns the squared distance.
static inline float Phys_NearestOnSegment( const float *pos, const float *a, const float *b, float *q )
{
	float ab[3], ap[3];

	Vec3Sub( b, a, ab );
	Vec3Sub( pos, a, ap );

	const float len2 = Vec3Dot( ab, ab );
	float t = len2 > 1e-6f ? Vec3Dot( ap, ab ) / len2 : 0.0f;

	if ( t < 0.0f )			t = 0.0f;
	else if ( t > 1.0f )	t = 1.0f;

	q[0] = a[0] + ab[0] * t;
	q[1] = a[1] + ab[1] * t;
	q[2] = a[2] + ab[2] * t;

	float d[3];
	Vec3Sub( pos, q, d );
	return Vec3Dot( d, d );
}

// Pushes pos out of one collider. The radius never exceeds the distance of the animated position (tgt) to the
// collider: the animation can be inside the collider, the simulation must not get deeper than it.
static bool Phys_CollideRelaxed( float *pos, const float *tgt, const physCollider_w &c, float margin )
{
	float q[3];
	float radius = c.radius + margin;

	if ( Phys_NearestOnSegment( pos, c.a, c.b, q ) >= radius * radius )
		return false;

	float qt[3];
	const float distT = sqrtf( Phys_NearestOnSegment( tgt, c.a, c.b, qt ) );

	if ( distT < radius )
		radius = distT;

	if ( radius < 1e-3f )
		return false;

	float d[3];
	Vec3Sub( pos, q, d );

	const float dist = sqrtf( Vec3Dot( d, d ) );

	if ( dist >= radius )
		return false;

	if ( dist < 1e-5f )
	{
		// On the axis: leave on the side of the animated position.
		float s[3];
		Vec3Sub( tgt, qt, s );

		const float sl = sqrtf( Vec3Dot( s, s ) );

		if ( sl > 1e-5f )
		{
			d[0] = s[0] / sl;
			d[1] = s[1] / sl;
			d[2] = s[2] / sl;
		}
		else
		{
			d[0] = 0.0f;
			d[1] = -1.0f;
			d[2] = 0.0f;
		}

		pos[0] = q[0] + d[0] * radius;
		pos[1] = q[1] + d[1] * radius;
		pos[2] = q[2] + d[2] * radius;
		return true;
	}

	const float scale = radius / dist;

	pos[0] = q[0] + d[0] * scale;
	pos[1] = q[1] + d[1] * scale;
	pos[2] = q[2] + d[2] * scale;
	return true;
}

// All colliders and the floor, for one particle.
static bool Phys_CollideAll( float *pos, const float *tgt, float margin, const physCollider_w *colliders, int numColliders,
							 bool hasGround, float groundZ )
{
	bool hit = false;

	for ( int c = 0; c < numColliders; c++ )
		hit |= Phys_CollideRelaxed( pos, tgt, colliders[c], margin );

	if ( hasGround )
	{
		float floorZ = groundZ + margin;

		if ( tgt[2] < floorZ )
			floorZ = tgt[2];

		if ( pos[2] < floorZ )
		{
			pos[2] = floorZ;
			hit = true;
		}
	}

	return hit;
}

void Phys_ProjectColliders( float *pos, const float *target, const float *freeFactor, int n, float margin,
							const physCollider_w *colliders, int numColliders, bool hasGround, float groundZ )
{
	for ( int i = 0; i < n; i++ )
	{
		if ( freeFactor[i] > PHYS_MIN_FREE )
			Phys_CollideAll( pos + i * 3, target + i * 3, margin, colliders, numColliders, hasGround, groundZ );
	}
}

static inline void Phys_ClampRelative( float *v, const float *animVel, float maxSpeed )
{
	float r[3] = { v[0] - animVel[0], v[1] - animVel[1], v[2] - animVel[2] };
	const float len2 = Vec3Dot( r, r );

	if ( len2 > maxSpeed * maxSpeed )
	{
		const float scale = maxSpeed / sqrtf( len2 );

		r[0] *= scale;
		r[1] *= scale;
		r[2] *= scale;
	}

	v[0] = animVel[0] + r[0];
	v[1] = animVel[1] + r[1];
	v[2] = animVel[2] + r[2];
}

void Phys_ClothStep( physCloth_t &cloth, const physTopology_t &topo, const physGroupDef_t &def,
					 float dt, const float *target, const float windVel[3],
					 const physCollider_w *colliders, int numColliders, bool hasGround, float groundZ )
{
	const int n = cloth.numParticles;

	if ( !cloth.valid || dt <= 0.0f || n == 0 )
		return;

	int substeps = (int)ceilf( dt / PHYS_MAX_STEP );

	if ( substeps < 1 )						substeps = 1;
	else if ( substeps > PHYS_MAX_SUBSTEPS )	substeps = PHYS_MAX_SUBSTEPS;

	const float h = dt / substeps;
	const float dampK = expf( -def.damping * h );
	const float dragK = 1.0f - expf( -def.drag * h );
	const float margin = def.margin;
	const float maxSpeed = def.maxSpeed > 0.0f ? def.maxSpeed : 1e9f;

	const int numEdges = (int)topo.edgeRest.size();
	const int numBends = def.bend > 0.0f ? (int)topo.bendRest.size() : 0;
	const float alphaStretch = Phys_Compliance( def.stretch, PHYS_STRETCH_COMPLIANCE ) / ( h * h );
	const float alphaBend = Phys_Compliance( def.bend, PHYS_BEND_COMPLIANCE ) / ( h * h );
	const float alphaFollow = def.follow > 0.0f ? 1.0f / ( def.follow * def.follow * h * h ) : -1.0f;

	const bool aero = def.aero > 0.0f && !topo.tris.empty();
	const bool aniso = def.maxDistN > 0.0f && def.maxDist > 0.0f && !topo.tris.empty();
	const float ratioN = aniso ? def.maxDistN / def.maxDist : 1.0f;

	float *p = &cloth.p[0];
	float *tgt = &cloth.tgt[0];
	float *tgtVel = &cloth.tgtVel[0];

	if ( aero )
		cloth.acc.resize( n * 3 );

	if ( aniso )
	{
		cloth.targetNormal.resize( n * 3 );
		Phys_ComputeNormals( topo, target, &cloth.targetNormal[0] );
	}

	cloth.lambda.resize( numEdges + numBends );

	for ( int i = 0; i < n * 3; i++ )
		tgtVel[i] = ( target[i] - cloth.prevTarget[i] ) / dt;

	// The free particles follow a part of the motion of the pinned ones, so a fast move or a turn
	// does not give the whole speed of the character to the cloth.
	float carry[3] = { 0.0f, 0.0f, 0.0f };
	{
		int count = 0;

		for ( int i = 0; i < n; i++ )
		{
			if ( !cloth.pinned[i] )
				continue;

			for ( int k = 0; k < 3; k++ )
				carry[k] += target[i * 3 + k] - cloth.prevTarget[i * 3 + k];

			count++;
		}

		if ( !count )
		{
			for ( int i = 0; i < n; i++ )
			{
				for ( int k = 0; k < 3; k++ )
					carry[k] += target[i * 3 + k] - cloth.prevTarget[i * 3 + k];
			}

			count = n;
		}

		const float part = ( 1.0f - def.inertia ) / (float)count;

		for ( int k = 0; k < 3; k++ )
			carry[k] *= part;
	}

	const float carryShift[3] = { carry[0] / substeps, carry[1] / substeps, carry[2] / substeps };
	const float carryVel[3] = { carry[0] / dt, carry[1] / dt, carry[2] / dt };

	for ( int s = 0; s < substeps; s++ )
	{
		const float blend = (float)( s + 1 ) / (float)substeps;

		for ( int i = 0; i < n * 3; i++ )
			tgt[i] = cloth.prevTarget[i] + ( target[i] - cloth.prevTarget[i] ) * blend;

		// Push of the air on each triangle, shared between its particles.
		if ( aero )
		{
			float *acc = &cloth.acc[0];
			const int numTris = (int)topo.tris.size() / 3;

			memset( acc, 0, sizeof( float ) * 3 * n );

			for ( int t = 0; t < numTris; t++ )
			{
				const int a = topo.tris[t * 3 + 0];
				const int b = topo.tris[t * 3 + 1];
				const int c = topo.tris[t * 3 + 2];
				float e1[3], e2[3], nrm[3];

				Vec3Sub( &cloth.x[b * 3], &cloth.x[a * 3], e1 );
				Vec3Sub( &cloth.x[c * 3], &cloth.x[a * 3], e2 );

				nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
				nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
				nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];

				const float len = sqrtf( Vec3Dot( nrm, nrm ) );

				if ( len < 1e-6f )
					continue;

				nrm[0] /= len;
				nrm[1] /= len;
				nrm[2] /= len;

				float rel[3];

				for ( int k = 0; k < 3; k++ )
					rel[k] = windVel[k] - ( cloth.v[a * 3 + k] + cloth.v[b * 3 + k] + cloth.v[c * 3 + k] ) * ( 1.0f / 3.0f ) - carryVel[k];

				const float push = Vec3Dot( rel, nrm ) * def.aero * h;
				const int ids[3] = { a, b, c };

				for ( int k = 0; k < 3; k++ )
				{
					acc[ids[k] * 3 + 0] += nrm[0] * push;
					acc[ids[k] * 3 + 1] += nrm[1] * push;
					acc[ids[k] * 3 + 2] += nrm[2] * push;
				}
			}
		}

		// Predict the new positions. The stored speed is relative to the frame that the particles follow.
		for ( int i = 0; i < n; i++ )
		{
			if ( cloth.pinned[i] )
			{
				for ( int k = 0; k < 3; k++ )
					p[i * 3 + k] = tgt[i * 3 + k];

				continue;
			}

			float *v = &cloth.v[i * 3];
			const float *av = &tgtVel[i * 3];
			float vw[3];

			for ( int k = 0; k < 3; k++ )
			{
				vw[k] = v[k] + carryVel[k];
				vw[k] = av[k] + ( vw[k] - av[k] ) * dampK;
			}

			vw[2] -= PHYS_GRAVITY * def.gravity * h;

			for ( int k = 0; k < 3; k++ )
			{
				vw[k] += ( windVel[k] - vw[k] ) * dragK;

				if ( aero )
					vw[k] += cloth.acc[i * 3 + k] / ( topo.valence[i] > 0.0f ? topo.valence[i] : 1.0f );
			}

			Phys_ClampRelative( vw, av, maxSpeed );

			for ( int k = 0; k < 3; k++ )
			{
				v[k] = vw[k] - carryVel[k];
				p[i * 3 + k] = cloth.x[i * 3 + k] + carryShift[k] + v[k] * h;
			}
		}

		memset( &cloth.hit[0], 0, n );
		memset( &cloth.lambda[0], 0, sizeof( float ) * ( numEdges + numBends ) );
		memset( &cloth.lambdaFollow[0], 0, sizeof( float ) * n );

		for ( int it = 0; it < def.iterations; it++ )
		{
			for ( int e = 0; e < numEdges; e++ )
				Phys_SolveDistance( p, &cloth.pinned[0], topo.edges[e * 2], topo.edges[e * 2 + 1], topo.edgeRest[e], alphaStretch, cloth.lambda[e] );

			for ( int b = 0; b < numBends; b++ )
				Phys_SolveDistance( p, &cloth.pinned[0], topo.bends[b * 2], topo.bends[b * 2 + 1], topo.bendRest[b], alphaBend, cloth.lambda[numEdges + b] );

			for ( int i = 0; i < n; i++ )
			{
				if ( cloth.pinned[i] )
					continue;

				float d[3];
				Vec3Sub( &p[i * 3], &tgt[i * 3], d );

				// Soft link to the animated pose.
				if ( alphaFollow >= 0.0f )
				{
					const float len = sqrtf( Vec3Dot( d, d ) );

					if ( len > 1e-5f )
					{
						const float dl = ( -len - alphaFollow * cloth.lambdaFollow[i] ) / ( 1.0f + alphaFollow );
						const float k = dl / len;

						cloth.lambdaFollow[i] += dl;

						for ( int c = 0; c < 3; c++ )
						{
							p[i * 3 + c] += d[c] * k;
							d[c] += d[c] * k;
						}
					}
				}

				// Backstop: a particle stays inside an ellipsoid around its animated position.
				const float rt = cloth.maxDist[i];
				float e;

				if ( aniso )
				{
					const float *nrm = &cloth.targetNormal[i * 3];
					const float dn = Vec3Dot( d, nrm );
					const float rn = rt * ratioN;
					const float tx = d[0] - nrm[0] * dn, ty = d[1] - nrm[1] * dn, tz = d[2] - nrm[2] * dn;

					e = ( dn * dn ) / ( rn * rn ) + ( tx * tx + ty * ty + tz * tz ) / ( rt * rt );
				}
				else
				{
					e = Vec3Dot( d, d ) / ( rt * rt );
				}

				if ( e > 1.0f )
				{
					const float scale = 1.0f / sqrtf( e );

					for ( int c = 0; c < 3; c++ )
						p[i * 3 + c] = tgt[i * 3 + c] + d[c] * scale;
				}
			}

			// Collisions run last, so the result of an iteration is outside the colliders.
			for ( int i = 0; i < n; i++ )
			{
				if ( !cloth.pinned[i] && Phys_CollideAll( &p[i * 3], &tgt[i * 3], margin, colliders, numColliders, hasGround, groundZ ) )
					cloth.hit[i] = 1;
			}

			// The middle of an edge: a triangle must not cross a collider between its particles.
			if ( numColliders )
			{
				for ( int e = 0; e < numEdges; e++ )
				{
					const int a = topo.edges[e * 2];
					const int b = topo.edges[e * 2 + 1];
					const float wa = cloth.pinned[a] ? 0.0f : 1.0f;
					const float wb = cloth.pinned[b] ? 0.0f : 1.0f;

					if ( wa + wb == 0.0f )
						continue;

					float mid[3], midT[3], pushed[3];

					for ( int k = 0; k < 3; k++ )
					{
						mid[k] = ( p[a * 3 + k] + p[b * 3 + k] ) * 0.5f;
						midT[k] = ( tgt[a * 3 + k] + tgt[b * 3 + k] ) * 0.5f;
						pushed[k] = mid[k];
					}

					bool hit = false;

					for ( int c = 0; c < numColliders; c++ )
						hit |= Phys_CollideRelaxed( pushed, midT, colliders[c], margin );

					if ( !hit )
						continue;

					for ( int k = 0; k < 3; k++ )
					{
						const float corr = ( pushed[k] - mid[k] ) * 2.0f / ( wa + wb );

						p[a * 3 + k] += corr * wa;
						p[b * 3 + k] += corr * wb;
					}

					if ( wa > 0.0f )	cloth.hit[a] = 1;
					if ( wb > 0.0f )	cloth.hit[b] = 1;
				}
			}
		}

		// Speed from the position change.
		for ( int i = 0; i < n; i++ )
		{
			float *x = &cloth.x[i * 3];

			if ( !cloth.pinned[i] )
			{
				float *v = &cloth.v[i * 3];
				float vw[3];

				for ( int k = 0; k < 3; k++ )
					vw[k] = ( p[i * 3 + k] - x[k] - carryShift[k] ) / h + carryVel[k];

				Phys_ClampRelative( vw, &tgtVel[i * 3], maxSpeed );

				for ( int k = 0; k < 3; k++ )
				{
					v[k] = vw[k] - carryVel[k];

					if ( cloth.hit[i] )
						v[k] *= PHYS_HIT_DAMPING;
				}
			}

			x[0] = p[i * 3 + 0];
			x[1] = p[i * 3 + 1];
			x[2] = p[i * 3 + 2];
		}
	}

	memcpy( &cloth.prevTarget[0], target, sizeof( float ) * 3 * n );
}

//
// Lag mode
//

static void Phys_LagCenters( physLag_t &lag, const float *target, const float *freeFactor, int n )
{
	float ct[3] = { 0.0f, 0.0f, 0.0f }, pivot[3] = { 0.0f, 0.0f, 0.0f };
	float weight = 0.0f;
	int numFixed = 0;

	for ( int i = 0; i < n; i++ )
	{
		const float f = freeFactor[i];

		if ( f > PHYS_MIN_FREE )
		{
			for ( int k = 0; k < 3; k++ )
				ct[k] += target[i * 3 + k] * f;

			weight += f;
		}
		else
		{
			for ( int k = 0; k < 3; k++ )
				pivot[k] += target[i * 3 + k];

			numFixed++;
		}
	}

	for ( int k = 0; k < 3; k++ )
	{
		lag.ct[k] = weight > 0.0f ? ct[k] / weight : 0.0f;
		lag.pivot[k] = numFixed ? pivot[k] / (float)numFixed : lag.ct[k];
	}

	// With no fixed particle the group turns around a point above its mass center.
	if ( !numFixed )
		lag.pivot[2] += 8.0f;

	lag.reach = 0.0f;

	for ( int i = 0; i < n; i++ )
	{
		if ( freeFactor[i] > PHYS_MIN_FREE )
		{
			const float d = Vec3Dist( &target[i * 3], lag.pivot );

			if ( d > lag.reach )
				lag.reach = d;
		}
	}
}

void Phys_LagReset( physLag_t &lag, const float *target, const float *freeFactor, int n )
{
	Phys_LagCenters( lag, target, freeFactor, n );

	for ( int k = 0; k < 3; k++ )
	{
		lag.xc[k] = lag.ct[k];
		lag.vc[k] = 0.0f;
		lag.vct[k] = 0.0f;
	}

	lag.valid = true;
}

void Phys_LagStep( physLag_t &lag, const physGroupDef_t &def, float dt, const float *target,
				   const float *freeFactor, int n, const float windVel[3] )
{
	float prevCt[3];

	for ( int k = 0; k < 3; k++ )
		prevCt[k] = lag.ct[k];

	Phys_LagCenters( lag, target, freeFactor, n );

	if ( !lag.valid || dt <= 0.0f )
		return;

	int substeps = (int)ceilf( dt / PHYS_MAX_STEP );

	if ( substeps < 1 )						substeps = 1;
	else if ( substeps > PHYS_MAX_SUBSTEPS )	substeps = PHYS_MAX_SUBSTEPS;

	const float h = dt / substeps;
	const float dragK = 1.0f - expf( -def.drag * h );
	const float maxSpeed = def.maxSpeed > 0.0f ? def.maxSpeed : 1e9f;
	float omega = def.lagFreq;

	if ( omega * h > 1.0f )
		omega = 1.0f / h;

	const float zeta2 = 2.0f * def.lagDamp * omega;

	// The turn is bounded so that the farthest particle moves by maxdist at most. The mass offset alone does
	// not bound it: the turn grows as the mass gets close to the pivot.
	lag.maxAngle = lag.reach > 1.0f ? def.maxDist / lag.reach : 3.0f;

	for ( int k = 0; k < 3; k++ )
		lag.vct[k] = ( lag.ct[k] - prevCt[k] ) / dt;

	for ( int s = 0; s < substeps; s++ )
	{
		const float blend = (float)( s + 1 ) / (float)substeps;
		float ctNow[3];

		for ( int k = 0; k < 3; k++ )
		{
			ctNow[k] = prevCt[k] + ( lag.ct[k] - prevCt[k] ) * blend;

			float vr = lag.vc[k] - lag.vct[k];

			vr += ( omega * omega * ( ctNow[k] - lag.xc[k] ) - zeta2 * vr ) * h;

			float va = lag.vct[k] + vr;

			va += ( windVel[k] - va ) * dragK;
			lag.vc[k] = va;
		}

		Phys_ClampRelative( lag.vc, lag.vct, maxSpeed );

		for ( int k = 0; k < 3; k++ )
			lag.xc[k] += lag.vc[k] * h;

		// The offset from the animated mass center has an upper limit.
		float off[3];

		Vec3Sub( lag.xc, ctNow, off );

		const float len = sqrtf( Vec3Dot( off, off ) );

		if ( len > def.maxDist && len > 1e-5f )
		{
			const float scale = def.maxDist / len;

			for ( int k = 0; k < 3; k++ )
				lag.xc[k] = ctNow[k] + off[k] * scale;
		}
	}
}

void Phys_LagApply( const physLag_t &lag, const float *target, const float *freeFactor, int n, float *outPos )
{
	float a[3], b[3], delta[3], axis[3];

	Vec3Sub( lag.ct, lag.pivot, a );
	Vec3Sub( lag.xc, lag.pivot, b );
	Vec3Sub( lag.xc, lag.ct, delta );

	const float la = sqrtf( Vec3Dot( a, a ) );
	const float lb = sqrtf( Vec3Dot( b, b ) );
	float angle = 0.0f;
	bool rotate = false;

	if ( la > 1.0f && lb > 1.0f )
	{
		axis[0] = a[1] * b[2] - a[2] * b[1];
		axis[1] = a[2] * b[0] - a[0] * b[2];
		axis[2] = a[0] * b[1] - a[1] * b[0];

		const float sinA = sqrtf( Vec3Dot( axis, axis ) ) / ( la * lb );

		if ( sinA > 1e-6f )
		{
			const float cosA = Vec3Dot( a, b ) / ( la * lb );
			const float len = sqrtf( Vec3Dot( axis, axis ) );

			angle = atan2f( sinA, cosA );

			if ( angle > lag.maxAngle )
				angle = lag.maxAngle;

			axis[0] /= len;
			axis[1] /= len;
			axis[2] /= len;
			rotate = true;
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		const float w = freeFactor[i];
		const float *t = target + i * 3;
		float *out = outPos + i * 3;

		if ( w <= PHYS_MIN_FREE )
		{
			out[0] = t[0];
			out[1] = t[1];
			out[2] = t[2];
			continue;
		}

		if ( !rotate )
		{
			out[0] = t[0] + delta[0] * w;
			out[1] = t[1] + delta[1] * w;
			out[2] = t[2] + delta[2] * w;
			continue;
		}

		// Rodrigues rotation of the offset from the pivot, by a part of the angle.
		float r[3];
		Vec3Sub( t, lag.pivot, r );

		const float th = angle * w;
		const float c = cosf( th ), s = sinf( th );
		const float dot = Vec3Dot( axis, r );
		const float cx = axis[1] * r[2] - axis[2] * r[1];
		const float cy = axis[2] * r[0] - axis[0] * r[2];
		const float cz = axis[0] * r[1] - axis[1] * r[0];

		out[0] = lag.pivot[0] + r[0] * c + cx * s + axis[0] * dot * ( 1.0f - c );
		out[1] = lag.pivot[1] + r[1] * c + cy * s + axis[1] * dot * ( 1.0f - c );
		out[2] = lag.pivot[2] + r[2] * c + cz * s + axis[2] * dot * ( 1.0f - c );
	}
}

#define PHYS_LAG_PASSES		3
#define PHYS_LAG_MIN_DEPTH	0.05f
#define PHYS_LAG_MIN_LEVER	0.25f	// a particle near the pivot moves little: its contact must not throw the mass center far

void Phys_LagCollide( physLag_t &lag, const physGroupDef_t &def, const float *target, const float *freeFactor, int n,
					  const physCollider_w *colliders, int numColliders, bool hasGround, float groundZ, float *outPos )
{
	if ( !lag.valid || ( numColliders == 0 && !hasGround ) )
		return;

	for ( int pass = 0; pass < PHYS_LAG_PASSES; pass++ )
	{
		// The deepest contact moves the whole mass, so the hair slides on the body as one piece.
		float bestDepth = 0.0f, bestVec[3] = { 0.0f, 0.0f, 0.0f }, bestW = 1.0f;

		for ( int i = 0; i < n; i++ )
		{
			const float w = freeFactor[i];

			if ( w <= PHYS_MIN_FREE )
				continue;

			float pushed[3] = { outPos[i * 3], outPos[i * 3 + 1], outPos[i * 3 + 2] };

			if ( !Phys_CollideAll( pushed, target + i * 3, def.margin, colliders, numColliders, hasGround, groundZ ) )
				continue;

			float c[3];
			Vec3Sub( pushed, outPos + i * 3, c );

			const float depth = sqrtf( Vec3Dot( c, c ) );

			if ( depth > bestDepth )
			{
				bestDepth = depth;
				bestW = w;
				memcpy( bestVec, c, sizeof( bestVec ) );
			}
		}

		if ( bestDepth < PHYS_LAG_MIN_DEPTH )
			break;

		const float lever = bestW > PHYS_LAG_MIN_LEVER ? bestW : PHYS_LAG_MIN_LEVER;
		float normal[3];

		for ( int k = 0; k < 3; k++ )
		{
			lag.xc[k] += bestVec[k] / lever;
			normal[k] = bestVec[k] / bestDepth;
		}

		// Keep the offset limit of the mass.
		float off[3];

		Vec3Sub( lag.xc, lag.ct, off );

		const float len = sqrtf( Vec3Dot( off, off ) );

		if ( len > def.maxDist && len > 1e-5f )
		{
			for ( int k = 0; k < 3; k++ )
				lag.xc[k] = lag.ct[k] + off[k] * ( def.maxDist / len );
		}

		// Remove the speed into the collider, measured relative to the animation.
		float rel[3];

		Vec3Sub( lag.vc, lag.vct, rel );

		const float into = Vec3Dot( rel, normal );

		if ( into < 0.0f )
		{
			for ( int k = 0; k < 3; k++ )
				lag.vc[k] -= into * normal[k];
		}

		Phys_LagApply( lag, target, freeFactor, n, outPos );
	}

	// What is left is pushed out one particle at a time.
	Phys_ProjectColliders( outPos, target, freeFactor, n, def.margin, colliders, numColliders, hasGround, groundZ );
}

void Phys_ComputeNormals( const physTopology_t &topo, const float *pos, float *normals )
{
	const int n = topo.numParticles;
	const int numTris = (int)topo.tris.size() / 3;

	memset( normals, 0, sizeof( float ) * 3 * n );

	for ( int t = 0; t < numTris; t++ )
	{
		const int a = topo.tris[t * 3 + 0];
		const int b = topo.tris[t * 3 + 1];
		const int c = topo.tris[t * 3 + 2];
		float e1[3], e2[3], nrm[3];

		Vec3Sub( pos + b * 3, pos + a * 3, e1 );
		Vec3Sub( pos + c * 3, pos + a * 3, e2 );

		nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
		nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
		nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];

		const int ids[3] = { a, b, c };

		for ( int k = 0; k < 3; k++ )
		{
			normals[ids[k] * 3 + 0] += nrm[0];
			normals[ids[k] * 3 + 1] += nrm[1];
			normals[ids[k] * 3 + 2] += nrm[2];
		}
	}

	for ( int i = 0; i < n; i++ )
	{
		float *v = normals + i * 3;
		const float len = sqrtf( Vec3Dot( v, v ) );

		if ( len > 1e-8f )
		{
			v[0] /= len;
			v[1] /= len;
			v[2] /= len;
		}
	}
}

//
// Text of a group
//

static void ( *s_userPrint )( const char *msg ) = NULL;
static int s_parseErrors = 0;

static void Phys_CountingPrint( const char *msg )
{
	s_parseErrors++;

	if ( s_userPrint )
		s_userPrint( msg );
}

bool Phys_ParseGroupText( const char *text, physGroupDef_t &g, void ( *print )( const char *msg ) )
{
	std::vector<std::string> tok;
	const char *p = text;
	const std::string name = g.name;

	s_userPrint = print;
	s_parseErrors = 0;

	Phys_InitGroup( g, g.hair );
	g.name = name;

	while ( Phys_ReadLine( p, tok ) )
	{
		if ( tok[0] == "{" || tok[0] == "}" )
			continue;

		if ( !Phys_Stricmp( tok[0].c_str(), "cloth" ) || !Phys_Stricmp( tok[0].c_str(), "hair" ) )
		{
			const bool hair = !Phys_Stricmp( tok[0].c_str(), "hair" );

			if ( hair != g.hair )
			{
				// The type sets the defaults: start again with the new type.
				Phys_InitGroup( g, hair );
			}

			g.name = ( tok.size() > 1 && tok[1] != "{" ) ? tok[1] : name;
			continue;
		}

		Phys_ParseLine( g, tok, Phys_CountingPrint );
	}

	s_userPrint = NULL;
	return s_parseErrors == 0 && !g.surfaces.empty();
}

static void Phys_Emit( std::string &out, const char *key, float value, float def )
{
	char buf[64];

	if ( fabsf( value - def ) < 1e-6f )
		return;

	snprintf( buf, sizeof( buf ), "\t%s\t\t%g\n", key, value );
	out += buf;
}

static const char *Phys_AxisName( const float axis[3] )
{
	if ( axis[0] > 0.5f )	return "x";
	if ( axis[0] < -0.5f )	return "-x";
	if ( axis[1] > 0.5f )	return "y";
	if ( axis[1] < -0.5f )	return "-y";
	if ( axis[2] < -0.5f )	return "-z";
	return "z";
}

std::string Phys_GroupToText( const physGroupDef_t &g )
{
	physGroupDef_t d;
	std::string out;
	char buf[256];

	Phys_InitGroup( d, g.hair );

	out += g.hair ? "hair " : "cloth ";
	out += g.name.empty() ? "group" : g.name;
	out += "\n{\n\tsurfaces\t";

	for ( size_t i = 0; i < g.surfaces.size(); i++ )
	{
		if ( i )
			out += " ";

		out += g.surfaces[i];
	}

	out += "\n";

	for ( size_t i = 0; i < g.pins.size(); i++ )
	{
		const physPinRule_t &r = g.pins[i];

		if ( r.type == PHYS_PIN_GRADIENT )
		{
			snprintf( buf, sizeof( buf ), "\tpin\t\t\tgradient %s %s %g %g\n", r.bone.c_str(), Phys_AxisName( r.axis ), r.pinned, r.free );
			out += buf;
		}
		else if ( r.type == PHYS_PIN_RADIUS )
		{
			if ( r.axis[0] != 0.0f || r.axis[1] != 0.0f || r.axis[2] != 0.0f )
				snprintf( buf, sizeof( buf ), "\tpin\t\t\tradius %s %g %g %g %g %g\n", r.bone.c_str(), r.pinned, r.free, r.axis[0], r.axis[1], r.axis[2] );
			else
				snprintf( buf, sizeof( buf ), "\tpin\t\t\tradius %s %g %g\n", r.bone.c_str(), r.pinned, r.free );

			out += buf;
		}
		else if ( r.type == PHYS_PIN_BONES )
		{
			out += "\tpin\t\t\tbones";

			for ( size_t b = 0; b < r.bones.size(); b++ )
				out += " " + r.bones[b];

			out += "\n";
		}
		else
		{
			snprintf( buf, sizeof( buf ), "\tpin\t\t\tuv %g %g\n", r.pinned, r.free );
			out += buf;
		}
	}

	if ( g.lag )
		out += "\tmode\t\tlag\n";

	Phys_Emit( out, "proxy", g.proxy, d.proxy );

	if ( g.maxDist != d.maxDist || g.maxDistN != d.maxDistN )
	{
		if ( g.maxDistN > 0.0f )
			snprintf( buf, sizeof( buf ), "\tmaxdist\t\t%g %g\n", g.maxDist, g.maxDistN );
		else
			snprintf( buf, sizeof( buf ), "\tmaxdist\t\t%g\n", g.maxDist );

		out += buf;
	}

	Phys_Emit( out, "maxspeed", g.maxSpeed, d.maxSpeed );
	Phys_Emit( out, "gravity", g.gravity, d.gravity );
	Phys_Emit( out, "damping", g.damping, d.damping );
	Phys_Emit( out, "drag", g.drag, d.drag );
	Phys_Emit( out, "aero", g.aero, d.aero );
	Phys_Emit( out, "wind", g.wind, d.wind );
	Phys_Emit( out, "stretch", g.stretch, d.stretch );
	Phys_Emit( out, "bend", g.bend, d.bend );
	Phys_Emit( out, "follow", g.follow, d.follow );
	Phys_Emit( out, "inertia", g.inertia, d.inertia );
	Phys_Emit( out, "lagfreq", g.lagFreq, d.lagFreq );
	Phys_Emit( out, "lagdamp", g.lagDamp, d.lagDamp );
	Phys_Emit( out, "margin", g.margin, d.margin );
	Phys_Emit( out, "range", g.range, d.range );
	Phys_Emit( out, "iterations", (float)g.iterations, (float)d.iterations );

	if ( !g.enabled )
		out += "\tenabled\t\t0\n";

	if ( g.modelFit )
	{
		if ( g.modelFitScale != 1.0f )
			snprintf( buf, sizeof( buf ), "\tcollide\t\tmodel %g\n", g.modelFitScale );
		else
			snprintf( buf, sizeof( buf ), "\tcollide\t\tmodel\n" );

		out += buf;
	}

	if ( g.humanoid )
	{
		if ( g.humanoidScale != 1.0f )
			snprintf( buf, sizeof( buf ), "\tcollide\t\thumanoid %g\n", g.humanoidScale );
		else
			snprintf( buf, sizeof( buf ), "\tcollide\t\thumanoid\n" );

		out += buf;
	}

	for ( size_t i = 0; i < g.radiusOverride.size(); i++ )
	{
		snprintf( buf, sizeof( buf ), "\tcollide\t\tradius %s %g\n", g.radiusOverride[i].first.c_str(), g.radiusOverride[i].second );
		out += buf;
	}

	for ( size_t i = 0; i < g.removed.size(); i++ )
		out += "\tcollide\t\tremove " + g.removed[i] + "\n";

	for ( size_t i = 0; i < g.colliders.size(); i++ )
	{
		const physCollider_t &c = g.colliders[i];

		if ( c.capsule )
		{
			snprintf( buf, sizeof( buf ), "\tcollide\t\tcapsule %s %s %g\n", c.boneA.c_str(), c.boneB.c_str(), c.radius );
		}
		else if ( c.offA[0] != 0.0f || c.offA[1] != 0.0f || c.offA[2] != 0.0f )
		{
			snprintf( buf, sizeof( buf ), "\tcollide\t\tsphere %s %g %g %g %g\n", c.boneA.c_str(), c.radius, c.offA[0], c.offA[1], c.offA[2] );
		}
		else
		{
			snprintf( buf, sizeof( buf ), "\tcollide\t\tsphere %s %g\n", c.boneA.c_str(), c.radius );
		}

		out += buf;
	}

	out += "}\n";
	return out;
}

std::string Phys_DefToText( const physDef_t &def )
{
	std::string out;

	for ( size_t i = 0; i < def.groups.size(); i++ )
	{
		if ( i )
			out += "\n";

		out += Phys_GroupToText( def.groups[i] );
	}

	return out;
}
