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

/** \file blender/blenvm/intern/bvm_api.cc
 *  \ingroup bvm
 */

#include <set>
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_anim.h"
#include "BKE_effect.h"
#include "BKE_node.h"

#include "DEG_depsgraph_build.h"

#include "RE_shader_ext.h"

#include "BVM_api.h"

#include "RNA_access.h"
}

#include "function_cache.h"
#include "node_graph.h"
#include "typedesc.h"

#include "bvm_codegen.h"
#include "bvm_eval.h"
#include "bvm_function.h"

#ifdef WITH_LLVM
#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#endif

#include "glsl_codegen.h"

#include "util_array.h"
#include "util_debug.h"
#include "util_map.h"
#include "util_thread.h"
#include "util_string.h"

namespace blenvm {
static mesh_ptr __empty_mesh__;
static duplis_ptr __empty_duplilist__ = duplis_ptr(new DupliList());

static blenvm::EvalGlobals *eval_globals_default()
{
	static blenvm::EvalGlobals default_globals;
	return &default_globals;
}

static std::vector<NodeInputParam> forcefield_inputs;
static std::vector<NodeOutputParam> forcefield_outputs;
static std::vector<NodeInputParam> texture_inputs;
static std::vector<NodeOutputParam> texture_outputs;
static std::vector<NodeInputParam> modifier_inputs;
static std::vector<NodeOutputParam> modifier_outputs;
static std::vector<NodeInputParam> dupli_inputs;
static std::vector<NodeOutputParam> dupli_outputs;
static std::vector<NodeInputParam> hair_deform_inputs;
static std::vector<NodeOutputParam> hair_deform_outputs;

static void register_graph_types()
{
	static const float3 zerovec(0.0f, 0.0f, 0.0f);
	static const float C[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	static const float N[3] = {0.0f, 0.0f, 0.0f};
	
	forcefield_inputs.push_back(NodeInputParam("effector.object", "RNAPOINTER"));
	forcefield_inputs.push_back(NodeInputParam("effector.position", "FLOAT3"));
	forcefield_inputs.push_back(NodeInputParam("effector.velocity", "FLOAT3"));
	forcefield_outputs.push_back(NodeOutputParam("force", "FLOAT3", zerovec));
	forcefield_outputs.push_back(NodeOutputParam("impulse", "FLOAT3", zerovec));
	
	texture_inputs.push_back(NodeInputParam("texture.co", "FLOAT3"));
	texture_inputs.push_back(NodeInputParam("texture.cfra", "INT"));
	texture_inputs.push_back(NodeInputParam("texture.osatex", "INT"));
	texture_outputs.push_back(NodeOutputParam("color", "FLOAT4", C));
	texture_outputs.push_back(NodeOutputParam("normal", "FLOAT3", N));
	
	modifier_inputs.push_back(NodeInputParam("modifier.object", "RNAPOINTER"));
	modifier_inputs.push_back(NodeInputParam("modifier.base_mesh", "RNAPOINTER"));
	modifier_outputs.push_back(NodeOutputParam("mesh", "MESH", __empty_mesh__));
	
	dupli_inputs.push_back(NodeInputParam("dupli.object", "RNAPOINTER"));
	dupli_outputs.push_back(NodeOutputParam("dupli.result", "DUPLIS", __empty_duplilist__));
	
	hair_deform_inputs.push_back(NodeInputParam("location", "FLOAT3"));
	hair_deform_inputs.push_back(NodeInputParam("parameter", "FLOAT"));
	hair_deform_inputs.push_back(NodeInputParam("target", "MATRIX44"));
	hair_deform_outputs.push_back(NodeOutputParam("offset", "FLOAT3", zerovec));
}

}

void BVM_init(void)
{
	using namespace blenvm;
	
	create_empty_mesh(__empty_mesh__);
	
	nodes_init();
	register_graph_types();
	
#ifdef WITH_LLVM
	llvm_init();
#endif
}

void BVM_free(void)
{
	using namespace blenvm;
	
	blenvm::function_bvm_cache_clear();
#ifdef WITH_LLVM
	blenvm::function_llvm_cache_clear();
	llvm_free();
#endif
	
	nodes_free();
	TypeSpec::clear_typespecs();
	
	destroy_empty_mesh(__empty_mesh__);
}

/* ------------------------------------------------------------------------- */

BLI_INLINE blenvm::NodeGraph *_GRAPH(struct BVMNodeGraph *graph)
{ return (blenvm::NodeGraph *)graph; }
BLI_INLINE blenvm::NodeInstance *_NODE(struct BVMNodeInstance *node)
{ return (blenvm::NodeInstance *)node; }
BLI_INLINE blenvm::NodeInput *_INPUT(struct BVMNodeInput *input)
{ return (blenvm::NodeInput *)input; }
BLI_INLINE blenvm::NodeOutput *_OUTPUT(struct BVMNodeOutput *output)
{ return (blenvm::NodeOutput *)output; }
BLI_INLINE blenvm::TypeDesc *_TYPEDESC(struct BVMTypeDesc *typedesc)
{ return (blenvm::TypeDesc *)typedesc; }

struct BVMNodeInstance *BVM_nodegraph_add_node(BVMNodeGraph *graph, const char *type, const char *name)
{ return (struct BVMNodeInstance *)_GRAPH(graph)->add_node(type, name); }

void BVM_nodegraph_get_input(struct BVMNodeGraph *graph, const char *name,
                             struct BVMNodeInstance **node, const char **socket)
{
	const blenvm::NodeGraph::Input *input = _GRAPH(graph)->get_input(name);
	if (input) {
		if (node) *node = (BVMNodeInstance *)input->key.node;
		if (socket) *socket = input->key.socket->name.c_str();
	}
	else {
		if (node) *node = NULL;
		if (socket) *socket = "";
	}
}

void BVM_nodegraph_get_output(struct BVMNodeGraph *graph, const char *name,
                              struct BVMNodeInstance **node, const char **socket)
{
	const blenvm::NodeGraph::Output *output = _GRAPH(graph)->get_output(name);
	if (output) {
		if (node) *node = (BVMNodeInstance *)output->key.node;
		if (socket) *socket = output->key.socket->name.c_str();
	}
	else {
		if (node) *node = NULL;
		if (socket) *socket = "";
	}
}


int BVM_node_num_inputs(struct BVMNodeInstance *node)
{ return _NODE(node)->num_inputs(); }

int BVM_node_num_outputs(struct BVMNodeInstance *node)
{ return _NODE(node)->num_outputs(); }

struct BVMNodeInput *BVM_node_get_input(struct BVMNodeInstance *node, const char *name)
{ return (struct BVMNodeInput *)_NODE(node)->type->find_input(name); }

struct BVMNodeInput *BVM_node_get_input_n(struct BVMNodeInstance *node, int index)
{
	if (index >= 0 && index < _NODE(node)->num_inputs())
		return (struct BVMNodeInput *)_NODE(node)->type->find_input(index);
	else
		return NULL;
}

bool BVM_node_set_input_link(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                             struct BVMNodeInstance *from_node, struct BVMNodeOutput *from_output)
{
	return _NODE(node)->link_set(_INPUT(input)->name, blenvm::OutputKey(_NODE(from_node), _OUTPUT(from_output)->name));
}

struct BVMNodeOutput *BVM_node_get_output(struct BVMNodeInstance *node, const char *name)
{ return (struct BVMNodeOutput *)_NODE(node)->type->find_output(name); }

struct BVMNodeOutput *BVM_node_get_output_n(struct BVMNodeInstance *node, int index)
{
	if (index >= 0 && index < _NODE(node)->num_outputs())
		return (struct BVMNodeOutput *)_NODE(node)->type->find_output(index);
	else
		return NULL;
}

void BVM_node_set_input_value_float(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                    float value)
{ _NODE(node)->input_value_set(_INPUT(input)->name, value); }

void BVM_node_set_input_value_float3(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                     const float value[3])
{ _NODE(node)->input_value_set(_INPUT(input)->name, blenvm::float3::from_data(value)); }

void BVM_node_set_input_value_float4(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                     const float value[4])
{ _NODE(node)->input_value_set(_INPUT(input)->name, blenvm::float4::from_data(value)); }

void BVM_node_set_input_value_matrix44(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                       float value[4][4])
{ _NODE(node)->input_value_set(_INPUT(input)->name, blenvm::matrix44::from_data(&value[0][0])); }

void BVM_node_set_input_value_int(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                  int value)
{ _NODE(node)->input_value_set(_INPUT(input)->name, value); }

const char *BVM_node_input_name(struct BVMNodeInput *input)
{ return _INPUT(input)->name.c_str(); }

struct BVMTypeDesc *BVM_node_input_typedesc(struct BVMNodeInput *input)
{ return (struct BVMTypeDesc *)(&_INPUT(input)->typedesc); }

BVMInputValueType BVM_node_input_value_type(struct BVMNodeInput *input)
{ return _INPUT(input)->value_type; }

const char *BVM_node_output_name(struct BVMNodeOutput *output)
{ return _OUTPUT(output)->name.c_str(); }

struct BVMTypeDesc *BVM_node_output_typedesc(struct BVMNodeOutput *output)
{ return (struct BVMTypeDesc *)(&_OUTPUT(output)->typedesc); }

BVMOutputValueType BVM_node_output_value_type(struct BVMNodeOutput *output)
{ return _OUTPUT(output)->value_type; }

BVMType BVM_typedesc_base_type(struct BVMTypeDesc *typedesc)
{ return _TYPEDESC(typedesc)->get_typespec()->base_type(); }

BVMBufferType BVM_typedesc_buffer_type(struct BVMTypeDesc *typedesc)
{ return _TYPEDESC(typedesc)->get_typespec()->buffer_type(); }

/* ------------------------------------------------------------------------- */

void BVM_nodetree_compile_dependencies(bNodeTree *ntree, DepsNodeHandle *handle)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	
	if (!ntree->typeinfo->ext.call)
		return;
	
	RNA_id_pointer_create((ID *)ntree, &ptr);
	
	func = RNA_struct_find_function(ptr.type, "bvm_compile_dependencies");
	if (!func)
		return;
	
	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "depsnode", &handle);
	ntree->typeinfo->ext.call(NULL, &ptr, func, &list);
	
	RNA_parameter_list_free(&list);
}

void BVM_nodetree_eval_dependencies(bNodeTree *ntree, DepsNodeHandle *handle)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	
	if (!ntree->typeinfo->ext.call)
		return;
	
	RNA_id_pointer_create((ID *)ntree, &ptr);
	
	func = RNA_struct_find_function(ptr.type, "bvm_eval_dependencies");
	if (!func)
		return;
	
	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "depsnode", &handle);
	ntree->typeinfo->ext.call(NULL, &ptr, func, &list);
	
	RNA_parameter_list_free(&list);
}

/* ------------------------------------------------------------------------- */

BLI_INLINE blenvm::EvalGlobals *_GLOBALS(struct BVMEvalGlobals *globals)
{ return (blenvm::EvalGlobals *)globals; }
BLI_INLINE const blenvm::EvalGlobals *_GLOBALS(const struct BVMEvalGlobals *globals)
{ return (const blenvm::EvalGlobals *)globals; }
BLI_INLINE blenvm::EvalContext *_CTX(struct BVMEvalContext *ctx)
{ return (blenvm::EvalContext *)ctx; }

struct BVMEvalGlobals *BVM_globals_create(void)
{ return (BVMEvalGlobals *)(new blenvm::EvalGlobals()); }

void BVM_globals_free(struct BVMEvalGlobals *globals)
{ delete _GLOBALS(globals); }

struct ImagePool *BVM_globals_image_pool(struct BVMEvalGlobals *globals)
{ return _GLOBALS(globals)->image_pool(); }

void BVM_globals_add_object(struct BVMEvalGlobals *globals, int key, struct Object *ob)
{ _GLOBALS(globals)->add_object(key, ob); }

namespace blenvm {

struct EvalGlobalsHandle
{
	static void add_object_relation(DepsNodeHandle *_handle, struct Object *ob, eDepsComponent /*component*/, const char */*description*/)
	{
		EvalGlobalsHandle *handle = (EvalGlobalsHandle *)_handle;
		handle->globals->add_object(EvalGlobals::get_id_key((ID *)ob), ob);
	}
	
	static void add_bone_relation(DepsNodeHandle *_handle, struct Object *ob, const char */*bone_name*/, eDepsComponent /*component*/, const char */*description*/)
	{
		EvalGlobalsHandle *handle = (EvalGlobalsHandle *)_handle;
		handle->globals->add_object(EvalGlobals::get_id_key((ID *)ob), ob);
	}
	
	static void add_image_relation(DepsNodeHandle *_handle, struct Image *ima, eDepsComponent /*component*/, const char */*description*/)
	{
		EvalGlobalsHandle *handle = (EvalGlobalsHandle *)_handle;
		handle->globals->add_image(EvalGlobals::get_id_key((ID *)ima), ima);
	}
	
	EvalGlobalsHandle(EvalGlobals *globals) :
	    globals(globals)
	{
		memset(&handle, 0, sizeof(handle));
		handle.add_object_relation = add_object_relation;
		handle.add_bone_relation = add_bone_relation;
		handle.add_image_relation = add_image_relation;
	}
	
	DepsNodeHandle handle;
	EvalGlobals *globals;
};

static void rna_globals_update(bNodeTree *ntree, blenvm::EvalGlobals *globals)
{
	EvalGlobalsHandle handle(globals);
	DepsNodeHandle *phandle = &handle.handle;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	
	if (!ntree->typeinfo->ext.call)
		return;
	
	RNA_id_pointer_create((ID *)ntree, &ptr);
	
	func = RNA_struct_find_function(ptr.type, "bvm_eval_dependencies");
	if (!func)
		return;
	
	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "depsnode", &phandle);
	ntree->typeinfo->ext.call(NULL, &ptr, func, &list);
	
	RNA_parameter_list_free(&list);
}

} /* namespace blenvm */

void BVM_globals_add_nodetree_relations(struct BVMEvalGlobals *globals, bNodeTree *ntree)
{ rna_globals_update(ntree, _GLOBALS(globals)); }

int BVM_get_id_key(struct ID *id)
{ return blenvm::EvalGlobals::get_id_key(id); }

struct BVMEvalContext *BVM_context_create(void)
{ return (BVMEvalContext *)(new blenvm::EvalContext()); }

void BVM_context_free(struct BVMEvalContext *ctx)
{ delete _CTX(ctx); }

/* ------------------------------------------------------------------------- */

BLI_INLINE blenvm::FunctionBVM *_FUNC_BVM(struct BVMFunction *fn)
{ return (blenvm::FunctionBVM *)fn; }

static blenvm::spin_lock bvm_lock = blenvm::spin_lock();

void BVM_function_bvm_release(BVMFunction *fn)
{
	bvm_lock.lock();
	blenvm::function_bvm_cache_release(_FUNC_BVM(fn));
	bvm_lock.unlock();
}

void BVM_function_bvm_cache_remove(void *key)
{
	bvm_lock.lock();
	blenvm::function_bvm_cache_remove(key);
#ifdef WITH_LLVM
	blenvm::function_llvm_cache_remove(key);
#endif
	bvm_lock.unlock();
}

#ifdef WITH_LLVM
BLI_INLINE blenvm::FunctionLLVM *_FUNC_LLVM(struct BVMFunction *fn)
{ return (blenvm::FunctionLLVM *)fn; }

static blenvm::spin_lock llvm_lock = blenvm::spin_lock();

void BVM_function_llvm_release(BVMFunction *fn)
{
	llvm_lock.lock();
	blenvm::function_llvm_cache_release(_FUNC_LLVM(fn));
	llvm_lock.unlock();
}

void BVM_function_llvm_cache_remove(void *key)
{
	llvm_lock.lock();
	blenvm::function_llvm_cache_remove(key);
	llvm_lock.unlock();
}
#else
void BVM_function_llvm_release(BVMFunction */*fn*/) {}
void BVM_function_llvm_cache_remove(void */*key*/) {}
#endif

/* ------------------------------------------------------------------------- */

namespace blenvm {

static string get_ntree_unique_function_name(bNodeTree *ntree)
{
	std::stringstream ss;
	ss << "nodetree_" << ntree;
//	ss << ntree->id.name << "_" << ntree;
	return ss.str();
}

static void parse_py_nodes(bNodeTree *btree, NodeGraph *graph)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	
	RNA_id_pointer_create((ID *)btree, &ptr);
	
	func = RNA_struct_find_function(ptr.type, "bvm_compile");
	if (!func)
		return;
	
	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "graph", &graph);
	btree->typeinfo->ext.call(NULL, &ptr, func, &list);
	
	RNA_parameter_list_free(&list);
}

static void debug_node_graph(blenvm::NodeGraph &graph, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	if (mode != BVM_DEBUG_NODES_UNOPTIMIZED)
		graph.finalize();
	
	switch (mode) {
		case BVM_DEBUG_NODES:
		case BVM_DEBUG_NODES_UNOPTIMIZED: {
			debug::NodeGraphDumper dumper(debug_file);
			dumper.dump_graph(&graph, label);
			break;
		}
		case BVM_DEBUG_BVM_CODE: {
			DebugGraphvizBVMCompiler compiler;
			compiler.compile_function(graph, debug_file, label);
			break;
		}
		case BVM_DEBUG_LLVM_CODE: {
#ifdef WITH_LLVM
			LLVMCodeGenerator codegen(2);
			Compiler compiler(&codegen);
			compiler.debug_node_graph(label, graph, debug_file);
#endif
			break;
		}
		case BVM_DEBUG_LLVM_CODE_UNOPTIMIZED: {
#ifdef WITH_LLVM
			LLVMCodeGenerator codegen(0);
			Compiler compiler(&codegen);
			compiler.debug_node_graph(label, graph, debug_file);
#endif
			break;
		}
		case BVM_DEBUG_GLSL_CODE: {
			GLSLCodeGenerator codegen;
			Compiler compiler(&codegen);
			compiler.debug_node_graph(label, graph, debug_file);
			break;
		}
	}
}

static struct BVMFunction *gen_function_bvm(struct bNodeTree *btree, const string &UNUSED(name), bool use_cache,
                                            ArrayRef<NodeInputParam> inputs,
                                            ArrayRef<NodeOutputParam> outputs)
{
	using namespace blenvm;
	
	bvm_lock.lock();
	
	FunctionBVM *fn = NULL;
	if (use_cache) {
		fn = function_bvm_cache_acquire(btree);
	}
	
	if (!fn) {
		NodeGraph graph(inputs, outputs);
		parse_py_nodes(btree, &graph);
		graph.finalize();
		
		BVMCompiler compiler;
		fn = compiler.compile_function(graph);
		
		if (use_cache) {
			function_bvm_cache_set(btree, fn);
		}
	}
	
	fn->retain(fn);
	
	bvm_lock.unlock();
	
	return (BVMFunction *)fn;
}

static struct BVMFunction *gen_function_llvm(struct bNodeTree *btree, const string &name, bool use_cache,
                                             ArrayRef<NodeInputParam> inputs,
                                             ArrayRef<NodeOutputParam> outputs)
{
#ifdef WITH_LLVM
	using namespace blenvm;
	
	llvm_lock.lock();
	
	FunctionLLVM *fn = NULL;
	if (use_cache) {
		fn = function_llvm_cache_acquire(btree);
	}
	
	if (!fn) {
		NodeGraph graph(inputs, outputs);
		parse_py_nodes(btree, &graph);
		graph.finalize();
		
		LLVMCodeGenerator codegen(2);
		Compiler compiler(&codegen);
		compiler.compile_node_graph(name, graph);
		fn = new FunctionLLVM(codegen.function_address());
		
		if (use_cache) {
			function_llvm_cache_set(btree, fn);
		}
	}
	
	fn->retain(fn);
	
	llvm_lock.unlock();
	
	return (BVMFunction *)fn;
#else
	UNUSED_VARS(btree, name, use_cache, inputs, outputs);
	return NULL;
#endif
}

static char *gen_function_glsl(struct bNodeTree *btree, const string &name,
                               ArrayRef<NodeInputParam> inputs,
                               ArrayRef<NodeOutputParam> outputs)
{
	using namespace blenvm;
	
	NodeGraph graph(inputs, outputs);
	parse_py_nodes(btree, &graph);
	graph.finalize();
	
	GLSLCodeGenerator codegen;
	Compiler compiler(&codegen);
	compiler.compile_node_graph(name, graph);
	
	return BLI_strdup(codegen.code().str().c_str());
}

static void debug_nodes(bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode,
                        ArrayRef<NodeInputParam> inputs,
                        ArrayRef<NodeOutputParam> outputs)
{
	using namespace blenvm;
	
	NodeGraph graph(inputs, outputs);
	parse_py_nodes(btree, &graph);
	
	debug_node_graph(graph, debug_file, label, mode);
}

} /* namespace blenvm */

/* ========================================================================= */

struct BVMFunction *BVM_gen_forcefield_function_bvm(bNodeTree *btree, const char *name, bool use_cache)
{
	using namespace blenvm;
	return gen_function_bvm(btree, name ? name : get_ntree_unique_function_name(btree), use_cache,
	                        forcefield_inputs, forcefield_outputs);
}

void BVM_debug_forcefield_nodes(bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace blenvm;
	debug_nodes(btree, debug_file, label, mode, forcefield_inputs, forcefield_outputs);
}

void BVM_eval_forcefield_bvm(struct BVMEvalGlobals *globals, struct BVMEvalContext *ctx, struct BVMFunction *fn,
                             struct Object *effob, const EffectedPoint *point,
                             float force[3], float impulse[3])
{
	using namespace blenvm;
	
	PointerRNA object_ptr;
	RNA_id_pointer_create((ID *)effob, &object_ptr);
	const void *args[] = { &object_ptr, point->loc, point->vel };
	void *results[] = { force, impulse };
	
	_FUNC_BVM(fn)->eval(_CTX(ctx), _GLOBALS(globals), args, results);
}

/* ------------------------------------------------------------------------- */

namespace blenvm {

typedef void (*TexNodesFunc)(const struct EvalGlobals *globals,
                             Dual2<float4> *r_color, Dual2<float3> *r_normal,
                             const Dual2<float3> *co, int cfra, int osatex);

static void set_texresult(TexResult *result, const float4 &color, const float3 &normal)
{
	result->tr = color.x;
	result->tg = color.y;
	result->tb = color.z;
	result->ta = color.w;
	
	result->tin = (result->tr + result->tg + result->tb) / 3.0f;
	result->talpha = true;
	
	if (result->nor) {
		result->nor[0] = normal.x;
		result->nor[1] = normal.y;
		result->nor[2] = normal.z;
	}
}

}

struct BVMFunction *BVM_gen_texture_function_bvm(bNodeTree *btree, const char *name, bool use_cache)
{
	using namespace blenvm;
	return gen_function_bvm(btree, name ? name : get_ntree_unique_function_name(btree), use_cache,
	                        texture_inputs, texture_outputs);
}

struct BVMFunction *BVM_gen_texture_function_llvm(bNodeTree *btree, const char *name, bool use_cache)
{
	using namespace blenvm;
	return gen_function_llvm(btree, name ? name : get_ntree_unique_function_name(btree), use_cache,
	                         texture_inputs, texture_outputs);
}

void BVM_debug_texture_nodes(bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace blenvm;
	debug_nodes(btree, debug_file, label, mode, texture_inputs, texture_outputs);
}

void BVM_eval_texture_bvm(struct BVMEvalGlobals *_globals, struct BVMEvalContext *ctx, struct BVMFunction *fn,
                          struct TexResult *target,
                          float coord[3], float dxt[3], float dyt[3], int osatex,
                          short UNUSED(which_output), int cfra, int UNUSED(preview))
{
	using namespace blenvm;
	
	EvalGlobals *globals = _GLOBALS(_globals);
	if (globals == NULL)
		globals = eval_globals_default();
	
	float4 color;
	float3 normal;
	const void *args[] = { coord, dxt, dyt, &cfra, &osatex };
	void *results[] = { &color.x, &normal.x };
	
	_FUNC_BVM(fn)->eval(_CTX(ctx), globals, args, results);
	
	set_texresult(target, color, normal);
}

void BVM_eval_texture_llvm(struct BVMEvalGlobals *_globals, struct BVMEvalContext *UNUSED(ctx), struct BVMFunction *fn,
                           struct TexResult *value, struct TexResult *value_dx, struct TexResult *value_dy,
                           const float coord[3], const float dxt[3], const float dyt[3], int osatex,
                           short UNUSED(which_output), int cfra, int UNUSED(preview))
{
	using namespace blenvm;
	
	EvalGlobals *globals = _GLOBALS(_globals);
	if (globals == NULL)
		globals = eval_globals_default();
	
	Dual2<float4> r_color;
	Dual2<float3> r_normal;
	
#ifdef WITH_LLVM
	TexNodesFunc fp = (TexNodesFunc)_FUNC_LLVM(fn)->ptr();
	
	Dual2<float3> coord_v;
	coord_v.set_value(float3(coord[0], coord[1], coord[2]));
	if (dxt)
		coord_v.set_dx(float3(dxt[0], dxt[1], dxt[2]));
	else
		coord_v.set_dx(float3(1.0f, 0.0f, 0.0f));
	if (dyt)
		coord_v.set_dy(float3(dyt[0], dyt[1], dyt[2]));
	else
		coord_v.set_dy(float3(0.0f, 1.0f, 0.0f));
	
	fp(globals, &r_color, &r_normal, &coord_v, cfra, osatex);
#else
	UNUSED_VARS(fn, globals, coord, dxt, dyt, cfra, osatex);
	r_color = Dual2<float4>(float4(0.0f, 0.0f, 0.0f, 0.0f));
	r_normal = Dual2<float3>(float3(0.0f, 0.0f, 1.0f));
#endif
	
	if (value)
		set_texresult(value, r_color.value(), r_normal.value());
	if (value_dx)
		set_texresult(value_dx, r_color.dx(), r_normal.dx());
	if (value_dy)
		set_texresult(value_dy, r_color.dy(), r_normal.dy());
}

/* ------------------------------------------------------------------------- */

namespace blenvm {

typedef void (*ModNodesFunc)(const struct EvalGlobals *globals,
                             mesh_ptr *result,
                             struct PointerRNA *object_ptr, struct PointerRNA *base_mesh_ptr);

}

struct BVMFunction *BVM_gen_modifier_function_bvm(struct bNodeTree *btree, const char *name, bool use_cache)
{
	using namespace blenvm;
	return gen_function_bvm(btree, name ? name : get_ntree_unique_function_name(btree), use_cache,
	                        modifier_inputs, modifier_outputs);
}

struct BVMFunction *BVM_gen_modifier_function_llvm(struct bNodeTree *btree, const char *name, bool use_cache)
{
	using namespace blenvm;
	return gen_function_llvm(btree, name ? name : get_ntree_unique_function_name(btree), use_cache,
	                         modifier_inputs, modifier_outputs);
}

void BVM_debug_modifier_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace blenvm;
	debug_nodes(btree, debug_file, label, mode, modifier_inputs, modifier_outputs);
}

struct DerivedMesh *BVM_eval_modifier_bvm(struct BVMEvalGlobals *globals,
                                          struct BVMEvalContext *ctx,
                                          struct BVMFunction *fn,
                                          struct Object *object,
                                          struct Mesh *base_mesh)
{
	using namespace blenvm;

	PointerRNA object_ptr, base_mesh_ptr;
	RNA_id_pointer_create((ID *)object, &object_ptr);
	RNA_id_pointer_create((ID *)base_mesh, &base_mesh_ptr);
	mesh_ptr result;
	const void *args[] = { &object_ptr, &base_mesh_ptr };
	void *results[] = { &result };
	
	_FUNC_BVM(fn)->eval(_CTX(ctx), _GLOBALS(globals), args, results);
	
#if 0
	DerivedMesh *dm = result.get();
#else
	DerivedMesh *dm = CDDM_new(0, 0, 0, 0, 0);
#endif
	/* destroy the pointer variable */
	result.ptr().reset();
	return dm;
}

struct DerivedMesh *BVM_eval_modifier_llvm(struct BVMEvalGlobals *_globals,
                                           struct BVMEvalContext *UNUSED(ctx),
                                           struct BVMFunction *fn,
                                           struct Object *object,
                                           struct Mesh *base_mesh)
{
	using namespace blenvm;
	
	EvalGlobals *globals = _GLOBALS(_globals);
	if (globals == NULL)
		globals = eval_globals_default();
	
	mesh_ptr result;
	
#ifdef WITH_LLVM
	PointerRNA object_ptr, base_mesh_ptr;
	RNA_id_pointer_create((ID *)object, &object_ptr);
	RNA_id_pointer_create((ID *)base_mesh, &base_mesh_ptr);
	
	ModNodesFunc fp = (ModNodesFunc)_FUNC_LLVM(fn)->ptr();
	
	fp(globals, &result, &object_ptr, &base_mesh_ptr);
#else
	UNUSED_VARS(fn, globals, object, base_mesh);
	result.set(CDDM_new(0, 0, 0, 0, 0));
#endif
	
	DerivedMesh *dm = result.get();
	/* destroy the pointer variable */
	result.ptr().reset();
	return dm;
}

/* ------------------------------------------------------------------------- */

struct BVMFunction *BVM_gen_dupli_function_bvm(struct bNodeTree *btree, const char *name, bool use_cache)
{
	using namespace blenvm;
	return gen_function_bvm(btree, name ? name : get_ntree_unique_function_name(btree), use_cache,
	                        dupli_inputs, dupli_outputs);
}

void BVM_debug_dupli_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace blenvm;
	debug_nodes(btree, debug_file, label, mode, dupli_inputs, dupli_outputs);
}

void BVM_eval_dupli_bvm(struct BVMEvalGlobals *globals,
                        struct BVMEvalContext *ctx,
                        struct BVMFunction *fn,
                        struct Object *object,
                        struct DupliContainer *duplicont)
{
	using namespace blenvm;

	PointerRNA object_ptr;
	RNA_id_pointer_create((ID *)object, &object_ptr);
	const void *args[] = { &object_ptr };
	
	duplis_ptr result;
	void *results[] = { &result };
	
	_FUNC_BVM(fn)->eval(_CTX(ctx), _GLOBALS(globals), args, results);
	
	DupliList *duplis = result.get();
	if (duplis) {
		for (DupliList::const_iterator it = duplis->begin(); it != duplis->end(); ++it) {
			const Dupli &dupli = *it;
			BKE_dupli_add_instance(duplicont, dupli.object, (float (*)[4])dupli.transform.data, dupli.index,
			                       false, dupli.hide, dupli.recursive);
		}
	}
	result.reset();
}

/* ------------------------------------------------------------------------- */

char *BVM_gen_hair_deform_function_glsl(bNodeTree *btree, const char *name)
{
	using namespace blenvm;
	return gen_function_glsl(btree, name ? name : get_ntree_unique_function_name(btree), hair_deform_inputs, hair_deform_outputs);
}

void BVM_debug_hair_deform_nodes(bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace blenvm;
	debug_nodes(btree, debug_file, label, mode, hair_deform_inputs, hair_deform_outputs);
}
