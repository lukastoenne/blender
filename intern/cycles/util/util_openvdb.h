#ifndef __UTIL_OPENVDB_H__
#define __UTIL_OPENVDB_H__

#include "util_map.h"
#include "util_types.h"

#include "kernel_types.h"

CCL_NAMESPACE_BEGIN

struct Ray;
struct Intersection;

enum {
	OPENVDB_SAMPLE_POINT = 0,
	OPENVDB_SAMPLE_BOX   = 1,
};

class float_volume {
public:
	virtual ~float_volume() {}
	virtual float sample(int sampling, float3 co) = 0;
	virtual bool intersect(const Ray *ray, Intersection *isect) = 0;
	virtual bool march(float *t0, float *t1) = 0;
};

class float3_volume {
public:
	virtual ~float3_volume() {}
	virtual float3 sample(int sampling, float3 co) = 0;
	virtual bool intersect(const Ray *ray, Intersection *isect) = 0;
	virtual bool march(float *t0, float *t1) = 0;
};

CCL_NAMESPACE_END

#ifdef WITH_OPENVDB

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/RayIntersector.h>

CCL_NAMESPACE_BEGIN

#if defined(HAS_CPP11_FEATURES)
using std::isfinite;
#else
using boost::math::isfinite;
#endif

class vdb_float_volume : public float_volume {
	typedef openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> point_sampler_t;
	typedef openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> box_sampler_t;
	point_sampler_t *point_sampler;
	box_sampler_t *box_sampler;

	typedef openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> isector_t;
	typedef isector_t::RayType vdb_ray_t;

	/* mainly used to ensure thread safety for the accessors */
	typedef unordered_map<pthread_t, isector_t *> isect_map;
	typedef unordered_map<pthread_t, point_sampler_t *> point_map;
	typedef unordered_map<pthread_t, box_sampler_t *> box_map;
	isect_map isectors;
	point_map point_samplers;
	box_map box_samplers;

	openvdb::FloatGrid::ConstAccessor *accessor;

	/* Main intersector, its purpose is to initialize the voxels' bounding box
	 * so the ones for the various threads do not do this, rather they are
	 * generated from of copy of it */
	isector_t *main_isector;

	bool uniform_voxels;

public:
	vdb_float_volume(openvdb::FloatGrid::Ptr grid)
	{
		accessor = new openvdb::FloatGrid::ConstAccessor(grid->getConstAccessor());
		point_sampler = new point_sampler_t(*accessor, grid->transform());
		box_sampler = new box_sampler_t(*accessor, grid->transform());

		/* only grids with uniform voxels can be used with VolumeRayIntersector */
		if(grid->hasUniformVoxels()) {
			uniform_voxels = true;
			main_isector = new isector_t(*grid);
		}
		else {
			uniform_voxels = false;
		}
	}

	~vdb_float_volume()
	{
		delete point_sampler;
		delete box_sampler;
	}

	ccl_always_inline float sample(int sampling, float3 co)
	{
		pthread_t thread = pthread_self();

		if(sampling == OPENVDB_SAMPLE_POINT) {
			point_map::iterator iter = point_samplers.find(thread);
			point_sampler_t *sampler;

			if(iter == point_samplers.end()) {
				openvdb::FloatGrid::ConstAccessor *acc = new openvdb::FloatGrid::ConstAccessor(*accessor);
				sampler = new point_sampler_t(*acc, point_sampler->transform());
				pair<pthread_t, point_sampler_t *> sampl(thread, sampler);
				point_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			return sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}
		else {
			box_map::iterator iter = box_samplers.find(thread);
			box_sampler_t *sampler;

			if(iter == box_samplers.end()) {
				openvdb::FloatGrid::ConstAccessor *acc = new openvdb::FloatGrid::ConstAccessor(*accessor);
				sampler = new box_sampler_t(*acc, box_sampler->transform());
				pair<pthread_t, box_sampler_t *> sampl(thread, sampler);
				box_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			return sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}
	}

	ccl_always_inline bool intersect(const Ray *ray, Intersection */*isect*/)
	{
		if(!uniform_voxels) {
			return false;
		}

		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		isector_t *vdb_isect;

		if(iter == isectors.end()) {
			vdb_isect = new isector_t(*main_isector);
			pair<pthread_t, isector_t *> inter(thread, vdb_isect);
			isectors.insert(inter);
		}
		else {
			vdb_isect = iter->second;
		}

		vdb_ray_t::Vec3Type P(ray->P.x, ray->P.y, ray->P.z);
		vdb_ray_t::Vec3Type D(ray->D.x, ray->D.y, ray->D.z);
		D.normalize();

		vdb_ray_t vdb_ray(P, D, 1e-5f, ray->t);

		if(vdb_isect->setWorldRay(vdb_ray)) {
			// TODO
//			isect->t = t;
//			isect->u = isect->v = 1.0f;
//			isect->type = ;
//			isect->shad = shader;
//			isect->norm = ;
//			isect->prim = 0;
//			isect->object = 0;

			return true;
		}

		return false;
	}

	ccl_always_inline bool march(float *t0, float *t1)
	{
		if(!uniform_voxels) {
			return false;
		}

		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		isector_t *vdb_isect;

		if(iter == isectors.end()) {
			vdb_isect = new isector_t(*main_isector);
			pair<pthread_t, isector_t *> inter(thread, vdb_isect);
			isectors.insert(inter);
		}
		else {
			vdb_isect = iter->second;
		}

		openvdb::Real vdb_t0(*t0), vdb_t1(*t1);

		if(vdb_isect->march(vdb_t0, vdb_t1)) {
			*t0 = (float)vdb_t0;
			*t1 = (float)vdb_t1;

			return true;
		}

		return false;
	}
};

/* Same as above, except for vector grids */
/* TODO(kevin): staggered velocity grid sampling */
class vdb_float3_volume : public float3_volume {
	typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::PointSampler> point_sampler_t;
	typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::BoxSampler> box_sampler_t;
	point_sampler_t *point_sampler;
	box_sampler_t *box_sampler;

	typedef openvdb::tools::VolumeRayIntersector<openvdb::Vec3SGrid> isector_t;
	typedef isector_t::RayType vdb_ray_t;

	/* mainly used to ensure thread safety for the accessors */
	typedef unordered_map<pthread_t, isector_t *> isect_map;
	typedef unordered_map<pthread_t, point_sampler_t *> point_map;
	typedef unordered_map<pthread_t, box_sampler_t *> box_map;
	isect_map isectors;
	point_map point_samplers;
	box_map box_samplers;

	openvdb::Vec3SGrid::ConstAccessor *accessor;

	/* Main intersector, its purpose is to initialize the voxels' bounding box
	 * so the ones for the various threads do not do this, rather they are
	 * generated from of copy of it. */
	isector_t *main_isector;

	bool uniform_voxels;

public:
	vdb_float3_volume(openvdb::Vec3SGrid::Ptr grid)
	{
		accessor = new openvdb::Vec3SGrid::ConstAccessor(grid->getConstAccessor());
		point_sampler = new point_sampler_t(grid->tree(), grid->transform());
		box_sampler = new box_sampler_t(grid->tree(), grid->transform());

		/* only grids with uniform voxels can be used with VolumeRayIntersector */
		if(grid->hasUniformVoxels()) {
			uniform_voxels = true;
			main_isector = new isector_t(*grid);
		}
		else {
			uniform_voxels = false;
		}
	}

	~vdb_float3_volume()
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
			point_sampler_t *sampler;

			if(iter == point_samplers.end()) {
				openvdb::Vec3SGrid::ConstAccessor *acc = new openvdb::Vec3SGrid::ConstAccessor(*accessor);
				sampler = new point_sampler_t(*acc, point_sampler->transform());
				pair<pthread_t, point_sampler_t *> sampl(thread, sampler);
				point_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			r = sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}
		else {
			box_map::iterator iter = box_samplers.find(thread);
			box_sampler_t *sampler;

			if(iter == box_samplers.end()) {
				openvdb::Vec3SGrid::ConstAccessor *acc = new openvdb::Vec3SGrid::ConstAccessor(*accessor);
				sampler = new box_sampler_t(*acc, box_sampler->transform());
				pair<pthread_t, box_sampler_t *> sampl(thread, sampler);
				box_samplers.insert(sampl);
			}
			else {
				sampler = iter->second;
			}

			r = sampler->wsSample(openvdb::Vec3d(co.x, co.y, co.z));
		}

		return make_float3(r.x(), r.y(), r.z());
	}

	ccl_always_inline bool intersect(const Ray *ray, Intersection */*isect*/)
	{
		if(!uniform_voxels) {
			return false;
		}

		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		isector_t *vdb_isect;

		if(iter == isectors.end()) {
			vdb_isect = new isector_t(*main_isector);
			pair<pthread_t, isector_t *> inter(thread, vdb_isect);
			isectors.insert(inter);
		}
		else {
			vdb_isect = iter->second;
		}

		vdb_ray_t::Vec3Type P(ray->P.x, ray->P.y, ray->P.z);
		vdb_ray_t::Vec3Type D(ray->D.x, ray->D.y, ray->D.z);
		D.normalize();

		vdb_ray_t vdb_ray(P, D, 1e-5f, ray->t);

		if(vdb_isect->setWorldRay(vdb_ray)) {
			// TODO
//			isect->t = t;
//			isect->u = isect->v = 1.0f;
//			isect->type = ;
//			isect->shad = shader;
//			isect->norm = ;
//			isect->prim = 0;
//			isect->object = 0;

			return true;
		}

		return false;
	}

	ccl_always_inline bool march(float *t0, float *t1)
	{
		if(!uniform_voxels) {
			return false;
		}

		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		isector_t *vdb_isect;

		if(iter == isectors.end()) {
			vdb_isect = new isector_t(*main_isector);
			pair<pthread_t, isector_t *> inter(thread, vdb_isect);
			isectors.insert(inter);
		}
		else {
			vdb_isect = iter->second;
		}

		openvdb::Real vdb_t0(*t0), vdb_t1(*t1);

		if(vdb_isect->march(vdb_t0, vdb_t1)) {
			*t0 = (float)vdb_t0;
			*t1 = (float)vdb_t1;

			return true;
		}

		return false;
	}
};

CCL_NAMESPACE_END

#endif /* WITH_OPENVDB */

#endif /* __UTIL_OPENVDB_H__ */

