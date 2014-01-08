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

/** \file blender/blenkernel/intern/anim.c
 *  \ingroup bke
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BKE_animsys.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_editmesh.h"
#include "BKE_depsgraph.h"
#include "BKE_anim.h"
#include "BKE_report.h"


// XXX bad level call...

/* --------------------- */
/* forward declarations */

/* ******************************************************************** */
/* Animation Visualization */

/* Initialize the default settings for animation visualization */
void animviz_settings_init(bAnimVizSettings *avs)
{
	/* sanity check */
	if (avs == NULL)
		return;

	/* ghosting settings */
	avs->ghost_bc = avs->ghost_ac = 10;

	avs->ghost_sf = 1; /* xxx - take from scene instead? */
	avs->ghost_ef = 250; /* xxx - take from scene instead? */

	avs->ghost_step = 1;


	/* path settings */
	avs->path_bc = avs->path_ac = 10;

	avs->path_sf = 1; /* xxx - take from scene instead? */
	avs->path_ef = 250; /* xxx - take from scene instead? */

	avs->path_viewflag = (MOTIONPATH_VIEW_KFRAS | MOTIONPATH_VIEW_KFNOS);

	avs->path_step = 1;
}

/* ------------------- */

/* Free the given motion path's cache */
void animviz_free_motionpath_cache(bMotionPath *mpath) 
{
	/* sanity check */
	if (mpath == NULL) 
		return;
		
	/* free the path if necessary */
	if (mpath->points)
		MEM_freeN(mpath->points);
	
	/* reset the relevant parameters */
	mpath->points = NULL;
	mpath->length = 0;
}

/* Free the given motion path instance and its data 
 * NOTE: this frees the motion path given!
 */
void animviz_free_motionpath(bMotionPath *mpath)
{
	/* sanity check */
	if (mpath == NULL)
		return;
	
	/* free the cache first */
	animviz_free_motionpath_cache(mpath);
	
	/* now the instance itself */
	MEM_freeN(mpath);
}

/* ------------------- */

/* Setup motion paths for the given data
 * - Only used when explicitly calculating paths on bones which may/may not be consider already
 *
 * < scene: current scene (for frame ranges, etc.)
 * < ob: object to add paths for (must be provided)
 * < pchan: posechannel to add paths for (optional; if not provided, object-paths are assumed)
 */
bMotionPath *animviz_verify_motionpaths(ReportList *reports, Scene *scene, Object *ob, bPoseChannel *pchan)
{
	bAnimVizSettings *avs;
	bMotionPath *mpath, **dst;
	
	/* sanity checks */
	if (ELEM(NULL, scene, ob))
		return NULL;
		
	/* get destination data */
	if (pchan) {
		/* paths for posechannel - assume that posechannel belongs to the object */
		avs = &ob->pose->avs;
		dst = &pchan->mpath;
	}
	else {
		/* paths for object */
		avs = &ob->avs;
		dst = &ob->mpath;
	}

	/* avoid 0 size allocs */
	if (avs->path_sf >= avs->path_ef) {
		BKE_reportf(reports, RPT_ERROR,
		            "Motion path frame extents invalid for %s (%d to %d)%s",
		            (pchan) ? pchan->name : ob->id.name,
		            avs->path_sf, avs->path_ef,
		            (avs->path_sf == avs->path_ef) ? TIP_(", cannot have single-frame paths") : "");
		return NULL;
	}

	/* if there is already a motionpath, just return that,
	 * provided it's settings are ok (saves extra free+alloc)
	 */
	if (*dst != NULL) {
		int expected_length = avs->path_ef - avs->path_sf;
		
		mpath = *dst;
		
		/* path is "valid" if length is valid, but must also be of the same length as is being requested */
		if ((mpath->start_frame != mpath->end_frame) && (mpath->length > 0)) {
			/* outer check ensures that we have some curve data for this path */
			if (mpath->length == expected_length) {
				/* return/use this as it is already valid length */
				return mpath;
			}
			else {
				/* clear the existing path (as the range has changed), and reallocate below */
				animviz_free_motionpath_cache(mpath);
			}
		}
	}
	else {
		/* create a new motionpath, and assign it */
		mpath = MEM_callocN(sizeof(bMotionPath), "bMotionPath");
		*dst = mpath;
	}
	
	/* set settings from the viz settings */
	mpath->start_frame = avs->path_sf;
	mpath->end_frame = avs->path_ef;
	
	mpath->length = mpath->end_frame - mpath->start_frame;
	
	if (avs->path_bakeflag & MOTIONPATH_BAKE_HEADS)
		mpath->flag |= MOTIONPATH_FLAG_BHEAD;
	else
		mpath->flag &= ~MOTIONPATH_FLAG_BHEAD;
	
	/* allocate a cache */
	mpath->points = MEM_callocN(sizeof(bMotionPathVert) * mpath->length, "bMotionPathVerts");
	
	/* tag viz settings as currently having some path(s) which use it */
	avs->path_bakeflag |= MOTIONPATH_BAKE_HAS_PATHS;
	
	/* return it */
	return mpath;
}

/* ------------------- */

/* Motion path needing to be baked (mpt) */
typedef struct MPathTarget {
	struct MPathTarget *next, *prev;
	
	bMotionPath *mpath;         /* motion path in question */
	
	Object *ob;                 /* source object */
	bPoseChannel *pchan;        /* source posechannel (if applicable) */
} MPathTarget;

/* ........ */

/* get list of motion paths to be baked for the given object
 *  - assumes the given list is ready to be used
 */
/* TODO: it would be nice in future to be able to update objects dependent on these bones too? */
void animviz_get_object_motionpaths(Object *ob, ListBase *targets)
{
	MPathTarget *mpt;
	
	/* object itself first */
	if ((ob->avs.recalc & ANIMVIZ_RECALC_PATHS) && (ob->mpath)) {
		/* new target for object */
		mpt = MEM_callocN(sizeof(MPathTarget), "MPathTarget Ob");
		BLI_addtail(targets, mpt);
		
		mpt->mpath = ob->mpath;
		mpt->ob = ob;
	}
	
	/* bones */
	if ((ob->pose) && (ob->pose->avs.recalc & ANIMVIZ_RECALC_PATHS)) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;
		
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			if ((pchan->bone) && (arm->layer & pchan->bone->layer) && (pchan->mpath)) {
				/* new target for bone */
				mpt = MEM_callocN(sizeof(MPathTarget), "MPathTarget PoseBone");
				BLI_addtail(targets, mpt);
				
				mpt->mpath = pchan->mpath;
				mpt->ob = ob;
				mpt->pchan = pchan;
			}
		}
	}
}

/* ........ */

/* Note on evaluation optimizations:
 * Optimization's currently used here play tricks with the depsgraph in order to try and
 * evaluate as few objects as strictly necessary to get nicer performance under standard
 * production conditions. For those people who really need the accurate version, 
 * disable the ifdef (i.e. 1 -> 0) and comment out the call to motionpaths_calc_optimise_depsgraph()
 */

/* tweak the object ordering to trick depsgraph into making MotionPath calculations run faster */
static void motionpaths_calc_optimise_depsgraph(Scene *scene, ListBase *targets)
{
	Base *base, *baseNext;
	MPathTarget *mpt;
	
	/* make sure our temp-tag isn't already in use */
	for (base = scene->base.first; base; base = base->next)
		base->object->flag &= ~BA_TEMP_TAG;
	
	/* for each target, dump its object to the start of the list if it wasn't moved already */
	for (mpt = targets->first; mpt; mpt = mpt->next) {
		for (base = scene->base.first; base; base = baseNext) {
			baseNext = base->next;
			
			if ((base->object == mpt->ob) && !(mpt->ob->flag & BA_TEMP_TAG)) {
				BLI_remlink(&scene->base, base);
				BLI_addhead(&scene->base, base);
				
				mpt->ob->flag |= BA_TEMP_TAG;
				
				/* we really don't need to continue anymore once this happens, but this line might really 'break' */
				break;
			}
		}
	}
	
	/* "brew me a list that's sorted a bit faster now depsy" */
	DAG_scene_relations_rebuild(G.main, scene);
}

/* update scene for current frame */
static void motionpaths_calc_update_scene(Scene *scene)
{
#if 1 // 'production' optimizations always on
	/* rigid body simulation needs complete update to work correctly for now */
	/* RB_TODO investigate if we could avoid updating everything */
	if (BKE_scene_check_rigidbody_active(scene)) {
		BKE_scene_update_for_newframe(G.main->eval_ctx, G.main, scene, scene->lay);
	}
	else { /* otherwise we can optimize by restricting updates */
		Base *base, *last = NULL;
		
		/* only stuff that moves or needs display still */
		DAG_scene_update_flags(G.main, scene, scene->lay, TRUE);
		
		/* find the last object with the tag 
		 * - all those afterwards are assumed to not be relevant for our calculations
		 */
		/* optimize further by moving out... */
		for (base = scene->base.first; base; base = base->next) {
			if (base->object->flag & BA_TEMP_TAG)
				last = base;
		}
		
		/* perform updates for tagged objects */
		/* XXX: this will break if rigs depend on scene or other data that
		 * is animated but not attached to/updatable from objects */
		for (base = scene->base.first; base; base = base->next) {
			/* update this object */
			BKE_object_handle_update(G.main->eval_ctx, scene, base->object);
			
			/* if this is the last one we need to update, let's stop to save some time */
			if (base == last)
				break;
		}
	}
#else // original, 'always correct' version
	  /* do all updates
	   *  - if this is too slow, resort to using a more efficient way
	   *    that doesn't force complete update, but for now, this is the
	   *    most accurate way!
	   */
	BKE_scene_update_for_newframe(G.main->eval_ctx, G.main, scene, scene->lay); /* XXX this is the best way we can get anything moving */
#endif
}

/* ........ */

/* perform baking for the targets on the current frame */
static void motionpaths_calc_bake_targets(Scene *scene, ListBase *targets)
{
	MPathTarget *mpt;
	
	/* for each target, check if it can be baked on the current frame */
	for (mpt = targets->first; mpt; mpt = mpt->next) {
		bMotionPath *mpath = mpt->mpath;
		bMotionPathVert *mpv;
		
		/* current frame must be within the range the cache works for 
		 *	- is inclusive of the first frame, but not the last otherwise we get buffer overruns
		 */
		if ((CFRA < mpath->start_frame) || (CFRA >= mpath->end_frame))
			continue;
		
		/* get the relevant cache vert to write to */
		mpv = mpath->points + (CFRA - mpath->start_frame);
		
		/* pose-channel or object path baking? */
		if (mpt->pchan) {
			/* heads or tails */
			if (mpath->flag & MOTIONPATH_FLAG_BHEAD) {
				copy_v3_v3(mpv->co, mpt->pchan->pose_head);
			}
			else {
				copy_v3_v3(mpv->co, mpt->pchan->pose_tail);
			}
			
			/* result must be in worldspace */
			mul_m4_v3(mpt->ob->obmat, mpv->co);
		}
		else {
			/* worldspace object location */
			copy_v3_v3(mpv->co, mpt->ob->obmat[3]);
		}
	}
}

/* Perform baking of the given object's and/or its bones' transforms to motion paths 
 *	- scene: current scene
 *	- ob: object whose flagged motionpaths should get calculated
 *	- recalc: whether we need to
 */
/* TODO: include reports pointer? */
void animviz_calc_motionpaths(Scene *scene, ListBase *targets)
{
	MPathTarget *mpt;
	int sfra, efra;
	int cfra;
	
	/* sanity check */
	if (ELEM(NULL, targets, targets->first))
		return;
	
	/* set frame values */
	cfra = CFRA;
	sfra = efra = cfra;
	
	/* TODO: this method could be improved...
	 * 1) max range for standard baking
	 * 2) minimum range for recalc baking (i.e. between keyframes, but how?) */
	for (mpt = targets->first; mpt; mpt = mpt->next) {
		/* try to increase area to do (only as much as needed) */
		sfra = MIN2(sfra, mpt->mpath->start_frame);
		efra = MAX2(efra, mpt->mpath->end_frame);
	}
	if (efra <= sfra) return;
	
	/* optimize the depsgraph for faster updates */
	/* TODO: whether this is used should depend on some setting for the level of optimizations used */
	motionpaths_calc_optimise_depsgraph(scene, targets);
	
	/* calculate path over requested range */
	for (CFRA = sfra; CFRA <= efra; CFRA++) {
		/* update relevant data for new frame */
		motionpaths_calc_update_scene(scene);
		
		/* perform baking for targets */
		motionpaths_calc_bake_targets(scene, targets);
	}
	
	/* reset original environment */
	CFRA = cfra;
	motionpaths_calc_update_scene(scene);
	
	/* clear recalc flags from targets */
	for (mpt = targets->first; mpt; mpt = mpt->next) {
		bAnimVizSettings *avs;
		
		/* get pointer to animviz settings for each target */
		if (mpt->pchan)
			avs = &mpt->ob->pose->avs;
		else
			avs = &mpt->ob->avs;
		
		/* clear the flag requesting recalculation of targets */
		avs->recalc &= ~ANIMVIZ_RECALC_PATHS;
	}
}

/* ******************************************************************** */
/* Curve Paths - for curve deforms and/or curve following */

/* free curve path data 
 * NOTE: frees the path itself!
 * NOTE: this is increasingly inaccurate with non-uniform BevPoint subdivisions [#24633]
 */
void free_path(Path *path)
{
	if (path->data) MEM_freeN(path->data);
	MEM_freeN(path);
}

/* calculate a curve-deform path for a curve 
 *  - only called from displist.c -> do_makeDispListCurveTypes
 */
void calc_curvepath(Object *ob, ListBase *nurbs)
{
	BevList *bl;
	BevPoint *bevp, *bevpn, *bevpfirst, *bevplast;
	PathPoint *pp;
	Nurb *nu;
	Path *path;
	float *fp, *dist, *maxdist, xyz[3];
	float fac, d = 0, fac1, fac2;
	int a, tot, cycl = 0;
	
	/* in a path vertices are with equal differences: path->len = number of verts */
	/* NOW WITH BEVELCURVE!!! */
	
	if (ob == NULL || ob->type != OB_CURVE) {
		return;
	}

	if (ob->curve_cache->path) free_path(ob->curve_cache->path);
	ob->curve_cache->path = NULL;
	
	/* weak! can only use first curve */
	bl = ob->curve_cache->bev.first;
	if (bl == NULL || !bl->nr) {
		return;
	}

	nu = nurbs->first;

	ob->curve_cache->path = path = MEM_callocN(sizeof(Path), "calc_curvepath");
	
	/* if POLY: last vertice != first vertice */
	cycl = (bl->poly != -1);
	
	tot = cycl ? bl->nr : bl->nr - 1;
	
	path->len = tot + 1;
	/* exception: vector handle paths and polygon paths should be subdivided at least a factor resolu */
	if (path->len < nu->resolu * SEGMENTSU(nu)) {
		path->len = nu->resolu * SEGMENTSU(nu);
	}
	
	dist = (float *)MEM_mallocN(sizeof(float) * (tot + 1), "calcpathdist");

	/* all lengths in *dist */
	bevp = bevpfirst = (BevPoint *)(bl + 1);
	fp = dist;
	*fp = 0.0f;
	for (a = 0; a < tot; a++) {
		fp++;
		if (cycl && a == tot - 1)
			sub_v3_v3v3(xyz, bevpfirst->vec, bevp->vec);
		else
			sub_v3_v3v3(xyz, (bevp + 1)->vec, bevp->vec);
		
		*fp = *(fp - 1) + len_v3(xyz);
		bevp++;
	}
	
	path->totdist = *fp;
	
	/* the path verts  in path->data */
	/* now also with TILT value */
	pp = path->data = (PathPoint *)MEM_callocN(sizeof(PathPoint) * path->len, "pathdata");

	bevp = bevpfirst;
	bevpn = bevp + 1;
	bevplast = bevpfirst + (bl->nr - 1);
	fp = dist + 1;
	maxdist = dist + tot;
	fac = 1.0f / ((float)path->len - 1.0f);
	fac = fac * path->totdist;
	
	for (a = 0; a < path->len; a++) {
		
		d = ((float)a) * fac;
		
		/* we're looking for location (distance) 'd' in the array */
		while ((d >= *fp) && fp < maxdist) {
			fp++;
			if (bevp < bevplast) bevp++;
			bevpn = bevp + 1;
			if (UNLIKELY(bevpn > bevplast)) {
				bevpn = cycl ? bevpfirst : bevplast;
			}
		}
		
		fac1 = (*(fp) - d) / (*(fp) - *(fp - 1));
		fac2 = 1.0f - fac1;

		interp_v3_v3v3(pp->vec, bevp->vec, bevpn->vec, fac2);
		pp->vec[3] = fac1 * bevp->alfa   + fac2 * bevpn->alfa;
		pp->radius = fac1 * bevp->radius + fac2 * bevpn->radius;
		pp->weight = fac1 * bevp->weight + fac2 * bevpn->weight;
		interp_qt_qtqt(pp->quat, bevp->quat, bevpn->quat, fac2);
		normalize_qt(pp->quat);
		
		pp++;
	}
	
	MEM_freeN(dist);
}

static int interval_test(const int min, const int max, int p1, const int cycl)
{
	if (cycl) {
		p1 = mod_i(p1 - min, (max - min + 1)) + min;
	}
	else {
		if      (p1 < min) p1 = min;
		else if (p1 > max) p1 = max;
	}
	return p1;
}


/* calculate the deformation implied by the curve path at a given parametric position,
 * and returns whether this operation succeeded.
 *
 * note: ctime is normalized range <0-1>
 *
 * returns OK: 1/0
 */
int where_on_path(Object *ob, float ctime, float vec[4], float dir[3], float quat[4], float *radius, float *weight)
{
	Curve *cu;
	Nurb *nu;
	BevList *bl;
	Path *path;
	PathPoint *pp, *p0, *p1, *p2, *p3;
	float fac;
	float data[4];
	int cycl = 0, s0, s1, s2, s3;
	ListBase *nurbs;

	if (ob == NULL || ob->type != OB_CURVE) return 0;
	cu = ob->data;
	if (ob->curve_cache == NULL || ob->curve_cache->path == NULL || ob->curve_cache->path->data == NULL) {
		printf("no path!\n");
		return 0;
	}
	path = ob->curve_cache->path;
	pp = path->data;
	
	/* test for cyclic */
	bl = ob->curve_cache->bev.first;
	if (!bl) return 0;
	if (!bl->nr) return 0;
	if (bl->poly > -1) cycl = 1;

	ctime *= (path->len - 1);
	
	s1 = (int)floor(ctime);
	fac = (float)(s1 + 1) - ctime;

	/* path->len is corected for cyclic */
	s0 = interval_test(0, path->len - 1 - cycl, s1 - 1, cycl);
	s1 = interval_test(0, path->len - 1 - cycl, s1, cycl);
	s2 = interval_test(0, path->len - 1 - cycl, s1 + 1, cycl);
	s3 = interval_test(0, path->len - 1 - cycl, s1 + 2, cycl);

	p0 = pp + s0;
	p1 = pp + s1;
	p2 = pp + s2;
	p3 = pp + s3;

	/* note, commented out for follow constraint */
	//if (cu->flag & CU_FOLLOW) {

	key_curve_tangent_weights(1.0f - fac, data, KEY_BSPLINE);

	interp_v3_v3v3v3v3(dir, p0->vec, p1->vec, p2->vec, p3->vec, data);

	/* make compatible with vectoquat */
	negate_v3(dir);
	//}

	nurbs = BKE_curve_editNurbs_get(cu);
	if (!nurbs)
		nurbs = &cu->nurb;
	nu = nurbs->first;

	/* make sure that first and last frame are included in the vectors here  */
	if (nu->type == CU_POLY) key_curve_position_weights(1.0f - fac, data, KEY_LINEAR);
	else if (nu->type == CU_BEZIER) key_curve_position_weights(1.0f - fac, data, KEY_LINEAR);
	else if (s0 == s1 || p2 == p3) key_curve_position_weights(1.0f - fac, data, KEY_CARDINAL);
	else key_curve_position_weights(1.0f - fac, data, KEY_BSPLINE);

	vec[0] = data[0] * p0->vec[0] + data[1] * p1->vec[0] + data[2] * p2->vec[0] + data[3] * p3->vec[0]; /* X */
	vec[1] = data[0] * p0->vec[1] + data[1] * p1->vec[1] + data[2] * p2->vec[1] + data[3] * p3->vec[1]; /* Y */
	vec[2] = data[0] * p0->vec[2] + data[1] * p1->vec[2] + data[2] * p2->vec[2] + data[3] * p3->vec[2]; /* Z */
	vec[3] = data[0] * p0->vec[3] + data[1] * p1->vec[3] + data[2] * p2->vec[3] + data[3] * p3->vec[3]; /* Tilt, should not be needed since we have quat still used */

	if (quat) {
		float totfac, q1[4], q2[4];

		totfac = data[0] + data[3];
		if (totfac > FLT_EPSILON) interp_qt_qtqt(q1, p0->quat, p3->quat, data[3] / totfac);
		else copy_qt_qt(q1, p1->quat);

		totfac = data[1] + data[2];
		if (totfac > FLT_EPSILON) interp_qt_qtqt(q2, p1->quat, p2->quat, data[2] / totfac);
		else copy_qt_qt(q2, p3->quat);

		totfac = data[0] + data[1] + data[2] + data[3];
		if (totfac > FLT_EPSILON) interp_qt_qtqt(quat, q1, q2, (data[1] + data[2]) / totfac);
		else copy_qt_qt(quat, q2);
	}

	if (radius)
		*radius = data[0] * p0->radius + data[1] * p1->radius + data[2] * p2->radius + data[3] * p3->radius;

	if (weight)
		*weight = data[0] * p0->weight + data[1] * p1->weight + data[2] * p2->weight + data[3] * p3->weight;

	return 1;
}

/* ******************************************************************** */
/* Dupli-Geometry */

typedef struct DupliContext {
	EvaluationContext *eval_ctx;
	bool do_update;
	bool animated;
	Group *group; /* XXX child objects are selected from this group if set, could be nicer */
	
	Scene *scene;
	Object *object;
	float space_mat[4][4];
	int lay;
	
	int persistent_id[MAX_DUPLI_RECUR];
	int level;
	int index;
	
	const struct DupliGenerator *gen;
	
	/* result containers */
	ListBase *duplilist; /* legacy doubly-linked list */
} DupliContext;

typedef struct DupliGenerator {
	int type;				/* dupli type */
	bool recursive;         /* generate duplis recursively */
	void (*make_duplis)(const DupliContext *ctx);
} DupliGenerator;

static const DupliGenerator *get_dupli_generator(const DupliContext *ctx);

/* create initial context for root object */
static DupliContext init_context(EvaluationContext *eval_ctx, Scene *scene, Object *ob, float space_mat[4][4], bool update)
{
	DupliContext ctx;
	ctx.eval_ctx = eval_ctx;
	ctx.scene = scene;
	/* don't allow BKE_object_handle_update for viewport during render, can crash */
	ctx.do_update = update && !(G.is_rendering && !eval_ctx->for_render);
	ctx.animated = false;
	ctx.group = NULL;
	
	ctx.object = ob;
	if (space_mat)
		copy_m4_m4(ctx.space_mat, space_mat);
	else
		unit_m4(ctx.space_mat);
	ctx.lay = ob->lay;
	ctx.level = 0;
	
	ctx.gen = get_dupli_generator(&ctx);
	
	ctx.duplilist = NULL;
	
	return ctx;
}

/* create sub-context for recursive duplis */
static DupliContext copy_dupli_context(const DupliContext *ctx, Object *ob, float mat[4][4], int index, bool animated)
{
	DupliContext rctx = *ctx;
	rctx.animated |= animated; /* object animation makes all children animated */
	
	/* XXX annoying, previously was done by passing an ID* argument, this at least is more explicit */
	if (ctx->gen->type == OB_DUPLIGROUP)
		rctx.group = ctx->object->dup_group;
	
	rctx.object = ob;
	if (mat)
		mul_m4_m4m4(rctx.space_mat, (float (*)[4])ctx->space_mat, mat);
	rctx.persistent_id[rctx.level] = index;
	++rctx.level;
	
	rctx.gen = get_dupli_generator(&rctx);
	
	return rctx;
}

/* setup a dupli object, allocation happens outside */
static DupliObject *make_dupli(const DupliContext *ctx,
                               Object *ob, float mat[4][4], int index,
                               bool animated, bool hide)
{
	DupliObject *dob;
	int i;
	
	/* add a DupliObject instance to the result container */
	if (ctx->duplilist) {
		dob = MEM_callocN(sizeof(DupliObject), "dupli object");
		BLI_addtail(ctx->duplilist, dob);
	}
	else {
		return NULL;
	}
	
	dob->ob = ob;
	mul_m4_m4m4(dob->mat, (float (*)[4])ctx->space_mat, mat);
	copy_m4_m4(dob->omat, ob->obmat);
	dob->type = ctx->gen->type;
	dob->animated = animated || ctx->animated; /* object itself or some parent is animated */

	dob->origlay = ob->lay;
	ob->lay = ctx->lay;

	/* set persistent id, which is an array with a persistent index for each level
	 * (particle number, vertex number, ..). by comparing this we can find the same
	 * dupli object between frames, which is needed for motion blur. last level
	 * goes first in the array. */
	dob->persistent_id[0] = index;
	for (i = 0; i < ctx->level; i++)
		dob->persistent_id[i+1] = ctx->persistent_id[ctx->level - 1 - i];
	
	if (hide)
		dob->no_draw = true;
	/* metaballs never draw in duplis, they are instead merged into one by the basis
	 * mball outside of the group. this does mean that if that mball is not in the
	 * scene, they will not show up at all, limitation that should be solved once. */
	if (ob->type == OB_MBALL)
		dob->no_draw = true;

	/* recursive dupli objects,
	 * simple preventing of too deep nested groups with MAX_DUPLI_RECUR
	 */
	if (ctx->gen->recursive && ctx->level < MAX_DUPLI_RECUR) {
		DupliContext rctx = copy_dupli_context(ctx, ob, mat, index, animated);
		if (rctx.gen) {
			copy_m4_m4(ob->obmat, dob->mat);
			rctx.gen->make_duplis(&rctx);
			copy_m4_m4(ob->obmat, dob->omat);
		}
	}
	
	return dob;
}

/* ---- Child Duplis ---- */

typedef void (*MakeChildDuplisFunc)(const DupliContext *ctx, void *userdata, Object *child, float child_obmat[4][4]);

BLI_INLINE bool is_child(const Object *ob, const Object *parent)
{
	const Object *ob_parent = ob->parent;
	while (ob_parent) {
		if (ob_parent == parent)
			return true;
		ob_parent = ob_parent->parent;
	}
	return false;
}

/* create duplis from every child in scene or group */
static void make_child_duplis(const DupliContext *ctx, void *userdata, MakeChildDuplisFunc make_child_duplis)
{
	Object *parent = ctx->object;
	Object *obedit = ctx->scene->obedit;
	float obmat[4][4]; /* child obmat in dupli context space */
	
	if (ctx->group) {
		unsigned int lay = ctx->group->layer;
		GroupObject *go;
		for (go = ctx->group->gobject.first; go; go = go->next) {
			Object *ob = go->ob;
			
			if ((ob->lay & lay) && ob != obedit && is_child(ob, parent)) {
				/* mballs have a different dupli handling */
				if (ob->type != OB_MBALL)
					ob->flag |= OB_DONE;  /* doesnt render */
				
				mul_m4_m4m4(obmat, (float (*)[4])ctx->space_mat, ob->obmat);
				
				make_child_duplis(ctx, userdata, ob, obmat);
			}
		}
	}
	else {
		unsigned int lay = ctx->scene->lay;
		Base *base;
		for (base = ctx->scene->base.first; base; base = base->next) {
			Object *ob = base->object;
			
			if ((base->lay & lay) && ob != obedit && is_child(ob, parent)) {
				/* mballs have a different dupli handling */
				if (ob->type != OB_MBALL)
					ob->flag |= OB_DONE;  /* doesnt render */
				
				mul_m4_m4m4(obmat, (float (*)[4])ctx->space_mat, ob->obmat);
				
				make_child_duplis(ctx, userdata, ob, obmat);
				
				/* Set proper layer in case of scene looping,
				 * in case of groups the object layer will be
				 * changed when it's duplicated due to the
				 * group duplication.
				 */
				ob->lay = ctx->object->lay;
			}
		}
	}
}


/*---- Implementations ----*/

/* OB_DUPLIGROUP */
static void make_duplis_group(const DupliContext *ctx)
{
	bool for_render = ctx->eval_ctx->for_render;
	Object *ob = ctx->object;
	Group *group;
	GroupObject *go;
	float ob_obmat_ofs[4][4], id;
	bool animated, hide;

	if (ob->dup_group == NULL) return;
	group = ob->dup_group;
	
	/* don't access 'ob->obmat' from now on. */
	copy_m4_m4(ob_obmat_ofs, ob->obmat);

	if (!is_zero_v3(group->dupli_ofs)) {
		float tvec[3];
		copy_v3_v3(tvec, group->dupli_ofs);
		mul_mat3_m4_v3(ob_obmat_ofs, tvec);
		sub_v3_v3(ob_obmat_ofs[3], tvec);
	}

	/* handles animated groups */

	/* we need to check update for objects that are not in scene... */
	if (ctx->do_update) {
		/* note: update is optional because we don't always need object
		 * transformations to be correct. Also fixes bug [#29616]. */
		BKE_group_handle_recalc_and_update(ctx->eval_ctx, ctx->scene, ob, group);
	}

	animated = BKE_group_is_animated(group, ob);
	
	for (go = group->gobject.first, id = 0; go; go = go->next, id++) {
		/* note, if you check on layer here, render goes wrong... it still deforms verts and uses parent imat */
		if (go->ob != ob) {
			/* check the group instance and object layers match, also that the object visible flags are ok. */
			hide = (go->ob->lay & group->layer) == 0 ||
			       (for_render ? go->ob->restrictflag & OB_RESTRICT_RENDER : go->ob->restrictflag & OB_RESTRICT_VIEW);
			
			make_dupli(ctx, go->ob, ob_obmat_ofs, id, animated, hide);
		}
	}
}

const DupliGenerator gen_dupli_group = {
    OB_DUPLIGROUP,                  /* type */
    true,                           /* recursive */
    make_duplis_group               /* make_duplis */
};

/* OB_DUPLIFRAMES */
static void make_duplis_frames(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *ob = ctx->object;
	extern int enable_cu_speed; /* object.c */
	Object copyob;
	int cfrao = scene->r.cfra;
	int dupend = ob->dupend;
	
	/* dupliframes not supported inside groups */
	if (ctx->group)
		return;
	/* if we don't have any data/settings which will lead to object movement,
	 * don't waste time trying, as it will all look the same...
	 */
	if (ob->parent == NULL && ob->constraints.first == NULL && ob->adt == NULL)
		return;
	
	/* make a copy of the object's original data (before any dupli-data overwrites it) 
	 * as we'll need this to keep track of unkeyed data
	 *	- this doesn't take into account other data that can be reached from the object,
	 *	  for example it's shapekeys or bones, hence the need for an update flush at the end
	 */
	copyob = *ob;
	
	/* duplicate over the required range */
	if (ob->transflag & OB_DUPLINOSPEED) enable_cu_speed = 0;
	
	for (scene->r.cfra = ob->dupsta; scene->r.cfra <= dupend; scene->r.cfra++) {
		short ok = 1;
		
		/* - dupoff = how often a frames within the range shouldn't be made into duplis
		 * - dupon = the length of each "skipping" block in frames
		 */
		if (ob->dupoff) {
			ok = scene->r.cfra - ob->dupsta;
			ok = ok % (ob->dupon + ob->dupoff);
			ok = (ok < ob->dupon);
		}
		
		if (ok) {
			DupliObject *dob;
			
			/* WARNING: doing animation updates in this way is not terribly accurate, as the dependencies
			 * and/or other objects which may affect this object's transforms are not updated either.
			 * However, this has always been the way that this worked (i.e. pre 2.5), so I guess that it'll be fine!
			 */
			BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, (float)scene->r.cfra, ADT_RECALC_ANIM); /* ob-eval will do drivers, so we don't need to do them */
			BKE_object_where_is_calc_time(scene, ob, (float)scene->r.cfra);
			
			dob = make_dupli(ctx, ob, ob->obmat, scene->r.cfra, false, false);
			copy_m4_m4(dob->omat, copyob.obmat);
		}
	}

	enable_cu_speed = 1;
	
	/* reset frame to original frame, then re-evaluate animation as above 
	 * as 2.5 animation data may have far-reaching consequences
	 */
	scene->r.cfra = cfrao;
	
	BKE_animsys_evaluate_animdata(scene, &ob->id, ob->adt, (float)scene->r.cfra, ADT_RECALC_ANIM); /* ob-eval will do drivers, so we don't need to do them */
	BKE_object_where_is_calc_time(scene, ob, (float)scene->r.cfra);
	
	/* but, to make sure unkeyed object transforms are still sane, 
	 * let's copy object's original data back over
	 */
	*ob = copyob;
}

const DupliGenerator gen_dupli_frames = {
    OB_DUPLIFRAMES,                 /* type */
    false,                          /* recursive */
    make_duplis_frames              /* make_duplis */
};

/* OB_DUPLIVERTS */
typedef struct VertexDupliData {
	DerivedMesh *dm;
	BMEditMesh *edit_btmesh;
	int totvert;
	float (*orco)[3];
	bool use_rotation;
	
	const DupliContext *ctx;
	Object *inst_ob; /* object to instantiate (argument for vertex map callback) */
	const float (*inst_obmat)[4];
} VertexDupliData;

static void vertex_dupli__mapFunc(void *userData, int index, const float co[3],
                                  const float no_f[3], const short no_s[3])
{
	const VertexDupliData *vdd = userData;
	const DupliContext *ctx = vdd->ctx;
	DupliObject *dob;
	float vec[3], q2[4], mat[3][3], tmat[4][4], obmat[4][4];
	int origlay;
	
	copy_v3_v3(vec, co);
	/* rotate into world space, offset by child origin */
	mul_mat3_m4_v3(ctx->object->obmat, vec);
	add_v3_v3(vec, vdd->inst_obmat[3]);
	
	copy_m4_m4(obmat, (float (*)[4])vdd->inst_obmat);
	copy_v3_v3(obmat[3], vec);
	
	if (ctx->object->transflag & OB_DUPLIROT) {
		if (no_f) {
			vec[0] = -no_f[0]; vec[1] = -no_f[1]; vec[2] = -no_f[2];
		}
		else if (no_s) {
			vec[0] = -no_s[0]; vec[1] = -no_s[1]; vec[2] = -no_s[2];
		}
		
		vec_to_quat(q2, vec, vdd->inst_ob->trackflag, vdd->inst_ob->upflag);
		
		quat_to_mat3(mat, q2);
		copy_m4_m4(tmat, obmat);
		mul_m4_m4m3(obmat, tmat, mat);
	}

	origlay = vdd->inst_ob->lay;
	dob = make_dupli(vdd->ctx, vdd->inst_ob, obmat, index, false, false);
	/* restore the original layer so that each dupli will have proper dob->origlay */
	vdd->inst_ob->lay = origlay;

	if (vdd->orco)
		copy_v3_v3(dob->orco, vdd->orco[index]);
}

static void make_child_duplis_verts(const DupliContext *UNUSED(ctx), void *userdata, Object *child, float child_obmat[4][4])
{
	VertexDupliData *vdd = userdata;
	DerivedMesh *dm = vdd->dm;
	
	vdd->inst_ob = child;
	vdd->inst_obmat = child_obmat;
	
	if (vdd->edit_btmesh) {
		dm->foreachMappedVert(dm, vertex_dupli__mapFunc, vdd,
		                      vdd->use_rotation ? DM_FOREACH_USE_NORMAL : 0);
	}
	else {
		int a, totvert = vdd->totvert;
		float vec[3], no[3];
		
		if (vdd->use_rotation) {
			for (a = 0; a < totvert; a++) {
				dm->getVertCo(dm, a, vec);
				dm->getVertNo(dm, a, no);
				
				vertex_dupli__mapFunc(vdd, a, vec, no, NULL);
			}
		}
		else {
			for (a = 0; a < totvert; a++) {
				dm->getVertCo(dm, a, vec);
				
				vertex_dupli__mapFunc(vdd, a, vec, NULL, NULL);
			}
		}
	}
}

static void make_duplis_verts(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *parent = ctx->object;
	bool for_render = ctx->eval_ctx->for_render;
	VertexDupliData vdd;
	
	vdd.ctx = ctx;
	vdd.use_rotation = parent->transflag & OB_DUPLIROT;

	/* gather mesh info */
	{
		Mesh *me = parent->data;
		BMEditMesh *em = BKE_editmesh_from_object(parent);
		CustomDataMask dm_mask = (for_render ? CD_MASK_BAREMESH | CD_MASK_ORCO : CD_MASK_BAREMESH);
		
		if (em)
			vdd.dm = editbmesh_get_derived_cage(scene, parent, em, dm_mask);
		else
			vdd.dm = mesh_get_derived_final(scene, parent, dm_mask);
		vdd.edit_btmesh = me->edit_btmesh;
		
		if (for_render)
			vdd.orco = vdd.dm->getVertDataArray(vdd.dm, CD_ORCO);
		else
			vdd.orco = NULL;
		
		vdd.totvert = vdd.dm->getNumVerts(vdd.dm);
	}
	
	make_child_duplis(ctx, &vdd, make_child_duplis_verts);
	
	vdd.dm->release(vdd.dm);
}

const DupliGenerator gen_dupli_verts = {
    OB_DUPLIVERTS,                  /* type */
    true,                           /* recursive */
    make_duplis_verts               /* make_duplis */
};

/* OB_DUPLIVERTS - FONT */
static Object *find_family_object(const char *family, size_t family_len, unsigned int ch, GHash *family_gh)
{
	Object **ob_pt;
	Object *ob;
	void *ch_key = SET_UINT_IN_POINTER(ch);

	if ((ob_pt = (Object **)BLI_ghash_lookup_p(family_gh, ch_key))) {
		ob = *ob_pt;
	}
	else {
		char ch_utf8[7];
		size_t ch_utf8_len;

		ch_utf8_len = BLI_str_utf8_from_unicode(ch, ch_utf8);
		ch_utf8[ch_utf8_len] = '\0';
		ch_utf8_len += 1;  /* compare with null terminator */

		for (ob = G.main->object.first; ob; ob = ob->id.next) {
			if (STREQLEN(ob->id.name + 2 + family_len, ch_utf8, ch_utf8_len)) {
				if (STREQLEN(ob->id.name + 2, family, family_len)) {
					break;
				}
			}
		}

		/* inserted value can be NULL, just to save searches in future */
		BLI_ghash_insert(family_gh, ch_key, ob);
	}

	return ob;
}

static void make_duplis_font(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *par = ctx->object;
	GHash *family_gh;
	Object *ob;
	Curve *cu;
	struct CharTrans *ct, *chartransdata = NULL;
	float vec[3], obmat[4][4], pmat[4][4], fsize, xof, yof;
	int text_len, a;
	size_t family_len;
	const wchar_t *text = NULL;
	bool text_free = false;
	
	/* font dupliverts not supported inside groups */
	if (ctx->group)
		return;
	
	copy_m4_m4(pmat, par->obmat);
	
	/* in par the family name is stored, use this to find the other objects */
	
	BKE_vfont_to_curve_ex(G.main, scene, par, FO_DUPLI, NULL,
	                      &text, &text_len, &text_free, &chartransdata);

	if (text == NULL || chartransdata == NULL) {
		return;
	}

	cu = par->data;
	fsize = cu->fsize;
	xof = cu->xof;
	yof = cu->yof;
	
	ct = chartransdata;
	
	/* cache result */
	family_len = strlen(cu->family);
	family_gh = BLI_ghash_int_new_ex(__func__, 256);

	/* advance matching BLI_strncpy_wchar_from_utf8 */
	for (a = 0; a < text_len; a++, ct++) {
		
		ob = find_family_object(cu->family, family_len, text[a], family_gh);
		if (ob) {
			vec[0] = fsize * (ct->xof - xof);
			vec[1] = fsize * (ct->yof - yof);
			vec[2] = 0.0;
			
			mul_m4_v3(pmat, vec);
			
			copy_m4_m4(obmat, par->obmat);

			if (UNLIKELY(ct->rot != 0.0f)) {
				float rmat[4][4];

				zero_v3(obmat[3]);
				unit_m4(rmat);
				rotate_m4(rmat, 'Z', -ct->rot);
				mul_m4_m4m4(obmat, obmat, rmat);
			}

			copy_v3_v3(obmat[3], vec);
			
			make_dupli(ctx, ob, obmat, a, false, false);
		}
	}

	if (text_free) {
		MEM_freeN((void *)text);
	}

	BLI_ghash_free(family_gh, NULL, NULL);

	MEM_freeN(chartransdata);
}

const DupliGenerator gen_dupli_verts_font = {
    OB_DUPLIVERTS,                  /* type */
    false,                          /* recursive */
    make_duplis_font                /* make_duplis */
};

/* OB_DUPLIFACES */
typedef struct FaceDupliData {
	DerivedMesh *dm;
	int totface;
	MPoly *mpoly;
	MLoop *mloop;
	MVert *mvert;
	float (*orco)[3];
	MLoopUV *mloopuv;
	bool use_scale;
	
	Object *inst_ob; /* object to instantiate (argument for vertex map callback) */
	const float (*inst_obmat)[4];
} FaceDupliData;

static void make_child_duplis_faces(const DupliContext *ctx, void *userdata, Object *child, float child_obmat[4][4])
{
	FaceDupliData *fdd = userdata;
	MPoly *mpoly = fdd->mpoly, *mp;
	MLoop *mloop = fdd->mloop;
	MVert *mvert = fdd->mvert;
	float (*orco)[3] = fdd->orco;
	MLoopUV *mloopuv = fdd->mloopuv;
	int a, totface = fdd->totface;
	float imat[3][3];
	DupliObject *dob;
	
	fdd->inst_ob = child;
	fdd->inst_obmat = child_obmat;
	
	copy_m3_m4(imat, child->parentinv);
	
	for (a = 0, mp = mpoly; a < totface; a++, mp++) {
		float *v1;
		float *v2;
		float *v3;
		/* float *v4; */ /* UNUSED */
		float cent[3], quat[4], mat[3][3], mat3[3][3], tmat[4][4], obmat[4][4];
		float f_no[3];
		MLoop *loopstart = mloop + mp->loopstart;

		if (UNLIKELY(mp->totloop < 3)) {
			continue;
		}
		else {
			BKE_mesh_calc_poly_normal(mp, mloop + mp->loopstart, mvert, f_no);
			v1 = mvert[loopstart[0].v].co;
			v2 = mvert[loopstart[1].v].co;
			v3 = mvert[loopstart[2].v].co;
		}

		/* translation */
		BKE_mesh_calc_poly_center(mp, loopstart, mvert, cent);

		/* rotate into world space, offset by child origin */
		mul_mat3_m4_v3(ctx->object->obmat, cent);
		add_v3_v3(cent, fdd->inst_obmat[3]);
		
		copy_m4_m4(obmat, (float (*)[4])fdd->inst_obmat);
		copy_v3_v3(obmat[3], cent);
		
		/* rotation */
		tri_to_quat_ex(quat, v1, v2, v3, f_no);
		quat_to_mat3(mat, quat);
		
		/* scale */
		if (fdd->use_scale) {
			float size = BKE_mesh_calc_poly_area(mp, loopstart, mvert, f_no);
			size = sqrtf(size) * ctx->object->dupfacesca;
			mul_m3_fl(mat, size);
		}
		
		copy_m3_m3(mat3, mat);
		mul_m3_m3m3(mat, imat, mat3);
		
		copy_m4_m4(tmat, obmat);
		mul_m4_m4m3(obmat, tmat, mat);
		
		dob = make_dupli(ctx, fdd->inst_ob, obmat, a, false, false);
		if (ctx->eval_ctx->for_render) {
			float w = 1.0f / (float)mp->totloop;

			if (orco) {
				int j;
				for (j = 0; j < mpoly->totloop; j++) {
					madd_v3_v3fl(dob->orco, orco[loopstart[j].v], w);
				}
			}

			if (mloopuv) {
				int j;
				for (j = 0; j < mpoly->totloop; j++) {
					madd_v2_v2fl(dob->uv, mloopuv[mp->loopstart + j].uv, w);
				}
			}
		}
	}
}

static void make_duplis_faces(const DupliContext *ctx)
{
	Scene *scene = ctx->scene;
	Object *parent = ctx->object;
	bool for_render = ctx->eval_ctx->for_render;
	FaceDupliData fdd;
	
	fdd.use_scale = parent->transflag & OB_DUPLIFACES_SCALE;
	
	/* gather mesh info */
	{
		BMEditMesh *em = BKE_editmesh_from_object(parent);
		CustomDataMask dm_mask = (for_render ? CD_MASK_BAREMESH | CD_MASK_ORCO | CD_MASK_MLOOPUV : CD_MASK_BAREMESH);
		
		if (em)
			fdd.dm = editbmesh_get_derived_cage(scene, parent, em, dm_mask);
		else
			fdd.dm = mesh_get_derived_final(scene, parent, dm_mask);
		
		if (for_render) {
			fdd.orco = fdd.dm->getVertDataArray(fdd.dm, CD_ORCO);
			fdd.mloopuv = fdd.dm->getLoopDataArray(fdd.dm, CD_MLOOPUV);
		}
		else {
			fdd.orco = NULL;
			fdd.mloopuv = NULL;
		}
		
		fdd.totface = fdd.dm->getNumPolys(fdd.dm);
		fdd.mpoly = fdd.dm->getPolyArray(fdd.dm);
		fdd.mloop = fdd.dm->getLoopArray(fdd.dm);
		fdd.mvert = fdd.dm->getVertArray(fdd.dm);
	}
	
	make_child_duplis(ctx, &fdd, make_child_duplis_faces);
	
	fdd.dm->release(fdd.dm);
}

const DupliGenerator gen_dupli_faces = {
    OB_DUPLIFACES,                  /* type */
    true,                           /* recursive */
    make_duplis_faces               /* make_duplis */
};

/* OB_DUPLIPARTS */
static void make_duplis_particle_system(const DupliContext *ctx, ParticleSystem *psys)
{
	Scene *scene = ctx->scene;
	Object *par = ctx->object;
	bool for_render = ctx->eval_ctx->for_render;
	
	GroupObject *go;
	Object *ob = NULL, **oblist = NULL, obcopy, *obcopylist = NULL;
	DupliObject *dob;
	ParticleDupliWeight *dw;
	ParticleSettings *part;
	ParticleData *pa;
	ChildParticle *cpa = NULL;
	ParticleKey state;
	ParticleCacheKey *cache;
	float ctime, pa_time, scale = 1.0f;
	float tmat[4][4], mat[4][4], pamat[4][4], vec[3], size = 0.0;
	float (*obmat)[4], (*oldobmat)[4];
	int a, b, hair = 0;
	int totpart, totchild, totgroup = 0 /*, pa_num */;
	int dupli_type_hack = !BKE_scene_use_new_shading_nodes(scene);

	int no_draw_flag = PARS_UNEXIST;

	if (psys == NULL) return;
	
	part = psys->part;

	if (part == NULL)
		return;

	if (!psys_check_enabled(par, psys))
		return;

	if (!for_render)
		no_draw_flag |= PARS_NO_DISP;
	
	ctime = BKE_scene_frame_get(scene); /* NOTE: in old animsys, used parent object's timeoffset... */

	totpart = psys->totpart;
	totchild = psys->totchild;

	BLI_srandom(31415926 + psys->seed);

	if ((psys->renderdata || part->draw_as == PART_DRAW_REND) && ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
		ParticleSimulationData sim = {NULL};
		sim.scene = scene;
		sim.ob = par;
		sim.psys = psys;
		sim.psmd = psys_get_modifier(par, psys);
		/* make sure emitter imat is in global coordinates instead of render view coordinates */
		invert_m4_m4(par->imat, par->obmat);

		/* first check for loops (particle system object used as dupli object) */
		if (part->ren_as == PART_DRAW_OB) {
			if (ELEM(part->dup_ob, NULL, par))
				return;
		}
		else { /*PART_DRAW_GR */
			if (part->dup_group == NULL || part->dup_group->gobject.first == NULL)
				return;

			if (BLI_findptr(&part->dup_group->gobject, par, offsetof(GroupObject, ob))) {
				return;
			}
		}

		/* if we have a hair particle system, use the path cache */
		if (part->type == PART_HAIR) {
			if (psys->flag & PSYS_HAIR_DONE)
				hair = (totchild == 0 || psys->childcache) && psys->pathcache;
			if (!hair)
				return;
			
			/* we use cache, update totchild according to cached data */
			totchild = psys->totchildcache;
			totpart = psys->totcached;
		}

		psys_check_group_weights(part);

		psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

		/* gather list of objects or single object */
		if (part->ren_as == PART_DRAW_GR) {
			if (ctx->do_update) {
				BKE_group_handle_recalc_and_update(ctx->eval_ctx, scene, par, part->dup_group);
			}

			if (part->draw & PART_DRAW_COUNT_GR) {
				for (dw = part->dupliweights.first; dw; dw = dw->next)
					totgroup += dw->count;
			}
			else {
				for (go = part->dup_group->gobject.first; go; go = go->next)
					totgroup++;
			}

			/* we also copy the actual objects to restore afterwards, since
			 * BKE_object_where_is_calc_time will change the object which breaks transform */
			oblist = MEM_callocN(totgroup * sizeof(Object *), "dupgroup object list");
			obcopylist = MEM_callocN(totgroup * sizeof(Object), "dupgroup copy list");

			if (part->draw & PART_DRAW_COUNT_GR && totgroup) {
				dw = part->dupliweights.first;

				for (a = 0; a < totgroup; dw = dw->next) {
					for (b = 0; b < dw->count; b++, a++) {
						oblist[a] = dw->ob;
						obcopylist[a] = *dw->ob;
					}
				}
			}
			else {
				go = part->dup_group->gobject.first;
				for (a = 0; a < totgroup; a++, go = go->next) {
					oblist[a] = go->ob;
					obcopylist[a] = *go->ob;
				}
			}
		}
		else {
			ob = part->dup_ob;
			obcopy = *ob;
		}

		if (totchild == 0 || part->draw & PART_DRAW_PARENT)
			a = 0;
		else
			a = totpart;

		for (pa = psys->particles; a < totpart + totchild; a++, pa++) {
			if (a < totpart) {
				/* handle parent particle */
				if (pa->flag & no_draw_flag)
					continue;

				/* pa_num = pa->num; */ /* UNUSED */
				pa_time = pa->time;
				size = pa->size;
			}
			else {
				/* handle child particle */
				cpa = &psys->child[a - totpart];

				/* pa_num = a; */ /* UNUSED */
				pa_time = psys->particles[cpa->parent].time;
				size = psys_get_child_size(psys, cpa, ctime, NULL);
			}

			/* some hair paths might be non-existent so they can't be used for duplication */
			if (hair &&
			    ((a < totpart && psys->pathcache[a]->steps < 0) ||
			     (a >= totpart && psys->childcache[a - totpart]->steps < 0)))
			{
				continue;
			}

			if (part->ren_as == PART_DRAW_GR) {
				/* prevent divide by zero below [#28336] */
				if (totgroup == 0)
					continue;

				/* for groups, pick the object based on settings */
				if (part->draw & PART_DRAW_RAND_GR)
					b = BLI_rand() % totgroup;
				else
					b = a % totgroup;

				ob = oblist[b];
				obmat = oblist[b]->obmat;
				oldobmat = obcopylist[b].obmat;
			}
			else {
				obmat = ob->obmat;
				oldobmat = obcopy.obmat;
			}

			if (hair) {
				/* hair we handle separate and compute transform based on hair keys */
				if (a < totpart) {
					cache = psys->pathcache[a];
					psys_get_dupli_path_transform(&sim, pa, NULL, cache, pamat, &scale);
				}
				else {
					cache = psys->childcache[a - totpart];
					psys_get_dupli_path_transform(&sim, NULL, cpa, cache, pamat, &scale);
				}

				copy_v3_v3(pamat[3], cache->co);
				pamat[3][3] = 1.0f;
				
			}
			else {
				/* first key */
				state.time = ctime;
				if (psys_get_particle_state(&sim, a, &state, 0) == 0) {
					continue;
				}
				else {
					float tquat[4];
					normalize_qt_qt(tquat, state.rot);
					quat_to_mat4(pamat, tquat);
					copy_v3_v3(pamat[3], state.co);
					pamat[3][3] = 1.0f;
				}
			}

			if (part->ren_as == PART_DRAW_GR && psys->part->draw & PART_DRAW_WHOLE_GR) {
				for (go = part->dup_group->gobject.first, b = 0; go; go = go->next, b++) {

					copy_m4_m4(tmat, oblist[b]->obmat);
					/* apply particle scale */
					mul_mat3_m4_fl(tmat, size * scale);
					mul_v3_fl(tmat[3], size * scale);
					/* group dupli offset, should apply after everything else */
					if (!is_zero_v3(part->dup_group->dupli_ofs))
						sub_v3_v3(tmat[3], part->dup_group->dupli_ofs);
					/* individual particle transform */
					mul_m4_m4m4(mat, pamat, tmat);

					dob = make_dupli(ctx, go->ob, mat, a, false, false);
					dob->particle_system = psys;
					copy_m4_m4(dob->omat, obcopylist[b].obmat);
					if (for_render)
						psys_get_dupli_texture(psys, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
				}
			}
			else {
				/* to give ipos in object correct offset */
				BKE_object_where_is_calc_time(scene, ob, ctime - pa_time);

				copy_v3_v3(vec, obmat[3]);
				obmat[3][0] = obmat[3][1] = obmat[3][2] = 0.0f;

				/* particle rotation uses x-axis as the aligned axis, so pre-rotate the object accordingly */
				if ((part->draw & PART_DRAW_ROTATE_OB) == 0) {
					float xvec[3], q[4], size_mat[4][4], original_size[3];

					mat4_to_size(original_size, obmat);
					size_to_mat4(size_mat, original_size);

					xvec[0] = -1.f;
					xvec[1] = xvec[2] = 0;
					vec_to_quat(q, xvec, ob->trackflag, ob->upflag);
					quat_to_mat4(obmat, q);
					obmat[3][3] = 1.0f;
					
					/* add scaling if requested */
					if ((part->draw & PART_DRAW_NO_SCALE_OB) == 0)
						mul_m4_m4m4(obmat, obmat, size_mat);
				}
				else if (part->draw & PART_DRAW_NO_SCALE_OB) {
					/* remove scaling */
					float size_mat[4][4], original_size[3];

					mat4_to_size(original_size, obmat);
					size_to_mat4(size_mat, original_size);
					invert_m4(size_mat);

					mul_m4_m4m4(obmat, obmat, size_mat);
				}

				mul_m4_m4m4(tmat, pamat, obmat);
				mul_mat3_m4_fl(tmat, size * scale);

				copy_m4_m4(mat, tmat);

				if (part->draw & PART_DRAW_GLOBAL_OB)
					add_v3_v3v3(mat[3], mat[3], vec);

				dob = make_dupli(ctx, ob, mat, a, false, false);
				dob->particle_system = psys;
				copy_m4_m4(dob->omat, oldobmat);
				if (for_render)
					psys_get_dupli_texture(psys, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
				/* XXX blender internal needs this to be set to dupligroup to render
				 * groups correctly, but we don't want this hack for cycles */
				if (dupli_type_hack && ctx->group)
					dob->type = OB_DUPLIGROUP;
			}
		}

		/* restore objects since they were changed in BKE_object_where_is_calc_time */
		if (part->ren_as == PART_DRAW_GR) {
			for (a = 0; a < totgroup; a++)
				*(oblist[a]) = obcopylist[a];
		}
		else
			*ob = obcopy;
	}

	/* clean up */
	if (oblist)
		MEM_freeN(oblist);
	if (obcopylist)
		MEM_freeN(obcopylist);

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}
}

static void make_duplis_particles(const DupliContext *ctx)
{
	ParticleSystem *psys;
	int psysid;

	/* particle system take up one level in id, the particles another */
	for (psys = ctx->object->particlesystem.first, psysid = 0; psys; psys = psys->next, psysid++) {
		/* particles create one more level for persistent psys index */
		DupliContext pctx = copy_dupli_context(ctx, ctx->object, NULL, psysid, false);
		make_duplis_particle_system(&pctx, psys);
	}
}

const DupliGenerator gen_dupli_particles = {
    OB_DUPLIPARTS,                  /* type */
    false,                          /* recursive */
    make_duplis_particles           /* make_duplis */
};

/* ------------- */

/* select dupli generator from given context */
static const DupliGenerator *get_dupli_generator(const DupliContext *ctx)
{
	int transflag = ctx->object->transflag;
	int restrictflag = ctx->object->restrictflag;
	
	if ((transflag & OB_DUPLI) == 0)
		return NULL;
	
	/* Should the dupli's be generated for this object? - Respect restrict flags */
	if (ctx->eval_ctx->for_render ? (restrictflag & OB_RESTRICT_RENDER) : (restrictflag & OB_RESTRICT_VIEW))
		return NULL;
	
	if (transflag & OB_DUPLIPARTS) {
		return &gen_dupli_particles;
	}
	else if (transflag & OB_DUPLIVERTS) {
		if (ctx->object->type == OB_MESH) {
			return &gen_dupli_verts;
		}
		else if (ctx->object->type == OB_FONT) {
			return &gen_dupli_verts_font;
		}
	}
	else if (transflag & OB_DUPLIFACES) {
		if (ctx->object->type == OB_MESH)
			return &gen_dupli_faces;
	}
	else if (transflag & OB_DUPLIFRAMES) {
		return &gen_dupli_frames;
	}
	else if (transflag & OB_DUPLIGROUP) {
		return &gen_dupli_group;
	}
	
	return NULL;
}


/* ---- ListBase dupli container implementation ---- */

/* Returns a list of DupliObject */
ListBase *object_duplilist_ex(EvaluationContext *eval_ctx, Scene *scene, Object *ob, bool update)
{
	ListBase *duplilist = MEM_callocN(sizeof(ListBase), "duplilist");
	DupliContext ctx = init_context(eval_ctx, scene, ob, NULL, update);
	if (ctx.gen) {
		ctx.duplilist = duplilist;
		ctx.gen->make_duplis(&ctx);
	}
	
	return duplilist;
}

/* note: previously updating was always done, this is why it defaults to be on
 * but there are likely places it can be called without updating */
ListBase *object_duplilist(EvaluationContext *eval_ctx, Scene *sce, Object *ob)
{
	return object_duplilist_ex(eval_ctx, sce, ob, true);
}

void free_object_duplilist(ListBase *lb)
{
	DupliObject *dob;
	
	/* loop in reverse order, if object is instanced multiple times
	 * the original layer may not really be original otherwise, proper
	 * solution is more complicated */
	for (dob = lb->last; dob; dob = dob->prev) {
		dob->ob->lay = dob->origlay;
		copy_m4_m4(dob->ob->obmat, dob->omat);
	}
	
	BLI_freelistN(lb);
	MEM_freeN(lb);
}

int count_duplilist(Object *ob)
{
	if (ob->transflag & OB_DUPLI) {
		if (ob->transflag & OB_DUPLIVERTS) {
			if (ob->type == OB_MESH) {
				if (ob->transflag & OB_DUPLIVERTS) {
					ParticleSystem *psys = ob->particlesystem.first;
					int pdup = 0;

					for (; psys; psys = psys->next)
						pdup += psys->totpart;

					if (pdup == 0) {
						Mesh *me = ob->data;
						return me->totvert;
					}
					else
						return pdup;
				}
			}
		}
		else if (ob->transflag & OB_DUPLIFRAMES) {
			int tot = ob->dupend - ob->dupsta;
			tot /= (ob->dupon + ob->dupoff);
			return tot * ob->dupon;
		}
	}
	return 1;
}
