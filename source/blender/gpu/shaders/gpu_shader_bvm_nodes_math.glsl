#define M_PI 3.1415926535897932384626433832795

void V_ADD_FLOAT(float a, float b, out float r)
{
	r = a + b;
}
void D_ADD_FLOAT(float a, float da, float b, float db, out float dr)
{
	dr = da + db;
}

void V_SUB_FLOAT(float a, float b, out float r)
{
	r = a - b;
}
void D_SUB_FLOAT(float a, float da, float b, float db, out float dr)
{
	dr = da - db;
}

void V_MUL_FLOAT(float a, float b, out float r)
{
	r = a * b;
}
void D_MUL_FLOAT(float a, float da, float b, float db, out float dr)
{
	dr = da * b + a * db;
}

void V_DIV_FLOAT(float a, float b, out float r)
{
	if (b != 0.0)
		r = a / b;
	else
		r = 0.0;
}
void D_DIV_FLOAT(float a, float da, float b, float db, out float dr)
{
	if (b != 0.0)
		dr = (da - db * a / b) / b;
	else
		dr = 0.0;
}

void V_SINE(float f, out float r)
{
	r = sin(f);
}
void D_SINE(float f, float df, out float dr)
{
	dr = df * cos(f);
}

void V_COSINE(float f, out float r)
{
	r = cos(f);
}
void D_COSINE(float f, float df, out float dr)
{
	dr = df * (-sin(f));
}

void V_TANGENT(float f, out float r)
{
	r = tan(f);
}
void D_TANGENT(float f, float df, out float dr)
{
	float c = cos(f);
	if (c != 0.0)
		dr = df / (c*c);
	else
		dr = 0.0;
}

void V_ARCSINE(float f, out float r)
{
	r = asin(f);
}
void D_ARCSINE(float f, float df, out float dr)
{
	if (f >= -1.0 && f <= 1.0)
		dr = df / sqrt(1.0 - f*f);
	else
		dr = 0.0;
}

void V_ARCCOSINE(float f, out float r)
{
	r = acos(f);
}
void D_ARCCOSINE(float f, float df, out float dr)
{
	if (f >= -1.0 && f <= 1.0)
		dr = -df / sqrt(1.0 - f*f);
	else
		dr = 0.0;
}

void V_ARCTANGENT(float f, out float r)
{
	r = atan(f);
}
void D_ARCTANGENT(float f, float df, out float dr)
{
	dr = df / (1.0 + f*f);
}

void V_POWER(float a, float b, out float r)
{
	if (a > 0.0)
		r = pow(a, b);
	else
		r = 0.0;
}
void D_POWER(float a, float da, float b, float db, out float dr)
{
	if (a > 0.0) {
		float ln_a = log(a);
		dr = (da * b / a + db * ln_a) * exp(b * ln_a);
	}
	else
		dr = 0.0;
}

void V_LOGARITHM(float a, float b, out float r)
{
	if (a > 0.0 && b >= 0.0)
		r = log(a) / log(b);
	else
		r = 0.0;
}
void D_LOGARITHM(float a, float da, float b, float db, out float dr)
{
	if (a > 0.0f && b > 0.0f) {
		float ln_a = log(a);
		float ln_b = log(b);
		dr = (da/a - db/b * ln_a/ln_b) / ln_b;
	}
	else
		dr = 0.0;
}

void V_MINIMUM(float a, float b, out float r)
{
	r = min(a, b);
}
void D_MINIMUM(float a, float da, float b, float db, out float dr)
{
	dr = (a < b) ? da : db;
}

void V_MAXIMUM(float a, float b, out float r)
{
	r = max(a, b);
}
void D_MAXIMUM(float a, float da, float b, float db, out float dr)
{
	dr = (a > b) ? da : db;
}

void V_ROUND(float f, out float r)
{
	r = floor(f + 0.5);
}
void D_ROUND(float f, float df, out float dr)
{
	dr = 0.0;
}

void V_LESS_THAN(float a, float b, out float r)
{
	r = (a < b)? 1.0 : 0.0;
}
void D_LESS_THAN(float a, float da, float b, float db, out float dr)
{
	dr = 0.0;
}

void V_GREATER_THAN(float a, float b, out float r)
{
	r = (a > b)? 1.0 : 0.0;
}
void D_GREATER_THAN(float a, float da, float b, float db, out float dr)
{
	dr = 0.0;
}

void V_MODULO(float a, float b, out float r)
{
	if (b != 0.0)
		r = mod(a, b);
	else
		r = 0.0;
}
void D_MODULO(float a, float da, float b, float db, out float dr)
{
	if (b != 0.0)
		dr = da;
	else
		dr = 0.0;
}

void V_ABSOLUTE(float f, out float r)
{
	r = abs(f);
}
void D_ABSOLUTE(float f, float df, out float dr)
{
	if (f >= 0.0)
		dr = df;
	else
		dr = -df;
}

void V_CLAMP_ONE(float f, out float r)
{
	r = clamp(f, 0.0, 1.0);
}
void D_CLAMP_ONE(float f, float df, out float dr)
{
	if (f >= 0.0 || f <= 1.0)
		dr = df;
	else
		dr = 0.0;
}

void V_SQRT(float f, out float r)
{
	if (f > 0.0)
		r = sqrt(f);
	else
		r = 0.0;
}
void D_SQRT(float f, float df, out float dr)
{
	if (f > 0.0)
		dr = df * 0.5 / sqrt(f);
	else
		dr = 0.0;
}

void V_ADD_FLOAT3(vec3 a, vec3 b, out vec3 r)
{
	r = a + b;
}
void D_ADD_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out vec3 dr)
{
	dr = da + db;
}

void V_SUB_FLOAT3(vec3 a, vec3 b, out vec3 r)
{
	r = a - b;
}
void D_SUB_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out vec3 dr)
{
	dr = da - db;
}

void V_MUL_FLOAT3(vec3 a, vec3 b, out vec3 r)
{
	r = a * b;
}
void D_MUL_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out vec3 dr)
{
	dr = da * b + a * db;
}

void V_DIV_FLOAT3(vec3 a, vec3 b, out vec3 r)
{
	r = mix(vec3(0.0), a / b, notEqual(b, vec3(0.0)));
}
void D_DIV_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out vec3 dr)
{
	dr = mix(vec3(0.0), (da - db * a / b) / b, notEqual(b, vec3(0.0)));
}

void V_MUL_FLOAT3_FLOAT(vec3 a, float b, out vec3 r)
{
	r = a * b;
}
void D_MUL_FLOAT3_FLOAT(vec3 a, vec3 da, float b, float db, out vec3 dr)
{
	dr = da * b + a * db;
}

void V_DIV_FLOAT3_FLOAT(vec3 a, float b, out vec3 r)
{
	r = (b != 0.0) ? a / b : vec3(0.0);
}
void D_DIV_FLOAT3_FLOAT(vec3 a, vec3 da, float b, float db, out vec3 dr)
{
	dr = (b != 0.0) ? (da - db * a / b) / b : vec3(0.0);
}

void V_AVERAGE_FLOAT3(vec3 a, vec3 b, out vec3 r)
{
	r = 0.5 * (a + b);
}
void D_AVERAGE_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out vec3 dr)
{
	dr = 0.5 * (da + db);
}

void V_DOT_FLOAT3(vec3 a, vec3 b, out float r)
{
	r = dot(a, b);
}
void D_DOT_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out float dr)
{
	dr = dot(da, b) + dot(a, db);
}

void V_CROSS_FLOAT3(vec3 a, vec3 b, out vec3 r)
{
	r = cross(a, b);
}
void D_CROSS_FLOAT3(vec3 a, vec3 da, vec3 b, vec3 db, out vec3 dr)
{
	dr = cross(da, b) + cross(a, db);
}

void V_NORMALIZE_FLOAT3(vec3 vec, out vec3 r_vec, out float r_len)
{
	r_vec = normalize(vec);
	r_len = length(vec);
}
void D_NORMALIZE_FLOAT3(vec3 vec, vec3 dvec, out vec3 dr_vec, out float dr_len)
{
	float len = length(vec);
	if (len > 0.0) {
		vec3 n = normalize(vec);
		float dlen = dot(n, dvec);
		dr_vec = (dvec - n * dlen) / len;
		dr_len = dlen;
	}
	else {
		dr_vec = vec3(0.0);
		dr_len = 0.0;
	}
}

void V_LENGTH_FLOAT3(vec3 vec, out float len)
{
	len = length(vec);
}
void D_LENGTH_FLOAT3(vec3 vec, vec3 dvec, out float dlen)
{
	float len = length(vec);
	if (len > 0.0)
		dlen = dot(vec, dvec) / len;
	else
		dlen = 0.0;
}

void V_ADD_MATRIX44(mat4 a, mat4 b, out mat4 r)
{
	r = a + b;
}

void V_SUB_MATRIX44(mat4 a, mat4 b, out mat4 r)
{
	r = a - b;
}

void V_MUL_MATRIX44(mat4 a, mat4 b, out mat4 r)
{
	r = a * b;
}

void V_MUL_MATRIX44_FLOAT(mat4 a, float b, out mat4 r)
{
	r = a * b;
}

void V_DIV_MATRIX44_FLOAT(mat4 a, float b, out mat4 r)
{
	if (b != 0.0)
		r = a / b;
	else
		r = mat4(0.0);
}

void V_NEGATE_MATRIX44(mat4 m, out mat4 r)
{
	r = -m;
}

void V_TRANSPOSE_MATRIX44(mat4 m, out mat4 r)
{
	r = transpose(m);
}

void V_INVERT_MATRIX44(mat4 m, out mat4 r)
{
	r = inverse(m);
}

void V_ADJOINT_MATRIX44(mat4 m, out mat4 r)
{
	vec4 col1 = m[0];
	vec4 col2 = m[1];
	vec4 col3 = m[2];
	vec4 col4 = m[3];

	r[0][0] = determinant(mat3(col2.yzw, col3.yzw, col4.yzw));
	r[0][1] = -determinant(mat3(col2.xzw, col3.xzw, col4.xzw));
	r[0][2] = determinant(mat3(col2.xyw, col3.xyw, col4.xyw));
	r[0][3] = -determinant(mat3(col2.xyz, col3.xyz, col4.xyz));

	r[1][0] = -determinant(mat3(col1.yzw, col3.yzw, col4.yzw));
	r[1][1] = determinant(mat3(col1.xzw, col3.xzw, col4.xzw));
	r[1][2] = -determinant(mat3(col1.xyw, col3.xyw, col4.xyw));
	r[1][3] = determinant(mat3(col1.xyz, col3.xyz, col4.xyz));

	r[2][0] = determinant(mat3(col1.yzw, col2.yzw, col4.yzw));
	r[2][1] = -determinant(mat3(col1.xzw, col2.xzw, col4.xzw));
	r[2][2] = determinant(mat3(col1.xyw, col2.xyw, col4.xyw));
	r[2][3] = -determinant(mat3(col1.xyz, col2.xyz, col4.xyz));

	r[3][0] = -determinant(mat3(col1.yzw, col2.yzw, col3.yzw));
	r[3][1] = determinant(mat3(col1.xzw, col2.xzw, col3.xzw));
	r[3][2] = -determinant(mat3(col1.xyw, col2.xyw, col3.xyw));
	r[3][3] = determinant(mat3(col1.xyz, col2.xyz, col3.xyz));
}

void V_DETERMINANT_MATRIX44(mat4 m, out float r)
{
	r = determinant(m);
}

void V_MUL_MATRIX44_FLOAT3(mat4 a, vec3 b, out vec3 r)
{
	r = (a * vec4(b, 1.0)).xyz;
}
void D_MUL_MATRIX44_FLOAT3(mat4 a, vec3 b, vec3 db, out vec3 dr)
{
	dr = (a * vec4(db, 0.0)).xyz;
}

void V_MUL_MATRIX44_FLOAT4(mat4 a, vec4 b, out vec4 r)
{
	r = a * b;
}
void D_MUL_MATRIX44_FLOAT4(mat4 a, vec4 b, vec4 db, out vec4 dr)
{
	dr = a * db;
}

void V_MATRIX44_TO_LOC(mat4 m, out vec3 loc)
{
	loc = m[3].xyz;
}
void D_MATRIX44_TO_LOC(mat4 m, out vec3 dloc)
{
	dloc = vec3(0.0);
}

void euler_rotation_order(int order, out int i, out int j, out int k, out bool parity)
{
	switch (order) {
		case 0:
		case 1:
			/* XYZ */
			i = 0; j = 1; k = 2;
			parity = false;
			break;
		case 2:
			/* XZY */
			i = 0; j = 2; k = 3;
			parity = true;
			break;
		case 3:
			/* YXZ */
			i = 1; j = 0; k = 2;
			parity = true;
			break;
		case 4:
			/* YZX */
			i = 1; j = 2; k = 0;
			parity = false;
			break;
		case 5:
			/* ZXY */
			i = 2; j = 0; k = 1;
			parity = false;
			break;
		case 6:
			/* ZYX */
			i = 2; j = 1; k = 0;
			parity = true;
			break;
	}
}

void V_MATRIX44_TO_EULER(int order, mat4 m, out vec3 euler)
{
	const float epsilon = 1.0e-12; /* XXX arbitrary, there is no FLT_EPSILON in GLSL */

	int i, j, k;
	bool parity;
	euler_rotation_order(order, i, j, k, parity);
	
	vec3 ui = normalize(m[i].xyz);
	vec3 uj = normalize(m[j].xyz);
	vec3 uk = normalize(m[k].xyz);

	float cy = length(vec2(ui[i], ui[j]));

	vec3 eul1, eul2;
	if (cy > epsilon) {
		eul1[i] = atan(uj[k], uk[k]);
		eul1[j] = atan(-ui[k], cy);
		eul1[k] = atan(ui[j], ui[i]);

		eul2[i] = atan(-uj[k], -uk[k]);
		eul2[j] = atan(-ui[k], -cy);
		eul2[k] = atan(-ui[j], -ui[i]);
	}
	else {
		eul1[i] = atan(-uk[j], uj[j]);
		eul1[j] = atan(-ui[k], cy);
		eul1[k] = 0;

		eul2 = eul1;
	}

	if (parity) {
		eul1 = -eul1;
		eul2 = -eul2;
	}

	/* return best, which is just the one with lowest values it in */
	float d1 = dot(eul1, eul1);
	float d2 = dot(eul2, eul2);
	if (d1 > d2)
		euler = eul2;
	else
		euler = eul1;
}
void D_MATRIX44_TO_EULER(int order, mat4 m, out vec3 deuler)
{
	deuler = vec3(0.0);
}

void V_MATRIX44_TO_AXISANGLE(mat4 m4, out vec3 axis, out float angle)
{
	mat3 m = mat3(normalize(m4[0].xyz), normalize(m4[1].xyz), normalize(m4[2].xyz));

	/* based on
	 * http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToAngle/
	 */
 	const float epsilon = 0.01; // margin to allow for rounding errors
	const float epsilon2 = 0.1; // margin to distinguish between 0 and 180 degrees

	if (   abs(m[0][1] - m[1][0]) < epsilon
		&& abs(m[0][2] - m[2][0]) < epsilon
		&& abs(m[1][2] - m[2][1]) < epsilon) {
		// singularity found
		// first check for identity matrix which must have +1 for all terms
		//  in leading diagonaland zero in other terms
		if (   (abs(m[0][1] + m[1][0]) < epsilon2)
		  	&& (abs(m[0][2] + m[2][0]) < epsilon2)
		  	&& (abs(m[1][2] + m[2][1]) < epsilon2)
		  	&& (abs(m[0][0] + m[1][1] + m[2][2] - 3.0) < epsilon2)) {
			// this singularity is identity matrix so angle = 0
			// arbitrary axis
			angle = 0.0;
			axis = vec3(0.0, 0.0, 1.0);
			return;
		}
		// otherwise this singularity is angle = 180
		angle = M_PI;
		float xx = (m[0][0] + 1.0) * 0.5;
		float yy = (m[1][1] + 1.0) * 0.5;
		float zz = (m[2][2] + 1.0) * 0.5;
		float xy = (m[0][1] + m[1][0]) * 0.25;
		float xz = (m[0][2] + m[2][0]) * 0.25;
		float yz = (m[1][2] + m[2][1]) * 0.25;
		if ((xx > yy) && (xx > zz)) { // m[0][0] is the largest diagonal term
			if (xx < epsilon)
				axis = vec3(0.0, 0.7071, 0.7071);
			else {
				float x = sqrt(xx);
				axis = vec3(x, xy / x, xz / x);
			}
		} else if (yy > zz) { // m[1][1] is the largest diagonal term
			if (yy < epsilon)
				axis = vec3(0.7071, 0.0, 0.7071);
			else {
				float y = sqrt(yy);
				axis = vec3(xy / y, y, yz / y);
			}
		} else { // m[2][2] is the largest diagonal term so base result on this
			if (zz < epsilon)
				axis = vec3(0.7071, 0.7071, 0.0);
			else {
				float z = sqrt(zz);
				axis = vec3(xz / z, yz / z, z);
			}
		}
		return;
	}
	// there are no singularities so we can handle normally
	float s = sqrt(  (m[2][1] - m[1][2]) * (m[2][1] - m[1][2])
			       + (m[0][2] - m[2][0]) * (m[0][2] - m[2][0])
		           + (m[1][0] - m[0][1]) * (m[1][0] - m[0][1])); // used to normalise
	angle = acos((m[0][0] + m[1][1] + m[2][2] - 1.0) * 0.5);
	axis = vec3((m[2][1] - m[1][2]), (m[0][2] - m[2][0]), (m[1][0] - m[0][1])) / s;
}
void D_MATRIX44_TO_AXISANGLE(mat4 m, out vec3 daxis, out float dangle)
{
	daxis = vec3(0.0);
	dangle = 0.0;
}

void V_MATRIX44_TO_SCALE(mat4 m, out vec3 scale)
{
	scale = vec3(length(m[0].xyz), length(m[1].xyz), length(m[2].xyz));
}
void D_MATRIX44_TO_SCALE(mat4 m, out vec3 dscale)
{
	dscale = vec3(0.0);
}

void V_LOC_TO_MATRIX44(vec3 loc, out mat4 m)
{
	m = mat4(1.0, 0.0, 0.0, 0.0,
		     0.0, 1.0, 0.0, 0.0,
		     0.0, 0.0, 1.0, 0.0,
		     loc.x, loc.y, loc.z, 1.0);
}

void V_EULER_TO_MATRIX44(int order, vec3 euler, out mat4 m)
{
	int i, j, k;
	bool parity;
	euler_rotation_order(order, i, j, k, parity);
	float ti, tj, th, ci, cj, ch, si, sj, sh, cc, cs, sc, ss;

	if (parity) {
		ti = -euler[i];
		tj = -euler[j];
		th = -euler[k];
	}
	else {
		ti = euler[i];
		tj = euler[j];
		th = euler[k];
	}

	ci = cos(ti);
	cj = cos(tj);
	ch = cos(th);
	si = sin(ti);
	sj = sin(tj);
	sh = sin(th);

	cc = ci * ch;
	cs = ci * sh;
	sc = si * ch;
	ss = si * sh;

	m[i][i] = cj * ch;
	m[j][i] = sj * sc - cs;
	m[k][i] = sj * cc + ss;
	m[i][j] = cj * sh;
	m[j][j] = sj * ss + cc;
	m[k][j] = sj * cs - sc;
	m[i][k] = -sj;
	m[j][k] = cj * si;
	m[k][k] = cj * ci;
	m[0][3] = 0.0;
	m[1][3] = 0.0;
	m[2][3] = 0.0;
	m[3] = vec4(0.0, 0.0, 0.0, 1.0);
}

void V_AXISANGLE_TO_MATRIX44(vec3 axis, float angle, out mat4 m)
{
	axis = normalize(axis);
	float angle_cos = cos(angle);
	float angle_sin = sin(angle);

	float n_00, n_01, n_11, n_02, n_12, n_22;

	/* now convert this to a 3x3 matrix */

	float ico = (1.0 - angle_cos);
	vec3 nsi = axis * angle_sin;

	n_00 = (axis[0] * axis[0]) * ico;
	n_01 = (axis[0] * axis[1]) * ico;
	n_11 = (axis[1] * axis[1]) * ico;
	n_02 = (axis[0] * axis[2]) * ico;
	n_12 = (axis[1] * axis[2]) * ico;
	n_22 = (axis[2] * axis[2]) * ico;

	m = mat4(n_00 + angle_cos, n_01 + nsi[2],    n_02 - nsi[1],    0.0,
		     n_01 - nsi[2],    n_11 + angle_cos, n_12 + nsi[0],    0.0,
		     n_02 + nsi[1],    n_12 - nsi[0],    n_22 + angle_cos, 0.0,
		     0.0,              0.0,              0.0,              1.0);
}

void V_SCALE_TO_MATRIX44(vec3 scale, out mat4 m)
{
	m = mat4(scale.x, 0.0, 0.0, 0.0,
		     0.0, scale.y, 0.0, 0.0,
		     0.0, 0.0, scale.z, 0.0,
		     0.0, 0.0, 0.0, 1.0);
}
