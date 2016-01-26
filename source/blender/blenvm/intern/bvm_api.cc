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

#include "bvm_codegen.h"
#include "bvm_eval.h"
#include "bvm_function.h"
#include "bvm_nodegraph.h"

#include "bvm_util_debug.h"
#include "bvm_util_map.h"
#include "bvm_util_thread.h"

namespace bvm {
static mesh_ptr __empty_mesh__;
static duplis_ptr __empty_duplilist__ = duplis_ptr(new DupliList());
}

void BVM_init(void)
{
	using namespace bvm;
	
	create_empty_mesh(__empty_mesh__);
	
	nodes_init();
}

void BVM_free(void)
{
	using namespace bvm;
	
	BVM_function_cache_clear();
	
	nodes_free();
	
	destroy_empty_mesh(__empty_mesh__);
}

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::Function *_FUNC(struct BVMFunction *fn)
{ return (bvm::Function *)fn; }

void BVM_function_free(struct BVMFunction *fn)
{ delete _FUNC(fn); }

namespace bvm {

typedef unordered_map<void*, Function*> FunctionCache;
typedef std::pair<void*, Function*> FunctionCachePair;

static FunctionCache bvm_function_cache;
static mutex bvm_function_cache_mutex;
static spin_lock bvm_function_cache_lock = spin_lock(bvm_function_cache_mutex);

} /* namespace bvm */

struct BVMFunction *BVM_function_cache_acquire(void *key)
{
	using namespace bvm;
	
	bvm_function_cache_lock.lock();
	FunctionCache::const_iterator it = bvm_function_cache.find(key);
	Function *fn = NULL;
	if (it != bvm_function_cache.end()) {
		fn = it->second;
		Function::retain(fn);
	}
	bvm_function_cache_lock.unlock();
	return (BVMFunction *)fn;
}

void BVM_function_release(BVMFunction *_fn)
{
	using namespace bvm;
	Function *fn = _FUNC(_fn);
	
	if (!fn)
		return;
	
	Function::release(&fn);
	
	if (fn == NULL) {
		bvm_function_cache_lock.lock();
		FunctionCache::iterator it = bvm_function_cache.begin();
		while (it != bvm_function_cache.end()) {
			if (it->second == fn) {
				FunctionCache::iterator it_del = it++;
				bvm_function_cache.erase(it_del);
			}
			else
				++it;
		}
		bvm_function_cache_lock.unlock();
	}
}

void BVM_function_cache_set(void *key, BVMFunction *_fn)
{
	using namespace bvm;
	Function *fn = _FUNC(_fn);
	
	bvm_function_cache_lock.lock();
	if (fn) {
		FunctionCache::iterator it = bvm_function_cache.find(key);
		if (it == bvm_function_cache.end()) {
			Function::retain(fn);
			bvm_function_cache.insert(FunctionCachePair(key, fn));
		}
		else if (fn != it->second) {
			Function::release(&it->second);
			Function::retain(fn);
			it->second = fn;
		}
	}
	else {
		FunctionCache::iterator it = bvm_function_cache.find(key);
		if (it != bvm_function_cache.end()) {
			Function::release(&it->second);
			bvm_function_cache.erase(it);
		}
	}
	bvm_function_cache_lock.unlock();
}

void BVM_function_cache_remove(void *key)
{
	using namespace bvm;
	
	bvm_function_cache_lock.lock();
	FunctionCache::iterator it = bvm_function_cache.find(key);
	if (it != bvm_function_cache.end()) {
		Function::release(&it->second);
		
		bvm_function_cache.erase(it);
	}
	bvm_function_cache_lock.unlock();
}

void BVM_function_cache_clear(void)
{
	using namespace bvm;
	
	bvm_function_cache_lock.lock();
	for (FunctionCache::iterator it = bvm_function_cache.begin(); it != bvm_function_cache.end(); ++it) {
		Function::release(&it->second);
	}
	bvm_function_cache.clear();
	bvm_function_cache_lock.unlock();
}

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::NodeGraph *_GRAPH(struct BVMNodeGraph *graph)
{ return (bvm::NodeGraph *)graph; }
BLI_INLINE bvm::NodeInstance *_NODE(struct BVMNodeInstance *node)
{ return (bvm::NodeInstance *)node; }
BLI_INLINE bvm::NodeInput *_INPUT(struct BVMNodeInput *input)
{ return (bvm::NodeInput *)input; }
BLI_INLINE bvm::NodeOutput *_OUTPUT(struct BVMNodeOutput *output)
{ return (bvm::NodeOutput *)output; }
BLI_INLINE bvm::TypeDesc *_TYPEDESC(struct BVMTypeDesc *typedesc)
{ return (bvm::TypeDesc *)typedesc; }

struct BVMNodeInstance *BVM_nodegraph_add_node(BVMNodeGraph *graph, const char *type, const char *name)
{ return (struct BVMNodeInstance *)_GRAPH(graph)->add_node(type, name); }

void BVM_nodegraph_get_input(struct BVMNodeGraph *graph, const char *name,
                             struct BVMNodeInstance **node, const char **socket)
{
	const bvm::NodeGraph::Input *input = _GRAPH(graph)->get_input(name);
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
	const bvm::NodeGraph::Output *output = _GRAPH(graph)->get_output(name);
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
	return _NODE(node)->link_set(_INPUT(input)->name, bvm::OutputKey(_NODE(from_node), _OUTPUT(from_output)->name));
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
{ _NODE(node)->input_value_set(_INPUT(input)->name, bvm::float3::from_data(value)); }

void BVM_node_set_input_value_float4(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                     const float value[4])
{ _NODE(node)->input_value_set(_INPUT(input)->name, bvm::float4::from_data(value)); }

void BVM_node_set_input_value_matrix44(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                       float value[4][4])
{ _NODE(node)->input_value_set(_INPUT(input)->name, bvm::matrix44::from_data(&value[0][0])); }

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
{ return _TYPEDESC(typedesc)->base_type(); }

BVMBufferType BVM_typedesc_buffer_type(struct BVMTypeDesc *typedesc)
{ return _TYPEDESC(typedesc)->buffer_type(); }

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::EvalGlobals *_GLOBALS(struct BVMEvalGlobals *globals)
{ return (bvm::EvalGlobals *)globals; }
BLI_INLINE const bvm::EvalGlobals *_GLOBALS(const struct BVMEvalGlobals *globals)
{ return (const bvm::EvalGlobals *)globals; }
BLI_INLINE bvm::EvalContext *_CTX(struct BVMEvalContext *ctx)
{ return (bvm::EvalContext *)ctx; }

struct BVMEvalGlobals *BVM_globals_create(void)
{ return (BVMEvalGlobals *)(new bvm::EvalGlobals()); }

void BVM_globals_free(struct BVMEvalGlobals *globals)
{ delete _GLOBALS(globals); }

void BVM_globals_add_object(struct BVMEvalGlobals *globals, int key, struct Object *ob)
{ _GLOBALS(globals)->add_object(key, ob); }

namespace bvm {

struct EvalGlobalsHandle
{
	static void add_object_relation(DepsNodeHandle *_handle, struct Object *ob, eDepsNode_Type /*component*/, const char */*description*/)
	{
		EvalGlobalsHandle *handle = (EvalGlobalsHandle *)_handle;
		handle->globals->add_object(EvalGlobals::get_id_key((ID *)ob), ob);
	}
	
	static void add_bone_relation(DepsNodeHandle *_handle, struct Object *ob, const char */*bone_name*/, eDepsNode_Type /*component*/, const char */*description*/)
	{
		EvalGlobalsHandle *handle = (EvalGlobalsHandle *)_handle;
		handle->globals->add_object(EvalGlobals::get_id_key((ID *)ob), ob);
	}
	
	static void add_image_relation(DepsNodeHandle *_handle, struct Image *ima, eDepsNode_Type /*component*/, const char */*description*/)
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

static void rna_globals_update(bNodeTree *ntree, bvm::EvalGlobals *globals)
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

} /* namespace bvm */

void BVM_globals_add_nodetree_relations(struct BVMEvalGlobals *globals, bNodeTree *ntree)
{ rna_globals_update(ntree, _GLOBALS(globals)); }

int BVM_get_id_key(struct ID *id)
{ return bvm::EvalGlobals::get_id_key(id); }

struct BVMEvalContext *BVM_context_create(void)
{ return (BVMEvalContext *)(new bvm::EvalContext()); }

void BVM_context_free(struct BVMEvalContext *ctx)
{ delete _CTX(ctx); }

/* ------------------------------------------------------------------------- */

static void parse_py_nodes(bNodeTree *btree, bvm::NodeGraph *graph)
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

static void init_forcefield_graph(bvm::NodeGraph &graph)
{
	using namespace bvm;
	
	graph.add_input("effector.object", "RNAPOINTER");
	graph.add_input("effector.position", "FLOAT3");
	graph.add_input("effector.velocity", "FLOAT3");
	
	float zero[3] = {0.0f, 0.0f, 0.0f};
	graph.add_output("force", "FLOAT3", zero);
	graph.add_output("impulse", "FLOAT3", zero);
}

struct BVMFunction *BVM_gen_forcefield_function(bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_forcefield_graph(graph);
	parse_py_nodes(btree, &graph);
	graph.finalize();
	
	BVMCompiler compiler;
	Function *fn = compiler.compile_function(graph);
	Function::retain(fn);
	
	return (BVMFunction *)fn;
}

void BVM_debug_forcefield_nodes(bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_forcefield_graph(graph);
	parse_py_nodes(btree, &graph);
	if (mode != BVM_DEBUG_NODES_UNOPTIMIZED)
		graph.finalize();
	
	switch (mode) {
		case BVM_DEBUG_NODES:
		case BVM_DEBUG_NODES_UNOPTIMIZED: {
			debug::NodeGraphDumper dumper(debug_file);
			dumper.dump_graph(&graph, "Force Field Graph");
			break;
		}
		case BVM_DEBUG_CODEGEN: {
			DebugGraphvizCompiler compiler;
			compiler.compile_function(graph, debug_file, label);
			break;
		}
	}
}

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *ctx, struct BVMFunction *fn,
                         struct Object *effob, const EffectedPoint *point, float force[3], float impulse[3])
{
	using namespace bvm;
	
	PointerRNA object_ptr;
	RNA_id_pointer_create((ID *)effob, &object_ptr);
	const void *args[] = { &object_ptr, point->loc, point->vel };
	void *results[] = { force, impulse };
	
	_FUNC(fn)->eval(_CTX(ctx), _GLOBALS(globals), args, results);
}

/* ------------------------------------------------------------------------- */

namespace bvm {

struct bNodeCompiler {
	typedef std::pair<bNode*, bNodeSocket*> bSocketPair;
	typedef std::set<InputKey> InputSet;
	typedef std::map<bSocketPair, InputSet> InputMap;
	typedef std::map<bSocketPair, OutputKey> OutputMap;
	
	bNodeCompiler(NodeGraph *graph) :
	    m_graph(graph),
	    m_current_bnode(NULL)
	{
	}
	
	~bNodeCompiler()
	{
	}
	
	bNode *current_node() const { return m_current_bnode; }
	void set_current_node(bNode *node) { m_current_bnode = node; }
	
	const InputMap &input_map() const { return m_input_map; }
	const OutputMap &output_map() const { return m_output_map; }
	
	NodeInstance *add_node(const string &type,  const string &name = "")
	{
		return m_graph->add_node(type, name);
	}
	
	OutputKey add_input_proxy(int index)
	{
		bNodeSocket *bsock = (bNodeSocket *)BLI_findlink(&m_current_bnode->inputs, index);
		assert(bsock != NULL);
		
		NodeInstance *node = NULL;
		switch (bsock->type) {
			case SOCK_FLOAT: node = add_node("PASS_FLOAT"); break;
			case SOCK_INT: node = add_node("PASS_INT"); break;
			case SOCK_VECTOR: node = add_node("PASS_FLOAT3"); break;
			case SOCK_RGBA: node = add_node("PASS_FLOAT4"); break;
		}
		map_input_socket(index, node->input("value"));
		return node->output("value");
	}
	
	void map_input_socket(int bindex, const InputKey &socket)
	{
		bNodeSocket *binput = (bNodeSocket *)BLI_findlink(&m_current_bnode->inputs, bindex);
		NodeInstance *node = socket.node;
		const string &name = socket.socket->name;
		
		m_input_map[bSocketPair(m_current_bnode, binput)].insert(socket);
		
		switch (binput->type) {
			case SOCK_FLOAT: {
				bNodeSocketValueFloat *bvalue = (bNodeSocketValueFloat *)binput->default_value;
				node->input_value_set(name, bvalue->value);
				break;
			}
			case SOCK_VECTOR: {
				bNodeSocketValueVector *bvalue = (bNodeSocketValueVector *)binput->default_value;
				node->input_value_set(name, float3(bvalue->value[0], bvalue->value[1], bvalue->value[2]));
				break;
			}
			case SOCK_INT: {
				bNodeSocketValueInt *bvalue = (bNodeSocketValueInt *)binput->default_value;
				node->input_value_set(name, bvalue->value);
				break;
			}
			case SOCK_RGBA: {
				bNodeSocketValueRGBA *bvalue = (bNodeSocketValueRGBA *)binput->default_value;
				node->input_value_set(name, float4(bvalue->value[0], bvalue->value[1], bvalue->value[2], bvalue->value[3]));
				break;
			}
		}
	}
	
	void map_output_socket(int bindex, const OutputKey &socket)
	{
		bNodeSocket *boutput = (bNodeSocket *)BLI_findlink(&m_current_bnode->outputs, bindex);
		
		m_output_map[bSocketPair(m_current_bnode, boutput)] = socket;
	}
	
	void map_all_sockets(NodeInstance *node)
	{
		bNodeSocket *bsock;
		int i;
		for (bsock = (bNodeSocket *)m_current_bnode->inputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
			const NodeInput *input = node->type->find_input(i);
			map_input_socket(i, InputKey(node, input->name));
		}
		for (bsock = (bNodeSocket *)m_current_bnode->outputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
			const NodeOutput *output = node->type->find_output(i);
			map_output_socket(i, OutputKey(node, output->name));
		}
	}
	
	OutputKey get_graph_input(const string &name)
	{
		return m_graph->get_input(name)->key;
	}
	
	void set_graph_output(const string &name, const OutputKey &key)
	{
		m_graph->set_output_socket(name, key);
	}
	
	void add_link(bNodeLink *blink)
	{
		OutputMap::const_iterator it_from = m_output_map.find(bSocketPair(blink->fromnode, blink->fromsock));
		InputMap::const_iterator it_to_set = m_input_map.find(bSocketPair(blink->tonode, blink->tosock));
		if (it_from != m_output_map.end() && it_to_set != m_input_map.end()) {
			const OutputKey &from_pair = it_from->second;
			const InputSet &to_set = it_to_set->second;
			
			for (InputSet::const_iterator it_to = to_set.begin(); it_to != to_set.end(); ++it_to) {
				const InputKey &to_pair = *it_to;
				
				to_pair.node->link_set(to_pair.socket->name, from_pair);
			}
		}
		else {
			// TODO better error handling, some exceptions here may be ok
			printf("Cannot map link from %s:%s to %s:%s\n",
			       blink->fromnode->name, blink->fromsock->name,
			       blink->tonode->name, blink->tosock->name);
		}
	}
	
	void add_link_intern(const OutputKey &from,
	                     const InputKey &to)
	{
		to.node->link_set(to.socket->name, from);
	}
	
	/* --------------------------------------------------------------------- */
	
	OutputKey node_value_fl(float v)
	{
		NodeInstance *node = add_node("PASS_FLOAT");
		node->input_value_set("value", v);
		return node->output("value");
	}
	
	OutputKey node_value_v3(float3 v)
	{
		NodeInstance *node = add_node("PASS_FLOAT3");
		node->input_value_set("value", v);
		return node->output("value");
	}
	
	OutputKey node_one_minus_fl(const OutputKey &a)
	{
		NodeInstance *node = add_node("SUB_FLOAT");
		node->input_value_set("value_a", 1.0f);
		add_link_intern(a, node->input("value_b"));
		return node->output("value");
	}
	
	OutputKey node_one_minus_v3(const OutputKey &a)
	{
		NodeInstance *node = add_node("SUB_FLOAT3");
		node->input_value_set("value_a", float3(1.0f, 1.0f, 1.0f));
		add_link_intern(a, node->input("value_b"));
		return node->output("value");
	}
	
	OutputKey node_math_binary(const char *mode, const OutputKey &a, const OutputKey &b)
	{
		NodeInstance *node = add_node(mode);
		add_link_intern(a, node->input("value_a"));
		add_link_intern(b, node->input("value_b"));
		return node->output("value");
	}
	
	OutputKey node_math_unary(const char *mode, const OutputKey &a)
	{
		NodeInstance *node = add_node(mode);
		add_link_intern(a, node->input("value"));
		return node->output("value");
	}
	
	OutputKey node_mul_v3_fl(const OutputKey &a, const OutputKey &b)
	{
		NodeInstance *node = add_node("MUL_FLOAT3_FLOAT");
		add_link_intern(a, node->input("value_a"));
		add_link_intern(b, node->input("value_b"));
		return node->output("value");
	}
	
	OutputKey node_blend(const OutputKey &a, const OutputKey &b, const OutputKey &fac)
	{
		OutputKey facinv = node_one_minus_fl(fac);
		OutputKey mul_a = node_mul_v3_fl(a, facinv);
		OutputKey mul_b = node_mul_v3_fl(b, fac);
		return node_math_binary("ADD_FLOAT3", mul_a, mul_b);
	}
	
	OutputKey node_compose_v4(const OutputKey &x, const OutputKey &y, const OutputKey &z, const OutputKey &w)
	{
		NodeInstance *node = add_node("SET_FLOAT4");
		add_link_intern(x, node->input("value_x"));
		add_link_intern(y, node->input("value_y"));
		add_link_intern(z, node->input("value_z"));
		add_link_intern(w, node->input("value_w"));
		return node->output("value");
	}
	
	void node_decompose_v4(const OutputKey &v, OutputKey *x, OutputKey *y, OutputKey *z, OutputKey *w)
	{
		NodeInstance *node = add_node("GET_ELEM_FLOAT4");
		add_link_intern(v, node->input("value"));
		if (x) *x = node->output("value_x");
		if (y) *y = node->output("value_y");
		if (z) *z = node->output("value_z");
		if (w) *w = node->output("value_w");
	}
	
private:
	NodeGraph *m_graph;
	bNode *m_current_bnode;
	
	InputMap m_input_map;
	OutputMap m_output_map;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:bNodeCompiler")
};


static void binary_math_node(bNodeCompiler *comp, const string &type)
{
	NodeInstance *node = comp->add_node(type);
	comp->map_input_socket(0, InputKey(node, "value_a"));
	comp->map_input_socket(1, InputKey(node, "value_b"));
	comp->map_output_socket(0, OutputKey(node, "value"));
}

static void unary_math_node(bNodeCompiler *comp, const string &type)
{
	NodeInstance *node = comp->add_node(type);
	bNodeSocket *sock0 = (bNodeSocket *)BLI_findlink(&comp->current_node()->inputs, 0);
	bNodeSocket *sock1 = (bNodeSocket *)BLI_findlink(&comp->current_node()->inputs, 1);
	bool sock0_linked = !nodeSocketIsHidden(sock0) && (sock0->flag & SOCK_IN_USE);
	bool sock1_linked = !nodeSocketIsHidden(sock1) && (sock1->flag & SOCK_IN_USE);
	if (sock0_linked || !sock1_linked)
		comp->map_input_socket(0, InputKey(node, "value"));
	else
		comp->map_input_socket(1, InputKey(node, "value"));
	comp->map_output_socket(0, OutputKey(node, "value"));
}

static void convert_tex_node(bNodeCompiler *comp, PointerRNA *bnode_ptr)
{
	bNode *bnode = (bNode *)bnode_ptr->data;
	string type = string(bnode->typeinfo->idname);
	if (type == "TextureNodeOutput") {
		comp->set_graph_output("color", comp->add_input_proxy(0));
		comp->set_graph_output("normal", comp->add_input_proxy(1));
	}
	else if (type == "TextureNodeDecompose") {
		{
			NodeInstance *node = comp->add_node("GET_ELEM_FLOAT4");
			node->input_value_set("index", 0);
			comp->map_input_socket(0, InputKey(node, "value"));
			comp->map_output_socket(0, OutputKey(node, "value"));
		}
		{
			NodeInstance *node = comp->add_node("GET_ELEM_FLOAT4");
			node->input_value_set("index", 1);
			comp->map_input_socket(0, InputKey(node, "value"));
			comp->map_output_socket(1, OutputKey(node, "value"));
		}
		{
			NodeInstance *node = comp->add_node("GET_ELEM_FLOAT4");
			node->input_value_set("index", 2);
			comp->map_input_socket(0, InputKey(node, "value"));
			comp->map_output_socket(2, OutputKey(node, "value"));
		}
		{
			NodeInstance *node = comp->add_node("GET_ELEM_FLOAT4");
			node->input_value_set("index", 3);
			comp->map_input_socket(0, InputKey(node, "value"));
			comp->map_output_socket(3, OutputKey(node, "value"));
		}
	}
	else if (type == "TextureNodeCompose") {
		NodeInstance *node = comp->add_node("SET_FLOAT4");
		comp->map_input_socket(0, InputKey(node, "value_x"));
		comp->map_input_socket(1, InputKey(node, "value_y"));
		comp->map_input_socket(2, InputKey(node, "value_z"));
		comp->map_input_socket(3, InputKey(node, "value_w"));
		comp->map_output_socket(0, OutputKey(node, "value"));
	}
	else if (type == "TextureNodeCoordinates") {
		comp->map_output_socket(0, comp->get_graph_input("texture.co"));
	}
	else if (type == "TextureNodeMixRGB") {
		int mode = RNA_enum_get(bnode_ptr, "blend_type");
		bool use_alpha = RNA_boolean_get(bnode_ptr, "use_alpha");
		bool use_clamp = RNA_boolean_get(bnode_ptr, "use_clamp");
		
		OutputKey fac = comp->add_input_proxy(0);
		OutputKey col_a = comp->add_input_proxy(1);
		OutputKey col_b = comp->add_input_proxy(2);
		if (use_alpha) {
			OutputKey alpha;
			comp->node_decompose_v4(col_b, NULL, NULL, NULL, &alpha);
			fac = comp->node_math_binary("MUL_FLOAT", fac, alpha);
		}
		
		NodeInstance *node = comp->add_node("MIX_RGB");
		node->input_value_set("mode", mode);
		comp->add_link_intern(fac, node->input("factor"));
		comp->add_link_intern(col_a, node->input("color1"));
		comp->add_link_intern(col_b, node->input("color2"));
		OutputKey color = node->output("color");
		
		if (use_clamp) {
			// TODO
		}
		
		comp->map_output_socket(0, color);
		
		/* TODO converting the blend function into atomic nodes
		 * should be done eventually
		 */
#if 0
		SocketPair fac = comp->add_input_proxy(0);
		SocketPair col_a = comp->add_input_proxy(1);
		SocketPair col_b = comp->add_input_proxy(2);
		SocketPair result;
		
		switch (mode) {
			case MA_RAMP_BLEND: {
				result = comp->node_blend(col_a, col_b, fac);
				break;
			}
			case MA_RAMP_ADD: {
				SocketPair add = comp->node_math_binary("ADD_FLOAT3", col_a, col_b);
				result = comp->node_blend(col_a, add, fac);
				break;
			}
			case MA_RAMP_MULT: {
				SocketPair mul = comp->node_math_binary("MUL_FLOAT3", col_a, col_b);
				result = comp->node_blend(col_a, mul, fac);
				break;
			}
			case MA_RAMP_SCREEN: {
				SocketPair facinv = comp->node_one_minus_fl(fac);
				SocketPair inv_a = comp->node_one_minus_v3(col_a);
				SocketPair inv_b = comp->node_one_minus_v3(col_a);
				SocketPair screen = comp->node_math_binary("ADD_FLOAT3", facinv, comp->node_mul_v3_fl(inv_b, fac));
				SocketPair col = comp->node_math_binary("MUL_FLOAT3", screen, inv_a);
				result = comp->node_one_minus_v3(col);
				break;
			}
			case MA_RAMP_OVERLAY:
			case MA_RAMP_SUB:
			case MA_RAMP_DIV:
			case MA_RAMP_DIFF:
			case MA_RAMP_DARK:
			case MA_RAMP_LIGHT:
			case MA_RAMP_DODGE:
			case MA_RAMP_BURN:
			case MA_RAMP_HUE:
			case MA_RAMP_SAT:
			case MA_RAMP_VAL:
			case MA_RAMP_COLOR:
			case MA_RAMP_SOFT:
			case MA_RAMP_LINEAR:
		}
		
		if (result) {
			comp->map_output_socket(0, result);
		}
#endif
	}
	else if (type == "TextureNodeMath") {
		int mode = RNA_enum_get(bnode_ptr, "operation");
		switch (mode) {
			case 0: binary_math_node(comp, "ADD_FLOAT"); break;
			case 1: binary_math_node(comp, "SUB_FLOAT"); break;
			case 2: binary_math_node(comp, "MUL_FLOAT"); break;
			case 3: binary_math_node(comp, "DIV_FLOAT"); break;
			case 4: unary_math_node(comp, "SINE"); break;
			case 5: unary_math_node(comp, "COSINE"); break;
			case 6: unary_math_node(comp, "TANGENT"); break;
			case 7: unary_math_node(comp, "ARCSINE"); break;
			case 8: unary_math_node(comp, "ARCCOSINE"); break;
			case 9: unary_math_node(comp, "ARCTANGENT"); break;
			case 10: binary_math_node(comp, "POWER"); break;
			case 11: binary_math_node(comp, "LOGARITHM"); break;
			case 12: binary_math_node(comp, "MINIMUM"); break;
			case 13: binary_math_node(comp, "MAXIMUM"); break;
			case 14: unary_math_node(comp, "ROUND"); break;
			case 15: binary_math_node(comp, "LESS_THAN"); break;
			case 16: binary_math_node(comp, "GREATER_THAN"); break;
			case 17: binary_math_node(comp, "MODULO"); break;
			case 18: unary_math_node(comp, "ABSOLUTE"); break;
			case 20: unary_math_node(comp, "SQRT"); break;
		}
	}
	else if (type == "TextureNodeTexVoronoi") {
		Tex *tex = (Tex *)bnode->storage;
		
		NodeInstance *node = comp->add_node("TEX_PROC_VORONOI");
		node->input_value_set("distance_metric", (int)tex->vn_distm);
		node->input_value_set("color_type", (int)tex->vn_coltype);
		node->input_value_set("minkowski_exponent", 2.5f);
		node->input_value_set("nabla", 0.05f);
		
		comp->map_input_socket(0, InputKey(node, "position"));
		comp->map_input_socket(3, InputKey(node, "w1"));
		comp->map_input_socket(4, InputKey(node, "w2"));
		comp->map_input_socket(5, InputKey(node, "w3"));
		comp->map_input_socket(6, InputKey(node, "w4"));
		comp->map_input_socket(7, InputKey(node, "scale"));
		comp->map_input_socket(8, InputKey(node, "noise_size"));
		
		comp->map_output_socket(0, OutputKey(node, "color"));
		comp->map_output_socket(1, OutputKey(node, "normal"));
	}
	else if (type == "TextureNodeTexClouds") {
		Tex *tex = (Tex *)bnode->storage;
		
		NodeInstance *node = comp->add_node("TEX_PROC_CLOUDS");
		node->input_value_set("depth", (int)tex->noisedepth);
		node->input_value_set("noise_basis", (int)tex->noisebasis);
		node->input_value_set("noise_hard", (int)(tex->noisetype != TEX_NOISESOFT));
		node->input_value_set("nabla", 0.05f);
		
		comp->map_input_socket(0, InputKey(node, "position"));
		comp->map_input_socket(3, InputKey(node, "size"));
		
		comp->map_output_socket(0, OutputKey(node, "color"));
		comp->map_output_socket(1, OutputKey(node, "normal"));
	}
}

} /* namespace bvm */

static void parse_tex_nodes(bNodeTree *btree, bvm::NodeGraph *graph)
{
	using namespace bvm;
	
	bNodeCompiler comp(graph);
	
	for (bNode *bnode = (bNode*)btree->nodes.first; bnode; bnode = bnode->next) {
		PointerRNA ptr;
		RNA_pointer_create((ID *)btree, &RNA_Node, bnode, &ptr);
		
		BLI_assert(bnode->typeinfo != NULL);
		if (!nodeIsRegistered(bnode))
			continue;
		
		comp.set_current_node(bnode);
		convert_tex_node(&comp, &ptr);
	}
	
	for (bNodeLink *blink = (bNodeLink *)btree->links.first; blink; blink = blink->next) {
		if (!(blink->flag & NODE_LINK_VALID))
			continue;
		
		comp.add_link(blink);
	}
}

static void init_texture_graph(bvm::NodeGraph &graph)
{
	using namespace bvm;
	
	graph.add_input("texture.co", "FLOAT3");
	graph.add_input("texture.dxt", "FLOAT3");
	graph.add_input("texture.dyt", "FLOAT3");
	graph.add_input("texture.cfra", "INT");
	graph.add_input("texture.osatex", "INT");
	
	float C[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	float N[3] = {0.0f, 0.0f, 0.0f};
	graph.add_output("color", "FLOAT4", C);
	graph.add_output("normal", "FLOAT3", N);
}

struct BVMFunction *BVM_gen_texture_function(bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_texture_graph(graph);
	parse_tex_nodes(btree, &graph);
	graph.finalize();
	
	BVMCompiler compiler;
	Function *fn = compiler.compile_function(graph);
	Function::retain(fn);
	
	return (BVMFunction *)fn;
}

void BVM_debug_texture_nodes(bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_texture_graph(graph);
	parse_tex_nodes(btree, &graph);
	if (mode != BVM_DEBUG_NODES_UNOPTIMIZED)
		graph.finalize();
	
	switch (mode) {
		case BVM_DEBUG_NODES:
		case BVM_DEBUG_NODES_UNOPTIMIZED: {
			debug::NodeGraphDumper dumper(debug_file);
			dumper.dump_graph(&graph, "Texture Node Graph");
			break;
		}
		case BVM_DEBUG_CODEGEN: {
			DebugGraphvizCompiler compiler;
			compiler.compile_function(graph, debug_file, label);
			break;
		}
	}
}

void BVM_eval_texture(struct BVMEvalContext *ctx, struct BVMFunction *fn,
                      struct TexResult *target,
                      float coord[3], float dxt[3], float dyt[3], int osatex,
                      short which_output, int cfra, int UNUSED(preview))
{
	using namespace bvm;
	
	EvalGlobals globals;
	
	float4 color;
	float3 normal;
	const void *args[] = { coord, dxt, dyt, &cfra, &osatex };
	void *results[] = { &color.x, &normal.x };
	
	_FUNC(fn)->eval(_CTX(ctx), &globals, args, results);
	
	target->tr = color.x;
	target->tg = color.y;
	target->tb = color.z;
	target->ta = color.w;
	
	target->tin = (target->tr + target->tg + target->tb) / 3.0f;
	target->talpha = true;
	
	if (target->nor) {
		target->nor[0] = normal.x;
		target->nor[1] = normal.y;
		target->nor[2] = normal.z;
	}
}

/* ------------------------------------------------------------------------- */

static void init_modifier_graph(bvm::NodeGraph &graph)
{
	using namespace bvm;
	
	graph.add_input("modifier.object", "RNAPOINTER");
	graph.add_input("modifier.base_mesh", "RNAPOINTER");
	graph.add_output("mesh", "MESH", __empty_mesh__);
}

struct BVMFunction *BVM_gen_modifier_function(struct bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_modifier_graph(graph);
	parse_py_nodes(btree, &graph);
	graph.finalize();
	
	BVMCompiler compiler;
	Function *fn = compiler.compile_function(graph);
	Function::retain(fn);
	
	return (BVMFunction *)fn;
}

void BVM_debug_modifier_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_modifier_graph(graph);
	parse_py_nodes(btree, &graph);
	if (mode != BVM_DEBUG_NODES_UNOPTIMIZED)
		graph.finalize();
	
	switch (mode) {
		case BVM_DEBUG_NODES:
		case BVM_DEBUG_NODES_UNOPTIMIZED: {
			debug::NodeGraphDumper dumper(debug_file);
			dumper.dump_graph(&graph, "Modifier Node Graph");
			break;
		}
		case BVM_DEBUG_CODEGEN: {
			DebugGraphvizCompiler compiler;
			compiler.compile_function(graph, debug_file, label);
			break;
		}
	}
}

struct DerivedMesh *BVM_eval_modifier(struct BVMEvalGlobals *globals,
                                      struct BVMEvalContext *ctx,
                                      struct BVMFunction *fn,
                                      struct Object *object,
                                      struct Mesh *base_mesh)
{
	using namespace bvm;

	PointerRNA object_ptr, base_mesh_ptr;
	RNA_id_pointer_create((ID *)object, &object_ptr);
	RNA_id_pointer_create((ID *)base_mesh, &base_mesh_ptr);
	int iteration = 0;
	int elem_index = 0;
	float3 elem_loc(0.0f, 0.0f, 0.0f);
	mesh_ptr result;
	const void *args[] = { &iteration, &elem_index, &elem_loc, &object_ptr, &base_mesh_ptr };
	void *results[] = { &result };
	
	_FUNC(fn)->eval(_CTX(ctx), _GLOBALS(globals), args, results);
	
	DerivedMesh *dm = result.get();
	/* destroy the pointer variable */
	result.ptr().reset();
	return dm;
}

/* ------------------------------------------------------------------------- */

static void init_dupli_graph(bvm::NodeGraph &graph)
{
	using namespace bvm;
	
	graph.add_input("dupli.object", "RNAPOINTER");
	graph.add_output("dupli.result", "DUPLIS", __empty_duplilist__);
}

struct BVMFunction *BVM_gen_dupli_function(struct bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_dupli_graph(graph);
	parse_py_nodes(btree, &graph);
	graph.finalize();
	
	BVMCompiler compiler;
	Function *fn = compiler.compile_function(graph);
	Function::retain(fn);
	
	return (BVMFunction *)fn;
}

void BVM_debug_dupli_nodes(struct bNodeTree *btree, FILE *debug_file, const char *label, BVMDebugMode mode)
{
	using namespace bvm;
	
	NodeGraph graph;
	init_dupli_graph(graph);
	
	parse_py_nodes(btree, &graph);
	if (mode != BVM_DEBUG_NODES_UNOPTIMIZED)
		graph.finalize();
	
	switch (mode) {
		case BVM_DEBUG_NODES:
		case BVM_DEBUG_NODES_UNOPTIMIZED: {
			debug::NodeGraphDumper dumper(debug_file);
			dumper.dump_graph(&graph, "Dupli Node Graph");
			break;
		}
		case BVM_DEBUG_CODEGEN: {
			DebugGraphvizCompiler compiler;
			compiler.compile_function(graph, debug_file, label);
			break;
		}
	}
}

void BVM_eval_dupli(struct BVMEvalGlobals *globals,
                    struct BVMEvalContext *ctx,
                    struct BVMFunction *fn,
                    struct Object *object,
                    struct DupliContainer *duplicont)
{
	using namespace bvm;

	PointerRNA object_ptr;
	RNA_id_pointer_create((ID *)object, &object_ptr);
	const void *args[] = { &object_ptr };
	
	duplis_ptr result;
	void *results[] = { &result };
	
	_FUNC(fn)->eval(_CTX(ctx), _GLOBALS(globals), args, results);
	
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
