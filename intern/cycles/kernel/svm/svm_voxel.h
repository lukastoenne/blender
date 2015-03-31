/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

ccl_device void svm_node_tex_voxel(KernelGlobals *kg,
                                   ShaderData *sd,
                                   float *stack,
                                   uint4 node,
                                   int *offset)
{
	int id = node.y;
	uint co_offset, out_offset, space;
	decode_node_uchar4(node.z, &co_offset, &out_offset, &space, NULL);
	float3 co = stack_load_float3(stack, co_offset);
	if(space == NODE_TEX_VOXEL_SPACE_OBJECT) {
		co = volume_normalized_position(kg, sd, co);
	}
	else {
		kernel_assert(space == NODE_TEX_VOXEL_SPACE_WORLD);
		Transform tfm;
		tfm.x = read_node_float(kg, offset);
		tfm.y = read_node_float(kg, offset);
		tfm.z = read_node_float(kg, offset);
		tfm.w = read_node_float(kg, offset);
		co = transform_point(&tfm, co);
	}
	if(co.x < 0.0f || co.y < 0.0f || co.z < 0.0f ||
	   co.x > 1.0f || co.y > 1.0f || co.z > 1.0f)
	{
		stack_store_float(stack, out_offset, 0.0f);
		return;
	}
#ifdef __KERNEL_GPU__
	float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
#else
	float4 r = kernel_tex_image_interp_3d(id, co.x, co.y, co.z);
#endif
	float3 f = float4_to_float3(r);
	stack_store_float(stack, out_offset, average(f));
}

CCL_NAMESPACE_END
