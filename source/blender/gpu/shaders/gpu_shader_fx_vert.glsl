//texture coordinates for framebuffer read
varying vec4 framecoords;

//very simple shader for gull screen FX, just pass values on
void main()
{
	framecoords = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
}
