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

/** \file blender/blenkernel/intern/volume.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_volume.h"

#include "RNA_access.h"

#ifdef WITH_OPENVDB
#include "openvdb_capi.h"
#endif

typedef struct MeshVertexPoints {
	OpenVDBExportPoints base;
	
	MVert *verts;
	int numverts;
	
	/* optional */
	float *attr_float;
	float (*attr_vector)[3];
	int *attr_int;
} MeshVertexPoints;

static int MeshVertexPoints_size(OpenVDBExportPoints *_points)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	return points->numverts;
}

static void MeshVertexPoints_get_location(OpenVDBExportPoints *_points, int index, float *loc)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	MVert *v = &points->verts[index];
	copy_v3_v3(loc, v->co);
}

static void MeshVertexPoints_get_radius(OpenVDBExportPoints *_points, int index, float *rad)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	MVert *v = &points->verts[index];
	*rad = (float)v->bweight / 255.0f;
}

static void MeshVertexPoints_get_velocity(OpenVDBExportPoints *_points, int index, float *vel)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	MVert *v = &points->verts[index];
	normal_short_to_float_v3(vel, v->no);
}

static void MeshVertexPoints_get_attr_float(OpenVDBExportPoints *_points, int index, float *value)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	if (points->attr_float) {
		*value = points->attr_float[index];
	}
	else {
		*value = 0.0f;
	}
}

static void MeshVertexPoints_get_attr_vector(OpenVDBExportPoints *_points, int index, float *value)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	if (points->attr_vector) {
		copy_v3_v3(value, points->attr_vector[index]);
	}
	else {
		zero_v3(value);
	}
}

static void MeshVertexPoints_get_attr_int(OpenVDBExportPoints *_points, int index, int *value)
{
	MeshVertexPoints *points = (MeshVertexPoints *)_points;
	if (points->attr_int) {
		*value = points->attr_int[index];
	}
	else {
		*value = 0;
	}
}

static void make_MeshVertexPoints(MeshVertexPoints *points, MVert *verts, int numverts,
                                  float *attr_float, float (*attr_vector)[3], int *attr_int)
{
	points->base.size = MeshVertexPoints_size;
	points->base.get_location = MeshVertexPoints_get_location;
	points->base.get_radius = MeshVertexPoints_get_radius;
	points->base.get_velocity = MeshVertexPoints_get_velocity;
	points->base.get_attr_float = MeshVertexPoints_get_attr_float;
	points->base.get_attr_vector = MeshVertexPoints_get_attr_vector;
	points->base.get_attr_int = MeshVertexPoints_get_attr_int;
	
	points->verts = verts;
	points->numverts = numverts;
	points->attr_float = attr_float;
	points->attr_vector = attr_vector;
	points->attr_int = attr_int;
}

/* ------------------------------------------------------------------------- */

typedef struct ParticlePoints {
	OpenVDBExportPoints base;
	
	ParticleSystem *psys;
	/* inverse matrix and scale for converting from world space to object space */
	float imat[4][4];
	float iscale;
} ParticlePoints;

static int ParticlePoints_size(OpenVDBExportPoints *_points)
{
	ParticlePoints *points = (ParticlePoints *)_points;
	return points->psys->totpart;
}

static void ParticlePoints_get_location(OpenVDBExportPoints *_points, int index, float *loc)
{
	ParticlePoints *points = (ParticlePoints *)_points;
	ParticleData *pa = &points->psys->particles[index];
	/* note: particles are in world space, we want object space here */
	mul_v3_m4v3(loc, points->imat, pa->state.co);
}

static void ParticlePoints_get_radius(OpenVDBExportPoints *_points, int index, float *rad)
{
	ParticlePoints *points = (ParticlePoints *)_points;
	ParticleData *pa = &points->psys->particles[index];
	/* note: particles are in world space, we want object space here */
	*rad = pa->size * points->iscale;
}

static void ParticlePoints_get_velocity(OpenVDBExportPoints *_points, int index, float *vel)
{
	ParticlePoints *points = (ParticlePoints *)_points;
	ParticleData *pa = &points->psys->particles[index];
	/* note: particles are in world space, we want object space here */
	mul_v3_mat3_m4v3(vel, points->imat, pa->state.vel);
}

static void ParticlePoints_get_attr_float(OpenVDBExportPoints *_points, int index, float *value)
{
	ParticlePoints *points = (ParticlePoints *)_points;
	ParticleData *pa = &points->psys->particles[index];
	/* TODO */
	*value = pa->size;
}

static void ParticlePoints_get_attr_vector(OpenVDBExportPoints *_points, int index, float *value)
{
	ParticlePoints *points = (ParticlePoints *)_points;
//	ParticleData *pa = &points->psys->particles[index];
	/* TODO */
	zero_v3(value);
}

static void ParticlePoints_get_attr_int(OpenVDBExportPoints *_points, int index, int *value)
{
	ParticlePoints *points = (ParticlePoints *)_points;
//	ParticleData *pa = &points->psys->particles[index];
	/* TODO */
	*value = 0;
}

static void make_ParticlePoints(ParticlePoints *points, ParticleSystem *psys, float obmat[4][4])
{
	float scale;
	
	points->base.size = ParticlePoints_size;
	points->base.get_location = ParticlePoints_get_location;
	points->base.get_radius = ParticlePoints_get_radius;
	points->base.get_velocity = ParticlePoints_get_velocity;
	points->base.get_attr_float = ParticlePoints_get_attr_float;
	points->base.get_attr_vector = ParticlePoints_get_attr_vector;
	points->base.get_attr_int = ParticlePoints_get_attr_int;
	
	points->psys = psys;
	invert_m4_m4(points->imat, obmat);
	scale = mat4_to_scale(obmat);
	points->iscale = (scale != 0.0f) ? 1.0f/scale : 0.0f;
}

/* ------------------------------------------------------------------------- */

void BKE_volume_export(Scene *scene, Object *ob, PointerRNA *config,
                       int frame_start, int frame_end, const char *filepath)
{
#ifdef WITH_OPENVDB
	char filename[FILE_MAX];
	float voxel_size;
	float gridmat[4][4];
	CustomDataMask dm_mask = 0;
	DerivedMesh *dm = NULL;
	
	BLI_strncpy(filename, filepath, FILE_MAX);
	BLI_path_abs(filename, G.main->name);
	
	struct OpenVDBWriter *writer = OpenVDBWriter_create();
	
	voxel_size = RNA_float_get(config, "voxel_size");
	copy_m4_m4(gridmat, ob->obmat);
	scale_m4_fl(gridmat, voxel_size);
	
	RNA_BEGIN (config, itemptr, "grids")
	{
		int data_source;
		
		data_source = RNA_enum_get(&itemptr, "data_source");
		
		switch (data_source) {
			case VOLUME_EXPORT_DATA_VERTICES:
				dm_mask |= CD_MASK_BAREMESH;
				break;
		}
	}
	RNA_END;
	
	if (dm_mask != 0) {
		dm = mesh_get_derived_final(scene, ob, dm_mask);
	}
	
	RNA_BEGIN (config, itemptr, "grids")
	{
		char *name, namebuf[MAX_NAME];
		int data_source;
		int psys_index;
		
		name = RNA_string_get_alloc(&itemptr, "name", namebuf, MAX_NAME);
		data_source = RNA_enum_get(&itemptr, "data_source");
		psys_index = 0; /* TODO */
		
		switch (data_source) {
			case VOLUME_EXPORT_DATA_VERTICES:
				if (dm) {
					MeshVertexPoints points;
					make_MeshVertexPoints(&points, dm->getVertArray(dm), dm->getNumVerts(dm),
					                      NULL, NULL, NULL);
					OpenVDB_export_points_fl(writer, name, gridmat, NULL, &points.base, voxel_size);
				}
				break;
			case VOLUME_EXPORT_DATA_PARTICLES: {
				ParticleSystem *psys = BLI_findlink(&ob->particlesystem, psys_index);
				if (psys) {
					ParticlePoints points;
					make_ParticlePoints(&points, psys, ob->obmat);
					OpenVDB_export_points_fl(writer, name, gridmat, NULL, &points.base, voxel_size);
				}
				break;
			}
		}
		
		if (name != namebuf)
			MEM_freeN(name);
	}
	RNA_END;
	
	if (dm)
		dm->release(dm);
	
	OpenVDBWriter_write(writer, filename);
	
	OpenVDBWriter_free(writer);
#else
	UNUSED_VARS(scene, ob, config, frame_start, frame_end, filepath);
#endif
}

void BKE_volume_force_link(void)
{
}
