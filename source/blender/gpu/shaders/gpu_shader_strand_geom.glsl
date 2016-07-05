#define MAX_CURVE_VERTS 64

layout(points) in;
layout(line_strip, max_vertices = MAX_CURVE_VERTS) out;

in vec3 vColor[];

in uvec4 v_control_index[];
in vec4 v_control_weight[];

out vec3 fColor;

uniform usamplerBuffer control_curves;
uniform samplerBuffer control_points;

bool is_valid_index(uint index)
{
	return index < uint(0xFFFFFFFF);
}

void emit_vertex(in vec4 loc)
{
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(loc.xyz, 1.0);
	EmitVertex();
}

void main()
{
	fColor = vColor[0];
	
	vec4 root = gl_in[0].gl_Position;
	
	emit_vertex(root);
	
	
	int index[4] = int[4](int(v_control_index[0].x),
	                      int(v_control_index[0].y),
	                      int(v_control_index[0].z),
	                      int(v_control_index[0].w));
	bool valid[4] = bool[4](is_valid_index(v_control_index[0].x),
		                    is_valid_index(v_control_index[0].y),
		                    is_valid_index(v_control_index[0].z),
		                    is_valid_index(v_control_index[0].w));
	float weight[4] = float[4](v_control_weight[0].x,
	                           v_control_weight[0].y,
	                           v_control_weight[0].z,
	                           v_control_weight[0].w);

	int cvert_begin[4];
	int num_cverts[4];
	float fnum_verts = 0.0;
	vec4 croot[4];
	vec4 offset[4];
	for (int k = 0; k < 4; ++k) {
		uvec4 curve = texelFetch(control_curves, index[k]);
		cvert_begin[k] = int(curve.x);
		num_cverts[k] = int(curve.y);
		
		fnum_verts += weight[k] * float(num_cverts[k]);
		
		croot[k] = texelFetch(control_points, cvert_begin[k]);
		offset[k] = root - croot[k];
	}
	int num_verts = max(int(ceil(fnum_verts)), 2);

	float dt[4];
	for (int k = 0; k < 4; ++k) {
		dt[k] = float(num_cverts[k] - 1) / float(num_verts - 1);
	}

	//fColor = vec3(0.0, 1.0, 0.0);
	emit_vertex(root);

	float t[4] = float[4](dt[0], dt[1], dt[2], dt[3]);
	for (int i = 1; i < num_verts; ++i) {
		vec4 loc = vec4(0.0, 0.0, 0.0, 1.0);

		for (int k = 0; k < 4; ++k) {
			if (!valid[k])
				continue;

			int ci0 = clamp(int(t[k]), 0, num_cverts[k] - 1);
			int ci1 = ci0 + 1;
			float lambda = t[k] - floor(t[k]);
			/* XXX could use texture filtering to do this for us? */
			vec4 cloc0 = texelFetch(control_points, cvert_begin[k] + ci0);
			vec4 cloc1 = texelFetch(control_points, cvert_begin[k] + ci1);
			vec4 cloc = mix(cloc0, cloc1, lambda) + offset[k];
			
			loc += weight[k] * cloc;
			t[k] += dt[k];
		}

		//fColor = vec3(float(i)/float(num_verts-1), 1.0-float(i)/float(num_verts-1), 0.0);
		//fColor = vec3(float(t[0]), 0.0, 0.0);
		emit_vertex(loc);
	}
	
	EndPrimitive();
}
