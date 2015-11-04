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

#ifndef __BVM_API_H__
#define __BVM_API_H__

/** \file BVM_api.h
 *  \ingroup bvm
 */

#include <stdio.h>

#include "BVM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BVMExpression;
struct BVMFunction;
struct BVMModule;

void BVM_init(void);
void BVM_free(void);

struct BVMModule *BVM_module_create(void);
void BVM_module_free(struct BVMModule *mod);

struct BVMFunction *BVM_module_create_function(struct BVMModule *mod, const char *name);
bool BVM_module_delete_function(struct BVMModule *mod, const char *name);

/* ------------------------------------------------------------------------- */

const char *BVM_function_name(const struct BVMFunction *fun);

/* ------------------------------------------------------------------------- */

void BVM_expression_free(struct BVMExpression *expr);

/* ------------------------------------------------------------------------- */

struct BVMCompileContext;
struct BVMNodeGraph;
struct BVMNodeInstance;

int BVM_compile_get_object_index(struct BVMCompileContext *context, struct Object *ob);

struct BVMNodeInstance *BVM_nodegraph_add_node(struct BVMNodeGraph *graph, const char *type, const char *name);
void BVM_nodegraph_add_link(struct BVMNodeGraph *graph,
                            struct BVMNodeInstance *from_node, const char *from_socket,
                            struct BVMNodeInstance *to_node, const char *to_socket,
                            bool autoconvert);
void BVM_nodegraph_set_output_link(struct BVMNodeGraph *graph,
                                   const char *name, struct BVMNodeInstance *node, const char *socket);

void BVM_node_set_input_value_float(struct BVMNodeInstance *node,
                                    const char *socket, float value);
void BVM_node_set_input_value_float3(struct BVMNodeInstance *node,
                                     const char *socket, const float value[3]);
void BVM_node_set_input_value_float4(struct BVMNodeInstance *node,
                                     const char *socket, const float value[4]);
void BVM_node_set_input_value_matrix44(struct BVMNodeInstance *node,
                                       const char *socket, float value[4][4]);
void BVM_node_set_input_value_int(struct BVMNodeInstance *node,
                                  const char *socket, int value);

/* ------------------------------------------------------------------------- */

struct BVMEvalGlobals;
struct BVMEvalContext;

struct BVMEvalGlobals *BVM_globals_create(void);
void BVM_globals_free(struct BVMEvalGlobals *globals);

void BVM_globals_add_object(struct BVMEvalGlobals *globals, struct Object *ob);

struct BVMEvalContext *BVM_context_create(void);
void BVM_context_free(struct BVMEvalContext *context);

/* ------------------------------------------------------------------------- */

struct bNodeTree;
struct Object;
struct EffectedPoint;

struct BVMExpression *BVM_gen_forcefield_expression(const struct BVMEvalGlobals *globals, struct bNodeTree *btree);

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *context, struct BVMExpression *expr,
                         struct Object *effob, const struct EffectedPoint *point, float force[3], float impulse[3]);

/* ------------------------------------------------------------------------- */

struct Tex;
struct TexResult;

struct BVMExpression *BVM_gen_texture_expression(const struct BVMEvalGlobals *globals, struct Tex *tex,
                                                 struct bNodeTree *btree, FILE *debug_file);

void BVM_eval_texture(struct BVMEvalContext *context, struct BVMExpression *expr,
                      struct TexResult *target,
                      float coord[3], float dxt[3], float dyt[3], int osatex,
                      short which_output, int cfra, int preview);

struct BVMExpression *BVM_texture_cache_acquire(Tex *tex);
void BVM_texture_cache_release(Tex *tex);
void BVM_texture_cache_invalidate(Tex *tex);
void BVM_texture_cache_clear(void);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* __BVM_API_H__ */
