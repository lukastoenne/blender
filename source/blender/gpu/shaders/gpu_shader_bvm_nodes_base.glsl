void V_NOOP()
{
}

void V_VALUE_FLOAT(const float value, out float result)
{
	result = value;
}
void D_VALUE_FLOAT(const float value, out float dresult)
{
	dresult = float(0.0);
}

void V_VALUE_FLOAT3(const vec3 value, out vec3 result)
{
	result = value;
}
void D_VALUE_FLOAT3(const vec3 value, out vec3 dresult)
{
	dresult = vec3(0.0, 0.0, 0.0);
}

void V_VALUE_FLOAT4(const vec4 value, out vec4 result)
{
	result = value;
}
void D_VALUE_FLOAT4(const vec4 value, out vec4 dresult)
{
	dresult = vec4(0.0, 0.0, 0.0, 0.0);
}

void V_VALUE_INT(const int value, out int result)
{
	result = value;
}

void V_VALUE_MATRIX44(const mat4 value, out mat4 result)
{
	result = value;
}


void V_FLOAT_TO_INT(float value, out int result)
{
	result = int(value);
}

void V_INT_TO_FLOAT(int value, out float result)
{
	result = float(value);
}
void D_INT_TO_FLOAT(int value, out float dresult)
{
	dresult = float(0.0);
}

void V_SET_FLOAT3(float x, float y, float z, out vec3 result)
{
	result = vec3(x, y, z);
}
void D_SET_FLOAT3(float x, float dx, float y, float dy, float z, float dz, out vec3 dresult)
{
	dresult = vec3(dx, dy, dz);
}

void V_GET_ELEM_FLOAT3(int index, vec3 vec, out float result)
{
	result = vec[index];
}
void D_GET_ELEM_FLOAT3(int index, vec3 vec, vec3 dvec, out float result)
{
	result = dvec[index];
}

void V_SET_FLOAT4(float x, float y, float z, float w, out vec4 result)
{
	result = vec4(x, y, z, w);
}
void D_SET_FLOAT4(float x, float dx, float y, float dy,
                  float z, float dz, float w, float dw,
                  out vec4 dresult)
{
	dresult = vec4(dx, dy, dz, dw);
}

void V_GET_ELEM_FLOAT4(int index, vec4 f, out float result)
{
	result = f[index];
}
void D_GET_ELEM_FLOAT4(int index, vec4 f, vec4 df, out float dresult)
{
	dresult = df[index];
}
