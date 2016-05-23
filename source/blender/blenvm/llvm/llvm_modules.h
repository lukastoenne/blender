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

#ifndef __LLVM_MODULES_H__
#define __LLVM_MODULES_H__

/** \file blender/blenvm/llvm/llvm_modules.h
 *  \ingroup llvm
 */

#include "util_string.h"

namespace llvm {
class Function;
class Module;
}

namespace blenvm {

llvm::Function *llvm_find_external_function(llvm::Module *mod, const string &name);

void llvm_load_module(const string &modfile, const string &modname);
void llvm_load_all_modules(const string &modpath, bool reload);
void llvm_unload_all_modules();

string llvm_value_function_name(const string &node);
string llvm_deriv_function_name(const string &node, int var_n);

} /* namespace blenvm */

#endif /* __LLVM_MODULES_H__ */
