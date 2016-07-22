#define M_PI 3.1415926535897932384626433832795

mat3 mat3_from_vectors(vec3 nor, vec3 tang)
{
	return mat3(tang, cross(nor, tang), nor);
}

mat4 mat4_combine(mat3 rotation, vec3 translation)
{
	return mat4(vec4(rotation[0], 1.0),
		        vec4(rotation[1], 1.0),
		        vec4(rotation[2], 1.0),
		        vec4(translation, 1.0));
}

struct Samplers {
	usamplerBuffer control_curves;
	samplerBuffer control_points;
	samplerBuffer control_normals;
	samplerBuffer control_tangents;
	samplerBuffer fiber_position;
	usamplerBuffer fiber_control_index;
	samplerBuffer fiber_control_weight;
};

uniform Samplers samplers;

void interpolate_control_curve(float segment, int verts_begin, int num_verts,
                               out vec3 loc, out vec3 nor, out vec3 tang)
{
	int idx0 = min(int(segment), num_verts - 2);
	int idx1 = idx0 + 1;
	
	vec3 loc0 = texelFetch(samplers.control_points, verts_begin + idx0).xyz;
	vec3 loc1 = texelFetch(samplers.control_points, verts_begin + idx1).xyz;
	vec3 nor0 = texelFetch(samplers.control_normals, verts_begin + idx0).xyz;
	vec3 nor1 = texelFetch(samplers.control_normals, verts_begin + idx1).xyz;
	vec3 tang0 = texelFetch(samplers.control_tangents, verts_begin + idx0).xyz;
	vec3 tang1 = texelFetch(samplers.control_tangents, verts_begin + idx1).xyz;

	float lambda = segment - float(idx0);
	loc = mix(loc0, loc1, lambda);
	nor = normalize(mix(nor0, nor1, lambda));
	tang = normalize(mix(tang0, tang1, lambda));
}

void interpolate_vertex(int fiber_index, float curve_param,
	                    out vec3 loc, out vec3 nor,
	                    out vec3 target_loc, out mat3 target_frame)
{
	vec3 root = texelFetch(samplers.fiber_position, fiber_index).xyz;
	uvec4 control_index = texelFetch(samplers.fiber_control_index, fiber_index);
	bvec4 control_valid = lessThan(control_index, uvec4(uint(0xFFFFFFFF)));
	vec4 control_weight = texelFetch(samplers.fiber_control_weight, fiber_index);

	loc = vec3(0.0, 0.0, 0.0);
	nor = vec3(0.0, 0.0, 0.0);
	vec3 cloc[4] = vec3[4]( vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0) );
	vec3 cnor[4] = vec3[4]( vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0) );
	vec3 ctang[4] = vec3[4]( vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0) );
	int cnum_verts[4] = int[4]( 0, 0, 0, 0 );
	for (int k = 0; k < 4; ++k) {
		if (!control_valid[k]) {
			continue;
		}

		uvec2 curve = texelFetch(samplers.control_curves, int(control_index[k])).xy;
		int verts_begin = int(curve.x);
		cnum_verts[k] = int(curve.y);
		float segment = curve_param * float(cnum_verts[k] - 1);

		vec3 croot = texelFetch(samplers.control_points, verts_begin).xyz;
		vec3 offset = root - croot;

		interpolate_control_curve(segment, verts_begin, cnum_verts[k],
			                      cloc[k], cnor[k], ctang[k]);

		loc += control_weight[k] * (cloc[k] + offset);
		nor += control_weight[k] * cnor[k];
	}

	target_loc = cloc[0];
	target_frame = mat3_from_vectors(cnor[0], ctang[0]);
}
