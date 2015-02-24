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

#include "alembic.h"
#include "abc_mesh.h"
#include "util_path.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"

#include "PIL_time.h"
}

#include "PTC_api.h"

//#define USE_TIMING

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcPointCacheWriter::AbcPointCacheWriter(Scene *scene, Object *ob, PointCacheModifierData *pcmd) :
    PointCacheWriter(ob, pcmd, &m_archive),
    m_archive(scene, ptc_archive_path("//blendcache/", &ob->id, ob->id.lib), m_error_handler)
{
	set_error_handler(new ModifierErrorHandler(&pcmd->modifier));
	
	if (m_archive.archive) {
		OObject root = m_archive.archive.getTop();
		m_mesh = OPolyMesh(root, m_pcmd->modifier.name, m_archive.frame_sampling_index());
		
		OPolyMeshSchema &schema = m_mesh.getSchema();
		OCompoundProperty geom_props = schema.getArbGeomParams();
		OCompoundProperty user_props = schema.getUserProperties();
		
		m_param_smooth = OBoolGeomParam(geom_props, "smooth", false, kUniformScope, 1, 0);
		m_prop_edges = OInt32ArrayProperty(user_props, "edges", 0);
		m_prop_edges_index = OInt32ArrayProperty(user_props, "edges_index", 0);
		m_param_poly_normals = ON3fGeomParam(geom_props, "poly_normals", false, kUniformScope, 1, 0);
		m_param_vertex_normals = ON3fGeomParam(geom_props, "vertex_normals", false, kVertexScope, 1, 0);
	}
}

AbcPointCacheWriter::~AbcPointCacheWriter()
{
}

/* XXX modifiers are not allowed to generate poly normals on their own!
 * see assert in DerivedMesh.c : dm_ensure_display_normals
 */
#if 0
static void ensure_normal_data(DerivedMesh *dm)
{
	MVert *mverts = dm->getVertArray(dm);
	MLoop *mloops = dm->getLoopArray(dm);
	MPoly *mpolys = dm->getPolyArray(dm);
	CustomData *cdata = dm->getPolyDataLayout(dm);
	float (*polynors)[3];
	int totvert = dm->getNumVerts(dm);
	int totloop = dm->getNumLoops(dm);
	int totpoly = dm->getNumPolys(dm);
	
	if (CustomData_has_layer(cdata, CD_NORMAL))
		polynors = (float (*)[3])CustomData_get_layer(cdata, CD_NORMAL);
	else
		polynors = (float (*)[3])CustomData_add_layer(cdata, CD_NORMAL, CD_CALLOC, NULL, totpoly);
	
	BKE_mesh_calc_normals_poly(mverts, totvert, mloops, mpolys, totloop, totpoly, polynors, false);
}
#endif

static P3fArraySample create_sample_positions(DerivedMesh *dm, std::vector<V3f> &data)
{
	MVert *mv, *mverts = dm->getVertArray(dm);
	int i, totvert = dm->getNumVerts(dm);
	
	data.reserve(totvert);
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		float *co = mv->co;
		data.push_back(V3f(co[0], co[1], co[2]));
	}
	
	return P3fArraySample(data);
}

static Int32ArraySample create_sample_vertex_indices(DerivedMesh *dm, std::vector<int> &data)
{
	MLoop *ml, *mloops = dm->getLoopArray(dm);
	int i, totloop = dm->getNumLoops(dm);
	
	data.reserve(totloop);
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		data.push_back(ml->v);
	}
	
	return Int32ArraySample(data);
}

static Int32ArraySample create_sample_loop_counts(DerivedMesh *dm, std::vector<int> &data)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	data.reserve(totpoly);
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		data.push_back(mp->totloop);
	}
	
	return Int32ArraySample(data);
}

static OBoolGeomParam::Sample create_sample_poly_smooth(DerivedMesh *dm, std::vector<bool_t> &data)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	data.reserve(totpoly);
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		data.push_back((bool)(mp->flag & ME_SMOOTH));
	}
	
	OBoolGeomParam::Sample sample;
	sample.setVals(BoolArraySample(data));
	sample.setScope(kUniformScope);
	return sample;
}

static OInt32ArrayProperty::sample_type create_sample_edge_vertices(DerivedMesh *dm, std::vector<int> &data)
{
	MEdge *me, *medges = dm->getEdgeArray(dm);
	int i, totedge = dm->getNumEdges(dm);
	
	data.reserve(totedge * 2);
	for (i = 0, me = medges; i < totedge; ++i, ++me) {
		data.push_back(me->v1);
		data.push_back(me->v2);
	}
	
	return OInt32ArrayProperty::sample_type(data);
}

static OInt32ArrayProperty::sample_type create_sample_edge_indices(DerivedMesh *dm, std::vector<int> &data)
{
	MLoop *ml, *mloops = dm->getLoopArray(dm);
	int i, totloop = dm->getNumLoops(dm);
	
	data.reserve(totloop);
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		data.push_back(ml->e);
	}
	
	return OInt32ArrayProperty::sample_type(data);
}

static N3fArraySample create_sample_loop_normals(DerivedMesh *dm, std::vector<N3f> &data)
{
	CustomData *cdata = dm->getLoopDataLayout(dm);
	float (*nor)[3], (*loopnors)[3];
	int i, totloop = dm->getNumLoops(dm);
	
	if (!CustomData_has_layer(cdata, CD_NORMAL))
		return N3fArraySample();
	
	loopnors = (float (*)[3])CustomData_get_layer(cdata, CD_NORMAL);
	
	data.reserve(totloop);
	for (i = 0, nor = loopnors; i < totloop; ++i, ++nor) {
		float *vec = *nor;
		data.push_back(N3f(vec[0], vec[1], vec[2]));
	}
	
	return N3fArraySample(data);
}

static N3fArraySample create_sample_poly_normals(DerivedMesh *dm, std::vector<N3f> &data)
{
	CustomData *cdata = dm->getPolyDataLayout(dm);
	float (*nor)[3], (*polynors)[3];
	int i, totpoly = dm->getNumPolys(dm);
	
	if (!CustomData_has_layer(cdata, CD_NORMAL))
		return N3fArraySample();
	
	polynors = (float (*)[3])CustomData_get_layer(cdata, CD_NORMAL);
	
	data.reserve(totpoly);
	for (i = 0, nor = polynors; i < totpoly; ++i, ++nor) {
		float *vec = *nor;
		data.push_back(N3f(vec[0], vec[1], vec[2]));
	}
	
	return N3fArraySample(data);
}

static N3fArraySample create_sample_vertex_normals(DerivedMesh *dm, std::vector<N3f> &data)
{
	MVert *mv, *mverts = dm->getVertArray(dm);
	int i, totvert = dm->getNumVerts(dm);
	
	data.reserve(totvert);
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		float nor[3];
		
		normal_short_to_float_v3(nor, mv->no);
		data.push_back(N3f(nor[0], nor[1], nor[2]));
	}
	
	return N3fArraySample(data);
}

void AbcPointCacheWriter::write_sample()
{
	if (!m_archive.archive)
		return;
	
	DerivedMesh *output_dm = m_pcmd->output_dm;
	if (!output_dm)
		return;
	
	/* TODO make this optional by a flag? */
	/* XXX does not work atm, see comment above */
	/*ensure_normal_data(output_dm);*/
	
	OPolyMeshSchema &schema = m_mesh.getSchema();
	
	std::vector<V3f> positions_buffer;
	std::vector<int> indices_buffer;
	std::vector<int> counts_buffer;
	std::vector<bool_t> smooth_buffer;
	std::vector<int> edges_buffer;
	std::vector<int> edges_index_buffer;
	std::vector<N3f> loop_normals_buffer;
	std::vector<N3f> poly_normals_buffer;
	std::vector<N3f> vertex_normals_buffer;
//	std::vector<V2f> uvs;
//	V2fArraySample()
	
	// TODO decide how to handle vertex/face normals, in caching vs. export ...
//	std::vector<V2f> uvs;
//	OV2fGeomParam::Sample uvs(V2fArraySample(uvs), kFacevaryingScope );
	
	P3fArraySample positions = create_sample_positions(output_dm, positions_buffer);
	Int32ArraySample indices = create_sample_vertex_indices(output_dm, indices_buffer);
	Int32ArraySample counts = create_sample_loop_counts(output_dm, counts_buffer);
	OBoolGeomParam::Sample smooth = create_sample_poly_smooth(output_dm, smooth_buffer);
	OInt32ArrayProperty::sample_type edges = create_sample_edge_vertices(output_dm, edges_buffer);
	OInt32ArrayProperty::sample_type edges_index = create_sample_edge_indices(output_dm, edges_index_buffer);
	N3fArraySample lnormals = create_sample_loop_normals(output_dm, loop_normals_buffer);
	N3fArraySample pnormals = create_sample_poly_normals(output_dm, poly_normals_buffer);
	N3fArraySample vnormals = create_sample_vertex_normals(output_dm, vertex_normals_buffer);
	
	OPolyMeshSchema::Sample sample = OPolyMeshSchema::Sample(
	            positions,
	            indices,
	            counts,
	            OV2fGeomParam::Sample(), /* XXX define how/which UV map should be considered primary for the alembic schema */
	            ON3fGeomParam::Sample(lnormals, kFacevaryingScope)
	            );
	schema.set(sample);
	
	if (pnormals.valid())
		m_param_poly_normals.set(ON3fGeomParam::Sample(pnormals, kUniformScope));
	if (vnormals.valid())
		m_param_vertex_normals.set(ON3fGeomParam::Sample(vnormals, kVertexScope));
	
	m_param_smooth.set(smooth);
	
	m_prop_edges.set(edges);
	m_prop_edges_index.set(edges_index);
}


AbcPointCacheReader::AbcPointCacheReader(Scene *scene, Object *ob, PointCacheModifierData *pcmd) :
    PointCacheReader(ob, pcmd, &m_archive),
    m_archive(scene, ptc_archive_path("//blendcache/", &ob->id, ob->id.lib), m_error_handler)
{
	set_error_handler(new ModifierErrorHandler(&pcmd->modifier));
	
	if (m_archive.archive.valid()) {
		IObject root = m_archive.archive.getTop();
		if (root.valid() && root.getChild(m_pcmd->modifier.name)) {
			m_mesh = IPolyMesh(root, m_pcmd->modifier.name);
			
			IPolyMeshSchema &schema = m_mesh.getSchema();
			ICompoundProperty geom_props = schema.getArbGeomParams();
			ICompoundProperty user_props = schema.getUserProperties();
			
			m_param_loop_normals = schema.getNormalsParam();
			m_param_poly_normals = IN3fGeomParam(geom_props, "poly_normals", 0);
			m_param_vertex_normals = IN3fGeomParam(geom_props, "vertex_normals", 0);
			m_param_smooth = IBoolGeomParam(geom_props, "smooth", 0);
			m_prop_edges = IInt32ArrayProperty(user_props, "edges", 0);
			m_prop_edges_index = IInt32ArrayProperty(user_props, "edges_index", 0);
		}
	}
}

AbcPointCacheReader::~AbcPointCacheReader()
{
}

static void apply_sample_positions(DerivedMesh *dm, P3fArraySamplePtr sample)
{
	MVert *mv, *mverts = dm->getVertArray(dm);
	int i, totvert = dm->getNumVerts(dm);
	
	BLI_assert(sample->size() == totvert);
	
	const V3f *data = sample->get();
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		const V3f &co = data[i];
		copy_v3_v3(mv->co, co.getValue());
	}
}

static void apply_sample_vertex_indices(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MLoop *ml, *mloops = dm->getLoopArray(dm);
	int i, totloop = dm->getNumLoops(dm);
	
	BLI_assert(sample->size() == totloop);
	
	const int32_t *data = sample->get();
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		ml->v = data[i];
	}
}

static void apply_sample_loop_counts(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	BLI_assert(sample->size() == totpoly);
	
	const int32_t *data = sample->get();
	int loopstart = 0;
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		mp->totloop = data[i];
		mp->loopstart = loopstart;
		
		loopstart += mp->totloop;
	}
}

static void apply_sample_loop_normals(DerivedMesh *dm, N3fArraySamplePtr sample)
{
	CustomData *cdata = dm->getLoopDataLayout(dm);
	float (*nor)[3], (*loopnors)[3];
	int i, totloop = dm->getNumLoops(dm);
	
	BLI_assert(sample->size() == totloop);
	
	if (CustomData_has_layer(cdata, CD_NORMAL))
		loopnors = (float (*)[3])CustomData_get_layer(cdata, CD_NORMAL);
	else
		loopnors = (float (*)[3])CustomData_add_layer(cdata, CD_NORMAL, CD_CALLOC, NULL, totloop);
	
	const N3f *data = sample->get();
	for (i = 0, nor = loopnors; i < totloop; ++i, ++nor) {
		copy_v3_v3(*nor, data[i].getValue());
	}
}

static void apply_sample_poly_normals(DerivedMesh *dm, N3fArraySamplePtr sample)
{
	CustomData *cdata = dm->getPolyDataLayout(dm);
	float (*nor)[3], (*polynors)[3];
	int i, totpoly = dm->getNumPolys(dm);
	
	BLI_assert(sample->size() == totpoly);
	
	if (CustomData_has_layer(cdata, CD_NORMAL))
		polynors = (float (*)[3])CustomData_get_layer(cdata, CD_NORMAL);
	else
		polynors = (float (*)[3])CustomData_add_layer(cdata, CD_NORMAL, CD_CALLOC, NULL, totpoly);
	
	const N3f *data = sample->get();
	for (i = 0, nor = polynors; i < totpoly; ++i, ++nor) {
		copy_v3_v3(*nor, data[i].getValue());
	}
}

static void apply_sample_vertex_normals(DerivedMesh *dm, N3fArraySamplePtr sample)
{
	MVert *mv, *mverts = dm->getVertArray(dm);
	int i, totvert = dm->getNumVerts(dm);
	
	BLI_assert(sample->size() == totvert);
	
	const N3f *data = sample->get();
	for (i = 0, mv = mverts; i < totvert; ++i, ++mv) {
		normal_float_to_short_v3(mv->no, data[i].getValue());
	}
}

static void apply_sample_poly_smooth(DerivedMesh *dm, BoolArraySamplePtr sample)
{
	MPoly *mp, *mpolys = dm->getPolyArray(dm);
	int i, totpoly = dm->getNumPolys(dm);
	
	BLI_assert(sample->size() == totpoly);
	
	const bool_t *data = sample->get();
	for (i = 0, mp = mpolys; i < totpoly; ++i, ++mp) {
		if (data[i]) {
			mp->flag |= ME_SMOOTH;
		}
	}
}

static void apply_sample_edge_vertices(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MEdge *me, *medges = dm->getEdgeArray(dm);
	int i, totedge = dm->getNumEdges(dm);
	
	BLI_assert(sample->size() == totedge * 2);
	
	const int32_t *data = sample->get();
	for (i = 0, me = medges; i < totedge; ++i, ++me) {
		me->v1 = data[(i << 1)];
		me->v2 = data[(i << 1) + 1];
	}
}

static void apply_sample_edge_indices(DerivedMesh *dm, Int32ArraySamplePtr sample)
{
	MLoop *ml, *mloops = dm->getLoopArray(dm);
	int i, totloop = dm->getNumLoops(dm);
	
	BLI_assert(sample->size() == totloop);
	
	const int32_t *data = sample->get();
	for (i = 0, ml = mloops; i < totloop; ++i, ++ml) {
		ml->e = data[i];
	}
}

PTCReadSampleResult AbcPointCacheReader::read_sample(float frame)
{
#ifdef USE_TIMING
	double start_time;
	double time_get_sample, time_build_mesh, time_calc_edges, time_calc_normals;
	
#define PROFILE_START \
	start_time = PIL_check_seconds_timer();
#define PROFILE_END(var) \
	var = PIL_check_seconds_timer() - start_time;
#else
#define PROFILE_START ;
#define PROFILE_END(var) ;
#endif
	
	/* discard existing result data */
	discard_result();
	
	if (!m_mesh.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	IPolyMeshSchema &schema = m_mesh.getSchema();
	if (!schema.valid() || schema.getPositionsProperty().getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
	ISampleSelector ss = m_archive.get_frame_sample_selector(frame);
	
	PROFILE_START;
	IPolyMeshSchema::Sample sample;
	schema.get(sample, ss);
	
	bool has_normals = false;
	P3fArraySamplePtr positions = sample.getPositions();
	Int32ArraySamplePtr indices = sample.getFaceIndices();
	Int32ArraySamplePtr counts = sample.getFaceCounts();
	N3fArraySamplePtr lnormals, pnormals, vnormals;
	if (m_param_poly_normals && m_param_poly_normals.getNumSamples() > 0
	    && m_param_vertex_normals && m_param_vertex_normals.getNumSamples() > 0) {
		pnormals = m_param_poly_normals.getExpandedValue(ss).getVals();
		vnormals = m_param_vertex_normals.getExpandedValue(ss).getVals();
		
		/* we need all normal properties defined, otherwise have to recalculate */
		has_normals = pnormals->valid() && vnormals->valid();
	}
	if (has_normals) {
		/* note: loop normals are not mandatory, but if poly/vertex normals don't exist they get recalculated anyway */
		if (m_param_loop_normals && m_param_loop_normals.getNumSamples() > 0)
			lnormals = m_param_loop_normals.getExpandedValue(ss).getVals();
	}
	
	BoolArraySamplePtr smooth;
	if (m_param_smooth && m_param_smooth.getNumSamples() > 0) {
		IBoolGeomParam::Sample sample_smooth;
		m_param_smooth.getExpanded(sample_smooth, ss);
		smooth = sample_smooth.getVals();
	}
	
	bool has_edges = false;
	Int32ArraySamplePtr edges, edges_index;
	if (m_prop_edges && m_prop_edges.getNumSamples() > 0
	    && m_prop_edges_index && m_prop_edges_index.getNumSamples() > 0) {
		m_prop_edges.get(edges, ss);
		m_prop_edges_index.get(edges_index, ss);
		BLI_assert(edges->size() % 2 == 0); /* 2 vertex indices per edge */
		
		has_edges = edges->valid() && edges_index->valid();
	}
	PROFILE_END(time_get_sample);
	
	PROFILE_START;
	int totverts = positions->size();
	int totloops = indices->size();
	int totpolys = counts->size();
	int totedges = has_edges ? edges->size() >> 1 : 0;
	m_result = CDDM_new(totverts, totedges, 0, totloops, totpolys);
	
	apply_sample_positions(m_result, positions);
	apply_sample_vertex_indices(m_result, indices);
	apply_sample_loop_counts(m_result, counts);
	if (has_normals) {
		apply_sample_poly_normals(m_result, lnormals);
		apply_sample_vertex_normals(m_result, vnormals);
		
		if (lnormals->valid())
			apply_sample_loop_normals(m_result, pnormals);
	}
	else {
		/* make sure normals are recalculated if there is no sample data */
		m_result->dirty = (DMDirtyFlag)((int)m_result->dirty | DM_DIRTY_NORMALS);
	}
	if (has_edges) {
		apply_sample_edge_vertices(m_result, edges);
		apply_sample_edge_indices(m_result, edges_index);
	}
	if (smooth)
		apply_sample_poly_smooth(m_result, smooth);
	PROFILE_END(time_build_mesh);
	
	PROFILE_START;
	if (!has_edges)
		CDDM_calc_edges(m_result);
	PROFILE_END(time_calc_edges);
	
	PROFILE_START;
	DM_ensure_normals(m_result); /* only recalculates normals if no valid samples were found (has_normals == false) */
	PROFILE_END(time_calc_normals);
	
//	BLI_assert(DM_is_valid(m_result));
	
#ifdef USE_TIMING
	printf("-------- Point Cache Timing --------\n");
	printf("read sample: %f seconds\n", time_get_sample);
	printf("build mesh: %f seconds\n", time_build_mesh);
	printf("calculate edges: %f seconds\n", time_calc_edges);
	printf("calculate normals: %f seconds\n", time_calc_normals);
	printf("------------------------------------\n");
#endif
	
	return PTC_READ_SAMPLE_EXACT;
}

/* ==== API ==== */

Writer *abc_writer_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd)
{
	return new AbcPointCacheWriter(scene, ob, pcmd);
}

Reader *abc_reader_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd)
{
	return new AbcPointCacheReader(scene, ob, pcmd);
}

} /* namespace PTC */
