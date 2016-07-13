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
		nnor = normalize(nor * (1.0 - factor)
			   + (target_loc - loc) * factor / (t * clump_shape));
	}
	else
		nnor = nor;

	loc = nloc;
	nor = nnor;
}

/* Hairs often don't have a circular cross section, but are somewhat flattened.
 * This creates the local bending which results in the typical curly hair geometry.
 */ 
void curl(inout vec3 loc, inout vec3 nor, in float t, in float tscale, in vec3 target_loc, in mat3 target_frame)
{
	float taper = pow(t, 1.0 / curl_shape);
	float factor = (1.0 - curl_thickness) * taper;

	float angle = 2.0*M_PI * t * tscale / curl_length;
	vec3 curlvec = target_frame * vec3(cos(angle), sin(angle), 0.0) * curl_radius;
	
	vec3 nloc = mix(loc, target_loc + curlvec, factor);
	vec3 nnor;
	if (t > 0.0) {
		nnor = normalize(nor * (1.0 - factor)
			   + vec3(-curlvec.y, curlvec.x, 0.0) * 2.0*M_PI * tscale / curl_length
			   + (target_loc + curlvec - loc) * factor / (t * clump_shape));
	}
	else
		nnor = nor;

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
