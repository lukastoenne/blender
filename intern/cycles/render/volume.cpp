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

#include "volume.h"
#include "scene.h"
#include "util_logging.h"
#include "util_progress.h"
#include "util_task.h"

CCL_NAMESPACE_BEGIN

#define MAX_VOLUME 1024

VolumeManager::VolumeManager()
{
#ifdef WITH_OPENVDB
	openvdb::initialize();

	scalar_grids.reserve(64);
	vector_grids.reserve(64);
	current_grids.reserve(64);
	float_volumes.reserve(64);
	float3_volumes.reserve(64);
#endif

	need_update = true;
}

VolumeManager::~VolumeManager()
{
#ifdef WITH_OPENVDB
	for(size_t i = 0; i < float_volumes.size(); ++i) {
		delete float_volumes[i];
	}

	for(size_t i = 0; i < float3_volumes.size(); ++i) {
		delete float3_volumes[i];
	}

	scalar_grids.clear();
	vector_grids.clear();
	current_grids.clear();
#endif
}

static inline void catch_exceptions()
{
#ifdef WITH_OPENVDB
	try {
		throw;
	}
	catch(const openvdb::IoError& e) {
		std::cerr << e.what() << "\n";
	}
#endif
}

int VolumeManager::add_volume(const string& filename, const string& name, int sampling, int grid_type)
{
	size_t slot = -1;

	if((slot = find_existing_slot(filename, name, sampling, grid_type)) != -1) {
		return slot;
	}

	try {
		if(is_openvdb_file(filename)) {
			slot = add_openvdb_volume(filename, name, sampling, grid_type);
		}

		add_grid_description(filename, name, sampling, slot);

		need_update = true;
	}
	catch(...) {
		catch_exceptions();
		need_update = false;
		slot = -1;
	}

	return slot;
}

int VolumeManager::find_existing_slot(const string& filename, const string& name, int sampling, int grid_type)
{
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];

		if(grid.filename == filename && grid.name == name) {
			if(grid.sampling == sampling) {
				return grid.slot;
			}
			else {
				/* sampling was changed, remove the volume */
				if(grid_type == NODE_VDB_FLOAT) {
					delete float_volumes[grid.slot];
					float_volumes[grid.slot] = NULL;
				}
				else {
					delete float3_volumes[grid.slot];
					float3_volumes[grid.slot] = NULL;
				}

				/* remove the grid description too */
				std::swap(current_grids[i], current_grids.back());
				current_grids.pop_back();
				break;
			}
		}
	}

	return -1;
}

int VolumeManager::find_density_slot()
{
	/* first try finding a matching grid name */
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];
		
		if(string_iequals(grid.name, "density") || string_iequals(grid.name, "density high"))
			return grid.slot;
	}
	
	/* try using the first scalar float grid instead */
	if(!float_volumes.empty()) {
		return 0;
	}
	
	return -1;
}

bool VolumeManager::is_openvdb_file(const string& filename) const
{
	return string_endswith(filename, ".vdb");
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

	if(slot == container.size()) {
		if(slot == MAX_VOLUME) {
			printf("VolumeManager::add_volume: volume limit reached %d!\n",
			       MAX_VOLUME);
			return -1;
		}

		container.resize(slot + 1);
	}

	return slot;
}

size_t VolumeManager::add_openvdb_volume(const std::string& filename, const std::string& name, int /*sampling*/, int grid_type)
{
	size_t slot = -1;

#ifdef WITH_OPENVDB
	using namespace openvdb;

	io::File file(filename);
	file.open();

	if(!file.hasGrid(name)) return -1;

	GridBase::Ptr grid = file.readGrid(name);
	if(grid->getGridClass() == GRID_LEVEL_SET) return -1;

	if(grid_type == NODE_VDB_FLOAT) {
		slot = find_empty_slot(float_volumes);

		if(slot == -1) return -1;

		FloatGrid::Ptr fgrid = gridPtrCast<FloatGrid>(grid);

		vdb_float_volume *volume = new vdb_float_volume(fgrid);
		volume->create_threads_utils(TaskScheduler::thread_ids());
		float_volumes.insert(float_volumes.begin() + slot, volume);
		scalar_grids.push_back(fgrid);
	}
	else if(grid_type == NODE_VDB_FLOAT3) {
		slot = find_empty_slot(float3_volumes);

		if(slot == -1) return -1;

		Vec3SGrid::Ptr vgrid = gridPtrCast<Vec3SGrid>(grid);

		vdb_float3_volume *volume = new vdb_float3_volume(vgrid);
		volume->create_threads_utils(TaskScheduler::thread_ids());
		float3_volumes.insert(float3_volumes.begin() + slot, volume);
		vector_grids.push_back(vgrid);
	}
#else
	(void)filename;
	(void)name;
	(void)grid_type;
#endif

	return slot;
}

void VolumeManager::add_grid_description(const string& filename, const string& name, int sampling, int slot)
{
	GridDescription descr;
	descr.filename = filename;
	descr.name = name;
	descr.sampling = sampling;
	descr.slot = slot;

	current_grids.push_back(descr);
}

void VolumeManager::device_update(Device *device, DeviceScene *dscene, Scene */*scene*/, Progress& progress)
{
	if(!need_update) {
		return;
	}

	device_free(device, dscene);
	progress.set_status("Updating OpenVDB volumes", "Sending volumes to device.");

	for(size_t i = 0; i < float_volumes.size(); ++i) {
		if(!float_volumes[i]) {
			continue;
		}
		device->const_copy_to("__float_volume", float_volumes[i], i);
	}

	for(size_t i = 0; i < float3_volumes.size(); ++i) {
		if(!float3_volumes[i]) {
			continue;
		}
		device->const_copy_to("__float3_volume", float3_volumes[i], i);
	}

	if(progress.get_cancel()) {
		return;
	}

	dscene->data.tables.num_volumes = float_volumes.size() + float3_volumes.size();
	dscene->data.tables.density_index = find_density_slot();

	VLOG(1) << "Volume allocate: __float_volume, " << float_volumes.size() * sizeof(float_volume) << " bytes";
	VLOG(1) << "Volume allocate: __float3_volume, " << float3_volumes.size() * sizeof(float3_volume) << " bytes";

#ifdef WITH_OPENVDB
	for(size_t i = 0; i < scalar_grids.size(); ++i) {
		VLOG(1) << scalar_grids[i]->getName().c_str() << " memory usage: " << scalar_grids[i]->memUsage() / 1024.0f << " kilobytes.\n";
	}

	for(size_t i = 0; i < vector_grids.size(); ++i) {
		VLOG(1) << vector_grids[i]->getName().c_str() << " memory usage: " << vector_grids[i]->memUsage() / 1024.0f << " kilobytes.\n";
	}
#endif

	need_update = false;
}

void VolumeManager::device_free(Device */*device*/, DeviceScene */*dscene*/)
{
}

CCL_NAMESPACE_END
