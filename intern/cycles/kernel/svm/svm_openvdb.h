/*
 * Copyright 2015 Blender Foundation
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

#ifdef __OPENVDB__

ccl_device void svm_node_openvdb(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint slot = node.y;
	uint type, out_offset, sampling, co_offset;
	decode_node_uchar4(node.z, &type, &co_offset, &out_offset, &sampling);

	float3 co = stack_load_float3(stack, co_offset);

	Transform tfm;
	tfm.x = read_node_float(kg, offset);
	tfm.y = read_node_float(kg, offset);
	tfm.z = read_node_float(kg, offset);
	tfm.w = read_node_float(kg, offset);
	co = transform_point(&tfm, co);

	if(type == NODE_VDB_FLOAT) {
		float out = kernel_tex_voxel_float(slot, co.x, co.y, co.z, sampling);

		if(stack_valid(out_offset)) {
			stack_store_float(stack, out_offset, out);
		}
	}
	else if(type == NODE_VDB_FLOAT3) {
		float3 out = kernel_tex_voxel_float3(slot, co.x, co.y, co.z, sampling);

		if(stack_valid(out_offset)) {
			stack_store_float3(stack, out_offset, out);
		}
	}
}

#endif

CCL_NAMESPACE_END

