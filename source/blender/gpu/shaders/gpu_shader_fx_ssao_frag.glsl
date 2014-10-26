// amount of offset to move one pixel left-right
uniform vec2 screendim;
// color buffer
uniform sampler2D colorbuffer;

// jitter texture for ssao
uniform sampler2D jitter_tex;

// depth buffer
uniform sampler2D depthbuffer;
// coordinates on framebuffer in normalized (0.0-1.0) uv space
varying vec4 uvcoordsvar;

/* ssao_params.x : pixel scale for the ssao radious */
/* ssao_params.y : factor for the ssao darkening */
uniform vec4 ssao_params;
uniform vec4 ssao_sample_params;
uniform vec4 ssao_color;
uniform vec2 sample_directions[16];

/* store the view space vectors for the corners of the view frustum here. It helps to quickly reconstruct view space vectors
 * by using uv coordinates, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 ssao_viewvecs[3];

vec3 calculate_view_space_normal(in vec3 viewposition)
{
    vec3 normal = cross(normalize(dFdx(viewposition)),
                        normalize(dFdy(viewposition)));
    normalize(normal);
    return normal;
}

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

float calculate_ssao_factor(float depth)
{
    /* take the normalized ray direction here */
    vec2 rotX = texture2D(jitter_tex, uvcoordsvar.xy * ssao_sample_params.zw).rg;
    vec2 rotY = vec2(-rotX.y, rotX.x);

    /* occlusion is zero in full depth */
    if (depth == 1.0)
        return 0.0;

    vec3 position = get_view_space_from_depth(uvcoordsvar.xy, depth);
    vec3 normal = calculate_view_space_normal(position);

    // find the offset in screen space by multiplying a point in camera space at the depth of the point by the projection matrix.
    vec4 offset = gl_ProjectionMatrix * vec4(ssao_params.x, ssao_params.x, position.z, 1.0);
    float factor = 0.0;
    int x, y;
    int radial_samples = int(ssao_sample_params.y);
    int azimuth_samples = int(ssao_sample_params.x);
    /* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
    offset = (offset / offset.w) * 0.5;

    for (x = 0; x < azimuth_samples; x++) {
        vec2 dir_sample = sample_directions[x];

        /* rotate with random direction to get jittered result */
        vec2 dir_jittered = vec2(dot(dir_sample, rotX), dot(dir_sample, rotY));

        for (y = 0; y < radial_samples; y++) {
            vec2 uvcoords = uvcoordsvar.xy + dir_jittered * offset.xy * float(y + 1) / ssao_sample_params.y;

            float depth_new = texture2D(depthbuffer, uvcoords).r;
            if (depth_new != 1.0) {
                vec3 pos_new = get_view_space_from_depth(uvcoords, depth_new);
                vec3 dir = pos_new - position;
                float len = length(dir);
                float f = dot(dir, normal);

                /* use minor bias here to avoid self shadowing */
                if (f > 0.05 * len)
                    factor += f / len * 1.0/(1.0 + len * len * ssao_params.z);
            }
        }
    }

    factor /= (ssao_sample_params.y * ssao_sample_params.x);
    
    return clamp(factor * ssao_params.y, 0.0, 1.0);
}

void main()
{
    float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;
    vec4 color = mix(texture2D(colorbuffer, uvcoordsvar.xy), ssao_color, calculate_ssao_factor(depth));
    gl_FragColor = vec4(color.rgb, 1.0);
}
