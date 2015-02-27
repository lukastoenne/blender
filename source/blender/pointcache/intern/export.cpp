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
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
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

void Exporter::bake(ListBase *writers, int start_frame, int end_frame)
{
	set_progress(0.0f);

	for (int cfra = start_frame; cfra <= end_frame; ++cfra) {
		m_scene->r.cfra = cfra;
		BKE_scene_update_for_newframe(m_evalctx, m_bmain, m_scene, m_scene->lay);

		for (LinkData *link = (LinkData *)writers->first; link; link = link->next) {
			Writer *writer = (Writer *)link->data;
			if (writer)
				writer->write_sample();
		}

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
