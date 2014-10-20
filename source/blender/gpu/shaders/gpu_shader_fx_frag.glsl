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

/* ssao_params.x : pixel scale for the ssao radious */
/* ssao_params.y : factor for the ssao darkening */
uniform vec4 ssao_params;
uniform vec4 ssao_color;

#define NUM_SAMPLES 8

vec3 calculate_view_space_normal(in vec4 viewposition)
{
    vec3 normal = cross(normalize(dFdx(viewposition.xyz)), 
                        normalize(dFdy(viewposition.xyz)));
    normalize(normal);
    return normal;
}

vec4 calculate_view_space_position(in vec2 uvcoords, float depth)
{	
    //First we need to calculate the view space distance from the shader inputs
    //This will unfortunately depend on the precision of the depth buffer which is not linear
    vec4 viewposition = vec4(uvcoords.x, uvcoords.y, depth, 1.0);
    viewposition = viewposition * 2.0 - 1.0;

    // convert to view space now
    viewposition = gl_ProjectionMatrixInverse * viewposition;
    viewposition = viewposition / viewposition.w;
    
    return viewposition;
}

float calculate_dof_coc(in vec4 viewposition, inout vec3 normal)
{
    float dist = length(viewposition);
    float coc = dof_params.x * abs(1.0 - dof_params.y / dist);
    
    coc = clamp(coc, 0.0, 1.0);
    
    return coc;
}


float calculate_ssao_factor(float depth)
{
    /* occlusion is zero in full depth */
    if (depth == 1.0)
        return 0.0;

    vec4 position = calculate_view_space_position(framecoords.xy, depth);
    vec3 normal = calculate_view_space_normal(position);

    // divide by distance to camera to make the effect independent
    vec2 offset = (ssao_params.x / position.z) * vec2(1.0/screendim.x, 1.0/screendim.y);
    float factor = 0.0;
    int x, y;
    
    for (x = 0; x < NUM_SAMPLES; x++) {
        for (y = 0; y < NUM_SAMPLES; y++) {
            vec2 uvcoords = framecoords.xy + (vec2(x,y) - vec2(4.0)) * offset;
	
            float depth = texture2D(depthbuffer, uvcoords).r;
            if (depth != 1.0) {
                vec4 pos_new = calculate_view_space_position(uvcoords, depth);
                vec3 dir = vec3(pos_new.xyz) - vec3(position.xyz);
                float len = length(dir);
                factor += max(dot(dir, normal) * (1.0/(1.0 + len * len * ssao_params.z)), 0.0);
            }
        }
    }

    factor /= float(NUM_SAMPLES * NUM_SAMPLES);    
    
    return max(0.0, factor * ssao_params.y);
}

void main()
{
    float depth = texture2D(depthbuffer, framecoords.xy).r;
    
    //vec3 color = normal * 0.5 + vec3(0.5);
    
    // blend between blurred-non blurred images based on coc	
    //vec4 color = coc * texture2D(blurredcolorbuffer, framecoords.xy) +
    //       (1.0 - coc) * texture2D(colorbuffer, framecoords.xy);
    
    vec4 color = mix(texture2D(colorbuffer, framecoords.xy), ssao_color, calculate_ssao_factor(depth)); 
    gl_FragColor = vec4(color.xyz, 1.0);
}
