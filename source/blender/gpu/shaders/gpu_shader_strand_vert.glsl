#ifdef USE_GEOMSHADER

in vec3 normal;
in vec3 tangent;
in uvec4 control_index;
in vec4 control_weight;
in vec2 root_distance;

out mat3 vRotation;
out uvec4 v_control_index;
out vec4 v_control_weight;
out vec2 v_root_distance;
out vec3 vColor;

void main()
{
	gl_Position = gl_Vertex;
	vRotation = mat3(tangent, cross(normal, tangent), normal);
	v_control_index = control_index;
	v_control_weight = control_weight;
	v_root_distance = root_distance;
	vColor = vec3(1.0, 1.0, 1.0);
}

#else

in uint fiber_index;
in float curve_param;

out vec3 fPosition;
out vec3 fTangent;
out vec3 fColor;

uniform float ribbon_width;

void main()
{
	vec3 loc, nor, target_loc;
	mat3 target_frame;
	interpolate_vertex(int(fiber_index), curve_param, loc, nor, target_loc, target_frame);

	// TODO define proper curve scale, independent of subdivision!
	displace_vertex(loc, nor, curve_param, 1.0, target_loc, target_frame);

	vec4 co = gl_ModelViewMatrix * vec4(loc, 1.0);
#ifdef FIBER_RIBBON
	vec4 view_nor = gl_ModelViewMatrix * vec4(nor, 0.0);
	//vec2 view_offset = normalize(cross(vec3(0.0, 0.0, 1.0), view_nor.xyz).xy);
	vec2 view_offset = normalize(vec2(view_nor.y, -view_nor.x));
	co += vec4((float(gl_VertexID % 2) - 0.5) * ribbon_width * view_offset, 0.0, 0.0);
#endif
	gl_Position = gl_ProjectionMatrix * co;

	fPosition = co.xyz;
	fTangent = gl_NormalMatrix * nor;
	fColor = (nor + vec3(1.0)) * 0.5;
}

#endif
