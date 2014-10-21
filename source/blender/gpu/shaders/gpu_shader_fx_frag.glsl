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

/* store the view space vectors for the corners of the view frustum here. It helps to quickly reconstruct view space vectors
 * by using uv coordinates, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 ssao_viewvecs[3];

#define NUM_SAMPLES 8

vec3 calculate_view_space_normal(in vec3 viewposition)
{
    vec3 normal = cross(normalize(dFdx(viewposition)),
                        normalize(dFdy(viewposition)));
    normalize(normal);
    return normal;
}

vec3 calculate_view_space_position(in vec2 uvcoords, float depth)
{	
    //First we need to calculate the view space distance from the shader inputs
    //This will unfortunately depend on the precision of the depth buffer which is not linear
    vec4 viewposition = vec4(uvcoords.x, uvcoords.y, depth, 1.0);
    viewposition = viewposition * 2.0 - 1.0;

    // convert to view space now
    viewposition = gl_ProjectionMatrixInverse * viewposition;
    viewposition = viewposition / viewposition.w;
    
    return viewposition.xyz;
}

vec3 get_view_space_from_depth(in vec2 uvcoords, float depth)
{
    /* convert depth to non-normalized range */
    float d = 2.0 * depth - 1.0;

    /* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
    float zview = gl_ProjectionMatrix[2][3] / (d - gl_ProjectionMatrix[2][2]);

    vec3 pos = vec3(zview * (ssao_viewvecs[0].xy + uvcoords * ssao_viewvecs[1].xy), zview);

    return pos;
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

    vec3 position = get_view_space_from_depth(framecoords.xy, depth);
    vec3 normal = calculate_view_space_normal(position);

    // find the offset in screen space by multiplying a point in camera space at the depth of the point by the projection matrix.
    vec4 offset = (gl_ProjectionMatrix * vec4(ssao_params.x, ssao_params.x, position.z, 1.0));
    float factor = 0.0;
    int x, y;
    
    /* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
    offset = (offset / offset.w) * 0.5 + vec4(0.5);

    for (x = 0; x < NUM_SAMPLES; x++) {
        for (y = 0; y < NUM_SAMPLES; y++) {
            vec2 uvcoords = framecoords.xy + (vec2(x,y) - vec2(4.0)) * offset.xy;
	
            float depth = texture2D(depthbuffer, uvcoords).r;
            if (depth != 1.0) {
                vec3 pos_new = get_view_space_from_depth(uvcoords, depth);
                vec3 dir = pos_new - position;
                float len = length(dir);
                factor += max(dot(dir, normal) * max(1.0 - len / ssao_params.x, 0.0), 0.0);
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

    vec3 position = get_view_space_from_depth(framecoords.xy, depth);
    vec3 normal = calculate_view_space_normal(position);

//    vec4 color = mix(texture2D(colorbuffer, framecoords.xy), ssao_color, calculate_ssao_factor(depth));
//    gl_FragColor = vec4(color.xyz, 1.0);
    gl_FragColor = vec4(normal * 0.5 + vec3(0.5), 1.0);
    gl_FragColor = vec4(position, 1.0);
}
