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

#include "attribute.h"

#include "util_string.h"
#include "util_types.h"
#include "util_volume.h"

#ifdef WITH_OPENVDB
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/RayIntersector.h>
#endif

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;
class Shader;

class Volume {
public:
	vector<Shader*> used_shaders;
	AttributeSet attributes;
	string name;

#ifdef WITH_OPENVDB
	vector<openvdb::FloatGrid::Ptr> scalar_grids;
	vector<openvdb::Vec3SGrid::Ptr> vector_grids;
#endif
	
	void tag_update(Scene *scene, bool rebuild);
};

class VolumeManager {
	struct GridDescription {
		Volume *volume;
		string filename;
		string name;
		int sampling;
		int slot;
	};

	vector<GridDescription> current_grids;
	int num_float_volume;
	int num_float3_volume;

	void delete_volume(int grid_type, int sampling, size_t slot);

#if 0
	void add_grid_description(const string& filename, const string& name, int sampling, int slot);
#endif
	void add_grid_description(Volume *volume, const string& filename, const string& name, int slot);
#if 0
	int find_existing_slot(const string& filename, const string& name, int sampling, int grid_type);
#endif
	int find_existing_slot(Volume *volume, const string& filename, const string& name);

	bool is_openvdb_file(const string& filename) const;
#if 0
	size_t add_openvdb_volume(const string& filename, const string& name, int sampling, int grid_type);
#endif
	size_t add_openvdb_volume(Volume *volume, const string& filename, const string& name);

public:
	VolumeManager();
	~VolumeManager();

#if 0
	int add_volume(const string& filename, const string& name, int sampling, int grid_type);
#endif
	int add_volume(Volume *volume, const string& filename, const string& name);
	int find_density_slot();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_attributes(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void update_svm_attributes(Device *device, DeviceScene *dscene, Scene *scene, vector<AttributeRequestSet>& mesh_attributes);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

	bool need_update;

	vector<Volume*> volumes;
};

CCL_NAMESPACE_END

#endif /* __VOLUMEMANAGER_H__ */
