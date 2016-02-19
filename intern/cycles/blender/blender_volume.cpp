/*
 * Copyright 2011-2016 Blender Foundation
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

#include "blender_sync.h"

#include "../render/volume.h"

#include <openvdb/openvdb.h>

class Volume {
public:
	vector<uint> used_shaders;
	AttributeSet attributes;
	string name;
};

static Attribute *get_openvdb_attribute(Volume *volume, const string& filename, const ustring& name)
{
	Attribute *attr = NULL;

	openvdb::initialize();

	openvdb::io::File file(filename);
	file.open();

	openvdb::GridBase::ConstPtr grid = file.readGrid(name.string());

	openvdb::Name name = grid->getName();
	openvdb::Name value_type = grid->valueType();

	if(value_type == "float") {
		attr = volume->attributes.add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VOXEL);
	}
	else if(value_type == "vec3s") {
		if (grid->getMetadata< openvdb::TypedMetadata<bool> >("is_color")) {
			attr = volume->attributes.add(name, TypeDesc::TypeColor, ATTR_ELEMENT_VOXEL);
		}
		else {
			attr = volume->attributes.add(name, TypeDesc::TypeVector, ATTR_ELEMENT_VOXEL);
		}
	}

	return attr;
}

static void create_mesh_volume_attribute(BL::Object& b_ob,
                                         Volume *volume,
                                         VolumeManager *volume_manager,
                                         const ustring &name,
                                         float frame)
{
	BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

	if(!b_domain)
		return;

	char filename[1024];
	SmokeDomainSettings_cache_filename_get(&b_domain.ptr, filename);

	Attribute *attr = get_openvdb_attribute(volume, filename, name);
	VoxelAttribute *volume_data = attr->data_voxel();

	// TODO(kevin): add volume fields to the Volume*
//	volume_data->manager = volume_manager;
	volume_data->slot = volume_manager->add_volume(filename, name, 0, 0);
}

static void create_volume_attributes(Scene *scene,
                                     BL::Object& b_ob,
                                     Volume *volume,
                                     float frame)
{
	foreach(uint id, volume->used_shaders) {
		Shader *shader = scene->shaders[id];

		foreach(AttributeRequest attribute, shader->attributes.requests) {
			if (attribute.name == "") {
				continue;
			}

			create_volume_attribute(b_ob, volume, scene->volume_manager, attribute.name, frame);
		}
	}
}

void BlenderSync::sync_volume(BL::Object &b_ob)
{
	BL::Material material_override = render_layer.material_override;

	/* find shader indices */
	vector<uint> used_shaders;

	BL::Object::material_slots_iterator slot;
	for(b_ob.material_slots.begin(slot); slot != b_ob.material_slots.end(); ++slot) {
		if(material_override) {
			find_shader(material_override, used_shaders, scene->default_volume);
		}
		else {
			BL::ID b_material(slot->material());
			find_shader(b_material, used_shaders, scene->default_volume);
		}
	}

	if(used_shaders.size() == 0) {
		if(material_override)
			find_shader(material_override, used_shaders, scene->default_volume);
		else
			used_shaders.push_back(scene->default_volume);
	}

	Volume *volume;
	volume->used_shaders = used_shaders;
	volume->name = ustring(b_ob_data.name().c_str());

	create_volume_attributes(scene, b_ob, volume, b_scene.frame_current());
}
