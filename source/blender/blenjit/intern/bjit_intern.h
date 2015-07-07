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
#include <map>

namespace llvm {
class Function;
class Module;
class Value;
class CallInst;
}

typedef std::map<std::string, llvm::Module*> ModuleMap;

/* modules.cpp */
//void *bjit_compile_module(llvm::Module *mod, const char *funcname);
void bjit_link_module(llvm::Module *mod);
void bjit_remove_module(llvm::Module *mod);

const char *bjit_get_function_name(llvm::Function *func);
llvm::Function *bjit_find_function(llvm::Module *mod, const std::string &name);
void bjit_finalize_function(llvm::Module *mod, llvm::Function *func, int opt_level);
void *bjit_compile_function(llvm::Function *func);
void bjit_free_function(llvm::Function *func);

ModuleMap &bjit_get_modules();
llvm::Module *bjit_get_module(const std::string &name);

namespace bjit {

struct NodeGraph;

void build_effector_module(void);
void free_effector_module(void);

/* codegen */
typedef std::map<std::string, llvm::Value*> InputList;
typedef std::map<std::string, llvm::Value*> OutputList;

llvm::Function *codegen(const NodeGraph &graph, llvm::Module *module);

} /* namespace bjit */

/* unused, but could be handy for treating ListBase like a standard container */
#if 0
template <typename T>
struct ListBaseIterator {
	ListBaseIterator() :
	    link(NULL)
	{}

	ListBaseIterator(const ListBase &lb) :
	    link((Link *)lb.first)
	{}
	
	ListBaseIterator& operator++ (void)
	{
		link = link->next;
		return *this;
	}
	
	T* operator* (void)
	{
		return (T *)link;
	}
	
private:
	Link *link;
};
#endif

#endif
