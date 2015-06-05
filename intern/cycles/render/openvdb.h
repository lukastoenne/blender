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

#ifndef __VOLUMEMANAGER_H__
#define __VOLUMEMANAGER_H__

#include "util_openvdb.h"
#include "util_string.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

class VolumeManager {
	struct GridDescription {
		string filename;
		string name;
		int sampling;
		int slot;
	};

	vector<GridDescription> current_grids;

#ifdef WITH_OPENVDB
	vector<openvdb::FloatGrid::Ptr> scalar_grids;
	vector<openvdb::Vec3SGrid::Ptr> vector_grids;
#endif

	void delete_volume(int grid_type, int sampling, size_t slot);

	void add_grid_description(const string& filename, const string& name, int sampling, int slot);
	int find_existing_slot(const string& filename, const string& name, int sampling, int grid_type);

	bool is_openvdb_file(const string& filename) const;
	size_t add_openvdb_volume(const string& filename, const string& name, int sampling, int grid_type);

public:
	VolumeManager();
	~VolumeManager();

	int add_volume(const string& filename, const string& name, int sampling, int grid_type);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	bool need_update;
	/* index for the density field, the one that will be used for ray intersection */
	int density_index;

	vector<float_volume*> float_volumes;
	vector<float3_volume*> float3_volumes;
};

CCL_NAMESPACE_END

#endif /* __VOLUMEMANAGER_H__ */
