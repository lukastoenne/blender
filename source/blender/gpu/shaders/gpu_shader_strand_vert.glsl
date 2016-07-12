#ifdef USE_GEOMSHADER
in vec3 normal;
in vec3 tangent;
in uvec4 control_index;
in vec4 control_weight;
in vec2 root_distance;
#else
in uint fiber_index;
in float curve_param;
#endif

#ifdef USE_GEOMSHADER
out mat3 vRotation;
out uvec4 v_control_index;
out vec4 v_control_weight;
out vec2 v_root_distance;
out vec3 vColor;
#else
out vec3 fPosition;
out vec3 fTangent;
out vec3 fColor;

uniform usamplerBuffer control_curves;
uniform samplerBuffer control_points;
uniform samplerBuffer control_normals;
uniform samplerBuffer control_tangents;
uniform samplerBuffer fiber_position;
uniform samplerBuffer fiber_normal;
uniform samplerBuffer fiber_tangent;
uniform usamplerBuffer fiber_control_index;
uniform samplerBuffer fiber_control_weight;
uniform samplerBuffer fiber_root_distance;
#endif

void main()
{
#ifdef USE_GEOMSHADER
	gl_Position = gl_Vertex;
	vRotation = mat3(tangent, cross(normal, tangent), normal);
	v_control_index = control_index;
	v_control_weight = control_weight;
	v_root_distance = root_distance;
	vColor = vec3(1.0, 1.0, 1.0);
#else
	vec3 root = texelFetch(fiber_position, int(fiber_index)).xyz;
	uvec4 control_index = texelFetch(fiber_control_index, int(fiber_index));
	bvec4 control_valid = lessThan(control_index, uvec4(uint(0xFFFFFFFF)));
	vec4 control_weight = texelFetch(fiber_control_weight, int(fiber_index));

	vec3 loc = vec3(0.0, 0.0, 0.0);
	vec3 tangent = vec3(0.0, 0.0, 0.0);
	vec3 offset0;
	for (int k = 0; k < 4; ++k) {
		if (!control_valid[k])
			continue;

		uvec2 curve = texelFetch(control_curves, int(control_index[k])).xy;
		int verts_begin = int(curve.x);
		int num_verts = int(curve.y);

		vec3 croot = texelFetch(control_points, verts_begin).xyz;
		vec3 offset = root - croot;
		if (k == 0)
			offset0 = offset;

		float segment = curve_param * float(num_verts);
		int idx0 = min(int(segment), num_verts - 2);
		int idx1 = idx0 + 1;
		float lambda = segment - float(idx0);
		vec3 cloc0 = texelFetch(control_points, verts_begin + idx0).xyz;
		vec3 cloc1 = texelFetch(control_points, verts_begin + idx1).xyz;
		vec3 cloc = mix(cloc0, cloc1, lambda) + offset;

		loc += control_weight[k] * cloc;
		tangent += control_weight[k] * (cloc1 - cloc0);
	}

	displace_vertex(loc, curve_param, offset0, mat3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0), vec2(0.0, 0.0));

	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(loc, 1.0);
	fPosition = mat3_emu(gl_ModelViewMatrix) * loc;
	fTangent = gl_NormalMatrix * tangent;
	fColor = vec3(curve_param, 1.0 - curve_param, 0.0);
#endif
}
