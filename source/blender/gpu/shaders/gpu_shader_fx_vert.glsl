//texture coordinates for framebuffer read
varying vec4 uvcoordsvar;

//very simple shader for gull screen FX, just pass values on
void main()
{
	uvcoordsvar = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
}
