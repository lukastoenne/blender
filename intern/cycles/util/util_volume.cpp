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

#include "util_volume.h"

#if defined(HAS_CPP11_FEATURES) && defined(_MSC_VER)

namespace std {

bool operator==(const pthread_t &lhs, const pthread_t &rhs)
{
	return lhs.p == rhs.p;
}

}  /* namespace std */

#endif

CCL_NAMESPACE_BEGIN

template <typename T>
void release_map_memory(unordered_map<pthread_t, T> &map)
{
	typename unordered_map<pthread_t, T>::iterator iter;

	for(iter = map.begin(); iter != map.end(); ++iter) {
		delete iter->second;
	}
}

template <typename IsectorType>
void create_isectors_threads(unordered_map<pthread_t, IsectorType *> &isect_map,
                             const vector<pthread_t> &thread_ids,
                             const IsectorType &main_isect)
{
	release_map_memory(isect_map);

	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		if (isect_map.find(thread_ids[i]) == isect_map.end()) {
			isect_map[thread_ids[i]] = new IsectorType(main_isect);
		}
	}

	if (isect_map.find(my_thread) == isect_map.end()) {
		isect_map[my_thread] = new IsectorType(main_isect);
	}
}

template <typename SamplerType, typename AccessorType>
void create_samplers_threads(unordered_map<pthread_t, SamplerType *> &sampler_map,
                             vector<AccessorType *> &accessors,
                             const vector<pthread_t> &thread_ids,
                             const openvdb::math::Transform *transform,
                             const AccessorType &main_accessor)
{
	release_map_memory(sampler_map);

	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		AccessorType *accessor = new AccessorType(main_accessor);
		accessors.push_back(accessor);

		if (sampler_map.find(thread_ids[i]) == sampler_map.end()) {
			sampler_map[thread_ids[i]] = new SamplerType(*accessor, *transform);
		}
	}

	if (sampler_map.find(my_thread) == sampler_map.end()) {
		AccessorType *accessor = new AccessorType(main_accessor);
		accessors.push_back(accessor);
		sampler_map[my_thread] = new SamplerType(*accessor, *transform);
	}
}

/* ********** OpenVDB floating pointing scalar volume ************ */

vdb_float_volume::vdb_float_volume(openvdb::FloatGrid::Ptr grid)
    : transform(&grid->transform())
    , uniform_voxels(grid->hasUniformVoxels())
    , main_isector(uniform_voxels ? new isector_t(*grid, 1) : NULL)
{
	accessor = new openvdb::FloatGrid::ConstAccessor(grid->getConstAccessor());
}

vdb_float_volume::~vdb_float_volume()
{
	release_map_memory(point_samplers);
	release_map_memory(box_samplers);

	if(uniform_voxels) {
		delete main_isector;
		release_map_memory(isectors);
	}

	delete accessor;

	for(size_t i = 0; i < accessors.size(); ++i) {
		delete accessors[i];
	}
}

void vdb_float_volume::create_threads_utils(const vector<pthread_t> &thread_ids)
{
	if (uniform_voxels) {
		create_isectors_threads(isectors, thread_ids, *main_isector);
	}

	create_samplers_threads(point_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(box_samplers, accessors, thread_ids, transform, *accessor);
}

/* ********** OpenVDB vector volume ************ */

vdb_float3_volume::vdb_float3_volume(openvdb::Vec3SGrid::Ptr grid)
    : transform(&grid->transform())
    , uniform_voxels(grid->hasUniformVoxels())
    , staggered(grid->getGridClass() == openvdb::GRID_STAGGERED)
    , main_isector(uniform_voxels ? new isector_t(*grid, 1) : NULL)
{
	accessor = new openvdb::Vec3SGrid::ConstAccessor(grid->getConstAccessor());
}

vdb_float3_volume::~vdb_float3_volume()
{
	release_map_memory(point_samplers);
	release_map_memory(box_samplers);
	release_map_memory(stag_point_samplers);
	release_map_memory(stag_box_samplers);

	if(uniform_voxels) {
		delete main_isector;
		release_map_memory(isectors);
	}

	delete accessor;

	for(size_t i = 0; i < accessors.size(); ++i) {
		delete accessors[i];
	}
}

void vdb_float3_volume::create_threads_utils(const vector<pthread_t> &thread_ids)
{
	if (uniform_voxels) {
		create_isectors_threads(isectors, thread_ids, *main_isector);
	}

	create_samplers_threads(point_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(box_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(stag_point_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(stag_box_samplers, accessors, thread_ids, transform, *accessor);
}

CCL_NAMESPACE_END
