#define NUM_LIGHTS 3

/* Classic Blender particle hair with tangent as normal (just for comparison) */
//#define SHADING_CLASSIC_BLENDER
/* Kajiya-Kay model with a diffuse component and main specular highlights */
#define SHADING_KAJIYA

in vec3 fPosition;
in vec3 fTangent;
in vec3 fColor;

out vec4 outColor;

void main()
{
#ifdef SHADING_CLASSIC_BLENDER
	vec3 N = normalize(fTangent);
#endif
#ifdef SHADING_KAJIYA
	vec3 T = normalize(fTangent);
#endif 

	/* view vector computation, depends on orthographics or perspective */
	vec3 V = (gl_ProjectionMatrix[3][3] == 0.0) ? normalize(fPosition) : vec3(0.0, 0.0, -1.0);
	float cosine_eye = dot(T, V);
	float sine_eye = sqrt(1.0 - cosine_eye*cosine_eye);

	vec3 L_diffuse = vec3(0.0);
	vec3 L_specular = vec3(0.0);
	for (int i = 0; i < NUM_LIGHTS; i++) {
		/* todo: this is a slow check for disabled lights */
		if (gl_LightSource[i].specular.a == 0.0)
			continue;

		float intensity = 1.0;
		vec3 light_direction;

		if (gl_LightSource[i].position.w == 0.0) {
			/* directional light */
			light_direction = gl_LightSource[i].position.xyz;
		}
		else {
			/* point light */
			vec3 d = gl_LightSource[i].position.xyz - fPosition;
			light_direction = normalize(d);

			/* spot light cone */
			if (gl_LightSource[i].spotCutoff < 90.0) {
				float cosine = max(dot(light_direction, -gl_LightSource[i].spotDirection), 0.0);
				intensity = pow(cosine, gl_LightSource[i].spotExponent);
				intensity *= step(gl_LightSource[i].spotCosCutoff, cosine);
			}

			/* falloff */
			float distance = length(d);

			intensity /= gl_LightSource[i].constantAttenuation +
			             gl_LightSource[i].linearAttenuation * distance +
			             gl_LightSource[i].quadraticAttenuation * distance * distance;
		}

#ifdef SHADING_CLASSIC_BLENDER
		/* diffuse light */
		vec3 light_diffuse = gl_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse * diffuse_bsdf * intensity;

		/* specular light */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = normalize(light_direction - V);
		float specular_bsdf = pow(max(dot(N, H), 0.0), gl_FrontMaterial.shininess);
		L_specular += light_specular * specular_bsdf * intensity;
#endif
#ifdef SHADING_KAJIYA
		float cosine_light = dot(T, light_direction);
		float sine_light = sqrt(1.0 - cosine_light*cosine_light);

		/* diffuse light */
		vec3 light_diffuse = gl_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = sine_light;
		L_diffuse += light_diffuse * diffuse_bsdf * intensity;

		/* specular light */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		float specular_bsdf = pow(abs(cosine_light)*abs(cosine_eye) + sine_light*sine_eye, gl_FrontMaterial.shininess);
		L_specular += light_specular * specular_bsdf * intensity;
#endif 
	}

	/* sum lighting */
	
	vec3 L = vec3(0.0, 0.0, 0.0);
	L += gl_FrontLightModelProduct.sceneColor.rgb;
	L += L_diffuse * gl_FrontMaterial.diffuse.rgb;
	L += L_specular * gl_FrontMaterial.specular.rgb;
	float alpha = gl_FrontMaterial.diffuse.a;

	/* write out fragment color */
	outColor = vec4(L, alpha);
}
