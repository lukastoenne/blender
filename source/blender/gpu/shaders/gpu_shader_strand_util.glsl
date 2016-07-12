mat3 mat3_emu(mat4 m4) {
  return mat3(
      m4[0][0], m4[0][1], m4[0][2],
      m4[1][0], m4[1][1], m4[1][2],
      m4[2][0], m4[2][1], m4[2][2]);
}

void interpolate_control_curve(in samplerBuffer control_points,
	                           in samplerBuffer control_normals,
	                           in samplerBuffer control_tangents,
	                           in float param, in int verts_begin, in int num_verts,
	                           out vec3 loc, out vec3 tangent, out mat3 frame)
{
	int idx0 = min(int(param), num_verts - 2);
	int idx1 = idx0 + 1;
	float lambda = param - float(idx0);
	vec3 loc0 = texelFetch(control_points, verts_begin + idx0).xyz;
	vec3 loc1 = texelFetch(control_points, verts_begin + idx1).xyz;
	
	/* XXX could use texture filtering to do this for us? */
	loc = mix(loc0, loc1, lambda);
	tangent = loc1 - loc0;
	frame = mat3(1.0, 0.0, 0.0,
		         0.0, 1.0, 0.0,
		         0.0, 0.0, 1.0);
}
