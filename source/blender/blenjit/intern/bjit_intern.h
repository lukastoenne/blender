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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/intern/bjit_intern.h
 *  \ingroup bjit
 */

#ifndef __BJIT_INTERN_H__
#define __BJIT_INTERN_H__

#include <string>

namespace llvm {
class Function;
class Module;
}

typedef std::map<std::string, llvm::Module*> ModuleMap;

/* modules.cpp */
//void *bjit_compile_module(llvm::Module *mod, const char *funcname);
void bjit_link_module(llvm::Module *mod);
void bjit_remove_module(llvm::Module *mod);

llvm::Function *bjit_find_function(llvm::Module *mod, const std::string &name);
void bjit_finalize_function(llvm::Module *mod, llvm::Function *func, int opt_level);
void *bjit_compile_function(llvm::Function *func);
void bjit_free_function(llvm::Function *func);

ModuleMap &bjit_get_modules();
llvm::Module *bjit_get_module(const std::string &name);

#endif
