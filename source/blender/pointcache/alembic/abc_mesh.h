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

struct Object;
struct PointCacheModifierData;
struct DerivedMesh;

namespace PTC {

class AbcDerivedMeshWriter : public DerivedMeshWriter, public AbcWriter {
public:
	AbcDerivedMeshWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, DerivedMesh **dm_ptr);
	~AbcDerivedMeshWriter();
	
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
	AbcDerivedMeshReader(AbcReaderArchive *archive, const std::string &name, Object *ob);
	~AbcDerivedMeshReader();
	
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


class AbcPointCacheWriter : public AbcDerivedMeshWriter {
public:
	AbcPointCacheWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, PointCacheModifierData *pcmd);
};

class AbcPointCacheReader : public AbcDerivedMeshReader {
public:
	AbcPointCacheReader(AbcReaderArchive *archive, const std::string &name, Object *ob, PointCacheModifierData *pcmd);
};

} /* namespace PTC */

#endif  /* PTC_MESH_H */
