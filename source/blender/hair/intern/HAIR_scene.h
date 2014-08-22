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

#ifndef __HAIR_SCENE_H__
#define __HAIR_SCENE_H__

struct Scene;
struct Object;
struct DerivedMesh;
struct HairSystem;
struct ParticleSystem;

struct rbDynamicsWorld;

HAIR_NAMESPACE_BEGIN

struct SolverData;
struct SolverForces;

struct SceneConverter {
	static SolverData *build_solver_data(Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time);
	static SolverData *build_solver_data(Scene *scene, Object *ob, DerivedMesh *dm, ParticleSystem *psys, float time);
	static void update_solver_data_externals(SolverData *data, SolverForces &force, Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time);
	static void update_solver_data_externals(SolverData *data, SolverForces &force, Scene *scene, Object *ob, DerivedMesh *dm, ParticleSystem *hsys, float time);
	
	static void apply_solver_data(SolverData *data, Scene *scene, Object *ob, HairSystem *hsys);
	static void apply_solver_data(SolverData *data, ParticleSystem *psys, float (*vertCoords)[3]);
	
	static void sync_rigidbody_data(SolverData *data, const HairParams &params);
};

HAIR_NAMESPACE_END

#endif
