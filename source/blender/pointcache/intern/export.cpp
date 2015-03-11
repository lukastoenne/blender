/*
 * Copyright 2013, Blender Foundation.
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
 */

#include "export.h"
#include "writer.h"

extern "C" {
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cache_library.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
}

namespace PTC {

Exporter::Exporter(Main *bmain, Scene *scene, EvaluationContext *evalctx, short *stop, short *do_update, float *progress) :
    m_bmain(bmain),
    m_scene(scene),
    m_evalctx(evalctx),
    m_stop(stop),
    m_do_update(do_update),
    m_progress(progress)
{
}

void Exporter::set_bake_object(Object *ob, DerivedMesh **render_dm_ptr, CacheModifierData **cachemd_ptr)
{
	const bool use_render = (m_evalctx->mode == DAG_EVAL_RENDER);
	const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
	
	if (!ob)
		return;
	
	/* cache modifier can be used to store an intermediate DM stage */
//	CacheModifierData *cachemd = (CacheModifierData *)mesh_find_cache_modifier(m_scene, ob, required_mode);
	CacheModifierData *cachemd = NULL;
	*cachemd_ptr = cachemd;
	
	/* construct the correct DM based on evaluation mode */
	if (use_render && ob->type == OB_MESH && ob->data) {
		/* tell the modifier to store a copy of the DM for us */
		if (cachemd)
			cachemd->flag |= MOD_CACHE_USE_OUTPUT_RENDER;
		
		/* Evaluate object for render settings
		 * This is not done by BKE_scene_update_for_newframe!
		 * Instead we have to construct the render DM explicitly like a render engine would do.
		 */
		DerivedMesh *render_dm = mesh_create_derived_render(m_scene, ob, CD_MASK_MESH);
		*render_dm_ptr = render_dm;
	}
	else {
		*render_dm_ptr = NULL;
	}
}

void Exporter::release_bake_object(DerivedMesh **render_dm_ptr, CacheModifierData *cachemd)
{
	const bool use_render = (m_evalctx->mode == DAG_EVAL_RENDER);
	
	if (cachemd) {
		if (use_render) {
			cachemd->flag &= ~MOD_CACHE_USE_OUTPUT_RENDER;
			if (cachemd->output_dm) {
				cachemd->output_dm->release(cachemd->output_dm);
				cachemd->output_dm = NULL;
			}
		}
	}
	if (*render_dm_ptr) {
		(*render_dm_ptr)->release(*render_dm_ptr);
		*render_dm_ptr = NULL;
	}
}

void Exporter::bake(ListBase *writers, DerivedMesh **render_dm_ptr, int start_frame, int end_frame)
{
	set_progress(0.0f);
	
	for (int cfra = start_frame; cfra <= end_frame; ++cfra) {
		m_scene->r.cfra = cfra;
		BKE_scene_update_for_newframe(m_evalctx, m_bmain, m_scene, m_scene->lay);
		
		/* Writers have been sorted by their objects.
		 * This allows us to evaluate one object at a time in order to avoid
		 * reconstructing the same object for render settings multiple times.
		 */
		Object *curob = NULL;
		CacheModifierData *cachemd = NULL;
		for (CacheLibraryWriterLink *link = (CacheLibraryWriterLink *)writers->first; link; link = link->next) {
			Writer *writer = (Writer *)link->writer;
			Object *ob = link->ob;
			
			if (ob != curob) {
				release_bake_object(render_dm_ptr, cachemd);
				set_bake_object(ob, render_dm_ptr, &cachemd);
				curob = ob;
			}
			
			if (writer)
				writer->write_sample();
		}
		
		release_bake_object(render_dm_ptr, cachemd);
		
		set_progress((float)(cfra - start_frame + 1) / (float)(end_frame - start_frame + 1));
		
		if (stop())
			break;
	}
}

bool Exporter::stop() const
{
	if (*m_stop)
		return true;
	
	return (G.is_break);
}

void Exporter::set_progress(float progress)
{
	*m_do_update = 1;
	*m_progress = progress;
}

} /* namespace PTC */
