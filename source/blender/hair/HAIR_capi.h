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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Object;
struct DerivedMesh;
struct HairCurve;
struct HairSystem;
struct rbDynamicsWorld;

struct HAIR_Solver;
struct HAIR_SmoothingIteratorFloat3;
struct HAIR_FrameIterator;

struct HAIR_Solver *HAIR_solver_new(void);
void HAIR_solver_free(struct HAIR_Solver *solver);
void HAIR_solver_set_params(struct HAIR_Solver *solver, const struct HairParams *params);
void HAIR_solver_build_data(struct HAIR_Solver *solver, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm, struct HairSystem *hsys, float time);
void HAIR_solver_update_externals(struct HAIR_Solver *solver, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm, struct HairSystem *hsys, float time);
void HAIR_solver_rebuild_rigidbodyworld(struct HAIR_Solver *solver, struct rbDynamicsWorld *world);

typedef struct HAIR_SolverContact {
	float coA[3], coB[3];
} HAIR_SolverContact;

void HAIR_solver_step(struct HAIR_Solver *solver, float time, float timestep);
void HAIR_solver_step_debug(struct HAIR_Solver *csolver, float time, float timestep, struct HAIR_SolverContact **contacts, int *totcontacts);

void HAIR_solver_apply(struct HAIR_Solver *solver, struct Scene *scene, struct Object *ob, struct HairSystem *hsys);

struct HAIR_SmoothingIteratorFloat3 *HAIR_smoothing_iter_new(struct HairCurve *curve, float rest_length, float amount, float cval[3]);
void HAIR_smoothing_iter_free(struct HAIR_SmoothingIteratorFloat3 *iter);
bool HAIR_smoothing_iter_valid(struct HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *iter);
void HAIR_smoothing_iter_next(struct HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *iter, float val[3]);
void HAIR_smoothing_iter_end(struct HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *citer, float cval[3]);

struct HAIR_FrameIterator *HAIR_frame_iter_new(struct HairCurve *curve, float rest_length, float amount, float nor[3], float tan[3], float cotan[3]);
void HAIR_frame_iter_free(struct HAIR_FrameIterator *iter);
bool HAIR_frame_iter_valid(struct HairCurve *curve, struct HAIR_FrameIterator *iter);
void HAIR_frame_iter_next(struct HairCurve *curve, struct HAIR_FrameIterator *iter, float nor[3], float tan[3], float cotan[3]);

#ifdef __cplusplus
}
#endif
