// amount of offset to move one pixel left-right
uniform vec2 screendim;
// color buffer
uniform sampler2D colorbuffer;
//blurred color buffer for DOF effect
uniform sampler2D blurredcolorbuffer;

// depth buffer
uniform sampler2D depthbuffer;
// coordinates on framebuffer in normalized (0.0-1.0) uv space
varying vec4 uvcoordsvar;

// this includes focal distance in x and aperture size in y
uniform vec4 dof_params;

/* projective matrix version */
vec3 get_view_space_from_depth(in vec2 uvcoords, float depth)
{
    /* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
     * we change the factors from the article to fit the OpennGL model.
    float d = 2.0 * depth - 1.0;

    float zview = -gl_ProjectionMatrix[2][3] / (d + gl_ProjectionMatrix[2][2]);

    vec3 pos = vec3(zview * (ssao_viewvecs[0].xy + uvcoords * ssao_viewvecs[1].xy), zview);
    */
    vec4 pos = 2.0 * vec4(uvcoords.xy, depth, 1.0) - vec4(1.0);

    pos = gl_ProjectionMatrixInverse * pos;
    pos /= pos.w;

    return pos.xyz;
}

float calculate_dof_coc(in vec3 viewposition)
{
    float dist = length(viewposition);
    float coc = dof_params.x * abs(1.0 - dof_params.y / dist);
    
    /* multiply by 1.0 / sensor size to get the normalized size */
    return coc * dof_params.z;
}

void main()
{
    float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;

    vec3 position = get_view_space_from_depth(uvcoordsvar.xy, depth);
    float coc = calculate_dof_coc(position);

    gl_FragColor = vec4(coc, coc, coc, 1.0);
}
