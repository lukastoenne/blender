#ifndef __UTIL_VOLUME_H__
#define __UTIL_VOLUME_H__

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
	virtual float sample(float x, float y, float z, int sampling) = 0;
	virtual bool intersect(const Ray *ray, Intersection *isect) = 0;
	virtual bool march(float *t0, float *t1) = 0;
	virtual bool has_uniform_voxels() = 0;
};

class float3_volume {
public:
	virtual ~float3_volume() {}
	virtual float3 sample(float x, float y, float z, int sampling) = 0;
	virtual bool intersect(const Ray *ray, Intersection *isect) = 0;
	virtual bool march(float *t0, float *t1) = 0;
	virtual bool has_uniform_voxels() = 0;
};

CCL_NAMESPACE_END

#ifdef WITH_OPENVDB

/* They are too many implicit float conversions happening in OpenVDB, disabling
 * errors for now (kevin) */
#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wfloat-conversion"
#	pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/RayIntersector.h>

#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif

#include "util_map.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

#if defined(HAS_CPP11_FEATURES)
using std::isfinite;
#else
using boost::math::isfinite;
#endif

template <typename IsectorType>
void create_isectors_threads(unordered_map<pthread_t, IsectorType *> &isect_map,
                             const vector<pthread_t> &thread_ids,
                             const IsectorType &main_isect)
{
	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		IsectorType *isect = new IsectorType(main_isect);
		pair<pthread_t, IsectorType *> inter(thread_ids[i], isect);
		isect_map.insert(inter);
	}

	if (isect_map.find(my_thread) == isect_map.end()) {
		IsectorType *isect = new IsectorType(main_isect);
		pair<pthread_t, IsectorType *> inter(my_thread, isect);
		isect_map.insert(inter);
	}
}

template <typename SamplerType, typename AccessorType>
void create_samplers_threads(unordered_map<pthread_t, SamplerType *> &sampler_map,
                             vector<AccessorType *> &accessors,
                             const vector<pthread_t> &thread_ids,
                             const openvdb::math::Transform *transform,
                             const AccessorType &main_accessor)
{
	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		AccessorType *accessor = new AccessorType(main_accessor);
		accessors.push_back(accessor);
		SamplerType *sampler = new SamplerType(*accessor, *transform);
		pair<pthread_t, SamplerType *> sampl(thread_ids[i], sampler);
		sampler_map.insert(sampl);
	}

	if (sampler_map.find(my_thread) == sampler_map.end()) {
		AccessorType *accessor = new AccessorType(main_accessor);
		accessors.push_back(accessor);
		SamplerType *sampler = new SamplerType(*accessor, *transform);
		pair<pthread_t, SamplerType *> sampl(my_thread, sampler);
		sampler_map.insert(sampl);
	}
}

typedef openvdb::math::Ray<float> vdb_ray_t;

class vdb_float_volume : public float_volume {
	typedef openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::PointSampler> point_sampler_t;
	typedef openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> box_sampler_t;
	typedef openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid,
	                                             openvdb::FloatTree::RootNodeType::ChildNodeType::LEVEL,
	                                             vdb_ray_t> isector_t;

	/* mainly used to ensure thread safety for the accessors */
	typedef unordered_map<pthread_t, isector_t *> isect_map;
	typedef unordered_map<pthread_t, point_sampler_t *> point_map;
	typedef unordered_map<pthread_t, box_sampler_t *> box_map;
	isect_map isectors;
	point_map point_samplers;
	box_map box_samplers;

	vector<openvdb::FloatGrid::ConstAccessor *> accessors;

	openvdb::FloatGrid::ConstAccessor *accessor;
	openvdb::math::Transform *transform;

	/* Main intersector, its purpose is to initialize the voxels' bounding box
	 * so the ones for the various threads do not do this, rather they are
	 * generated from a copy of it */
	isector_t *main_isector;

	bool uniform_voxels;

public:
	vdb_float_volume(openvdb::FloatGrid::Ptr grid)
	    : transform(&grid->transform())
	{
		accessor = new openvdb::FloatGrid::ConstAccessor(grid->getConstAccessor());

		/* only grids with uniform voxels can be used with VolumeRayIntersector */
		if(grid->hasUniformVoxels()) {
			uniform_voxels = true;
			/* 1 = size of the largest sampling kernel radius (BoxSampler) */
			main_isector = new isector_t(*grid, 1);
		}
		else {
			uniform_voxels = false;
			main_isector = NULL;
		}
	}

	~vdb_float_volume()
	{
		for(point_map::iterator iter = point_samplers.begin();
		    iter != point_samplers.end();
		    ++iter)
		{
			delete iter->second;
		}

		for(box_map::iterator iter = box_samplers.begin();
		    iter != box_samplers.end();
		    ++iter)
		{
			delete iter->second;
		}

		if(uniform_voxels) {
			delete main_isector;

			for(isect_map::iterator iter = isectors.begin();
			    iter != isectors.end();
			    ++iter)
			{
				delete iter->second;
			}
		}

		delete accessor;

		for(size_t i = 0; i < accessors.size(); ++i) {
			delete accessors[i];
		}
	}

	void create_threads_utils(const vector<pthread_t> &thread_ids)
	{
		create_isectors_threads(isectors, thread_ids, *main_isector);
		create_samplers_threads(point_samplers, accessors, thread_ids, transform, *accessor);
		create_samplers_threads(box_samplers, accessors, thread_ids, transform, *accessor);
	}

	ccl_always_inline float sample(float x, float y, float z, int sampling)
	{
		pthread_t thread = pthread_self();

		if(sampling == OPENVDB_SAMPLE_POINT) {
			point_map::iterator iter = point_samplers.find(thread);
			assert(iter != point_samplers.end());
			point_sampler_t *sampler = iter->second;

			return sampler->wsSample(openvdb::Vec3d(x, y, z));
		}
		else {
			box_map::iterator iter = box_samplers.find(thread);
			assert(iter != box_samplers.end());
			box_sampler_t *sampler = iter->second;

			return sampler->wsSample(openvdb::Vec3d(x, y, z));
		}
	}

	ccl_always_inline bool intersect(const Ray *ray, Intersection */*isect*/)
	{
		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		assert(iter != isectors.end());
		isector_t *vdb_isect = iter->second;

		vdb_ray_t::Vec3Type P(ray->P.x, ray->P.y, ray->P.z);
		vdb_ray_t::Vec3Type D(ray->D.x, ray->D.y, ray->D.z);
		D.normalize();

		vdb_ray_t vdb_ray(P, D, 1e-5f, ray->t);

		if(vdb_isect->setWorldRay(vdb_ray)) {
			// TODO
//			isect->t = vdb_ray.t1(); // (kevin) is this correct?
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
		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		isector_t *vdb_isect = iter->second;

		float vdb_t0(*t0), vdb_t1(*t1);

		if(vdb_isect->march(vdb_t0, vdb_t1)) {
			*t0 = vdb_isect->getWorldTime(vdb_t0);
			*t1 = vdb_isect->getWorldTime(vdb_t1);

			return true;
		}

		return false;
	}

	ccl_always_inline bool has_uniform_voxels()
	{
		return uniform_voxels;
	}
};

/* Same as above, except for vector grids, including staggered grids */
class vdb_float3_volume : public float3_volume {
	typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::PointSampler> point_sampler_t;
	typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::BoxSampler> box_sampler_t;
	typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::StaggeredPointSampler> stag_point_sampler_t;
	typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid::ConstAccessor, openvdb::tools::StaggeredBoxSampler> stag_box_sampler_t;

	typedef openvdb::tools::VolumeRayIntersector<openvdb::Vec3SGrid,
	                                             openvdb::FloatTree::RootNodeType::ChildNodeType::LEVEL,
	                                             vdb_ray_t> isector_t;

	/* mainly used to ensure thread safety for the accessors */
	typedef unordered_map<pthread_t, isector_t *> isect_map;
	typedef unordered_map<pthread_t, point_sampler_t *> point_map;
	typedef unordered_map<pthread_t, box_sampler_t *> box_map;
	typedef unordered_map<pthread_t, stag_point_sampler_t *> stag_point_map;
	typedef unordered_map<pthread_t, stag_box_sampler_t *> stag_box_map;
	isect_map isectors;
	point_map point_samplers;
	box_map box_samplers;
	stag_point_map stag_point_samplers;
	stag_box_map stag_box_samplers;

	vector<openvdb::Vec3SGrid::ConstAccessor *> accessors;

	openvdb::Vec3SGrid::ConstAccessor *accessor;
	openvdb::math::Transform *transform;

	/* Main intersector, its purpose is to initialize the voxels' bounding box
	 * so the ones for the various threads do not do this, rather they are
	 * generated from a copy of it. */
	isector_t *main_isector;

	bool uniform_voxels, staggered;

public:
	vdb_float3_volume(openvdb::Vec3SGrid::Ptr grid)
	    : transform(&grid->transform())
	{
		accessor = new openvdb::Vec3SGrid::ConstAccessor(grid->getConstAccessor());
		staggered = (grid->getGridClass() == openvdb::GRID_STAGGERED);

		/* only grids with uniform voxels can be used with VolumeRayIntersector */
		if(grid->hasUniformVoxels()) {
			uniform_voxels = true;
			/* 1 = size of the largest sampling kernel radius (BoxSampler) */
			main_isector = new isector_t(*grid, 1);
		}
		else {
			uniform_voxels = false;
			main_isector = NULL;
		}
	}

	~vdb_float3_volume()
	{
		for(point_map::iterator iter = point_samplers.begin();
		    iter != point_samplers.end();
		    ++iter)
		{
			delete iter->second;
		}

		for(box_map::iterator iter = box_samplers.begin();
		    iter != box_samplers.end();
		    ++iter)
		{
			delete iter->second;
		}

		for(stag_point_map::iterator iter = stag_point_samplers.begin();
		    iter != stag_point_samplers.end();
		    ++iter)
		{
			delete iter->second;
		}

		for(stag_box_map::iterator iter = stag_box_samplers.begin();
		    iter != stag_box_samplers.end();
		    ++iter)
		{
			delete iter->second;
		}

		if(uniform_voxels) {
			delete main_isector;

			for(isect_map::iterator iter = isectors.begin();
			    iter != isectors.end();
			    ++iter)
			{
				delete iter->second;
			}
		}

		delete accessor;

		for(size_t i = 0; i < accessors.size(); ++i) {
			delete accessors[i];
		}
	}

	void create_threads_utils(const vector<pthread_t> &thread_ids)
	{
		create_isectors_threads(isectors, thread_ids, *main_isector);
		create_samplers_threads(point_samplers, accessors, thread_ids, transform, *accessor);
		create_samplers_threads(box_samplers, accessors, thread_ids, transform, *accessor);
		create_samplers_threads(stag_point_samplers, accessors, thread_ids, transform, *accessor);
		create_samplers_threads(stag_box_samplers, accessors, thread_ids, transform, *accessor);
	}

	ccl_always_inline float3 sample_staggered(float x, float y, float z, int sampling)
	{
		openvdb::Vec3s r;
		pthread_t thread = pthread_self();

		if(sampling == OPENVDB_SAMPLE_POINT) {
			stag_point_map::iterator iter = stag_point_samplers.find(thread);
			assert(iter != stag_point_samplers.end());
			stag_point_sampler_t *sampler = iter->second;

			r = sampler->wsSample(openvdb::Vec3d(x, y, z));
		}
		else {
			stag_box_map::iterator iter = stag_box_samplers.find(thread);
			assert(iter != stag_box_samplers.end());
			stag_box_sampler_t *sampler = iter->second;

			r = sampler->wsSample(openvdb::Vec3d(x, y, z));
		}

		return make_float3(r.x(), r.y(), r.z());
	}

	ccl_always_inline float3 sample_ex(float x, float y, float z, int sampling)
	{
		openvdb::Vec3s r;
		pthread_t thread = pthread_self();

		if(sampling == OPENVDB_SAMPLE_POINT) {
			point_map::iterator iter = point_samplers.find(thread);
			assert(iter != point_samplers.end());
			point_sampler_t *sampler = iter->second;

			r = sampler->wsSample(openvdb::Vec3d(x, y, z));
		}
		else {
			box_map::iterator iter = box_samplers.find(thread);
			assert(iter != box_samplers.end());
			box_sampler_t *sampler = iter->second;

			r = sampler->wsSample(openvdb::Vec3d(x, y, z));
		}

		return make_float3(r.x(), r.y(), r.z());
	}

	ccl_always_inline float3 sample(float x, float y, float z, int sampling)
	{
		if(staggered)
			return sample_staggered(x, y, z, sampling);
		else
			return sample_ex(x, y, z, sampling);
	}

	ccl_always_inline bool intersect(const Ray *ray, Intersection */*isect*/)
	{
		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		assert(iter != isectors.end());
		isector_t *vdb_isect = iter->second;

		vdb_ray_t::Vec3Type P(ray->P.x, ray->P.y, ray->P.z);
		vdb_ray_t::Vec3Type D(ray->D.x, ray->D.y, ray->D.z);
		D.normalize();

		vdb_ray_t vdb_ray(P, D, 1e-5f, ray->t);

		if(vdb_isect->setWorldRay(vdb_ray)) {
			// TODO
//			isect->t = vdb_ray.t1(); // (kevin) is this correct?
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
		pthread_t thread = pthread_self();
		isect_map::iterator iter = isectors.find(thread);
		isector_t *vdb_isect = iter->second;

		float vdb_t0(*t0), vdb_t1(*t1);

		if(vdb_isect->march(vdb_t0, vdb_t1)) {
			*t0 = vdb_isect->getWorldTime(vdb_t0);
			*t1 = vdb_isect->getWorldTime(vdb_t1);

			return true;
		}

		return false;
	}

	ccl_always_inline bool has_uniform_voxels()
	{
		return uniform_voxels;
	}
};

CCL_NAMESPACE_END

#endif /* WITH_OPENVDB */

#endif /* __UTIL_VOLUME_H__ */

