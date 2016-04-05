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

#ifndef __LLVM_FUNCTION_H__
#define __LLVM_FUNCTION_H__

/** \file blender/blenvm/llvm/llvm_function.h
 *  \ingroup llvm
 */

#include "MEM_guardedalloc.h"

#include "function.h"

#include "llvm_engine.h"

#include "util_opcode.h"

namespace blenvm {

struct FunctionLLVM : public FunctionBase {
	FunctionLLVM(uint64_t address);
	
	uint64_t address() const { return m_address; }
	void *ptr() const { return (void *)m_address; }
	
private:
	uint64_t m_address;
	
	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:FunctionLLVM")
};

} /* namespace blenvm */

#endif /* __LLVM_FUNCTION_H__ */
