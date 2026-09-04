#version 450

// Shadow volume silhouette extrusion, geometry stage. Replaces the per-frame CPU
// walk in R_CalcShadowEdges (tr_shadows.cpp) with the standard adjacency test:
// an edge is a silhouette edge when this triangle faces the light and the
// neighbour across it does not, or when it has no neighbour at all.
//
// Input layout is the GL_TRIANGLES_ADJACENCY convention: 0,2,4 are p0,p1,p2 and
// 1,3,5 are the opposite vertex of the neighbour across (p0,p1), (p1,p2), (p2,p0).
// Quads are emitted in the same vertex order RB_ShadowTessEnd uses, so the same
// increment/decrement pipelines drive them unchanged.

layout(triangles_adjacency) in;
layout(triangle_strip, max_vertices = 18) out;

// Must match shadow_volume.vert's stripped-down gl_PerVertex member for member;
// without this, gl_in[] assumes the full default block and fails SPIR-V
// interface validation against the vertex stage.
in gl_PerVertex {
	vec4 gl_Position;
} gl_in[];

layout(push_constant) uniform Transform {
	mat4 mvp;
	vec3 lightDir;
	// entity.origin.z - shadowPlane + fudge in one constant, so extrusion needs
	// a single add (matches RB_ShadowTessEnd's groundDist).
	float groundOffset;
};

out gl_PerVertex {
	vec4 gl_Position;
};

vec3 extrude( vec3 pos )
{
	float groundDist = pos.z + groundOffset;
	return pos - groundDist * lightDir;
}

void emitQuad( vec3 a, vec3 b, vec3 aExt, vec3 bExt )
{
	gl_Position = mvp * vec4( a, 1.0 );
	EmitVertex();
	gl_Position = mvp * vec4( b, 1.0 );
	EmitVertex();
	gl_Position = mvp * vec4( aExt, 1.0 );
	EmitVertex();
	EndPrimitive();

	gl_Position = mvp * vec4( b, 1.0 );
	EmitVertex();
	gl_Position = mvp * vec4( bExt, 1.0 );
	EmitVertex();
	gl_Position = mvp * vec4( aExt, 1.0 );
	EmitVertex();
	EndPrimitive();
}

void main() {
	vec3 p0    = gl_in[0].gl_Position.xyz;
	vec3 adj01 = gl_in[1].gl_Position.xyz;
	vec3 p1    = gl_in[2].gl_Position.xyz;
	vec3 adj12 = gl_in[3].gl_Position.xyz;
	vec3 p2    = gl_in[4].gl_Position.xyz;
	vec3 adj20 = gl_in[5].gl_Position.xyz;

	vec3 mainNormal = cross( p1 - p0, p2 - p0 );
	if ( dot( mainNormal, lightDir ) <= 0.0 )
		return; // this triangle doesn't face the light: none of its edges can be silhouette edges

	// A well-wound neighbour traverses the shared edge backwards, so the one
	// across (p0,p1) is (p1,p0,adj01) and its normal uses that vertex order.
	// R_BuildShadowAdjacency marks "no neighbour" by putting one of the edge's
	// own vertices in the slot, hence the exact position test - both come from
	// the same index, so the same skinning result bit for bit.
	vec3 n0 = cross( p0 - p1, adj01 - p1 );
	if ( adj01 == p0 || dot( n0, lightDir ) <= 0.0 ) {
		emitQuad( p0, p1, extrude( p0 ), extrude( p1 ) );
	}

	vec3 n1 = cross( p1 - p2, adj12 - p2 );
	if ( adj12 == p1 || dot( n1, lightDir ) <= 0.0 ) {
		emitQuad( p1, p2, extrude( p1 ), extrude( p2 ) );
	}

	vec3 n2 = cross( p2 - p0, adj20 - p0 );
	if ( adj20 == p2 || dot( n2, lightDir ) <= 0.0 ) {
		emitQuad( p2, p0, extrude( p2 ), extrude( p0 ) );
	}
}
