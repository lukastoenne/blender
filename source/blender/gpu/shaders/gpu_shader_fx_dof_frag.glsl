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

uniform vec4 viewvecs[3];

float calculate_dof_coc(in vec3 viewposition)
{
    float dist = length(viewposition);
    float coc = dof_params.x * abs(1.0 - dof_params.y / dist);
    
    /* multiply by 1.0 / sensor size to get the normalized size */
    return coc * dof_params.z;
}

/* first pass blurs the color buffer heavily and gets the near coc only. There are too many texture accesses here but they are done on a
 * lower resolution image */
void first_pass()
{
    float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;
    vec4 color = texture2D(colorbuffer, uvcoordsvar.xy);

    vec3 position = get_view_space_from_depth(uvcoordsvar.xy, viewvecs[0].xyz, viewvecs[1].xyz, depth);
    float coc = calculate_dof_coc(position);

    gl_FragColor = vec4(coc * color.rgb, 1.0);
}


/* second pass, just visualize the first pass contents */
void second_pass()
{
    vec4 color = texture2D(colorbuffer, uvcoordsvar.xy);

    gl_FragColor = vec4(color.a * color.rgb, 1.0);
}


void main()
{
#ifdef FIRST_PASS
    first_pass();
#elif defined(SECOND_PASS)
    second_pass();
#elif defined(THIRD_PASS)
#elif defined(FOURTH_PASS)
#endif
}
