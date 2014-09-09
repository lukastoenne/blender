// amount of offset to move one pixel left-right
uniform vec2 screendim;
// color buffer
uniform sampler2D colorbuffer;
//blurred color buffer for DOF effect
uniform sampler2D blurredcolorbuffer;

// depth buffer
uniform sampler2D depthbuffer;
// coordinates on framebuffer in normalized (0.0-1.0) uv space
varying vec4 framecoords;

// this includes focal distance in x and aperture size in y
uniform vec2 dof_params; 

#define NUM_SAMPLES 5

void calculate_ss_normal (in float depth, out vec4 color)
{
	vec2 offset_co = vec2(1.0/screendim.x, 1.0/screendim.y);
	float depthx = texture2D(depthbuffer, framecoords.xy + vec2(offset_co.x, 0.0)).r;
	float depthy = texture2D(depthbuffer, framecoords.xy + vec2(0.0, offset_co.y)).r;

	// This is essentially a cross product multiplied screendim.x * screendim.y
	// If we were to use normal multiplication then we could easily get out of half-float range
	vec3 normal = vec3(screendim.y*(depthx - depth), screendim.x*(depthy - depth), 1.0);
	normalize(normal);
	normal = normal * 0.5 + vec3(0.5);
	color = vec4(normal, 1.0);
}


void main()
{
	float depth = texture2D(depthbuffer, framecoords.xy).r;
	
	//First we need to calculate the view space distance from the shader inputs
	//This will unfortunately depend on the precision of the depth buffer which is not linear
	vec2 norm_scr = framecoords.xy * 2.0 - 1.0;
	vec4 viewposition = vec4(norm_scr.x, norm_scr.y, depth, 1.0);

	// convert to view space now
	viewposition = gl_ProjectionMatrixInverse * viewposition;
	viewposition = viewposition / viewposition.w;

	//vec3 normal = cross(dFdx(viewposition.xyz), dFdy(viewposition.xyz));
	//normalize(normal);
	//normal.z = -normal.z;
	//normal = normal * 0.5 + vec3(0.5);

        //calculate circle of confusion
	float dist = length(viewposition);
	float coc = dof_params.x * abs(1.0 - dof_params.y / dist);

        coc = clamp(coc, 0.0, 1.0);
        
        // blend between blurred-non blurred images based on coc	
        vec4 color = coc * vec4(1.0, 0.0, 0.0, 1.0) * texture2D(blurredcolorbuffer, framecoords.xy) +
                (1.0 - coc) * vec4(0.0, 1.0, 0.0, 1.0) * texture2D(colorbuffer, framecoords.xy);

 	gl_FragColor = vec4(color.xyz, 1.0);
}
