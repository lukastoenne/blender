#define M_PI 3.1415926535897932384626433832795

mat3 mat3_emu(mat4 m4) {
  return mat3(
      m4[0][0], m4[0][1], m4[0][2],
      m4[1][0], m4[1][1], m4[1][2],
      m4[2][0], m4[2][1], m4[2][2]);
}

mat3 mat3_from_vectors(vec3 nor, vec3 tang)
{
	return mat3(tang, cross(nor, tang), nor);
}

void interpolate_control_curve(in samplerBuffer control_points,
                      	       in samplerBuffer control_normals,
                               in samplerBuffer control_tangents,
                               in float segment, in int verts_begin, in int num_verts,
                               out vec3 loc, out vec3 nor, out vec3 tang)
{
	int idx0 = min(int(segment), num_verts - 2);
	int idx1 = idx0 + 1;
	
	vec3 loc0 = texelFetch(control_points, verts_begin + idx0).xyz;
	vec3 loc1 = texelFetch(control_points, verts_begin + idx1).xyz;
	vec3 nor0 = texelFetch(control_normals, verts_begin + idx0).xyz;
	vec3 nor1 = texelFetch(control_normals, verts_begin + idx1).xyz;
	vec3 tang0 = texelFetch(control_tangents, verts_begin + idx0).xyz;
	vec3 tang1 = texelFetch(control_tangents, verts_begin + idx1).xyz;

	float lambda = segment - float(idx0);
	loc = mix(loc0, loc1, lambda);
	nor = normalize(mix(nor0, nor1, lambda));
	tang = normalize(mix(tang0, tang1, lambda));
}
