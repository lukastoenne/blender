#include "stdio.h"
#include "math.h"

#include <stdbool.h>

#include "BJIT_forcefield.h"

static inline void copy_v3_v3(float r[3], const float a[3])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

static inline void add_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] = a[0] + b[0];
	r[1] = a[1] + b[1];
	r[2] = a[2] + b[2];
}

static inline void sub_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
	r[2] = a[2] - b[2];
}

static inline void mul_v3_m4v3(float r[3], float mat[4][4], const float vec[3])
{
	const float x = vec[0];
	const float y = vec[1];

	r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
	r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
	r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}

static inline void mul_v3_v3fl(float r[3], const float v[3], float f)
{
	r[0] = v[0] * f;
	r[1] = v[1] * f;
	r[2] = v[2] * f;
}

static inline float dot_v3v3(const float a[3], const float b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void zero_v3(float v[3])
{
	v[0] = 0.0f;
	v[1] = 0.0f;
	v[2] = 0.0f;
}

static inline float len_v3(const float v[3])
{
	float d = dot_v3v3(v, v);
	return sqrtf(d);
}

static inline float normalize_v3_v3(float r[3], const float a[3])
{
	float d = dot_v3v3(a, a);

	/* a larger value causes normalize errors in a
	 * scaled down models with camera extreme close */
	if (d > 1.0e-35f) {
		d = sqrtf(d);
		mul_v3_v3fl(r, a, 1.0f / d);
	}
	else {
		zero_v3(r);
		d = 0.0f;
	}

	return d;
}

static inline float normalize_v3(float n[3])
{
	return normalize_v3_v3(n, n);
}

/* Project v1 on v2 */
void project_v3_v3v3(float c[3], const float v1[3], const float v2[3])
{
	const float mul = dot_v3v3(v1, v2) / dot_v3v3(v2, v2);

	c[0] = mul * v2[0];
	c[1] = mul * v2[1];
	c[2] = mul * v2[2];
}

/**
 * In this case plane is a 3D vector only (no 4th component).
 *
 * Projecting will make \a c a copy of \a v orthogonal to \a v_plane.
 *
 * \note If \a v is exactly perpendicular to \a v_plane, \a c will just be a copy of \a v.
 */
void project_plane_v3_v3v3(float c[3], const float v[3], const float v_plane[3])
{
	float delta[3];
	project_v3_v3v3(delta, v, v_plane);
	sub_v3_v3v3(c, v, delta);
}

/* ------------------------------------------------------------------------- */

/* maxdist: zero effect from this distance outwards (if usemax) */
/* mindist: full effect up to this distance (if usemin) */
/* power: falloff with formula 1/r^power */
static float get_falloff_old(float fac, bool usemin, float mindist, bool usemax, float maxdist, float power)
{
	/* first quick checks */
	if (usemax && fac > maxdist)
		return 0.0f;

	if (usemin && fac < mindist)
		return 1.0f;

	if (!usemin)
		mindist = 0.0;

	return pow((double)(1.0f+fac-mindist), (double)(-power));
}

static float get_falloff(const EffectorEvalSettings *settings, float distance)
{
	const bool use_min = settings->flag & EFF_FIELD_USE_MIN;
	const bool use_max = settings->flag & EFF_FIELD_USE_MAX;
	return get_falloff_old(distance, use_min, settings->mindist, use_max, settings->maxdist, settings->f_power);
}

static float get_falloff_radial(const EffectorEvalSettings *settings, float distance)
{
	const bool use_min = settings->flag & EFF_FIELD_USE_MIN_RAD;
	const bool use_max = settings->flag & EFF_FIELD_USE_MAX_RAD;
	return get_falloff_old(distance, use_min, settings->minrad, use_max, settings->maxrad, settings->f_power_r);
}

/* ------------------------------------------------------------------------- */

/* relation of a point to the effector,
 * based on type, shape, etc.
 */
typedef struct EffectorPointRelation {
	/* closest on the effector */
	float closest_loc[3];
	float closest_nor[3];
	float closest_vel[3];
	
	/* relative coordinates of the point */
	float loc_rel[3];
	float dist_rel;
} EffectorPointRelation;

bool get_point_relation(EffectorPointRelation *rel, const EffectorEvalInput *input, const EffectorEvalSettings *settings)
{
	switch (settings->shape) {
		case EFF_FIELD_SHAPE_POINT: {
			/* use center of object for distance calculus */
			copy_v3_v3(rel->closest_loc, settings->tfm[3]);
			
			/* use z-axis as normal*/
			normalize_v3_v3(rel->closest_nor, settings->tfm[2]);
			
			// TODO
			zero_v3(rel->closest_vel);
			
			break;
		}
		case EFF_FIELD_SHAPE_PLANE: {
			float center[3], locrel[3], offset[3];
			
			/* use z-axis as normal*/
			normalize_v3_v3(rel->closest_nor, settings->tfm[2]);
			
			/* use center of object for distance calculus */
			copy_v3_v3(center, settings->tfm[3]);
			/* radial offset */
			sub_v3_v3v3(locrel, input->loc, center);
			project_plane_v3_v3v3(offset, locrel, rel->closest_nor);
			add_v3_v3v3(rel->closest_loc, center, offset);
			
			// TODO
			zero_v3(rel->closest_vel);
			
			break;
		}
		case EFF_FIELD_SHAPE_SURFACE: {
			return false; // TODO
			break;
		}
		case EFF_FIELD_SHAPE_POINTS: {
			return false; // TODO
			break;
		}
		
		default:
			return false;
	}
	
	sub_v3_v3v3(rel->loc_rel, input->loc, rel->closest_loc);
	rel->dist_rel = len_v3(rel->loc_rel);
	
	return true;
}

void effector_force_eval(const EffectorEvalInput *input, EffectorEvalResult *result, const EffectorEvalSettings *settings)
{
	EffectorPointRelation rel;
	float dir[3], strength;
	
	get_point_relation(&rel, input, settings);
	strength = settings->f_strength * get_falloff(settings, rel.dist_rel);
	
	normalize_v3_v3(dir, rel.loc_rel);
	mul_v3_v3fl(result->force, dir, strength);
}

void effector_wind_eval(const EffectorEvalInput *input, EffectorEvalResult *result, const EffectorEvalSettings *settings)
{
	EffectorPointRelation rel;
	float strength;
	
	get_point_relation(&rel, input, settings);
	strength = settings->f_strength * get_falloff(settings, rel.dist_rel);
	
	mul_v3_v3fl(result->force, rel.closest_nor, strength);
}
