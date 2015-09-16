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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_ANIM_H__
#define __BKE_ANIM_H__

/** \file BKE_anim.h
 *  \ingroup bke
 *  \author nzc
 *  \since March 2001
 */

#include "RNA_types.h"

struct EvaluationContext;
struct Path;
struct Object;
struct Scene;
struct ListBase;
struct bAnimVizSettings;
struct bMotionPath;
struct bPoseChannel;
struct ReportList;

/* ---------------------------------------------------- */
/* Animation Visualization */

void animviz_settings_init(struct bAnimVizSettings *avs);

void animviz_free_motionpath_cache(struct bMotionPath *mpath);
void animviz_free_motionpath(struct bMotionPath *mpath);

struct bMotionPath *animviz_verify_motionpaths(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct bPoseChannel *pchan);

void animviz_get_object_motionpaths(struct Object *ob, ListBase *targets);
void animviz_calc_motionpaths(struct Scene *scene, ListBase *targets);

/* ---------------------------------------------------- */
/* Curve Paths */

void free_path(struct Path *path);
void calc_curvepath(struct Object *ob, struct ListBase *nurbs);
int where_on_path(struct Object *ob, float ctime, float vec[4], float dir[3], float quat[4], float *radius, float *weight);

/* ---------------------------------------------------- */
/* Dupli-Generators */

struct DupliContext;
struct DupliContainer;
struct GHashIterator;

typedef struct DupliGenerator {
	char idname[64];
	char name[64];
	char description[256];
	int type;				/* dupli type */
	
	void (*make_duplis)(const struct DupliContext *ctx);
	
	/* RNA integration */
	ExtensionRNA ext;
} DupliGenerator;

void BKE_dupli_system_init(void);
void BKE_dupli_system_free(void);

struct DupliGenerator *BKE_dupli_gen_find(const char *identifier);
void BKE_dupli_gen_register(struct DupliGenerator *gen);
void BKE_dupli_gen_unregister(struct DupliGenerator *gen);
void BKE_dupli_gen_get_iterator(struct GHashIterator *iter);

struct DupliGenerator *BKE_dupli_context_generator(const struct DupliContext *ctx);
struct Object *BKE_dupli_context_object(const struct DupliContext *ctx);
struct DupliContainer *BKE_dupli_context_container(const struct DupliContext *ctx);

void BKE_dupli_add_instance(struct DupliContainer *cont, struct Object *ob, float mat[4][4], int index,
                            bool animated, bool hide, bool recursive);

/* ---------------------------------------------------- */
/* Dupli-Geometry */

struct ListBase *object_duplilist_ex(struct EvaluationContext *eval_ctx, struct Scene *sce, struct Object *ob, bool update);
struct ListBase *object_duplilist(struct EvaluationContext *eval_ctx, struct Scene *sce, struct Object *ob);
void free_object_duplilist(struct ListBase *lb);
int count_duplilist(struct Object *ob);

typedef struct DupliExtraData {
	float obmat[4][4];
	unsigned int lay;
} DupliExtraData;

typedef struct DupliApplyData {
	int num_objects;
	DupliExtraData *extra;
} DupliApplyData;

DupliApplyData *duplilist_apply(struct Object *ob, struct Scene *scene, struct ListBase *duplilist);
void duplilist_restore(struct ListBase *duplilist, DupliApplyData *apply_data);
void duplilist_free_apply_data(DupliApplyData *apply_data);

#endif
