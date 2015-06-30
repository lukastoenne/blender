#include "stdio.h"

typedef struct EffectorEvalInput {
	float loc[3];
	float vel[3];
} EffectorEvalInput;

int effector_force(EffectorEvalInput *input)
{
	printf("Hello World! %f, %f, %f\n", input->loc[0], input->loc[1], input->loc[2]);
	return 0;
}
