in uint fiber_index;
in float curve_param;

out float vCurveParam;
out vec3 vPosition;
out vec3 vOldPosition;
out vec3 vTangent;
out vec3 vOldTangent;
out vec3 vTargetPosition;
out mat3 vTargetFrame;
out vec3 vColor;

uniform float ribbon_width;

void main()
{
	vec3 loc, nor, target_loc;
	mat3 target_frame;
	interpolate_vertex(int(fiber_index), curve_param, loc, nor, target_loc, target_frame);

	// TODO define proper curve scale, independent of subdivision!
	vec3 oldloc = loc;
	vec3 oldnor = nor;
	displace_vertex(loc, nor, curve_param, 1.0, target_loc, target_frame);

	vCurveParam = curve_param;
	vPosition = loc;
	vOldPosition = oldloc;
	vTangent = nor;
	vOldTangent = oldnor;
	vTargetPosition = target_loc;
	vTargetFrame = target_frame;
	vColor = vec3(1,0,1);
}
