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

#ifndef PTC_EXPORT_H
#define PTC_EXPORT_H

#include "thread.h"

struct Main;
struct Scene;
struct EvaluationContext;
struct ListBase;

namespace PTC {

class Writer;

class Exporter
{
public:
	Exporter(Main *bmain, Scene *scene, EvaluationContext *evalctx, short *stop, short *do_update, float *progress);
	
	void bake(ListBase *writers, int start_frame, int end_frame);

	bool stop() const;

	void set_progress(float progress);

private:
	thread_mutex m_mutex;
	
	Main *m_bmain;
	Scene *m_scene;
	EvaluationContext *m_evalctx;
	
	short *m_stop;
	short *m_do_update;
	float *m_progress;
};

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */
