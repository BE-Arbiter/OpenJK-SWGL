#version 450

// Depth+normal+velocity G-buffer extraction pass, r_velocityBuffer ON variant (2
// color attachments: normal + velocity). See gbuffer.vert and gbuffer.frag (the
// other variant, used instead whenever r_velocityBuffer is off).

layout(location = 0) in vec3 var_ViewNormal;

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec2 out_velocity;

void main() {
	vec3 n = normalize(var_ViewNormal);
	out_normal = vec4(n * 0.5 + 0.5, 0.0);

	// Ghoul2 (skinned) only - every rigid surface goes through gbuffer_worldvel.frag
	// instead, which computes a real motion vector. Correct skinned velocity needs
	// previous-frame bone matrices (and a push constant range vk.pipeline_layout
	// doesn't have), neither of which exists yet. Zero == "no motion", matching this
	// attachment's own clear value, so it stays a well-defined placeholder rather
	// than undefined content.
	out_velocity = vec2(0.0);
}
