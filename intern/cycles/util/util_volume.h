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

#ifndef __UTIL_VOLUME_H__
#define __UTIL_VOLUME_H__

#include "util_types.h"

#include "../kernel/kernel_types.h"

#ifdef WITH_OPENVDB
#include "../kernel/openvdb/vdb_thread.h"
#endif

CCL_NAMESPACE_BEGIN

#if 0
struct KernelGlobals;
struct Ray;
struct Intersection;

class float_volume {
public:
	virtual ~float_volume() {}
#if 0
	virtual float sample(float x, float y, float z, int sampling) = 0;
	virtual bool intersect(const Ray *ray, float *isect_t) = 0;
	virtual bool march(float *t0, float *t1) = 0;
	virtual bool has_uniform_voxels() = 0;
#endif
};

class float3_volume {
public:
	virtual ~float3_volume() {}
#if 0
	virtual float3 sample(float x, float y, float z, int sampling) = 0;
	virtual bool intersect(const Ray *ray, float *isect_t) = 0;
	virtual bool march(float *t0, float *t1) = 0;
	virtual bool has_uniform_voxels() = 0;
#endif
};
#endif

CCL_NAMESPACE_END

#endif /* __UTIL_VOLUME_H__ */

