/*
 * Copyright 2014, Blender Foundation.
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

#ifndef PTC_ABC_MESH_H
#define PTC_ABC_MESH_H

#include <Alembic/AbcGeom/IPolyMesh.h>
#include <Alembic/AbcGeom/OPolyMesh.h>

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

extern "C" {
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
}

struct Object;
struct CacheModifierData;
struct DerivedMesh;

namespace PTC {

class AbcDerivedMeshWriter : public DerivedMeshWriter, public AbcWriter {
public:
	AbcDerivedMeshWriter(const std::string &name, Object *ob, DerivedMesh **dm_ptr);
	~AbcDerivedMeshWriter();
	
	void open_archive(WriterArchive *archive);
	
	void write_sample();
	
private:
	AbcGeom::OPolyMesh m_mesh;
	AbcGeom::OBoolGeomParam m_param_smooth;
	AbcGeom::OInt32ArrayProperty m_prop_edges;
	AbcGeom::OInt32ArrayProperty m_prop_edges_index;
	AbcGeom::ON3fGeomParam m_param_vertex_normals;
	AbcGeom::ON3fGeomParam m_param_poly_normals;
	/* note: loop normals are already defined as a parameter in the schema */
};

class AbcDerivedMeshReader : public DerivedMeshReader, public AbcReader {
public:
	AbcDerivedMeshReader(const std::string &name, Object *ob);
	~AbcDerivedMeshReader();
	
	void open_archive(ReaderArchive *archive);
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcGeom::IPolyMesh m_mesh;
	AbcGeom::IBoolGeomParam m_param_smooth;
	AbcGeom::IInt32ArrayProperty m_prop_edges;
	AbcGeom::IInt32ArrayProperty m_prop_edges_index;
	AbcGeom::IN3fGeomParam m_param_loop_normals;
	AbcGeom::IN3fGeomParam m_param_vertex_normals;
	AbcGeom::IN3fGeomParam m_param_poly_normals;
};


class AbcDerivedFinalWriter : public AbcDerivedMeshWriter {
public:
	AbcDerivedFinalWriter(const std::string &name, Object *ob) :
	    AbcDerivedMeshWriter(name, ob, &ob->derivedFinal)
	{}
};


class AbcCacheModifierWriter : public AbcDerivedMeshWriter {
public:
	AbcCacheModifierWriter(const std::string &name, Object *ob, CacheModifierData *cmd) :
	    AbcDerivedMeshWriter(name, ob, &cmd->output_dm),
	    m_cmd(cmd)
	{
		cmd->flag |= MOD_CACHE_USE_OUTPUT;
	}
	
	~AbcCacheModifierWriter()
	{
		m_cmd->flag &= ~MOD_CACHE_USE_OUTPUT;
		if (m_cmd->output_dm) {
			m_cmd->output_dm->release(m_cmd->output_dm);
			m_cmd->output_dm = NULL;
		}
	}
	
private:
	CacheModifierData *m_cmd;
};


} /* namespace PTC */

#endif  /* PTC_MESH_H */
