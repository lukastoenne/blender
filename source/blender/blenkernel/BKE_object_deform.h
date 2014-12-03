/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_OBJECT_DEFORM_H__
#define __BKE_OBJECT_DEFORM_H__

/** \file BKE_object_deform.h
 * \ingroup bke
 * \brief Functions for dealing with objects and deform verts,
 *        used by painting and tools.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bDeformGroup;
struct bFaceMap;
struct ID;
struct ListBase;
struct MDeformVert;
struct Object;

/* General vgroup operations */
void BKE_object_defgroup_remap_update_users(struct Object *ob, int *map);

bool BKE_object_defgroup_array_get(struct ID *id, struct MDeformVert **dvert_arr, int *dvert_tot);

struct bDeformGroup *BKE_object_defgroup_add(struct Object *ob);
struct bDeformGroup *BKE_object_defgroup_add_name(struct Object *ob, const char *name);
struct MDeformVert  *BKE_object_defgroup_data_create(struct ID *id);

bool BKE_object_defgroup_clear(struct Object *ob, struct bDeformGroup *dg, const bool use_selection);
bool BKE_object_defgroup_clear_all(struct Object *ob, const bool use_selection);

void BKE_object_defgroup_remove(struct Object *ob, struct bDeformGroup *defgroup);
void BKE_object_defgroup_remove_all(struct Object *ob);

/* Face map operations */
struct bFaceMap *BKE_object_facemap_add(struct Object *ob);
struct bFaceMap *BKE_object_facemap_add_name(struct Object *ob, const char *name);
void BKE_object_facemap_remove(struct Object *ob, struct bFaceMap *fmap);
void BKE_object_fmap_remove_all(struct Object *ob);

void ED_fmap_face_add(struct Object *ob, struct bFaceMap *fmap, int facenum);
void ED_fmap_face_remove(struct Object *ob, struct bFaceMap *fmap, int facenum);

int fmap_name_index(struct Object *ob, const char *name);
void fmap_unique_name(struct bFaceMap *fmap, struct Object *ob);
struct bFaceMap *fmap_find_name(struct Object *ob, const char *name);
void fmap_copy_list(struct ListBase *outbase, struct ListBase *inbase);

/* Select helpers */
enum eVGroupSelect;
bool *BKE_object_defgroup_subset_from_select_type(
        struct Object *ob, enum eVGroupSelect subset_type, int *r_defgroup_tot, int *r_subset_count);
void BKE_object_defgroup_subset_to_index_array(
        const bool *defgroup_validmap, const int defgroup_tot, int *r_defgroup_subset_map);


/* ********** */

bool *BKE_object_defgroup_lock_flags_get(struct Object *ob, const int defbase_tot);
bool *BKE_object_defgroup_validmap_get(struct Object *ob, const int defbase_tot);
bool *BKE_object_defgroup_selected_get(struct Object *ob, int defbase_tot, int *r_dg_flags_sel_tot);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_OBJECT_DEFORM_H__ */
