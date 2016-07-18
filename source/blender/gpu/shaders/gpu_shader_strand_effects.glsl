uniform int debug_value;

uniform float clump_thickness;
uniform float clump_shape;
uniform float curl_thickness;
uniform float curl_shape;
uniform float curl_radius;
uniform float curl_length;

/* Note: The deformer functions below calculate a new location vector
 * as well as a new direction (aka "normal"), using the partial derivatives of the transformation.
 * 
 * Each transformation function can depend on the location L as well as the curve parameter t:
 *
 *         Lnew = f(L, t)
 *  => dLnew/dt = del f/del L * dL/dt + del f/del t
 *
 * The first term is the Jacobian of the function f, dL/dt is the original direction vector.
 * Some more information can be found here:
 * http://http.developer.nvidia.com/GPUGems/gpugems_ch42.html
 */

/* Hairs tend to stick together and run in parallel.
 * The effect increases with distance from the root,
 * as the stresses pulling fibers apart decrease.
 */
void clumping(inout vec3 loc, inout vec3 nor, in float t, in float tscale, in vec3 target_loc, in mat3 target_frame)
{
	float taper = pow(t, 1.0 / clump_shape);
	float factor = (1.0 - clump_thickness) * taper;
	
	vec3 nloc = mix(loc, target_loc, factor);
	vec3 nnor;
	if (t > 0.0) {
		nnor = normalize(mix(nor, (target_loc - loc) / (t * clump_shape), factor));
	}
	else
		nnor = nor;

	loc = nloc;
	nor = nnor;
}

struct CurlParams {
	float taper, dtaper;
	float factor, dfactor;
	float turns;
	float angle, dangle;
	vec3 curlvec, dcurlvec;
};

CurlParams curl_params(float t, float tscale, vec3 target_loc, mat3 target_frame)
{
	CurlParams params;
	
	params.taper = pow(t, 1.0 / curl_shape);
	params.dtaper = (t > 0.0) ? params.taper / (t * curl_shape) : 0.0;
	params.factor = (1.0 - curl_thickness) * params.taper;
	params.dfactor = (1.0 - curl_thickness) * params.dtaper;

	params.turns = 2.0*M_PI * tscale / curl_length;
	params.angle = t * params.turns;
	params.dangle = params.turns;
	
	params.curlvec = target_frame * vec3(cos(params.angle), sin(params.angle), 0.0) * curl_radius;
	params.dcurlvec = target_frame * vec3(-sin(params.angle), cos(params.angle), 0.0) * curl_radius;

	return params;
}

/* Hairs often don't have a circular cross section, but are somewhat flattened.
 * This creates the local bending which results in the typical curly hair geometry.
 */ 
void curl(inout vec3 loc, inout vec3 nor, in float t, in float tscale, in vec3 target_loc, in mat3 target_frame)
{
	CurlParams params = curl_params(t, tscale, target_loc, target_frame);

	vec3 dloc = (target_loc + params.curlvec - loc) * params.factor;
	vec3 nloc = loc + dloc;
	vec3 dshape = dloc / (t * curl_shape);
	vec3 dtarget = target_frame[2] + params.turns * params.dcurlvec;
	vec3 nnor = normalize(dshape + mix(nor, dtarget, params.factor));

	loc = nloc;
	nor = nnor;
}

void displace_vertex(inout vec3 loc, inout vec3 nor, in float t, in float tscale, in vec3 target_loc, in mat3 target_frame)
{
#ifdef USE_EFFECT_CLUMPING
	clumping(loc, nor, t, tscale, target_loc, target_frame);
#endif
#ifdef USE_EFFECT_CURL
	curl(loc, nor, t, tscale, target_loc, target_frame);
#endif
}
