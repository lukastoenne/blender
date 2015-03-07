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

#include "abc_customdata.h"
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
	
	CustomDataWriter m_vertex_data_writer;
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
	
	CustomDataReader m_vertex_data_reader;
};


/* -------------------------------------------------------------------------
 * Writing derived mesh results requires different variants
 * depending on viewport/render output and whether a cache modifier is used.
 * 
 * Render DMs are constructed on-the-fly for each sample write, since they
 * are not constructed immediately during scene frame updates. The writer is
 * expected to only be called once per frame and object.
 * 
 * If a cache modifier is used it must be have be activate at the time when
 * the DM is built. For viewport output this means it should activate the
 * modifier during it's whole lifetime, so that it caches meshes during the
 * scene frame update. For render output the modifier should only be active
 * during the render DM construction.
 * ------------------------------------------------------------------------- */


class AbcDerivedFinalRealtimeWriter : public AbcDerivedMeshWriter {
public:
	AbcDerivedFinalRealtimeWriter(const std::string &name, Object *ob);
};


class AbcCacheModifierRealtimeWriter : public AbcDerivedMeshWriter {
public:
	AbcCacheModifierRealtimeWriter(const std::string &name, Object *ob, CacheModifierData *cmd);
	~AbcCacheModifierRealtimeWriter();
	
private:
	CacheModifierData *m_cmd;
};


class AbcDerivedFinalRenderWriter : public AbcDerivedMeshWriter {
public:
	AbcDerivedFinalRenderWriter(const std::string &name, Scene *scene, Object *ob, DerivedMesh **render_dm_ptr);
	
private:
	Scene *m_scene;
};


class AbcCacheModifierRenderWriter : public AbcDerivedMeshWriter {
public:
	AbcCacheModifierRenderWriter(const std::string &name, Scene *scene, Object *ob, CacheModifierData *cmd);
	~AbcCacheModifierRenderWriter();
	
private:
	Scene *m_scene;
	CacheModifierData *m_cmd;
};


} /* namespace PTC */

#endif  /* PTC_MESH_H */
