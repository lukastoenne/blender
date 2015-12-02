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

struct BVMFunction;
struct BVMModule;

void BVM_init(void);
void BVM_free(void);

/* ------------------------------------------------------------------------- */

void BVM_function_free(struct BVMFunction *fn);

/* ------------------------------------------------------------------------- */

struct BVMCompileContext;
struct BVMNodeGraph;
struct BVMNodeInstance;
struct BVMNodeInput;
struct BVMNodeOutput;
struct BVMTypeDesc;

int BVM_compile_get_object_index(struct BVMCompileContext *context, struct Object *ob);

struct BVMNodeInstance *BVM_nodegraph_add_node(struct BVMNodeGraph *graph, const char *type, const char *name);

void BVM_nodegraph_get_output(struct BVMNodeGraph *graph, const char *name,
                              struct BVMNodeInstance **node, const char **socket);

int BVM_node_num_inputs(struct BVMNodeInstance *node);
int BVM_node_num_outputs(struct BVMNodeInstance *node);
struct BVMNodeInput *BVM_node_get_input(struct BVMNodeInstance *node, const char *name);
struct BVMNodeInput *BVM_node_get_input_n(struct BVMNodeInstance *node, int index);
struct BVMNodeOutput *BVM_node_get_output(struct BVMNodeInstance *node, const char *name);
struct BVMNodeOutput *BVM_node_get_output_n(struct BVMNodeInstance *node, int index);

bool BVM_node_set_input_link(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                             struct BVMNodeInstance *from_node, struct BVMNodeOutput *from_output);

void BVM_node_set_input_value_float(struct BVMNodeInstance *node, struct BVMNodeInput *input, float value);
void BVM_node_set_input_value_float3(struct BVMNodeInstance *node, struct BVMNodeInput *input, const float value[3]);
void BVM_node_set_input_value_float4(struct BVMNodeInstance *node, struct BVMNodeInput *input, const float value[4]);
void BVM_node_set_input_value_matrix44(struct BVMNodeInstance *node, struct BVMNodeInput *input, float value[4][4]);
void BVM_node_set_input_value_int(struct BVMNodeInstance *node, struct BVMNodeInput *input, int value);

const char *BVM_node_input_name(struct BVMNodeInput *input);
struct BVMTypeDesc *BVM_node_input_typedesc(struct BVMNodeInput *input);
BVMValueType BVM_node_input_value_type(struct BVMNodeInput *input);
const char *BVM_node_output_name(struct BVMNodeOutput *output);
struct BVMTypeDesc *BVM_node_output_typedesc(struct BVMNodeOutput *output);

BVMType BVM_typedesc_base_type(struct BVMTypeDesc *typedesc);

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

struct BVMFunction *BVM_gen_forcefield_function(const struct BVMEvalGlobals *globals, struct bNodeTree *btree);

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *context, struct BVMFunction *fn,
                         struct Object *effob, const struct EffectedPoint *point, float force[3], float impulse[3]);

/* ------------------------------------------------------------------------- */

struct Tex;
struct TexResult;

struct BVMFunction *BVM_gen_texture_function(const struct BVMEvalGlobals *globals, struct Tex *tex,
                                                 struct bNodeTree *btree, FILE *debug_file);

void BVM_eval_texture(struct BVMEvalContext *context, struct BVMFunction *fn,
                      struct TexResult *target,
                      float coord[3], float dxt[3], float dyt[3], int osatex,
                      short which_output, int cfra, int preview);

struct BVMFunction *BVM_texture_cache_acquire(struct Tex *tex);
void BVM_texture_cache_release(struct Tex *tex);
void BVM_texture_cache_invalidate(struct Tex *tex);
void BVM_texture_cache_clear(void);

/* ------------------------------------------------------------------------- */

struct DerivedMesh;
struct Mesh;

struct BVMFunction *BVM_gen_modifier_function(const struct BVMEvalGlobals *globals,
                                              struct Object *ob, struct bNodeTree *btree,
                                              FILE *debug_file);

struct DerivedMesh *BVM_eval_modifier(struct BVMEvalContext *context, struct BVMFunction *fn, struct Mesh *base_mesh);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* __BVM_API_H__ */
