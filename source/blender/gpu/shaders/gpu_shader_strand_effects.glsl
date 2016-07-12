uniform float clumping_factor;
uniform float clumping_shape;
uniform float curl_shape;

/* Hairs tend to stick together and run in parallel.
 * The effect increases with distance from the root,
 * as the stresses pulling fibers apart decrease.
 */
void clumping(inout vec3 location, in float t, vec3 offset, mat3 rotation, in vec2 root_distance)
{
	float taper = pow(t, 1.0 / clumping_shape);
	float factor = taper * clumping_factor;
	location -= offset * factor;
}

/* Hair often don't have a circular cross section, but are somewhat flattened.
 * This creates the local bending which results in the typical curly hair geometry.
 */ 
void curl(inout vec3 location, in float t, vec3 offset, mat3 rotation, in vec2 root_distance)
{
	float factor = pow(t, 1.0 / curl_shape);
	//location -= rotation * vec3(root_distance.xy, 0.0) * taper * ;
}

void displace_vertex(inout vec3 location, in float t, vec3 offset, mat3 rotation, in vec2 root_distance)
{
#ifdef USE_EFFECT_CLUMPING
	clumping(location, t, offset, rotation, root_distance);
#endif
#ifdef USE_EFFECT_CURL
	curl(location, t, offset, rotation, root_distance);
#endif
}
