#include "stdio.h"

#include "BJIT_forcefield.h"

void effector_force_eval(const EffectorEvalInput *input, EffectorEvalResult *result)
{
//	printf("Hello World! %f, %f, %f\n", input->loc[0], input->loc[1], input->loc[2]);
	
	result->force[0] = input->loc[0];
	result->force[1] = input->loc[1];
	result->force[2] = input->loc[2];
}
