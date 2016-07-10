in vec3 normal;
in vec3 tangent;
in uvec4 control_index;
in vec4 control_weight;
in vec2 root_distance;

out mat3 vRotation;
out uvec4 v_control_index;
out vec4 v_control_weight;
out vec3 vColor;

void main()
{
	gl_Position = gl_Vertex;
	vRotation = mat3(tangent, cross(normal, tangent), normal);
	v_control_index = control_index;
	v_control_weight = control_weight;
	vColor = vec3(1.0, 1.0, 1.0);
}
