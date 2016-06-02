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

/** \file blender/blenvm/BVM_api.h
 *  \ingroup bvm
 */

#include <stdio.h>

#include "BVM_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Generic handle for functions.
 * Warning: This is used for all backends! Callers must know which backend to use!
 */
struct BVMFunction;

void BVM_init(void);
void BVM_free(void);

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

struct bNodeTree;
struct DepsNodeHandle;

void BVM_nodetree_compile_dependencies(struct bNodeTree *ntree, struct DepsNodeHandle *handle);
void BVM_nodetree_eval_dependencies(struct bNodeTree *ntree, struct DepsNodeHandle *handle);

/* ------------------------------------------------------------------------- */

struct BVMEvalGlobals;
struct BVMEvalContext;

struct bNodeTree;
struct ImagePool;

struct BVMEvalGlobals *BVM_globals_create(void);
void BVM_globals_free(struct BVMEvalGlobals *globals);

struct ImagePool *BVM_globals_image_pool(struct BVMEvalGlobals *globals);
void BVM_globals_add_object(struct BVMEvalGlobals *globals, int key, struct Object *ob);
void BVM_globals_add_nodetree_relations(struct BVMEvalGlobals *globals, struct bNodeTree *ntree);

int BVM_get_id_key(struct ID *id);

struct BVMEvalContext *BVM_context_create(void);
void BVM_context_free(struct BVMEvalContext *context);

typedef enum BVMDebugMode {
	BVM_DEBUG_NODES,
	BVM_DEBUG_NODES_UNOPTIMIZED,
	BVM_DEBUG_BVM_CODE,
	BVM_DEBUG_LLVM_CODE,
	BVM_DEBUG_LLVM_CODE_UNOPTIMIZED,
} BVMDebugMode;

/* ------------------------------------------------------------------------- */

void BVM_function_bvm_release(struct BVMFunction *fn);
void BVM_function_bvm_cache_remove(void *key);

void BVM_function_llvm_release(struct BVMFunction *fn);
void BVM_function_llvm_cache_remove(void *key);

/* ------------------------------------------------------------------------- */

struct Object;
struct EffectedPoint;

struct BVMFunction *BVM_gen_forcefield_function_bvm(struct bNodeTree *btree, bool use_cache);
void BVM_debug_forcefield_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

void BVM_eval_forcefield_bvm(struct BVMEvalGlobals *globals, struct BVMEvalContext *context, struct BVMFunction *fn,
                             struct Object *effob, const struct EffectedPoint *point, float force[3], float impulse[3]);

/* ------------------------------------------------------------------------- */

struct Tex;
struct TexResult;

struct BVMFunction *BVM_gen_texture_function_bvm(struct bNodeTree *btree, bool use_cache);
struct BVMFunction *BVM_gen_texture_function_llvm(struct bNodeTree *btree, bool use_cache);

void BVM_debug_texture_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

void BVM_eval_texture_bvm(struct BVMEvalGlobals *globals, struct BVMEvalContext *context,
                          struct BVMFunction *fn,
                          struct TexResult *target,
                          float coord[3], float dxt[3], float dyt[3], int osatex,
                          short which_output, int cfra, int preview);
void BVM_eval_texture_llvm(struct BVMEvalGlobals *globals, struct BVMEvalContext *context,
                           struct BVMFunction *fn,
                           struct TexResult *value,
                           struct TexResult *value_dx,
                           struct TexResult *value_dy,
                           const float coord[3], const float dxt[3], const float dyt[3], int osatex,
                           short which_output, int cfra, int preview);

/* ------------------------------------------------------------------------- */

struct DerivedMesh;
struct Mesh;

struct BVMFunction *BVM_gen_modifier_function_bvm(struct bNodeTree *btree, bool use_cache);
void BVM_debug_modifier_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

struct DerivedMesh *BVM_eval_modifier_bvm(struct BVMEvalGlobals *globals,
                                      struct BVMEvalContext *context,
                                      struct BVMFunction *fn,
                                      struct Object *ob,
                                      struct Mesh *base_mesh);

/* ------------------------------------------------------------------------- */

struct DupliContainer;

struct BVMFunction *BVM_gen_dupli_function_bvm(struct bNodeTree *btree, bool use_cache);
void BVM_debug_dupli_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode);

void BVM_eval_dupli_bvm(struct BVMEvalGlobals *globals,
                    struct BVMEvalContext *context,
                    struct BVMFunction *fn,
                    struct Object *object,
                    struct DupliContainer *duplicont);

#ifdef __cplusplus
}
#endif

#endif /* __BVM_API_H__ */
