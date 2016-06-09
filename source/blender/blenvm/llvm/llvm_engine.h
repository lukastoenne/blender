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

#ifndef __LLVM_ENGINE_H__
#define __LLVM_ENGINE_H__

/** \file blender/blenvm/llvm/llvm_engine.h
 *  \ingroup llvm
 */

#include "util_string.h"
#include "util_opcode.h"

namespace llvm {
class ExecutionEngine;
class Function;
class Module;
}

namespace blenvm {

void llvm_init();
void llvm_free();

llvm::ExecutionEngine *llvm_execution_engine();

void llvm_optimize_module(llvm::Module *mod, int opt_level);
void llvm_optimize_function(llvm::Function *func, int opt_level);

bool llvm_has_external_impl_value(OpCode op);
bool llvm_has_external_impl_deriv(OpCode op);

} /* namespace blenvm */

#endif /* __LLVM_ENGINE_H__ */
