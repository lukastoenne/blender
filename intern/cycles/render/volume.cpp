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

#include "scene.h"
#include "volume.h"

#include "util_foreach.h"
#include "util_logging.h"
#include "util_progress.h"
#include "util_task.h"

#include "../kernel/openvdb/vdb_globals.h"

CCL_NAMESPACE_BEGIN

#define MAX_VOLUME 1024

VolumeManager::VolumeManager()
{
#ifdef WITH_OPENVDB
	openvdb::initialize();

	current_grids.reserve(64);
#endif

	need_update = true;
	num_float_volume = 0;
	num_float3_volume = 0;
}

VolumeManager::~VolumeManager()
{
	for (size_t i = 0; i < volumes.size(); ++i) {
		Volume *volume = volumes[i];

#ifdef WITH_OPENVDB
		volume->scalar_grids.clear();
		volume->vector_grids.clear();
#endif
	}

	current_grids.clear();
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

#if 0
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
#endif

int VolumeManager::add_volume(Volume *volume, const std::string &filename, const std::string &name)
{
	size_t slot = -1;

	if((slot = find_existing_slot(volume, filename, name)) != -1) {
		return slot;
	}

	if((num_float_volume + num_float3_volume + 1) > MAX_VOLUME) {
		printf("VolumeManager::add_volume: volume limit reached %d!\n", MAX_VOLUME);
		return -1;
	}

	try {
		if(is_openvdb_file(filename)) {
			slot = add_openvdb_volume(volume, filename, name);
		}

		add_grid_description(volume, filename, name, slot);

		volumes.push_back(volume);
		need_update = true;
	}
	catch(...) {
		catch_exceptions();
		need_update = false;
		slot = -1;
	}

	return slot;
}

#if 0
int VolumeManager::find_existing_slot(const string& filename, const string& name, int sampling, int grid_type)
{
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];

		if(grid.filename == filename && grid.name == name) {
			if(grid.sampling == sampling) {
				return grid.slot;
			}
			else {
				/* remove the grid description too */
				std::swap(current_grids[i], current_grids.back());
				current_grids.pop_back();
				break;
			}
		}
	}

	return -1;
}
#endif

int VolumeManager::find_existing_slot(Volume *volume, const std::string &filename, const std::string &name)
{
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];

		if(grid.volume == volume) {
			if(grid.filename == filename && grid.name == name) {
				return grid.slot;
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
	for (size_t i = 0; i < volumes.size(); ++i) {
		Volume *volume = volumes[i];
		
		if (!volume->scalar_grids.empty()) {
			return 0;
		}
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

#if 0
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
		slot = find_empty_slot(scalar_grids);
		if (slot == -1)
			return -1;
		
		scalar_grids.insert(scalar_grids.begin() + slot, gridPtrCast<FloatGrid>(grid));
	}
	else if(grid_type == NODE_VDB_FLOAT3) {
		slot = find_empty_slot(vector_grids);
		if (slot == -1)
			return -1;
		
		vector_grids.insert(vector_grids.begin() + slot, gridPtrCast<Vec3SGrid>(grid));
	}
#else
	(void)filename;
	(void)name;
	(void)grid_type;
#endif

	return slot;
}
#endif

size_t VolumeManager::add_openvdb_volume(Volume *volume, const std::string &filename, const std::string &name)
{
	size_t slot = -1;

#ifdef WITH_OPENVDB
	openvdb::io::File file(filename);
	file.open();

	if(!file.hasGrid(name)) return -1;

	openvdb::GridBase::Ptr grid = file.readGrid(name);
	if(grid->getGridClass() == openvdb::GRID_LEVEL_SET) return -1;

	if(grid->isType<openvdb::FloatGrid>()) {
		openvdb::FloatGrid::Ptr fgrid = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);
		volume->scalar_grids.push_back(fgrid);

		/* XXX Ray intersectors only support uniform grids.
		 * Can we make this transparent somehow? - lukas
		 */
		assert(fgrid->hasUniformVoxels());

		slot = num_float_volume++;
	}
	else if(grid->isType<openvdb::Vec3SGrid>()) {
		openvdb::Vec3SGrid::Ptr vgrid = openvdb::gridPtrCast<openvdb::Vec3SGrid>(grid);
		volume->vector_grids.push_back(vgrid);

		slot = num_float3_volume++;
	}
#else
	(void)volume;
	(void)filename;
	(void)name;
#endif

	return slot;
}

#if 0
void VolumeManager::add_grid_description(const string& filename, const string& name, int sampling, int slot)
{
	GridDescription descr;
	descr.filename = filename;
	descr.name = name;
	descr.sampling = sampling;
	descr.slot = slot;

	current_grids.push_back(descr);
}
#endif

void VolumeManager::add_grid_description(Volume *volume, const std::string &filename, const std::string &name, int slot)
{
	GridDescription descr;
	descr.filename = filename;
	descr.name = name;
	descr.volume = volume;
	descr.slot = slot;

	current_grids.push_back(descr);
}

static void update_attribute_element_offset(Attribute *vattr,
                                            TypeDesc& type,
                                            int& offset,
                                            AttributeElement& element)
{
	if(vattr) {
		/* store element and type */
		element = vattr->element;
		type = vattr->type;

		/* store slot in offset value */
		VoxelAttribute *voxel_data = vattr->data_voxel();
		offset = voxel_data->slot;
	}
	else {
		/* attribute not found */
		element = ATTR_ELEMENT_NONE;
		offset = 0;
	}
}

void VolumeManager::device_update_attributes(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	progress.set_status("Updating Volume", "Computing attributes");

	/* gather per volume requested attributes. as volumes may have multiple
	 * shaders assigned, this merges the requested attributes that have
	 * been set per shader by the shader manager */
	vector<AttributeRequestSet> volume_attributes(volumes.size());

	for(size_t i = 0; i < volumes.size(); i++) {
		Volume *volume = volumes[i];

		foreach(Shader *shader, volume->used_shaders) {
			volume_attributes[i].add(shader->attributes);
		}
	}

	for(size_t i = 0; i < volumes.size(); i++) {
		Volume *volume = volumes[i];
		AttributeRequestSet& attributes = volume_attributes[i];

		/* todo: we now store std and name attributes from requests even if
		 * they actually refer to the same mesh attributes, optimize */
		foreach(AttributeRequest& req, attributes.requests) {
			Attribute *vattr = volume->attributes.find(req);

			update_attribute_element_offset(vattr,
			                                req.triangle_type,
			                                req.triangle_desc.offset,
			                                req.triangle_desc.element);

			if(progress.get_cancel()) return;
		}
	}

	update_svm_attributes(device, dscene, scene, volume_attributes);
}

void VolumeManager::update_svm_attributes(Device *device, DeviceScene *dscene, Scene *scene, vector<AttributeRequestSet>& mesh_attributes)
{
	/* compute array stride */
	int attr_map_stride = 0;

	for(size_t i = 0; i < volumes.size(); i++) {
		attr_map_stride = max(attr_map_stride, (mesh_attributes[i].size() + 1));
	}

	if(attr_map_stride == 0) {
		return;
	}

	/* create attribute map */
	uint4 *attr_map = dscene->attributes_map.resize(attr_map_stride*volumes.size());
	memset(attr_map, 0, dscene->attributes_map.size()*sizeof(uint));

	for(size_t i = 0; i < volumes.size(); i++) {
		AttributeRequestSet& attributes = mesh_attributes[i];

		/* set object attributes */
		int index = i*attr_map_stride;

		foreach(AttributeRequest& req, attributes.requests) {
			uint id = scene->shader_manager->get_attribute_id(req.name);

			attr_map[index].x = id;
			attr_map[index].y = req.triangle_desc.element;
			attr_map[index].z = as_uint(req.triangle_desc.offset);

			if(req.triangle_type == TypeDesc::TypeFloat)
				attr_map[index].w = NODE_ATTR_FLOAT;
			else
				attr_map[index].w = NODE_ATTR_FLOAT3;

			index++;
		}

		/* terminator */
		attr_map[index].x = ATTR_STD_NONE;
		attr_map[index].y = 0;
		attr_map[index].z = 0;
		attr_map[index].w = 0;

		index++;
	}

	device->tex_alloc("__attributes_map", dscene->attributes_map);
}

void VolumeManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update) {
		return;
	}

	device_free(device, dscene);
	progress.set_status("Updating OpenVDB volumes", "Sending volumes to device.");

	uint *vol_shader = dscene->vol_shader.resize(num_float_volume + num_float3_volume);
	int s = 0;

	for (size_t i = 0; i < volumes.size(); ++i) {
		Volume *volume = volumes[i];

		for(size_t i = 0; i < volume->scalar_grids.size(); ++i) {
			if(!volume->scalar_grids[i]) {
				continue;
			}

			vol_shader[s++] = scene->shader_manager->get_shader_id(volume->used_shaders[0], false);
		}

		for(size_t i = 0; i < volume->vector_grids.size(); ++i) {
			if(!volume->vector_grids[i]) {
				continue;
			}

			vol_shader[s++] = scene->shader_manager->get_shader_id(volume->used_shaders[0], false);
		}

		if(progress.get_cancel()) {
			return;
		}
	}

	device->tex_alloc("__vol_shader", dscene->vol_shader);

#ifdef WITH_OPENVDB
	typedef typename OpenVDBGlobals::scalar_grid_t scalar_grid_t;
	typedef typename OpenVDBGlobals::vector_grid_t vector_grid_t;
	typedef typename OpenVDBGlobals::scalar_isector_t scalar_isector_t;
	typedef typename OpenVDBGlobals::vector_isector_t vector_isector_t;
	OpenVDBGlobals *vdb = device->vdb_memory();
	
	vdb->scalar_grids.reserve(num_float_volume);
	vdb->vector_grids.reserve(num_float3_volume);
	vdb->scalar_main_isectors.reserve(num_float_volume);
	vdb->vector_main_isectors.reserve(num_float3_volume);
	for (size_t i = 0; i < volumes.size(); ++i) {
		Volume *volume = volumes[i];
		
		for (size_t k = 0; k < volume->scalar_grids.size(); ++k) {
			scalar_grid_t *grid = volume->scalar_grids[k].get();
			vdb->scalar_grids.push_back(grid);
			vdb->scalar_main_isectors.push_back(new scalar_isector_t(*grid));
			VLOG(1) << grid->getName().c_str() << " memory usage: " << grid->memUsage() / 1024.0f << " kilobytes.\n";
		}
		for (size_t k = 0; k < volume->vector_grids.size(); ++k) {
			vector_grid_t *grid = volume->vector_grids[k].get();
			vdb->vector_grids.push_back(grid);
			vdb->vector_main_isectors.push_back(new vector_isector_t(*grid));
			VLOG(1) << grid->getName().c_str() << " memory usage: " << grid->memUsage() / 1024.0f << " kilobytes.\n";
		}
	}
#endif

	if(progress.get_cancel()) {
		return;
	}

	dscene->data.tables.num_volumes = num_float_volume/* + float3_volumes.size()*/;
	dscene->data.tables.density_index = 0;

	need_update = false;
}

void VolumeManager::device_free(Device */*device*/, DeviceScene */*dscene*/)
{
}

CCL_NAMESPACE_END
