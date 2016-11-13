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

#include "attribute.h"
#include "../render/volume.h"

#include "util_foreach.h"

#include <openvdb/openvdb.h>

CCL_NAMESPACE_BEGIN

static Attribute *get_openvdb_attribute(Volume *volume, const string& filename, const ustring& name)
{
	Attribute *attr = NULL;

	openvdb::initialize();

	openvdb::io::File file(filename);
	file.open();

	openvdb::GridBase::ConstPtr grid = file.readGrid(name.string());

	openvdb::Name value_type = grid->valueType();

	if(value_type == "float") {
		attr = volume->attributes.add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VOXEL);
		fprintf(stderr, "Adding volume attribute: %s\n", name.string().c_str());
	}
	else if(value_type == "vec3s") {
		if (grid->getMetadata< openvdb::TypedMetadata<bool> >("is_color")) {
			attr = volume->attributes.add(name, TypeDesc::TypeColor, ATTR_ELEMENT_VOXEL);
			fprintf(stderr, "Adding volume attribute: %s\n", name.string().c_str());
		}
		else {
			attr = volume->attributes.add(name, TypeDesc::TypeVector, ATTR_ELEMENT_VOXEL);
			fprintf(stderr, "Adding volume attribute: %s\n", name.string().c_str());
		}
	}
	else {
		fprintf(stderr, "Skipping volume attribute: %s\n", name.string().c_str());
	}

	return attr;
}

static void create_volume_attribute(BL::Object& b_ob,
                                    Volume *volume,
                                    VolumeManager *volume_manager,
                                    const ustring& name,
                                    float /*frame*/)
{
	BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

	if(!b_domain) {
		fprintf(stderr, "No domain found!\n");
		return;
	}

	char filename[1024];
	SmokeDomainSettings_cache_filename_get(&b_domain.ptr, filename);

	Attribute *attr = get_openvdb_attribute(volume, filename, name);
	VoxelAttribute *volume_data = attr->data_voxel();
	int slot = volume_manager->add_volume(volume, filename, name.string());

	if (!volume_data) {
		fprintf(stderr, "Failed to create volume data!\n");
	}

	// TODO(kevin): add volume fields to the Volume*
//	volume_data->manager = volume_manager;
	volume_data->slot = slot;
}

static void create_volume_attributes(Scene *scene,
                                     BL::Object& b_ob,
                                     Volume *volume,
                                     float frame)
{
	foreach(Shader *shader, volume->used_shaders) {
		fprintf(stderr, "Number of attribute requests: %lu\n", shader->attributes.requests.size());

		foreach(AttributeRequest req, shader->attributes.requests) {
			ustring name;

			if (req.std == ATTR_STD_VOLUME_DENSITY) {
				name = ustring(Attribute::standard_name(ATTR_STD_VOLUME_DENSITY));
			}
			else if (req.std == ATTR_STD_VOLUME_FLAME) {
				name = ustring(Attribute::standard_name(ATTR_STD_VOLUME_FLAME));
			}
			else if (req.std == ATTR_STD_VOLUME_COLOR) {
				name = ustring(Attribute::standard_name(ATTR_STD_VOLUME_COLOR));
			}
			else if (req.std == ATTR_STD_VOLUME_VELOCITY) {
				name = ustring(Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY));
			}
			else if (req.std == ATTR_STD_VOLUME_HEAT) {
				name = ustring(Attribute::standard_name(ATTR_STD_VOLUME_HEAT));
			}
			else if (req.name != "") {
				name = req.name;
			}
			else {
				continue;
			}

			create_volume_attribute(b_ob, volume, scene->volume_manager, name, frame);
		}
	}
}

Volume *BlenderSync::sync_volume(BL::Object &b_ob)
{
	BL::ID key = b_ob;
	BL::Material material_override = render_layer.material_override;

	/* find shader indices */
	vector<Shader*> used_shaders;
	BL::ID b_ob_data = b_ob.data();

	fprintf(stderr, "%s: before material assignment\n", __func__);

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

	fprintf(stderr, "%s: after material assignment\n", __func__);

	Volume *volume;

	if(!volume_map.sync(&volume, key)) {
		/* test if shaders changed, these can be object level so mesh
		 * does not get tagged for recalc */
		if(volume->used_shaders != used_shaders);
		else {
			/* even if not tagged for recalc, we may need to sync anyway
			 * because the shader needs different volume attributes */
			bool attribute_recalc = false;

			foreach(Shader *shader, volume->used_shaders)
				if(shader->need_update_attributes)
					attribute_recalc = true;

			if(!attribute_recalc)
				return volume;
		}
	}

	/* ensure we only sync instanced meshes once */
	if(volume_synced.find(volume) != volume_synced.end())
		return volume;
	
	volume_synced.insert(volume);
	
	volume->used_shaders = used_shaders;
	volume->name = ustring(b_ob_data.name().c_str());

	fprintf(stderr, "Number of shaders: %lu\n", volume->used_shaders.size());

	create_volume_attributes(scene, b_ob, volume, b_scene.frame_current());

	/* tag update */
	bool rebuild = false;

	volume->tag_update(scene, rebuild);

	return volume;
}

CCL_NAMESPACE_END
