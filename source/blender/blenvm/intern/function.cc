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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenvm/intern/function.cc
 *  \ingroup blenvm
 */

#include <assert.h>

#include "function.h"

#include "util_thread.h"

namespace blenvm {

static spin_lock users_lock = spin_lock();

FunctionBase::FunctionBase() :
    m_users(0)
{
}

FunctionBase::~FunctionBase()
{
}

void FunctionBase::retain(FunctionBase *fn)
{
	if (fn) {
		users_lock.lock();
		++fn->m_users;
		users_lock.unlock();
	}
}

bool FunctionBase::release(FunctionBase *fn)
{
	bool released = false;
	if (fn) {
		users_lock.lock();
		assert(fn->m_users > 0);
		--fn->m_users;
		released = (fn->m_users == 0);
		users_lock.unlock();
	}
	return released;
}

} /* namespace blenvm */
