/*
 * Copyright 2016 Blender Foundation
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

#ifndef __VDB_THREAD_H__
#define __VDB_THREAD_H__

CCL_NAMESPACE_BEGIN

struct Intersection;
struct KernelGlobals;
struct OpenVDBGlobals;
struct OpenVDBThreadData;
struct Ray;

void vdb_thread_init(KernelGlobals *kg, const KernelGlobals *kernel_globals, OpenVDBGlobals *vdb_globals);
void vdb_thread_free(KernelGlobals *kg);

enum OpenVDB_SampleType {
	OPENVDB_SAMPLE_POINT = 0,
	OPENVDB_SAMPLE_BOX   = 1,
};

bool vdb_volume_scalar_has_uniform_voxels(OpenVDBGlobals *vdb, int vdb_index);
bool vdb_volume_vector_has_uniform_voxels(OpenVDBGlobals *vdb, int vdb_index);
float vdb_volume_sample_scalar(OpenVDBGlobals *vdb, OpenVDBThreadData *vdb_thread, int vdb_index,
                               float x, float y, float z, int sampling);
float3 vdb_volume_sample_vector(OpenVDBGlobals *vdb, OpenVDBThreadData *vdb_thread, int vdb_index,
                                float x, float y, float z, int sampling);
bool vdb_volume_intersect(OpenVDBThreadData *vdb_thread, int vdb_index,
                          const Ray *ray, float *isect);
bool vdb_volume_march(OpenVDBThreadData *vdb_thread, int vdb_index,
                      float *t0, float *t1);

CCL_NAMESPACE_END

#endif /* __VDB_THREAD_H__ */
