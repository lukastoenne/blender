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
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

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

OpenVDBManager::OpenVDBManager()
{
	openvdb::initialize();
	need_update = true;
	float_samplers_p.reserve(64);
	float_samplers_b.reserve(64);
	vec3s_samplers_p.reserve(64);
	vec3s_samplers_b.reserve(64);
}

OpenVDBManager::~OpenVDBManager()
{
	float_samplers_p.clear();
	float_samplers_b.clear();
	vec3s_samplers_p.clear();
	vec3s_samplers_b.clear();
}

int OpenVDBManager::add_volume(const string &filename, const string &name, int sampling, int grid_type)
{
	using namespace openvdb;
	size_t slot = -1;

	try {
		io::File file(filename);
		file.open();

		for(io::File::NameIterator name_iter = file.beginName();
		    name_iter != file.endName();
		    ++name_iter)
		{
			std::cout << name_iter.gridName() << std::endl;
		}

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

	need_update = false;
}

void OpenVDBManager::device_free(Device *device, DeviceScene *dscene)
{
	(void)device;
	(void)dscene;
}

CCL_NAMESPACE_END
