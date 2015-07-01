#include "stdio.h"
#include "math.h"

#include <stdbool.h>

#include "BJIT_forcefield.h"

/* maxdist: zero effect from this distance outwards (if usemax) */
/* mindist: full effect up to this distance (if usemin) */
/* power: falloff with formula 1/r^power */
static float falloff_old(float fac, bool usemin, float mindist, bool usemax, float maxdist, float power)
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

static inline void mul_v3_m4v3(float r[3], float mat[4][4], const float vec[3])
{
	const float x = vec[0];
	const float y = vec[1];

	r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
	r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
	r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}

void effector_force_eval(const EffectorEvalInput *input, EffectorEvalResult *result, const EffectorEvalSettings *settings)
{
	float loc[3];
	
	mul_v3_m4v3(loc, settings->itfm, input->loc);
	
	result->force[0] = loc[0];
	result->force[1] = loc[1];
	result->force[2] = loc[2];
}
