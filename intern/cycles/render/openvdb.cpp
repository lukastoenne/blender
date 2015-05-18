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
	float_samplers_p.reserve(64);
	float_samplers_b.reserve(64);
	vec3s_samplers_p.reserve(64);
	vec3s_samplers_b.reserve(64);
	need_update = true;
}

OpenVDBManager::~OpenVDBManager()
{
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
	/* OpenVDB exceptions all derive from std::exception so it should be fine */
	catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "Unknown error in OpenVDB library..." << std::endl;
	}
}

int OpenVDBManager::add_volume(const string &filename, const string &name, int sampling, int grid_type)
{
	using namespace openvdb;
	size_t slot = -1;

	/* Find existing grid */
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];

		if(grid.filename == filename && grid.name == name) {
			if(grid.sampling == sampling) {
				return grid.slot;
			}
			/* sampling was changed, remove the sampler */
			else {
				if(grid_type == NODE_VDB_FLOAT) {
					if(grid.sampling == OPENVDB_SAMPLE_POINT) {
						delete float_samplers_p[grid.slot];
						float_samplers_p[grid.slot] = NULL;
					}
					else {
						delete float_samplers_b[grid.slot];
						float_samplers_b[grid.slot] = NULL;
					}
				}
				else {
					if(grid.sampling == OPENVDB_SAMPLE_POINT) {
						delete vec3s_samplers_p[grid.slot];
						vec3s_samplers_p[grid.slot] = NULL;
					}
					else {
						delete vec3s_samplers_b[grid.slot];
						vec3s_samplers_b[grid.slot] = NULL;
					}
				}

				/* remove grid description too */
				std::swap(current_grids[i], current_grids.back());
				current_grids.pop_back();
				break;
			}
		}
	}

	try {
		io::File file(filename);
		file.open();

		if(grid_type == NODE_VDB_FLOAT) {
			FloatGrid::Ptr fgrid = gridPtrCast<FloatGrid>(file.readGrid(name));

			if(sampling == OPENVDB_SAMPLE_POINT) {
				vdb_fsampler_p *sampler = new vdb_fsampler_p(fgrid->tree(), fgrid->transform());

				for(slot = 0; slot < float_samplers_p.size(); slot++) {
					if(!float_samplers_p[slot]) {
						break;
					}
				}
				float_samplers_p.insert(float_samplers_p.begin() + slot, sampler);
			}
			else {
				vdb_fsampler_b *sampler = new vdb_fsampler_b(fgrid->tree(), fgrid->transform());

				for(slot = 0; slot < float_samplers_b.size(); slot++) {
					if(!float_samplers_b[slot]) {
						break;
					}
				}
				float_samplers_b.insert(float_samplers_b.begin() + slot, sampler);
			}

			scalar_grids.insert(scalar_grids.begin() + slot, fgrid);
		}
		else if(grid_type == NODE_VDB_VEC3S) {
			Vec3SGrid::Ptr vgrid = gridPtrCast<Vec3SGrid>(file.readGrid(name));

			if(sampling == OPENVDB_SAMPLE_POINT) {
				vdb_vsampler_p *sampler = new vdb_vsampler_p(vgrid->tree(), vgrid->transform());

				for(slot = 0; slot < vec3s_samplers_p.size(); slot++) {
					if(!vec3s_samplers_p[slot]) {
						break;
					}
				}
				vec3s_samplers_p.insert(vec3s_samplers_p.begin() + slot, sampler);
			}
			else {
				vdb_vsampler_b *sampler = new vdb_vsampler_b(vgrid->tree(), vgrid->transform());

				for(slot = 0; slot < vec3s_samplers_b.size(); slot++) {
					if(!vec3s_samplers_b[slot]) {
						break;
					}
				}
				vec3s_samplers_b.insert(vec3s_samplers_b.begin() + slot, sampler);
			}

			vector_grids.insert(vector_grids.begin() + slot, vgrid);
		}
	}
	catch (...) {
		catch_exceptions();
	}

	GridDescription descr;
	descr.filename = filename;
	descr.name = name;
	descr.sampling = sampling;
	descr.slot = slot;

	current_grids.push_back(descr);

	need_update = true;

	return slot;
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
		device->const_copy_to("__vdb_float_samplers_p", float_samplers_p[i], i);
	}

	for(size_t i = 0; i < float_samplers_b.size(); ++i) {
		device->const_copy_to("__vdb_float_samplers_b", float_samplers_b[i], i);
	}

	for(size_t i = 0; i < vec3s_samplers_p.size(); ++i) {
		device->const_copy_to("__vdb_vec3s_samplers_p", vec3s_samplers_p[i], i);
	}

	for(size_t i = 0; i < vec3s_samplers_b.size(); ++i) {
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

int OpenVDBManager::add_volume(const string &filename, const string &name, int sampling, int grid_type)
{
	(void)filename;
	(void)name;
	(void)sampling;
	(void)grid_type;

	return -1;
}

void OpenVDBManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress)
{
	(void)device;
	(void)dscene;
	(void)scene;
	(void)progress;
}

void OpenVDBManager::device_free(Device *device, DeviceScene *dscene)
{
	(void)device;
	(void)dscene;
}

#endif

CCL_NAMESPACE_END
