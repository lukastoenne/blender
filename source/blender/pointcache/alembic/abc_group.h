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

#include <Alembic/AbcGeom/IXform.h>
#include <Alembic/AbcGeom/OXform.h>

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct DupliCache;
struct Group;
struct Object;
struct Scene;

namespace PTC {

class AbcGroupWriter : public GroupWriter, public AbcWriter {
public:
	AbcGroupWriter(const std::string &name, Group *group);
	
	void open_archive(WriterArchive *archive);
	void create_refs();
	
	void write_sample();
	
private:
	Abc::OObject m_abc_object;
};

class AbcGroupReader : public GroupReader, public AbcReader {
public:
	AbcGroupReader(const std::string &name, Group *group);
	
	void open_archive(ReaderArchive *archive);
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	Abc::IObject m_abc_object;
};

/* ========================================================================= */

class AbcDupligroupWriter : public GroupWriter, public AbcWriter {
public:
	typedef std::vector<Abc::ObjectWriterPtr> WriterList;
	
	AbcDupligroupWriter(const std::string &name, EvaluationContext *eval_ctx, Scene *scene, Group *group);
	
	void open_archive(WriterArchive *archive);
	
	void write_sample();
	void write_sample_object(Object *ob);
	void write_sample_dupli(DupliObject *dob, int index);
	
private:
	EvaluationContext *m_eval_ctx;
	Scene *m_scene;
	
	Abc::OObject m_abc_group;
	WriterList m_writers;
};

PTCReadSampleResult abc_read_dupligroup(ReaderArchive *archive, float frame, Group *dupgroup, DupliCache *dupcache);

} /* namespace PTC */

#endif  /* PTC_OBJECT_H */
