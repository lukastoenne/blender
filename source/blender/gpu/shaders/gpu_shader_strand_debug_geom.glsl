#define MAX_CURVE_VERTS 8

layout(points) in;
layout(line_strip, max_vertices = MAX_CURVE_VERTS) out;

uniform float debug_scale;

in vec3 vPosition[];
in vec3 vTangent[];
in vec3 vTargetPosition[];
in mat3 vTargetFrame[];
in vec3 vColor[];

out vec3 fColor;

void emit_vertex(vec3 location, vec3 color)
{
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(location, 1.0);
	fColor = color;
	EmitVertex();
}

void main()
{
	fColor = vColor[0];

	vec3 co = vPosition[0].xyz;

	emit_vertex(co, vec3(1.0, 0.0, 1.0));
	emit_vertex(co + vTangent[0] * debug_scale, vec3(1.0, 0.0, 1.0));
	EndPrimitive();

	vec3 target_loc = vTargetPosition[0];
	mat3 target_frame = vTargetFrame[0];
	vec3 red = vec3(1, 0, 0);
	vec3 green = vec3(0, 1, 0);
	vec3 blue = vec3(0, 0, 1);
	emit_vertex(target_loc, red);
	emit_vertex(target_loc + target_frame[0] * debug_scale, red);
	EndPrimitive();
	emit_vertex(target_loc, green);
	emit_vertex(target_loc + target_frame[1] * debug_scale, green);
	EndPrimitive();
	emit_vertex(target_loc, blue);
	emit_vertex(target_loc + target_frame[2] * debug_scale, blue);
	EndPrimitive();
}
