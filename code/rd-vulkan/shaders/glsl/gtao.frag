#version 450

// Ground-truth ambient occlusion over the G-buffer extraction pass's depth + normal
// attachments (r_gtao). Horizon-search GTAO after Jimenez et al. 2016: per slice, walk
// the depth buffer either side of the pixel to find the largest unoccluded angles, then
// integrate the cosine-weighted visibility over the arc between them.
//
// Everything here is in VIEW space. That is only true because the skinned gbuffer
// shaders were switched over to it - a normal attachment holding two different spaces
// would give characters occlusion computed against a normal pointing somewhere else
// entirely, with nothing in the image to say which pixels were wrong.

layout(set = 0, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 0) uniform sampler2D normalTexture;

layout(location = 0) in vec2 frag_tex_coord;

// R = ambient visibility (GTAO), G = contact shadow visibility. One attachment for both
// so they share this pass's depth/normal binds and its horizon-search cost structure.
layout(location = 0) out vec4 out_ao;

layout(push_constant) uniform Params {
	// Projection terms, straight out of viewParms.projectionMatrix - see
	// R_SetupProjection()/R_SetupProjectionZ(). p5 is the UNFLIPPED value; the Y flip
	// the renderer applies for Vulkan is undone below rather than baked in here.
	float p0, p5, p8, p9, p10, p14;
	vec2  invScreen;		// 1 / render target size
	float radius;			// world-space sampling radius
	float intensity;		// final power curve
	float frameNoise;		// rotates the slice pattern per frame
	int   sliceCount;
	int   stepCount;

	// Contact shadows. Three loose floats rather than a vec3 on purpose: a vec3 in a push
	// constant block aligns to 16 bytes and would silently introduce padding the C struct
	// would have to mirror exactly. Direction points TOWARD the light, in view space.
	float lightX, lightY, lightZ;
	float csLength;		// world-space ray length; 0 disables
	float csThickness;	// how deep behind a surface a hit still counts as an occluder
	int   csSteps;
};

// Which end of the depth range means "nothing was drawn here". USE_REVERSED_DEPTH is
// currently OFF in this renderer, so the gbuffer clears depth to 1.0 and empty pixels sit
// at the far end - assuming the reversed convention here made the sky read as geometry
// glued to the far plane, which GTAO then happily occluded.
layout(constant_id = 0) const int reversedDepth = 0;

bool DepthIsEmpty(float d) {
	return (reversedDepth == 1) ? (d <= 0.0) : (d >= 1.0);
}

const float PI = 3.14159265359;
const float HALF_PI = 1.57079632679;

// View-space position of a pixel, from its depth. Derived from the frustum in
// R_SetupProjection(): clip.w is -z, so ndc = (p0*x + p8*z) / -z and
// depth = (p10*z + p14) / -z, which inverts to the three lines below. Returns a z of
// zero for the cleared background, which callers must treat as "no surface".
vec3 ViewPosition(vec2 uv, float depth) {
	vec2 ndc = uv * 2.0 - 1.0;

	float z = -p14 / (depth + p10);
	float x = -z * (ndc.x + p8) / p0;
	// The mvp is built with p5 negated (Vulkan's Y-down clip space), so the sign here
	// differs from x rather than matching it.
	float y =  z * (ndc.y + p9) / p5;

	return vec3(x, y, z);
}

float SampleDepth(vec2 uv) {
	return texture(depthTexture, uv).r;
}

// Screen-space contact shadow: march from the surface toward the light and see whether
// anything in the depth buffer stands in the way. Returns 1 for lit, 0 for shadowed.
//
// The thickness test is what keeps this honest: the depth buffer records a surface, not a
// solid, so a ray passing far behind a distant wall would otherwise report a hit. Only a
// hit within csThickness of the ray counts.
float ContactShadow(vec3 P, vec3 N, vec3 L) {
	if (csLength <= 0.0 || csSteps <= 0) {
		return 1.0;
	}

	// A surface turned away from the light is already unlit - the shading term has that
	// answer. Marching it anyway is what painted every wall and ceiling solid black: they
	// were reported as shadowed for facing the wrong way, not for being blocked.
	float NdotL = dot(N, L);
	if (NdotL <= 0.0) {
		return 1.0;
	}

	for (int i = 1; i <= csSteps; i++) {
		float t = (float(i) / float(csSteps)) * csLength;
		vec3 Q = P + L * t;

		// Forward projection, the exact inverse of ViewPosition(): w is -z, and the Y term
		// carries the negated p5 the renderer uses for Vulkan's clip space.
		float w = -Q.z;
		if (w <= 0.0) {
			break;	// stepped behind the eye
		}

		vec2 ndc = vec2( (p0 * Q.x + p8 * Q.z) / w,
						 (-p5 * Q.y + p9 * Q.z) / w );
		vec2 uv = ndc * 0.5 + 0.5;

		if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
			break;
		}

		float sceneDepth = SampleDepth(uv);
		if (DepthIsEmpty(sceneDepth)) {
			continue;	// nothing was drawn there
		}

		float sceneZ = -p14 / (sceneDepth + p10);
		float diff = sceneZ - Q.z;	// both negative; positive means the scene is nearer

		if (diff > 0.0 && diff < csThickness) {
			// Fade with how far along the ray the blocker sits: this is a CONTACT shadow, so a
			// blocker right against the surface is opaque and one at the end of the ray barely
			// registers. A flat 0 here is what produced the hard binary mask with no gradient
			// anywhere. NdotL softens grazing surfaces, where the march is least reliable.
			float fade = smoothstep(0.0, 1.0, t / csLength);
			return mix(1.0 - clamp(NdotL * 2.0, 0.0, 1.0), 1.0, fade);
		}
	}

	return 1.0;
}

void main() {
	float centerDepth = SampleDepth(frag_tex_coord);

	// Sky, and anything else the extraction pass skips, lands in this branch and stays
	// fully visible - both for ambient occlusion and for the contact shadow.
	if (DepthIsEmpty(centerDepth)) {
		out_ao = vec4(1.0);
		return;
	}

	vec3 P = ViewPosition(frag_tex_coord, centerDepth);
	vec3 N = normalize(texture(normalTexture, frag_tex_coord).rgb * 2.0 - 1.0);
	vec3 V = normalize(-P);

	// Screen-space extent of the world-space radius at this depth. Clamped so a surface
	// close to the camera does not turn into a full-screen search.
	float screenRadius = clamp(radius * p0 / (-P.z) * 0.5 / invScreen.x, 4.0, 128.0);

	// Interleaved gradient noise, offset per frame so the slice pattern differs frame to
	// frame rather than baking a fixed rosette into every image.
	vec2 pixel = gl_FragCoord.xy;
	float noise = fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))) + frameNoise);

	// Visibility, not occlusion: 1 is fully open. Named for what it holds so the output
	// convention is not something you have to derive from the integral.
	float visibilitySum = 0.0;

	for (int slice = 0; slice < sliceCount; slice++) {
		float phi = (float(slice) + noise) * PI / float(sliceCount);
		vec2 omega = vec2(cos(phi), sin(phi));

		// The slice plane is spanned by V and this direction; everything below is the
		// 2D problem inside it. The direction has to be the VIEW-space image of the screen
		// direction we are about to march, not the screen angle reused as if it were a view
		// angle: p0 and p5 differ (2*zProj/width vs 2*zProj/height), so 45 degrees on screen
		// is not 45 degrees in view space. Getting this wrong projects the normal into a
		// different plane from the one the horizons are found in - exact only dead centre.
		// Reconstructing one step along omega at the centre depth costs no texture fetch.
		vec3 sliceDir = ViewPosition(frag_tex_coord + omega * invScreen, centerDepth) - P;
		float sliceDirLen = length(sliceDir);
		if (sliceDirLen < 1e-6) {
			continue;
		}
		sliceDir /= sliceDirLen;
		vec3 orthoDir = sliceDir - dot(sliceDir, V) * V;
		vec3 axis = cross(sliceDir, V);
		vec3 projN = N - axis * dot(N, axis);

		float projNLen = length(projN);
		if (projNLen < 1e-5) {
			continue;
		}

		vec3 projNn = projN / projNLen;

		float sgn = sign(dot(orthoDir, projNn));
		float n = sgn * acos(clamp(dot(projNn, V), -1.0, 1.0));

		// Horizon angles either side, seeded at the tangent plane so a flat surface
		// integrates to no occlusion.
		float h[2];
		h[0] = -HALF_PI;
		h[1] =  HALF_PI;

		for (int side = 0; side < 2; side++) {
			float dirSign = (side == 0) ? -1.0 : 1.0;
			float cosHorizon = -1.0;

			for (int s = 1; s <= stepCount; s++) {
				// Jittered so the steps of neighbouring pixels do not line up into rings.
				float t = (float(s) - 0.5 + noise) / float(stepCount);
				vec2 offset = omega * dirSign * t * screenRadius * invScreen;
				vec2 sampleUV = frag_tex_coord + offset;

				if (any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0)))) {
					break;
				}

				float sampleDepth = SampleDepth(sampleUV);
				if (DepthIsEmpty(sampleDepth)) {
					continue;
				}

				vec3 S = ViewPosition(sampleUV, sampleDepth) - P;
				float dist = length(S);
				if (dist < 1e-4) {
					continue;
				}

				// Attenuate past the radius instead of cutting off, so a sample crossing
				// the boundary does not pop, and never let a distant background pixel
				// count as a near occluder.
				float falloff = clamp(1.0 - (dist / radius), 0.0, 1.0);
				float cosH = dot(S / dist, V);
				cosHorizon = max(cosHorizon, mix(-1.0, cosH, falloff));
			}

			h[side] = dirSign * acos(clamp(cosHorizon, -1.0, 1.0));
		}

		// Clamp each horizon into the hemisphere around the normal, then apply the
		// cosine-weighted arc integral (Jimenez eq. 7).
		h[0] = n + max(h[0] - n, -HALF_PI);
		h[1] = n + min(h[1] - n,  HALF_PI);

		float sinN = sin(n);
		float visibility =
			0.25 * (-cos(2.0 * h[0] - n) + cos(n) + 2.0 * h[0] * sinN) +
			0.25 * (-cos(2.0 * h[1] - n) + cos(n) + 2.0 * h[1] * sinN);

		visibilitySum += projNLen * visibility;
	}

	float visibility = clamp(visibilitySum / float(sliceCount), 0.0, 1.0);
	visibility = pow(visibility, intensity);

	// Offset the ray start along the normal so a surface does not shadow itself at
	// grazing angles, which is the classic contact-shadow acne.
	vec3 L = vec3(lightX, lightY, lightZ);
	// Bias scaled with view depth: one fixed world-space offset is either useless up close
	// or visibly detaches the shadow far away.
	float bias = max(0.5, -P.z * 0.004);
	float shadow = ContactShadow(P + N * bias, N, L);

	out_ao = vec4(visibility, shadow, 0.0, 1.0);
}
