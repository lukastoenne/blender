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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_MESH_SAMPLE_H__
#define __BKE_MESH_SAMPLE_H__

/** \file BKE_mesh_sample.h
 *  \ingroup bke
 */

struct DerivedMesh;
struct Key;
struct KeyBlock;
struct MFace;
struct MVert;

struct MSurfaceSample;
struct MSurfaceSampleGenerator;

typedef struct MSurfaceSampleGenerator MSurfaceSampleGenerator;
typedef float (*MeshSampleVertexWeightFp)(struct DerivedMesh *dm, struct MVert *vert, unsigned int index, void *userdata);
typedef bool (*MeshSampleRayFp)(void *userdata, float ray_start[3], float ray_end[3]);

/* ==== Evaluate ==== */

bool BKE_mesh_sample_eval(struct DerivedMesh *dm, const struct MSurfaceSample *sample, float loc[3], float nor[3], float tang[3]);
bool BKE_mesh_sample_shapekey(struct Key *key, struct KeyBlock *kb, const struct MSurfaceSample *sample, float loc[3]);


/* ==== Sampling ==== */

/* face_weights is optional */
struct MSurfaceSampleGenerator *BKE_mesh_sample_create_generator_random(struct DerivedMesh *dm, unsigned int seed);
struct MSurfaceSampleGenerator *BKE_mesh_sample_create_generator_random_ex(struct DerivedMesh *dm, unsigned int seed,
                                                                           MeshSampleVertexWeightFp vertex_weight_cb, void *userdata, bool use_facearea);

struct MSurfaceSampleGenerator *BKE_mesh_sample_create_generator_raycast(struct DerivedMesh *dm, MeshSampleRayFp ray_cb, void *userdata);
void BKE_mesh_sample_free_generator(struct MSurfaceSampleGenerator *gen);
bool BKE_mesh_sample_generate(struct MSurfaceSampleGenerator *gen, struct MSurfaceSample *sample);


/* ==== Utilities ==== */

struct ParticleSystem;
struct ParticleData;
struct BVHTreeFromMesh;

bool BKE_mesh_sample_from_particle(struct MSurfaceSample *sample, struct ParticleSystem *psys, struct DerivedMesh *dm, struct ParticleData *pa);
bool BKE_mesh_sample_to_particle(struct MSurfaceSample *sample, struct ParticleSystem *psys, struct DerivedMesh *dm, struct BVHTreeFromMesh *bvhtree, struct ParticleData *pa);

#endif  /* __BKE_MESH_SAMPLE_H__ */
