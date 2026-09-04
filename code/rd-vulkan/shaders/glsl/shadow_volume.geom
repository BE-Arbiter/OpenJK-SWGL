#version 450

// GPU stencil shadow volume silhouette extrusion - geometry stage.
//
// Replaces the per-frame CPU walk in R_CalcShadowEdges/RB_ShadowTessEnd
// (tr_shadows.cpp) with the standard triangles-with-adjacency silhouette
// test: for each triangle, and for each of its 3 edges, compare this
// triangle's facing (relative to the light) against the neighboring
// triangle sharing that edge (given to us via the adjacency indices built
// once at model load - see R_BuildShadowAdjacency, vk_vbo.cpp). An edge is
// a silhouette edge if this triangle faces the light and the neighbor does
// not - matching R_CalcShadowEdges' rule exactly, including its treatment
// of open/boundary edges: an edge with no neighbour at all is always a
// silhouette edge. R_BuildShadowAdjacency encodes "no neighbour" by pointing
// the adjacency slot at one of the edge's own vertices, which main() below
// detects by exact position comparison (see the comment there).
//
// Input vertex layout (standard GL_TRIANGLES_ADJACENCY convention):
//   0,2,4 = the triangle's own vertices (p0,p1,p2)
//   1,3,5 = the opposite vertex of the neighbor across edges (p0-p1),
//           (p1-p2), (p2-p0) respectively
//
// For each silhouette edge, emits the same 2 quad triangles (with the same
// vertex order, hence the same winding) that RB_ShadowTessEnd's CPU path
// builds, so this drives the exact same front/back-culled increment/
// decrement pipelines (vk.std_pipeline.shadow_volume_pipelines) unchanged.

layout(triangles_adjacency) in;
layout(triangle_strip, max_vertices = 18) out;

// Must match shadow_volume.vert's stripped-down "out gl_PerVertex { vec4
// gl_Position; }" exactly, member for member - without this redeclaration
// GLSL assumes the full default block (Position/PointSize/ClipDistance/
// CullDistance) for gl_in[], which then fails SPIR-V interface validation
// against the vertex stage's smaller one.
in gl_PerVertex {
	vec4 gl_Position;
} gl_in[];

layout(push_constant) uniform Transform {
	mat4 mvp;
	vec3 lightDir;
	// entity.origin.z - shadowPlane + fudge, folded into one constant so the
	// per-vertex extrusion only needs a single add (matches
	// RB_ShadowTessEnd's groundDist computation).
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

	// An edge is a silhouette edge if the neighbouring triangle across it does
	// NOT face the light, OR if there is no neighbour at all - that second half
	// is R_CalcShadowEdges' rule verbatim ("if it doesn't share the edge with
	// another front facing triangle, it is a sil edge"), and it is what keeps
	// the volume closed across the many open edges a .glm has (every surface
	// boundary and every UV split duplicates its vertices, so those edges never
	// find a neighbour by index).
	//
	// R_BuildShadowAdjacency (vk_vbo.cpp) encodes "no neighbour" by pointing the
	// adjacency slot at one of the edge's own two vertices, so the boundary test
	// is an exact position comparison against that vertex: both come from the
	// same source index, hence the same skinning result, bit for bit. Do NOT
	// test the neighbour normal's magnitude instead - the cross product of two
	// identical vectors is only exactly zero without FMA contraction, and with
	// it the residue is large enough (~1e-7 for this vertex scale) that no fixed
	// epsilon separates a boundary from a genuine thin sliver.
	//
	// Neighbour winding: a well-wound neighbour traverses the shared edge
	// backwards, so the one across (p0,p1) is (p1,p0,adj01) and its normal uses
	// that vertex order.
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
