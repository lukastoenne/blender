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

#include "abc_cloth.h"
#include "abc_mesh.h"
#include "abc_particles.h"

extern "C" {
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_listBase.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_anim.h"
#include "BKE_particle.h"
#include "BKE_strands.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;


struct StrandsChildrenSample {
	std::vector<int32_t> numverts;
	std::vector<M33f> root_matrix;
	std::vector<V3f> root_positions;
	
	std::vector<V3f> positions;
	std::vector<float32_t> times;
	std::vector<int32_t> parents;
	std::vector<float32_t> parent_weights;
};

struct StrandsSample {
	std::vector<int32_t> numverts;
	std::vector<M33f> root_matrix;
	
	std::vector<V3f> positions;
	std::vector<float32_t> times;
	std::vector<float32_t> weights;
	
	std::vector<V3f> motion_co;
	std::vector<V3f> motion_vel;
};


AbcStrandsChildrenWriter::AbcStrandsChildrenWriter(const std::string &name, const std::string &abc_name, DupliObjectData *dobdata) :
    m_name(name),
    m_abc_name(abc_name),
    m_dobdata(dobdata)
{
}

StrandsChildren *AbcStrandsChildrenWriter::get_strands() const
{
	return BKE_dupli_object_data_find_strands_children(m_dobdata, m_name.c_str());
}

void AbcStrandsChildrenWriter::init_abc(OObject parent)
{
	if (m_curves)
		return;
	m_curves = OCurves(parent, m_abc_name, abc_archive()->frame_sampling_index());
	
	OCurvesSchema &schema = m_curves.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	OCompoundProperty user_props = schema.getUserProperties();
	
	m_prop_root_matrix = OM33fArrayProperty(user_props, "root_matrix", abc_archive()->frame_sampling());
	m_prop_root_positions = OV3fArrayProperty(user_props, "root_positions", abc_archive()->frame_sampling());
	m_param_times = OFloatGeomParam(geom_props, "times", false, kVertexScope, 1, abc_archive()->frame_sampling());
	m_prop_parents = OInt32ArrayProperty(user_props, "parents", abc_archive()->frame_sampling());
	m_prop_parent_weights = OFloatArrayProperty(user_props, "parent_weights", abc_archive()->frame_sampling());
}

static void strands_children_create_sample(StrandsChildren *strands, StrandsChildrenSample &sample, bool write_constants)
{
	int totcurves = strands->totcurves;
	int totverts = strands->totverts;
	
	if (write_constants) {
		sample.numverts.reserve(totcurves);
		sample.parents.reserve(4*totcurves);
		sample.parent_weights.reserve(4*totcurves);
		
		sample.positions.reserve(totverts);
		sample.times.reserve(totverts);
	}
	
	sample.root_matrix.reserve(totcurves);
	sample.root_positions.reserve(totcurves);
	
	StrandChildIterator it_strand;
	for (BKE_strand_child_iter_init(&it_strand, strands); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		int numverts = it_strand.curve->numverts;
		
		if (write_constants) {
			sample.numverts.push_back(numverts);
			
			sample.parents.push_back(it_strand.curve->parents[0]);
			sample.parents.push_back(it_strand.curve->parents[1]);
			sample.parents.push_back(it_strand.curve->parents[2]);
			sample.parents.push_back(it_strand.curve->parents[3]);
			sample.parent_weights.push_back(it_strand.curve->parent_weights[0]);
			sample.parent_weights.push_back(it_strand.curve->parent_weights[1]);
			sample.parent_weights.push_back(it_strand.curve->parent_weights[2]);
			sample.parent_weights.push_back(it_strand.curve->parent_weights[3]);
			
			StrandChildVertexIterator it_vert;
			for (BKE_strand_child_vertex_iter_init(&it_vert, &it_strand); BKE_strand_child_vertex_iter_valid(&it_vert); BKE_strand_child_vertex_iter_next(&it_vert)) {
				const float *co = it_vert.vertex->co;
				sample.positions.push_back(V3f(co[0], co[1], co[2]));
				sample.times.push_back(it_vert.vertex->time);
			}
		}
		
		float mat3[3][3];
		copy_m3_m4(mat3, it_strand.curve->root_matrix);
		sample.root_matrix.push_back(M33f(mat3));
		float *co = it_strand.curve->root_matrix[3];
		sample.root_positions.push_back(V3f(co[0], co[1], co[2]));
	}
}

void AbcStrandsChildrenWriter::write_sample()
{
	if (!m_curves)
		return;
	StrandsChildren *strands = get_strands();
	if (!strands)
		return;
	
	OCurvesSchema &schema = m_curves.getSchema();
	
	StrandsChildrenSample strands_sample;
	OCurvesSchema::Sample sample;
	if (schema.getNumSamples() == 0) {
		/* write curve sizes only first time, assuming they are constant! */
		strands_children_create_sample(strands, strands_sample, true);
		sample = OCurvesSchema::Sample(strands_sample.positions, strands_sample.numverts);
		
		m_prop_parents.set(Int32ArraySample(strands_sample.parents));
		m_prop_parent_weights.set(FloatArraySample(strands_sample.parent_weights));
		
		m_param_times.set(OFloatGeomParam::Sample(FloatArraySample(strands_sample.times), kVertexScope));
		
		schema.set(sample);
	}
	else {
		strands_children_create_sample(strands, strands_sample, false);
	}
	
	m_prop_root_matrix.set(M33fArraySample(strands_sample.root_matrix));
	m_prop_root_positions.set(V3fArraySample(strands_sample.root_positions));
}


AbcStrandsWriter::AbcStrandsWriter(const std::string &name, DupliObjectData *dobdata) :
    m_name(name),
    m_dobdata(dobdata),
    m_child_writer(name, "children", dobdata)
{
}

Strands *AbcStrandsWriter::get_strands() const
{
	return BKE_dupli_object_data_find_strands(m_dobdata, m_name.c_str());
}

void AbcStrandsWriter::init(WriterArchive *archive)
{
	AbcWriter::init(archive);
	m_child_writer.init(archive);
}

void AbcStrandsWriter::init_abc(OObject parent)
{
	if (m_curves)
		return;
	m_curves = OCurves(parent, m_name, abc_archive()->frame_sampling_index());
	
	OCurvesSchema &schema = m_curves.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_root_matrix = OM33fGeomParam(geom_props, "root_matrix", false, kUniformScope, 1, abc_archive()->frame_sampling());
	
	m_param_times = OFloatGeomParam(geom_props, "times", false, kVertexScope, 1, abc_archive()->frame_sampling());
	m_param_weights = OFloatGeomParam(geom_props, "weights", false, kVertexScope, 1, abc_archive()->frame_sampling());
	
	m_param_motion_state = OCompoundProperty(geom_props, "motion_state", abc_archive()->frame_sampling());
	m_param_motion_co = OP3fGeomParam(m_param_motion_state, "position", false, kVertexScope, 1, abc_archive()->frame_sampling());
	m_param_motion_vel = OV3fGeomParam(m_param_motion_state, "velocity", false, kVertexScope, 1, abc_archive()->frame_sampling());
	
	m_child_writer.init_abc(m_curves);
}

static void strands_create_sample(Strands *strands, StrandsSample &sample, bool do_numverts)
{
	const bool do_state = strands->state;
	
	int totcurves = strands->totcurves;
	int totverts = strands->totverts;
	
	if (totverts == 0)
		return;
	
	if (do_numverts)
		sample.numverts.reserve(totcurves);
	sample.root_matrix.reserve(totcurves);
	
	sample.positions.reserve(totverts);
	sample.times.reserve(totverts);
	sample.weights.reserve(totverts);
	if (do_state) {
		sample.motion_co.reserve(totverts);
		sample.motion_vel.reserve(totverts);
	}
	
	StrandIterator it_strand;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		int numverts = it_strand.curve->numverts;
		
		if (do_numverts)
			sample.numverts.push_back(numverts);
		sample.root_matrix.push_back(M33f(it_strand.curve->root_matrix));
		
		StrandVertexIterator it_vert;
		for (BKE_strand_vertex_iter_init(&it_vert, &it_strand); BKE_strand_vertex_iter_valid(&it_vert); BKE_strand_vertex_iter_next(&it_vert)) {
			const float *co = it_vert.vertex->co;
			sample.positions.push_back(V3f(co[0], co[1], co[2]));
			sample.times.push_back(it_vert.vertex->time);
			sample.weights.push_back(it_vert.vertex->weight);
			
			if (do_state) {
				float *co = it_vert.state->co;
				float *vel = it_vert.state->vel;
				sample.motion_co.push_back(V3f(co[0], co[1], co[2]));
				sample.motion_vel.push_back(V3f(vel[0], vel[1], vel[2]));
			}
		}
	}
}

void AbcStrandsWriter::write_sample()
{
	if (!m_curves)
		return;
	Strands *strands = get_strands();
	if (!strands)
		return;
	
	OCurvesSchema &schema = m_curves.getSchema();
	
	StrandsSample strands_sample;
	OCurvesSchema::Sample sample;
	if (schema.getNumSamples() == 0) {
		/* write curve sizes only first time, assuming they are constant! */
		strands_create_sample(strands, strands_sample, true);
		sample = OCurvesSchema::Sample(strands_sample.positions, strands_sample.numverts);
	}
	else {
		strands_create_sample(strands, strands_sample, false);
		sample = OCurvesSchema::Sample(strands_sample.positions);
	}
	schema.set(sample);
	
	m_param_root_matrix.set(OM33fGeomParam::Sample(M33fArraySample(strands_sample.root_matrix), kUniformScope));
	
	m_param_times.set(OFloatGeomParam::Sample(FloatArraySample(strands_sample.times), kVertexScope));
	m_param_weights.set(OFloatGeomParam::Sample(FloatArraySample(strands_sample.weights), kVertexScope));
	
	if (strands->state) {
		m_param_motion_co.set(OP3fGeomParam::Sample(P3fArraySample(strands_sample.motion_co), kVertexScope));
		m_param_motion_vel.set(OV3fGeomParam::Sample(V3fArraySample(strands_sample.motion_vel), kVertexScope));
	}
	
	m_child_writer.write_sample();
}


#define PRINT_M3_FORMAT "((%.3f, %.3f, %.3f), (%.3f, %.3f, %.3f), (%.3f, %.3f, %.3f))"
#define PRINT_M3_ARGS(m) (double)m[0][0], (double)m[0][1], (double)m[0][2], (double)m[1][0], (double)m[1][1], (double)m[1][2], (double)m[2][0], (double)m[2][1], (double)m[2][2]
#define PRINT_M4_FORMAT "((%.3f, %.3f, %.3f, %.3f), (%.3f, %.3f, %.3f, %.3f), (%.3f, %.3f, %.3f, %.3f), (%.3f, %.3f, %.3f, %.3f))"
#define PRINT_M4_ARGS(m) (double)m[0][0], (double)m[0][1], (double)m[0][2], (double)m[0][3], (double)m[1][0], (double)m[1][1], (double)m[1][2], (double)m[1][3], \
                         (double)m[2][0], (double)m[2][1], (double)m[2][2], (double)m[2][3], (double)m[3][0], (double)m[3][1], (double)m[3][2], (double)m[3][3]

AbcStrandsChildrenReader::AbcStrandsChildrenReader(StrandsChildren *strands) :
    m_strands(strands)
{
}

AbcStrandsChildrenReader::~AbcStrandsChildrenReader()
{
	discard_result();
}

void AbcStrandsChildrenReader::init_abc(IObject object)
{
	if (m_curves)
		return;
	m_curves = ICurves(object, kWrapExisting);
	
	ICurvesSchema &schema = m_curves.getSchema();
	ICompoundProperty geom_props = schema.getArbGeomParams();
	ICompoundProperty user_props = schema.getUserProperties();
	
	m_prop_root_matrix = IM33fArrayProperty(user_props, "root_matrix");
	m_prop_root_positions = IV3fArrayProperty(user_props, "root_positions");
	m_param_times = IFloatGeomParam(geom_props, "times");
	m_prop_parents = IInt32ArrayProperty(user_props, "parents", 0);
	m_prop_parent_weights = IFloatArrayProperty(user_props, "parent_weights", 0);
}

PTCReadSampleResult AbcStrandsChildrenReader::read_sample(float frame)
{
	ISampleSelector ss = abc_archive()->get_frame_sample_selector(frame);
	
	if (!m_curves.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	ICurvesSchema &schema = m_curves.getSchema();
	if (schema.getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
	ICurvesSchema::Sample sample;
	schema.get(sample, ss);
	
	P3fArraySamplePtr sample_co = sample.getPositions();
	Int32ArraySamplePtr sample_numvert = sample.getCurvesNumVertices();
	M33fArraySamplePtr sample_root_matrix = m_prop_root_matrix.getValue(ss);
	V3fArraySamplePtr sample_root_positions = m_prop_root_positions.getValue(ss);
	IFloatGeomParam::Sample sample_time = m_param_times.getExpandedValue(ss);
	Int32ArraySamplePtr sample_parents = m_prop_parents.getValue(ss);
	FloatArraySamplePtr sample_parent_weights = m_prop_parent_weights.getValue(ss);
	
	if (!sample_co || !sample_numvert)
		return PTC_READ_SAMPLE_INVALID;
	
	int totcurves = sample_numvert->size();
	int totverts = sample_co->size();
	
	if (sample_root_matrix->size() != totcurves ||
	    sample_root_positions->size() != totcurves ||
	    sample_parents->size() != 4 * totcurves ||
	    sample_parent_weights->size() != 4 * totcurves)
		return PTC_READ_SAMPLE_INVALID;
	
	if (m_strands && (m_strands->totcurves != totcurves || m_strands->totverts != totverts))
		m_strands = NULL;
	if (!m_strands)
		m_strands = BKE_strands_children_new(totcurves, totverts);
	
	const int32_t *numvert = sample_numvert->get();
	const M33f *root_matrix = sample_root_matrix->get();
	const V3f *root_positions = sample_root_positions->get();
	const int32_t *parents = sample_parents->get();
	const float32_t *parent_weights = sample_parent_weights->get();
	for (int i = 0; i < sample_numvert->size(); ++i) {
		StrandsChildCurve *scurve = &m_strands->curves[i];
		scurve->numverts = *numvert;
		
		float mat[3][3];
		memcpy(mat, root_matrix->getValue(), sizeof(mat));
		copy_m4_m3(scurve->root_matrix, mat);
		copy_v3_v3(scurve->root_matrix[3], root_positions->getValue());
		
		scurve->parents[0] = parents[0];
		scurve->parents[1] = parents[1];
		scurve->parents[2] = parents[2];
		scurve->parents[3] = parents[3];
		scurve->parent_weights[0] = parent_weights[0];
		scurve->parent_weights[1] = parent_weights[1];
		scurve->parent_weights[2] = parent_weights[2];
		scurve->parent_weights[3] = parent_weights[3];
		
		++numvert;
		++root_matrix;
		++root_positions;
		parents += 4;
		parent_weights += 4;
	}
	
	const V3f *co = sample_co->get();
	const float32_t *time = sample_time.getVals()->get();
	for (int i = 0; i < sample_co->size(); ++i) {
		StrandsChildVertex *svert = &m_strands->verts[i];
		copy_v3_v3(svert->co, co->getValue());
		svert->time = *time;
		
		++co;
		++time;
	}
	
	BKE_strands_children_ensure_normals(m_strands);
	
	return PTC_READ_SAMPLE_EXACT;
}

StrandsChildren *AbcStrandsChildrenReader::acquire_result()
{
	StrandsChildren *strands = m_strands;
	m_strands = NULL;
	return strands;
}

void AbcStrandsChildrenReader::discard_result()
{
	BKE_strands_children_free(m_strands);
	m_strands = NULL;
}


AbcStrandsReader::AbcStrandsReader(Strands *strands, StrandsChildren *children, bool read_motion, bool read_children) :
    m_read_motion(read_motion),
    m_read_children(read_children),
    m_strands(strands),
    m_child_reader(children)
{
}

AbcStrandsReader::~AbcStrandsReader()
{
	discard_result();
}

void AbcStrandsReader::init(ReaderArchive *archive)
{
	AbcReader::init(archive);
	m_child_reader.init(archive);
}

void AbcStrandsReader::init_abc(IObject object)
{
	if (m_curves)
		return;
	m_curves = ICurves(object, kWrapExisting);
	
	ICurvesSchema &schema = m_curves.getSchema();
	ICompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_root_matrix = IM33fGeomParam(geom_props, "root_matrix");
	
	m_param_times = IFloatGeomParam(geom_props, "times");
	m_param_weights = IFloatGeomParam(geom_props, "weights");
	
	if (m_read_motion && geom_props.getPropertyHeader("motion_state")) {
		m_param_motion_state = ICompoundProperty(geom_props, "motion_state");
		m_param_motion_co = IP3fGeomParam(m_param_motion_state, "position");
		m_param_motion_vel = IV3fGeomParam(m_param_motion_state, "velocity");
	}
	
	if (m_read_children && m_curves.getChildHeader("children")) {
		IObject child = m_curves.getChild("children");
		m_child_reader.init_abc(child);
	}
}

PTCReadSampleResult AbcStrandsReader::read_sample(float frame)
{
	ISampleSelector ss = abc_archive()->get_frame_sample_selector(frame);
	
	if (!m_curves.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	ICurvesSchema &schema = m_curves.getSchema();
	if (schema.getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
	ICurvesSchema::Sample sample, sample_base;
	schema.get(sample, ss);
	schema.get(sample_base, ISampleSelector((index_t)0));
	
	P3fArraySamplePtr sample_co = sample.getPositions();
	P3fArraySamplePtr sample_co_base = sample_base.getPositions();
	Int32ArraySamplePtr sample_numvert = sample.getCurvesNumVertices();
	IM33fGeomParam::Sample sample_root_matrix = m_param_root_matrix.getExpandedValue(ss);
	IM33fGeomParam::Sample sample_root_matrix_base = m_param_root_matrix.getExpandedValue(ISampleSelector((index_t)0));
	IFloatGeomParam::Sample sample_time = m_param_times.getExpandedValue(ss);
	IFloatGeomParam::Sample sample_weight = m_param_weights.getExpandedValue(ss);
	
	if (!sample_co || !sample_numvert || !sample_co_base || sample_co_base->size() != sample_co->size())
		return PTC_READ_SAMPLE_INVALID;
	
	if (m_strands && (m_strands->totcurves != sample_numvert->size() || m_strands->totverts != sample_co->size()))
		m_strands = NULL;
	if (!m_strands)
		m_strands = BKE_strands_new(sample_numvert->size(), sample_co->size());
	
	const int32_t *numvert = sample_numvert->get();
	const M33f *root_matrix = sample_root_matrix.getVals()->get();
	for (int i = 0; i < sample_numvert->size(); ++i) {
		StrandsCurve *scurve = &m_strands->curves[i];
		scurve->numverts = *numvert;
		memcpy(scurve->root_matrix, root_matrix->getValue(), sizeof(scurve->root_matrix));
		
		++numvert;
		++root_matrix;
	}
	
	const V3f *co = sample_co->get();
	const float32_t *time = sample_time.getVals()->get();
	const float32_t *weight = sample_weight.getVals()->get();
	for (int i = 0; i < sample_co->size(); ++i) {
		StrandsVertex *svert = &m_strands->verts[i];
		copy_v3_v3(svert->co, co->getValue());
		svert->time = *time;
		svert->weight = *weight;
		
		++co;
		++time;
		++weight;
	}
	
	/* Correction for base coordinates: these are in object space of frame 1,
	 * but we want the relative shape. Offset them to the current root location.
	 */
	const M33f *root_matrix_base = sample_root_matrix_base.getVals()->get();
	const V3f *co_base = sample_co_base->get();
	StrandIterator it_strand;
	for (BKE_strand_iter_init(&it_strand, m_strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		if (it_strand.curve->numverts <= 0)
			continue;
		
#if 0
		float hairmat_base[4][4];
		float tmpmat[3][3];
		memcpy(tmpmat, root_matrix_base->getValue(), sizeof(tmpmat));
		copy_m4_m3(hairmat_base, tmpmat);
		copy_v3_v3(hairmat_base[3], co_base[0].getValue());
		
		float hairmat[4][4];
		copy_m4_m3(hairmat, it_strand.curve->root_matrix);
		copy_v3_v3(hairmat[3], it_strand.verts[0].co);
		
		float mat[4][4];
		invert_m4_m4(mat, hairmat_base);
		mul_m4_m4m4(mat, hairmat, mat);
#endif
		
		StrandVertexIterator it_vert;
		for (BKE_strand_vertex_iter_init(&it_vert, &it_strand); BKE_strand_vertex_iter_valid(&it_vert); BKE_strand_vertex_iter_next(&it_vert)) {
//			mul_v3_m4v3(it_vert.vertex->base, mat, co_base->getValue());
			copy_v3_v3(it_vert.vertex->base, it_vert.vertex->co);
			
			++co_base;
		}
		
		++root_matrix_base;
	}
	
	if (m_read_motion &&
	    m_param_motion_co && m_param_motion_co.getNumSamples() > 0 &&
	    m_param_motion_vel && m_param_motion_vel.getNumSamples() > 0)
	{
		IP3fGeomParam::Sample sample_motion_co = m_param_motion_co.getExpandedValue(ss);
		IV3fGeomParam::Sample sample_motion_vel = m_param_motion_vel.getExpandedValue(ss);
		
		const V3f *co = sample_motion_co.getVals()->get();
		const V3f *vel = sample_motion_vel.getVals()->get();
		if (co && vel) {
			BKE_strands_add_motion_state(m_strands);
			
			for (int i = 0; i < m_strands->totverts; ++i) {
				StrandsMotionState *ms = &m_strands->state[i];
				copy_v3_v3(ms->co, co->getValue());
				copy_v3_v3(ms->vel, vel->getValue());
				
				++co;
				++vel;
			}
		}
	}
	
	BKE_strands_ensure_normals(m_strands);
	
	if (m_read_children) {
		m_child_reader.read_sample(frame);
	}
	
	return PTC_READ_SAMPLE_EXACT;
}

Strands *AbcStrandsReader::acquire_result()
{
	Strands *strands = m_strands;
	m_strands = NULL;
	return strands;
}

void AbcStrandsReader::discard_result()
{
	BKE_strands_free(m_strands);
	m_strands = NULL;
}


} /* namespace PTC */
