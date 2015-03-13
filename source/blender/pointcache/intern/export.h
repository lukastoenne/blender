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

struct Main;
struct Scene;
struct EvaluationContext;
struct Group;
struct Object;
struct DerivedMesh;
struct CacheModifierData;
struct ListBase;
struct PTCWriter;

namespace PTC {

class Writer;
class WriterArchive;

class Exporter
{
public:
	Exporter(Main *bmain, Scene *scene, EvaluationContext *evalctx, short *stop, short *do_update, float *progress);
	
#if 1
	void bake(PTCWriter *writer, int start_frame, int end_frame);
#else
	void bake(ListBase *writers, DerivedMesh **render_dm_ptr, int start_frame, int end_frame);
#endif
	
	bool stop() const;
	
	void set_progress(float progress);
	
protected:
	void set_bake_object(Object *ob, DerivedMesh **render_dm_ptr, CacheModifierData **cachemd_ptr);
	void release_bake_object(DerivedMesh **render_dm_ptr, CacheModifierData *cachemd);

private:
	Main *m_bmain;
	Scene *m_scene;
	EvaluationContext *m_evalctx;
	
	short *m_stop;
	short *m_do_update;
	float *m_progress;
};

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */
