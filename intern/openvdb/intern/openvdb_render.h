#ifndef __OPENVDB_RENDER_H__
#define __OPENVDB_RENDER_H__

struct OpenVDBPrimitive;

namespace internal {

void OpenVDBPrimitive_draw_tree(OpenVDBPrimitive *vdb_prim, const bool draw_root,
                                const bool draw_level_1, const bool draw_level_2,
                                const bool draw_leaves);

void OpenVDBPrimitive_draw_values(OpenVDBPrimitive *vdb_prim);

}

#endif /* __OPENVDB_RENDER_H__ */

