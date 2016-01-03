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

struct BVMFunction *BVM_function_cache_acquire(void *key);
void BVM_function_release(struct BVMFunction *_fn);
void BVM_function_cache_set(void *key, struct BVMFunction *_fn);
void BVM_function_cache_remove(void *key);
void BVM_function_cache_clear(void);

/* ------------------------------------------------------------------------- */

struct BVMNodeGraph;
struct BVMNodeInstance;
struct BVMNodeInput;
struct BVMNodeOutput;
struct BVMTypeDesc;

struct BVMNodeInstance *BVM_nodegraph_add_node(struct BVMNodeGraph *graph, const char *type, const char *name);

void BVM_nodegraph_get_input(struct BVMNodeGraph *graph, const char *name,
                             struct BVMNodeInstance **node, const char **socket);
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
BVMInputValueType BVM_node_input_value_type(struct BVMNodeInput *input);
const char *BVM_node_output_name(struct BVMNodeOutput *output);
struct BVMTypeDesc *BVM_node_output_typedesc(struct BVMNodeOutput *output);
BVMOutputValueType BVM_node_output_value_type(struct BVMNodeOutput *output);

BVMType BVM_typedesc_base_type(struct BVMTypeDesc *typedesc);
BVMBufferType BVM_typedesc_buffer_type(struct BVMTypeDesc *typedesc);

/* ------------------------------------------------------------------------- */

struct BVMEvalGlobals;
struct BVMEvalContext;

struct bNodeTree;

struct BVMEvalGlobals *BVM_globals_create(void);
void BVM_globals_free(struct BVMEvalGlobals *globals);

void BVM_globals_add_object(struct BVMEvalGlobals *globals, int key, struct Object *ob);
void BVM_globals_add_nodetree_relations(struct BVMEvalGlobals *globals, struct bNodeTree *ntree);

int BVM_get_id_key(struct ID *id);

struct BVMEvalContext *BVM_context_create(void);
void BVM_context_free(struct BVMEvalContext *context);

typedef enum BVMDebugMode {
	BVM_DEBUG_NODES,
	BVM_DEBUG_NODES_UNOPTIMIZED,
	BVM_DEBUG_CODEGEN,
} BVMDebugMode;

/* ------------------------------------------------------------------------- */

struct Object;
struct EffectedPoint;

struct BVMFunction *BVM_gen_forcefield_function(struct bNodeTree *btree);
void BVM_debug_forcefield_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *context, struct BVMFunction *fn,
                         struct Object *effob, const struct EffectedPoint *point, float force[3], float impulse[3]);

/* ------------------------------------------------------------------------- */

struct Tex;
struct TexResult;

struct BVMFunction *BVM_gen_texture_function(struct bNodeTree *btree);
void BVM_debug_texture_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

void BVM_eval_texture(struct BVMEvalContext *context, struct BVMFunction *fn,
                      struct TexResult *target,
                      float coord[3], float dxt[3], float dyt[3], int osatex,
                      short which_output, int cfra, int preview);

/* ------------------------------------------------------------------------- */

struct DerivedMesh;
struct Mesh;

struct BVMFunction *BVM_gen_modifier_function(struct bNodeTree *btree);
void BVM_debug_modifier_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

struct DerivedMesh *BVM_eval_modifier(struct BVMEvalGlobals *globals,
                                      struct BVMEvalContext *context,
                                      struct BVMFunction *fn,
                                      struct Object *ob,
                                      struct Mesh *base_mesh);

/* ------------------------------------------------------------------------- */

struct DupliContainer;

struct BVMFunction *BVM_gen_dupli_function(struct bNodeTree *btree);
void BVM_debug_dupli_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

void BVM_eval_dupli(struct BVMEvalGlobals *globals,
                    struct BVMEvalContext *context,
                    struct BVMFunction *fn,
                    struct Object *object,
                    struct DupliContainer *duplicont);

#ifdef __cplusplus
}
#endif

#endif /* __BVM_API_H__ */
