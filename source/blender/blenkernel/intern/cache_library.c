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
 * The Original Code is Copyright (C) 2015 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/cache_library.c
 *  \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_group_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim.h"
#include "BKE_cache_library.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_strands.h"

#include "BLF_translation.h"

#include "PTC_api.h"

#include "BPH_mass_spring.h"

CacheLibrary *BKE_cache_library_add(Main *bmain, const char *name)
{
	CacheLibrary *cachelib;
	char basename[MAX_NAME];

	cachelib = BKE_libblock_alloc(bmain, ID_CL, name);

	BLI_strncpy(basename, cachelib->id.name+2, sizeof(basename));
	BLI_filename_make_safe(basename);
	BLI_snprintf(cachelib->output_filepath, sizeof(cachelib->output_filepath), "//cache/%s.%s", basename, PTC_get_default_archive_extension());

	cachelib->source_mode = CACHE_LIBRARY_SOURCE_SCENE;
	cachelib->display_mode = CACHE_LIBRARY_DISPLAY_RESULT;
	cachelib->eval_mode = CACHE_LIBRARY_EVAL_REALTIME | CACHE_LIBRARY_EVAL_RENDER;

	/* cache everything by default */
	cachelib->data_types = CACHE_TYPE_ALL;

	return cachelib;
}

CacheLibrary *BKE_cache_library_copy(CacheLibrary *cachelib)
{
	CacheLibrary *cachelibn;
	
	cachelibn = BKE_libblock_copy(&cachelib->id);
	
	{
		CacheModifier *md;
		BLI_listbase_clear(&cachelibn->modifiers);
		for (md = cachelib->modifiers.first; md; md = md->next) {
			BKE_cache_modifier_copy(cachelibn, md);
		}
	}
	
	if (cachelib->id.lib) {
		BKE_id_lib_local_paths(G.main, cachelib->id.lib, &cachelibn->id);
	}
	
	return cachelibn;
}

void BKE_cache_library_free(CacheLibrary *cachelib)
{
	BKE_cache_modifier_clear(cachelib);
}

void BKE_cache_library_unlink(CacheLibrary *UNUSED(cachelib))
{
}

/* ========================================================================= */

static void cache_library_tag_recursive(int level, Object *ob)
{
	if (level > MAX_CACHE_GROUP_LEVEL)
		return;
	
	/* dupli group recursion */
	if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
		GroupObject *gob;
		
		for (gob = ob->dup_group->gobject.first; gob; gob = gob->next) {
			if (!(ob->id.flag & LIB_DOIT)) {
				ob->id.flag |= LIB_DOIT;
				
				cache_library_tag_recursive(level + 1, gob->ob);
			}
		}
	}
}

/* tag IDs contained in the cache library group */
void BKE_cache_library_make_object_list(Main *bmain, CacheLibrary *cachelib, ListBase *lb)
{
	if (cachelib) {
		Object *ob;
		LinkData *link;
		
		/* clear tags */
		BKE_main_id_tag_idcode(bmain, ID_OB, false);
		
		for (ob = bmain->object.first; ob; ob = ob->id.next) {
			if (ob->cache_library == cachelib) {
				cache_library_tag_recursive(0, ob);
			}
		}
		
		/* store object pointers in the list */
		for (ob = bmain->object.first; ob; ob = ob->id.next) {
			if (ob->id.flag & LIB_DOIT) {
				link = MEM_callocN(sizeof(LinkData), "cache library ID link");
				link->data = ob;
				BLI_addtail(lb, link);
			}
		}
	}
}

/* ========================================================================= */

const char *BKE_cache_item_name_prefix(int type)
{
	/* note: avoid underscores and the like here,
	 * the prefixes must be unique and safe when combined with arbitrary strings!
	 */
	switch (type) {
		case CACHE_TYPE_OBJECT: return "OBJECT";
		case CACHE_TYPE_DERIVED_MESH: return "MESH";
		case CACHE_TYPE_HAIR: return "HAIR";
		case CACHE_TYPE_HAIR_PATHS: return "HAIRPATHS";
		case CACHE_TYPE_PARTICLES: return "PARTICLES";
		default: BLI_assert(false); return NULL; break;
	}
}

void BKE_cache_item_name(Object *ob, int type, int index, char *name)
{
	if (index >= 0)
		sprintf(name, "%s_%s_%d", BKE_cache_item_name_prefix(type), ob->id.name+2, index);
	else
		sprintf(name, "%s_%s", BKE_cache_item_name_prefix(type), ob->id.name+2);
}

int BKE_cache_item_name_length(Object *ob, int type, int index)
{
	char *str_dummy = (char *)"";
	if (index >= 0)
		return BLI_snprintf(str_dummy, 0, "%s_%s_%d", BKE_cache_item_name_prefix(type), ob->id.name + 2, index);
	else
		return BLI_snprintf(str_dummy, 0, "%s_%s", BKE_cache_item_name_prefix(type), ob->id.name + 2);
}

eCacheReadSampleResult BKE_cache_read_result(int ptc_result)
{
	switch (ptc_result) {
		case PTC_READ_SAMPLE_INVALID: return CACHE_READ_SAMPLE_INVALID;
		case PTC_READ_SAMPLE_EARLY: return CACHE_READ_SAMPLE_EARLY;
		case PTC_READ_SAMPLE_LATE: return CACHE_READ_SAMPLE_LATE;
		case PTC_READ_SAMPLE_EXACT: return CACHE_READ_SAMPLE_EXACT;
		case PTC_READ_SAMPLE_INTERPOLATED: return CACHE_READ_SAMPLE_INTERPOLATED;
		default: BLI_assert(false); break; /* should never happen, enums out of sync? */
	}
	return CACHE_READ_SAMPLE_INVALID;
}

bool BKE_cache_library_validate_item(CacheLibrary *cachelib, Object *ob, int type, int index)
{
	if (!cachelib)
		return false;
	
	if (ELEM(type, CACHE_TYPE_DERIVED_MESH)) {
		if (ob->type != OB_MESH)
			return false;
	}
	else if (ELEM(type, CACHE_TYPE_PARTICLES, CACHE_TYPE_HAIR, CACHE_TYPE_HAIR_PATHS)) {
		ParticleSystem *psys = BLI_findlink(&ob->particlesystem, index);
		
		if (!psys)
			return false;
		
		if (ELEM(type, CACHE_TYPE_PARTICLES)) {
			if (psys->part->type != PART_EMITTER)
				return false;
		}
		
		if (ELEM(type, CACHE_TYPE_HAIR, CACHE_TYPE_HAIR_PATHS)) {
			if (psys->part->type != PART_HAIR)
				return false;
		}
	}
	
	return true;
}

/* ========================================================================= */

BLI_INLINE bool path_is_dirpath(const char *path)
{
	/* last char is a slash? */
	return *(BLI_last_slash(path) + 1) == '\0';
}

bool BKE_cache_archive_path_test(CacheLibrary *cachelib, const char *path)
{
	if (BLI_path_is_rel(path)) {
		if (!(G.relbase_valid || cachelib->id.lib))
			return false;
	}
	
	return true;
	
}

void BKE_cache_archive_path_ex(const char *path, Library *lib, const char *default_filename, char *result, int max)
{
	char abspath[FILE_MAX];
	
	result[0] = '\0';
	
	if (BLI_path_is_rel(path)) {
		if (G.relbase_valid || lib) {
			const char *relbase = lib ? lib->filepath : G.main->name;
			
			BLI_strncpy(abspath, path, sizeof(abspath));
			BLI_path_abs(abspath, relbase);
		}
		else {
			/* can't construct a valid path */
			return;
		}
	}
	else {
		BLI_strncpy(abspath, path, sizeof(abspath));
	}
	
	if (abspath[0] != '\0') {
		if (path_is_dirpath(abspath) || BLI_is_dir(abspath)) {
			if (default_filename && default_filename[0] != '\0')
				BLI_join_dirfile(result, max, abspath, default_filename);
		}
		else {
			BLI_strncpy(result, abspath, max);
		}
	}
}

void BKE_cache_archive_input_path(CacheLibrary *cachelib, char *result, int max)
{
	BKE_cache_archive_path_ex(cachelib->input_filepath, cachelib->id.lib, NULL, result, max);
}

void BKE_cache_archive_output_path(CacheLibrary *cachelib, char *result, int max)
{
	BKE_cache_archive_path_ex(cachelib->output_filepath, cachelib->id.lib, cachelib->id.name+2, result, max);
}


static struct PTCReaderArchive *find_active_cache(Scene *scene, CacheLibrary *cachelib)
{
	char filename[FILE_MAX];
	struct PTCReaderArchive *archive = NULL;
	
	const bool is_baking = cachelib->flag & CACHE_LIBRARY_BAKING;
	
	/* don't read results from output archive when baking */
	if (!is_baking) {
		if (cachelib->display_mode == CACHE_LIBRARY_DISPLAY_RESULT) {
			/* try using the output cache */
			BKE_cache_archive_output_path(cachelib, filename, sizeof(filename));
			archive = PTC_open_reader_archive(scene, filename);
		}
	}
	
	if (!archive && cachelib->source_mode == CACHE_LIBRARY_SOURCE_CACHE) {
		BKE_cache_archive_input_path(cachelib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
	}
	
	return archive;
}

bool BKE_cache_read_dupli_cache(CacheLibrary *cachelib, DupliCache *dupcache,
                                Scene *scene, Group *dupgroup, float frame, eCacheLibrary_EvalMode eval_mode)
{
	struct PTCReaderArchive *archive;
	struct PTCReader *reader;
	
	if (!dupcache)
		return false;
	
	dupcache->result = CACHE_READ_SAMPLE_INVALID;
	
	if (!dupgroup || !cachelib)
		return false;
	if (!(cachelib->eval_mode & eval_mode))
		return false;
	
	archive = find_active_cache(scene, cachelib);
	if (!archive)
		return false;
	
	// TODO duplicache reader should only overwrite data that is not sequentially generated by modifiers (simulations) ...
	reader = PTC_reader_duplicache(dupgroup->id.name, dupgroup, dupcache);
	PTC_reader_init(reader, archive);
	
	dupcache->result = BKE_cache_read_result(PTC_read_sample(reader, frame));
	
	PTC_reader_free(reader);
	PTC_close_reader_archive(archive);
	
	return (dupcache->result != CACHE_READ_SAMPLE_INVALID);
}

bool BKE_cache_read_dupli_object(CacheLibrary *cachelib, DupliObjectData *data,
                                 Scene *scene, Object *ob, float frame, eCacheLibrary_EvalMode eval_mode)
{
	struct PTCReaderArchive *archive;
	struct PTCReader *reader;
	/*eCacheReadSampleResult result;*/ /* unused */
	
	if (!data || !ob || !cachelib)
		return false;
	if (!(cachelib->eval_mode & eval_mode))
		return false;
	
	archive = find_active_cache(scene, cachelib);
	if (!archive)
		return false;
	
	PTC_reader_archive_use_render(archive, eval_mode == CACHE_LIBRARY_EVAL_RENDER);
	
	reader = PTC_reader_duplicache_object(ob->id.name, ob, data);
	PTC_reader_init(reader, archive);
	
	/*result = */BKE_cache_read_result(PTC_read_sample(reader, frame));
	
	PTC_reader_free(reader);
	PTC_close_reader_archive(archive);
	
	return true;
}


void BKE_cache_library_dag_recalc_tag(EvaluationContext *eval_ctx, Main *bmain)
{
	UNUSED_VARS(eval_ctx, bmain);
#if 0
	eCacheLibrary_EvalMode eval_mode = (eval_ctx->mode == DAG_EVAL_RENDER) ? CACHE_LIBRARY_EVAL_RENDER : CACHE_LIBRARY_EVAL_REALTIME;
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (cachelib->flag & CACHE_LIBRARY_READ) {
			CacheItem *item;
			
			// TODO tag group instance objects or so?
			
			for (item = cachelib->items.first; item; item = item->next) {
				if (item->ob && (item->flag & CACHE_ITEM_ENABLED)) {
					
					switch (item->type) {
						case CACHE_TYPE_OBJECT:
							DAG_id_tag_update(&item->ob->id, OB_RECALC_OB | OB_RECALC_TIME);
							break;
						case CACHE_TYPE_DERIVED_MESH:
						case CACHE_TYPE_PARTICLES:
						case CACHE_TYPE_HAIR:
						case CACHE_TYPE_HAIR_PATHS:
							DAG_id_tag_update(&item->ob->id, OB_RECALC_DATA | OB_RECALC_TIME);
							break;
					}
				}
			}
		}
	}
#endif
}

/* ========================================================================= */

CacheModifierTypeInfo cache_modifier_types[NUM_CACHE_MODIFIER_TYPES];

static CacheModifierTypeInfo *cache_modifier_type_get(eCacheModifier_Type type)
{
	return &cache_modifier_types[type];
}

static void cache_modifier_type_set(eCacheModifier_Type type, CacheModifierTypeInfo *mti)
{
	memcpy(&cache_modifier_types[type], mti, sizeof(CacheModifierTypeInfo));
}

const char *BKE_cache_modifier_type_name(eCacheModifier_Type type)
{
	return cache_modifier_type_get(type)->name;
}

const char *BKE_cache_modifier_type_struct_name(eCacheModifier_Type type)
{
	return cache_modifier_type_get(type)->struct_name;
}

int BKE_cache_modifier_type_struct_size(eCacheModifier_Type type)
{
	return cache_modifier_type_get(type)->struct_size;
}

/* ------------------------------------------------------------------------- */

bool BKE_cache_modifier_unique_name(ListBase *modifiers, CacheModifier *md)
{
	if (modifiers && md) {
		CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);

		return BLI_uniquename(modifiers, md, DATA_(mti->name), '.', offsetof(CacheModifier, name), sizeof(md->name));
	}
	return false;
}

CacheModifier *BKE_cache_modifier_add(CacheLibrary *cachelib, const char *name, eCacheModifier_Type type)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(type);
	
	CacheModifier *md = MEM_callocN(mti->struct_size, "cache modifier");
	md->type = type;
	
	if (!name)
		name = mti->name;
	BLI_strncpy_utf8(md->name, name, sizeof(md->name));
	/* make sure modifier has unique name */
	BKE_cache_modifier_unique_name(&cachelib->modifiers, md);
	
	if (mti->init)
		mti->init(md);
	
	BLI_addtail(&cachelib->modifiers, md);
	
	return md;
}

void BKE_cache_modifier_remove(CacheLibrary *cachelib, CacheModifier *md)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
	
	BLI_remlink(&cachelib->modifiers, md);
	
	if (mti->free)
		mti->free(md);
	
	MEM_freeN(md);
}

void BKE_cache_modifier_clear(CacheLibrary *cachelib)
{
	CacheModifier *md, *md_next;
	for (md = cachelib->modifiers.first; md; md = md_next) {
		CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
		md_next = md->next;
		
		if (mti->free)
			mti->free(md);
		
		MEM_freeN(md);
	}
	
	BLI_listbase_clear(&cachelib->modifiers);
}

CacheModifier *BKE_cache_modifier_copy(CacheLibrary *cachelib, CacheModifier *md)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
	
	CacheModifier *tmd = MEM_dupallocN(md);
	
	if (mti->copy)
		mti->copy(md, tmd);
	
	BLI_addtail(&cachelib->modifiers, md);
	
	return tmd;
}

void BKE_cache_modifier_foreachIDLink(struct CacheLibrary *cachelib, struct CacheModifier *md, CacheModifier_IDWalkFunc walk, void *userdata)
{
	CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
	
	if (mti->foreachIDLink)
		mti->foreachIDLink(md, cachelib, walk, userdata);
}

void BKE_cache_process_dupli_cache(CacheLibrary *cachelib, CacheProcessData *data,
                                   Scene *scene, Group *dupgroup, float frame_prev, float frame, eCacheLibrary_EvalMode UNUSED(eval_mode))
{
	CacheProcessContext ctx;
	CacheModifier *md;
	
	ctx.bmain = G.main;
	ctx.scene = scene;
	ctx.cachelib = cachelib;
	ctx.group = dupgroup;
	
	for (md = cachelib->modifiers.first; md; md = md->next) {
		CacheModifierTypeInfo *mti = cache_modifier_type_get(md->type);
		
		if (mti->process)
			mti->process(md, &ctx, data, frame, frame_prev);
	}
}

/* ------------------------------------------------------------------------- */

static void hairsim_params_init(StrandSimParams *params)
{
	params->timescale = 1.0f;
	params->substeps = 5;
}

static void hairsim_init(HairSimCacheModifier *hsmd)
{
	hairsim_params_init(&hsmd->sim_params);
}

static void hairsim_copy(HairSimCacheModifier *UNUSED(md), HairSimCacheModifier *UNUSED(tmd))
{
}

static void hairsim_process(HairSimCacheModifier *hsmd, CacheProcessContext *ctx, CacheProcessData *data, int frame, int frame_prev)
{
	struct DupliCacheIterator *iter = BKE_dupli_cache_iter_new(data->dupcache);
	for (; BKE_dupli_cache_iter_valid(iter); BKE_dupli_cache_iter_next(iter)) {
		DupliObjectData *data = BKE_dupli_cache_iter_get(iter);
		DupliObjectDataStrands *link;
		
		for (link = data->strands.first; link; link = link->next) {
			Strands *strands = link->strands;
			
			struct Implicit_Data *solver_data;
			int numsprings;
			
			BKE_strands_add_motion_state(strands);
			
			numsprings = strands->totverts - strands->totcurves;
			solver_data = BPH_mass_spring_solver_create(strands->totverts, numsprings);
			
			BPH_strands_solve(strands, solver_data, &hsmd->sim_params, (float)frame, (float)frame_prev, ctx->scene, NULL);
			
			BPH_mass_spring_solver_free(solver_data);
		}
	}
	BKE_dupli_cache_iter_free(iter);
}

CacheModifierTypeInfo cacheModifierType_HairSimulation = {
    /* name */              "HairSimulation",
    /* structName */        "HairSimCacheModifier",
    /* structSize */        sizeof(HairSimCacheModifier),

    /* copy */              (CacheModifier_CopyFunc)hairsim_copy,
    /* foreachIDLink */     (CacheModifier_ForeachIDLinkFunc)NULL,
    /* process */           (CacheModifier_ProcessFunc)hairsim_process,
    /* init */              (CacheModifier_InitFunc)hairsim_init,
    /* free */              (CacheModifier_FreeFunc)NULL,
};

void BKE_cache_modifier_init(void)
{
	cache_modifier_type_set(eCacheModifierType_HairSimulation, &cacheModifierType_HairSimulation);
}
