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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_volume.h
 *  \ingroup bke
 */


#ifndef __BKE_VOLUME_H__
#define __BKE_VOLUME_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct PointerRNA;
struct Scene;

typedef enum eVolumeExport_DataSource {
	VOLUME_EXPORT_DATA_VERTICES,
	VOLUME_EXPORT_DATA_PARTICLES,
} eVolumeExport_DataSource;

void BKE_volume_export(struct Scene *scene, struct Object *ob, struct PointerRNA *config,
                       int frame_start, int frame_end, const char *filepath);

void BKE_volume_force_link(void);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_VOLUME_H__ */
