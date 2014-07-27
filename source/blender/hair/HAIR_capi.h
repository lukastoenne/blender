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

struct HairCurve;
struct HairSystem;

struct HAIR_Solver;
struct HAIR_SmoothingIteratorFloat3;

struct HAIR_Solver *HAIR_solver_new(void);
void HAIR_solver_free(struct HAIR_Solver *solver);
void HAIR_solver_init(struct HAIR_Solver *solver, struct HairSystem *hsys);
void HAIR_solver_step(struct HAIR_Solver *solver, float timestep);
void HAIR_solver_apply(struct HAIR_Solver *solver, struct HairSystem *hsys);

struct HAIR_SmoothingIteratorFloat3 *HAIR_smoothing_iter_new(struct HairCurve *curve, float rest_length, float amount, float cval[3]);
void HAIR_smoothing_iter_free(struct HAIR_SmoothingIteratorFloat3 *iter);
bool HAIR_smoothing_iter_valid(struct HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *iter);
void HAIR_smoothing_iter_next(struct HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *iter, float val[3]);
void HAIR_smoothing_iter_end(struct HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *citer, float cval[3]);

#ifdef __cplusplus
}
#endif
