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

#include "abc_mesh.h"
#include "abc_group.h"

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

void AbcGroupWriter::open_archive(WriterArchive *archive)
{
	BLI_assert(dynamic_cast<AbcWriterArchive*>(archive));
	AbcWriter::abc_archive(static_cast<AbcWriterArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_object = abc_archive()->add_id_object<OObject>((ID *)m_group);
	}
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
	if (!abc_archive()->archive)
		return;
}


AbcGroupReader::AbcGroupReader(const std::string &name, Group *group) :
    GroupReader(group, name)
{
}

void AbcGroupReader::open_archive(ReaderArchive *archive)
{
	BLI_assert(dynamic_cast<AbcReaderArchive*>(archive));
	AbcReader::abc_archive(static_cast<AbcReaderArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_object = abc_archive()->get_id_object((ID *)m_group);
	}
}

PTCReadSampleResult AbcGroupReader::read_sample(float frame)
{
	if (!m_abc_object)
		return PTC_READ_SAMPLE_INVALID;
	
	return PTC_READ_SAMPLE_EXACT;
}

/* ========================================================================= */

typedef float Matrix[4][4];

typedef float (*MatrixPtr)[4];

static Matrix I = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};

struct DupliGroupContext {
	typedef std::map<ObjectReaderPtr, DupliObjectData*> DupliMap;
	typedef std::pair<ObjectReaderPtr, DupliObjectData*> DupliPair;
	
	struct Transform {
		Transform() {}
		Transform(float (*value)[4]) { copy_m4_m4(matrix, value); }
		Transform(const Transform &tfm) { memcpy(matrix, tfm.matrix, sizeof(Matrix)); }
		
		Matrix matrix;
	};
	typedef std::vector<Transform> TransformStack;
	
	typedef std::map<std::string, Object*> ObjectMap;
	typedef std::pair<std::string, Object*> ObjectPair;
	
	/* constructor */
	DupliGroupContext(DupliCache *dupli_cache) :
	    dupli_cache(dupli_cache)
	{
		tfm_stack.push_back(Transform(I));
	}
	
	
	DupliObjectData *find_dupli_data(ObjectReaderPtr ptr) const
	{
		DupliMap::const_iterator it = dupli_map.find(ptr);
		if (it == dupli_map.end())
			return NULL;
		else
			return it->second;
	}
	
	void insert_dupli_data(ObjectReaderPtr ptr, DupliObjectData *data)
	{
		dupli_map.insert(DupliPair(ptr, data));
	}
	
	
	MatrixPtr get_transform() { return tfm_stack.back().matrix; }
//	void push_transform(float mat[4][4])
	
	
	void build_object_map(Main *bmain, Group *group)
	{
		BKE_main_id_tag_idcode(bmain, ID_OB, false);
		BKE_main_id_tag_idcode(bmain, ID_GR, false);
		object_map.clear();
		
		build_object_map_add_group(group);
	}
	
	Object *find_object(const std::string &name) const
	{
		ObjectMap::const_iterator it = object_map.find(name);
		if (it == object_map.end())
			return NULL;
		else
			return it->second;
	}
	
	DupliMap dupli_map;
	DupliCache *dupli_cache;
	
	TransformStack tfm_stack;
	
	ObjectMap object_map;
	
protected:
	void build_object_map_add_group(Group *group)
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
};

static void read_dupligroup_object(DupliGroupContext &ctx, IObject object, float frame)
{
	if (GS(object.getName().c_str()) == ID_OB) {
		/* instances are handled later, we create true object data here */
		if (object.isInstanceDescendant())
			return;
		
		Object *b_ob = ctx.find_object(object.getName());
		if (!b_ob)
			return;
		
		/* TODO load DM, from subobjects for IPolyMesh etc. */
		DerivedMesh *dm = NULL;
		DupliObjectData *data = BKE_dupli_cache_add_mesh(ctx.dupli_cache, b_ob, dm);
		ctx.insert_dupli_data(object.getPtr(), data);
	}
}

static void read_dupligroup_group(DupliGroupContext &ctx, IObject object, float frame);

static void read_dupligroup_instance(DupliGroupContext &ctx, IObject instance, float frame)
{
	if (instance.isInstanceRoot()) {
		IObject object = IObject(instance.getPtr(), kWrapExisting);
		if (object && GS(object.getName().c_str()) == ID_OB) {
			DupliObjectData *data = ctx.find_dupli_data(object.getPtr());
			if (data)
				BKE_dupli_cache_add_instance(ctx.dupli_cache, ctx.get_transform(), data);
			
			IObject dup_group_object = object.getChild("dup_group");
			if (dup_group_object)
				read_dupligroup_group(ctx, dup_group_object, frame);
		}
	}
}

static void read_dupligroup_group(DupliGroupContext &ctx, IObject object, float frame)
{
	if (GS(object.getName().c_str()) == ID_GR) {
		printf("reading group %s\n", object.getName().c_str());
		size_t num_child = object.getNumChildren();
		
		for (size_t i = 0; i < num_child; ++i) {
			read_dupligroup_instance(ctx, object.getChild(i), frame);
		}
	}
}

PTCReadSampleResult abc_read_dupligroup(ReaderArchive *_archive, float frame, Group *dupgroup, DupliCache *dupcache)
{
	AbcReaderArchive *archive = (AbcReaderArchive *)_archive;
	DupliGroupContext ctx(dupcache);
	
	/* XXX this mapping allows fast lookup of existing objects in Blender data
	 * to associate with duplis. Later i may be possible to create instances of
	 * non-DNA data, but for the time being this is a requirement due to other code parts (drawing, rendering)
	 */
	ctx.build_object_map(G.main, dupgroup);
	
	IObject top = archive->archive.getTop();
	size_t num_child = top.getNumChildren();
	
	/* first create shared object data */
	for (size_t i = 0; i < num_child; ++i) {
		read_dupligroup_object(ctx, top.getChild(i), frame);
	}
	
	/* now generate dupli instances for the dupgroup */
	IObject dupgroup_object = top.getChild(dupgroup->id.name);
	if (dupgroup_object)
		read_dupligroup_group(ctx, dupgroup_object, frame);
	
	return PTC_READ_SAMPLE_EXACT;
}

} /* namespace PTC */
