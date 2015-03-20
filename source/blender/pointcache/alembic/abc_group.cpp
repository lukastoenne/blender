/*
 * Copyright 2013, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <map>
#include <sstream>
#include <string>

#include <Alembic/Abc/IObject.h>
#include <Alembic/Abc/OObject.h>

#include "abc_mesh.h"
#include "abc_group.h"
#include "abc_object.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "BKE_anim.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcGroupWriter::AbcGroupWriter(const std::string &name, Group *group) :
    GroupWriter(group, name)
{
}

void AbcGroupWriter::init_abc()
{
	if (m_abc_object)
		return;
	
	m_abc_object = abc_archive()->add_id_object<OObject>((ID *)m_group);
}

void AbcGroupWriter::create_refs()
{
	GroupObject *gob = (GroupObject *)m_group->gobject.first;
	int i = 0;
	for (; gob; gob = gob->next, ++i) {
		OObject abc_object = abc_archive()->get_id_object((ID *)gob->ob);
		if (abc_object) {
			std::stringstream ss;
			ss << i;
			m_abc_object.addChildInstance(abc_object, std::string("group_object")+ss.str());
		}
	}
}

void AbcGroupWriter::write_sample()
{
	if (!m_abc_object)
		return;
}


AbcGroupReader::AbcGroupReader(const std::string &name, Group *group) :
    GroupReader(group, name)
{
}

void AbcGroupReader::init_abc()
{
	if (m_abc_object)
		return;
	m_abc_object = abc_archive()->get_id_object((ID *)m_group);
}

PTCReadSampleResult AbcGroupReader::read_sample(float frame)
{
	if (!m_abc_object)
		return PTC_READ_SAMPLE_INVALID;
	
	return PTC_READ_SAMPLE_EXACT;
}

/* ========================================================================= */

AbcDupligroupWriter::AbcDupligroupWriter(const std::string &name, EvaluationContext *eval_ctx, Scene *scene, Group *group) :
    GroupWriter(group, name),
    m_eval_ctx(eval_ctx),
    m_scene(scene)
{
}

AbcDupligroupWriter::~AbcDupligroupWriter()
{
	for (IDWriterMap::iterator it = m_id_writers.begin(); it != m_id_writers.end(); ++it) {
		if (it->second)
			delete it->second;
	}
}

void AbcDupligroupWriter::init_abc()
{
	if (m_abc_group)
		return;
	
	m_abc_group = abc_archive()->add_id_object<OObject>((ID *)m_group);
}

void AbcDupligroupWriter::write_sample_object(Object *ob)
{
	AbcWriter *ob_writer = find_id_writer((ID *)ob);
	if (!ob_writer) {
		ob_writer = new AbcObjectWriter(ob->id.name, m_scene, ob);
		ob_writer->init(abc_archive());
		m_id_writers.insert(IDWriterPair((ID *)ob, ob_writer));
	}
	
	ob_writer->write_sample();
}

void AbcDupligroupWriter::write_sample_dupli(DupliObject *dob, int index)
{
	OObject abc_object = abc_archive()->get_id_object((ID *)dob->ob);
	if (!abc_object)
		return;
	
	std::stringstream ss;
	ss << "DupliObject" << index;
	std::string name = ss.str();
	
	OObject abc_dupli = m_abc_group.getChild(name);
	OCompoundProperty props;
	OM44fProperty prop_matrix;
	if (!abc_dupli) {
		abc_dupli = OObject(m_abc_group, name, 0);
		m_object_writers.push_back(abc_dupli.getPtr());
		props = abc_dupli.getProperties();
		
		abc_dupli.addChildInstance(abc_object, "object");
		
		prop_matrix = OM44fProperty(props, "matrix", 0);
		m_property_writers.push_back(prop_matrix.getPtr());
	}
	else {
		props = abc_dupli.getProperties();
		
		prop_matrix = OM44fProperty(props.getProperty("matrix").getPtr()->asScalarPtr(), kWrapExisting);
	}
	
	prop_matrix.set(M44f(dob->mat));
}

void AbcDupligroupWriter::write_sample()
{
	if (!m_abc_group)
		return;
	
	ListBase *duplilist = group_duplilist_ex(m_eval_ctx, m_scene, m_group, true);
	DupliObject *dob;
	int i;
	
	/* LIB_DOIT is used to mark handled objects, clear first */
	for (dob = (DupliObject *)duplilist->first; dob; dob = dob->next) {
		if (dob->ob)
			dob->ob->id.flag &= ~LIB_DOIT;
	}
	
	/* write actual object data: duplicator itself + all instanced objects */
	for (dob = (DupliObject *)duplilist->first; dob; dob = dob->next) {
		if (dob->ob->id.flag & LIB_DOIT)
			continue;
		dob->ob->id.flag |= LIB_DOIT;
		
		write_sample_object(dob->ob);
	}
	
	/* write dupli instances */
	for (dob = (DupliObject *)duplilist->first, i = 0; dob; dob = dob->next, ++i) {
		write_sample_dupli(dob, i);
	}
	
	free_object_duplilist(duplilist);
}

AbcWriter *AbcDupligroupWriter::find_id_writer(ID *id) const
{
	IDWriterMap::const_iterator it = m_id_writers.find(id);
	if (it == m_id_writers.end())
		return NULL;
	else
		return it->second;
}

/* ------------------------------------------------------------------------- */

AbcDupliCacheReader::AbcDupliCacheReader(const std::string &name, Group *group, DupliCache *dupli_cache) :
    GroupReader(group, name),
    dupli_cache(dupli_cache)
{
	/* XXX this mapping allows fast lookup of existing objects in Blender data
	 * to associate with duplis. Later i may be possible to create instances of
	 * non-DNA data, but for the time being this is a requirement due to other code parts (drawing, rendering)
	 */
	build_object_map(G.main, group);
}

AbcDupliCacheReader::~AbcDupliCacheReader()
{
}

void AbcDupliCacheReader::init_abc()
{
}

void AbcDupliCacheReader::read_dupligroup_object(IObject object, float frame)
{
	if (GS(object.getName().c_str()) == ID_OB) {
		/* instances are handled later, we create true object data here */
		if (object.isInstanceDescendant())
			return;
		
		Object *b_ob = find_object(object.getName());
		if (!b_ob)
			return;
		
		AbcDerivedMeshReader dm_reader("mesh", b_ob);
		dm_reader.init(abc_archive());
		dm_reader.init_abc(object);
		if (dm_reader.read_sample(frame) != PTC_READ_SAMPLE_INVALID) {
			DerivedMesh *dm = dm_reader.acquire_result();
			DupliObjectData *data = BKE_dupli_cache_add_mesh(dupli_cache, b_ob, dm);
			insert_dupli_data(object.getPtr(), data);
		}
		else
			dm_reader.discard_result();
	}
}

void AbcDupliCacheReader::read_dupligroup_group(IObject abc_group, const ISampleSelector &ss)
{
	if (GS(abc_group.getName().c_str()) == ID_GR) {
		size_t num_child = abc_group.getNumChildren();
		
		for (size_t i = 0; i < num_child; ++i) {
			IObject abc_dupli = abc_group.getChild(i);
			ICompoundProperty props = abc_dupli.getProperties();
			
			IM44fProperty prop_matrix(props, "matrix", 0);
			M44f abc_matrix = prop_matrix.getValue(ss);
			float matrix[4][4];
			memcpy(matrix, abc_matrix.getValue(), sizeof(float)*4*4);
			
			IObject abc_dupli_object = abc_dupli.getChild("object");
			if (abc_dupli_object.isInstanceRoot()) {
				DupliObjectData *dupli_data = find_dupli_data(abc_dupli_object.getPtr());
				if (dupli_data) {
					BKE_dupli_cache_add_instance(dupli_cache, matrix, dupli_data);
				}
			}
		}
	}
}

PTCReadSampleResult AbcDupliCacheReader::read_sample(float frame)
{
	ISampleSelector ss = abc_archive()->get_frame_sample_selector(frame);
	
	IObject abc_top = abc_archive()->root();
	IObject abc_group = abc_archive()->get_id_object((ID *)m_group);
	if (!abc_group)
		return PTC_READ_SAMPLE_INVALID;
	
	/* first create shared object data */
	for (size_t i = 0; i < abc_top.getNumChildren(); ++i) {
		read_dupligroup_object(abc_top.getChild(i), frame);
	}
	
	/* now generate dupli instances for the group */
	read_dupligroup_group(abc_group, ss);
	
	return PTC_READ_SAMPLE_EXACT;
}

DupliObjectData *AbcDupliCacheReader::find_dupli_data(ObjectReaderPtr ptr) const
{
	DupliMap::const_iterator it = dupli_map.find(ptr);
	if (it == dupli_map.end())
		return NULL;
	else
		return it->second;
}

void AbcDupliCacheReader::insert_dupli_data(ObjectReaderPtr ptr, DupliObjectData *data)
{
	dupli_map.insert(DupliPair(ptr, data));
}

void AbcDupliCacheReader::build_object_map(Main *bmain, Group *group)
{
	BKE_main_id_tag_idcode(bmain, ID_OB, false);
	BKE_main_id_tag_idcode(bmain, ID_GR, false);
	object_map.clear();
	
	build_object_map_add_group(group);
}

Object *AbcDupliCacheReader::find_object(const std::string &name) const
{
	ObjectMap::const_iterator it = object_map.find(name);
	if (it == object_map.end())
		return NULL;
	else
		return it->second;
}

void AbcDupliCacheReader::build_object_map_add_group(Group *group)
{
	if (group->id.flag & LIB_DOIT)
		return;
	group->id.flag |= LIB_DOIT;
	
	for (GroupObject *gob = (GroupObject *)group->gobject.first; gob; gob = gob->next) {
		Object *ob = gob->ob;
		if (ob->id.flag & LIB_DOIT)
			continue;
		ob->id.flag |= LIB_DOIT;
		object_map.insert(ObjectPair(ob->id.name, ob));
		
		if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
			build_object_map_add_group(ob->dup_group);
		}
	}
}

/* ------------------------------------------------------------------------- */

AbcDupliObjectReader::AbcDupliObjectReader(const std::string &name, Object *ob, DupliObjectData *dupli_data) :
    ObjectReader(ob, name),
    dupli_data(dupli_data)
{
}

AbcDupliObjectReader::~AbcDupliObjectReader()
{
}

void AbcDupliObjectReader::init_abc()
{
}

void AbcDupliObjectReader::read_dupligroup_object(IObject object, float frame)
{
	if (GS(object.getName().c_str()) == ID_OB) {
		/* instances are handled later, we create true object data here */
		if (object.isInstanceDescendant())
			return;
		
		AbcDerivedMeshReader dm_reader("mesh", m_ob);
		dm_reader.init(abc_archive());
		dm_reader.init_abc(object);
		if (dm_reader.read_sample(frame) != PTC_READ_SAMPLE_INVALID) {
			DerivedMesh *dm = dm_reader.acquire_result();
			BKE_dupli_object_data_init(dupli_data, m_ob, dm);
		}
		else
			dm_reader.discard_result();
	}
}

PTCReadSampleResult AbcDupliObjectReader::read_sample(float frame)
{
	IObject abc_object = abc_archive()->get_id_object((ID *)m_ob);
	if (!abc_object)
		return PTC_READ_SAMPLE_INVALID;
	
	read_dupligroup_object(abc_object, frame);
	
	return PTC_READ_SAMPLE_EXACT;
}

DupliObjectData *AbcDupliObjectReader::find_dupli_data(ObjectReaderPtr ptr) const
{
	DupliMap::const_iterator it = dupli_map.find(ptr);
	if (it == dupli_map.end())
		return NULL;
	else
		return it->second;
}

void AbcDupliObjectReader::insert_dupli_data(ObjectReaderPtr ptr, DupliObjectData *data)
{
	dupli_map.insert(DupliPair(ptr, data));
}

} /* namespace PTC */
