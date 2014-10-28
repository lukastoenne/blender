vec3 calculate_view_space_normal(in vec3 viewposition)
{
    vec3 normal = cross(normalize(dFdx(viewposition)),
                        normalize(dFdy(viewposition)));
    normalize(normal);
    return normal;
}

#ifdef PERSP_MATRIX

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec2 viewvec_origin, in vec2 viewvec_diff, float depth)
{
    /* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
     * we change the factors from the article to fit the OpennGL model.  */

    float d = 2.0 * depth - 1.0;

    float zview = -gl_ProjectionMatrix[3][2] / (d + gl_ProjectionMatrix[2][2]);

    return vec3(zview * (viewvec_origin + uvcoords * viewvec_diff), zview);
}

#else

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec2 viewvec_origin, in vec2 viewvec_diff, float depth)
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

#endif