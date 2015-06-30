#include "stdio.h"

#include "BJIT_forcefield.h"

int effector_force_eval(EffectorEvalInput *input)
{
	printf("Hello World! %f, %f, %f\n", input->loc[0], input->loc[1], input->loc[2]);
	return 0;
}
