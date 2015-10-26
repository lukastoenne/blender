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

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_effect.h"
#include "BKE_node.h"

#include "BVM_api.h"

#include "RNA_access.h"
}

#include "bvm_codegen.h"
#include "bvm_eval.h"
#include "bvm_expression.h"
#include "bvm_function.h"
#include "bvm_module.h"
#include "bvm_nodegraph.h"

void BVM_init(void)
{
	bvm::register_opcode_node_types();
}

void BVM_free(void)
{
}

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::Module *_MOD(struct BVMModule *mod)
{ return (bvm::Module *)mod; }

struct BVMModule *BVM_module_create(void)
{ return (struct BVMModule *)(new bvm::Module()); }

void BVM_module_free(BVMModule *mod)
{ delete _MOD(mod); }

struct BVMFunction *BVM_module_create_function(BVMModule *mod, const char *name)
{ return (struct BVMFunction *)_MOD(mod)->create_function(name); }

bool BVM_module_delete_function(BVMModule *mod, const char *name)
{ return _MOD(mod)->remove_function(name); }

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::Expression *_EXPR(struct BVMExpression *expr)
{ return (bvm::Expression *)expr; }

void BVM_expression_free(struct BVMExpression *expr)
{ delete _EXPR(expr); }

/* ------------------------------------------------------------------------- */

BLI_INLINE bvm::NodeGraph *_GRAPH(struct BVMNodeGraph *graph)
{ return (bvm::NodeGraph *)graph; }
BLI_INLINE bvm::NodeInstance *_NODE(struct BVMNodeInstance *node)
{ return (bvm::NodeInstance *)node; }

struct BVMNodeInstance *BVM_nodegraph_add_node(BVMNodeGraph *graph, const char *type, const char *name)
{ return (struct BVMNodeInstance *)_GRAPH(graph)->add_node(type, name); }

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

void BVM_globals_add_object(struct BVMEvalGlobals *globals, struct Object *ob)
{ _GLOBALS(globals)->objects.push_back(ob); }

struct BVMEvalContext *BVM_context_create(void)
{ return (BVMEvalContext *)(new bvm::EvalContext()); }

void BVM_context_free(struct BVMEvalContext *ctx)
{ delete _CTX(ctx); }

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *ctx, struct BVMExpression *expr,
                         const EffectedPoint *point, float force[3], float impulse[3])
{
	bvm::EvalData data;
	data.effector.position = bvm::float3(point->loc[0], point->loc[1], point->loc[2]);
	data.effector.velocity = bvm::float3(point->vel[0], point->vel[1], point->vel[2]);
	void *results[] = { force, impulse };
	
	_CTX(ctx)->eval_expression(_GLOBALS(globals), &data, _EXPR(expr), results);
}

/* ------------------------------------------------------------------------- */

typedef std::pair<bNode*, bNodeSocket*> bSocketPair;
typedef std::pair<bvm::NodeInstance*, bvm::string> SocketPair;
typedef std::set<SocketPair> SocketSet;
typedef std::map<bSocketPair, SocketSet> InputMap;
typedef std::map<bSocketPair, SocketPair> OutputMap;

typedef std::map<struct Object *, int> ObjectPtrMap;

static void map_input_socket(InputMap &input_map, bNode *bnode, int bindex, bvm::NodeInstance *node, const bvm::string &name)
{
	bNodeSocket *binput = (bNodeSocket *)BLI_findlink(&bnode->inputs, bindex);
	
	input_map[bSocketPair(bnode, binput)].insert(SocketPair(node, name));
	
	switch (binput->type) {
		case SOCK_FLOAT: {
			bNodeSocketValueFloat *bvalue = (bNodeSocketValueFloat *)binput->default_value;
			node->set_input_value(name, bvalue->value);
			break;
		}
		case SOCK_VECTOR: {
			bNodeSocketValueVector *bvalue = (bNodeSocketValueVector *)binput->default_value;
			node->set_input_value(name, bvm::float3(bvalue->value[0], bvalue->value[1], bvalue->value[2]));
			break;
		}
	}
}

static void map_output_socket(OutputMap &output_map,
                              bNode *bnode, int bindex,
                              bvm::NodeInstance *node, const bvm::string &name)
{
	bNodeSocket *boutput = (bNodeSocket *)BLI_findlink(&bnode->outputs, bindex);
	
	output_map[bSocketPair(bnode, boutput)] = SocketPair(node, name);
}

static void map_all_sockets(InputMap &input_map, OutputMap &output_map,
                            bNode *bnode, bvm::NodeInstance *node)
{
	bNodeSocket *bsock;
	int i;
	for (bsock = (bNodeSocket *)bnode->inputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
		const bvm::NodeSocket *input = node->type->find_input(i);
		map_input_socket(input_map, bnode, i, node, input->name);
	}
	for (bsock = (bNodeSocket *)bnode->outputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
		const bvm::NodeSocket *output = node->type->find_output(i);
		map_output_socket(output_map, bnode, i, node, output->name);
	}
}

static void binary_math_node(bvm::NodeGraph &graph, InputMap &input_map, OutputMap &output_map,
                             bNode *bnode, const bvm::string &type)
{
	bvm::NodeInstance *node = graph.add_node(type, bnode->name);
	map_input_socket(input_map, bnode, 0, node, "value_a");
	map_input_socket(input_map, bnode, 1, node, "value_b");
	map_output_socket(output_map, bnode, 0, node, "value");
}

static void unary_math_node(bvm::NodeGraph &graph, InputMap &input_map, OutputMap &output_map,
                            bNode *bnode, const bvm::string &type)
{
	bvm::NodeInstance *node = graph.add_node(type, bnode->name);
	bNodeSocket *sock0 = (bNodeSocket *)BLI_findlink(&bnode->inputs, 0);
	bNodeSocket *sock1 = (bNodeSocket *)BLI_findlink(&bnode->inputs, 1);
	bool sock0_linked = !nodeSocketIsHidden(sock0) && (sock0->flag & SOCK_IN_USE);
	bool sock1_linked = !nodeSocketIsHidden(sock1) && (sock1->flag & SOCK_IN_USE);
	if (sock0_linked || !sock1_linked)
		map_input_socket(input_map, bnode, 0, node, "value");
	else
		map_input_socket(input_map, bnode, 1, node, "value");
	map_output_socket(output_map, bnode, 0, node, "value");
}

static void gen_forcefield_nodegraph(Object *effob, bNodeTree *btree, bvm::NodeGraph &graph, const ObjectPtrMap &obmap)
{
	{
		float zero[3] = {0.0f, 0.0f, 0.0f};
		graph.add_output("force", BVM_FLOAT3, zero);
		graph.add_output("impulse", BVM_FLOAT3, zero);
	}
	
	/* maps bNodeTree sockets to internal sockets, for converting links */
	InputMap input_map;
	OutputMap output_map;
	
	for (bNode *bnode = (bNode*)btree->nodes.first; bnode; bnode = bnode->next) {
		PointerRNA ptr;
		RNA_pointer_create((ID *)btree, &RNA_Node, bnode, &ptr);
		
		BLI_assert(bnode->typeinfo != NULL);
		if (!nodeIsRegistered(bnode))
			continue;
		
		bvm::string type = bvm::string(bnode->typeinfo->idname);
		if (type == "ForceOutputNode") {
			{
				bvm::NodeInstance *node = graph.add_node("PASS_FLOAT3", "RET_FORCE_" + bvm::string(bnode->name));
				map_input_socket(input_map, bnode, 0, node, "value");
				map_output_socket(output_map, bnode, 0, node, "value");
				
				graph.set_output_link("force", node, "value");
			}
			
			{
				bvm::NodeInstance *node = graph.add_node("PASS_FLOAT3", "RET_IMPULSE_" + bvm::string(bnode->name));
				map_input_socket(input_map, bnode, 1, node, "value");
				map_output_socket(output_map, bnode, 0, node, "value");
				
				graph.set_output_link("impulse", node, "value");
			}
		}
		else if (type == "ObjectSeparateVectorNode") {
			{
				bvm::NodeInstance *node = graph.add_node("GET_ELEM_FLOAT3", "GET_ELEM0_" + bvm::string(bnode->name));
				node->set_input_value("index", 0);
				map_input_socket(input_map, bnode, 0, node, "value");
				map_output_socket(output_map, bnode, 0, node, "value");
			}
			{
				bvm::NodeInstance *node = graph.add_node("GET_ELEM_FLOAT3", "GET_ELEM1_" + bvm::string(bnode->name));
				node->set_input_value("index", 1);
				map_input_socket(input_map, bnode, 0, node, "value");
				map_output_socket(output_map, bnode, 1, node, "value");
			}
			{
				bvm::NodeInstance *node = graph.add_node("GET_ELEM_FLOAT3", "GET_ELEM2_" + bvm::string(bnode->name));
				node->set_input_value("index", 2);
				map_input_socket(input_map, bnode, 0, node, "value");
				map_output_socket(output_map, bnode, 2, node, "value");
			}
		}
		else if (type == "ObjectCombineVectorNode") {
			bvm::NodeInstance *node = graph.add_node("SET_FLOAT3", bvm::string(bnode->name));
			map_input_socket(input_map, bnode, 0, node, "value_x");
			map_input_socket(input_map, bnode, 1, node, "value_y");
			map_input_socket(input_map, bnode, 2, node, "value_z");
			map_output_socket(output_map, bnode, 0, node, "value");
		}
		else if (type == "ForceEffectorDataNode") {
			{
				bvm::NodeInstance *node = graph.add_node("EFFECTOR_POSITION", "EFFECTOR_POS" + bvm::string(bnode->name));
				map_output_socket(output_map, bnode, 0, node, "value");
			}
			{
				bvm::NodeInstance *node = graph.add_node("EFFECTOR_VELOCITY", "EFFECTOR_VEL" + bvm::string(bnode->name));
				map_output_socket(output_map, bnode, 1, node, "value");
			}
		}
		else if (type == "ObjectMathNode") {
			int mode = RNA_enum_get(&ptr, "mode");
			switch (mode) {
				case 0: binary_math_node(graph, input_map, output_map, bnode, "ADD_FLOAT"); break;
				case 1: binary_math_node(graph, input_map, output_map, bnode, "SUB_FLOAT"); break;
				case 2: binary_math_node(graph, input_map, output_map, bnode, "MUL_FLOAT"); break;
				case 3: binary_math_node(graph, input_map, output_map, bnode, "DIV_FLOAT"); break;
				case 4: unary_math_node(graph, input_map, output_map, bnode, "SINE"); break;
				case 5: unary_math_node(graph, input_map, output_map, bnode, "COSINE"); break;
				case 6: unary_math_node(graph, input_map, output_map, bnode, "TANGENT"); break;
				case 7: unary_math_node(graph, input_map, output_map, bnode, "ARCSINE"); break;
				case 8: unary_math_node(graph, input_map, output_map, bnode, "ARCCOSINE"); break;
				case 9: unary_math_node(graph, input_map, output_map, bnode, "ARCTANGENT"); break;
				case 10: binary_math_node(graph, input_map, output_map, bnode, "POWER"); break;
				case 11: binary_math_node(graph, input_map, output_map, bnode, "LOGARITHM"); break;
				case 12: binary_math_node(graph, input_map, output_map, bnode, "MINIMUM"); break;
				case 13: binary_math_node(graph, input_map, output_map, bnode, "MAXIMUM"); break;
				case 14: unary_math_node(graph, input_map, output_map, bnode, "ROUND"); break;
				case 15: binary_math_node(graph, input_map, output_map, bnode, "LESS_THAN"); break;
				case 16: binary_math_node(graph, input_map, output_map, bnode, "GREATER_THAN"); break;
				case 17: binary_math_node(graph, input_map, output_map, bnode, "MODULO"); break;
				case 18: unary_math_node(graph, input_map, output_map, bnode, "ABSOLUTE"); break;
				case 19: unary_math_node(graph, input_map, output_map, bnode, "CLAMP"); break;
			}
		}
		else if (type == "ObjectVectorMathNode") {
			int mode = RNA_enum_get(&ptr, "mode");
			switch (mode) {
				case 0: {
					bvm::NodeInstance *node = graph.add_node("ADD_FLOAT3", bnode->name);
					map_input_socket(input_map, bnode, 0, node, "value_a");
					map_input_socket(input_map, bnode, 1, node, "value_b");
					map_output_socket(output_map, bnode, 0, node, "value");
					break;
				}
				case 1: {
					bvm::NodeInstance *node = graph.add_node("SUB_FLOAT3", bnode->name);
					map_input_socket(input_map, bnode, 0, node, "value_a");
					map_input_socket(input_map, bnode, 1, node, "value_b");
					map_output_socket(output_map, bnode, 0, node, "value");
					break;
				}
			}
		}
		else if (type == "ForceClosestPointNode") {
			bvm::NodeInstance *node = graph.add_node("EFFECTOR_CLOSEST_POINT", bnode->name);
			Object *object = effob;
			ObjectPtrMap::const_iterator it = obmap.find(object);
			if (it != obmap.end()) {
				int object_index = it->second;
				node->set_input_value("object", object_index);
				map_input_socket(input_map, bnode, 0, node, "vector");
				map_output_socket(output_map, bnode, 0, node, "position");
				map_output_socket(output_map, bnode, 1, node, "normal");
				map_output_socket(output_map, bnode, 2, node, "tangent");
			}
		}
	}
	
	for (bNodeLink *blink = (bNodeLink *)btree->links.first; blink; blink = blink->next) {
		if (!(blink->flag & NODE_LINK_VALID))
			continue;
		
		OutputMap::const_iterator it_from = output_map.find(bSocketPair(blink->fromnode, blink->fromsock));
		InputMap::const_iterator it_to_set = input_map.find(bSocketPair(blink->tonode, blink->tosock));
		if (it_from != output_map.end() && it_to_set != input_map.end()) {
			const SocketPair &from_pair = it_from->second;
			const SocketSet &to_set = it_to_set->second;
			
			for (SocketSet::const_iterator it_to = to_set.begin(); it_to != to_set.end(); ++it_to) {
				const SocketPair &to_pair = *it_to;
				
				graph.add_link(from_pair.first, from_pair.second,
				               to_pair.first, to_pair.second);
			}
		}
		else {
			// TODO better error handling, some exceptions here may be ok
			printf("Cannot map link from %s:%s to %s:%s\n",
			       blink->fromnode->name, blink->fromsock->name,
			       blink->tonode->name, blink->tosock->name);
		}
	}
}

static void create_object_ptr_map(ObjectPtrMap &obmap, const bvm::EvalGlobals *globals)
{
	for (int i = 0; i < globals->objects.size(); ++i) {
		obmap[globals->objects[i]] = i;
	}
}

struct BVMExpression *BVM_gen_forcefield_expression(const struct BVMEvalGlobals *globals, Object *effob, bNodeTree *btree)
{
	using namespace bvm;
	
	ObjectPtrMap obmap;
	create_object_ptr_map(obmap, _GLOBALS(globals));
	
	NodeGraph graph;
	gen_forcefield_nodegraph(effob, btree, graph, obmap);
	
	BVMCompiler compiler;
	Expression *expr = compiler.codegen_expression(graph);
	
	return (BVMExpression *)expr;
}

#undef _GRAPH

#undef _EXPR
