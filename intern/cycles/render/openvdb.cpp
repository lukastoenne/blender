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

#include "openvdb.h"
#include "scene.h"
#include "util_logging.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENVDB

OpenVDBManager::OpenVDBManager()
{
	openvdb::initialize();

	scalar_grids.reserve(64);
	vector_grids.reserve(64);
	current_grids.reserve(64);
	float_samplers_p.reserve(64);
	float_samplers_b.reserve(64);
	vec3s_samplers_p.reserve(64);
	vec3s_samplers_b.reserve(64);

	need_update = true;
}

OpenVDBManager::~OpenVDBManager()
{
	scalar_grids.clear();
	vector_grids.clear();
	current_grids.clear();
	float_samplers_p.clear();
	float_samplers_b.clear();
	vec3s_samplers_p.clear();
	vec3s_samplers_b.clear();
}

static inline void catch_exceptions()
{
	try {
		throw;
	}
	catch (const openvdb::Exception &e) {
		std::cerr << e.what() << "\n";
	}
	catch (const std::exception &e) {
		std::cerr << e.what() << "\n";
	}
	catch (...) {
		std::cerr << "Unknown error in OpenVDB library...\n";
	}
}

int OpenVDBManager::add_volume(const string &filename, const string &name, int sampling, int grid_type)
{
	using namespace openvdb;
	size_t slot = -1;

	if((slot = find_existing_slot(filename, name, sampling, grid_type)) != -1) {
		return slot;
	}

	try {
		io::File file(filename);
		file.open();

		if(grid_type == NODE_VDB_FLOAT) {
			FloatGrid::Ptr grid = gridPtrCast<FloatGrid>(file.readGrid(name));
			slot = add_scalar_grid(grid, sampling);
		}
		else if(grid_type == NODE_VDB_VEC3S) {
			Vec3SGrid::Ptr grid = gridPtrCast<Vec3SGrid>(file.readGrid(name));
			slot = add_vector_grid(grid, sampling);
		}

		add_grid_description(filename, name, sampling, slot);

		need_update = true;
	}
	catch (...) {
		catch_exceptions();
		need_update = false;
		slot = -1;
	}

	return slot;
}

int OpenVDBManager::find_existing_slot(const string &filename, const string &name, int sampling, int grid_type)
{
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];

		if(grid.filename == filename && grid.name == name) {
			if(grid.sampling == sampling) {
				return grid.slot;
			}
			else {
				/* sampling was changed, remove the sampler */
				delete_sampler(grid_type, grid.sampling, grid.slot);

				/* remove the grid description too */
				std::swap(current_grids[i], current_grids.back());
				current_grids.pop_back();
				break;
			}
		}
	}

	return -1;
}

void OpenVDBManager::delete_sampler(int grid_type, int sampling, size_t slot)
{
	if(grid_type == NODE_VDB_FLOAT) {
		if(sampling == OPENVDB_SAMPLE_POINT) {
			delete float_samplers_p[slot];
			float_samplers_p[slot] = NULL;
		}
		else {
			delete float_samplers_b[slot];
			float_samplers_b[slot] = NULL;
		}
	}
	else {
		if(sampling == OPENVDB_SAMPLE_POINT) {
			delete vec3s_samplers_p[slot];
			vec3s_samplers_p[slot] = NULL;
		}
		else {
			delete vec3s_samplers_b[slot];
			vec3s_samplers_b[slot] = NULL;
		}
	}
}

template <typename Container>
size_t find_empty_slot(Container container)
{
	size_t slot = 0;

	for(; slot < container.size(); ++slot) {
		if(!container[slot]) {
			break;
		}
	}

	return slot;
}

size_t OpenVDBManager::add_scalar_grid(openvdb::FloatGrid::Ptr grid, int sampling)
{
	size_t slot = 0;

	if(sampling == OPENVDB_SAMPLE_POINT) {
		slot = find_empty_slot(float_samplers_p);

		vdb_fsampler_p *sampler = new vdb_fsampler_p(grid->tree(), grid->transform());
		float_samplers_p.insert(float_samplers_p.begin() + slot, sampler);
	}
	else {
		slot = find_empty_slot(float_samplers_b);

		vdb_fsampler_b *sampler = new vdb_fsampler_b(grid->tree(), grid->transform());
		float_samplers_b.insert(float_samplers_b.begin() + slot, sampler);
	}

	scalar_grids.insert(scalar_grids.begin() + slot, grid);

	return slot;
}

size_t OpenVDBManager::add_vector_grid(openvdb::Vec3SGrid::Ptr grid, int sampling)
{
	size_t slot = 0;

	if(sampling == OPENVDB_SAMPLE_POINT) {
		slot = find_empty_slot(vec3s_samplers_p);

		vdb_vsampler_p *sampler = new vdb_vsampler_p(grid->tree(), grid->transform());
		vec3s_samplers_p.insert(vec3s_samplers_p.begin() + slot, sampler);
	}
	else {
		slot = find_empty_slot(vec3s_samplers_b);

		vdb_vsampler_b *sampler = new vdb_vsampler_b(grid->tree(), grid->transform());
		vec3s_samplers_b.insert(vec3s_samplers_b.begin() + slot, sampler);
	}

	vector_grids.insert(vector_grids.begin() + slot, grid);

	return slot;
}

void OpenVDBManager::add_grid_description(const string &filename, const string &name, int sampling, int slot)
{
	GridDescription descr;
	descr.filename = filename;
	descr.name = name;
	descr.sampling = sampling;
	descr.slot = slot;

	current_grids.push_back(descr);
}

void OpenVDBManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress)
{
	(void)scene;

	if(!need_update) {
		return;
	}

	device_free(device, dscene);
	progress.set_status("Updating OpenVDB volumes", "Sending samplers to device.");

	for(size_t i = 0; i < float_samplers_p.size(); ++i) {
		if(!float_samplers_p[i]) {
			continue;
		}
		device->const_copy_to("__vdb_float_samplers_p", float_samplers_p[i], i);
	}

	for(size_t i = 0; i < float_samplers_b.size(); ++i) {
		if(!float_samplers_b[i]) {
			continue;
		}
		device->const_copy_to("__vdb_float_samplers_b", float_samplers_b[i], i);
	}

	for(size_t i = 0; i < vec3s_samplers_p.size(); ++i) {
		if(!vec3s_samplers_p[i]) {
			continue;
		}
		device->const_copy_to("__vdb_vec3s_samplers_p", vec3s_samplers_p[i], i);
	}

	for(size_t i = 0; i < vec3s_samplers_b.size(); ++i) {
		if(!vec3s_samplers_b[i]) {
			continue;
		}
		device->const_copy_to("__vdb_vec3s_samplers_b", vec3s_samplers_b[i], i);
	}

	if(progress.get_cancel()) {
		return;
	}

	VLOG(1) << "VDB Samplers allocate: __vdb_float_samplers_p, " << float_samplers_p.size() * sizeof(vdb_fsampler_p) << " bytes";
	VLOG(1) << "VDB Samplers allocate: __vdb_float_samplers_b, " << float_samplers_b.size() * sizeof(vdb_fsampler_b) << " bytes";
	VLOG(1) << "VDB Samplers allocate: __vdb_vec3s_samplers_p, " << vec3s_samplers_p.size() * sizeof(vdb_vsampler_p) << " bytes";
	VLOG(1) << "VDB Samplers allocate: __vdb_vec3s_samplers_b, " << vec3s_samplers_b.size() * sizeof(vdb_vsampler_b) << " bytes";

	for(size_t i = 0; i < scalar_grids.size(); ++i) {
		VLOG(1) << scalar_grids[i]->getName() << " memory usage: " << scalar_grids[i]->memUsage() / 1024.0f << " kilobytes.\n";
	}

	for(size_t i = 0; i < vector_grids.size(); ++i) {
		VLOG(1) << vector_grids[i]->getName() << " memory usage: " << vector_grids[i]->memUsage() / 1024.0f << " kilobytes.\n";
	}

	need_update = false;
}

void OpenVDBManager::device_free(Device *device, DeviceScene *dscene)
{
	(void)device;
	(void)dscene;
}

#else

OpenVDBManager::OpenVDBManager()
{
	need_update = false;
}

OpenVDBManager::~OpenVDBManager()
{
}

int OpenVDBManager::add_volume(const string &/*filename*/, const string &/*name*/, int /*sampling*/, int /*grid_type*/)
{
	return -1;
}

void OpenVDBManager::device_update(Device */*device*/, DeviceScene */*dscene*/, Scene */*scene*/, Progress &/*progress*/)
{
}

void OpenVDBManager::device_free(Device */*device*/, DeviceScene */*dscene*/)
{
}

#endif

CCL_NAMESPACE_END
