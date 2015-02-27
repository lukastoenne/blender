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
#include "abc_particles.h"
#include "util_path.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_particle.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcParticlesWriter::AbcParticlesWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) :
    ParticlesWriter(ob, psys, archive),
    AbcWriter(archive)
{
	if (archive->archive) {
		OObject root = archive->archive.getTop();
		m_points = OPoints(root, name, archive->frame_sampling_index());
	}
}

AbcParticlesWriter::~AbcParticlesWriter()
{
}

void AbcParticlesWriter::write_sample()
{
	if (!archive()->archive)
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


AbcParticlesReader::AbcParticlesReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) :
    ParticlesReader(ob, psys, archive),
    AbcReader(archive)
{
	if (archive->archive.valid()) {
		IObject root = archive->archive.getTop();
		m_points = IPoints(root, name);
	}
	
	/* XXX TODO read first sample for info on particle count and times */
	m_totpoint = 0;
}

AbcParticlesReader::~AbcParticlesReader()
{
}

PTCReadSampleResult AbcParticlesReader::read_sample(float frame)
{
	ISampleSelector ss = archive()->get_frame_sample_selector(frame);
	
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


AbcParticlePathcacheWriter::AbcParticlePathcacheWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix) :
    ParticlesWriter(ob, psys, NULL),
    AbcWriter(archive),
    m_pathcache(pathcache),
    m_totpath(totpath),
    m_suffix(suffix)
{
	OObject root = archive->archive.getTop();
	/* XXX non-escaped string construction here ... */
	m_curves = OCurves(root, name + m_suffix, archive->frame_sampling_index());
	
	OCurvesSchema &schema = m_curves.getSchema();
	OCompoundProperty geom_props = schema.getArbGeomParams();
	
	m_param_velocities = OV3fGeomParam(geom_props, "velocities", false, kVertexScope, 1, 0);
	m_param_rotations = OQuatfGeomParam(geom_props, "rotations", false, kVertexScope, 1, 0);
	m_param_colors = OV3fGeomParam(geom_props, "colors", false, kVertexScope, 1, 0);
	m_param_times = OFloatGeomParam(geom_props, "times", false, kVertexScope, 1, 0);
}

AbcParticlePathcacheWriter::~AbcParticlePathcacheWriter()
{
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

static Int32ArraySample paths_create_sample_nvertices(ParticleCacheKey **pathcache, int totpart, std::vector<int32_t> &data)
{
	int p;
	
	data.reserve(totpart);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		
		data.push_back(keys->segments + 1);
	}
	
	return Int32ArraySample(data);
}

static P3fArraySample paths_create_sample_positions(ParticleCacheKey **pathcache, int totpart, int totkeys, std::vector<V3f> &data)
{
	int p, k;
	
	data.reserve(totkeys);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int numkeys = keys->segments + 1;
		
		for (k = 0; k < numkeys; ++k) {
			float *co = keys[k].co;
			data.push_back(V3f(co[0], co[1], co[2]));
		}
	}
	
	return P3fArraySample(data);
}

static OV3fGeomParam::Sample paths_create_sample_velocities(ParticleCacheKey **pathcache, int totpart, int totkeys, std::vector<V3f> &data)
{
	int p, k;
	
	data.reserve(totkeys);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int numkeys = keys->segments + 1;
		
		for (k = 0; k < numkeys; ++k) {
			float *vel = keys[k].vel;
			data.push_back(V3f(vel[0], vel[1], vel[2]));
		}
	}
	
	OV3fGeomParam::Sample sample;
	sample.setVals(V3fArraySample(data));
	sample.setScope(kVertexScope);
	return sample;
}

static OQuatfGeomParam::Sample paths_create_sample_rotations(ParticleCacheKey **pathcache, int totpart, int totkeys, std::vector<Quatf> &data)
{
	int p, k;
	
	data.reserve(totkeys);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int numkeys = keys->segments + 1;
		
		for (k = 0; k < numkeys; ++k) {
			float *rot = keys[k].rot;
			data.push_back(Quatf(rot[0], rot[1], rot[2], rot[3]));
		}
	}
	
	OQuatfGeomParam::Sample sample;
	sample.setVals(QuatfArraySample(data));
	sample.setScope(kVertexScope);
	return sample;
}

static OV3fGeomParam::Sample paths_create_sample_colors(ParticleCacheKey **pathcache, int totpart, int totkeys, std::vector<V3f> &data)
{
	int p, k;
	
	data.reserve(totkeys);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int numkeys = keys->segments + 1;
		
		for (k = 0; k < numkeys; ++k) {
			float *col = keys[k].col;
			data.push_back(V3f(col[0], col[1], col[2]));
		}
	}
	
	OV3fGeomParam::Sample sample;
	sample.setVals(V3fArraySample(data));
	sample.setScope(kVertexScope);
	return sample;
}

static OFloatGeomParam::Sample paths_create_sample_times(ParticleCacheKey **pathcache, int totpart, int totkeys, std::vector<float32_t> &data)
{
	int p, k;
	
	data.reserve(totkeys);
	
	for (p = 0; p < totpart; ++p) {
		ParticleCacheKey *keys = pathcache[p];
		int numkeys = keys->segments + 1;
		
		for (k = 0; k < numkeys; ++k) {
			data.push_back(keys[k].time);
		}
	}
	
	OFloatGeomParam::Sample sample;
	sample.setVals(FloatArraySample(data));
	sample.setScope(kVertexScope);
	return sample;
}

void AbcParticlePathcacheWriter::write_sample()
{
	if (!archive()->archive)
		return;
	if (!(*m_pathcache))
		return;
	
	int totkeys = paths_count_totkeys(*m_pathcache, *m_totpath);
	if (totkeys == 0)
		return;
	
	OCurvesSchema &schema = m_curves.getSchema();
	
	std::vector<V3f> positions_buffer;
	std::vector<V3f> velocities_buffer;
	std::vector<Quatf> rotations_buffer;
	std::vector<V3f> colors_buffer;
	std::vector<float32_t> times_buffer;
	V3fArraySample positions = paths_create_sample_positions(*m_pathcache, *m_totpath, totkeys, positions_buffer);
	OV3fGeomParam::Sample velocities = paths_create_sample_velocities(*m_pathcache, *m_totpath, totkeys, velocities_buffer);
	OQuatfGeomParam::Sample rotations = paths_create_sample_rotations(*m_pathcache, *m_totpath, totkeys, rotations_buffer);
	OV3fGeomParam::Sample colors = paths_create_sample_colors(*m_pathcache, *m_totpath, totkeys, colors_buffer);
	OFloatGeomParam::Sample times = paths_create_sample_times(*m_pathcache, *m_totpath, totkeys, times_buffer);
	
	OCurvesSchema::Sample sample;
//	if (schema.getNumSamples() == 0) {
		/* write curve sizes only first time, assuming they are constant! */
		std::vector<int32_t> nvertices_buffer;
		Int32ArraySample nvertices = paths_create_sample_nvertices(*m_pathcache, *m_totpath, nvertices_buffer);
		
		sample = OCurvesSchema::Sample(positions, nvertices);
//	}
//	else {
//		sample = OCurvesSchema::Sample(positions);
//	}
	
	schema.set(sample);
	
	m_param_velocities.set(velocities);
	m_param_rotations.set(rotations);
	m_param_colors.set(colors);
	m_param_times.set(times);
}


AbcParticlePathcacheReader::AbcParticlePathcacheReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix) :
    ParticlesReader(ob, psys, archive),
    AbcReader(archive),
    m_pathcache(pathcache),
    m_totpath(totpath),
    m_suffix(suffix)
{
	if (archive->archive.valid()) {
		IObject root = archive->archive.getTop();
		if (root.valid()) {
			/* XXX non-escaped string construction here ... */
			std::string curves_name = name + suffix;
			if (root.getChild(curves_name)) {
				m_curves = ICurves(root, curves_name);
				ICurvesSchema &schema = m_curves.getSchema();
				ICompoundProperty geom_props = schema.getArbGeomParams();
				
				m_param_velocities = IV3fGeomParam(geom_props, "velocities", 0);
				m_param_rotations = IQuatfGeomParam(geom_props, "rotations", 0);
				m_param_colors = IV3fGeomParam(geom_props, "colors", 0);
				m_param_times = IFloatGeomParam(geom_props, "times", 0);
			}
		}
	}
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
	
	if (!m_curves.valid())
		return PTC_READ_SAMPLE_INVALID;
	
	ISampleSelector ss = archive()->get_frame_sample_selector(frame);
	
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

/* ==== API ==== */

Writer *abc_writer_particles(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
{
	BLI_assert(dynamic_cast<AbcWriterArchive *>(archive));
	return new AbcParticlesWriter((AbcWriterArchive *)archive, name, ob, psys);
}

Reader *abc_reader_particles(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
{
	BLI_assert(dynamic_cast<AbcReaderArchive *>(archive));
	return new AbcParticlesReader((AbcReaderArchive *)archive, name, ob, psys);
}

Writer *abc_writer_particle_pathcache_parents(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
{
	BLI_assert(dynamic_cast<AbcWriterArchive *>(archive));
	return new AbcParticlePathcacheParentsWriter((AbcWriterArchive *)archive, name, ob, psys);
}

Writer *abc_writer_particle_pathcache_children(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
{
	BLI_assert(dynamic_cast<AbcWriterArchive *>(archive));
	return new AbcParticlePathcacheChildrenWriter((AbcWriterArchive *)archive, name, ob, psys);
}

Reader *abc_reader_particle_pathcache_parents(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
{
	BLI_assert(dynamic_cast<AbcReaderArchive *>(archive));
	return new AbcParticlePathcacheParentsReader((AbcReaderArchive *)archive, name, ob, psys);
}

Reader *abc_reader_particle_pathcache_children(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
{
	BLI_assert(dynamic_cast<AbcReaderArchive *>(archive));
	return new AbcParticlePathcacheChildrenReader((AbcReaderArchive *)archive, name, ob, psys);
}

} /* namespace PTC */
