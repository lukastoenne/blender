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

/** \file blender/blenjit/BJIT_modules.h
 *  \ingroup bjit
 */

#ifndef __BJIT_MODULES_H__
#define __BJIT_MODULES_H__

#ifdef __cplusplus
extern "C" {
#endif

struct LLVMModule;

void BJIT_init(void);
void BJIT_free(void);

/* Load precompiled BJIT modules.
 * modpath: path to a folder containing modules files (optional, default scripts path is used if NULL)
 * reload: unload all loaded modules first, otherwise existing modules are skipped
 */
void BJIT_load_all_modules(const char *modpath, bool reload);
void BJIT_unload_all_modules(void);

/* load a single module from a file */
void BJIT_load_module(const char *modfile, const char *modname);

int BJIT_num_loaded_modules(void);
struct LLVMModule *BJIT_get_loaded_module_n(int n);
struct LLVMModule *BJIT_get_loaded_module(const char *name);

const char *BJIT_module_name(struct LLVMModule *mod);

int BJIT_module_num_functions(struct LLVMModule *mod);
struct LLVMFunction *BJIT_module_get_function_n(struct LLVMModule *mod, int n);
struct LLVMFunction *BJIT_module_get_function(struct LLVMModule *mod, const char *name);

#ifdef __cplusplus
}
#endif

#endif
