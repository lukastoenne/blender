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

struct bNodeCompiler {
	typedef std::pair<bNode*, bNodeSocket*> bSocketPair;
	typedef std::pair<bvm::NodeInstance*, bvm::string> SocketPair;
	typedef std::set<SocketPair> SocketSet;
	typedef std::map<bSocketPair, SocketSet> InputMap;
	typedef std::map<bSocketPair, SocketPair> OutputMap;
	
	bNodeCompiler(bvm::NodeGraph *graph) :
	    m_graph(graph)
	{
	}
	
	~bNodeCompiler()
	{
	}
	
	const InputMap &input_map() const { return m_input_map; }
	const OutputMap &output_map() const { return m_output_map; }
	
	bvm::NodeInstance *add_node(const bvm::string &type,  const bvm::string &name)
	{
		return m_graph->add_node(type, name);
	}
	
	void map_input_socket(bNode *bnode, int bindex, bvm::NodeInstance *node, const bvm::string &name)
	{
		bNodeSocket *binput = (bNodeSocket *)BLI_findlink(&bnode->inputs, bindex);
		
		m_input_map[bSocketPair(bnode, binput)].insert(SocketPair(node, name));
		
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
	
	void map_output_socket(bNode *bnode, int bindex, bvm::NodeInstance *node, const bvm::string &name)
	{
		bNodeSocket *boutput = (bNodeSocket *)BLI_findlink(&bnode->outputs, bindex);
		
		m_output_map[bSocketPair(bnode, boutput)] = SocketPair(node, name);
	}
	
	void set_graph_output(const bvm::string &graph_output_name, bvm::NodeInstance *node, const bvm::string &name)
	{
		m_graph->set_output_link(graph_output_name, node, name);
	}
	
	void map_all_sockets(bNode *bnode, bvm::NodeInstance *node)
	{
		bNodeSocket *bsock;
		int i;
		for (bsock = (bNodeSocket *)bnode->inputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
			const bvm::NodeSocket *input = node->type->find_input(i);
			map_input_socket(bnode, i, node, input->name);
		}
		for (bsock = (bNodeSocket *)bnode->outputs.first, i = 0; bsock; bsock = bsock->next, ++i) {
			const bvm::NodeSocket *output = node->type->find_output(i);
			map_output_socket(bnode, i, node, output->name);
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
	bvm::NodeGraph *m_graph;
	
	InputMap m_input_map;
	OutputMap m_output_map;
};

struct bNodeParser {
	virtual void parse(bNodeCompiler *nodecomp, PointerRNA *bnode_ptr) const = 0;
};

static void gen_nodegraph(bNodeTree *btree, bvm::NodeGraph &graph, const bNodeParser &parser)
{
	bNodeCompiler nodecomp(&graph);
	
	for (bNode *bnode = (bNode*)btree->nodes.first; bnode; bnode = bnode->next) {
		PointerRNA ptr;
		RNA_pointer_create((ID *)btree, &RNA_Node, bnode, &ptr);
		
		BLI_assert(bnode->typeinfo != NULL);
		if (!nodeIsRegistered(bnode))
			continue;
		
		parser.parse(&nodecomp, &ptr);
	}
	
	for (bNodeLink *blink = (bNodeLink *)btree->links.first; blink; blink = blink->next) {
		if (!(blink->flag & NODE_LINK_VALID))
			continue;
		
		nodecomp.add_link(blink);
	}
}

/* ------------------------------------------------------------------------- */

static void binary_math_node(bNodeCompiler *comp, bNode *bnode, const bvm::string &type)
{
	bvm::NodeInstance *node = comp->add_node(type, bnode->name);
	comp->map_input_socket(bnode, 0, node, "value_a");
	comp->map_input_socket(bnode, 1, node, "value_b");
	comp->map_output_socket(bnode, 0, node, "value");
}

static void unary_math_node(bNodeCompiler *comp, bNode *bnode, const bvm::string &type)
{
	bvm::NodeInstance *node = comp->add_node(type, bnode->name);
	bNodeSocket *sock0 = (bNodeSocket *)BLI_findlink(&bnode->inputs, 0);
	bNodeSocket *sock1 = (bNodeSocket *)BLI_findlink(&bnode->inputs, 1);
	bool sock0_linked = !nodeSocketIsHidden(sock0) && (sock0->flag & SOCK_IN_USE);
	bool sock1_linked = !nodeSocketIsHidden(sock1) && (sock1->flag & SOCK_IN_USE);
	if (sock0_linked || !sock1_linked)
		comp->map_input_socket(bnode, 0, node, "value");
	else
		comp->map_input_socket(bnode, 1, node, "value");
	comp->map_output_socket(bnode, 0, node, "value");
}

struct ForceFieldNodeParser : public bNodeParser {
	typedef std::map<struct Object *, int> ObjectPtrMap;
	
	struct Object *effob;
	ObjectPtrMap obmap;
	
	ForceFieldNodeParser(const bvm::EvalGlobals *globals, struct Object *effob) :
	    effob(effob)
	{
		for (int i = 0; i < globals->objects.size(); ++i) {
			obmap[globals->objects[i]] = i;
		}
	}
	
	void parse(bNodeCompiler *comp, PointerRNA *bnode_ptr) const
	{
		bNode *bnode = (bNode *)bnode_ptr->data;
		bvm::string type = bvm::string(bnode->typeinfo->idname);
		if (type == "ForceOutputNode") {
			{
				bvm::NodeInstance *node = comp->add_node("PASS_FLOAT3", "RET_FORCE_" + bvm::string(bnode->name));
				comp->map_input_socket(bnode, 0, node, "value");
				comp->map_output_socket(bnode, 0, node, "value");
				
				comp->set_graph_output("force", node, "value");
			}
			
			{
				bvm::NodeInstance *node = comp->add_node("PASS_FLOAT3", "RET_IMPULSE_" + bvm::string(bnode->name));
				comp->map_input_socket(bnode, 1, node, "value");
				comp->map_output_socket(bnode, 0, node, "value");
				
				comp->set_graph_output("impulse", node, "value");
			}
		}
		else if (type == "ObjectSeparateVectorNode") {
			{
				bvm::NodeInstance *node = comp->add_node("GET_ELEM_FLOAT3", "GET_ELEM0_" + bvm::string(bnode->name));
				node->set_input_value("index", 0);
				comp->map_input_socket(bnode, 0, node, "value");
				comp->map_output_socket(bnode, 0, node, "value");
			}
			{
				bvm::NodeInstance *node = comp->add_node("GET_ELEM_FLOAT3", "GET_ELEM1_" + bvm::string(bnode->name));
				node->set_input_value("index", 1);
				comp->map_input_socket(bnode, 0, node, "value");
				comp->map_output_socket(bnode, 1, node, "value");
			}
			{
				bvm::NodeInstance *node = comp->add_node("GET_ELEM_FLOAT3", "GET_ELEM2_" + bvm::string(bnode->name));
				node->set_input_value("index", 2);
				comp->map_input_socket(bnode, 0, node, "value");
				comp->map_output_socket(bnode, 2, node, "value");
			}
		}
		else if (type == "ObjectCombineVectorNode") {
			bvm::NodeInstance *node = comp->add_node("SET_FLOAT3", bvm::string(bnode->name));
			comp->map_input_socket(bnode, 0, node, "value_x");
			comp->map_input_socket(bnode, 1, node, "value_y");
			comp->map_input_socket(bnode, 2, node, "value_z");
			comp->map_output_socket(bnode, 0, node, "value");
		}
		else if (type == "ForcePointDataNode") {
			{
				bvm::NodeInstance *node = comp->add_node("POINT_POSITION", "POINT_POS" + bvm::string(bnode->name));
				comp->map_output_socket(bnode, 0, node, "value");
			}
			{
				bvm::NodeInstance *node = comp->add_node("POINT_VELOCITY", "POINT_VEL" + bvm::string(bnode->name));
				comp->map_output_socket(bnode, 1, node, "value");
			}
		}
		else if (type == "ObjectMathNode") {
			int mode = RNA_enum_get(bnode_ptr, "mode");
			switch (mode) {
				case 0: binary_math_node(comp, bnode, "ADD_FLOAT"); break;
				case 1: binary_math_node(comp, bnode, "SUB_FLOAT"); break;
				case 2: binary_math_node(comp, bnode, "MUL_FLOAT"); break;
				case 3: binary_math_node(comp, bnode, "DIV_FLOAT"); break;
				case 4: unary_math_node(comp, bnode, "SINE"); break;
				case 5: unary_math_node(comp, bnode, "COSINE"); break;
				case 6: unary_math_node(comp, bnode, "TANGENT"); break;
				case 7: unary_math_node(comp, bnode, "ARCSINE"); break;
				case 8: unary_math_node(comp, bnode, "ARCCOSINE"); break;
				case 9: unary_math_node(comp, bnode, "ARCTANGENT"); break;
				case 10: binary_math_node(comp, bnode, "POWER"); break;
				case 11: binary_math_node(comp, bnode, "LOGARITHM"); break;
				case 12: binary_math_node(comp, bnode, "MINIMUM"); break;
				case 13: binary_math_node(comp, bnode, "MAXIMUM"); break;
				case 14: unary_math_node(comp, bnode, "ROUND"); break;
				case 15: binary_math_node(comp, bnode, "LESS_THAN"); break;
				case 16: binary_math_node(comp, bnode, "GREATER_THAN"); break;
				case 17: binary_math_node(comp, bnode, "MODULO"); break;
				case 18: unary_math_node(comp, bnode, "ABSOLUTE"); break;
				case 19: unary_math_node(comp, bnode, "CLAMP"); break;
			}
		}
		else if (type == "ObjectVectorMathNode") {
			int mode = RNA_enum_get(bnode_ptr, "mode");
			switch (mode) {
				case 0: {
					bvm::NodeInstance *node = comp->add_node("ADD_FLOAT3", bnode->name);
					comp->map_input_socket(bnode, 0, node, "value_a");
					comp->map_input_socket(bnode, 1, node, "value_b");
					comp->map_output_socket(bnode, 0, node, "value");
					break;
				}
				case 1: {
					bvm::NodeInstance *node = comp->add_node("SUB_FLOAT3", bnode->name);
					comp->map_input_socket(bnode, 0, node, "value_a");
					comp->map_input_socket(bnode, 1, node, "value_b");
					comp->map_output_socket(bnode, 0, node, "value");
					break;
				}
				case 2: {
					bvm::NodeInstance *node = comp->add_node("AVERAGE_FLOAT3", bnode->name);
					comp->map_input_socket(bnode, 0, node, "value_a");
					comp->map_input_socket(bnode, 1, node, "value_b");
					comp->map_output_socket(bnode, 0, node, "value");
					break;
				}
				case 3: {
					bvm::NodeInstance *node = comp->add_node("DOT_FLOAT3", bnode->name);
					comp->map_input_socket(bnode, 0, node, "value_a");
					comp->map_input_socket(bnode, 1, node, "value_b");
					comp->map_output_socket(bnode, 1, node, "value");
					break;
				}
				case 4: {
					bvm::NodeInstance *node = comp->add_node("CROSS_FLOAT3", bnode->name);
					comp->map_input_socket(bnode, 0, node, "value_a");
					comp->map_input_socket(bnode, 1, node, "value_b");
					comp->map_output_socket(bnode, 0, node, "value");
					break;
				}
				case 5: {
					bvm::NodeInstance *node = comp->add_node("NORMALIZE_FLOAT3", bnode->name);
					bNodeSocket *sock0 = (bNodeSocket *)BLI_findlink(&bnode->inputs, 0);
					bNodeSocket *sock1 = (bNodeSocket *)BLI_findlink(&bnode->inputs, 1);
					bool sock0_linked = !nodeSocketIsHidden(sock0) && (sock0->flag & SOCK_IN_USE);
					bool sock1_linked = !nodeSocketIsHidden(sock1) && (sock1->flag & SOCK_IN_USE);
					if (sock0_linked || !sock1_linked)
						comp->map_input_socket(bnode, 0, node, "value");
					else
						comp->map_input_socket(bnode, 1, node, "value");
					comp->map_output_socket(bnode, 0, node, "vector");
					comp->map_output_socket(bnode, 1, node, "value");
					break;
				}
			}
		}
		else if (type == "ForceClosestPointNode") {
			bvm::NodeInstance *node = comp->add_node("EFFECTOR_CLOSEST_POINT", bnode->name);
			Object *object = effob;
			ObjectPtrMap::const_iterator it = obmap.find(object);
			if (it != obmap.end()) {
				int object_index = it->second;
				node->set_input_value("object", object_index);
				comp->map_input_socket(bnode, 0, node, "vector");
				comp->map_output_socket(bnode, 0, node, "position");
				comp->map_output_socket(bnode, 1, node, "normal");
				comp->map_output_socket(bnode, 2, node, "tangent");
			}
		}
	}
};

struct BVMExpression *BVM_gen_forcefield_expression(const struct BVMEvalGlobals *globals, Object *effob, bNodeTree *btree)
{
	using namespace bvm;
	
	NodeGraph graph;
	{
		float zero[3] = {0.0f, 0.0f, 0.0f};
		graph.add_output("force", BVM_FLOAT3, zero);
		graph.add_output("impulse", BVM_FLOAT3, zero);
	}
	ForceFieldNodeParser parser(_GLOBALS(globals), effob);
	gen_nodegraph(btree, graph, parser);
	
	BVMCompiler compiler;
	Expression *expr = compiler.codegen_expression(graph);
	
	return (BVMExpression *)expr;
}

void BVM_eval_forcefield(struct BVMEvalGlobals *globals, struct BVMEvalContext *ctx, struct BVMExpression *expr,
                         const EffectedPoint *point, float force[3], float impulse[3])
{
	using namespace bvm;
	
	EvalData data;
	data.effector.position = float3(point->loc[0], point->loc[1], point->loc[2]);
	data.effector.velocity = float3(point->vel[0], point->vel[1], point->vel[2]);
	void *results[] = { force, impulse };
	
	_CTX(ctx)->eval_expression(_GLOBALS(globals), &data, _EXPR(expr), results);
}

/* ------------------------------------------------------------------------- */

struct TextureNodeParser : public bNodeParser {
	TextureNodeParser(const bvm::EvalGlobals */*globals*/)
	{
	}
	
	void parse(bNodeCompiler *comp, PointerRNA *bnode_ptr) const
	{
		bNode *bnode = (bNode *)bnode_ptr->data;
		bvm::string type = bvm::string(bnode->typeinfo->idname);
		if (type == "TextureNodeOutput") {
			{
				bvm::NodeInstance *node = comp->add_node("PASS_FLOAT4", "RET_COLOR_" + bvm::string(bnode->name));
				comp->map_input_socket(bnode, 0, node, "value");
				comp->map_output_socket(bnode, 0, node, "value");
				
				comp->set_graph_output("color", node, "value");
			}
			
			{
				bvm::NodeInstance *node = comp->add_node("PASS_FLOAT3", "RET_NORMAL_" + bvm::string(bnode->name));
				comp->map_input_socket(bnode, 1, node, "value");
				comp->map_output_socket(bnode, 0, node, "value");
				
				comp->set_graph_output("normal", node, "value");
			}
		}
	}
};

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
	TextureNodeParser parser(_GLOBALS(globals));
	gen_nodegraph(btree, graph, parser);
	
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
