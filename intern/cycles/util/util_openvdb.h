#ifndef __UTIL_OPENVDB_H__
#define __UTIL_OPENVDB_H__

#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>

CCL_NAMESPACE_BEGIN

#if defined(__cplusplus) && ((__cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1800))
#  define HAS_CPP11_FEATURES
#endif

#if defined(__GNUC__) || defined(__clang__)
#  if defined(HAS_CPP11_FEATURES)
using std::isfinite;
#  else
using boost::math::isfinite;
#  endif
#endif

enum {
	OPENVDB_SAMPLE_POINT = 0,
	OPENVDB_SAMPLE_BOX   = 1,
};

typedef openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::PointSampler> vdb_fsampler_p;
typedef openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::BoxSampler> vdb_fsampler_b;
typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid, openvdb::tools::PointSampler> vdb_vsampler_p;
typedef openvdb::tools::GridSampler<openvdb::Vec3SGrid, openvdb::tools::BoxSampler> vdb_vsampler_b;

CCL_NAMESPACE_END

#endif /* __UTIL_OPENVDB_H__ */

