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

#ifndef __HAIR_DEBUG_H__
#define __HAIR_DEBUG_H__

#include <vector>

#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

#ifndef NDEBUG
#define HAIR_DEBUG
#endif

struct Debug {
	struct Contact {
		float3 coA, coB;
	};
	typedef std::vector<Contact> CollisionContacts;
	
#ifdef HAIR_DEBUG

	static void collision_contact(const float3 &coA, const float3 &coB)
	{
		if (m_contacts) {
			Contact c;
			c.coA = coA;
			c.coB = coB;
			m_contacts->push_back(c);
		}
	}
	
	static void set_collision_contacts(CollisionContacts *contacts)
	{
		m_contacts = contacts;
	}
	
private:
	static CollisionContacts *m_contacts;

#else
	
	static void collision_contact(const float3 &co) {}
	static void set_collision_contacts(CollisionContacts *contacts) {}
	
#endif
};

HAIR_NAMESPACE_END

#endif
