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

#include "RE_shader_ext.h"

#include "BVM_api.h"

#include "RNA_access.h"
}

#include "bvm_codegen.h"
#include "bvm_eval.h"
#include "bvm_expression.h"
#include "bvm_function.h"
#include "bvm_module.h"
#include "bvm_nodegraph.h"
#include "bvm_util_map.h"
#include "bvm_util_thread.h"

void BVM_init(void)
{
	bvm::register_opcode_node_types();
}

void BVM_free(void)
{
	BVM_texture_cache_clear();
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

void BVM_nodegraph_add_link(BVMNodeGraph *graph,
                            struct BVMNodeInstance *from_node, const char *from_socket,
                            struct BVMNodeInstance *to_node, const char *to_socket)
{
	_GRAPH(graph)->add_link(_NODE(from_node), bvm::string(from_socket),
	                        _NODE(to_node), bvm::string(to_socket));
}

void BVM_nodegraph_set_output_link(struct BVMNodeGraph *graph,
                                   const char *name, struct BVMNodeInstance *node, const char *socket)
{
	_GRAPH(graph)->set_output_link(name, _NODE(node), socket);
}


void BVM_node_set_input_value_float(struct BVMNodeInstance *node, const char *socket,
                                    float value)
{ _NODE(node)->set_input_value(socket, value); }

void BVM_node_set_input_value_float3(struct BVMNodeInstance *node, const char *socket,
                                     const float value[3])
{ _NODE(node)->set_input_value(socket, bvm::float3::from_data(value)); }

void BVM_node_set_input_value_float4(struct BVMNodeInstance *node, const char *socket,
                                     const float value[4])
{ _NODE(node)->set_input_value(socket, bvm::float4::from_data(value)); }

void BVM_node_set_input_value_matrix44(struct BVMNodeInstance *node, const char *socket,
                                       float value[4][4])
{ _NODE(node)->set_input_value(socket, bvm::matrix44::from_data(&value[0][0])); }

void BVM_node_set_input_value_int(struct BVMNodeInstance *node, const char *socket,
                                  int value)
{ _NODE(node)->set_input_value(socket, value); }

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

/* ------------------------------------------------------------------------- */

struct CompileContext {
	typedef std::map<struct Object *, int> ObjectPtrMap;
	
	ObjectPtrMap obmap;
	
	CompileContext(const bvm::EvalGlobals *globals)
	{
		for (int i = 0; i < globals->objects.size(); ++i)
			obmap[globals->objects[i]] = i;
	}
	
	int get_object_index(Object *ob)
	{
		ObjectPtrMap::const_iterator it = obmap.find(ob);
		if (it != obmap.end())
			return it->second;
		else
			return -1;
	}
};

inline static CompileContext *_COMP(BVMCompileContext *c)
{
	return (CompileContext *)c;
}

int BVM_compile_get_object_index(BVMCompileContext *context, Object *ob)
{
	return _COMP(context)->get_object_index(ob);
}

/* ------------------------------------------------------------------------- */

static void parse_py_nodes(CompileContext *_context, bNodeTree *btree, bvm::NodeGraph *graph)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	BVMCompileContext *context = (BVMCompileContext *)_context;
	
	RNA_id_pointer_create((ID *)btree, &ptr);
	
	func = RNA_struct_find_function(ptr.type, "bvm_compile");
	if (!func)
		return;
	
	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &context);
	RNA_parameter_set_lookup(&list, "graph", &graph);
	btree->typeinfo->ext.call(NULL, &ptr, func, &list);
	
	RNA_parameter_list_free(&list);
}

struct BVMExpression *BVM_gen_forcefield_expression(const struct BVMEvalGlobals *globals, bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	{
		float zero[3] = {0.0f, 0.0f, 0.0f};
		graph.add_output("force", BVM_FLOAT3, zero);
		graph.add_output("impulse", BVM_FLOAT3, zero);
	}
	
	CompileContext comp(_GLOBALS(globals));
	parse_py_nodes(&comp, btree, &graph);
	
	BVMCompiler compiler;
	Expression *expr = compiler.codegen_expression(graph);
	
	return (BVMExpression *)expr;
}

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *ctx, struct BVMExpression *expr,
                         struct Object *effob, const EffectedPoint *point, float force[3], float impulse[3])
{
	using namespace bvm;
	
	EvalData data;
	RNA_id_pointer_create((ID *)effob, &data.effector.object);
	data.effector.position = float3(point->loc[0], point->loc[1], point->loc[2]);
	data.effector.velocity = float3(point->vel[0], point->vel[1], point->vel[2]);
	void *results[] = { force, impulse };
	
	_CTX(ctx)->eval_expression(_GLOBALS(globals), &data, _EXPR(expr), results);
}

/* ------------------------------------------------------------------------- */

namespace bvm {

struct bNodeCompiler {
	typedef std::pair<bNode*, bNodeSocket*> bSocketPair;
	typedef std::pair<bvm::NodeInstance*, bvm::string> SocketPair;
	typedef std::set<SocketPair> SocketSet;
	typedef std::map<bSocketPair, SocketSet> InputMap;
	typedef std::map<bSocketPair, SocketPair> OutputMap;
	
	bNodeCompiler(bvm::NodeGraph *graph) :
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
	
	bvm::NodeInstance *add_node(const bvm::string &type,  const bvm::string &name)
	{
		return m_graph->add_node(type, name);
	}
	
	void map_input_socket(int bindex, bvm::NodeInstance *node, const bvm::string &name)
	{
		bNodeSocket *binput = (bNodeSocket *)BLI_findlink(&m_current_bnode->inputs, bindex);
		
		m_input_map[bSocketPair(m_current_bnode, binput)].insert(SocketPair(node, name));
		
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
			case SOCK_INT: {
				bNodeSocketValueInt *bvalue = (bNodeSocketValueInt *)binput->default_value;
				node->set_input_value(name, bvalue->value);
				break;
			}
			case SOCK_RGBA: {
				bNodeSocketValueRGBA *bvalue = (bNodeSocketValueRGBA *)binput->default_value;
				node->set_input_value(name, bvm::float4(bvalue->value[0], bvalue->value[1], bvalue->value[2], bvalue->value[3]));
				break;
			}
		}
	}
	
	void map_output_socket(int bindex, bvm::NodeInstance *node, const bvm::string &name)
	{
		bNodeSocket *boutput = (bNodeSocket *)BLI_findlink(&m_current_bnode->outputs, bindex);
		
		m_output_map[bSocketPair(m_current_bnode, boutput)] = SocketPair(node, name);
	}
	
	void set_graph_output(const bvm::string &graph_output_name, bvm::NodeInstance *node, const bvm::string &name)
	{
		m_graph->set_output_link(graph_output_name, node, name);
	}
	
	void map_all_sockets(bvm::NodeInstance *node)
	{
		bNodeSocket *bsock;
		int i;
		for (bsock = (bNodeSocket *)m_current_bnode->inputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
			const bvm::NodeSocket *input = node->type->find_input(i);
			map_input_socket(i, node, input->name);
		}
		for (bsock = (bNodeSocket *)m_current_bnode->outputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
			const bvm::NodeSocket *output = node->type->find_output(i);
			map_output_socket(i, node, output->name);
		}
	}
	
	void add_link(bNodeLink *blink)
	{
		OutputMap::const_iterator it_from = m_output_map.find(bSocketPair(blink->fromnode, blink->fromsock));
		InputMap::const_iterator it_to_set = m_input_map.find(bSocketPair(blink->tonode, blink->tosock));
		if (it_from != m_output_map.end() && it_to_set != m_input_map.end()) {
			const SocketPair &from_pair = it_from->second;
			const SocketSet &to_set = it_to_set->second;
			
			for (SocketSet::const_iterator it_to = to_set.begin(); it_to != to_set.end(); ++it_to) {
				const SocketPair &to_pair = *it_to;
				
				m_graph->add_link(from_pair.first, from_pair.second,
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
	
private:
	NodeGraph *m_graph;
	bNode *m_current_bnode;
	
	InputMap m_input_map;
	OutputMap m_output_map;
};

} /* namespace bvm */

static void convert_tex_node(bvm::bNodeCompiler *comp, PointerRNA *bnode_ptr)
{
	bNode *bnode = (bNode *)bnode_ptr->data;
	bvm::string type = bvm::string(bnode->typeinfo->idname);
	if (type == "TextureNodeOutput") {
		{
			bvm::NodeInstance *node = comp->add_node("PASS_FLOAT4", "RET_COLOR_" + bvm::string(comp->current_node()->name));
			comp->map_input_socket(0, node, "value");
			comp->map_output_socket(0, node, "value");
			
			comp->set_graph_output("color", node, "value");
		}
		
		{
			bvm::NodeInstance *node = comp->add_node("PASS_FLOAT3", "RET_NORMAL_" + bvm::string(comp->current_node()->name));
			comp->map_input_socket(1, node, "value");
			comp->map_output_socket(0, node, "value");
			
			comp->set_graph_output("normal", node, "value");
		}
	}
	else if (type == "TextureNodeCoordinates") {
		bvm::NodeInstance *node = comp->add_node("TEX_COORD", bvm::string(comp->current_node()->name));
		comp->map_output_socket(0, node, "value");
	}
	else if (type == "TextureNodeTexVoronoi") {
		Tex *tex = (Tex *)bnode->storage;
		
		bvm::NodeInstance *node_pos = comp->add_node("TEX_COORD", "TEX_VORONOI_COORD_"+bvm::string(comp->current_node()->name));
		
		bvm::NodeInstance *node = comp->add_node("TEX_PROC_VORONOI", "TEX_VORONOI_"+bvm::string(comp->current_node()->name));
		node->set_input_value("distance_metric", (int)tex->vn_distm);
		node->set_input_value("color_type", (int)tex->vn_coltype);
		node->set_input_value("minkowski_exponent", 2.5f);
		node->set_input_value("nabla", 0.05f);
		
		node->set_input_link("position", node_pos, node_pos->type->find_output(0));
		
		comp->map_input_socket(2, node, "w1");
		comp->map_input_socket(3, node, "w2");
		comp->map_input_socket(4, node, "w3");
		comp->map_input_socket(5, node, "w4");
		comp->map_input_socket(6, node, "scale");
		comp->map_input_socket(7, node, "noise_size");
		
		comp->map_output_socket(0, node, "color");
		comp->map_output_socket(1, node, "normal");
	}
}

static void parse_tex_nodes(CompileContext */*_context*/, bNodeTree *btree, bvm::NodeGraph *graph)
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


struct BVMExpression *BVM_gen_texture_expression(const struct BVMEvalGlobals *globals, struct Tex *tex, bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	{
		float C[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		float N[3] = {0.0f, 0.0f, 0.0f};
		graph.add_output("color", BVM_FLOAT4, C);
		graph.add_output("normal", BVM_FLOAT3, N);
	}
	CompileContext comp(_GLOBALS(globals));
	parse_tex_nodes(&comp, btree, &graph);
	
	BVMCompiler compiler;
	Expression *expr = compiler.codegen_expression(graph);
	
	return (BVMExpression *)expr;
}

void BVM_eval_texture(struct BVMEvalContext *ctx, struct BVMExpression *expr,
                      struct TexResult *target,
                      float coord[3], float dxt[3], float dyt[3], int osatex,
                      short which_output, int cfra, int UNUSED(preview))
{
	using namespace bvm;
	
	EvalGlobals globals;
	
	EvalData data;
	TextureEvalData &texdata = data.texture;
	texdata.co = float3::from_data(coord);
	texdata.dxt = dxt ? float3::from_data(dxt) : float3(0.0f, 0.0f, 0.0f);
	texdata.dyt = dyt ? float3::from_data(dyt) : float3(0.0f, 0.0f, 0.0f);
	texdata.cfra = cfra;
	texdata.osatex = osatex;

	float4 color;
	float3 normal;
	void *results[] = { &color.x, &normal.x };
	
	_CTX(ctx)->eval_expression(&globals, &data, _EXPR(expr), results);
	
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

/* TODO using shared_ptr or similar here could help relax
 * order of acquire/release/invalidate calls (keep alive as long as used)
 */
typedef unordered_map<Tex*, bvm::Expression*> ExpressionCache;

static ExpressionCache bvm_tex_cache;
static bvm::mutex bvm_tex_mutex;

struct BVMExpression *BVM_texture_cache_acquire(Tex *tex)
{
	using namespace bvm;
	
	scoped_lock lock(bvm_tex_mutex);
	
	ExpressionCache::const_iterator it = bvm_tex_cache.find(tex);
	if (it != bvm_tex_cache.end()) {
		return (BVMExpression *)it->second;
	}
	else if (tex->use_nodes && tex->nodetree) {
		EvalGlobals globals;
		
		BVMExpression *expr = BVM_gen_texture_expression((BVMEvalGlobals *)(&globals), tex, tex->nodetree);
		
		bvm_tex_cache[tex] = _EXPR(expr);
		
		return expr;
	}
	else
		return NULL;
}

void BVM_texture_cache_release(Tex *tex)
{
	(void)tex;
}

void BVM_texture_cache_invalidate(Tex *tex)
{
	using namespace bvm;
	
	scoped_lock lock(bvm_tex_mutex);
	
	ExpressionCache::iterator it = bvm_tex_cache.find(tex);
	if (it != bvm_tex_cache.end()) {
		delete it->second;
		bvm_tex_cache.erase(it);
	}
}

void BVM_texture_cache_clear(void)
{
	using namespace bvm;
	
	scoped_lock lock(bvm_tex_mutex);
	
	for (ExpressionCache::iterator it = bvm_tex_cache.begin(); it != bvm_tex_cache.end(); ++it) {
		delete it->second;
	}
	bvm_tex_cache.clear();
}

#undef _GRAPH

#undef _EXPR
