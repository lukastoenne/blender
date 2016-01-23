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
	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		IsectorType *isect = new IsectorType(main_isect);
		pair<pthread_t, IsectorType *> inter(thread_ids[i], isect);
		isect_map.insert(inter);
	}

	if (isect_map.find(my_thread) == isect_map.end()) {
		IsectorType *isect = new IsectorType(main_isect);
		pair<pthread_t, IsectorType *> inter(my_thread, isect);
		isect_map.insert(inter);
	}
}

template <typename SamplerType, typename AccessorType>
void create_samplers_threads(unordered_map<pthread_t, SamplerType *> &sampler_map,
                             vector<AccessorType *> &accessors,
                             const vector<pthread_t> &thread_ids,
                             const openvdb::math::Transform *transform,
                             const AccessorType &main_accessor)
{
	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		AccessorType *accessor = new AccessorType(main_accessor);
		accessors.push_back(accessor);
		SamplerType *sampler = new SamplerType(*accessor, *transform);
		pair<pthread_t, SamplerType *> sampl(thread_ids[i], sampler);
		sampler_map.insert(sampl);
	}

	if (sampler_map.find(my_thread) == sampler_map.end()) {
		AccessorType *accessor = new AccessorType(main_accessor);
		accessors.push_back(accessor);
		SamplerType *sampler = new SamplerType(*accessor, *transform);
		pair<pthread_t, SamplerType *> sampl(my_thread, sampler);
		sampler_map.insert(sampl);
	}
}

/* ********** OpenVDB floating pointing scalar volume ************ */

vdb_float_volume::vdb_float_volume(openvdb::FloatGrid::Ptr grid)
    : transform(&grid->transform())
{
	accessor = new openvdb::FloatGrid::ConstAccessor(grid->getConstAccessor());

	/* only grids with uniform voxels can be used with VolumeRayIntersector */
	if(grid->hasUniformVoxels()) {
		uniform_voxels = true;
		/* 1 = size of the largest sampling kernel radius (BoxSampler) */
		main_isector = new isector_t(*grid, 1);
	}
	else {
		uniform_voxels = false;
		main_isector = NULL;
	}
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
	create_isectors_threads(isectors, thread_ids, *main_isector);
	create_samplers_threads(point_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(box_samplers, accessors, thread_ids, transform, *accessor);
}

/* ********** OpenVDB vector volume ************ */

vdb_float3_volume::vdb_float3_volume(openvdb::Vec3SGrid::Ptr grid)
    : transform(&grid->transform())
{
	accessor = new openvdb::Vec3SGrid::ConstAccessor(grid->getConstAccessor());
	staggered = (grid->getGridClass() == openvdb::GRID_STAGGERED);

	/* only grids with uniform voxels can be used with VolumeRayIntersector */
	if(grid->hasUniformVoxels()) {
		uniform_voxels = true;
		/* 1 = size of the largest sampling kernel radius (BoxSampler) */
		main_isector = new isector_t(*grid, 1);
	}
	else {
		uniform_voxels = false;
		main_isector = NULL;
	}
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
	create_isectors_threads(isectors, thread_ids, *main_isector);
	create_samplers_threads(point_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(box_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(stag_point_samplers, accessors, thread_ids, transform, *accessor);
	create_samplers_threads(stag_box_samplers, accessors, thread_ids, transform, *accessor);
}

CCL_NAMESPACE_END
