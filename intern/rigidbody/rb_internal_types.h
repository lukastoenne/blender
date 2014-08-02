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
 * The Original Code is Copyright (C) 2014 Blender Foundation,
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung, Sergej Reich, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RBI_types.h
 *  \ingroup RigidBody
 *  \brief Rigid Body datatypes for Physics Engines
 */

#ifndef __RB_TYPES_H__
#define __RB_TYPES_H__

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

struct rbRigidBody {
	rbRigidBody(const btRigidBody::btRigidBodyConstructionInfo& constructionInfo) :
	    body(constructionInfo),
	    col_groups(0),
	    flag(0)
	{}
	~rbRigidBody()
	{}
	
	btRigidBody body;
	int col_groups;
	int flag;
};

struct rbGhostObject {
	rbGhostObject() :
	    ghost(),
	    col_groups(0),
	    flag(0)
	{}
	~rbGhostObject()
	{}
	
	btGhostObject ghost;
	int col_groups;
	int flag;
};

#endif /* __RB_TYPES_H__ */
