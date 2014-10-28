vec3 calculate_view_space_normal(in vec3 viewposition)
{
    vec3 normal = cross(normalize(dFdx(viewposition)),
                        normalize(dFdy(viewposition)));
    normalize(normal);
    return normal;
}

#ifdef PERSP_MATRIX

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec3 viewvec_origin, in vec3 viewvec_diff, float depth)
{
    /* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
     * we change the factors from the article to fit the OpennGL model.  */

    float d = 2.0 * depth - 1.0;

    float zview = -gl_ProjectionMatrix[3][2] / (d + gl_ProjectionMatrix[2][2]);

    return zview * (viewvec_origin + vec3(uvcoords, 0.0) * viewvec_diff);
}

#else

vec3 get_view_space_from_depth(in vec2 uvcoords, in vec3 viewvec_origin, in vec3 viewvec_diff, float depth)
{
    /* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
     * we change the factors from the article to fit the OpennGL model.  */

    float d = 2.0 * depth - 1.0;
    vec3 offset = vec3(uvcoords, d);

    return vec3(viewvec_origin + offset * viewvec_diff);
}

#endif