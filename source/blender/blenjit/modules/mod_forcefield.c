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

void effector_force_eval(const EffectorEvalInput *input, EffectorEvalResult *result, const EffectorEvalSettings *settings)
{
	result->force[0] = input->loc[0];
	result->force[1] = input->loc[1];
	result->force[2] = input->loc[2];
}
