layout(points) in;
layout(line_strip, max_vertices = 16) out;

uniform float debug_scale;

in float vCurveParam[];
in vec3 vPosition[];
in vec3 vOldPosition[];
in vec3 vTangent[];
in vec3 vOldTangent[];
in vec3 vTargetPosition[];
in mat3 vTargetFrame[];
in vec3 vColor[];

out vec3 fColor;

void emit_vertex(vec3 location, vec3 color)
{
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(location, 1.0);
	fColor = color;
	EmitVertex();
}

void main()
{
	fColor = vColor[0];

	vec3 loc = vPosition[0];
	vec3 oldloc = vOldPosition[0];
	vec3 nor = vTangent[0];
	vec3 oldnor = vOldTangent[0];
	vec3 target_loc = vTargetPosition[0];
	mat3 target_frame = vTargetFrame[0];
	vec3 red = vec3(1, 0, 0);
	vec3 green = vec3(0, 1, 0);
	vec3 blue = vec3(0, 0, 1);
	vec3 magenta = vec3(1, 0, 1);
	vec3 yellow = vec3(1, 1, 0);
	vec3 cyan = vec3(0, 1, 1);

#if 1
	emit_vertex(loc, magenta);
	emit_vertex(loc + vTangent[0] * debug_scale, magenta);
	EndPrimitive();
#endif

#if 1
	emit_vertex(loc, red);
	emit_vertex(loc + target_frame[0] * debug_scale, red);
	EndPrimitive();
	emit_vertex(loc, green);
	emit_vertex(loc + target_frame[1] * debug_scale, green);
	EndPrimitive();
	emit_vertex(loc, blue);
	emit_vertex(loc + target_frame[2] * debug_scale, blue);
	EndPrimitive();
#endif

	CurlParams params = curl_params(vCurveParam[0], 1.0, target_loc, target_frame);
	
#if 0
	vec3 dcurl = (params.dcurlvec - nor) * params.factor;
	vec3 dshape = (target_loc + params.curlvec - loc) * params.dfactor;
	emit_vertex(loc, cyan);
	emit_vertex(loc + dcurl * debug_scale, cyan);
	EndPrimitive();
	emit_vertex(loc, yellow);
	emit_vertex(loc + dshape * debug_scale, yellow);
	EndPrimitive();

	emit_vertex(loc, magenta);
	emit_vertex(loc + (oldnor + dcurl + dshape) * debug_scale, magenta);
	EndPrimitive();
#endif

#if 0
	vec3 dloc = (target_loc + params.curlvec - loc) * params.factor;
	vec3 nloc = loc + dloc;
	vec3 dshape = dloc / (vCurveParam[0] * curl_shape);
	vec3 dtarget = target_frame[2] + params.turns * params.dcurlvec;
	vec3 nnor = normalize(dshape + mix(oldnor, dtarget, params.factor));
	emit_vertex(loc, cyan);
	emit_vertex(loc + nnor * debug_scale, cyan);
	EndPrimitive();
#endif
}
