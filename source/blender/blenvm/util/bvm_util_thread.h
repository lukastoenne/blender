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

#ifndef __BVM_UTIL_THREAD_H__
#define __BVM_UTIL_THREAD_H__

/** \file bvm_util_thread.h
 *  \ingroup bvm
 */

#include "BLI_threads.h"

namespace bvm {

struct mutex {
	mutex()
	{
		BLI_mutex_init(&m);
	}
	
	~mutex()
	{
		BLI_mutex_end(&m);
	}
	
	void lock()
	{
		BLI_mutex_lock(&m);
	}
	
	void unlock()
	{
		BLI_mutex_unlock(&m);
	}
	
private:
	ThreadMutex m;
};

struct scoped_lock {
	scoped_lock(mutex &m_) :
	    m(&m_)
	{
		m->lock();
	}
	
	~scoped_lock()
	{
		m->unlock();
	}
	
private:
	mutex *m;
};

struct spin_lock {
	spin_lock(mutex &m_) :
	    m(&m_)
	{
		BLI_spin_init(&m_lock);
	}
	
	~spin_lock()
	{
		BLI_spin_end(&m_lock);
	}
	
	void lock()
	{
		BLI_spin_lock(&m_lock);
	}
	
	void unlock()
	{
		BLI_spin_unlock(&m_lock);
	}
	
private:
	mutex *m;
	SpinLock m_lock;
};

} /* namespace bvm */

#endif /* __BVM_UTIL_THREAD_H__ */
