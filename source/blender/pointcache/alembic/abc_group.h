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

#ifndef PTC_ABC_GROUP_H
#define PTC_ABC_GROUP_H

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct DupliCache;
struct DupliObject;
struct DupliObjectData;
struct Group;
struct Object;
struct Scene;

namespace PTC {

class AbcGroupWriter : public GroupWriter, public AbcWriter {
public:
	AbcGroupWriter(const std::string &name, Group *group);
	
	void init_abc();
	void create_refs();
	
	void write_sample();
	
private:
	Abc::OObject m_abc_object;
};

class AbcGroupReader : public GroupReader, public AbcReader {
public:
	AbcGroupReader(const std::string &name, Group *group);
	
	void init_abc();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	Abc::IObject m_abc_object;
};

/* ========================================================================= */

class AbcDupligroupWriter : public GroupWriter, public AbcWriter {
public:
	typedef std::vector<Abc::ObjectWriterPtr> ObjectWriterList;
	typedef std::vector<Abc::BasePropertyWriterPtr> PropertyWriterList;
	
	typedef std::map<ID*, AbcWriter*> IDWriterMap;
	typedef std::pair<ID*, AbcWriter*> IDWriterPair;
	
	AbcDupligroupWriter(const std::string &name, EvaluationContext *eval_ctx, Scene *scene, Group *group);
	~AbcDupligroupWriter();
	
	void init_abc();
	
	void write_sample();
	void write_sample_object(Object *ob);
	void write_sample_dupli(DupliObject *dob, int index);
	
	AbcWriter *find_id_writer(ID *id) const;
	
private:
	EvaluationContext *m_eval_ctx;
	Scene *m_scene;
	
	Abc::OObject m_abc_group;
	ObjectWriterList m_object_writers;
	PropertyWriterList m_property_writers;
	IDWriterMap m_id_writers;
};

class AbcDupligroupReader : public GroupReader, public AbcReader {
public:
	typedef std::map<Abc::ObjectReaderPtr, DupliObjectData*> DupliMap;
	typedef std::pair<Abc::ObjectReaderPtr, DupliObjectData*> DupliPair;
	
	typedef std::map<std::string, Object*> ObjectMap;
	typedef std::pair<std::string, Object*> ObjectPair;
	
public:
	AbcDupligroupReader(const std::string &name, Group *group, DupliCache *dupcache);
	~AbcDupligroupReader();
	
	void init_abc();
	
	PTCReadSampleResult read_sample(float frame);
	
protected:
	void read_dupligroup_object(Abc::IObject object, const Abc::ISampleSelector &ss);
	void read_dupligroup_group(Abc::IObject abc_group, const Abc::ISampleSelector &ss);
	
	DupliObjectData *find_dupli_data(Abc::ObjectReaderPtr ptr) const;
	void insert_dupli_data(Abc::ObjectReaderPtr ptr, DupliObjectData *data);
	
	void build_object_map(Main *bmain, Group *group);
	void build_object_map_add_group(Group *group);
	Object *find_object(const std::string &name) const;
	
private:
	DupliMap dupli_map;
	DupliCache *dupli_cache;
	
	ObjectMap object_map;
};

} /* namespace PTC */

#endif  /* PTC_OBJECT_H */
