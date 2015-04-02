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

AbcParticlesWriter::AbcParticlesWriter(const std::string &name, Object *ob, ParticleSystem *psys) :
    ParticlesWriter(ob, psys, name)
{
}

AbcParticlesWriter::~AbcParticlesWriter()
{
}

void AbcParticlesWriter::init_abc(OObject parent)
{
	if (m_points)
		return;
	m_points = OPoints(parent, m_name, abc_archive()->frame_sampling_index());
}

void AbcParticlesWriter::write_sample()
{
	if (!m_points)
		return;
	
	OPointsSchema &schema = m_points.getSchema();
	
	int totpart = m_psys->totpart;
	ParticleData *pa;
	int i;
	
	/* XXX TODO only needed for the first frame/sample */
	std::vector<Util::uint64_t> ids;
	ids.reserve(totpart);
	for (i = 0, pa = m_psys->particles; i < totpart; ++i, ++pa)
		ids.push_back(i);
	
	std::vector<V3f> positions;
	positions.reserve(totpart);
	for (i = 0, pa = m_psys->particles; i < totpart; ++i, ++pa) {
		float *co = pa->state.co;
		positions.push_back(V3f(co[0], co[1], co[2]));
	}
	
	OPointsSchema::Sample sample = OPointsSchema::Sample(V3fArraySample(positions), UInt64ArraySample(ids));

	schema.set(sample);
}


AbcParticlesReader::AbcParticlesReader(const std::string &name, Object *ob, ParticleSystem *psys) :
    ParticlesReader(ob, psys, name)
{
}

AbcParticlesReader::~AbcParticlesReader()
{
}

void AbcParticlesReader::init_abc(IObject object)
{
	if (m_points)
		return;
	m_points = IPoints(object, kWrapExisting);
	
	/* XXX TODO read first sample for info on particle count and times */
	m_totpoint = 0;
}

PTCReadSampleResult AbcParticlesReader::read_sample(float frame)
{
	ISampleSelector ss = abc_archive()->get_frame_sample_selector(frame);
	
	if (!m_points.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	IPointsSchema &schema = m_points.getSchema();
	
	IPointsSchema::Sample sample;
	schema.get(sample, ss);
	
	const V3f *positions = sample.getPositions()->get();
	int /*totpart = m_psys->totpart,*/ i;
	ParticleData *pa;
	for (i = 0, pa = m_psys->particles; i < sample.getPositions()->size(); ++i, ++pa) {
		pa->state.co[0] = positions[i].x;
		pa->state.co[1] = positions[i].y;
		pa->state.co[2] = positions[i].z;
	}
	
	return PTC_READ_SAMPLE_EXACT;
}


struct ParticleHairSample {
	std::vector<int32_t> numverts;
	
	std::vector<V3f> positions;
	std::vector<float32_t> times;
	std::vector<float32_t> weights;
};

AbcHairWriter::AbcHairWriter(const std::string &name, Object *ob, ParticleSystem *psys) :
    ParticlesWriter(ob, psys, name)
{
	m_psmd = psys_get_modifier(ob, psys);
}

AbcHairWriter::~AbcHairWriter()
{
}

void AbcHairWriter::init_abc(OObject parent)
{
	if (m_curves)
		return;
	m_curves = OCurves(parent, m_name, abc_archive()->frame_sampling_index());
	
	OCurvesSchema &schema = m_curves.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_times = OFloatGeomParam(geom_props, "times", false, kVertexScope, 1, 0);
	m_param_weights = OFloatGeomParam(geom_props, "weights", false, kVertexScope, 1, 0);
}

static int hair_count_totverts(ParticleSystem *psys)
{
	int p;
	int totverts = 0;
	
	for (p = 0; p < psys->totpart; ++p) {
		ParticleData *pa = &psys->particles[p];
		totverts += pa->totkey;
	}
	
	return totverts;
}

static void hair_create_sample(Object *ob, DerivedMesh *dm, ParticleSystem *psys, ParticleHairSample &sample, bool do_numverts)
{
	int totpart = psys->totpart;
	int totverts = hair_count_totverts(psys);
	int p, k;
	
	if (totverts == 0)
		return;
	
	if (do_numverts)
		sample.numverts.reserve(totpart);
	sample.positions.reserve(totverts);
	sample.times.reserve(totverts);
	sample.weights.reserve(totverts);
	
	for (p = 0; p < totpart; ++p) {
		ParticleData *pa = &psys->particles[p];
		int numverts = pa->totkey;
		
		if (do_numverts)
			sample.numverts.push_back(numverts);
		
		for (k = 0; k < numverts; ++k) {
			HairKey *key = &pa->hair[k];
			float hairmat[4][4];
			float co[3];
			
			/* hair keys are in "hair space" relative to the mesh,
			 * store them in object space for compatibility and to avoid
			 * complexities of how particles work.
			 */
			psys_mat_hair_to_object(ob, dm, psys->part->from, pa, hairmat);
			mul_v3_m4v3(co, hairmat, key->co);
			
			sample.positions.push_back(V3f(co[0], co[1], co[2]));
			sample.times.push_back(key->time);
			sample.weights.push_back(key->weight);
		}
	}
}

void AbcHairWriter::write_sample()
{
	if (!m_curves)
		return;
	if (!m_psmd || !m_psmd->dm)
		return;
	
	OCurvesSchema &schema = m_curves.getSchema();
	
	ParticleHairSample hair_sample;
	OCurvesSchema::Sample sample;
	if (schema.getNumSamples() == 0) {
		/* write curve sizes only first time, assuming they are constant! */
		hair_create_sample(m_ob, m_psmd->dm, m_psys, hair_sample, true);
		sample = OCurvesSchema::Sample(hair_sample.positions, hair_sample.numverts);
	}
	else {
		hair_create_sample(m_ob, m_psmd->dm, m_psys, hair_sample, false);
		sample = OCurvesSchema::Sample(hair_sample.positions);
	}
	schema.set(sample);
	
	m_param_times.set(OFloatGeomParam::Sample(FloatArraySample(hair_sample.times), kVertexScope));
	m_param_weights.set(OFloatGeomParam::Sample(FloatArraySample(hair_sample.weights), kVertexScope));
}


struct StrandsSample {
	std::vector<int32_t> numverts;
	
	std::vector<V3f> positions;
	std::vector<float32_t> times;
	std::vector<float32_t> weights;
	
	std::vector<V3f> motion_co;
	std::vector<V3f> motion_vel;
};

AbcStrandsWriter::AbcStrandsWriter(const std::string &name, DupliObjectData *dobdata) :
    m_name(name),
    m_dobdata(dobdata)
{
}

Strands *AbcStrandsWriter::get_strands() const
{
	return BKE_dupli_object_data_find_strands(m_dobdata, m_name.c_str());
}

void AbcStrandsWriter::init_abc(OObject parent)
{
	if (m_curves)
		return;
	m_curves = OCurves(parent, m_name, abc_archive()->frame_sampling_index());
	
	OCurvesSchema &schema = m_curves.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_times = OFloatGeomParam(geom_props, "times", false, kVertexScope, 1, 0);
	m_param_weights = OFloatGeomParam(geom_props, "weights", false, kVertexScope, 1, 0);
	
	m_param_motion_state = OCompoundProperty(geom_props, "motion_state");
	m_param_motion_co = OP3fGeomParam(m_param_motion_state, "position", false, kVertexScope, 1, 0);
	m_param_motion_vel = OV3fGeomParam(m_param_motion_state, "velocity", false, kVertexScope, 1, 0);
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
	
	m_param_times.set(OFloatGeomParam::Sample(FloatArraySample(strands_sample.times), kVertexScope));
	m_param_weights.set(OFloatGeomParam::Sample(FloatArraySample(strands_sample.weights), kVertexScope));
	
	if (!strands->state) {
		m_param_motion_co.set(OP3fGeomParam::Sample(P3fArraySample(strands_sample.motion_co), kVertexScope));
		m_param_motion_vel.set(OV3fGeomParam::Sample(V3fArraySample(strands_sample.motion_vel), kVertexScope));
	}
}


AbcStrandsReader::AbcStrandsReader(Strands *strands) :
    m_strands(strands)
{
}

AbcStrandsReader::~AbcStrandsReader()
{
	discard_result();
}

void AbcStrandsReader::init_abc(IObject object)
{
	if (m_curves)
		return;
	m_curves = ICurves(object, kWrapExisting);
	
	ICurvesSchema &schema = m_curves.getSchema();
	ICompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_times = IFloatGeomParam(geom_props, "times", 0);
	m_param_weights = IFloatGeomParam(geom_props, "weights", 0);
	
	if (geom_props.getPropertyHeader("motion_state")) {
		m_param_motion_state = ICompoundProperty(geom_props, "motion_state");
		m_param_motion_co = IP3fGeomParam(m_param_motion_state, "position");
		m_param_motion_vel = IV3fGeomParam(m_param_motion_state, "velocity");
	}
}

PTCReadSampleResult AbcStrandsReader::read_sample(float frame)
{
	ISampleSelector ss = abc_archive()->get_frame_sample_selector(frame);
	
	if (!m_curves.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	ICurvesSchema &schema = m_curves.getSchema();
	
	ICurvesSchema::Sample sample;
	schema.get(sample, ss);
	
	P3fArraySamplePtr sample_co = sample.getPositions();
	Int32ArraySamplePtr sample_numvert = sample.getCurvesNumVertices();
	IFloatGeomParam::Sample sample_time = m_param_times.getExpandedValue(ss);
	IFloatGeomParam::Sample sample_weight = m_param_weights.getExpandedValue(ss);
	
	if (!sample_co || !sample_numvert)
		return PTC_READ_SAMPLE_INVALID;
	
	if (m_strands && (m_strands->totcurves != sample_numvert->size() || m_strands->totverts != sample_co->size()))
		m_strands = NULL;
	if (!m_strands)
		m_strands = BKE_strands_new(sample_numvert->size(), sample_co->size());
	
	const int32_t *numvert = sample_numvert->get();
	for (int i = 0; i < sample_numvert->size(); ++i) {
		StrandsCurve *scurve = &m_strands->curves[i];
		scurve->numverts = *numvert;
		
		++numvert;
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
	
	if (m_param_motion_co && m_param_motion_co.getNumSamples() > 0 &&
	    m_param_motion_vel && m_param_motion_vel.getNumSamples() > 0) {
		IP3fGeomParam::Sample sample_motion_co = m_param_motion_co.getExpandedValue(ss);
		IV3fGeomParam::Sample sample_motion_vel = m_param_motion_vel.getExpandedValue(ss);
		
		BKE_strands_add_motion_state(m_strands);
		
		const V3f *co = sample_motion_co.getVals()->get();
		const V3f *vel = sample_motion_vel.getVals()->get();
		if (co && vel) {
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


AbcHairDynamicsWriter::AbcHairDynamicsWriter(const std::string &name, Object *ob, ParticleSystem *psys) :
    ParticlesWriter(ob, psys, name),
    m_cloth_writer(name + "__cloth", ob, psys->clmd)
{
}

void AbcHairDynamicsWriter::init_abc(OObject parent)
{
	m_cloth_writer.init_abc(parent);
}

void AbcHairDynamicsWriter::write_sample()
{
	m_cloth_writer.write_sample();
}

AbcHairDynamicsReader::AbcHairDynamicsReader(const std::string &name, Object *ob, ParticleSystem *psys) :
	ParticlesReader(ob, psys, name),
	m_cloth_reader(name + "__cloth", ob, psys->clmd)
{
}

void AbcHairDynamicsReader::init_abc(IObject object)
{
	m_cloth_reader.init_abc(object);
}

PTCReadSampleResult AbcHairDynamicsReader::read_sample(float frame)
{
	return m_cloth_reader.read_sample(frame);
}


struct ParticlePathcacheSample {
	std::vector<int32_t> numkeys;
	
	std::vector<V3f> positions;
	std::vector<V3f> velocities;
	std::vector<Quatf> rotations;
	std::vector<C3f> colors;
	std::vector<float32_t> times;
};

AbcParticlePathcacheWriter::AbcParticlePathcacheWriter(const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix) :
    ParticlesWriter(ob, psys, name),
    m_pathcache(pathcache),
    m_totpath(totpath),
    m_suffix(suffix)
{
}

AbcParticlePathcacheWriter::~AbcParticlePathcacheWriter()
{
}

void AbcParticlePathcacheWriter::init_abc(OObject parent)
{
	if (m_curves)
		return;
	
	/* XXX non-escaped string construction here ... */
	m_curves = OCurves(parent, m_name + m_suffix, abc_archive()->frame_sampling_index());
	
	OCurvesSchema &schema = m_curves.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_velocities = OV3fGeomParam(geom_props, "velocities", false, kVertexScope, 1, 0);
	m_param_rotations = OQuatfGeomParam(geom_props, "rotations", false, kVertexScope, 1, 0);
	m_param_colors = OC3fGeomParam(geom_props, "colors", false, kVertexScope, 1, 0);
	m_param_times = OFloatGeomParam(geom_props, "times", false, kVertexScope, 1, 0);
}

static int paths_count_totkeys(ParticleCacheKey **pathcache, int totpart)
{
	int p;
	int totkeys = 0;
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		totkeys += keys->segments + 1;
	}
	
	return totkeys;
}

static void paths_create_sample(ParticleCacheKey **pathcache, int totpart, int totkeys, ParticlePathcacheSample &sample, bool do_numkeys)
{
	int p, k;
	
	if (do_numkeys)
		sample.numkeys.reserve(totpart);
	sample.positions.reserve(totkeys);
	sample.velocities.reserve(totkeys);
	sample.rotations.reserve(totkeys);
	sample.colors.reserve(totkeys);
	sample.times.reserve(totkeys);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int numkeys = keys->segments + 1;
		
		if (do_numkeys)
			sample.numkeys.push_back(numkeys);
		
		for (k = 0; k < numkeys; ++k) {
			ParticleCacheKey *key = &keys[k];
			
			sample.positions.push_back(V3f(key->co[0], key->co[1], key->co[2]));
			sample.velocities.push_back(V3f(key->vel[0], key->vel[1], key->vel[2]));
			sample.rotations.push_back(Quatf(key->rot[0], key->rot[1], key->rot[2], key->rot[3]));
			sample.colors.push_back(C3f(key->col[0], key->col[1], key->col[2]));
			sample.times.push_back(key->time);
		}
	}
}

void AbcParticlePathcacheWriter::write_sample()
{
	if (!m_curves)
		return;
	if (!(*m_pathcache))
		return;
	
	int totkeys = paths_count_totkeys(*m_pathcache, *m_totpath);
	if (totkeys == 0)
		return;
	
	OCurvesSchema &schema = m_curves.getSchema();
	
	ParticlePathcacheSample path_sample;
	OCurvesSchema::Sample sample;
	if (schema.getNumSamples() == 0) {
		/* write curve sizes only first time, assuming they are constant! */
		paths_create_sample(*m_pathcache, *m_totpath, totkeys, path_sample, true);
		sample = OCurvesSchema::Sample(path_sample.positions, path_sample.numkeys);
	}
	else {
		paths_create_sample(*m_pathcache, *m_totpath, totkeys, path_sample, false);
		sample = OCurvesSchema::Sample(path_sample.positions);
	}
	schema.set(sample);
	
	m_param_velocities.set(OV3fGeomParam::Sample(V3fArraySample(path_sample.velocities), kVertexScope));
	m_param_rotations.set(OQuatfGeomParam::Sample(QuatfArraySample(path_sample.rotations), kVertexScope));
	m_param_colors.set(OC3fGeomParam::Sample(C3fArraySample(path_sample.colors), kVertexScope));
	m_param_times.set(OFloatGeomParam::Sample(FloatArraySample(path_sample.times), kVertexScope));
}


AbcParticlePathcacheReader::AbcParticlePathcacheReader(const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix) :
    ParticlesReader(ob, psys, name),
    m_pathcache(pathcache),
    m_totpath(totpath),
    m_suffix(suffix)
{
}

void AbcParticlePathcacheReader::init_abc(IObject object)
{
	if (m_curves)
		return;
	m_curves = ICurves(object, kWrapExisting);
	ICurvesSchema &schema = m_curves.getSchema();
	ICompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_velocities = IV3fGeomParam(geom_props, "velocities", 0);
	m_param_rotations = IQuatfGeomParam(geom_props, "rotations", 0);
	m_param_colors = IV3fGeomParam(geom_props, "colors", 0);
	m_param_times = IFloatGeomParam(geom_props, "times", 0);
}

static void paths_apply_sample_nvertices(ParticleCacheKey **pathcache, int totpart, Int32ArraySamplePtr sample)
{
	int p, k;
	
	BLI_assert(sample->size() == totpart);
	
	const int32_t *data = sample->get();
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int num_keys = data[p];
		int segments = num_keys - 1;
		
		for (k = 0; k < num_keys; ++k) {
			keys[k].segments = segments;
		}
	}
}

/* Warning: apply_sample_nvertices has to be called before this! */
static void paths_apply_sample_data(ParticleCacheKey **pathcache, int totpart,
                                    P3fArraySamplePtr sample_pos,
                                    V3fArraySamplePtr sample_vel,
                                    QuatfArraySamplePtr sample_rot,
                                    V3fArraySamplePtr sample_col,
                                    FloatArraySamplePtr sample_time)
{
	int p, k;
	
//	BLI_assert(sample->size() == totvert);
	
	const V3f *data_pos = sample_pos->get();
	const V3f *data_vel = sample_vel->get();
	const Quatf *data_rot = sample_rot->get();
	const V3f *data_col = sample_col->get();
	const float32_t *data_time = sample_time->get();
	ParticleCacheKey **pkeys = pathcache;
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *key = *pkeys;
		int num_keys = key->segments + 1;
		
		for (k = 0; k < num_keys; ++k) {
			copy_v3_v3(key->co, data_pos->getValue());
			copy_v3_v3(key->vel, data_vel->getValue());
			key->rot[0] = (*data_rot)[0];
			key->rot[1] = (*data_rot)[1];
			key->rot[2] = (*data_rot)[2];
			key->rot[3] = (*data_rot)[3];
			copy_v3_v3(key->col, data_col->getValue());
			key->time = *data_time;
			
			++key;
			++data_pos;
			++data_vel;
			++data_rot;
			++data_col;
			++data_time;
		}
		
		++pkeys;
	}
}

PTCReadSampleResult AbcParticlePathcacheReader::read_sample(float frame)
{
	if (!(*m_pathcache))
		return PTC_READ_SAMPLE_INVALID;
	
	if (!m_curves)
		return PTC_READ_SAMPLE_INVALID;
	
	ISampleSelector ss = abc_archive()->get_frame_sample_selector(frame);
	
	ICurvesSchema &schema = m_curves.getSchema();
	if (!schema.valid() || schema.getPositionsProperty().getNumSamples() == 0)
		return PTC_READ_SAMPLE_INVALID;
	
	ICurvesSchema::Sample sample;
	schema.get(sample, ss);
	
	P3fArraySamplePtr positions = sample.getPositions();
	Int32ArraySamplePtr nvertices = sample.getCurvesNumVertices();
	IV3fGeomParam::Sample sample_vel = m_param_velocities.getExpandedValue(ss);
	IQuatfGeomParam::Sample sample_rot = m_param_rotations.getExpandedValue(ss);
	IV3fGeomParam::Sample sample_col = m_param_colors.getExpandedValue(ss);
	IFloatGeomParam::Sample sample_time = m_param_times.getExpandedValue(ss);
	
//	int totkeys = positions->size();
	
	if (nvertices->valid()) {
		BLI_assert(nvertices->size() == *m_totpath);
		paths_apply_sample_nvertices(*m_pathcache, *m_totpath, nvertices);
	}
	
	paths_apply_sample_data(*m_pathcache, *m_totpath, positions, sample_vel.getVals(), sample_rot.getVals(), sample_col.getVals(), sample_time.getVals());
	
	return PTC_READ_SAMPLE_EXACT;
}

} /* namespace PTC */
