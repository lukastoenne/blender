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

#ifndef __VDB_GLOBALS_H__
#define __VDB_GLOBALS_H__

#include "vdb_intern.h"

CCL_NAMESPACE_BEGIN

typedef openvdb::math::Ray<float> vdb_ray_t;
typedef openvdb::math::Transform vdb_transform_t;

struct OpenVDBGlobals {
	typedef openvdb::FloatGrid scalar_grid_t;
	typedef openvdb::Vec3SGrid vector_grid_t;
	typedef openvdb::tools::VolumeRayIntersector<scalar_grid_t, scalar_grid_t::TreeType::RootNodeType::ChildNodeType::LEVEL, vdb_ray_t> scalar_isector_t;
	typedef openvdb::tools::VolumeRayIntersector<vector_grid_t, vector_grid_t::TreeType::RootNodeType::ChildNodeType::LEVEL, vdb_ray_t> vector_isector_t;
	
	vector<const scalar_grid_t *> scalar_grids;
	vector<const vector_grid_t *> vector_grids;
	/* Main intersectors, which initialize the voxels' bounding box
	 * so the ones for the various threads do not do this,
	 * rather they are generated from a copy of these
	 */
	vector<scalar_isector_t *> scalar_main_isectors;
	vector<vector_isector_t *> vector_main_isectors;
};

CCL_NAMESPACE_END

#endif /* __VDB_GLOBALS_H__ */
