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

#include <vector>

extern "C" {
#include "RBI_api.h"
}

#include "HAIR_debug.h"
#include "HAIR_math.h"
#include "HAIR_solver.h"

HAIR_NAMESPACE_BEGIN

PointContactInfo::PointContactInfo(const btManifoldPoint &bt_point, const btCollisionObject *ob0, const btCollisionObject *ob1, int point_index) :
    point_index(point_index),
    local_point_body(bt_point.m_localPointB.m_floats),
    local_point_hair(bt_point.m_localPointA),
    world_point_body(bt_point.m_positionWorldOnB),
    world_point_hair(bt_point.m_positionWorldOnA),
    world_normal_body(bt_point.m_normalWorldOnB),
    distance(bt_point.m_distance1)
{
	float3 lin_vel(ob1->getInterpolationLinearVelocity().m_floats);
	float3 ang_vel(ob1->getInterpolationAngularVelocity().m_floats);
	float3 p((ob1->getWorldTransform() * bt_point.m_localPointB).m_floats);
	
	world_vel_body = dot_v3v3(lin_vel + cross_v3_v3(ang_vel, p), world_normal_body) * world_normal_body;
	
	/* note: combined friction and restitution in the manifold point are not usable,
	 * have to calculate it manually here
	 */
	friction = ob0->getFriction() * ob1->getFriction();
	restitution = ob0->getRestitution() * ob1->getRestitution();
}

struct HairContactResultCallback : btCollisionWorld::ContactResultCallback {
	HairContactResultCallback(const HairParams &params, PointContactCache &cache) :
	    cache(&cache),
	    point_index(0),
	    margin(params.margin)
	{
	}

	btScalar addSingleResult(btManifoldPoint &cp,
	                         const btCollisionObjectWrapper *colObj0Wrap, int partId0, int index0,
	                         const btCollisionObjectWrapper *colObj1Wrap, int partId1, int index1)
	{
		if (cp.getDistance() < margin) {
			const btCollisionObject *ob0 = colObj0Wrap->getCollisionObject();
			const btCollisionObject *ob1 = colObj1Wrap->getCollisionObject();
			
			PointContactInfo info(cp, ob0, ob1, point_index);
			cache->push_back(info);
			
//			const btVector3 &ptA = cp.getPositionWorldOnA();
//			const btVector3 &ptB = cp.getPositionWorldOnB();
//			Debug::collision_contact(debug_data, float3(ptA.x(), ptA.y(), ptA.z()), float3(ptB.x(), ptB.y(), ptB.z()));
		}
		
		/* note: return value is unused
		 * http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?p=20990#p20990
		 */
		return 0.0f;
	}
	
	PointContactCache *cache;
	int point_index;
	float margin;
};

void Solver::cache_point_contacts(PointContactCache &cache) const
{
	btDynamicsWorld *dworld = m_forces.dynamics_world->dynamicsWorld;
	btPairCachingGhostObject *ghost = &m_data->rb_ghost.ghost;
	
//	btManifoldArray manifold_array;
	const btBroadphasePairArray& pairs = ghost->getOverlappingPairCache()->getOverlappingPairArray();
	int num_pairs = pairs.size();
	
	cache.reserve(num_pairs);
	
	for (int i = 0; i < num_pairs; i++) {
//		manifold_array.clear();
		
		const btBroadphasePair& pair = pairs[i];
		
		/* unless we manually perform collision detection on this pair, the contacts are in the dynamics world paircache */
		btBroadphasePair* collision_pair = dworld->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
		if (!collision_pair)
			continue;
		
		btCollisionObject *ob0 = (btCollisionObject *)pair.m_pProxy0->m_clientObject;
		btCollisionObject *ob1 = (btCollisionObject *)pair.m_pProxy1->m_clientObject;
		btCollisionObject *other = ob0 == ghost ? ob1 : ob0;
		
		HairContactResultCallback cb(m_params, cache);
		
		Point *point = m_data->points;
		int totpoints = m_data->totpoints;
		for (int k = 0; k < totpoints; ++k, ++point) {
			cb.point_index = k;
			dworld->contactPairTest(&point->rb_ghost.ghost, other, cb);
		}
	}
}

#if 0
struct HairContactResultCallback : btCollisionWorld::ContactResultCallback {
	btScalar addSingleResult(btManifoldPoint &cp,
	                         const btCollisionObjectWrapper *colObj0Wrap, int partId0, int index0,
	                         const btCollisionObjectWrapper *colObj1Wrap, int partId1, int index1)
	{
		if (cp.getDistance() < 0.f) {
			const btVector3 &ptA = cp.getPositionWorldOnA();
			const btVector3 &ptB = cp.getPositionWorldOnB();
//			const btVector3 &normalOnB = cp.m_normalWorldOnB;
			
			Debug::collision_contact(float3(ptA.x(), ptA.y(), ptA.z()), float3(ptB.x(), ptB.y(), ptB.z()));
		}
		
		/* note: return value is unused
		 * http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?p=20990#p20990
		 */
		return 0.0f;
	}
};

static void debug_ghost_contacts(SolverData *data, btDynamicsWorld *dworld, rbGhostObject *object)
{
	btPairCachingGhostObject *ghost = &object->ghost;
	
	btManifoldArray manifold_array;
	const btBroadphasePairArray& pairs = ghost->getOverlappingPairCache()->getOverlappingPairArray();
	int num_pairs = pairs.size();
	
	for (int i = 0; i < num_pairs; i++) {
		manifold_array.clear();
		
		const btBroadphasePair& pair = pairs[i];
		
		/* unless we manually perform collision detection on this pair, the contacts are in the dynamics world paircache */
		btBroadphasePair* collision_pair = dworld->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
		if (!collision_pair)
			continue;
		
		btCollisionObject *ob0 = (btCollisionObject *)pair.m_pProxy0->m_clientObject;
		btCollisionObject *ob1 = (btCollisionObject *)pair.m_pProxy1->m_clientObject;
		btCollisionObject *other = ob0 == ghost ? ob1 : ob0;
		
		HairContactResultCallback cb;
		
		Curve *curve = data->curves;
		for (int i = 0; i < data->totcurves; ++i, ++curve) {
			Point *pt = curve->points;
			for (int k = 0; k < curve->totpoints; ++k, ++pt) {
				dworld->contactPairTest(&pt->rb_ghost.ghost, other, cb);
			}
		}
		
#if 0
		if (collision_pair->m_algorithm)
			collision_pair->m_algorithm->getAllContactManifolds(manifold_array);
		
		for (int j = 0; j < manifold_array.size(); j++) {
			btPersistentManifold* manifold = manifold_array[j];
			btScalar direction_sign = manifold->getBody0() == ghost ? btScalar(-1.0) : btScalar(1.0);
			for (int p = 0; p < manifold->getNumContacts(); p++) {
				const btManifoldPoint &pt = manifold->getContactPoint(p);
				if (pt.getDistance() < 0.f) {
					const btVector3 &ptA = pt.getPositionWorldOnA();
					const btVector3 &ptB = pt.getPositionWorldOnB();
					const btVector3 &normalOnB = pt.m_normalWorldOnB;
					
					Debug::collision_contact(float3(ptA.x(), ptA.y(), ptA.z()), float3(ptB.x(), ptB.y(), ptB.z()));
				}
			}
		}
#endif
	}
}

void SolverData::debug_contacts(rbDynamicsWorld *world)
{
	btDynamicsWorld *dworld = world->dynamicsWorld;
	
	debug_ghost_contacts(this, dworld, &rb_ghost);

#if 0 /* collision shape / bounding box used for main collision detection */
	if (rb_ghost.ghost.getBroadphaseHandle()) {
		float3 c[8];
		c[0] = float3((float *)rb_ghost.ghost.getBroadphaseHandle()->m_aabbMin.m_floats);
		c[7] = float3((float *)rb_ghost.ghost.getBroadphaseHandle()->m_aabbMax.m_floats);
		c[1] = float3(c[7].x, c[0].y, c[0].z);
		c[2] = float3(c[0].x, c[7].y, c[0].z);
		c[3] = float3(c[7].x, c[7].y, c[0].z);
		c[4] = float3(c[0].x, c[0].y, c[7].z);
		c[5] = float3(c[7].x, c[0].y, c[7].z);
		c[6] = float3(c[0].x, c[7].y, c[7].z);
		Debug::collision_contact(c[0], c[1]);
		Debug::collision_contact(c[1], c[3]);
		Debug::collision_contact(c[3], c[2]);
		Debug::collision_contact(c[2], c[0]);
		
		Debug::collision_contact(c[0], c[4]);
		Debug::collision_contact(c[1], c[5]);
		Debug::collision_contact(c[2], c[6]);
		Debug::collision_contact(c[3], c[7]);
		
		Debug::collision_contact(c[4], c[5]);
		Debug::collision_contact(c[5], c[7]);
		Debug::collision_contact(c[7], c[6]);
		Debug::collision_contact(c[6], c[4]);
	}
#endif
}
#endif

HAIR_NAMESPACE_END
