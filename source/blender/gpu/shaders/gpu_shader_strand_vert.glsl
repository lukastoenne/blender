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

uniform usamplerBuffer control_curves;
uniform samplerBuffer control_points;
uniform samplerBuffer control_normals;
uniform samplerBuffer control_tangents;
uniform samplerBuffer fiber_position;
uniform usamplerBuffer fiber_control_index;
uniform samplerBuffer fiber_control_weight;


void interpolate_vertex(out vec3 loc, out vec3 nor, out vec3 target_loc, out mat3 target_frame)
{
	vec3 root = texelFetch(fiber_position, int(fiber_index)).xyz;
	uvec4 control_index = texelFetch(fiber_control_index, int(fiber_index));
	bvec4 control_valid = lessThan(control_index, uvec4(uint(0xFFFFFFFF)));
	vec4 control_weight = texelFetch(fiber_control_weight, int(fiber_index));

	loc = vec3(0.0, 0.0, 0.0);
	nor = vec3(0.0, 0.0, 0.0);
	vec3 cloc[4], cnor[4], ctang[4];
	int cnum_verts[4];
	for (int k = 0; k < 4; ++k) {
		if (!control_valid[k])
			continue;

		uvec2 curve = texelFetch(control_curves, int(control_index[k])).xy;
		int verts_begin = int(curve.x);
		cnum_verts[k] = int(curve.y);
		float segment = curve_param * float(cnum_verts[k]);

		vec3 croot = texelFetch(control_points, verts_begin).xyz;
		vec3 offset = root - croot;

		interpolate_control_curve(control_points, control_normals, control_tangents,
			                      segment, verts_begin, cnum_verts[k],
			                      cloc[k], cnor[k], ctang[k]);

		loc += control_weight[k] * (cloc[k] + offset);
		nor += control_weight[k] * cnor[k];
	}

	target_loc = cloc[0];
	target_frame = mat3_from_vectors(cnor[0], ctang[0]);
}

void main()
{
	vec3 loc, nor, target_loc;
	mat3 target_frame;
	interpolate_vertex(loc, nor, target_loc, target_frame);

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

	fPosition = mat3_emu(gl_ModelViewMatrix) * loc;
	fTangent = gl_NormalMatrix * nor;
	fColor = (nor + vec3(1.0)) * 0.5;
}

#endif
