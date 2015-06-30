#include "stdio.h"

#include "BJIT_forcefield.h"

void effector_force_eval(const EffectorEvalInput *input, EffectorEvalResult *result)
{
	printf("Hello World! %f, %f, %f\n", input->loc[0], input->loc[1], input->loc[2]);
	
	result->force[0] = 6.44f;
	result->force[1] = -9.0f;
	result->force[2] = 0.123f;
}
