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

#include "kernel_compat_cpu.h"
#include "kernel_types.h"
#include "kernel_globals.h"

#include "vdb_globals.h"
#include "vdb_intern.h"
#include "vdb_thread.h"

#include "util_vector.h"

CCL_NAMESPACE_BEGIN

/* Manage thread-local data associated with volumes */

struct OpenVDBScalarThreadData {
	typedef openvdb::FloatGrid grid_t;
	typedef openvdb::FloatGrid::ConstAccessor accessor_t;
	typedef openvdb::tools::GridSampler<accessor_t, openvdb::tools::PointSampler> point_sampler_t;
	typedef openvdb::tools::GridSampler<accessor_t, openvdb::tools::BoxSampler> box_sampler_t;
	typedef openvdb::tools::VolumeRayIntersector<grid_t, grid_t::TreeType::RootNodeType::ChildNodeType::LEVEL, vdb_ray_t> isector_t;
	
	void init(const grid_t &grid, const isector_t &main_isector)
	{
		accessor = new accessor_t(grid.getConstAccessor());
		point_sampler = new point_sampler_t(*accessor, grid.transform());
		box_sampler = new box_sampler_t(*accessor, grid.transform());
		isector = new isector_t(main_isector);
	}
	
	void free()
	{
		delete accessor;
		delete point_sampler;
		delete box_sampler;
		delete isector;
	}
	
	accessor_t *accessor;
	point_sampler_t *point_sampler;
	box_sampler_t *box_sampler;
	isector_t *isector;
};

struct OpenVDBVectorThreadData {
	typedef openvdb::Vec3SGrid grid_t;
	typedef openvdb::Vec3SGrid::ConstAccessor accessor_t;
	typedef openvdb::tools::GridSampler<accessor_t, openvdb::tools::PointSampler> point_sampler_t;
	typedef openvdb::tools::GridSampler<accessor_t, openvdb::tools::BoxSampler> box_sampler_t;
	typedef openvdb::tools::GridSampler<accessor_t, openvdb::tools::StaggeredPointSampler> stag_point_sampler_t;
	typedef openvdb::tools::GridSampler<accessor_t, openvdb::tools::StaggeredBoxSampler> stag_box_sampler_t;
	
	typedef openvdb::tools::VolumeRayIntersector<grid_t, grid_t::TreeType::RootNodeType::ChildNodeType::LEVEL, vdb_ray_t> isector_t;
	
	void init (const grid_t &grid, const isector_t &main_isector)
	{
		accessor = new accessor_t(grid.getConstAccessor());
		point_sampler = new point_sampler_t(*accessor, grid.transform());
		box_sampler = new box_sampler_t(*accessor, grid.transform());
		stag_point_sampler = new stag_point_sampler_t(*accessor, grid.transform());
		stag_box_sampler = new stag_box_sampler_t(*accessor, grid.transform());
		isector = new isector_t(main_isector);
	}
	
	void free()
	{
		delete accessor;
		delete point_sampler;
		delete box_sampler;
		delete stag_point_sampler;
		delete stag_box_sampler;
		delete isector;
	}
	
	accessor_t *accessor;
	point_sampler_t *point_sampler;
	box_sampler_t *box_sampler;
	stag_point_sampler_t *stag_point_sampler;
	stag_box_sampler_t *stag_box_sampler;
	isector_t *isector;
};

struct OpenVDBThreadData {
	std::vector<OpenVDBScalarThreadData> scalar_data;
	std::vector<OpenVDBVectorThreadData> vector_data;
};

void vdb_thread_init(KernelGlobals *kg, const KernelGlobals *kernel_globals, OpenVDBGlobals *vdb_globals)
{
	kg->vdb = vdb_globals;
	
	OpenVDBThreadData *tdata = new OpenVDBThreadData;
	
	tdata->scalar_data.resize(vdb_globals->scalar_grids.size());
	tdata->vector_data.resize(vdb_globals->vector_grids.size());
	for (size_t i = 0; i < vdb_globals->scalar_grids.size(); ++i) {
		tdata->scalar_data[i].init(*vdb_globals->scalar_grids[i], *vdb_globals->scalar_main_isectors[i]);
	}
	for (size_t i = 0; i < vdb_globals->vector_grids.size(); ++i) {
		tdata->vector_data[i].init(*vdb_globals->vector_grids[i], *vdb_globals->vector_main_isectors[i]);
	}
	kg->vdb_tdata = tdata;
}

void vdb_thread_free(KernelGlobals *kg)
{
	OpenVDBThreadData *tdata = kg->vdb_tdata;
	kg->vdb_tdata = NULL;
	
	for (size_t i = 0; i < tdata->scalar_data.size(); ++i) {
		tdata->scalar_data[i].free();
	}
	for (size_t i = 0; i < tdata->vector_data.size(); ++i) {
		tdata->vector_data[i].free();
	}
	delete tdata;
}

bool vdb_volume_scalar_has_uniform_voxels(OpenVDBGlobals *vdb, int vdb_index)
{
	return vdb->scalar_grids[vdb_index]->hasUniformVoxels();
}

bool vdb_volume_vector_has_uniform_voxels(OpenVDBGlobals *vdb, int vdb_index)
{
	return vdb->vector_grids[vdb_index]->hasUniformVoxels();
}

float vdb_volume_sample_scalar(OpenVDBGlobals */*vdb*/, OpenVDBThreadData *vdb_thread, int vdb_index,
                               float x, float y, float z, int sampling)
{
	OpenVDBScalarThreadData &data = vdb_thread->scalar_data[vdb_index];
	
	if (sampling == OPENVDB_SAMPLE_POINT)
		return data.point_sampler->wsSample(openvdb::Vec3d(x, y, z));
	else
		return data.box_sampler->wsSample(openvdb::Vec3d(x, y, z));
}

float3 vdb_volume_sample_vector(OpenVDBGlobals *vdb, OpenVDBThreadData *vdb_thread, int vdb_index,
                                float x, float y, float z, int sampling)
{
	bool staggered = (vdb->vector_grids[vdb_index]->getGridClass() == openvdb::GRID_STAGGERED);
	OpenVDBVectorThreadData &data = vdb_thread->vector_data[vdb_index];
	openvdb::Vec3s r;
	
	if (staggered) {
		if (sampling == OPENVDB_SAMPLE_POINT)
			r = data.stag_point_sampler->wsSample(openvdb::Vec3d(x, y, z));
		else
			r = data.stag_box_sampler->wsSample(openvdb::Vec3d(x, y, z));
	}
	else {
		if (sampling == OPENVDB_SAMPLE_POINT)
			r = data.point_sampler->wsSample(openvdb::Vec3d(x, y, z));
		else
			r = data.box_sampler->wsSample(openvdb::Vec3d(x, y, z));
	}
	
	return make_float3(r.x(), r.y(), r.z());
}

bool vdb_volume_intersect(OpenVDBThreadData *vdb_thread, int vdb_index,
                          const Ray *ray, float *isect)
{
	OpenVDBScalarThreadData &data = vdb_thread->scalar_data[vdb_index];
	
	vdb_ray_t::Vec3Type P(ray->P.x, ray->P.y, ray->P.z);
	vdb_ray_t::Vec3Type D(ray->D.x, ray->D.y, ray->D.z);
	D.normalize();
	
	vdb_ray_t vdb_ray(P, D, 1e-5f, ray->t);
	
	if(data.isector->setWorldRay(vdb_ray)) {
		// TODO(kevin): is this correct?
		*isect = static_cast<float>(vdb_ray.t1());
		
		return true;
	}
	
	return false;
}

bool vdb_volume_march(OpenVDBThreadData *vdb_thread, int vdb_index,
                      float *t0, float *t1)
{
	OpenVDBScalarThreadData &data = vdb_thread->scalar_data[vdb_index];
	
	float vdb_t0(*t0), vdb_t1(*t1);

	if(data.isector->march(vdb_t0, vdb_t1)) {
		*t0 = data.isector->getWorldTime(vdb_t0);
		*t1 = data.isector->getWorldTime(vdb_t1);

		return true;
	}

	return false;
}

CCL_NAMESPACE_END
