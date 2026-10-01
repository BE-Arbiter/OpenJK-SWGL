// ========================================================================== //
// ReSTIR GI for the first diffuse bounce.
//
// The bounce pass of a diffuse ray makes one candidate: the point that the
// bounce ray hits and the radiance Lo that it sends back to the primary surface.
// The candidate and the reservoir of the previous frame go into one RIS step.
// The selected sample gives the LF contribution of the pixel, and the reservoir
// stays in the images for the next frame.
//
// Estimator. The tracer without ReSTIR adds T * Lo for a direction w that the
// sampler draws with the solid angle density pdf_s(w). T is the throughput of
// the primary surface. The expectation is the integral of h(w) = T * Lo * pdf_s(w).
// With the target p_hat(w) = luminance(Lo) * pdf_s(w), a candidate has the
// weight p_hat / pdf_s = luminance(Lo). The reservoir selects a sample y and has
//   W = w_sum / (M * p_hat(y)),
// and the contribution is h(y) * W = T * Lo * pdf_s(y) * W. Without reuse
// M = 1 and W = 1 / pdf_s(y), so the contribution is T * Lo as before.
//
// A candidate that hits a dynamic surface (model, mover, deformed world) cannot be reused: its point
// does not stay in place. The class of the hit is a property of the candidate, so the reservoir chain
// holds static hits only. A dynamic candidate enters the RIS with weight 0 (it counts in M, it is never
// selected or stored) and adds its own T * Lo directly. The expectation is the sum of the two parts.
//
// The includer has to include path_tracer_rgen.h before this file.
// ========================================================================== //

#ifndef  _RESTIR_GI_H_
#define  _RESTIR_GI_H_

#include "restir_common.h"

#define RESTIR_GI_FLAG_SKY			1u		// pos is a unit direction: the sample is the sky
#define RESTIR_GI_FLAG_DYNAMIC		2u		// the sample point is on a model, a mover or a deformed surface: the candidate is never stored

#define RESTIR_GI_M_MASK			0xffffu	// bits of the reservoir meta word: M, then the age, then the flags
#define RESTIR_GI_AGE_SHIFT			16u
#define RESTIR_GI_AGE_MASK			0xffu
#define RESTIR_GI_FLAGS_SHIFT		24u

#define RESTIR_GI_ORIGIN_OFFSET		0.01	// moves the visibility ray off the primary surface, as the bounce ray does
#define RESTIR_GI_RAY_LENGTH		10000.0	// length of the visibility ray of a sky sample, as the bounce ray

struct GiReservoir
{
	// Persisted between frames
	vec3 pos;		// the sample point; for a sky sample the unit direction
	vec3 normal;	// the normal of the sample point, on the side of the ray that hit it
	vec3 lo;		// the radiance that the sample point sends to the primary surface, without its throughput
	float W;		// the unbiased contribution weight
	uint M;			// number of candidates that the sample stands for
	uint age;		// frames since the sample was a candidate
	uint flags;		// RESTIR_GI_FLAG_*

	// Not persisted
	float w_sum;	// sum of the weights of the candidates so far
	float p_hat;	// the target function of the selected sample
};

// The data of the primary surface that the resampling needs.
struct GiPrimary
{
	ivec2 ipos;
	vec3 position;
	vec3 view_direction;		// from the camera to the surface
	vec3 bounce_direction;		// the direction that the sampler drew for the candidate
	vec3 normal;
	vec3 basis_normal;			// the normal that the direction sampler used
	vec3 throughput;			// of the primary surface, with 1 / (1 - specular_pdf) but without the Fresnel term
	vec3 base_reflectivity;
	float specular_factor;
	int shadow_cull_mask;
	bool is_gradient;
};

void
init_gi_reservoir(out GiReservoir r)
{
	r.pos = vec3(0);
	r.normal = vec3(0, 0, 1);
	r.lo = vec3(0);
	r.W = 0.0;
	r.M = 0u;
	r.age = 0u;
	r.flags = 0u;
	r.w_sum = 0.0;
	r.p_hat = 0.0;
}

// A candidate for a ray that left the primary surface in the given direction. It is
// a sky sample without radiance. The caller sets lo, and pos, normal and flags for a surface hit.
GiReservoir
restir_gi_candidate(vec3 direction)
{
	GiReservoir r;
	init_gi_reservoir(r);
	r.pos = direction;
	r.M = 1u;
	r.flags = RESTIR_GI_FLAG_SKY;

	return r;
}

// The Fresnel term of the diffuse ray: the light that the specular lobe reflects does not go to the diffuse lobe.
// V points from the surface to the camera, L to the sample.
vec3
diffuse_fresnel(vec3 L, vec3 V, vec3 base_reflectivity, float specular_factor)
{
	vec3 H = normalize(V + L);
	float VoH = max(0, dot(V, H));

	return schlick_fresnel(base_reflectivity, VoH, specular_factor);
}

// ========================================================================== //
// Storage
// ========================================================================== //

void
restir_gi_store_invalid(ivec2 ipos)
{
	imageStore(IMG_PT_RESTIR_GI_RAD_A, ipos, uvec4(0));
}

// POS_A: pos and the encoded normal. RAD_A: lo as two halves in x and y, the meta word in z, W in w.
// A reservoir without weight is stored as invalid: the next frame does not read its POS_A.
void
restir_gi_store(ivec2 ipos, GiReservoir r)
{
	if(!(r.W > 0.0) || r.M == 0u)
	{
		restir_gi_store_invalid(ipos);
		return;
	}

	imageStore(IMG_PT_RESTIR_GI_POS_A, ipos, vec4(r.pos, uintBitsToFloat(encode_normal(r.normal))));

	uvec4 rad;
	rad.x = packHalf2x16(r.lo.rg);
	rad.y = packHalf2x16(vec2(r.lo.b, 0.0));
	rad.z = min(r.M, RESTIR_GI_M_MASK)
		| (min(r.age, RESTIR_GI_AGE_MASK) << RESTIR_GI_AGE_SHIFT)
		| (r.flags << RESTIR_GI_FLAGS_SHIFT);
	rad.w = floatBitsToUint(r.W);
	imageStore(IMG_PT_RESTIR_GI_RAD_A, ipos, rad);
}

// Reads the reservoir of a pixel of the previous frame. False when it holds no sample.
bool
restir_gi_load(ivec2 pos, out GiReservoir r)
{
	init_gi_reservoir(r);

	uvec4 rad = texelFetch(TEX_PT_RESTIR_GI_RAD_B, pos, 0);
	r.W = uintBitsToFloat(rad.w);
	r.M = rad.z & RESTIR_GI_M_MASK;

	if(r.M == 0u || !(r.W > 0.0) || isinf(r.W))
		return false;

	vec4 pos_normal = texelFetch(TEX_PT_RESTIR_GI_POS_B, pos, 0);
	r.pos = pos_normal.xyz;
	r.normal = decode_normal(floatBitsToUint(pos_normal.w));
	r.lo = vec3(unpackHalf2x16(rad.x), unpackHalf2x16(rad.y).x);
	r.age = (rad.z >> RESTIR_GI_AGE_SHIFT) & RESTIR_GI_AGE_MASK;
	r.flags = rad.z >> RESTIR_GI_FLAGS_SHIFT;

	return true;
}

// ========================================================================== //
// Sample, target function and shading
// ========================================================================== //

// The direction from the primary surface to the sample.
vec3
restir_gi_direction(GiReservoir s, vec3 position)
{
	return (s.flags & RESTIR_GI_FLAG_SKY) != 0u ? s.pos : normalize(s.pos - position);
}

// The solid angle density of the direction sampler in indirect_lighting.rgen. It makes a point of
// a disk with the radius u^power, u uniform, and lifts it to the hemisphere: with the density
// r^(1 / power - 2) / (2 pi power) in the disk, and cos(theta) for the solid angle.
// The power is 0.5 for the cosine sampler, and HEMISPHERE_UNIFORMISH for the SH store.
float
restir_gi_source_pdf(vec3 direction, vec3 basis_normal)
{
	float cos_theta = dot(direction, basis_normal);

	if(!(cos_theta > 0.0))
		return 0.0;

#if ENABLE_SH
	if((global_ubo.pt_denoiser_flags & DENOISER_FLAG_LF_SH) != 0)
	{
		float radius = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

		return cos_theta * pow(radius, 1.0 / HEMISPHERE_UNIFORMISH - 2.0) / (2.0 * M_PI * HEMISPHERE_UNIFORMISH);
	}
#endif

	return cos_theta / M_PI;
}

// The target function of the sample for the primary surface at position.
float
restir_gi_target(GiReservoir s, vec3 position, vec3 basis_normal)
{
	return luminance(s.lo) * restir_gi_source_pdf(restir_gi_direction(s, position), basis_normal);
}

// Copies the selected sample, not the sums.
void
restir_gi_take(inout GiReservoir r, GiReservoir s, float p_hat)
{
	r.pos = s.pos;
	r.normal = s.normal;
	r.lo = s.lo;
	r.age = s.age;
	r.flags = s.flags;
	r.p_hat = p_hat;
}

// The LF contribution of a reservoir for a direction from the primary surface to its sample.
vec3
restir_gi_shade(GiReservoir r, GiPrimary p, vec3 direction)
{
	float pdf = restir_gi_source_pdf(direction, p.basis_normal);
	vec3 F = diffuse_fresnel(direction, -p.view_direction, p.base_reflectivity, p.specular_factor);

	return p.throughput * (vec3(1.0) - F) * r.lo * (pdf * r.W);
}

// 1 when nothing blocks the way from the primary surface to the sample.
float
restir_gi_visibility(GiReservoir s, GiPrimary p)
{
	vec3 origin = p.position - p.view_direction * RESTIR_GI_ORIGIN_OFFSET;
	vec3 target = (s.flags & RESTIR_GI_FLAG_SKY) != 0u ? origin + s.pos * RESTIR_GI_RAY_LENGTH : s.pos;

	return trace_shadow_ray(get_shadow_ray(origin, target, 0.0), p.shadow_cull_mask);
}

// ========================================================================== //
// Temporal reuse
// ========================================================================== //

// The reservoir of the previous frame for the surface of pixel ipos. False when there is none
// that fits: no history, other surface, other object, or too old.
bool
restir_gi_load_temporal(ivec2 ipos, vec3 normal, out GiReservoir prev)
{
	init_gi_reservoir(prev);

	if(global_ubo.restir_gi_history_valid == 0 || global_ubo.pt_restir_gi_max_age < 1.0)
		return false;

	vec4 motion = texelFetch(TEX_PT_MOTION, ipos, 0);
	float view_depth = texelFetch(TEX_PT_VIEW_DEPTH_A, ipos, 0).x;
	uint object = restir_object(texelFetch(TEX_PT_VISBUF_PRIM_A, ipos, 0).x, false);

	int field_left, field_right;
	restir_get_prev_field(ipos, field_left, field_right);
	ivec2 pos_prev = restir_get_prev_pos(ipos, motion);

	if(pos_prev.x < field_left || pos_prev.x >= field_right || pos_prev.y < 0 || pos_prev.y >= global_ubo.prev_height)
		return false;

	if(!restir_prev_is_same_surface(pos_prev, view_depth, normal, object, true))
		return false;

	if(!restir_gi_load(pos_prev, prev))
		return false;

	uint max_age = uint(min(global_ubo.pt_restir_gi_max_age, float(RESTIR_GI_AGE_MASK)));
	uint m_clamp = uint(clamp(global_ubo.pt_restir_gi_m_clamp, 1.0, float(RESTIR_GI_M_MASK)));

	if(prev.age + 1u > max_age)
		return false;

	prev.M = min(prev.M, m_clamp);

	return true;
}

// A pixel that traces no diffuse ray this frame passes the reservoir of the previous frame on.
void
restir_gi_carry(ivec2 ipos, vec3 normal)
{
	GiReservoir prev;

	if(restir_gi_load_temporal(ipos, normal, prev))
	{
		prev.age += 1u;
		restir_gi_store(ipos, prev);
	}
	else
		restir_gi_store_invalid(ipos);
}

// Resamples the candidate with the reservoir of the previous frame, stores the result for the
// next frame, and returns the LF contribution of the reservoir with its direction. The contribution of a
// dynamic candidate comes in extra_contrib with its own direction, for the SH store. The pixels that the A-SVGF re-shades as gradient
// samples take the candidate only: they have to repeat the sampling of the previous frame.
vec3
restir_gi_resolve(GiPrimary p, GiReservoir candidate, out vec3 direction, out vec3 extra_contrib, out vec3 extra_direction)
{
	GiReservoir r;
	init_gi_reservoir(r);

	float rng = get_rng(RNG_RESTIR_GI_SELECTION);

	// The weight of a candidate is p_hat / pdf of the sampler that made it. The candidate keeps the
	// direction that was drawn: the direction to its hit point differs by the ray origin offset.
	float pdf = restir_gi_source_pdf(p.bounce_direction, p.basis_normal);
	float p_hat = luminance(candidate.lo) * pdf;
	bool dynamic = (candidate.flags & RESTIR_GI_FLAG_DYNAMIC) != 0u;
	float w = (pdf > 0.0 && !dynamic) ? p_hat / pdf : 0.0;

	extra_contrib = vec3(0);
	extra_direction = p.bounce_direction;

	if(dynamic)
	{
		vec3 F = diffuse_fresnel(p.bounce_direction, -p.view_direction, p.base_reflectivity, p.specular_factor);
		extra_contrib = p.throughput * (vec3(1.0) - F) * candidate.lo;
	}

	if(ris_select(r.w_sum, r.M, w, candidate.M, rng))
		restir_gi_take(r, candidate, p_hat);

	bool from_previous = false;
	GiReservoir prev;

	if(!p.is_gradient && restir_gi_load_temporal(p.ipos, p.normal, prev))
	{
		float p_hat_prev = restir_gi_target(prev, p.position, p.basis_normal);
		float w_prev = p_hat_prev * prev.W * float(prev.M);

		if(isnan(w_prev) || isinf(w_prev))
			w_prev = 0.0;

		if(ris_select(r.w_sum, r.M, w_prev, prev.M, rng))
		{
			restir_gi_take(r, prev, p_hat_prev);
			r.age = prev.age + 1u;
			from_previous = true;
		}
	}

	if(r.p_hat > 0.0)
		r.W = r.w_sum / (float(r.M) * r.p_hat);

	if(isnan(r.W) || isinf(r.W))
		r.W = 0.0;

	// A sample of the previous frame has to be visible from this pixel.
	if(from_previous && r.W > 0.0 && restir_gi_visibility(r, p) == 0.0)
		r.W = 0.0;

	restir_gi_store(p.ipos, r);

	direction = from_previous ? restir_gi_direction(r, p.position) : p.bounce_direction;

	return r.W > 0.0 ? restir_gi_shade(r, p, direction) : vec3(0);
}

#endif  /*_RESTIR_GI_H_*/
