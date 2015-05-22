#ifndef __UTIL_OPENVDB_H__
#define __UTIL_OPENVDB_H__

#include "util_map.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

enum {
	OPENVDB_SAMPLE_POINT = 0,
	OPENVDB_SAMPLE_BOX   = 1,
};

class float_volume_sampler {
public:
	virtual ~float_volume_sampler() {}
	virtual float sample(int sampling, float3 co) = 0;
};

class float3_volume_sampler {
public:
	virtual ~float3_volume_sampler() {}
	virtual float3 sample(int sampling, float3 co) = 0;
};

CCL_NAMESPACE_END

#ifdef WITH_OPENVDB

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>

CCL_NAMESPACE_BEGIN

#if defined(HAS_CPP11_FEATURES)
using std::isfinite;
#else
using boost::math::isfinite;
#endif

typedef openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> vdb_fsampler_p;
typedef openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> vdb_fsampler_b;
typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::PointSampler> vdb_vsampler_p;
typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::BoxSampler> vdb_vsampler_b;

class vdb_float_sampler : public float_volume_sampler {
	vdb_fsampler_p *point_sampler;
	vdb_fsampler_b *box_sampler;

	/* mainly used to ensure thread safety for the accessors */
	typedef unordered_map<pthread_t, vdb_fsampler_p *> point_map;
	typedef unordered_map<pthread_t, vdb_fsampler_b *> box_map;
	point_map point_samplers;
	box_map box_samplers;

	openvdb::FloatGrid::ConstAccessor *accessor;

public:
	vdb_float_sampler(openvdb::FloatGrid::Ptr grid)
	{
		accessor = new openvdb::FloatGrid::ConstAccessor(grid->getConstAccessor());
		point_sampler = new vdb_fsampler_p(*accessor, grid->transform());
		box_sampler = new vdb_fsampler_b(*accessor, grid->transform());
	}

	~vdb_float_sampler()
	{
		delete point_sampler;
		delete box_sampler;
	}

	ccl_always_inline float sample(int sampling, float3 co)
	{
		pthread_t thread = pthread_self();

		if(sampling == OPENVDB_SAMPLE_POINT) {
			point_map::iterator iter = point_samplers.find(thread);
			vdb_fsampler_p *sampler;

			if(iter == point_samplers.end()) {
				openvdb::FloatGrid::ConstAccessor *acc = new openvdb::FloatGrid::ConstAccessor(*accessor);
				sampler = new vdb_fsampler_p(*acc, point_sampler->transform());
				pair<pthread_t, vdb_fsampler_p *> sampl(thread, sampler);
				point_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			return sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}
		else {
			box_map::iterator iter = box_samplers.find(thread);
			vdb_fsampler_b *sampler;

			if(iter == box_samplers.end()) {
				openvdb::FloatGrid::ConstAccessor *acc = new openvdb::FloatGrid::ConstAccessor(*accessor);
				sampler = new vdb_fsampler_b(*acc, box_sampler->transform());
				pair<pthread_t, vdb_fsampler_b *> sampl(thread, sampler);
				box_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			return sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}
	}
};

class vdb_float3_sampler : public float3_volume_sampler {
	vdb_vsampler_p *point_sampler;
	vdb_vsampler_b *box_sampler;

	/* mainly used to ensure thread safety for the accessors */
	typedef unordered_map<pthread_t, vdb_vsampler_p *> point_map;
	typedef unordered_map<pthread_t, vdb_vsampler_b *> box_map;
	point_map point_samplers;
	box_map box_samplers;

	openvdb::Vec3SGrid::ConstAccessor *accessor;

public:
	vdb_float3_sampler(openvdb::Vec3SGrid::Ptr grid)
	{
		point_sampler = new vdb_vsampler_p(grid->tree(), grid->transform());
		box_sampler = new vdb_vsampler_b(grid->tree(), grid->transform());
	}

	~vdb_float3_sampler()
	{
		delete point_sampler;
		delete box_sampler;
	}

	ccl_always_inline float3 sample(int sampling, float3 co)
	{
		openvdb::Vec3s r;
		pthread_t thread = pthread_self();

		if(sampling == OPENVDB_SAMPLE_POINT) {
			point_map::iterator iter = point_samplers.find(thread);
			vdb_vsampler_p *sampler;

			if(iter == point_samplers.end()) {
				openvdb::Vec3SGrid::ConstAccessor *acc = new openvdb::Vec3SGrid::ConstAccessor(*accessor);
				sampler = new vdb_vsampler_p(*acc, point_sampler->transform());
				pair<pthread_t, vdb_vsampler_p *> sampl(thread, sampler);
				point_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			r = sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}
		else {
			point_map::iterator iter = box_samplers.find(thread);
			vdb_vsampler_b *sampler;

			if(iter == box_samplers.end()) {
				openvdb::Vec3SGrid::ConstAccessor *acc = new openvdb::Vec3SGrid::ConstAccessor(*accessor);
				sampler = new vdb_vsampler_b(*acc, box_sampler->transform());
				pair<pthread_t, vdb_vsampler_b *> sampl(thread, sampler);
				box_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			r = sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}

		return make_float3(r.x(), r.y(), r.z());
	}
};

CCL_NAMESPACE_END

#endif /* WITH_OPENVDB */

#endif /* __UTIL_OPENVDB_H__ */

