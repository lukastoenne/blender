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

float calculate_dof_coc(in float zdepth)
{
	float coc = dof_params.x * abs(1.0 - dof_params.y / zdepth);

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

/* near coc only! when distance is nearer than focus plane first term is bigger than one */
float calculate_near_coc(in float zdepth)
{
	float coc = dof_params.x * max(dof_params.y / zdepth - 1.0, 0.0);

	/* multiply by 1.0 / sensor size to get the normalized size */
	return coc * dof_params.z;
}

/* first pass blurs the color buffer heavily and gets the near coc only. There are too many texture accesses here but they are done on a
 * lower resolution image */
void first_pass()
{
	float depth = texture2D(depthbuffer, uvcoordsvar.xy).r;

	/* amount to add to uvs so that they move one row further */
	vec2 offset_row = vec2(0.0, 2.0 * invrendertargetdim.y);

	/* heavily blur the image */
	vec4 color = texture2D(colorbuffer, color_uv1);
	color += texture2D(colorbuffer, color_uv1 + offset_row);
	color += texture2D(colorbuffer, color_uv2);
	color += texture2D(colorbuffer, color_uv2 + offset_row);
	color /= 4.0;

	float zdepth = get_view_space_z_from_depth(viewvecs[0].z, viewvecs[1].z, depth);
	float coc = calculate_near_coc(zdepth);

	gl_FragColor = vec4(color.rgb, coc);
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

	gl_FragColor = vec4(color.a * color.rgb, 1.0);
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
