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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BPH_MASS_SPRING_H__
#define __BPH_MASS_SPRING_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Implicit_Data;
struct ClothModifierData;
struct DerivedMesh;
struct EffectorWeights;
struct HairSolverData;
struct HairSystem;
struct HairParams;
struct Object;
struct Scene;
struct SimDebugData;

typedef enum eMassSpringSolverStatus {
	BPH_SOLVER_SUCCESS              = (1 << 0),
	BPH_SOLVER_NUMERICAL_ISSUE      = (1 << 1),
	BPH_SOLVER_NO_CONVERGENCE       = (1 << 2),
	BPH_SOLVER_INVALID_INPUT        = (1 << 3),
} eMassSpringSolverStatus;

struct Implicit_Data *BPH_mass_spring_solver_create(int numverts, int numsprings);
void BPH_mass_spring_solver_free(struct Implicit_Data *id);
int BPH_mass_spring_solver_numvert(struct Implicit_Data *id);

int BPH_cloth_solver_init(struct Object *ob, struct ClothModifierData *clmd);
void BPH_cloth_solver_free(struct ClothModifierData *clmd);
int BPH_cloth_solve(struct Object *ob, float frame, struct ClothModifierData *clmd, struct ListBase *effectors);
void BPH_cloth_solver_set_positions(struct ClothModifierData *clmd);

struct HairSolverData *BPH_hair_solver_create(struct Object *ob, struct HairSystem *hsys);
void BPH_hair_solver_free(struct HairSolverData *data);

void BPH_hair_solver_set_externals(struct HairSolverData *data, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm, struct EffectorWeights *effector_weights);
void BPH_hair_solver_clear_externals(struct HairSolverData *data);

void BPH_hair_solver_set_positions(struct HairSolverData *data, struct Object *ob, struct HairSystem *hsys);
void BPH_hair_solve(struct HairSolverData *data, struct Object *ob, struct HairSystem *hsys, float time, float timestep, struct SimDebugData *debug_data);
void BPH_hair_solver_apply_positions(struct HairSolverData *data, struct Scene *scene, struct Object *ob, struct HairSystem *hsys);

bool implicit_hair_volume_get_texture_data(struct Object *UNUSED(ob), struct ClothModifierData *clmd, struct ListBase *UNUSED(effectors), struct VoxelData *vd);

#ifdef __cplusplus
}
#endif

#endif
