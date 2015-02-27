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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/io/io_cache_library.c
 *  \ingroup editor/io
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLF_translation.h"

#include "BLI_blenlib.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_listBase.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_depsgraph.h"
#include "BKE_cache_library.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "io_cache_library.h"

#include "PTC_api.h"

/********************** new cache library operator *********************/

static int new_cachelib_exec(bContext *C, wmOperator *UNUSED(op))
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	
	/* add or copy material */
	if (cachelib) {
		cachelib = BKE_cache_library_copy(cachelib);
	}
	else {
		cachelib = BKE_cache_library_add(bmain, DATA_("CacheLibrary"));
	}
	
	/* enable fake user by default */
	cachelib->id.flag |= LIB_FAKEUSER;
	
	/* hook into UI */
	UI_context_active_but_prop_get_templateID(C, &ptr, &prop);
	
	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		* pointer se also increases user, so this compensates it */
		cachelib->id.us--;
		
		RNA_id_pointer_create(&cachelib->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	
	WM_event_add_notifier(C, NC_SCENE, cachelib);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Cache Library";
	ot->idname = "CACHELIBRARY_OT_new";
	ot->description = "Add a new cache library";
	
	/* api callbacks */
	ot->exec = new_cachelib_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/********************** delete cache library operator *********************/

static int cache_library_delete_poll(bContext *C)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	
	if (!cachelib)
		return false;
	
	return true;
}

static int cache_library_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	
	BKE_cache_library_unlink(cachelib);
	BKE_libblock_free(bmain, cachelib);
	
	WM_event_add_notifier(C, NC_SCENE, cachelib);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Cache Library";
	ot->idname = "CACHELIBRARY_OT_delete";
	ot->description = "Delete a cache library data block";
	
	/* api callbacks */
	ot->exec = cache_library_delete_exec;
	/* XXX confirm popup would be nicer, but problem is the popup layout
	 * does not inherit the cache_library context pointer, so poll fails ...
	 */
	/*ot->invoke = WM_operator_confirm;*/
	ot->poll = cache_library_delete_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/********************** enable cache item operator *********************/

static int cache_item_enable_poll(bContext *C)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	Object *obcache = CTX_data_pointer_get_type(C, "cache_object", &RNA_Object).data;
	
	if (!cachelib || !obcache)
		return false;
	
	return true;
}

static int cache_item_enable_exec(bContext *C, wmOperator *op)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	Object *obcache = CTX_data_pointer_get_type(C, "cache_object", &RNA_Object).data;
	int type = RNA_enum_get(op->ptr, "type");
	int index = RNA_int_get(op->ptr, "index");
	
	CacheItem *item = BKE_cache_library_add_item(cachelib, obcache, type, index);
	item->flag |= CACHE_ITEM_ENABLED;
	
	WM_event_add_notifier(C, NC_OBJECT, cachelib);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_item_enable(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Enable Cache Item";
	ot->idname = "CACHELIBRARY_OT_item_enable";
	ot->description = "Enable a cache item";
	
	/* api callbacks */
	ot->poll = cache_item_enable_poll;
	ot->exec = cache_item_enable_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	prop = RNA_def_enum(ot->srna, "type", cache_library_item_type_items, CACHE_TYPE_OBJECT, "Type", "Type of cache item to add");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	RNA_def_int(ot->srna, "index", -1, -1, INT_MAX, "Index", "Index of data in the object", -1, INT_MAX);
}

/********************** bake cache operator *********************/

static int cache_library_bake_poll(bContext *C)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	
	if (!cachelib)
		return false;
	
	return true;
}

typedef struct CacheLibraryBakeJob {
	short *stop, *do_update;
	float *progress;
	
	struct Main *bmain;
	struct Scene *scene;
	EvaluationContext eval_ctx;
	struct CacheLibrary *cachelib;
	
	struct PTCWriterArchive *archive;
	ListBase writers;
	
	int origfra;                            /* original frame to reset scene after export */
	float origframelen;                     /* original frame length to reset scene after export */
} CacheLibraryBakeJob;

static void cache_library_bake_freejob(void *customdata)
{
	CacheLibraryBakeJob *data= (CacheLibraryBakeJob *)customdata;
	MEM_freeN(data);
}

static void cache_library_bake_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
	CacheLibraryBakeJob *data= (CacheLibraryBakeJob *)customdata;
	Scene *scene = data->scene;
	int start_frame, end_frame;
	int required_mode;
	
	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;
	
	data->origfra = scene->r.cfra;
	data->origframelen = scene->r.framelen;
	scene->r.framelen = 1.0f;
	memset(&data->eval_ctx, 0, sizeof(EvaluationContext));
	data->eval_ctx.mode = DAG_EVAL_RENDER;
	
	G.is_break = false;
	
	/* XXX how can this be defined properly? */
	required_mode = eModifierMode_Render;
	
	/* disable reading for the duration of the bake process */
	data->cachelib->flag &= ~CACHE_LIBRARY_READ;
	
	data->archive = PTC_cachelib_writers(scene, required_mode, data->cachelib, &data->writers);
	
	/* XXX where to get this from? */
	start_frame = scene->r.sfra;
	end_frame = scene->r.efra;
	PTC_bake(data->bmain, scene, &data->eval_ctx, &data->writers, start_frame, end_frame, stop, do_update, progress);
	
	*do_update = true;
	*stop = 0;
}

static void cache_library_bake_endjob(void *customdata)
{
	CacheLibraryBakeJob *data = (CacheLibraryBakeJob *)customdata;
	Scene *scene = data->scene;
	
	G.is_rendering = false;
	BKE_spacedata_draw_locks(false);
	
	PTC_cachelib_writers_free(data->archive, &data->writers);
	
	/* enable reading */
	data->cachelib->flag |= CACHE_LIBRARY_READ;
	
	/* reset scene frame */
	scene->r.cfra = data->origfra;
	scene->r.framelen = data->origframelen;
	BKE_scene_update_for_newframe(&data->eval_ctx, data->bmain, scene, scene->lay);
}

static int cache_library_bake_exec(bContext *C, wmOperator *UNUSED(op))
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cache_library", &RNA_CacheLibrary).data;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	CacheLibraryBakeJob *data;
	wmJob *wm_job;
	
	/* XXX annoying hack: needed to prevent data corruption when changing
	 * scene frame in separate threads
	 */
	G.is_rendering = true;
	
	BKE_spacedata_draw_locks(true);
	
	/* XXX set WM_JOB_EXCL_RENDER to prevent conflicts with render jobs,
	 * since we need to set G.is_rendering
	 */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Cache Library Bake",
	                     WM_JOB_PROGRESS | WM_JOB_EXCL_RENDER, WM_JOB_TYPE_CACHELIBRARY_BAKE);
	
	/* setup job */
	data = MEM_callocN(sizeof(CacheLibraryBakeJob), "Cache Library Bake Job");
	data->bmain = bmain;
	data->scene = scene;
	data->cachelib = cachelib;
	
	WM_jobs_customdata_set(wm_job, data, cache_library_bake_freejob);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE|ND_FRAME, NC_SCENE|ND_FRAME);
	WM_jobs_callbacks(wm_job, cache_library_bake_startjob, NULL, NULL, cache_library_bake_endjob);
	
	WM_jobs_start(CTX_wm_manager(C), wm_job);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake";
	ot->description = "Bake cache library";
	ot->idname = "CACHELIBRARY_OT_bake";
	
	/* api callbacks */
	ot->exec = cache_library_bake_exec;
	ot->poll = cache_library_bake_poll;
	
	/* flags */
	/* no undo for this operator, cannot restore old cache files anyway */
	ot->flag = OPTYPE_REGISTER;
}
