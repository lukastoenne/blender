in uvec4 control_index;
in vec4 control_weight;

out uvec4 v_control_index;
out vec4 v_control_weight;

out vec3 vColor;

void main()
{
	gl_Position = gl_Vertex;
	
	v_control_index = control_index;
	v_control_weight = control_weight;
	
	vColor = vec3(1.0, 1.0, 1.0);
}
