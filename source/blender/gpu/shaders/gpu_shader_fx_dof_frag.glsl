// amount of offset to move one pixel left-right
uniform vec2 invrendertargetdim;
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

/* color texture coordinates, offset by a small amount */
varying vec2 color_uv1;
varying vec2 color_uv2;

varying vec2 depth_uv1;
varying vec2 depth_uv2;
varying vec2 depth_uv3;
varying vec2 depth_uv4;

float calculate_dof_coc(in float zdepth)
{
	float coc = dof_params.x * abs(1.0 - dof_params.y / zdepth);

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

/* near coc only! when distance is nearer than focus plane first term is bigger than one */
vec4 calculate_near_coc(in vec4 zdepth)
{
	vec4 coc = dof_params.x * max(vec4(dof_params.y) / zdepth - vec4(1.0), vec4(0.0));

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

/* first pass blurs the color buffer heavily and gets the near coc only. There are too many texture accesses here but they are done on a
 * lower resolution image */
void first_pass()
{
	vec4 depth;
	vec4 zdepth;
	vec4 coc;
	float final_coc;

	/* amount to add to uvs so that they move one row further */
	vec2 offset_row[3];
	offset_row[0] = vec2(0.0, invrendertargetdim.y);
	offset_row[1] = 2 * offset_row[0];
	offset_row[2] = 3 * offset_row[0];

	/* heavily blur the image */
	vec4 color = texture2D(colorbuffer, color_uv1);
	color += texture2D(colorbuffer, color_uv1 + offset_row[1]);
	color += texture2D(colorbuffer, color_uv2);
	color += texture2D(colorbuffer, color_uv2 + offset_row[1]);
	color /= 4.0;

	depth.r = texture2D(depthbuffer, depth_uv1).r;
	depth.g = texture2D(depthbuffer, depth_uv2).r;
	depth.b = texture2D(depthbuffer, depth_uv3).r;
	depth.a = texture2D(depthbuffer, depth_uv4).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = calculate_near_coc(zdepth);

	depth.r = texture2D(depthbuffer, depth_uv1 + offset_row[0]).r;
	depth.g = texture2D(depthbuffer, depth_uv2 + offset_row[0]).r;
	depth.b = texture2D(depthbuffer, depth_uv3 + offset_row[0]).r;
	depth.a = texture2D(depthbuffer, depth_uv4 + offset_row[0]).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = max(calculate_near_coc(zdepth), coc);

	depth.r = texture2D(depthbuffer, depth_uv1 + offset_row[1]).r;
	depth.g = texture2D(depthbuffer, depth_uv2 + offset_row[1]).r;
	depth.b = texture2D(depthbuffer, depth_uv3 + offset_row[1]).r;
	depth.a = texture2D(depthbuffer, depth_uv4 + offset_row[1]).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = max(calculate_near_coc(zdepth), coc);

	depth.r = texture2D(depthbuffer, depth_uv1 + offset_row[2]).r;
	depth.g = texture2D(depthbuffer, depth_uv2 + offset_row[2]).r;
	depth.b = texture2D(depthbuffer, depth_uv3 + offset_row[2]).r;
	depth.a = texture2D(depthbuffer, depth_uv4 + offset_row[2]).r;

	zdepth = get_view_space_z_from_depth(vec4(viewvecs[0].z), vec4(viewvecs[1].z), depth);
	coc = max(calculate_near_coc(zdepth), coc);

	final_coc = max(max(coc.x, coc.y), max(coc.z, coc.w));
	gl_FragColor = vec4(color.rgb, final_coc);
}


/* second pass, blur the near coc texture and calculate the coc */
void second_pass()
{
	vec4 color =  texture2D(colorbuffer, uvcoordsvar.xz);
	color += texture2D(colorbuffer, uvcoordsvar.yz);
	color += texture2D(colorbuffer, uvcoordsvar.xw);
	color += texture2D(colorbuffer, uvcoordsvar.yw);

	gl_FragColor = color/4.0;
}

/* second pass, just visualize the first pass contents */
void third_pass()
{
	vec4 color = texture2D(colorbuffer, uvcoordsvar.xy);

	gl_FragColor = vec4(color.a, color.a, color.a, 1.0);
}

void main()
{
#ifdef FIRST_PASS
	first_pass();
#elif defined(SECOND_PASS)
	second_pass();
#elif defined(THIRD_PASS)
	third_pass();
#elif defined(FOURTH_PASS)
#endif
}
